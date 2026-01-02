const fs = require("fs");
const fsPromises = fs.promises;
const path = require("path");
const {exec} = require("child_process");
const express = require("express");
const {Server: WebSocketServer} = require("ws");
const net = require("net");
const {
    handleLoadChartData, initializeFallbackImage, findLogoUrl, downloadAndSaveImage
} = require("./ChartDataHandler");
const {startBacktestingEngine, runSingleBacktesting, stopBacktestingEngine} = require("./BacktestingEngine");


// =====================================================================================================================
// 전역 변수
// =====================================================================================================================
const app = express();
let port = Number(process.env.BACKBOARD_PORT);

// 연결된 WebSocket 클라이언트
const activeClients = new Set();

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
        // 무시
    }

    return null;
}

function broadcastLog(level, message, timestamp = null, fileInfo = null) {
    if (message === undefined || message === null) {
        message = '';
    }

    if (typeof message !== 'string') {
        try {
            message = String(message);
        } catch (e) {
            message = '';
        }
    }

    if (!fileInfo) {
        const inferred = getCallerFileInfo();

        if (inferred) {
            fileInfo = inferred;
        }
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
                    action: "backtestingLog", level: level, message: message, timestamp: timestamp, fileInfo: fileInfo
                }));
            }
        });
    } catch (e) {
        // 무시
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
        currentPort++;
    }

    return currentPort;
}

function toPosix(p) {
    if (!p || typeof p !== 'string') {
        return p;
    }

    return p.replace(/\\/g, '/');
}

// =====================================================================================================================
// 정적 파일 제공
// =====================================================================================================================
/*
 DEV vs EXE 규칙

 ○ 공통
  - 프로젝트 폴더: 사용자가 선택한 폴더
  - 동적 폴더: '프로젝트 폴더/Backboard'
  - 아이콘: '프로젝트 폴더/BackBoard/Icons'
  - 로고: '프로젝트 폴더/BackBoard/Logos'

 ○ DEV (개발)
  - Assets | index.html: '프로젝트 폴더/BackBoard/Assets' | '프로젝트 폴더/BackBoard/index.html'

 ○ EXE (배포)
  - Assets | index.html: 'resources/Assets' / 'resources/index.html'
*/

let projectDir = process.env.PROJECT_DIR

let dynamicDir;
let iconDir;
let logoDir;
let staticDir;

// ○ 공통 설정
dynamicDir = path.join(projectDir, 'BackBoard');
app.use('/BackBoard', express.static(dynamicDir));

iconDir = path.join(dynamicDir, 'Icons');
app.use('/Icons', express.static(iconDir));

logoDir = path.join(dynamicDir, 'Logos');
app.use('/Logos', express.static(logoDir));

// ○ 개별 설정
const isDev = process.env.NODE_ENV === 'development';

if (isDev) {
    // DEV: 정적 파일 경로 설정
    staticDir = path.join(projectDir, 'BackBoard');
    app.use(express.static(staticDir));
} else {
    // EXE: 정적 파일 경로 설정
    staticDir = process.resourcesPath;
    app.use(express.static(staticDir));
}

// EXE일 때 번들된 Icons/Logos를 project BackBoard로 복사
async function copyDirIfEmpty(srcDir, dstDir) {
    try {
        const srcStat = await fsPromises.stat(srcDir);

        if (!srcStat.isDirectory()) {
            return;
        }
    } catch (e) {
        return;
    }

    await fsPromises.mkdir(dstDir, {recursive: true});

    try {
        const dstFiles = await fsPromises.readdir(dstDir);

        // 이미 파일이 있으면 복사하지 않음
        if (dstFiles.length > 0) {
            return;
        }
    } catch (e) {
        // 무시
    }

    const entries = await fsPromises.readdir(srcDir, {withFileTypes: true});
    for (const entry of entries) {
        if (entry.isFile()) {
            await fsPromises.copyFile(path.join(srcDir, entry.name), path.join(dstDir, entry.name));
        }
    }
}

async function ensureBundledIconsLogosCopied() {
    if (!projectDir || !staticDir) {
        return;
    }

    try {
        await copyDirIfEmpty(path.join(staticDir, 'Icons'), iconDir);
        await copyDirIfEmpty(path.join(staticDir, 'Logos'), logoDir);
    } catch (e) {
        broadcastLog('WARN', `아이콘/로고 복사 중 오류: ${e.message}`);
    }
}

