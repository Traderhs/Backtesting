const fs = require("fs");
const fsPromises = fs.promises;
const path = require("path");
const {exec} = require("child_process");
const express = require("express");
const {Server: WebSocketServer} = require("ws");
const net = require("net");
const {handleLoadChartData, initializeFallbackImage, findLogoUrl, downloadAndSaveImage, USDT_FALLBACK_ICON_PATH} = require("./chartDataHandler");
const {startCppProcess, runSingleBacktesting, stopCppProcess} = require("./cppServer");

// =====================================================================================================================
// 전역 변수
// =====================================================================================================================
const app = express();
let port = 7777;

// 연결된 WebSocket 클라이언트
const activeClients = new Set();
let pendingProjectDirectoryResolvers = [];

// 베이스 디렉터리
const processWithPkg = process;
const isDevelopment = !processWithPkg.pkg;
const baseDir = isDevelopment ? process.cwd() : (path.dirname(process.execPath) || process.cwd());

// 디버그 로그 설정
const DEBUG_LOGS = process.env.LAUNCH_DEBUG_LOGS === '0';
if (!DEBUG_LOGS) {
    console.log = () => {};
    console.error = () => {};
}

// =====================================================================================================================
// 유틸리티 함수
// =====================================================================================================================

function requestProjectDirectoryFromClients() {
    const pollInterval = 200;
    return new Promise((resolve) => {
        const trySend = () => {
            if (activeClients && activeClients.size > 0) {
                activeClients.forEach(c => {
                    try {
                        c.send(JSON.stringify({action: 'requestProjectDirectory'}));
                    } catch (e) {}
                });

                pendingProjectDirectoryResolvers.push((val) => resolve(val));
                return;
            }
            setTimeout(trySend, pollInterval);
        };
        trySend();
    });
}

function getCallerFileInfo() {
    try {
        const err = new Error();
        const stack = err.stack || '';
        const lines = stack.split('\n').map(l => l.trim());
        
        for (let i = 1; i < lines.length; i++) {
            const line = lines[i];
            if (!line || line.includes('getCallerFileInfo') || line.includes('broadcastLog') || line.includes('Object.<anonymous>')) {
                continue;
            }

            const m1 = line.match(/\(?([^\s)]+):(\d+):(\d+)\)?$/);
            if (m1) {
                let fp = m1[1];
                const ln = m1[2];

                fp = fp.split('?')[0];
                fp = fp.replace(/-([A-Za-z0-9]+)(?=\.js$)/, '');

                const parts = fp.split(/[\\/]/);
                return `${parts[parts.length - 1]}:${ln}`;
            }
        }
    } catch (e) {}
    return null;
}

function broadcastLog(level, message, timestamp = null, fileInfo = null) {
    if (message === undefined || message === null) message = '';
    if (typeof message !== 'string') {
        try {
            message = String(message);
        } catch (e) {
            message = '';
        }
    }

    if (!fileInfo) {
        const inferred = getCallerFileInfo();
        if (inferred) fileInfo = inferred;
    }

    if (level !== 'SEPARATOR') {
        if (!message.startsWith(' |')) {
            message = ' | ' + message;
        }
    }

    try {
        activeClients.forEach(client => {
            if (client.readyState === 1) {
                client.send(JSON.stringify({
                    action: "backtestingLog",
                    level: level,
                    message: message,
                    timestamp: timestamp,
                    fileInfo: fileInfo
                }));
            }
        });
    } catch (e) {}
}

function isPortAvailable(port) {
    return new Promise((resolve) => {
        const server = net.createServer();

        server.listen(port, () => {
            server.once('close', () => resolve(true));
            server.close();
        });

        server.on('error', () => resolve(false));
    });
}

async function findAvailablePort(startPort) {
    let currentPort = startPort;

    while (!(await isPortAvailable(currentPort))) {
        broadcastLog("INFO", `포트 ${currentPort}가 이미 사용 중입니다., 포트 ${currentPort + 1} 사용을 시도합니다.`, null, null);
        currentPort++;
    }
    return currentPort;
}

// =====================================================================================================================
// 정적 파일 제공
// =====================================================================================================================
const iconDir = path.join(baseDir, "Backboard", "icon");
app.use("/Backboard", express.static(path.join(baseDir, "Backboard")));

const distPath = path.join(baseDir, "Backboard");
app.use(express.static(distPath));

broadcastLog("INFO", `${isDevelopment ? '개발자' : '배포'} 모드에서 실행`, null, null);
broadcastLog("INFO", `베이스 폴더: ${baseDir}`, null, null);

