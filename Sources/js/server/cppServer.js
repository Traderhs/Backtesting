const fs = require("fs");
const fsPromises = fs.promises;
const path = require("path");
const {spawn} = require("child_process");
const crypto = require("crypto");

// =====================================================================================================================
// C++ 프로세스 관리
// =====================================================================================================================

let cppProcess = null;
let cppProcessReady = false;

function startCppProcess(activeClients, broadcastLog, baseDir) {
    if (cppProcess) {
        broadcastLog("INFO", `C++ 프로세스가 이미 실행 중입니다.`, null, null);
        return;
    }

    const possiblePaths = [
        path.join(path.dirname(process.execPath), 'Backtesting.exe'),
        path.join(path.dirname(process.execPath), 'Backboard', 'Backtesting.exe'),
        path.join('d:', 'Programming', 'Backtesting', 'Builds', 'Release', 'Release', 'Backtesting.exe')
    ];

    // 세 후보 경로 중에서 매칭 시도
    let exePath = null;
    for (const tryPath of possiblePaths) {
        if (fs.existsSync(tryPath)) {
            exePath = tryPath;
            break;
        }
    }

    if (!exePath) {
        broadcastLog("ERROR", `Backtesting.exe 파일을 찾을 수 없습니다.`, null, null);
        return;
    }

    broadcastLog("INFO", `C++ 백테스팅 프로세스를 시작합니다...`, null, null);
    broadcastLog("INFO", `실행 파일: ${exePath}`, null, null);

    // 성능을 위한 NO_PARSE 모드
    const NO_PARSE = process.env.LAUNCH_NO_PARSE === '1' || process.env.BACKBOARD_NO_PARSE === '1';

    try {
        cppProcess = spawn(exePath, ["--server"], {
            cwd: baseDir,
            stdio: ['pipe', 'pipe', 'pipe'],
            windowsHide: false
        });

        broadcastLog("INFO", `C++ 프로세스 PID: ${cppProcess.pid}`, null, null);

        cppProcess.stdout.on('data', (data) => {
            const output = data.toString().trim();
            if (process.env.LAUNCH_DEBUG_LOGS === '0') {
                broadcastLog("INFO", `C++ STDOUT: ${output}`, null, null);
            }

            const ansiRegex = /\x1b\[[0-9;]*[mGKHF]/g;
            const lines = output.split('\n');

            lines.forEach(line => {
                if (!line || !line.trim()) {
                    return;
                }

                const cleaned = line.replace(ansiRegex, '').trim();

                if (/^=+$/.test(cleaned) && cleaned.length >= 50) {
                    broadcastLog("SEPARATOR", "", null, null);
                    return;
                }

                if (cleaned.includes('백테스팅 엔진이 준비되었습니다.')) {
                    cppProcessReady = true;
                    broadcastLog("INFO", `백테스팅 엔진이 준비되었습니다.`, null, null);
                    return;
                }

                if (NO_PARSE) {
                    const level = cleaned.includes('[ERROR]') ? 'ERROR' : 'INFO';
                    broadcastLog(level, cleaned, null, null);
                    return;
                }

                const logMatch = cleaned.match(/^\[([^\]]+)]\s*\[([^\]]+?)]\s*(?:\[([^\]]+)]\s*)?\|\s*(.*)$/);
                if (logMatch) {
                    const [, timestamp, level, fileInfo, message] = logMatch;
                    const formattedTimestamp = timestamp ? timestamp.replace(/-(?=\d{2}:\d{2}:\d{2}$)/, ' ') : null;

                    broadcastLog(level, message, formattedTimestamp, fileInfo || null);
                } else {
                    broadcastLog("INFO", cleaned, null, null);
                }
            });
        });

        cppProcess.stderr.on('data', (data) => {
            if (process.env.LAUNCH_DEBUG_LOGS === '0') {
                const ansiRegex = /\x1b\[[0-9;]*[mGKHF]/g;
                const cleaned = data.toString().trim().replace(ansiRegex, '').trim();

                console.error(`[DEBUG] C++ STDERR: ${cleaned}`);
            }
        });

        cppProcess.on('error', (err) => {
            broadcastLog("ERROR", `C++ 프로세스 실행 실패: ${err.message}`, null, null);

            cppProcess = null;
            cppProcessReady = false;
        });

        cppProcess.on('exit', (code, signal) => {
            const level = code === 0 ? "INFO" : "ERROR";
            const signalInfo = signal ? ` (시그널: ${signal})` : '';

            broadcastLog(level, `C++ 프로세스 종료 (코드: ${code})${signalInfo}`, null, null);

            cppProcess = null;
            cppProcessReady = false;
        });
    } catch (err) {
        broadcastLog("ERROR", `C++ 프로세스 시작 실패: ${err.message}`, null, null);
    }
}

