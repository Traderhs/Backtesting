const path = require("path");
const { exec } = require("child_process");
const express = require("express");
const { readdir, readFileSync } = require("fs");
const { Server: WebSocketServer } = require("ws");
const parquet = require("parquetjs");

const app = express();
const port = 7777;

// Backboard 폴더 전체를 정적으로 제공
app.use("/Backboard", express.static(path.join(process.cwd(), "Backboard")));

// 차트 파일 목록 제공
app.get("/chart-files", (req, res) => {
    const chartDir = path.join(process.cwd(), "Backboard", "charts");
    readdir(chartDir, (err, files) => {
        if (err) {
            res.status(500).send("차트 폴더를 읽을 수 없습니다.");
            return;
        }
        const htmlFiles = files.filter(f => f.endsWith(".html"));
        res.json(htmlFiles);
    });
});

// pkg 실행파일 기준으로, 리소스 폴더 위치
const distPath = path.join(process.cwd(), "Backboard");
app.use(express.static(distPath));

// 나머지 요청은 index.html로 라우팅
app.get("*", (req, res) => {
    res.sendFile(path.join(distPath, "index.html"));
});

// 서버 실행 및 브라우저 자동 열기
const server = app.listen(port, () => {
    console.log(`Dashboard running at http://localhost:${port}`);
    exec(`start http://localhost:${port}`, (err) => {
        if (err) {
            console.error("브라우저 열기 실패:", err);
        }
    });
});

// WebSocket 관련 코드
const wss = new WebSocketServer({ server });

// 종료 타이머 (창이 닫혔을 때만 서버 종료)
let shutdownTimer = null;
const SHUTDOWN_DELAY = 5000; // 5초 후 종료

function scheduleShutdown() {
    if (!shutdownTimer) {
        console.log("WebSocket 연결이 모두 종료되었습니다. 서버를 종료합니다...");
        shutdownTimer = setTimeout(() => {
            process.exit();
        }, SHUTDOWN_DELAY);
    }
}

function cancelShutdown() {
    if (shutdownTimer) {
        clearTimeout(shutdownTimer);
        shutdownTimer = null;
        console.log("새로운 WebSocket 연결이 감지되어 서버 종료 취소");
    }
}

wss.on("connection", (ws) => {
    console.log("클라이언트와 WebSocket 연결됨");
    cancelShutdown();

    ws.on("close", () => {
        console.log("클라이언트 WebSocket 연결 종료");
        if (wss.clients.size === 0) {
            scheduleShutdown();
        }
    });
});

// 강제 종료 함수 및 API
function forceShutdown() {
    console.log("서버가 강제 종료됩니다.");
    process.exit(0);
}

app.get("/force-shutdown", (req, res) => {
    res.send("서버가 강제 종료됩니다.");
    forceShutdown();
});

//
// config.json을 읽어 심볼별 데이터 경로 설정
//
const configPath = path.join(process.cwd(), "Backboard", "config.json");
let config;
try {
    config = JSON.parse(readFileSync(configPath, "utf8"));
} catch (e) {
    console.error("config.json 읽기 오류:", e);
}

const dataPaths = {};  // 심볼명 -> Parquet 파일 경로
if (config && config["심볼"]) {
    config["심볼"].forEach(symbol => {
        if (
            symbol["트레이딩 바 데이터"] &&
            symbol["트레이딩 바 데이터"]["데이터 경로"]
        ) {
            const symbolName = symbol["심볼명"] || "default";
            const dataPath = symbol["트레이딩 바 데이터"]["데이터 경로"];
            dataPaths[symbolName] = dataPath;
            console.log(`심볼 ${symbolName} 데이터 경로: ${dataPath}`);
        }
    });
}

//
// persistent reader 캐시: 파일별로 ParquetReader를 유지합니다.
// 파일이 정적이므로 한 번 열린 reader를 재사용합니다.
//
const parquetReaderCache = {};

/**
 * 지정한 Parquet 파일의 reader를 캐시에서 반환합니다.
 * 캐시에 없으면 파일을 열고 reader를 캐시한 후 반환합니다.
 */
async function getParquetReader(filePath) {
    if (parquetReaderCache[filePath]) {
        return parquetReaderCache[filePath];
    }
    const reader = await parquet.ParquetReader.openFile(filePath);
    parquetReaderCache[filePath] = reader;
    return reader;
}

/**
 * Parquet 파일에서 "Open Time" 컬럼을 기준으로 fromTime ~ toTime 범위의 데이터를 로드합니다.
 * fromTime, toTime은 초 단위이며, 파일 내 시간은 ms 단위입니다.
 * 이 함수는 캐시된 ParquetReader를 재사용합니다.
 */
async function loadParquetDataInRange(filePath, fromTime, toTime) {
    const fromMs = fromTime * 1000;
    const toMs = toTime * 1000;

    const reader = await getParquetReader(filePath);
    // ParquetJS에서는 reader.metadata.row_groups (또는 row_groups) 속성으로 row group 정보를 제공합니다.
    const rowGroups = reader.metadata.row_groups;
    let result = [];

    for (let i = 0; i < rowGroups.length; i++) {
        const rg = rowGroups[i];
        // "Open Time" 컬럼의 통계 정보를 찾습니다.
        const openTimeColumn = rg.columns.find(col => {
            return col.meta_data.path.length === 1 && col.meta_data.path[0] === "Open Time";
        });

        if (openTimeColumn) {
            const stats = openTimeColumn.meta_data.statistics;
            const groupMin = Number(stats.min);
            const groupMax = Number(stats.max);
            if (groupMax < fromMs || groupMin > toMs) {
                continue;
            }
        }

        let cursor = reader.getCursor({ rowGroup: i });
        let record = null;
        while (record = await cursor.next()) {
            if (record["Open Time"] >= fromMs && record["Open Time"] <= toMs) {
                result.push(record);
            }
        }
    }
    return result;
}

//
// WebSocket 메시지 처리: 클라이언트가 "loadMore" 요청 시 해당 심볼의 Parquet 파일에서 데이터 범위 추출
//
wss.on("connection", (ws) => {
    ws.on("message", async (message) => {
        try {
            const msg = JSON.parse(message);
            if (msg.action === "loadMore") {
                const symbol = msg.symbol;         // 예: "BTCUSD"
                const fromTime = msg.from;           // 요청 시작 시간 (초 단위)
                const toTime = msg.to;               // 요청 종료 시간 (초 단위)
                const filePath = dataPaths[symbol];
                if (filePath) {
                    try {
                        const filteredData = await loadParquetDataInRange(filePath, fromTime, toTime);
                        ws.send(JSON.stringify({
                            action: "loadMoreResponse",
                            data: filteredData
                        }));
                    } catch (err) {
                        console.error("Parquet 데이터 처리 오류:", err);
                        ws.send(JSON.stringify({ error: "Parquet 데이터 처리 오류" }));
                    }
                } else {
                    ws.send(JSON.stringify({ error: "심볼 데이터 경로 없음" }));
                }
            }
        } catch (e) {
            console.error("메시지 처리 오류:", e);
            ws.send(JSON.stringify({ error: "메시지 처리 오류" }));
        }
    });
});