// =====================================================================================================================
// API: 로고 가져오기
// =====================================================================================================================
app.get("/api/get-logo", async (req, res) => {
    const {symbol} = req.query;
    if (!symbol) {
        return res.status(400).send("Symbol query parameter is required.");
    }

    const safeSymbolName = symbol.replace(/[^a-zA-Z0-9]/g, '_');
    const localFilePath = path.join(iconDir, `${safeSymbolName}.png`);
    const localFileUrl = `/Backboard/icon/${safeSymbolName}.png`;

    try {
        await fsPromises.access(localFilePath, fs.constants.F_OK);
        return res.json({logoUrl: localFileUrl});
    } catch (error) {}

    try {
        const imageUrl = await findLogoUrl(symbol);
        if (!imageUrl) {
            return res.json({logoUrl: USDT_FALLBACK_ICON_PATH});
        }

        try {
            await fsPromises.mkdir(path.dirname(localFilePath), {recursive: true});
        } catch (dirError) {
            broadcastLog("ERROR", `디렉토리 생성 실패: ${dirError.message}`, null, null);
        }

        try {
            await downloadAndSaveImage(imageUrl, localFilePath);
        } catch (downloadError) {
            broadcastLog("ERROR", `이미지 다운로드 실패 (${symbol}): ${downloadError.message}`, null, null);
            return res.json({logoUrl: USDT_FALLBACK_ICON_PATH});
        }

        try {
            const stats = await fsPromises.stat(localFilePath);
            if (stats.size === 0) {
                broadcastLog("ERROR", `다운로드된 파일 크기가 0입니다: ${localFilePath}`, null, null);

                return res.json({logoUrl: USDT_FALLBACK_ICON_PATH});
            }
        } catch (statError) {
            broadcastLog("ERROR", `파일 상태 확인 실패: ${statError.message}`, null, null);

            return res.json({logoUrl: USDT_FALLBACK_ICON_PATH});
        }

        return res.json({logoUrl: localFileUrl});
    } catch (fetchError) {
        broadcastLog("ERROR", `전체 로고 처리 실패 (${symbol}): ${fetchError.message}`, null, null);

        return res.json({logoUrl: USDT_FALLBACK_ICON_PATH});
    }
});

// =====================================================================================================================
// API: 소스 코드 가져오기
// =====================================================================================================================
app.get("/api/get-source-code", async (req, res) => {
    try {
        const {filePath} = req.query;
        if (!filePath) {
            return res.status(400).json({error: "파일 경로가 제공되지 않았습니다."});
        }

        try {
            await fsPromises.access(filePath, fs.constants.F_OK);
            const fileContent = await fsPromises.readFile(filePath, {encoding: 'utf8'});

            res.set({
                'Cache-Control': 'no-cache, no-store, must-revalidate',
                'Pragma': 'no-cache',
                'Expires': '0',
                'ETag': `"${Date.now()}-${Math.random()}"`
            });

            res.json({content: fileContent});
        } catch (error) {
            res.status(404).json({
                error: `파일을 찾을 수 없거나 읽을 수 없습니다: ${filePath}`,
                details: error.message
            });
        }
    } catch (error) {
        res.status(500).json({
            error: "서버 오류가 발생했습니다.",
            details: error.message
        });
    }
});

// =====================================================================================================================
// 강제 종료 API
// =====================================================================================================================
app.get("/force-shutdown", (req, res) => {
    res.send("서버가 강제 종료됩니다.");
    stopCppProcess();
    process.exit(0);
});

// =====================================================================================================================
// 모든 라우트 핸들러
// =====================================================================================================================
app.get("*", (req, res) => {
    res.sendFile(path.join(distPath, "index.html"));
});

// =====================================================================================================================
// 서버 시작
// =====================================================================================================================
async function startServer() {
    port = await findAvailablePort(port);
    broadcastLog("INFO", `포트 ${port}에서 서버를 실행합니다.`, null, null);

    return app.listen(port, () => {
        broadcastLog("INFO", `http://localhost:${port}에서 서버가 실행 중입니다.`, null, null);

        exec(`start http://localhost:${port}`, (err) => {
            if (err) {
                broadcastLog("ERROR", `브라우저를 여는 데 실패했습니다.: ${err.message}`, null, null);
            }
        });
    });
}

