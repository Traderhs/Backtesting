const fs = require("fs");
const fsPromises = fs.promises;
const path = require("path");
const {exec} = require("child_process");
const express = require("express");
const {Server: WebSocketServer} = require("ws");
const net = require("net");
const {
    handleLoadChartData,
    initializeFallbackImage,
    findLogoUrl,
    downloadAndSaveImage,
    USDT_FALLBACK_ICON_PATH
} = require("./chartDataHandler");
const {startBacktestingEngine, runSingleBacktesting, stopBacktestingEngine} = require("./backtestingEngine");

// =====================================================================================================================
// 전역 변수
// =====================================================================================================================
const app = express();
const requestedPort = Number(process.env.BACKBOARD_PORT);
const isFixedPort = Number.isFinite(requestedPort) && requestedPort > 0;
let port = isFixedPort ? requestedPort : 7777;

// 연결된 WebSocket 클라이언트
const activeClients = new Set();
let pendingProjectDirectoryResolvers = [];

// 베이스 디렉터리
const processWithPkg = process;
const isDevelopment = !processWithPkg.pkg;
const baseDir = isDevelopment ? process.cwd() : (path.dirname(process.execPath) || process.cwd());

function toPosix(p) {
    if (!p || typeof p !== 'string') {
        return p;
    }

    return p.replace(/\\/g, '/');
}

// 디버그 로그 설정
const DEBUG_LOGS = process.env.LAUNCH_DEBUG_LOGS === '0';
if (!DEBUG_LOGS) {
    console.log = () => {
    };
    console.error = () => {
    };
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
                    } catch (e) {
                    }
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
    } catch (e) {
    }
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
    } catch (e) {
    }
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
const iconDir = path.join(baseDir, "BackBoard", "Icons");
app.use("/BackBoard", express.static(path.join(baseDir, "BackBoard")));

const distPath = path.join(baseDir, "BackBoard");
app.use(express.static(distPath));

// =====================================================================================================================
// API: config 가져오기
// =====================================================================================================================
// 서버에서 미리 로드한 BackBoard/config.json을 클라이언트가 요청할 수 있도록 전역 변수와 API 제공
let config = null;

app.get('/api/config', (req, res) => {
    if (!config) {
        // 설정이 아직 로드되지 않았거나 문제 발생 시 빈 객체 반환
        return res.json({});
    }

    res.json(config);
});

// =====================================================================================================================
// API: 거래 내역 가져오기
// =====================================================================================================================
app.get('/api/trade-list', async (req, res) => {
    try {
        const tradeListPath = path.join(baseDir, 'BackBoard', 'trade_list.json');

        // 파일 존재 확인
        await fsPromises.access(tradeListPath, fs.constants.F_OK);

        // 파일 읽기
        let fileContent = await fsPromises.readFile(tradeListPath, {encoding: 'utf8'});

        // BOM (Byte Order Mark) 제거
        if (fileContent.charCodeAt(0) === 0xFEFF) {
            fileContent = fileContent.slice(1);
        }

        const tradeData = JSON.parse(fileContent);

        res.set({
            'Cache-Control': 'no-cache, no-store, must-revalidate',
            'Pragma': 'no-cache',
            'Expires': '0'
        });

        res.json(tradeData);
    } catch (error) {
        res.status(404).json({
            error: 'trade_list.json 파일을 찾을 수 없습니다.',
            attemptedPath: toPosix(path.join(baseDir, 'BackBoard', 'trade_list.json')),
            baseDir: toPosix(baseDir),
            cwd: toPosix(process.cwd()),
            details: error.message
        });
    }
});

