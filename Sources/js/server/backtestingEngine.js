const fs = require("fs");
const fsPromises = fs.promises;
const path = require("path");
const {spawn} = require("child_process");
const crypto = require("crypto");

function toPosix(p) {
    if (!p || typeof p !== 'string') {
        return p;
    }

    return p.replace(/\\/g, '/');
}

// =====================================================================================================================
// C++ 백테스팅 엔진 관리
// =====================================================================================================================

let backtestingEngine = null;
let backtestingEngineReady = false;

function startBacktestingEngine(activeClients, broadcastLog, baseDir) {
    if (backtestingEngine) {
        return;
    }

    const possiblePaths = [path.join(path.dirname(process.execPath), 'Backtesting.exe'), path.join(path.dirname(process.execPath), 'BackBoard', 'Backtesting.exe'), path.join('d:', 'Programming', 'Backtesting', 'Builds', 'Release', 'Release', 'Backtesting.exe')];

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

    // 성능을 위한 NO_PARSE 모드
    const NO_PARSE = process.env.LAUNCH_NO_PARSE === '1' || process.env.BACKBOARD_NO_PARSE === '1';

    try {
        backtestingEngine = spawn(exePath, ["--server"], {
            cwd: baseDir, stdio: ['pipe', 'pipe', 'pipe'], windowsHide: false
        });

        backtestingEngine.stdout.on('data', (data) => {
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

                if (cleaned.includes('백테스팅 엔진 준비 완료')) {
                    backtestingEngineReady = true;
                    broadcastLog("INFO", `백테스팅 엔진이 준비되었습니다.`, null, null);
                    return;
                }

                if (cleaned.startsWith('업데이트 완료')) {
                    try {
                        const payloadText = cleaned.substring('업데이트 완료'.length).trim();
                        const ts = JSON.parse(payloadText);

                        if (typeof ts !== 'string') {
                            return;
                        }

                        if (editorConfig && editorConfig.projectDirectory) {
                            editorConfig.lastDataUpdates = ts;
                            (async () => {
                                await saveEditorConfig(editorConfig, broadcastLog, baseDir);
                            })();

                            // 클라이언트에게 설정 업데이트 전파
                            if (activeClients) {
                                activeClients.forEach(client => {
                                    if (client.readyState === 1) {
                                        client.send(JSON.stringify({
                                            action: "editorConfigLoaded",
                                            config: configToWs(editorConfig)
                                        }));
                                    }
                                });
                            }
                        }
                    } catch (e) {
                        broadcastLog("WARN", `업데이트 완료 파싱 실패: ${e && e.message ? e.message : e}`, null, null);
                    }

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

        backtestingEngine.stderr.on('data', (data) => {
            if (process.env.LAUNCH_DEBUG_LOGS === '0') {
                const ansiRegex = /\x1b\[[0-9;]*[mGKHF]/g;
                const cleaned = data.toString().trim().replace(ansiRegex, '').trim();

                console.error(`[DEBUG] C++ STDERR: ${cleaned}`);
            }
        });

        backtestingEngine.on('error', (err) => {
            broadcastLog("ERROR", `백테스팅 엔진 실행 실패: ${err.message}`, null, null);

            backtestingEngine = null;
            backtestingEngineReady = false;
        });

        backtestingEngine.on('exit', (code, signal) => {
            const level = code === 0 ? "INFO" : "ERROR";
            const signalInfo = signal ? ` (시그널: ${signal})` : '';

            broadcastLog(level, `백테스팅 엔진 종료 (코드: ${code})${signalInfo}`, null, null);

            backtestingEngine = null;
            backtestingEngineReady = false;
        });
    } catch (err) {
        broadcastLog("ERROR", `백테스팅 서버 시작 실패: ${err.message}`, null, null);
    }
}

function runSingleBacktesting(ws, symbolConfigs, barDataConfigs, useBarMagnifier, clearAndAddBarData, editorConfig, broadcastLog) {
    if (!backtestingEngine || !backtestingEngineReady) {
        broadcastLog("ERROR", "백테스팅 엔진이 준비되지 않았습니다. 잠시 후 다시 시도해주세요.", null, null);
        return;
    }

    const config = {
        apiKeyEnvVar: (editorConfig && editorConfig.apiKeyEnvVar) ? editorConfig.apiKeyEnvVar : "",
        apiSecretEnvVar: (editorConfig && editorConfig.apiSecretEnvVar) ? editorConfig.apiSecretEnvVar : "",
        exchangeInfoPath: (editorConfig && editorConfig.exchangeInfoPath) ? toPosix(editorConfig.exchangeInfoPath) : "",
        leverageBracketPath: (editorConfig && editorConfig.leverageBracketPath) ? toPosix(editorConfig.leverageBracketPath) : "",
        lastDataUpdates: (editorConfig && typeof editorConfig.lastDataUpdates === 'string') ? editorConfig.lastDataUpdates : "",
        clearAndAddBarData: clearAndAddBarData !== undefined ? clearAndAddBarData : true,
        symbolConfigs: symbolConfigs || [],
        barDataConfigs: barDataConfigs || [],
        projectDirectory: (editorConfig && editorConfig.projectDirectory) ? editorConfig.projectDirectory : "",
        useBarMagnifier: useBarMagnifier !== undefined ? useBarMagnifier : true
    };

    const command = `runSingleBacktesting ${JSON.stringify(config)}\n`;
    backtestingEngine.stdin.write(command);
}

function stopBacktestingEngine() {
    if (backtestingEngine) {
        backtestingEngine.stdin.write("shutdown\n");
        backtestingEngine.kill();

        backtestingEngine = null;
        backtestingEngineReady = false;
    }
}

// =====================================================================================================================
// Config 관리 (editor.json)
// =====================================================================================================================
let editorConfig = null;
let editorConfigLoading = false;
let hmacKey = null;

function getKeyPath(baseDir) {
    return path.join(baseDir, 'BackBoard', '.editorkey');
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
            '서명': sig, '로그 패널 열림': logOpen, '로그 패널 높이': logHeight, ...remaining,
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
        "거래소 설정": {
            "API 키 환경 변수": (config && typeof config.apiKeyEnvVar === 'string') ? config.apiKeyEnvVar : "",
            "API 시크릿 환경 변수": (config && typeof config.apiSecretEnvVar === 'string') ? config.apiSecretEnvVar : "",

            "거래소 정보 파일 경로": (config && typeof config.exchangeInfoPath === 'string') ? config.exchangeInfoPath : "",
            "레버리지 구간 파일 경로": (config && typeof config.leverageBracketPath === 'string') ? config.leverageBracketPath : "",
            "마지막 데이터 업데이트": (config && typeof config.lastDataUpdates === 'string') ? config.lastDataUpdates : "",
        },
        "심볼 설정": {
            "심볼": Array.isArray(config && config.symbolConfigs) ? config.symbolConfigs : ((config && config.symbolConfigs) || []),
            "페어": {
                "선택된 페어": config ? (config.selectedPair || "") : "",
                "커스텀 페어": Array.isArray(config && config.customPairs) ? config.customPairs : ((config && config.customPairs) || [])
            }
        },
        "바 데이터 설정": (config && Array.isArray(config.barDataConfigs)) ? config.barDataConfigs.map((bar_cfg) => ({
            "타임프레임": bar_cfg.timeframe, "바 데이터 폴더": bar_cfg.klinesDirectory, "바 데이터 유형": bar_cfg.barDataType,
        })) : [],
        "엔진 설정": {
            "프로젝트 폴더": config ? (config.projectDirectory || "") : "",
            "바 돋보기 기능": (config && config.useBarMagnifier) ? "활성화" : "비활성화",
        },
    };
}

function objToConfig(obj) {
    const cfg = {};

    // 에디터 설정
    const editor = obj && typeof obj === 'object' ? obj["에디터 설정"] : null;

    cfg.logPanelOpen = editor && (editor["로그 패널 열림"] === "열림");
    cfg.logPanelHeight = editor && (editor["로그 패널 높이"] !== undefined) ? editor["로그 패널 높이"] : 400;

    // 거래소 설정
    const exchange = obj && typeof obj === 'object' ? obj["거래소 설정"] : null;

    cfg.apiKeyEnvVar = (exchange && typeof exchange["API 키 환경 변수"] === 'string') ? exchange["API 키 환경 변수"] : (engine && typeof engine["API 키 환경 변수"] === 'string' ? engine["API 키 환경 변수"] : (typeof obj["API 키 환경 변수"] === 'string' ? obj["API 키 환경 변수"] : ""));
    cfg.apiSecretEnvVar = (exchange && typeof exchange["API 시크릿 환경 변수"] === 'string') ? exchange["API 시크릿 환경 변수"] : (engine && typeof engine["API 시크릿 환경 변수"] === 'string' ? engine["API 시크릿 환경 변수"] : (typeof obj["API 시크릿 환경 변수"] === 'string' ? obj["API 시크릿 환경 변수"] : ""));

    cfg.exchangeInfoPath = (exchange && typeof exchange["거래소 정보 파일 경로"] === 'string') ? exchange["거래소 정보 파일 경로"] : (engine && typeof engine["거래소 정보 파일 경로"] === 'string' ? engine["거래소 정보 파일 경로"] : (typeof obj["거래소 정보 파일 경로"] === 'string' ? obj["거래소 정보 파일 경로"] : ''));
    cfg.leverageBracketPath = (exchange && typeof exchange["레버리지 구간 파일 경로"] === 'string') ? exchange["레버리지 구간 파일 경로"] : (engine && typeof engine["레버리지 구간 파일 경로"] === 'string' ? engine["레버리지 구간 파일 경로"] : (typeof obj["레버리지 구간 파일 경로"] === 'string' ? obj["레버리지 구간 파일 경로"] : ''));
    cfg.lastDataUpdates = (exchange && typeof exchange["마지막 데이터 업데이트"] === 'string') ? exchange["마지막 데이터 업데이트"] : (engine && typeof engine["마지막 데이터 업데이트"] === 'string' ? engine["마지막 데이터 업데이트"] : "");

    // 심볼 설정
    const symSection = obj && typeof obj === 'object' ? obj["심볼 설정"] : null;

    if (symSection && typeof symSection === 'object') {
        cfg.symbolConfigs = Array.isArray(symSection["심볼"]) ? symSection["심볼"] : [];

        const pairCfgInner = symSection["페어"] || {};

        cfg.selectedPair = typeof pairCfgInner["선택된 페어"] === 'string' ? pairCfgInner["선택된 페어"] : '';
        cfg.customPairs = Array.isArray(pairCfgInner["커스텀 페어"]) ? pairCfgInner["커스텀 페어"] : [];
    }

    // 바 데이터 설정
    cfg.barDataConfigs = (obj["바 데이터 설정"] || []).map((bar_data_config) => ({
        timeframe: bar_data_config["타임프레임"],
        klinesDirectory: bar_data_config["바 데이터 폴더"],
        barDataType: bar_data_config["바 데이터 유형"],
    }));

    // 엔진 설정
    const engine = obj && typeof obj === 'object' ? obj["엔진 설정"] : null;

    cfg.projectDirectory = engine && (engine["프로젝트 폴더"] !== undefined) ? engine["프로젝트 폴더"] : "";
    cfg.useBarMagnifier = engine && (engine["바 돋보기 기능"] === "활성화");

    return cfg;
}

function createDefaultConfig(projectDirectory) {
    return {
        logPanelOpen: false,
        logPanelHeight: 400,

        apiKeyEnvVar: "",
        apiSecretEnvVar: "",

        exchangeInfoPath: toPosix(path.join(projectDirectory, 'Data', 'exchange_info.json')),
        leverageBracketPath: toPosix(path.join(projectDirectory, 'Data', 'leverage_bracket.json')),
        lastDataUpdates: "",

        symbolConfigs: [],
        selectedPair: 'USDT',
        customPairs: [],

        barDataConfigs: [{
            timeframe: null,
            klinesDirectory: toPosix(path.join(projectDirectory, 'Data', 'Continuous Klines')),
            barDataType: '트레이딩'
        }, {
            timeframe: null,
            klinesDirectory: toPosix(path.join(projectDirectory, 'Data', 'Continuous Klines')),
            barDataType: '돋보기'
        }, {
            timeframe: null,
            klinesDirectory: toPosix(path.join(projectDirectory, 'Data', 'Continuous Klines')),
            barDataType: '참조'
        }, {
            timeframe: null,
            klinesDirectory: toPosix(path.join(projectDirectory, 'Data', 'Mark Price Klines')),
            barDataType: '마크 가격'
        }],

        projectDirectory: projectDirectory,
        useBarMagnifier: true,
    };
}

function configToWs(config) {
    return {
        "에디터 설정": {
            "로그 패널 열림": !!(config && config.logPanelOpen),
            "로그 패널 높이": (config && typeof config.logPanelHeight === 'number') ? config.logPanelHeight : 400,
        },
        "거래소 설정": {
            "API 키 환경 변수": (config && typeof config.apiKeyEnvVar === 'string') ? config.apiKeyEnvVar : "",
            "API 시크릿 환경 변수": (config && typeof config.apiSecretEnvVar === 'string') ? config.apiSecretEnvVar : "",

            "거래소 정보 파일 경로": (config && typeof config.exchangeInfoPath === 'string') ? config.exchangeInfoPath : "",
            "레버리지 구간 파일 경로": (config && typeof config.leverageBracketPath === 'string') ? config.leverageBracketPath : "",
            "마지막 데이터 업데이트": (config && typeof config.lastDataUpdates === 'string') ? config.lastDataUpdates : "",
        },
        "심볼 설정": {
            "심볼": (config && Array.isArray(config.symbolConfigs)) ? config.symbolConfigs : [],
            "페어": {
                "선택된 페어": (config && typeof config.selectedPair === 'string') ? config.selectedPair : '',
                "커스텀 페어": (config && Array.isArray(config.customPairs)) ? config.customPairs : []
            }
        },
        "바 데이터 설정": (config && Array.isArray(config.barDataConfigs)) ? config.barDataConfigs.map((b) => ({
            ...b, klinesDirectory: b.klinesDirectory ? toPosix(b.klinesDirectory) : b.klinesDirectory
        })) : [],
        "엔진 설정": {
            "프로젝트 폴더": (config && config.projectDirectory) ? toPosix(config.projectDirectory) : "",
            "바 돋보기 기능": !!(config && config.useBarMagnifier),
        },
    };
}

function wsToConfig(wsConfig) {
    if (!wsConfig || typeof wsConfig !== 'object') {
        return {
            logPanelOpen: false, logPanelHeight: 400,

            useBarMagnifier: true,

            symbolConfigs: [], barDataConfigs: []
        };
    }

    if (wsConfig["에디터 설정"] || wsConfig["엔진 설정"] || wsConfig["심볼 설정"] || wsConfig["바 데이터 설정"] || wsConfig["거래소 설정"]) {
        return {
            logPanelOpen: !!wsConfig["에디터 설정"]?.["로그 패널 열림"],
            logPanelHeight: (typeof wsConfig["에디터 설정"]?.["로그 패널 높이"] === 'number') ? wsConfig["에디터 설정"]["로그 패널 높이"] : 400,

            apiKeyEnvVar: (wsConfig["거래소 설정"] && typeof wsConfig["거래소 설정"]["API 키 환경 변수"] === 'string') ? wsConfig["거래소 설정"]["API 키 환경 변수"] : (wsConfig["엔진 설정"] && typeof wsConfig["엔진 설정"]["API 키 환경 변수"] === 'string' ? wsConfig["엔진 설정"]["API 키 환경 변수"] : ''),
            apiSecretEnvVar: (wsConfig["거래소 설정"] && typeof wsConfig["거래소 설정"]["API 시크릿 환경 변수"] === 'string') ? wsConfig["거래소 설정"]["API 시크릿 환경 변수"] : (wsConfig["엔진 설정"] && typeof wsConfig["엔진 설정"]["API 시크릿 환경 변수"] === 'string' ? wsConfig["엔진 설정"]["API 시크릿 환경 변수"] : ''),

            exchangeInfoPath: (wsConfig["거래소 설정"] && typeof wsConfig["거래소 설정"]["거래소 정보 파일 경로"] === 'string') ? wsConfig["거래소 설정"]["거래소 정보 파일 경로"] : (wsConfig["엔진 설정"] && typeof wsConfig["엔진 설정"]["거래소 정보 파일 경로"] === 'string' ? wsConfig["엔진 설정"]["거래소 정보 파일 경로"] : ''),
            leverageBracketPath: (wsConfig["거래소 설정"] && typeof wsConfig["거래소 설정"]["레버리지 구간 파일 경로"] === 'string') ? wsConfig["거래소 설정"]["레버리지 구간 파일 경로"] : (wsConfig["엔진 설정"] && typeof wsConfig["엔진 설정"]["레버리지 구간 파일 경로"] === 'string' ? wsConfig["엔진 설정"]["레버리지 구간 파일 경로"] : ''),
            lastDataUpdates: (wsConfig["거래소 설정"] && typeof wsConfig["거래소 설정"]["마지막 데이터 업데이트"] === 'string') ? wsConfig["거래소 설정"]["마지막 데이터 업데이트"] : (wsConfig["엔진 설정"] && typeof wsConfig["엔진 설정"]["마지막 데이터 업데이트"] === 'string' ? wsConfig["엔진 설정"]["마지막 데이터 업데이트"] : ''),

            symbolConfigs: Array.isArray(wsConfig["심볼 설정"]?.["심볼"]) ? wsConfig["심볼 설정"]["심볼"] : [],
            selectedPair: (wsConfig["심볼 설정"] && wsConfig["심볼 설정"]["페어"] && typeof wsConfig["심볼 설정"]["페어"]["선택된 페어"] === 'string') ? wsConfig["심볼 설정"]["페어"]["선택된 페어"] : '',
            customPairs: (wsConfig["심볼 설정"] && wsConfig["심볼 설정"]["페어"] && Array.isArray(wsConfig["심볼 설정"]["페어"]["커스텀 페어"])) ? wsConfig["심볼 설정"]["페어"]["커스텀 페어"] : [],

            barDataConfigs: Array.isArray(wsConfig["바 데이터 설정"]) ? wsConfig["바 데이터 설정"] : [],

            projectDirectory: wsConfig["엔진 설정"]?.["프로젝트 폴더"] || "",
            useBarMagnifier: !!wsConfig["엔진 설정"]?.["바 돋보기 기능"]
        };
    }

    return {
        logPanelOpen: wsConfig.logPanelOpen ?? false,
        logPanelHeight: wsConfig.logPanelHeight ?? 400,

        apiKeyEnvVar: typeof wsConfig.apiKeyEnvVar === 'string' ? wsConfig.apiKeyEnvVar : '',
        apiSecretEnvVar: typeof wsConfig.apiSecretEnvVar === 'string' ? wsConfig.apiSecretEnvVar : '',

        exchangeInfoPath: typeof wsConfig.exchangeInfoPath === 'string' ? wsConfig.exchangeInfoPath : '',
        leverageBracketPath: typeof wsConfig.leverageBracketPath === 'string' ? wsConfig.leverageBracketPath : '',
        lastDataUpdates: (typeof wsConfig.lastDataUpdates === 'string') ? wsConfig.lastDataUpdates : '',

        symbolConfigs: Array.isArray(wsConfig.symbolConfigs) ? wsConfig.symbolConfigs : [],
        selectedPair: typeof wsConfig.selectedPair === 'string' ? wsConfig.selectedPair : '',
        customPairs: Array.isArray(wsConfig.customPairs) ? wsConfig.customPairs : [],

        barDataConfigs: Array.isArray(wsConfig.barDataConfigs) ? wsConfig.barDataConfigs : [],

        projectDirectory: wsConfig.projectDirectory || "",
        useBarMagnifier: wsConfig.useBarMagnifier ?? true
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
    const backboardDir = path.join(projectDir, 'BackBoard');

    const continuousDir = path.join(projectDir, 'Data', 'Continuous Klines');
    const markPriceDir = path.join(projectDir, 'Data', 'Mark Price Klines');

    return await ensureDirectory(continuousDir) && await ensureDirectory(markPriceDir) && await ensureDirectory(backboardDir);
}

function getEditorConfigPath(projectDir) {
    return path.join(projectDir, 'BackBoard', 'editor.json');
}

async function validateBackBoardExe(projectDir) {
    const backboardExePath = path.join(projectDir, 'BackBoard.exe');

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

    if (!(await validateBackBoardExe(projectDirectory))) {
        broadcastLog("ERROR", `필수 파일이 없습니다: ${toPosix(path.join(projectDirectory, 'BackBoard.exe'))}`, null, null);

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
                broadcastLog("ERROR", `바 데이터 폴더가 프로젝트 폴더 내부에 있지 않습니다: ${toPosix(abs)}`, null, null);

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
                    broadcastLog("INFO", `기본 위치의 editor.json을 찾았습니다: ${toPosix(defaultEditorPath)}`, null, null);

                    return config;
                } else {
                    broadcastLog("WARN", `기본 위치의 editor.json의 검증에 실패했습니다: ${toPosix(defaultEditorPath)}`, null, null);
                }
            } else {
                broadcastLog("WARN", `editor.json 서명 검증 실패: ${toPosix(defaultEditorPath)}`, null, null);
                deleteConfigAndBak(defaultEditorPath);
            }
        } catch (e) {
            // 무시
        }

        if (activeClients && activeClients.size > 0) {
            while (true) {
                broadcastLog("INFO", `BackBoard/editor.json이 없습니다. 프로젝트 폴더 입력을 요청합니다.`, null, null);

                const provided = await requestProjectDirectoryFromClients();
                if (!provided || typeof provided !== 'string' || provided.trim() === '') {
                    broadcastLog("WARN", `프로젝트 폴더가 입력되지 않았습니다.`, null, null);

                    try {
                        activeClients.forEach(c => c.send(JSON.stringify({
                            action: 'projectDirectoryInvalid', reason: '프로젝트 폴더를 입력해주세요.'
                        })));
                    } catch (e) {
                        // 무시
                    }

                    continue;
                }

                const resolvedRoot = path.resolve(provided);
                const backboardExe = await validateBackBoardExe(resolvedRoot);

                if (!backboardExe) {
                    broadcastLog("WARN", `BackBoard.exe 파일을 찾을 수 없습니다: ${toPosix(resolvedRoot)}`, null, null);

                    try {
                        activeClients.forEach(c => c.send(JSON.stringify({
                            action: 'projectDirectoryInvalid', reason: 'BackBoard.exe 파일을 찾을 수 없습니다.'
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

                        return defaultConfig;
                    } catch (err) {
                        broadcastLog("ERROR", `editor.json 생성 실패: ${err.message}`, null, null);

                        try {
                            activeClients.forEach(c => c.send(JSON.stringify({
                                action: 'projectDirectoryInvalid', reason: `설정 파일 생성 실패: ${err.message}`
                            })));
                        } catch (e) {
                            // 무시
                        }
                    }
                } else {
                    try {
                        activeClients.forEach(c => c.send(JSON.stringify({
                            action: 'projectDirectoryInvalid', reason: '프로젝트 폴더 구조 생성 실패'
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
        const backboardExe = await validateBackBoardExe(resolvedRoot);
        const editorConfigPath = getEditorConfigPath(resolvedRoot);

        if (!backboardExe) {
            ws.send(JSON.stringify({
                action: "projectDirectoryInvalid", reason: `BackBoard.exe 파일을 찾을 수 없습니다.`
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

                // 프로젝트 디렉터리 설정 후 백테스팅 엔진이 아직 시작되지 않았으면 시작
                try {
                    startBacktestingEngine(activeClients, broadcastLog, resolvedRoot);
                } catch (e) {
                    broadcastLog("ERROR", `백테스팅 엔진 시작 실패: ${e.message}`, null, null);
                }

                ws.send(JSON.stringify({
                    action: "editorConfigLoaded", config: configToWs(editorConfig)
                }));
            } catch (err) {
                ws.send(JSON.stringify({
                    action: "projectDirectoryInvalid", reason: `설정 파일 생성 실패: ${err.message}`
                }));
            }
        } else {
            ws.send(JSON.stringify({
                action: "projectDirectoryInvalid", reason: `프로젝트 폴더 구조 생성 실패`
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
    startBacktestingEngine: startBacktestingEngine,
    runSingleBacktesting,
    stopBacktestingEngine: stopBacktestingEngine,
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