function runSingleBacktesting(ws, symbolConfigs, barDataConfigs, useBarMagnifier, clearAndAddBarData, editorConfig, broadcastLog) {
    if (!cppProcess || !cppProcessReady) {
        broadcastLog("ERROR", "C++ 프로세스가 준비되지 않았습니다. 잠시 후 다시 시도해주세요.", null, null);
        return;
    }

    const config = {
        projectDirectory: (editorConfig && editorConfig.projectDirectory) ? editorConfig.projectDirectory : "",
        useBarMagnifier: useBarMagnifier === undefined ? true : useBarMagnifier,
        clearAndAddBarData: clearAndAddBarData === undefined ? true : clearAndAddBarData,
        symbolConfigs: symbolConfigs || [],
        barDataConfigs: barDataConfigs || [],
    };

    const command = `runSingleBacktesting ${JSON.stringify(config)}\n`;
    cppProcess.stdin.write(command);
}

function stopCppProcess() {
    if (cppProcess) {
        cppProcess.stdin.write("shutdown\n");
        cppProcess.kill();

        cppProcess = null;
        cppProcessReady = false;
    }
}

// =====================================================================================================================
// Config 관리 (editor.json)
// =====================================================================================================================
let editorConfig = null;
let editorConfigLoading = false;
let hmacKey = null;

function getKeyPath(baseDir) {
    return path.join(baseDir, 'Backboard', '.editorkey');
}

function loadOrCreateKey(baseDir, broadcastLog) {
    const keyPath = getKeyPath(baseDir);

    try {
        if (fs.existsSync(keyPath)) {
            hmacKey = fs.readFileSync(keyPath);
        } else {
            hmacKey = crypto.randomBytes(32);

            fs.mkdirSync(path.dirname(keyPath), {recursive: true});
            fs.writeFileSync(keyPath, hmacKey, {mode: 0o600});
        }
    } catch (err) {
        broadcastLog("ERROR", `HMAC 키 로드/생성 실패: ${err.message}`, null, null);

        hmacKey = crypto.randomBytes(32);
    }
}

function stableStringify(value) {
    if (value === null) {
        return 'null';
    }

    const valueType = typeof value;
    if (valueType === 'number' || valueType === 'boolean' || valueType === 'string') {
        return JSON.stringify(value);
    }

    if (valueType === 'undefined' || valueType === 'function' || valueType === 'symbol') {
        return 'null';
    }

    if (Array.isArray(value)) {
        return '[' + value.map((item) => {
            const itemType = typeof item;

            if (item === undefined || itemType === 'function' || itemType === 'symbol') {
                return 'null';
            }

            return stableStringify(item);
        }).join(',') + ']';
    }

    const keys = Object.keys(value)
        .filter((k) => {
            const v = value[k];
            const t = typeof v;

            return !(v === undefined || t === 'function' || t === 'symbol');
        })
        .sort();

    return '{' + keys.map((k) => {
        return JSON.stringify(k) + ':' + stableStringify(value[k]);
    }).join(',') + '}';
}

function canonicalizeConfigV2(obj) {
    const cleanObj = JSON.parse(JSON.stringify(obj || {}));
    delete cleanObj['서명'];

    if (cleanObj['에디터 설정'] && typeof cleanObj['에디터 설정'] === 'object') {
        delete cleanObj['에디터 설정']['서명'];
    }

    return stableStringify(cleanObj);
}

function canonicalizeConfigV1(obj) {
    const cleanObj = JSON.parse(JSON.stringify(obj || {}));
    delete cleanObj['서명'];

    if (cleanObj['에디터 설정'] && typeof cleanObj['에디터 설정'] === 'object') {
        delete cleanObj['에디터 설정']['서명'];
    }

    return JSON.stringify(cleanObj, Object.keys(cleanObj).sort());
}