// =====================================================================================================================
// WebSocket 설정
// =====================================================================================================================
function setupWebSocket(server, dataPaths, indicatorPaths) {
    const wss = new WebSocketServer({server});
    const {
        loadOrCreateEditorConfig,
        saveEditorConfig,
        configToWs,
        wsToConfig,
        getEditorConfig,
        setEditorConfig,
        isEditorConfigLoading,
        setEditorConfigLoading,
        validateProjectDirectoryConfig,
        handleProvideProjectDirectory
    } = require("./cppServer");

    let shutdownTimer = null;
    const SHUTDOWN_DELAY = 500;

    function scheduleShutdown() {
        if (!shutdownTimer) {
            shutdownTimer = setTimeout(() => process.exit(), SHUTDOWN_DELAY);
        }
    }

    function cancelShutdown() {
        if (shutdownTimer) {
            clearTimeout(shutdownTimer);
            shutdownTimer = null;
        }
    }

    wss.on("connection", (ws) => {
        cancelShutdown();
        activeClients.add(ws);

        ws.on("close", () => {
            activeClients.delete(ws);
            if (wss.clients.size === 0) {
                scheduleShutdown();
            }
        });

        ws.on('message', (m) => {
            try {
                const msg = JSON.parse(m);
                if (msg && msg.action === 'provideProjectDirectory') {
                    if (pendingProjectDirectoryResolvers.length > 0) {
                        const resolver = pendingProjectDirectoryResolvers.shift();

                        resolver(msg.projectDirectory || null);
                    }
                }
            } catch (e) {}
        });

        // 클라이언트 연결 시 editor.json 로드
        (async () => {
            if (!getEditorConfig() && !isEditorConfigLoading()) {
                setEditorConfigLoading(true);
                try {
                    const config = await loadOrCreateEditorConfig(activeClients, requestProjectDirectoryFromClients, broadcastLog, baseDir);
                    if (config && !(await validateProjectDirectoryConfig(config, broadcastLog))) {
                        setEditorConfig(null);
                    } else {
                        setEditorConfig(config);
                    }
                } catch (e) {
                    broadcastLog("ERROR", `editor.json 로드 실패: ${e.message}`, null, null);
                }
                setEditorConfigLoading(false);
            }
        })();

        // 메시지 핸들러
        ws.on("message", async (message) => {
            try {
                const msg = JSON.parse(message);

                switch (msg.action) {
                    case "loadChartData":
                        await handleLoadChartData(ws, msg, dataPaths, indicatorPaths);
                        break;

                    case "runSingleBacktesting":
                        runSingleBacktesting(ws, msg.symbolConfigs, msg.barDataConfigs, msg.useBarMagnifier, msg.clearAndAddBarData, getEditorConfig(), broadcastLog);
                        break;

                    case "loadEditorConfig":
                        if (!getEditorConfig() && !isEditorConfigLoading()) {
                            setEditorConfigLoading(true);

                            try {
                                const config = await loadOrCreateEditorConfig(activeClients, requestProjectDirectoryFromClients, broadcastLog, baseDir);
                                if (config && !(await validateProjectDirectoryConfig(config, broadcastLog))) {
                                    setEditorConfig(null);
                                } else {
                                    setEditorConfig(config);
                                }
                            } catch (e) {
                                broadcastLog("ERROR", `editor.json 로드 실패: ${e.message}`, null, null);
                            }

                            setEditorConfigLoading(false);
                        }

                        ws.send(JSON.stringify({
                            action: "editorConfigLoaded",
                            config: configToWs(getEditorConfig() || {
                                logPanelOpen: false,
                                logPanelHeight: 400,
                                projectDirectory: '',
                                useBarMagnifier: true,
                                symbolConfigs: [],
                                barDataConfigs: []
                            })
                        }));
                        break;

                    case "saveEditorConfig":
                        const nextConfig = wsToConfig(msg.config);
                        const success = saveEditorConfig(nextConfig, broadcastLog, baseDir);

                        ws.send(JSON.stringify({
                            action: "editorConfigSaved",
                            success: success
                        }));

                        if (success) {
                            setEditorConfig(nextConfig);
                        }

                        break;

                    case "provideProjectDirectory":
                        await handleProvideProjectDirectory(ws, msg.projectDirectory, activeClients, broadcastLog, baseDir);
                        break;

                    default:
                        ws.send(JSON.stringify({error: "알 수 없는 action"}));
                        break;
                }
            } catch (err) {
                ws.send(JSON.stringify({error: "메시지 처리 오류"}));
            }
        });
    });

    return wss;
}

// =====================================================================================================================
// 메인 실행
// =====================================================================================================================
async function main() {
    try {
        // 폴백 이미지 초기화
        await initializeFallbackImage(iconDir);

        // 서버 시작
        const server = await startServer();

        // config.json에서 데이터 경로 파싱
        const configPath = path.join(baseDir, "Backboard", "config.json");
        let config;

        try {
            config = JSON.parse(fs.readFileSync(configPath, "utf8"));
        } catch (e) {
            broadcastLog("WARN", `config.json 로드 실패: ${e.message}`, null, null);
        }

        // 캔들스틱 데이터 경로 구성
        const dataPaths = {};
        if (config && config["심볼"]) {
            config["심볼"].forEach((symbol) => {
                if (symbol["트레이딩 바 데이터"] && symbol["트레이딩 바 데이터"]["데이터 경로"]) {
                    const symbolName = symbol["심볼 이름"] || "default";

                    dataPaths[symbolName] = symbol["트레이딩 바 데이터"]["데이터 경로"].replace(/\/[^\/]*$/, "");
                }
            });
        }

        // 지표 데이터 경로 구성
        const indicatorPaths = {};
        if (config && config["지표"]) {
            config["지표"].forEach((indicatorObj) => {
                const indicatorName = indicatorObj["지표 이름"] || "unknown_indicator";
                const indicatorPath = indicatorObj["데이터 경로"] || "";

                indicatorPaths[indicatorName] = indicatorPath.replace(/\/[^\/]*$/, "");
            });
        }

        // WebSocket 설정
        setupWebSocket(server, dataPaths, indicatorPaths);

        // C++ 프로세스 시작
        startCppProcess(activeClients, broadcastLog, baseDir);

    } catch (err) {
        broadcastLog("ERROR", `서버를 시작하는 데 실패했습니다.: ${err}`, null, null);
        process.exit(1);
    }
}

// 실행
main().then();
