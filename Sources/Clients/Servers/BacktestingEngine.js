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

let _projectDirForEngine = null;
const _pendingBacktestRequesters = [];

function startBacktestingEngine(activeClients, broadcastLog, projectDir, staticDir) {
    if (backtestingEngine) {
        return;
    }

    // 프로젝트 경로를 보관
    _projectDirForEngine = projectDir;

    const exePath = path.join(staticDir, 'Cores', 'Backtesting.exe');
    if (!fs.existsSync(exePath)) {
        return;
    }

    // 성능을 위한 NO_PARSE 모드
    const NO_PARSE = process.env.LAUNCH_NO_PARSE === '1' || process.env.BACKBOARD_NO_PARSE === '1';

    // 백테스팅 실패를 전파
    const notifyAllClientsBacktestingFailed = () => {
        if (activeClients && activeClients.size) {
            activeClients.forEach((c) => {
                try {
                    if (c && c.readyState === 1) {
                        c.send(JSON.stringify({action: 'backtestingFailed'}));
                    }
                } catch (e) {
                    // 무시
                }
            });
        }

        try {
            const requester = _pendingBacktestRequesters.shift();
            if (requester && requester.readyState === 1) {
                requester.send(JSON.stringify({action: 'backtestingFailed'}));
            }
        } catch (e) {
            // 무시
        }
    };

    try {
        backtestingEngine = spawn(exePath, ["--server"], {
            cwd: projectDir, stdio: ['pipe', 'pipe', 'pipe'], windowsHide: false
        });

        backtestingEngine.stdout.on('data', (data) => {
            const output = data.toString().trim();
            if (process.env.LAUNCH_DEBUG_LOGS === '0') {
                broadcastLog("INFO", `C++ STDOUT: ${output}`, null, null);
            }

            const ansiRegex = /\x1b\[[0-9;]*[mGKHF]/g;
            const lines = output.split('\n');

            lines.forEach(async (line) => {
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

                // 백테스팅이 정상적으로 완료되었을 때, 해당 작업을 요청한 클라이언트에게만 성공 알림을 보냄
                if (cleaned.includes('백테스팅이 완료되었습니다.')) {
                    const parsed = cleaned.match(/^\[([^\]]+)]\s*\[([^\]]+?)]\s*(?:\[([^\]]+)]\s*)?\|\s*(.*)$/);
                    if (parsed) {
                        const [, timestamp, level, fileInfo, message] = parsed;
                        const formattedTimestamp = timestamp ? timestamp.replace(/-(?=\d{2}:\d{2}:\d{2}$)/, ' ') : null;
                        broadcastLog(level || 'INFO', message || '', formattedTimestamp, fileInfo || null);
                    } else {
                        // 포맷이 다를 경우 안전하게 전체 문자열을 INFO로 전파
                        broadcastLog('INFO', cleaned, null, null);
                    }

                    try {
                        // 가장 최신의 결과 폴더를 찾아 응답자에게 송신
                        let latestName = null;
                        const resultsDir = path.join(_projectDirForEngine || process.cwd(), 'Results');

                        try {
                            const entries = await fsPromises.readdir(resultsDir, {withFileTypes: true});
                            const dirs = entries.filter(e => e.isDirectory()).map(d => d.name);
                            const stats = await Promise.all(dirs.map(async (d) => {
                                try {
                                    const p = path.join(resultsDir, d);
                                    const st = await fsPromises.stat(p);

                                    return {name: d, mtime: st.mtimeMs};
                                } catch (e) {
                                    return null;
                                }
                            }));

                            const valid = stats.filter(Boolean).sort((a, b) => (b.mtime - a.mtime));
                            if (valid.length > 0) {
                                latestName = valid[0].name;
                            }
                        } catch (e) {
                            latestName = null;
                        }

                        // 큐에서 요청자(ws)를 꺼내 해당 클라이언트에만 알림 전송
                        async function ensureLogReady(name) {
                            if (!name) {
                                return false;
                            }

                            const p = path.join(resultsDir, name, 'backtesting.log');
                            const deadline = Date.now() + 3000; // 최대 3s 대기
                            while (Date.now() < deadline) {
                                try {
                                    const st = await fsPromises.stat(p);
                                    if (st.isFile() && st.size > 0) {
                                        return true;
                                    }
                                } catch (err) {
                                    // 파일이 아직 없으면 무시하고 재시도
                                }

                                await new Promise(r => setTimeout(r, 200));
                            }
                            return false;
                        }

                        const requester = _pendingBacktestRequesters.shift();
                        if (requester && requester.readyState === 1) {
                            const payload = {action: 'backtestingSuccess'};

                            if (latestName) {
                                const ready = await ensureLogReady(latestName).catch(() => false);
                                if (ready) {
                                    payload.resultName = latestName;
                                } else {
                                    // 로그가 준비되지 않았더라도 결과는 존재할 수 있으므로 경고 후 이름을 포함
                                    broadcastLog('WARN', `백테스팅 로그가 아직 준비되지 않았습니다`);
                                    payload.resultName = latestName;
                                }
                            }

                            try {
                                requester.send(JSON.stringify(payload));
                            } catch (e) {
                                // 무시
                            }
                        }
                    } catch (e) {
                        // 안전하게 무시
                        broadcastLog("WARN", `백테스팅 완료 후 알림 처리 중 오류: ${e && e.message ? e.message : e}`, null, null);
                    }

                    return;
                }

                if (cleaned.includes('백테스팅이 중지되었습니다.')) {
                    broadcastLog('INFO', '백테스팅이 중지되었습니다.', null, null);

                    if (activeClients && activeClients.size) {
                        activeClients.forEach((c) => {
                            try {
                                if (c && c.readyState === 1) {
                                    c.send(JSON.stringify({action: 'backtestingStopped'}));
                                }
                            } catch (e) {
                                // 무시
                            }
                        });
                    }

                    return;
                }

                // 백테스팅 오류를 즉시 클라이언트에 전파하여 UI가 고착되지 않도록 처리
                if (cleaned.includes('단일 백테스팅 실행 중 오류가 발생했습니다.')) {
                    broadcastLog('ERROR', '단일 백테스팅 실행 중 오류가 발생했습니다.', null, null);
                    notifyAllClientsBacktestingFailed();
                    return;
                }

                if (cleaned.includes('바 데이터 다운로드가 중지되었습니다.') || cleaned.includes('바 데이터 업데이트가 중지되었습니다.')) {
                    broadcastLog('INFO', cleaned, null, null);

                    if (activeClients && activeClients.size) {
                        activeClients.forEach((c) => {
                            try {
                                if (c && c.readyState === 1) {
                                    c.send(JSON.stringify({action: 'fetchStopped'}));
                                }
                            } catch (e) {
                                // 무시
                            }
                        });
                    }

                    return;
                }

                if (cleaned.includes('바 데이터 다운로드가 완료되었습니다.') || cleaned.includes('바 데이터 업데이트가 완료되었습니다.')) {
                    const message = cleaned.includes('다운로드') ? '바 데이터 다운로드가 완료되었습니다.' : '바 데이터 업데이트가 완료되었습니다.';
                    broadcastLog('INFO', message, null, null);

                    if (activeClients && activeClients.size) {
                        activeClients.forEach((c) => {
                            try {
                                if (c && c.readyState === 1) {
                                    c.send(JSON.stringify({action: 'fetchSuccess'}));
                                }
                            } catch (e) {
                                // 무시
                            }
                        });
                    }

                    return;
                }

                if (cleaned.includes('바 데이터 다운로드/업데이트가 실패했습니다.')) {
                    broadcastLog('ERROR', '바 데이터 다운로드/업데이트가 실패했습니다.', null, null);

                    if (activeClients && activeClients.size) {
                        activeClients.forEach((c) => {
                            try {
                                if (c && c.readyState === 1) {
                                    c.send(JSON.stringify({action: 'fetchFailed'}));
                                }
                            } catch (e) {
                                // 무시
                            }
                        });
                    }

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
                            // '마지막 데이터 업데이트' 항목만 갱신
                            try {
                                const editorConfigPath = getEditorConfigPath(editorConfig.projectDirectory);
                                const loaded = readSignedConfigWithRecovery(editorConfigPath, editorConfig.projectDirectory, broadcastLog);
                                let obj;

                                if (loaded && loaded.obj) {
                                    obj = loaded.obj;
                                } else {
                                    // 파일이 없거나 손상되었으면 기본 config로 채움
                                    obj = configToObj(createDefaultConfig(editorConfig.projectDirectory));
                                }

                                obj['거래소 설정'] = obj['거래소 설정'] || {};
                                obj['거래소 설정']['마지막 데이터 업데이트'] = ts;

                                if (writeSignedFileAtomic(editorConfigPath, obj, editorConfig.projectDirectory, broadcastLog)) {
                                    const newConfig = objToConfig(obj);
                                    setEditorConfig(newConfig);

                                    // 전체 설정을 전파하면 사용자 UI 상태가 덮어써질 수 있으므로 변경된 필드만 전파
                                    if (activeClients) {
                                        activeClients.forEach(client => {
                                            if (client.readyState === 1) {
                                                client.send(JSON.stringify({
                                                    action: "lastDataUpdates", lastDataUpdates: ts
                                                }));
                                            }
                                        });
                                    }
                                } else {
                                    broadcastLog("ERROR", `editor.json 저장 실패 (마지막 데이터 업데이트 갱신)`, null, null);
                                }
                            } catch (e) {
                                broadcastLog("WARN", `마지막 데이터 업데이트 저장 중 오류: ${e && e.message ? e.message : e}`, null, null);
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

            // 프로세스 실행 오류인 경우에도 클라이언트 상태 해제
            notifyAllClientsBacktestingFailed();

            backtestingEngine = null;
            backtestingEngineReady = false;
        });

        backtestingEngine.on('exit', (code, signal) => {
            const level = code === 0 ? "INFO" : "ERROR";
            const signalInfo = signal ? ` (시그널: ${signal})` : '';

            broadcastLog(level, `백테스팅 엔진 종료 (코드: ${code})${signalInfo}`, null, null);

            // 비정상 종료시 클라이언트에게 실패 알림 전파
            if (code !== 0) {
                notifyAllClientsBacktestingFailed();
            }

            backtestingEngine = null;
            backtestingEngineReady = false;
        });
    } catch (err) {
        broadcastLog("ERROR", `백테스팅 서버 시작 실패: ${err.message}`, null, null);
    }
}

function runSingleBacktesting(ws, backtestingStartTime, editorConfig, symbolConfigs, barDataConfigs, useBarMagnifier, clearAndAddBarData, fundingRatesDirectory, strategyConfig, broadcastLog) {
    if (!backtestingEngine || !backtestingEngineReady) {
        broadcastLog("ERROR", "백테스팅 엔진이 준비되지 않았습니다. 잠시 후 다시 시도해주세요.", null, null);
        return;
    }

    const config = {
        backtestingStartTime: backtestingStartTime,

        apiKeyEnvVar: (editorConfig && editorConfig.apiKeyEnvVar) ? editorConfig.apiKeyEnvVar : "",
        apiSecretEnvVar: (editorConfig && editorConfig.apiSecretEnvVar) ? editorConfig.apiSecretEnvVar : "",

        exchangeInfoPath: (editorConfig && editorConfig.exchangeInfoPath) ? toPosix(editorConfig.exchangeInfoPath) : "",
        leverageBracketPath: (editorConfig && editorConfig.leverageBracketPath) ? toPosix(editorConfig.leverageBracketPath) : "",
        lastDataUpdates: (editorConfig && typeof editorConfig.lastDataUpdates === 'string') ? editorConfig.lastDataUpdates : "",

        clearAndAddBarData: clearAndAddBarData !== undefined ? clearAndAddBarData : true,

        symbolConfigs: symbolConfigs || [],

        barDataConfigs: barDataConfigs || [],

        fundingRatesDirectory: fundingRatesDirectory,

        projectDirectory: (editorConfig && editorConfig.projectDirectory) ? editorConfig.projectDirectory : "",

        useBacktestPeriodStart: (editorConfig && typeof editorConfig.useBacktestPeriodStart === 'boolean') ? editorConfig.useBacktestPeriodStart : true,
        useBacktestPeriodEnd: (editorConfig && typeof editorConfig.useBacktestPeriodEnd === 'boolean') ? editorConfig.useBacktestPeriodEnd : true,
        backtestPeriodStart: (editorConfig && editorConfig.backtestPeriodStart) ? editorConfig.backtestPeriodStart : "",
        backtestPeriodEnd: (editorConfig && editorConfig.backtestPeriodEnd) ? editorConfig.backtestPeriodEnd : "",
        backtestPeriodFormat: (editorConfig && editorConfig.backtestPeriodFormat) ? editorConfig.backtestPeriodFormat : "%Y-%m-%d %H:%M:%S",

        useBarMagnifier: useBarMagnifier !== undefined ? useBarMagnifier : true,

        initialBalance: (editorConfig && typeof editorConfig.initialBalance === 'number') ? editorConfig.initialBalance : undefined,

        takerFeePercentage: (editorConfig && typeof editorConfig.takerFeePercentage === 'number') ? editorConfig.takerFeePercentage : undefined,
        makerFeePercentage: (editorConfig && typeof editorConfig.makerFeePercentage === 'number') ? editorConfig.makerFeePercentage : undefined,

        slippageModel: (editorConfig && editorConfig.slippageModel) ? editorConfig.slippageModel : "MarketImpactSlippage",
        slippageTakerPercentage: (editorConfig && typeof editorConfig.slippageTakerPercentage === 'number') ? editorConfig.slippageTakerPercentage : undefined,
        slippageMakerPercentage: (editorConfig && typeof editorConfig.slippageMakerPercentage === 'number') ? editorConfig.slippageMakerPercentage : undefined,
        slippageStressMultiplier: (editorConfig && typeof editorConfig.slippageStressMultiplier === 'number') ? editorConfig.slippageStressMultiplier : undefined,

        checkMarketMaxQty: (editorConfig && typeof editorConfig.checkMarketMaxQty === 'boolean') ? editorConfig.checkMarketMaxQty : true,
        checkMarketMinQty: (editorConfig && typeof editorConfig.checkMarketMinQty === 'boolean') ? editorConfig.checkMarketMinQty : true,
        checkLimitMaxQty: (editorConfig && typeof editorConfig.checkLimitMaxQty === 'boolean') ? editorConfig.checkLimitMaxQty : true,
        checkLimitMinQty: (editorConfig && typeof editorConfig.checkLimitMinQty === 'boolean') ? editorConfig.checkLimitMinQty : true,
        checkMinNotionalValue: (editorConfig && typeof editorConfig.checkMinNotionalValue === 'boolean') ? editorConfig.checkMinNotionalValue : true,

        checkSameBarDataWithTarget: (editorConfig && typeof editorConfig.checkSameBarDataWithTarget === 'boolean') ? editorConfig.checkSameBarDataWithTarget : true,
        checkSameBarDataTrading: (editorConfig && typeof editorConfig.checkSameBarDataTrading === 'boolean') ? editorConfig.checkSameBarDataTrading : true,
        checkSameBarDataMagnifier: (editorConfig && typeof editorConfig.checkSameBarDataMagnifier === 'boolean') ? editorConfig.checkSameBarDataMagnifier : true,
        checkSameBarDataReference: (editorConfig && typeof editorConfig.checkSameBarDataReference === 'boolean') ? editorConfig.checkSameBarDataReference : true,
        checkSameBarDataMarkPrice: (editorConfig && typeof editorConfig.checkSameBarDataMarkPrice === 'boolean') ? editorConfig.checkSameBarDataMarkPrice : true,

        strategyConfig: strategyConfig || null
    };

    // 요청자(ws)를 대기열에 추가
    try {
        if (ws && ws.send && typeof ws.send === 'function') {
            _pendingBacktestRequesters.push(ws);
        }
    } catch (e) {
        // 무시
    }

    const command = `runSingleBacktesting ${JSON.stringify(config)}\n`;
    backtestingEngine.stdin.write(command);
}

function fetchOrUpdateBarData(ws, operation, editorConfig, symbolConfigs, barDataConfigs, fundingRatesDirectory, broadcastLog) {
    if (!backtestingEngine || !backtestingEngineReady) {
        broadcastLog("ERROR", "백테스팅 엔진이 준비되지 않았습니다. 잠시 후 다시 시도해주세요.", null, null);
        return;
    }

    const config = {
        operation: operation,

        apiKeyEnvVar: (editorConfig && editorConfig.apiKeyEnvVar) ? editorConfig.apiKeyEnvVar : "",
        apiSecretEnvVar: (editorConfig && editorConfig.apiSecretEnvVar) ? editorConfig.apiSecretEnvVar : "",

        symbolConfigs: symbolConfigs || [],

        barDataConfigs: barDataConfigs || [],

        fundingRatesDirectory: fundingRatesDirectory,

        projectDirectory: (editorConfig && editorConfig.projectDirectory) ? editorConfig.projectDirectory : "",
    };

    const command = `fetchOrUpdateBarData ${JSON.stringify(config)}\n`;
    backtestingEngine.stdin.write(command);
}

function stopSingleBacktesting() {
    if (!backtestingEngine || !backtestingEngineReady) {
        return;
    }

    backtestingEngine.stdin.write("stopSingleBacktesting\n");
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

function getKeyPath(projectDir) {
    return path.join(projectDir, 'BackBoard', '.editorkey');
}

function loadOrCreateKey(projectDir, broadcastLog) {
    const keyPath = getKeyPath(projectDir);

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

function computeHmacHex(canonical, projectDir, broadcastLog) {
    if (!hmacKey) {
        loadOrCreateKey(projectDir, broadcastLog);
    }

    const hmac = crypto.createHmac('sha256', hmacKey);
    hmac.update(canonical);

    return hmac.digest('hex');
}

function signConfig(obj, projectDir, broadcastLog) {
    return computeHmacHex(canonicalizeConfigV2(obj), projectDir, broadcastLog);
}

function verifyConfig(obj, projectDir, broadcastLog) {
    if (!obj || typeof obj !== 'object') {
        return 0;
    }

    const providedSigRaw = (obj['에디터 설정'] && typeof obj['에디터 설정'] === 'object' && obj['에디터 설정']['서명']);

    if (!providedSigRaw || typeof providedSigRaw !== 'string') {
        return 0;
    }

    if (!hmacKey) {
        loadOrCreateKey(projectDir, broadcastLog);
    }

    const providedSig = String(providedSigRaw).trim();
    if (!/^[0-9a-fA-F]{64}$/.test(providedSig)) {
        return 0;
    }

    try {
        const providedBuf = Buffer.from(providedSig, 'hex');
        const expectedV2 = computeHmacHex(canonicalizeConfigV2(obj), projectDir, broadcastLog);
        const expectedV2Buf = Buffer.from(expectedV2, 'hex');

        if (crypto.timingSafeEqual(providedBuf, expectedV2Buf)) {
            return 2;
        }

        const expectedV1 = computeHmacHex(canonicalizeConfigV1(obj), projectDir, broadcastLog);
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

function writeSignedFileAtomic(editorConfigPath, obj, projectDir, broadcastLog) {
    try {
        fs.mkdirSync(path.dirname(editorConfigPath), {recursive: true});
        const cleanObj = JSON.parse(JSON.stringify(obj || {}));
        delete cleanObj['서명'];

        if (!cleanObj['에디터 설정'] || typeof cleanObj['에디터 설정'] !== 'object') {
            cleanObj['에디터 설정'] = {};
        }
        delete cleanObj['에디터 설정']['서명'];

        const sig = signConfig(cleanObj, projectDir, broadcastLog);
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

function readSignedConfigWithRecovery(editorConfigPath, projectDir, broadcastLog) {
    const tryRead = (p, allowMigrate) => {
        try {
            if (!fs.existsSync(p)) {
                return null;
            }

            const raw = fs.readFileSync(p, {encoding: 'utf8'});
            const parsed = JSON.parse(raw);
            const v = verifyConfig(parsed, projectDir, broadcastLog);

            if (v > 0) {
                if (allowMigrate && v === 1) {
                    writeSignedFileAtomic(p, parsed, projectDir, broadcastLog);
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
    const hasSelectedStrategy = !!(config?.strategyConfig && config.strategyConfig.name);

    return {
        "에디터 설정": {
            "서명": "",
            "로그 패널 열림": config?.logPanelOpen ? "열림" : "닫힘",
            "로그 패널 높이": config?.logPanelHeight ?? 400,
        },
        "거래소 설정": {
            "API 키 환경 변수": config?.apiKeyEnvVar ?? "",
            "API 시크릿 환경 변수": config?.apiSecretEnvVar ?? "",

            "거래소 정보 파일 경로": config?.exchangeInfoPath ?? "",
            "레버리지 구간 파일 경로": config?.leverageBracketPath ?? "",
            "마지막 데이터 업데이트": config?.lastDataUpdates ?? "",
        },
        "심볼 설정": {
            "심볼": config?.symbolConfigs ?? [],
            "페어": {
                "선택된 페어": config?.selectedPair ?? "",
                "커스텀 페어": config?.customPairs ?? []
            }
        },
        "바 데이터 설정": (config?.barDataConfigs ?? []).map((bar_cfg) => ({
            "타임프레임": bar_cfg.timeframe,
            "바 데이터 폴더": bar_cfg.klinesDirectory,
            "바 데이터 유형": bar_cfg.barDataType,
        })),
        "펀딩 비율 설정": {
            "펀딩 비율 폴더": config?.fundingRatesDirectory ?? "",
        },
        "엔진 설정": {
            "프로젝트 폴더": config?.projectDirectory ?? "",

            "백테스팅 기간 처음부터": config?.useBacktestPeriodStart ? "사용" : "미사용",
            "백테스팅 기간 끝까지": config?.useBacktestPeriodEnd ? "사용" : "미사용",
            "백테스팅 기간 시작": config?.backtestPeriodStart ?? "",
            "백테스팅 기간 종료": config?.backtestPeriodEnd ?? "",
            "백테스팅 기간 형식": config?.backtestPeriodFormat ?? "%Y-%m-%d %H:%M:%S",

            "바 돋보기 기능": config?.useBarMagnifier ? "활성화" : "비활성화",

            "초기 자금": config?.initialBalance ?? undefined,

            "테이커 수수료율": config?.takerFeePercentage ?? undefined,
            "메이커 수수료율": config?.makerFeePercentage ?? undefined,

            "슬리피지 모델": config?.slippageModel ?? "MarketImpactSlippage",
            "테이커 슬리피지율": config?.slippageTakerPercentage ?? undefined,
            "메이커 슬리피지율": config?.slippageMakerPercentage ?? undefined,
            "슬리피지 스트레스 계수": config?.slippageStressMultiplier ?? undefined,

            "시장가 최대 수량 검사": config?.checkMarketMaxQty ? "활성화" : "비활성화",
            "시장가 최소 수량 검사": config?.checkMarketMinQty ? "활성화" : "비활성화",
            "지정가 최대 수량 검사": config?.checkLimitMaxQty ? "활성화" : "비활성화",
            "지정가 최소 수량 검사": config?.checkLimitMinQty ? "활성화" : "비활성화",
            "최소 명목 가치 검사": config?.checkMinNotionalValue ? "활성화" : "비활성화",

            "마크 가격 바 데이터와 목표 바 데이터 중복 검사": config?.checkSameBarDataWithTarget ? "활성화" : "비활성화",
            "심볼 간 트레이딩 바 데이터 중복 검사": config?.checkSameBarDataTrading ? "활성화" : "비활성화",
            "심볼 간 돋보기 바 데이터 중복 검사": config?.checkSameBarDataMagnifier ? "활성화" : "비활성화",
            "심볼 간 참조 바 데이터 중복 검사": config?.checkSameBarDataReference ? "활성화" : "비활성화",
            "심볼 간 마크 가격 바 데이터 중복 검사": config?.checkSameBarDataMarkPrice ? "활성화" : "비활성화"
        },
        "전략 설정": {
            "전략 헤더 폴더": config?.strategyConfig?.strategyHeaderDirs,
            "전략 소스 폴더": config?.strategyConfig?.strategySourceDirs,
            "지표 헤더 폴더": config?.strategyConfig?.indicatorHeaderDirs,
            "지표 소스 폴더": config?.strategyConfig?.indicatorSourceDirs,

            "선택된 전략": hasSelectedStrategy ? {
                "이름": config.strategyConfig.name,
                "DLL 파일 경로": config.strategyConfig.dllPath,
                "헤더 파일 경로": config.strategyConfig.strategyHeaderPath,
                "소스 파일 경로": config.strategyConfig.strategySourcePath,
            } : null
        }
    };
}

function objToConfig(obj) {
    // 각 섹션 먼저 추출
    const editor = obj?.['에디터 설정'];
    const exchange = obj?.['거래소 설정'];
    const symSection = obj?.['심볼 설정'];
    const fundingRatesSection = obj?.['펀딩 비율 설정'];
    const engine = obj?.['엔진 설정'];
    const strategySection = obj?.['전략 설정'];
    const selectedStrategy = strategySection?.['선택된 전략'];

    const strategyHeaderDirs = strategySection?.['전략 헤더 폴더'] ?? [];
    const strategySourceDirs = strategySection?.['전략 소스 폴더'] ?? [];
    const indicatorHeaderDirs = strategySection?.['지표 헤더 폴더'] ?? [];
    const indicatorSourceDirs = strategySection?.['지표 소스 폴더'] ?? [];

    return {
        // 에디터 설정
        logPanelOpen: editor?.['로그 패널 열림'] === '열림',
        logPanelHeight: editor?.['로그 패널 높이'] ?? 400,

        // 거래소 설정
        apiKeyEnvVar: exchange?.['API 키 환경 변수'] ?? '',
        apiSecretEnvVar: exchange?.['API 시크릿 환경 변수'] ?? '',

        exchangeInfoPath: exchange?.['거래소 정보 파일 경로'] ?? '',
        leverageBracketPath: exchange?.['레버리지 구간 파일 경로'] ?? '',
        lastDataUpdates: exchange?.['마지막 데이터 업데이트'] ?? '',

        // 심볼 설정
        symbolConfigs: symSection?.['심볼'] ?? [],
        selectedPair: symSection?.['페어']?.['선택된 페어'] ?? '',
        customPairs: symSection?.['페어']?.['커스텀 페어'] ?? [],

        // 바 데이터 설정
        barDataConfigs: (obj?.['바 데이터 설정'] ?? []).map((bar_data_config) => ({
            timeframe: bar_data_config['타임프레임'],
            klinesDirectory: bar_data_config['바 데이터 폴더'],
            barDataType: bar_data_config['바 데이터 유형'],
        })),

        // 펀딩 비율 설정
        fundingRatesDirectory: fundingRatesSection?.['펀딩 비율 폴더'] ?? '',

        // 엔진 설정
        projectDirectory: engine?.['프로젝트 폴더'] ?? '',

        useBacktestPeriodStart: engine?.['백테스팅 기간 처음부터'] === '사용',
        useBacktestPeriodEnd: engine?.['백테스팅 기간 끝까지'] === '사용',
        backtestPeriodStart: engine?.['백테스팅 기간 시작'] ?? '',
        backtestPeriodEnd: engine?.['백테스팅 기간 종료'] ?? '',
        backtestPeriodFormat: engine?.['백테스팅 기간 형식'] ?? '%Y-%m-%d %H:%M:%S',

        useBarMagnifier: engine?.['바 돋보기 기능'] === '활성화',

        initialBalance: engine?.['초기 자금'] ?? undefined,

        takerFeePercentage: engine?.['테이커 수수료율'] ?? undefined,
        makerFeePercentage: engine?.['메이커 수수료율'] ?? undefined,

        slippageModel: engine?.['슬리피지 모델'] ?? 'MarketImpactSlippage',
        slippageTakerPercentage: engine?.['테이커 슬리피지율'] ?? undefined,
        slippageMakerPercentage: engine?.['메이커 슬리피지율'] ?? undefined,
        slippageStressMultiplier: engine?.['슬리피지 스트레스 계수'] ?? undefined,

        checkMarketMaxQty: engine?.['시장가 최대 수량 검사'] === '활성화',
        checkMarketMinQty: engine?.['시장가 최소 수량 검사'] === '활성화',
        checkLimitMaxQty: engine?.['지정가 최대 수량 검사'] === '활성화',
        checkLimitMinQty: engine?.['지정가 최소 수량 검사'] === '활성화',
        checkMinNotionalValue: engine?.['최소 명목 가치 검사'] === '활성화',

        checkSameBarDataWithTarget: engine?.['마크 가격 바 데이터와 목표 바 데이터 중복 검사'] === '활성화',
        checkSameBarDataTrading: engine?.['심볼 간 트레이딩 바 데이터 중복 검사'] === '활성화',
        checkSameBarDataMagnifier: engine?.['심볼 간 돋보기 바 데이터 중복 검사'] === '활성화',
        checkSameBarDataReference: engine?.['심볼 간 참조 바 데이터 중복 검사'] === '활성화',
        checkSameBarDataMarkPrice: engine?.['심볼 간 마크 가격 바 데이터 중복 검사'] === '활성화',

        // 전략 설정
        strategyConfig: {
            strategyHeaderDirs: Array.isArray(strategyHeaderDirs) ? strategyHeaderDirs : [],
            strategySourceDirs: Array.isArray(strategySourceDirs) ? strategySourceDirs : [],
            indicatorHeaderDirs: Array.isArray(indicatorHeaderDirs) ? indicatorHeaderDirs : [],
            indicatorSourceDirs: Array.isArray(indicatorSourceDirs) ? indicatorSourceDirs : [],

            name: selectedStrategy?.['이름'] ?? '',
            dllPath: selectedStrategy?.['DLL 파일 경로'] ?? null,
            strategyHeaderPath: selectedStrategy?.['헤더 파일 경로'] ?? null,
            strategySourcePath: selectedStrategy?.['소스 파일 경로'] ?? null
        }
    };
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

        fundingRatesDirectory: toPosix(path.join(projectDirectory, 'Data', 'Funding Rates')),

        projectDirectory: projectDirectory,

        useBacktestPeriodStart: true,
        useBacktestPeriodEnd: true,
        backtestPeriodStart: "",
        backtestPeriodEnd: "",
        backtestPeriodFormat: "%Y-%m-%d %H:%M:%S",

        useBarMagnifier: true,

        initialBalance: undefined,

        takerFeePercentage: undefined,
        makerFeePercentage: undefined,

        slippageModel: "MarketImpactSlippage",
        slippageTakerPercentage: undefined,
        slippageMakerPercentage: undefined,
        slippageStressMultiplier: undefined,

        checkMarketMaxQty: true,
        checkMarketMinQty: true,
        checkLimitMaxQty: true,
        checkLimitMinQty: true,
        checkMinNotionalValue: true,

        checkSameBarDataWithTarget: true,
        checkSameBarDataTrading: true,
        checkSameBarDataMagnifier: true,
        checkSameBarDataReference: true,
        checkSameBarDataMarkPrice: true,

        // 전략 설정 기본값은 절대 경로로 생성
        strategyConfig: {
            strategyHeaderDirs: [toPosix(path.join(projectDirectory, 'Includes', 'Strategies'))],
            strategySourceDirs: [toPosix(path.join(projectDirectory, 'Sources', 'Cores', 'Strategies'))],
            indicatorHeaderDirs: [toPosix(path.join(projectDirectory, 'Includes', 'Indicators'))],
            indicatorSourceDirs: [toPosix(path.join(projectDirectory, 'Sources', 'Cores', 'Indicators'))],

            name: '',
            dllPath: null,
            strategyHeaderPath: null,
            strategySourcePath: null
        }
    };
}

function configToWs(config) {
    const hasSelectedStrategy = !!(config?.strategyConfig && config.strategyConfig.name);

    return {
        "에디터 설정": {
            "로그 패널 열림": config?.logPanelOpen ? "열림" : "닫힘",
            "로그 패널 높이": config?.logPanelHeight ?? 400,
        },
        "거래소 설정": {
            "API 키 환경 변수": config?.apiKeyEnvVar ?? "",
            "API 시크릿 환경 변수": config?.apiSecretEnvVar ?? "",

            "거래소 정보 파일 경로": config?.exchangeInfoPath ?? "",
            "레버리지 구간 파일 경로": config?.leverageBracketPath ?? "",
            "마지막 데이터 업데이트": config?.lastDataUpdates ?? "",
        },
        "심볼 설정": {
            "심볼": config?.symbolConfigs ?? [],
            "페어": {
                "선택된 페어": config?.selectedPair ?? '',
                "커스텀 페어": config?.customPairs ?? []
            }
        },
        "바 데이터 설정": (config?.barDataConfigs ?? []).map((b) => ({
            ...b, klinesDirectory: b.klinesDirectory ? toPosix(b.klinesDirectory) : b.klinesDirectory
        })),
        "펀딩 비율 설정": {
            "펀딩 비율 폴더": config?.fundingRatesDirectory ? toPosix(config.fundingRatesDirectory) : "",
        },
        "엔진 설정": {
            "프로젝트 폴더": config?.projectDirectory ? toPosix(config.projectDirectory) : "",

            "백테스팅 기간 처음부터": config?.useBacktestPeriodStart ? "사용" : "미사용",
            "백테스팅 기간 끝까지": config?.useBacktestPeriodEnd ? "사용" : "미사용",
            "백테스팅 기간 시작": config?.backtestPeriodStart ?? "",
            "백테스팅 기간 종료": config?.backtestPeriodEnd ?? "",
            "백테스팅 기간 형식": config?.backtestPeriodFormat ?? "%Y-%m-%d %H:%M:%S",

            "바 돋보기 기능": config?.useBarMagnifier ? "활성화" : "비활성화",

            "초기 자금": config?.initialBalance ?? undefined,

            "테이커 수수료율": config?.takerFeePercentage ?? undefined,
            "메이커 수수료율": config?.makerFeePercentage ?? undefined,

            "슬리피지 모델": config?.slippageModel ?? "MarketImpactSlippage",
            "테이커 슬리피지율": config?.slippageTakerPercentage ?? undefined,
            "메이커 슬리피지율": config?.slippageMakerPercentage ?? undefined,
            "슬리피지 스트레스 계수": config?.slippageStressMultiplier ?? undefined,

            "시장가 최대 수량 검사": config?.checkMarketMaxQty ? "활성화" : "비활성화",
            "시장가 최소 수량 검사": config?.checkMarketMinQty ? "활성화" : "비활성화",
            "지정가 최대 수량 검사": config?.checkLimitMaxQty ? "활성화" : "비활성화",
            "지정가 최소 수량 검사": config?.checkLimitMinQty ? "활성화" : "비활성화",
            "최소 명목 가치 검사": config?.checkMinNotionalValue ? "활성화" : "비활성화",

            "마크 가격 바 데이터와 목표 바 데이터 중복 검사": config?.checkSameBarDataWithTarget ? "활성화" : "비활성화",
            "심볼 간 트레이딩 바 데이터 중복 검사": config?.checkSameBarDataTrading ? "활성화" : "비활성화",
            "심볼 간 돋보기 바 데이터 중복 검사": config?.checkSameBarDataMagnifier ? "활성화" : "비활성화",
            "심볼 간 참조 바 데이터 중복 검사": config?.checkSameBarDataReference ? "활성화" : "비활성화",
            "심볼 간 마크 가격 바 데이터 중복 검사": config?.checkSameBarDataMarkPrice ? "활성화" : "비활성화"
        },
        "전략 설정": {
            "전략 헤더 폴더": config?.strategyConfig?.strategyHeaderDirs,
            "전략 소스 폴더": config?.strategyConfig?.strategySourceDirs,
            "지표 헤더 폴더": config?.strategyConfig?.indicatorHeaderDirs,
            "지표 소스 폴더": config?.strategyConfig?.indicatorSourceDirs,

            "선택된 전략": hasSelectedStrategy ? {
                "이름": config.strategyConfig.name,
                "DLL 파일 경로": config.strategyConfig.dllPath,
                "헤더 파일 경로": config.strategyConfig.strategyHeaderPath,
                "소스 파일 경로": config.strategyConfig.strategySourcePath
            } : null
        }
    }
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

    const editor = wsConfig["에디터 설정"];
    const exchange = wsConfig["거래소 설정"];
    const symSection = wsConfig["심볼 설정"];
    const fundingRateSection = wsConfig["펀딩 비율 설정"];
    const engine = wsConfig["엔진 설정"];
    const strategySection = wsConfig["전략 설정"];
    const selectedStrategy = strategySection?.["선택된 전략"];

    if (editor || exchange || symSection || engine) {
        return {
            logPanelOpen: editor?.["로그 패널 열림"] === "열림",
            logPanelHeight: editor?.["로그 패널 높이"] ?? 400,

            apiKeyEnvVar: exchange?.["API 키 환경 변수"] ?? engine?.["API 키 환경 변수"] ?? '',
            apiSecretEnvVar: exchange?.["API 시크릿 환경 변수"] ?? engine?.["API 시크릿 환경 변수"] ?? '',

            exchangeInfoPath: exchange?.["거래소 정보 파일 경로"] ?? engine?.["거래소 정보 파일 경로"] ?? '',
            leverageBracketPath: exchange?.["레버리지 구간 파일 경로"] ?? engine?.["레버리지 구간 파일 경로"] ?? '',
            lastDataUpdates: exchange?.["마지막 데이터 업데이트"] ?? engine?.["마지막 데이터 업데이트"] ?? '',

            symbolConfigs: symSection?.["심볼"] ?? [],
            selectedPair: symSection?.["페어"]?.["선택된 페어"] ?? '',
            customPairs: symSection?.["페어"]?.["커스텀 페어"] ?? [],

            barDataConfigs: wsConfig["바 데이터 설정"] ?? [],

            fundingRatesDirectory: fundingRateSection?.['펀딩 비율 폴더'] ?? '',

            projectDirectory: engine?.["프로젝트 폴더"] ?? "",

            useBacktestPeriodStart: engine?.["백테스팅 기간 처음부터"] === "사용",
            useBacktestPeriodEnd: engine?.["백테스팅 기간 끝까지"] === "사용",
            backtestPeriodStart: engine?.["백테스팅 기간 시작"] ?? "",
            backtestPeriodEnd: engine?.["백테스팅 기간 종료"] ?? "",
            backtestPeriodFormat: engine?.["백테스팅 기간 형식"] ?? "%Y-%m-%d %H:%M:%S",

            useBarMagnifier: engine?.["바 돋보기 기능"] === "활성화",

            initialBalance: engine?.["초기 자금"] ?? undefined,

            takerFeePercentage: engine?.["테이커 수수료율"] ?? undefined,
            makerFeePercentage: engine?.["메이커 수수료율"] ?? undefined,

            slippageModel: engine?.["슬리피지 모델"] ?? "MarketImpactSlippage",
            slippageTakerPercentage: engine?.["테이커 슬리피지율"] ?? undefined,
            slippageMakerPercentage: engine?.["메이커 슬리피지율"] ?? undefined,
            slippageStressMultiplier: engine?.["슬리피지 스트레스 계수"] ?? undefined,

            checkMarketMaxQty: engine?.["시장가 최대 수량 검사"] === "활성화",
            checkMarketMinQty: engine?.["시장가 최소 수량 검사"] === "활성화",
            checkLimitMaxQty: engine?.["지정가 최대 수량 검사"] === "활성화",
            checkLimitMinQty: engine?.["지정가 최소 수량 검사"] === "활성화",
            checkMinNotionalValue: engine?.["최소 명목 가치 검사"] === "활성화",

            checkSameBarDataWithTarget: engine?.["마크 가격 바 데이터와 목표 바 데이터 중복 검사"] === "활성화",
            checkSameBarDataTrading: engine?.["심볼 간 트레이딩 바 데이터 중복 검사"] === "활성화",
            checkSameBarDataMagnifier: engine?.["심볼 간 돋보기 바 데이터 중복 검사"] === "활성화",
            checkSameBarDataReference: engine?.["심볼 간 참조 바 데이터 중복 검사"] === "활성화",
            checkSameBarDataMarkPrice: engine?.["심볼 간 마크 가격 바 데이터 중복 검사"] === "활성화",

            strategyConfig: {
                strategyHeaderDirs: strategySection?.["전략 헤더 폴더"] ?? [],
                strategySourceDirs: strategySection?.["전략 소스 폴더"] ?? [],
                indicatorHeaderDirs: strategySection?.["지표 헤더 폴더"] ?? [],
                indicatorSourceDirs: strategySection?.["지표 소스 폴더"] ?? [],

                name: selectedStrategy?.["이름"] ?? '',
                dllPath: selectedStrategy?.["DLL 파일 경로"] ?? null,
                strategyHeaderPath: selectedStrategy?.["헤더 파일 경로"] ?? null,
                strategySourcePath: selectedStrategy?.["소스 파일 경로"] ?? null
            }
        };
    }

    // 영어 키 형식
    return {
        logPanelOpen: wsConfig.logPanelOpen ?? false,
        logPanelHeight: wsConfig.logPanelHeight ?? 400,

        apiKeyEnvVar: wsConfig.apiKeyEnvVar ?? '',
        apiSecretEnvVar: wsConfig.apiSecretEnvVar ?? '',

        exchangeInfoPath: wsConfig.exchangeInfoPath ?? '',
        leverageBracketPath: wsConfig.leverageBracketPath ?? '',
        lastDataUpdates: wsConfig.lastDataUpdates ?? '',

        symbolConfigs: wsConfig.symbolConfigs ?? [],
        selectedPair: wsConfig.selectedPair ?? '',
        customPairs: wsConfig.customPairs ?? [],

        barDataConfigs: wsConfig.barDataConfigs ?? [],

        fundingRatesDirectory: wsConfig.fundingRatesDirectory ?? '',

        projectDirectory: wsConfig.projectDirectory ?? "",

        useBacktestPeriodStart: wsConfig.useBacktestPeriodStart ?? true,
        useBacktestPeriodEnd: wsConfig.useBacktestPeriodEnd ?? true,
        backtestPeriodStart: wsConfig.backtestPeriodStart ?? "",
        backtestPeriodEnd: wsConfig.backtestPeriodEnd ?? "",
        backtestPeriodFormat: wsConfig.backtestPeriodFormat ?? "%Y-%m-%d %H:%M:%S",

        useBarMagnifier: wsConfig.useBarMagnifier ?? true,

        initialBalance: wsConfig.initialBalance ?? undefined,

        takerFeePercentage: wsConfig.takerFeePercentage ?? undefined,
        makerFeePercentage: wsConfig.makerFeePercentage ?? undefined,

        slippageModel: wsConfig.slippageModel ?? "MarketImpactSlippage",
        slippageTakerPercentage: wsConfig.slippageTakerPercentage ?? undefined,
        slippageMakerPercentage: wsConfig.slippageMakerPercentage ?? undefined,
        slippageStressMultiplier: wsConfig.slippageStressMultiplier ?? undefined,

        checkMarketMaxQty: wsConfig.checkMarketMaxQty ?? true,
        checkMarketMinQty: wsConfig.checkMarketMinQty ?? true,
        checkLimitMaxQty: wsConfig.checkLimitMaxQty ?? true,
        checkLimitMinQty: wsConfig.checkLimitMinQty ?? true,
        checkMinNotionalValue: wsConfig.checkMinNotionalValue ?? true,

        checkSameBarDataWithTarget: wsConfig.checkSameBarDataWithTarget ?? true,
        checkSameBarDataTrading: wsConfig.checkSameBarDataTrading ?? true,
        checkSameBarDataMagnifier: wsConfig.checkSameBarDataMagnifier ?? true,
        checkSameBarDataReference: wsConfig.checkSameBarDataReference ?? true,
        checkSameBarDataMarkPrice: wsConfig.checkSameBarDataMarkPrice ?? true,

        strategyConfig: wsConfig.strategyConfig || null
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
    const fundingRatesDir = path.join(projectDir, 'Data', 'Funding Rates');

    return await ensureDirectory(backboardDir) && await ensureDirectory(continuousDir) && await ensureDirectory(markPriceDir) && await ensureDirectory(fundingRatesDir);
}

function getEditorConfigPath(projectDir) {
    return path.join(projectDir, 'BackBoard', 'editor.json');
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

async function loadOrCreateEditorConfig(activeClients, broadcastLog, projectDir) {
    try {
        const defaultEditorPath = getEditorConfigPath(projectDir);

        try {
            await fsPromises.access(defaultEditorPath, fs.constants.F_OK);
            const loaded = readSignedConfigWithRecovery(defaultEditorPath, projectDir, broadcastLog);

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

        // 서버를 시작할 때 전달된 프로젝트 폴더를 기본으로 사용하여 설정 파일을 자동 생성
        try {
            const resolvedRoot = projectDir ? path.resolve(projectDir) : null;
            if (!resolvedRoot) {
                broadcastLog("ERROR", `프로젝트 폴더 정보가 없습니다.`, null, null);
                return;
            }

            if (await createProjectStructure(resolvedRoot)) {
                const defaultConfig = createDefaultConfig(resolvedRoot);
                const editorConfigPath = getEditorConfigPath(resolvedRoot);

                try {
                    const obj = configToObj(defaultConfig);
                    // HMAC 키/파일 작업은 프로젝트 루트를 기준으로 수행
                    if (!writeSignedFileAtomic(editorConfigPath, obj, resolvedRoot, broadcastLog)) {
                        broadcastLog("ERROR", '서명 파일 저장 실패', null, null);
                        return;
                    }


                    broadcastLog("INFO", `기본 editor.json을 생성했습니다: ${toPosix(editorConfigPath)}`, null, null);
                    return defaultConfig;
                } catch (err) {
                    broadcastLog("ERROR", `editor.json 생성 실패: ${err.message}`, null, null);

                }
            } else {
                broadcastLog("ERROR", `프로젝트 폴더 구조 생성에 실패했습니다: ${toPosix(resolvedRoot)}`, null, null);

            }
        } catch (e) {
            // 무시
        }
    } catch (e) {
        // 무시
    }
}

function saveEditorConfig(config, broadcastLog) {
    const projectDirResolved = config && config.projectDirectory;
    if (!projectDirResolved) {
        return false;
    }

    try {
        const editorConfigPath = getEditorConfigPath(projectDirResolved);
        fs.mkdirSync(path.dirname(editorConfigPath), {recursive: true});
        const obj = configToObj(config);

        return writeSignedFileAtomic(editorConfigPath, obj, projectDirResolved, broadcastLog);
    } catch (err) {
        broadcastLog("ERROR", `editor.json 저장 실패: ${err}`, null, null);
        return false;
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
    stopSingleBacktesting,
    fetchOrUpdateBarData,
    stopBacktestingEngine: stopBacktestingEngine,
    loadOrCreateEditorConfig,
    saveEditorConfig,
    configToWs,
    wsToConfig,
    getEditorConfig,
    setEditorConfig,
    isEditorConfigLoading,
    setEditorConfigLoading,
    validateProjectDirectoryConfig
};