function computeHmacHex(canonical, baseDir, broadcastLog) {
    if (!hmacKey) {
        loadOrCreateKey(baseDir, broadcastLog);
    }

    const hmac = crypto.createHmac('sha256', hmacKey);
    hmac.update(canonical);

    return hmac.digest('hex');
}

function signConfig(obj, baseDir, broadcastLog) {
    return computeHmacHex(canonicalizeConfigV2(obj), baseDir, broadcastLog);
}

function verifyConfig(obj, baseDir, broadcastLog) {
    if (!obj || typeof obj !== 'object') {
        return 0;
    }

    const providedSigRaw = (obj['에디터 설정'] && typeof obj['에디터 설정'] === 'object' && obj['에디터 설정']['서명']);

    if (!providedSigRaw || typeof providedSigRaw !== 'string') {
        return 0;
    }

    if (!hmacKey) {
        loadOrCreateKey(baseDir, broadcastLog);
    }

    const providedSig = String(providedSigRaw).trim();
    if (!/^[0-9a-fA-F]{64}$/.test(providedSig)) {
        return 0;
    }

    try {
        const providedBuf = Buffer.from(providedSig, 'hex');
        const expectedV2 = computeHmacHex(canonicalizeConfigV2(obj), baseDir, broadcastLog);
        const expectedV2Buf = Buffer.from(expectedV2, 'hex');

        if (crypto.timingSafeEqual(providedBuf, expectedV2Buf)) {
            return 2;
        }

        const expectedV1 = computeHmacHex(canonicalizeConfigV1(obj), baseDir, broadcastLog);
        const expectedV1Buf = Buffer.from(expectedV1, 'hex');

        if (crypto.timingSafeEqual(providedBuf, expectedV1Buf)) {
            return 1;
        }

        return 0;
    } catch (err) {
        return 0;
    }
}

function deleteConfigAndBak(editorConfigPath) {
    try {
        if (fs.existsSync(editorConfigPath)) {
            fs.unlinkSync(editorConfigPath);
        }
    } catch (e) {
        // 무시
    }

    try {
        const bakPath = editorConfigPath + '.bak';

        if (fs.existsSync(bakPath)) {
            fs.unlinkSync(bakPath);
        }
    } catch (e) {
        // 무시
    }
}

function writeSignedFileAtomic(editorConfigPath, obj, baseDir, broadcastLog) {
    try {
        fs.mkdirSync(path.dirname(editorConfigPath), {recursive: true});
        const cleanObj = JSON.parse(JSON.stringify(obj || {}));
        delete cleanObj['서명'];

        if (!cleanObj['에디터 설정'] || typeof cleanObj['에디터 설정'] !== 'object') {
            cleanObj['에디터 설정'] = {};
        }
        delete cleanObj['에디터 설정']['서명'];

        const sig = signConfig(cleanObj, baseDir, broadcastLog);
        const editorSettings = (cleanObj['에디터 설정'] && typeof cleanObj['에디터 설정'] === 'object') ? cleanObj['에디터 설정'] : {};

        const remaining = {...editorSettings};
        delete remaining['서명'];

        const logOpen = remaining.hasOwnProperty('로그 패널 열림') ? remaining['로그 패널 열림'] : '닫힘';
        const logHeight = remaining.hasOwnProperty('로그 패널 높이') ? remaining['로그 패널 높이'] : 400;

        delete remaining['로그 패널 열림'];
        delete remaining['로그 패널 높이'];

        cleanObj['에디터 설정'] = {
            '서명': sig,
            '로그 패널 열림': logOpen,
            '로그 패널 높이': logHeight,
            ...remaining,
        };

        const tmpPath = editorConfigPath + '.tmp';
        const bakPath = editorConfigPath + '.bak';

        fs.writeFileSync(tmpPath, JSON.stringify(cleanObj, null, 2), {encoding: 'utf8'});
        const fd = fs.openSync(tmpPath, 'r+');

        fs.fsyncSync(fd);
        fs.closeSync(fd);

        if (fs.existsSync(editorConfigPath)) {
            if (fs.existsSync(bakPath)) {
                try {
                    fs.unlinkSync(bakPath);
                } catch (e) {
                    // 무시
                }
            }

            fs.renameSync(editorConfigPath, bakPath);
        }

        fs.renameSync(tmpPath, editorConfigPath);

        if (!fs.existsSync(bakPath)) {
            try {
                fs.copyFileSync(editorConfigPath, bakPath);
            } catch (e) {
                // 무시
            }
        }

        return true;
    } catch (err) {
        return false;
    }
}