// =====================================================================================================================
// API: config 가져오기
// =====================================================================================================================
// 서버에서 미리 로드한 config.json을 클라이언트가 요청할 수 있도록 전역 변수와 API 제공
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
    const tradeListPath = path.join(dynamicDir, 'trade_list.json');

    try {
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
            'Cache-Control': 'no-cache, no-store, must-revalidate', 'Pragma': 'no-cache', 'Expires': '0'
        });

        res.json(tradeData);
    } catch (error) {
        res.status(404).json({
            error: 'trade_list.json 파일을 찾을 수 없습니다.',
            attemptedPath: toPosix(tradeListPath),
            projectDir: toPosix(projectDir),
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

    const safeName = path.basename(name);

    if (!iconDir) {
        return res.status(404).send('icon not found');
    }

    const filePath = path.join(iconDir, safeName);

    try {
        await fsPromises.access(filePath, fs.constants.F_OK);

        return res.sendFile(filePath);
    } catch (e) {
        return res.status(404).send('icon not found');
    }

});

// =====================================================================================================================
// API: 로고 가져오기
// =====================================================================================================================
app.get('/api/get-logo', async (req, res) => {
    const {symbol} = req.query;
    if (!symbol) {
        return res.status(400).send('Symbol query parameter is required.');
    }

    const safeSymbolName = symbol.replace(/[^a-zA-Z0-9]/g, '_');

    if (!logoDir) {
        return res.status(404).send('icon not found');
    }

    const filePath = path.join(logoDir, `${safeSymbolName}.png`);

    try {
        await fsPromises.access(filePath, fs.constants.F_OK);

        const origin = (req.protocol && req.get('host')) ? `${req.protocol}://${req.get('host')}` : `http://localhost:${port}`;
        return res.json({logoUrl: `${origin}${isDev ? '/Logos' : '/BackBoard/Logos'}/${safeSymbolName}.png`});
    } catch (e) {
        // 파일이 없으면 다운로드 시도
        try {
            const imageUrl = await findLogoUrl(symbol);
            if (!imageUrl) {
                const origin = (req.protocol && req.get('host')) ? `${req.protocol}://${req.get('host')}` : `http://localhost:${port}`;
                return res.json({logoUrl: `${origin}${isDev ? '/Logos' : '/BackBoard/Logos'}/USDT.png`});
            }

            await fsPromises.mkdir(path.dirname(filePath), {recursive: true});
            await downloadAndSaveImage(imageUrl, filePath);

            // 다운로드 후 존재 확인
            await fsPromises.access(filePath, fs.constants.F_OK);

            const origin = (req.protocol && req.get('host')) ? `${req.protocol}://${req.get('host')}` : `http://localhost:${port}`;
            return res.json({logoUrl: `${origin}${isDev ? '/Logos' : '/BackBoard/Logos'}/${safeSymbolName}.png`});
        } catch (err) {
            // 실패 시 폴백 반환
            broadcastLog('ERROR', `로고 다운로드 실패 (${symbol}): ${err.message}`, null, null);

            const origin = (req.protocol && req.get('host')) ? `${req.protocol}://${req.get('host')}` : `http://localhost:${port}`;
            return res.json({logoUrl: `${origin}${isDev ? '/Logos' : '/BackBoard/Logos'}/USDT.png`});
        }
    }
});

// =====================================================================================================================
// API: 사용 가능한 심볼 목록
// =====================================================================================================================
app.get('/api/symbols', async (req, res) => {
    try {
        // 디렉터리에 있는 데이터로 심볼 목록 추출
        const continuousPath = path.join(projectDir, 'Data', 'Continuous Klines');
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
            const cfg = config['심볼'].map((s) => (s['심볼 이름'])).filter(Boolean).map(String).map(s => s.toUpperCase());
            symbols = Array.from(new Set([...symbols, ...cfg]));
        }

        // 기본으로 포함할 인기 심볼 (빠른 검색용)
        const popularSymbols = ['BTC', 'ETH', 'BNB', 'SOL', 'XRP', 'ADA', 'DOGE', 'DOT', 'LTC', 'LINK', 'TRX', 'NEAR', 'AVAX', 'ATOM'];

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
                error: `파일을 찾을 수 없거나 읽을 수 없습니다: ${toPosix(filePath)}`, details: error.message
            });
        }
    } catch (error) {
        res.status(500).json({
            error: "서버 오류가 발생했습니다.", details: error.message
        });
    }
});

// =====================================================================================================================
// API: 백테스팅 로그
// =====================================================================================================================
app.head('/api/log', async (req, res) => {
    try {
        const logPath = path.join(dynamicDir, 'backtesting.log');

        let filePath = null;
        try {
            await fsPromises.access(logPath, fs.constants.F_OK);

            filePath = logPath;
        } catch (e) {
            // continue
        }

        if (!filePath) {
            return res.status(404).end();
        }

        const stat = await fsPromises.stat(filePath);

        res.set({
            'Content-Length': stat.size, 'Accept-Ranges': 'bytes', 'Cache-Control': 'no-cache'
        });

        return res.status(200).end();
    } catch (err) {
        broadcastLog('ERROR', `로그 메타데이터 응답 실패: ${err.message}`);
        return res.status(500).end();
    }
});