// =====================================================================================================================
// API: 정적 아이콘 제공
// =====================================================================================================================
app.get('/api/icon', async (req, res) => {
    const {name} = req.query;
    if (!name || typeof name !== 'string') {
        return res.status(400).send('name query parameter is required');
    }

    // 경로 조작 방지: basename만 사용
    const safeName = path.basename(name);
    const filePath = path.join(iconDir, safeName);

    try {
        await fsPromises.access(filePath, require('fs').constants.F_OK);
        return res.sendFile(filePath);
    } catch (err) {
        // 파일이 없으면 404로 응답 (클라이언트가 CSS 기반 폴백을 처리하도록 함)
        return res.status(404).send('icon not found');
    }
});

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
    const localFileUrl = `/BackBoard/Icons/${safeSymbolName}.png`;

    try {
        await fsPromises.access(localFilePath, fs.constants.F_OK);

        return res.json({logoUrl: localFileUrl});
    } catch (error) {
        // 무시
    }

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
                broadcastLog("ERROR", `다운로드된 파일 크기가 0입니다: ${toPosix(localFilePath)}`, null, null);

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
// API: 사용 가능한 심볼 목록
// =====================================================================================================================
app.get('/api/symbols', async (req, res) => {
    try {
        // 디렉터리에 있는 데이터로 심볼 목록 추출
        const continuousPath = path.join(baseDir, 'Data', 'Continuous Klines');
        let symbols = [];

        try {
            const dirents = await fsPromises.readdir(continuousPath, {withFileTypes: true});
            const dirs = dirents.filter(d => d.isDirectory()).map(d => d.name.toUpperCase());
            symbols = symbols.concat(dirs);
        } catch (e) {
            // 디렉터리가 없거나 접근 불가 시 무시
        }

        // config에 명시된 심볼도 합침
        if (config && Array.isArray(config['심볼'])) {
            const cfg = config['심볼'].map((s) => (s['심볼 이름'] || s['심볼'] || s['symbol'] || '')).filter(Boolean).map(String).map(s => s.toUpperCase());
            symbols = Array.from(new Set([...symbols, ...cfg]));
        }

        // 기본으로 포함할 인기 심볼 (빠른 검색용)
        const popularSymbols = [
            'BTC', 'ETH', 'BNB', 'SOL', 'XRP', 'ADA', 'DOGE',
            'DOT', 'LTC', 'LINK', 'TRX', 'NEAR', 'AVAX', 'ATOM'
        ];

        // 쿼리 파라미터로 추가 소스 지정 가능: include=popular,web
        const includes = (req.query.include || '').toString().split(',').map(s => s.trim().toLowerCase()).filter(Boolean);

        if (includes.includes('popular')) {
            symbols = Array.from(new Set([...symbols, ...popularSymbols]));
        }

        // 웹(거래소)에서 가져오기 (옵션): Binance 공용 API 사용, 실패 시 무시
        if (includes.includes('web') || includes.includes('exchange')) {
            // 간단 캐시(10분)
            const now = Date.now();
            if (!global.__symbol_cache) {
                global.__symbol_cache = {ts: 0, symbols: []};
            }

            if (now - global.__symbol_cache.ts > 10 * 60 * 1000) {
                try {
                    const controller = new AbortController();
                    const timeout = setTimeout(() => controller.abort(), 3000);

                    const resp = await fetch('https://api.binance.com/api/v3/ticker/price', {signal: controller.signal});
                    clearTimeout(timeout);

                    if (resp.ok) {
                        const data = await resp.json();

                        // 모든 알려진 페어를 제거하여 베이스 심볼만 추출
                        const knownPairs = ['USDT', 'USDC', 'BUSD', 'BTC', 'ETH', 'BNB', 'USD', 'EUR', 'GBP', 'JPY', 'KRW', 'SOL', 'DOGE'];
                        const baseSymbols = new Set();

                        data.forEach(p => {
                            const symbol = (p.symbol || '').toUpperCase();
                            if (!symbol) {
                                return;
                            }

                            // 각 페어로 끝나는지 확인하고 베이스 심볼 추출
                            for (const pair of knownPairs) {
                                if (symbol.endsWith(pair) && symbol !== pair) {
                                    const base = symbol.slice(0, -pair.length);

                                    if (base && base.length >= 2) {
                                        baseSymbols.add(base);
                                        break;
                                    }
                                }
                            }
                        });

                        global.__symbol_cache = {ts: Date.now(), symbols: Array.from(baseSymbols)};
                    }
                } catch (e) {
                    // 네트워크 문제, 무시
                    broadcastLog('WARN', `외부 심볼 조회 실패: ${e.message}`);
                }
            }

            if (global.__symbol_cache && Array.isArray(global.__symbol_cache.symbols)) {
                symbols = Array.from(new Set([...symbols, ...global.__symbol_cache.symbols]));
            }
        }

        // 디렉터리에서 가져온 심볼도 베이스 심볼로 변환
        const knownPairs = ['USDT', 'USDC', 'BUSD', 'BTC', 'ETH', 'BNB', 'USD', 'EUR', 'GBP', 'JPY', 'KRW', 'SOL', 'DOGE'];
        const baseSymbols = new Set();

        symbols.forEach(symbol => {
            let isBase = true;

            for (const pair of knownPairs) {
                if (symbol.endsWith(pair) && symbol !== pair) {
                    const base = symbol.slice(0, -pair.length);

                    if (base && base.length >= 2) {
                        baseSymbols.add(base);
                        isBase = false;
                        break;
                    }
                }
            }

            // 페어로 끝나지 않는 경우 (이미 베이스 심볼) 또는 페어 자체인 경우
            if (isBase && symbol.length >= 2) {
                baseSymbols.add(symbol);
            }
        });

        symbols = Array.from(baseSymbols);

        // 정렬 및 반환
        symbols = Array.from(new Set(symbols)).sort();

        res.json({symbols});
    } catch (err) {
        broadcastLog('ERROR', `심볼 목록을 불러오는 동안 오류가 발생했습니다: ${err.message}`);
        res.status(500).json({symbols: []});
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
                error: `파일을 찾을 수 없거나 읽을 수 없습니다: ${toPosix(filePath)}`,
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
// API: 백테스팅 로그
// =====================================================================================================================
app.head('/api/log', async (req, res) => {
    try {
        const p = path.join(baseDir, 'BackBoard', 'backtesting.log');

        let filePath = null;
        try {
            await fsPromises.access(p, fs.constants.F_OK);

            filePath = p;
        } catch (e) {
            // continue
        }

        if (!filePath) {
            return res.status(404).end();
        }

        const stat = await fsPromises.stat(filePath);

        res.set({
            'Content-Length': stat.size,
            'Accept-Ranges': 'bytes',
            'Cache-Control': 'no-cache'
        });

        return res.status(200).end();
    } catch (err) {
        broadcastLog('ERROR', `로그 메타데이터 응답 실패: ${err.message}`);
        return res.status(500).end();
    }
});

app.get('/api/log', async (req, res) => {
    try {
        const p = path.join(baseDir, 'BackBoard', 'backtesting.log');

        let filePath = null;
        try {
            await fsPromises.access(p, fs.constants.F_OK);

            filePath = p;
        } catch (e) {
            // continue
        }

        if (!filePath) {
            broadcastLog('WARN', '백테스팅 로그 파일을 찾을 수 없습니다.');
            return res.status(404).send('log not found');
        }

        const stat = await fsPromises.stat(filePath);
        const range = req.headers.range;

        // Range가 없는 경우 전체 파일 반환
        if (!range) {
            res.set({
                'Content-Type': 'text/plain; charset=utf-8',
                'Content-Length': stat.size,
                'Accept-Ranges': 'bytes',
                'Cache-Control': 'no-cache'
            });
            return fs.createReadStream(filePath).pipe(res);
        }

        // Range 요청 처리
        const bytesPrefix = 'bytes=';
        if (!range.startsWith(bytesPrefix)) {
            return res.status(416).send('Invalid Range');
        }

        const rangeParts = range.substring(bytesPrefix.length).split('-');
        const start = parseInt(rangeParts[0], 10) || 0;
        const end = rangeParts[1] && rangeParts[1] !== '' ? parseInt(rangeParts[1], 10) : (stat.size - 1);

        if (isNaN(start) || isNaN(end) || start > end || start >= stat.size) {
            res.set('Content-Range', `bytes */${stat.size}`);
            return res.status(416).end();
        }

        const chunkSize = (end - start) + 1;
        const stream = fs.createReadStream(filePath, {start, end});

        res.status(206).set({
            'Content-Range': `bytes ${start}-${end}/${stat.size}`,
            'Accept-Ranges': 'bytes',
            'Content-Length': chunkSize,
            'Content-Type': 'text/plain; charset=utf-8',
            'Cache-Control': 'no-cache'
        });

        stream.pipe(res);

    } catch (err) {
        broadcastLog('ERROR', `로그 파일 전송 실패: ${err.message}`);
        return res.status(500).send('internal error');
    }
});

// =====================================================================================================================
// 강제 종료 API
// =====================================================================================================================
app.get("/force-shutdown", (req, res) => {
    res.send("서버가 강제 종료됩니다.");
    stopBacktestingEngine();
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
    if (isFixedPort) {
        const available = await isPortAvailable(port);
        if (!available) {
            throw new Error(`BACKBOARD_PORT=${port} 가 이미 사용 중입니다.`);
        }
    } else {
        port = await findAvailablePort(port);
    }
    broadcastLog("INFO", `포트 ${port}에서 서버를 실행합니다.`, null, null);

    return app.listen(port, () => {
        broadcastLog("INFO", `http://localhost:${port}에서 서버가 실행 중입니다.`, null, null);

        // 백엔드가 직접 브라우저를 여는 동작은 DEV 실행 시 Vite가 대신 열도록 제어
        if (process.env.BACKBOARD_OPEN_BROWSER !== 'none') {
            exec(`start http://localhost:${port}`, (err) => {
                if (err) {
                    broadcastLog("ERROR", `브라우저를 여는 데 실패했습니다.: ${err.message}`, null, null);
                }
            });
        }
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
    } = require("./backtestingEngine");

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
            } catch (e) {
                // 무시
            }
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

                        // 프로젝트 폴더가 설정되어 있으면 백테스팅 엔진을 그 프로젝트 디렉터리에서 실행합니다.
                        try {
                            const startDir = config && config.projectDirectory ? config.projectDirectory : baseDir;
                            startBacktestingEngine(activeClients, broadcastLog, startDir);
                        } catch (e) {
                            broadcastLog("ERROR", `백테스팅 엔진 시작 실패: ${e.message}`, null, null);
                        }
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
        const configPath = path.join(baseDir, "BackBoard", "config.json");

        try {
            // 전역 `config` 변수에 파일 내용을 로드
            config = JSON.parse(fs.readFileSync(configPath, "utf8"));
        } catch (e) {
            broadcastLog("WARN", `config.json 로드 실패: ${e.message}`, null, null);

            // 실패 시 전역 config를 null로 유지
            config = config || null;
        }

        // 캔들스틱 데이터 경로 구성
        const dataPaths = {};
        if (config && config["심볼"]) {
            config["심볼"].forEach((symbol) => {
                if (symbol["트레이딩 바 데이터"] && symbol["트레이딩 바 데이터"]["데이터 경로"]) {
                    const symbolName = symbol["심볼 이름"] || "default";

                    const rawPath = String(symbol["트레이딩 바 데이터"]["데이터 경로"] || '');
                    const posixPath = rawPath.replace(/\\/g, '/');
                    dataPaths[symbolName] = posixPath.replace(/\/[^\/]*$/, "");
                }
            });
        }

        // 지표 데이터 경로 구성
        const indicatorPaths = {};
        if (config && config["지표"]) {
            config["지표"].forEach((indicatorObj) => {
                const indicatorName = indicatorObj["지표 이름"] || "unknown_indicator";
                const indicatorPath = indicatorObj["데이터 경로"] || "";
                const posIndicatorPath = String(indicatorPath || '').replace(/\\/g, '/');

                indicatorPaths[indicatorName] = posIndicatorPath.replace(/\/[^\/]*$/, "");
            });
        }

        // WebSocket 설정
        setupWebSocket(server, dataPaths, indicatorPaths);
    } catch (err) {
        broadcastLog("ERROR", `서버를 시작하는 데 실패했습니다.: ${err}`, null, null);
        process.exit(1);
    }
}

// 실행
main().then();