function readSignedConfigWithRecovery(editorConfigPath, baseDir, broadcastLog) {
    const tryRead = (p, allowMigrate) => {
        try {
            if (!fs.existsSync(p)) {
                return null;
            }

            const raw = fs.readFileSync(p, {encoding: 'utf8'});
            const parsed = JSON.parse(raw);
            const v = verifyConfig(parsed, baseDir, broadcastLog);

            if (v > 0) {
                if (allowMigrate && v === 1) {
                    writeSignedFileAtomic(p, parsed, baseDir, broadcastLog);
                }

                return {obj: parsed, verify: v};
            }

            return null;
        } catch (e) {
            return null;
        }
    };

    const primary = tryRead(editorConfigPath, true);
    if (primary) {
        return primary;
    }

    const bakPath = editorConfigPath + '.bak';
    const backup = tryRead(bakPath, false);
    if (backup) {
        try {
            fs.copyFileSync(bakPath, editorConfigPath);
        } catch (e) {
            // 무시
        }

        const restored = tryRead(editorConfigPath, true);

        return restored || backup;
    }

    return null;
}

function configToObj(config) {
    return {
        "에디터 설정": {
            "서명": "",
            "로그 패널 열림": (config && config.logPanelOpen) ? "열림" : "닫힘",
            "로그 패널 높이": (config && config.logPanelHeight !== undefined) ? config.logPanelHeight : 400,
        },
        "엔진 설정": {
            "프로젝트 폴더": config ? (config.projectDirectory || "") : "",
            "바 돋보기 기능": (config && config.useBarMagnifier) ? "활성화" : "비활성화",
        },
        "심볼 설정": Array.isArray(config && config.symbolConfigs) ? config.symbolConfigs : ((config && config.symbolConfigs) || []),
        "바 데이터 설정": (config && Array.isArray(config.barDataConfigs)) ? config.barDataConfigs.map((bar_cfg) => ({
            "타임프레임": bar_cfg.timeframe,
            "바 데이터 폴더": bar_cfg.klinesDirectory,
            "바 데이터 유형": bar_cfg.barDataType,
        })) : [],
    };
}

function objToConfig(obj) {
    const cfg = {};

    const editor = obj && typeof obj === 'object' ? obj["에디터 설정"] : null;
    cfg.logPanelOpen = editor && (editor["로그 패널 열림"] === "열림");
    cfg.logPanelHeight = editor && (editor["로그 패널 높이"] !== undefined) ? editor["로그 패널 높이"] : 400;

    const engine = obj && typeof obj === 'object' ? obj["엔진 설정"] : null;
    cfg.projectDirectory = engine && (engine["프로젝트 폴더"] !== undefined) ? engine["프로젝트 폴더"] : "";
    cfg.useBarMagnifier = engine && (engine["바 돋보기 기능"] === "활성화");

    cfg.symbolConfigs = obj["심볼 설정"] || [];

    cfg.barDataConfigs = (obj["바 데이터 설정"] || []).map((bar_data_config) => ({
        timeframe: bar_data_config["타임프레임"],
        klinesDirectory: bar_data_config["바 데이터 폴더"],
        barDataType: bar_data_config["바 데이터 유형"],
    }));

    return cfg;
}

function createDefaultConfig(projectDirectory) {
    return {
        logPanelOpen: false,
        logPanelHeight: 400,

        projectDirectory: projectDirectory,
        useBarMagnifier: true,

        symbolConfigs: [],

        barDataConfigs: [
            {
                    timeframe: null,
                klinesDirectory: path.join(projectDirectory, 'Data', 'Continuous Klines'),
                barDataType: '트레이딩'
            },
            {
                    timeframe: null,
                klinesDirectory: path.join(projectDirectory, 'Data', 'Continuous Klines'),
                barDataType: '돋보기'
            },
            {
                    timeframe: null,
                klinesDirectory: path.join(projectDirectory, 'Data', 'Continuous Klines'),
                barDataType: '참조'
            },
            {
                    timeframe: null,
                klinesDirectory: path.join(projectDirectory, 'Data', 'Mark Price Klines'),
                barDataType: '마크 가격'
            }]
    };
}