app.get('/api/log', async (req, res) => {
    try {
        const logPath = path.join(dynamicDir, 'backtesting.log');

        let filePath = null;
        try {
            await fsPromises.access(logPath, fs.constants.F_OK);

            filePath = logPath;
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
app.get("*", async (req, res) => {
    const indexPath = path.join(staticDir, "index.html");

    try {
        await fsPromises.access(indexPath, fs.constants.F_OK);
        return res.sendFile(indexPath);
    } catch (e) {
        return res.status(404).send('index.html not found');
    }
});

// =====================================================================================================================
// 서버 시작
// =====================================================================================================================
async function startServer() {
    const available = await isPortAvailable(port);
    if (!available) {
        port = await findAvailablePort(port);
    }

    broadcastLog("INFO", `포트 ${port}에서 서버를 실행합니다.`, null, null);

    return app.listen(port, () => {
        broadcastLog("INFO", `http://localhost:${port}에서 서버가 실행 중입니다.`, null, null);

        // Electron 환경에서는 브라우저를 열지 않음
        if (!(process.env.ELECTRON_RUN === 'true') && process.env.BACKBOARD_OPEN_BROWSER !== 'none') {
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
        validateProjectDirectoryConfig
    } = require("./BacktestingEngine");

    let shutdownTimer = null;
    const SHUTDOWN_DELAY = 500;

    function cancelShutdown() {
        if (shutdownTimer) {
            clearTimeout(shutdownTimer);
            shutdownTimer = null;
        }
    }

    function scheduleShutdown() {
        if (!shutdownTimer) {
            shutdownTimer = setTimeout(() => process.exit(), SHUTDOWN_DELAY);
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

        // 클라이언트 연결 시 editor.json 로드
        (async () => {
            if (!getEditorConfig() && !isEditorConfigLoading()) {
                setEditorConfigLoading(true);

                try {
                    const config = await loadOrCreateEditorConfig(activeClients, broadcastLog, projectDir);

                    if (config && !(await validateProjectDirectoryConfig(config, broadcastLog))) {
                        setEditorConfig(null);
                    } else {
                        setEditorConfig(config);

                        // 프로젝트 폴더가 설정되어 있으면 백테스팅 엔진을 그 프로젝트 디렉터리에서 실행합니다.
                        try {
                            const startDir = config && config.projectDirectory ? config.projectDirectory : projectDir;
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
                    case "loadChartData": {
                        await handleLoadChartData(ws, msg, dataPaths, indicatorPaths);
                        break;
                    }

                    case "runSingleBacktesting": {
                        runSingleBacktesting(ws, msg.symbolConfigs, msg.barDataConfigs, msg.useBarMagnifier, msg.clearAndAddBarData, getEditorConfig(), broadcastLog);
                        break;
                    }

                    case "loadEditorConfig": {
                        if (!getEditorConfig() && !isEditorConfigLoading()) {
                            setEditorConfigLoading(true);

                            try {
                                const config = await loadOrCreateEditorConfig(activeClients, broadcastLog, projectDir);

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
                            action: "editorConfigLoaded", config: configToWs(getEditorConfig() || {
                                logPanelOpen: false,
                                logPanelHeight: 400,
                                projectDirectory: '',
                                useBarMagnifier: true,
                                symbolConfigs: [],
                                barDataConfigs: []
                            })
                        }));

                        break;
                    }

                    case "saveEditorConfig": {
                        const nextConfig = wsToConfig(msg.config);

                        // 클라이언트가 빈 문자열을 보냈는데 서버에는 값이 있다면 서버 값을 유지
                        const currentConfig = getEditorConfig();
                        if (currentConfig && currentConfig.lastDataUpdates && !nextConfig.lastDataUpdates) {
                            nextConfig.lastDataUpdates = currentConfig.lastDataUpdates;
                        }

                        const success = saveEditorConfig(nextConfig, broadcastLog, projectDir);

                        ws.send(JSON.stringify({
                            action: "editorConfigSaved", success: success
                        }));

                        if (success) {
                            setEditorConfig(nextConfig);
                        }

                        break;
                    }

                    default: {
                        ws.send(JSON.stringify({error: "알 수 없는 action"}));
                        break;
                    }
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
        await (async () => {
            try {
                // EXE일 경우 번들된 아이콘/로고를 프로젝트 폴더로 복사 (초기 실행, 파일 누락 등)
                if (!isDev) {
                    await ensureBundledIconsLogosCopied();
                }

                const fallbackPath = path.join(logoDir, 'USDT.png');
                await fsPromises.access(fallbackPath, fs.constants.F_OK);
                // 존재하면 아무것도 안함
            } catch (e) {
                // 존재하지 않으면 생성
                initializeFallbackImage(logoDir).catch(() => {
                });
            }
        })();

        // 서버 시작
        const server = await startServer();

        // config.json에서 데이터 경로 파싱
        const configPath = path.join(dynamicDir, "config.json");

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
                    const symbolName = symbol["심볼 이름"];

                    const rawPath = String(symbol["트레이딩 바 데이터"]["데이터 경로"]);
                    const posixPath = rawPath.replace(/\\/g, '/');
                    dataPaths[symbolName] = posixPath.replace(/\/[^\/]*$/, "");
                }
            });
        }

        // 지표 데이터 경로 구성
        const indicatorPaths = {};
        if (config && config["지표"]) {
            config["지표"].forEach((indicatorObj) => {
                const indicatorName = indicatorObj["지표 이름"];
                const indicatorPath = indicatorObj["데이터 경로"];
                const posIndicatorPath = String(indicatorPath).replace(/\\/g, '/');

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