function configToWs(config) {
    return {
        "에디터 설정": {
            "로그 패널 열림": !!(config && config.logPanelOpen),
            "로그 패널 높이": (config && typeof config.logPanelHeight === 'number') ? config.logPanelHeight : 400,
        },
        "엔진 설정": {
            "프로젝트 폴더": (config && config.projectDirectory) ? config.projectDirectory : "",
            "바 돋보기 기능": !!(config && config.useBarMagnifier),
        },
        "심볼 설정": (config && Array.isArray(config.symbolConfigs)) ? config.symbolConfigs : [],
        "바 데이터 설정": (config && Array.isArray(config.barDataConfigs)) ? config.barDataConfigs : [],
    };
}

function wsToConfig(wsConfig) {
    if (!wsConfig || typeof wsConfig !== 'object') {
        return {
            logPanelOpen: false,
            logPanelHeight: 400,

            useBarMagnifier: true,

            symbolConfigs: [],
            barDataConfigs: []
        };
    }

    if (wsConfig["에디터 설정"] || wsConfig["엔진 설정"] || wsConfig["심볼 설정"] || wsConfig["바 데이터 설정"]) {
        return {
            logPanelOpen: !!wsConfig["에디터 설정"]?.["로그 패널 열림"],
            logPanelHeight: (typeof wsConfig["에디터 설정"]?.["로그 패널 높이"] === 'number') ? wsConfig["에디터 설정"]["로그 패널 높이"] : 400,

            projectDirectory: wsConfig["엔진 설정"]?.["프로젝트 폴더"] || "",
            useBarMagnifier: !!wsConfig["엔진 설정"]?.["바 돋보기 기능"],

            symbolConfigs: Array.isArray(wsConfig["심볼 설정"]) ? wsConfig["심볼 설정"] : [],
            barDataConfigs: Array.isArray(wsConfig["바 데이터 설정"]) ? wsConfig["바 데이터 설정"] : []
        };
    }

    return {
        logPanelOpen: wsConfig.logPanelOpen ?? false,
        logPanelHeight: wsConfig.logPanelHeight ?? 400,

        projectDirectory: wsConfig.projectDirectory || "",
        useBarMagnifier: wsConfig.useBarMagnifier ?? true,

        symbolConfigs: Array.isArray(wsConfig.symbolConfigs) ? wsConfig.symbolConfigs : [],
        barDataConfigs: Array.isArray(wsConfig.barDataConfigs) ? wsConfig.barDataConfigs : []
    };
}

async function fileExists(p) {
    try {
        await fsPromises.access(p, fs.constants.F_OK);

        return true;
    } catch (e) {
        return false;
    }
}

async function ensureDirectory(dirPath) {
    try {
        if (!(await fileExists(dirPath))) {
            await fsPromises.mkdir(dirPath, {recursive: true});
        }

        return true;
    } catch (err) {
        return false;
    }
}

async function createProjectStructure(projectDir) {
    const backboardDir = path.join(projectDir, 'Backboard');

    const continuousDir = path.join(projectDir, 'Data', 'Continuous Klines');
    const markPriceDir = path.join(projectDir, 'Data', 'Mark Price Klines');

    return await ensureDirectory(continuousDir) && await ensureDirectory(markPriceDir) && await ensureDirectory(backboardDir);
}

function getEditorConfigPath(projectDir) {
    return path.join(projectDir, 'Backboard', 'editor.json');
}

async function validateBackboardExe(projectDir) {
    const backboardExePath = path.join(projectDir, 'Backboard.exe');

    if (await fileExists(backboardExePath)) {
        return backboardExePath;
    }

    return null;
}

async function validateProjectDirectoryConfig(cfg, broadcastLog) {
    let projectDirectory = cfg.projectDirectory || null;

    if (!projectDirectory && Array.isArray(cfg.barDataConfigs) && cfg.barDataConfigs.length > 0) {
        const samplePath = cfg.barDataConfigs[0].klinesDirectory || '';

        if (samplePath) {
            projectDirectory = path.dirname(path.dirname(samplePath));
        }
    }

    if (!projectDirectory) {
        broadcastLog("ERROR", `editor.json에서 프로젝트 폴더를 찾을 수 없습니다.`, null, null);

        return false;
    }

    projectDirectory = path.resolve(projectDirectory);

    if (!(await validateBackboardExe(projectDirectory))) {
        broadcastLog("ERROR", `필수 파일이 없습니다: ${path.join(projectDirectory, 'Backboard.exe')}`, null, null);

        return false;
    }

    if (!(await createProjectStructure(projectDirectory))) {
        return false;
    }

    if (Array.isArray(cfg.barDataConfigs)) {
        for (const config of cfg.barDataConfigs) {
            if (!config || !config.klinesDirectory) {
                continue;
            }

            const abs = path.resolve(config.klinesDirectory);
            if (!abs.startsWith(projectDirectory)) {
                broadcastLog("ERROR", `바 데이터 폴더가 프로젝트 폴더 내부에 있지 않습니다: ${abs}`, null, null);

                return false;
            }
        }
    }

    return true;
}

async function loadOrCreateEditorConfig(activeClients, requestProjectDirectoryFromClients, broadcastLog, baseDir) {
    try {
        const defaultEditorPath = getEditorConfigPath(baseDir);

        try {
            await fsPromises.access(defaultEditorPath, fs.constants.F_OK);
            const loaded = readSignedConfigWithRecovery(defaultEditorPath, baseDir, broadcastLog);

            if (loaded) {
                const config = objToConfig(loaded.obj);

                if (await validateProjectDirectoryConfig(config, broadcastLog)) {
                    broadcastLog("INFO", `기본 위치의 editor.json을 찾았습니다: ${defaultEditorPath}`, null, null);

                    return config;
                } else {
                    broadcastLog("WARN", `기본 위치의 editor.json의 검증에 실패했습니다: ${defaultEditorPath}`, null, null);
                }
            } else {
                broadcastLog("WARN", `editor.json 서명 검증 실패: ${defaultEditorPath}`, null, null);
                deleteConfigAndBak(defaultEditorPath);
            }
        } catch (e) {
            // 무시
        }

        if (activeClients && activeClients.size > 0) {
            while (true) {
                broadcastLog("INFO", `Backboard/editor.json이 없습니다. 프로젝트 폴더 입력을 요청합니다.`, null, null);

                const provided = await requestProjectDirectoryFromClients();
                if (!provided || typeof provided !== 'string' || provided.trim() === '') {
                    broadcastLog("WARN", `프로젝트 폴더가 입력되지 않았습니다.`, null, null);

                    try {
                        activeClients.forEach(c => c.send(JSON.stringify({
                            action: 'projectDirectoryInvalid',
                            reason: '프로젝트 폴더를 입력해주세요.'
                        })));
                    } catch (e) {
                        // 무시
                    }

                    continue;
                }

                const resolvedRoot = path.resolve(provided);
                const backboardExe = await validateBackboardExe(resolvedRoot);

                if (!backboardExe) {
                    broadcastLog("WARN", `Backboard.exe 파일을 찾을 수 없습니다: ${resolvedRoot}`, null, null);

                    try {
                        activeClients.forEach(c => c.send(JSON.stringify({
                            action: 'projectDirectoryInvalid',
                            reason: 'Backboard.exe 파일을 찾을 수 없습니다.'
                        })));
                    } catch (e) {
                        // 무시
                    }
                    continue;
                }

                if (await createProjectStructure(resolvedRoot)) {
                    const defaultConfig = createDefaultConfig(resolvedRoot);
                    const editorConfigPath = getEditorConfigPath(resolvedRoot);

                    try {
                        const obj = configToObj(defaultConfig);
                        if (!writeSignedFileAtomic(editorConfigPath, obj, baseDir, broadcastLog)) {
                            broadcastLog("ERROR", '서명 파일 저장 실패');

                            return;
                        }

                        broadcastLog("INFO", `editor.json 파일을 생성했습니다: ${editorConfigPath}`, null, null);

                        return defaultConfig;
                    } catch (err) {
                        broadcastLog("ERROR", `editor.json 생성 실패: ${err.message}`, null, null);

                        try {
                            activeClients.forEach(c => c.send(JSON.stringify({
                                action: 'projectDirectoryInvalid',
                                reason: `설정 파일 생성 실패: ${err.message}`
                            })));
                        } catch (e) {
                            // 무시
                        }
                    }
                } else {
                    try {
                        activeClients.forEach(c => c.send(JSON.stringify({
                            action: 'projectDirectoryInvalid',
                            reason: '프로젝트 폴더 구조 생성 실패'
                        })));
                    } catch (e) {
                        // 무시
                    }
                }
            }
        }
    } catch (e) {
        // 무시
    }
}

function saveEditorConfig(config, broadcastLog, baseDir) {
    const projectDir = config && config.projectDirectory;
    if (!projectDir) {
        broadcastLog("ERROR", `editor.json 저장 실패: 프로젝트 폴더 정보가 없습니다.`, null, null);

        return false;
    }

    try {
        const editorConfigPath = getEditorConfigPath(projectDir);
        fs.mkdirSync(path.dirname(editorConfigPath), {recursive: true});
        const obj = configToObj(config);

        return writeSignedFileAtomic(editorConfigPath, obj, baseDir, broadcastLog);
    } catch (err) {
        broadcastLog("ERROR", `editor.json 저장 실패: ${err}`, null, null);
        return false;
    }
}

async function handleProvideProjectDirectory(ws, projectDir, activeClients, broadcastLog, baseDir) {
    if (projectDir && typeof projectDir === 'string') {
        const resolvedRoot = path.resolve(projectDir);
        const backboardExe = await validateBackboardExe(resolvedRoot);
        const editorConfigPath = getEditorConfigPath(resolvedRoot);

        if (!backboardExe) {
            ws.send(JSON.stringify({
                action: "projectDirectoryInvalid",
                reason: `Backboard.exe 파일을 찾을 수 없습니다.`
            }));
        } else if (await createProjectStructure(resolvedRoot)) {
            try {
                const defaultConfig = createDefaultConfig(resolvedRoot);

                if (await fileExists(editorConfigPath)) {
                    try {
                        const loaded = readSignedConfigWithRecovery(editorConfigPath, baseDir, broadcastLog);

                        if (loaded) {
                            editorConfig = objToConfig(loaded.obj);
                        } else {
                            deleteConfigAndBak(editorConfigPath);
                            const obj = configToObj(defaultConfig);
                            writeSignedFileAtomic(editorConfigPath, obj, baseDir, broadcastLog);

                            editorConfig = defaultConfig;
                        }
                    } catch (e) {
                        broadcastLog("WARN", `editor.json 파싱 실패: ${e.message}`, null, null);

                        deleteConfigAndBak(editorConfigPath);
                        const obj = configToObj(defaultConfig);
                        writeSignedFileAtomic(editorConfigPath, obj, baseDir, broadcastLog);

                        editorConfig = defaultConfig;
                    }
                } else {
                    const obj = configToObj(defaultConfig);
                    writeSignedFileAtomic(editorConfigPath, obj, baseDir, broadcastLog);
                    editorConfig = defaultConfig;
                }

                ws.send(JSON.stringify({
                    action: "editorConfigLoaded",
                    config: configToWs(editorConfig)
                }));
            } catch (err) {
                ws.send(JSON.stringify({
                    action: "projectDirectoryInvalid",
                    reason: `설정 파일 생성 실패: ${err.message}`
                }));
            }
        } else {
            ws.send(JSON.stringify({
                action: "projectDirectoryInvalid",
                reason: `프로젝트 폴더 구조 생성 실패`
            }));
        }
    }
}

function getEditorConfig() {
    return editorConfig;
}

function setEditorConfig(config) {
    editorConfig = config;
}

function isEditorConfigLoading() {
    return editorConfigLoading;
}

function setEditorConfigLoading(loading) {
    editorConfigLoading = loading;
}

module.exports = {
    startCppProcess,
    runSingleBacktesting,
    stopCppProcess,
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
};
