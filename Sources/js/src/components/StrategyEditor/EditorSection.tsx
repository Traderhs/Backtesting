import React, {useEffect, useRef, useState} from 'react';
import {useWebSocket} from '../Server/WebSocketContext';
import {Button} from '../ui/button';
import {Input} from '@/components/ui/input';
import {parseTimeframeString, timeframeToString} from '@/types/barData.ts';
import {useStrategy} from './StrategyContext';

interface Props {
    // LogPanel 관련
    isLogPanelOpen: boolean;
    setIsLogPanelOpen: (v: boolean) => void;
    logPanelHeight: number;
    setLogPanelHeight: (h: number) => void;
}

/**
 * 에디터 설정 관리 및 로그 패널 컴포넌트
 * editor.json 로드/저장, 프로젝트 디렉토리 설정, 로그 표시 담당
 */
export default function EditorSection({
                                          isLogPanelOpen,
                                          setIsLogPanelOpen,
                                          logPanelHeight,
                                          setLogPanelHeight,
                                      }: Props) {
    const {
        symbolConfigs, setSymbolConfigs,
        selectedPair, setSelectedPair,
        customPairs, setCustomPairs,
        barDataConfigs, setBarDataConfigs,
        exchangeConfig, setExchangeConfig,
        lastDataUpdates, setLastDataUpdates,
        engineConfig, setEngineConfig,
        logs, addLog,
        configLoaded, setConfigLoaded
    } = useStrategy();

    const {ws, clearProjectDirectoryRequest, projectDirectoryRequested} = useWebSocket();

    // 서버 요청에 의한 입력 다이얼로그만 처리
    const [showProjectDialog, setShowProjectDialog] = useState(false);
    const [projectDirectoryInput, setProjectDirectoryInput] = useState('');

    const [isResizing, setIsResizing] = useState(false);
    const logContainerRef = useRef<HTMLDivElement | null>(null);

    // 컴포넌트 마운트 시 editor.json 로드
    useEffect(() => {
        if (!ws) return;

        const trySendLoad = () => {
            if (ws.readyState === WebSocket.OPEN && !configLoaded) {
                try {
                    ws.send(JSON.stringify({action: 'loadEditorConfig'}));
                } catch (e) {
                    console.error('loadEditorConfig 전송 실패:', e);
                }
            }
        };

        trySendLoad();
        const onOpen = () => trySendLoad();
        ws.addEventListener('open', onOpen);
        return () => {
            try {
                ws.removeEventListener('open', onOpen);
            } catch (e) { /* 무시 */
            }
        };
    }, [ws, configLoaded]);

    // WebSocket 메시지 수신 (Config & Log)
    useEffect(() => {
        if (!ws) return;

        const handleMessage = (event: MessageEvent) => {
            try {
                const data = JSON.parse(event.data);

                // Log 처리
                if (data.action === 'backtestingLog') {
                    addLog(data.level, data.message, data.timestamp, data.fileInfo);
                }
                // Config 처리
                else if (data.action === 'requestProjectDirectory') {
                    setShowProjectDialog(true);
                } else if (data.action === 'projectDirectoryInvalid') {
                    setShowProjectDialog(true);
                } else if (data.action === 'editorConfigLoaded') {
                    const config = data.config;

                    if (config && typeof config === 'object') {
                        // 에디터 설정
                        const editor = config['에디터 설정'] || {};

                        if (typeof editor['로그 패널 열림'] === 'boolean') {
                            setIsLogPanelOpen(editor['로그 패널 열림']);
                        } else if (typeof editor['로그 패널 열림'] === 'string') {
                            setIsLogPanelOpen(editor['로그 패널 열림'] === "열림");
                        }

                        if (typeof editor['로그 패널 높이'] === 'number') {
                            setLogPanelHeight(editor['로그 패널 높이']);
                        }

                        // 거래소 설정
                        const exchange = config['거래소 설정'] || {};
                        const newExchangeConfig = {...exchangeConfig};

                        if (typeof exchange['API 키 환경 변수'] === 'string') {
                            newExchangeConfig.apiKeyEnvVar = exchange['API 키 환경 변수'];
                        }

                        if (typeof exchange['API 시크릿 환경 변수'] === 'string') {
                            newExchangeConfig.apiSecretEnvVar = exchange['API 시크릿 환경 변수'];
                        }

                        if (typeof exchange['거래소 정보 파일 경로'] === 'string') {
                            newExchangeConfig.exchangeInfoPath = exchange['거래소 정보 파일 경로'];
                        }

                        if (typeof exchange['레버리지 구간 파일 경로'] === 'string') {
                            newExchangeConfig.leverageBracketPath = exchange['레버리지 구간 파일 경로'];
                        }

                        setExchangeConfig(newExchangeConfig);

                        if (typeof exchange['마지막 데이터 업데이트'] === 'string') {
                            setLastDataUpdates(exchange['마지막 데이터 업데이트']);
                        }

                        // 심볼 설정
                        const symbolSection = config['심볼 설정'];
                        if (symbolSection && typeof symbolSection === 'object') {
                            if (Array.isArray(symbolSection['심볼'])) {
                                setSymbolConfigs(symbolSection['심볼']);
                            } else {
                                setSymbolConfigs([]);
                            }

                            const innerPair = symbolSection['페어'] || {};
                            if (typeof innerPair['선택된 페어'] === 'string') {
                                setSelectedPair(innerPair['선택된 페어']);
                            }

                            if (Array.isArray(innerPair['커스텀 페어'])) {
                                setCustomPairs(innerPair['커스텀 페어']);
                            }
                        }

                        // 바 데이터 설정
                        if (Array.isArray(config['바 데이터 설정'])) {
                            const loadedConfigs = config['바 데이터 설정'].map((barDataConfig: any) => ({
                                timeframe: parseTimeframeString(barDataConfig.timeframe),
                                klinesDirectory: barDataConfig.klinesDirectory,
                                barDataType: barDataConfig.barDataType
                            }));

                            setBarDataConfigs(loadedConfigs);
                        }

                        // 엔진 설정
                        const engine = config['엔진 설정'] || {};
                        let newProjectDir = engineConfig.projectDirectory;
                        let newUseMagnifier = engineConfig.useBarMagnifier;

                        if (typeof engine['프로젝트 폴더'] === 'string') {
                            newProjectDir = engine['프로젝트 폴더'];
                        }

                        if (typeof engine['바 돋보기 기능'] === 'string') {
                            newUseMagnifier = engine['바 돋보기 기능'] === "활성화";
                        }

                        setEngineConfig({projectDirectory: newProjectDir, useBarMagnifier: newUseMagnifier});
                    }
                    setConfigLoaded(true);
                }
            } catch (err) {
                console.error('메시지 파싱 오류:', err);
            }
        };

        ws.addEventListener('message', handleMessage);
        return () => ws.removeEventListener('message', handleMessage);
    }, [ws, addLog, setIsLogPanelOpen, setLogPanelHeight, setExchangeConfig, setLastDataUpdates, setSymbolConfigs, setSelectedPair, setCustomPairs, setBarDataConfigs, setEngineConfig, setConfigLoaded, configLoaded, engineConfig, exchangeConfig]);

    // 서버가 프로젝트 폴더 입력을 요청한 경우
    useEffect(() => {
        if (projectDirectoryRequested && !showProjectDialog) {
            setShowProjectDialog(true);
        }
    }, [projectDirectoryRequested, showProjectDialog, setShowProjectDialog]);

    // editor.json 저장 함수
    const saveConfig = () => {
        if (!ws || ws.readyState !== WebSocket.OPEN) return;

        ws.send(JSON.stringify({
            action: 'saveEditorConfig',
            config: {
                '에디터 설정': {
                    '로그 패널 열림': isLogPanelOpen,
                    '로그 패널 높이': logPanelHeight,
                },
                '거래소 설정': {
                    'API 키 환경 변수': exchangeConfig.apiKeyEnvVar,
                    'API 시크릿 환경 변수': exchangeConfig.apiSecretEnvVar,

                    '거래소 정보 파일 경로': exchangeConfig.exchangeInfoPath,
                    '레버리지 구간 파일 경로': exchangeConfig.leverageBracketPath,
                    '마지막 데이터 업데이트': lastDataUpdates,
                },
                '심볼 설정': {
                    '심볼': symbolConfigs,
                    '페어': {
                        '선택된 페어': selectedPair,
                        '커스텀 페어': customPairs,
                    }
                },
                '바 데이터 설정': barDataConfigs.map(config => ({
                    timeframe: timeframeToString(config.timeframe),
                    klinesDirectory: (config.klinesDirectory || '').replace(/\\/g, '/'),
                    barDataType: config.barDataType
                })),
                '엔진 설정': {
                    '프로젝트 폴더': engineConfig.projectDirectory ? engineConfig.projectDirectory.replace(/\\/g, '/') : engineConfig.projectDirectory,
                    '바 돋보기 기능': engineConfig.useBarMagnifier ? "활성화" : "비활성화",
                },
            }
        }));
    };

    // 설정이 변경될 때마다 자동 저장 (초기 로드 제외)
    useEffect(() => {
        if (!configLoaded) return;
        const timer = setTimeout(() => {
            saveConfig();
        }, 300);
        return () => clearTimeout(timer);
    }, [
        isLogPanelOpen,
        logPanelHeight,
        exchangeConfig,
        lastDataUpdates,
        symbolConfigs,
        selectedPair,
        customPairs,
        barDataConfigs,
        engineConfig,
        configLoaded
    ]);

    // 프로젝트 디렉토리 제공
    const handleProvideProjectDirectory = () => {
        if (!ws || ws.readyState !== WebSocket.OPEN) return;
        if (!projectDirectoryInput || projectDirectoryInput.trim() === '') {
            addLog('ERROR', '프로젝트 폴더 경로를 입력해주세요.');
            return;
        }
        const normalizedProjectDir = projectDirectoryInput.replace(/\\/g, '/');
        ws.send(JSON.stringify({action: 'provideProjectDirectory', projectDirectory: normalizedProjectDir}));
        setEngineConfig(prev => ({...prev, projectDirectory: normalizedProjectDir}));
        setShowProjectDialog(false);
        try {
            clearProjectDirectoryRequest();
        } catch (e) { /* 무시 */
        }
    };

    // --- LogPanel UI Logic ---
    const getLogColor = (level: string) => {
        switch (level) {
            case 'INFO':
                return 'rgb(255, 255, 255)';
            case 'BALANCE':
            case 'DEBUG':
                return 'rgb(128, 128, 128)';
            case 'WARN':
                return 'rgb(229, 192, 123)';
            case 'ERROR':
                return 'rgb(224, 108, 117)';
            default:
                return '#ffffff';
        }
    };

    const handleResizeMouseDown = (e: React.MouseEvent) => {
        e.preventDefault();
        setIsResizing(true);
    };

    useEffect(() => {
        if (!isResizing) return;
        let rafId: number | null = null;
        let lastY = 0;

        const handleMouseMove = (e: MouseEvent) => {
            lastY = e.clientY;
            if (rafId === null) {
                rafId = requestAnimationFrame(() => {
                    const newHeight = window.innerHeight - lastY;
                    if (newHeight >= 200 && newHeight <= window.innerHeight * 0.8) {
                        setLogPanelHeight(newHeight);
                    }
                    rafId = null;
                });
            }
        };

        const handleMouseUp = () => {
            setIsResizing(false);
            if (rafId !== null) cancelAnimationFrame(rafId);
        };

        document.addEventListener('mousemove', handleMouseMove);
        document.addEventListener('mouseup', handleMouseUp);
        return () => {
            document.removeEventListener('mousemove', handleMouseMove);
            document.removeEventListener('mouseup', handleMouseUp);
            if (rafId !== null) cancelAnimationFrame(rafId);
        };
    }, [isResizing, setLogPanelHeight]);

    useEffect(() => {
        if (logContainerRef.current) {
            logContainerRef.current.scrollTop = logContainerRef.current.scrollHeight;
        }
    }, [logs]);

    return (
        <>
            {/* 프로젝트 디렉토리 다이얼로그 */}
            {showProjectDialog && (
                <div className="fixed inset-0 z-50 flex items-center justify-center">
                    <div className="absolute inset-0 bg-black/60"/>
                    <div className="relative bg-[#0f1724] rounded-lg p-6 w-[520px] border border-gray-600 z-10">
                        <h3 className="text-lg font-semibold text-white mb-3">프로젝트 폴더 입력</h3>
                        <p className="text-sm text-gray-300 mb-3">프로젝트 폴더를 입력하세요. 해당 폴더에 BackBoard.exe가 존재해야 합니다.</p>
                        <Input type="text" value={projectDirectoryInput}
                               onChange={(e) => setProjectDirectoryInput(e.currentTarget.value)}
                               placeholder="프로젝트 폴더 경로"
                               className="mb-4 bg-[#0b1220] border-gray-600"/>
                        <div className="flex justify-end gap-2">
                            <Button onClick={handleProvideProjectDirectory} className="bg-blue-600">입력</Button>
                        </div>
                    </div>
                </div>
            )}

            {/* 로그 패널 */}
            {isLogPanelOpen && (
                <>
                    <div
                        className="h-1 bg-gray-700 hover:bg-blue-500 cursor-ns-resize transition-colors"
                        onMouseDown={handleResizeMouseDown}
                        style={{userSelect: 'none'}}
                    />
                    <div className="bg-[#1a1a1a] border-t border-gray-700 flex flex-col"
                         style={{height: `${logPanelHeight}px`}}>
                        <div className="px-4 py-2 border-b border-gray-700 flex items-center justify-between">
                            <h2 className="text-sm font-semibold text-gray-300">실행 로그</h2>
                            <button
                                onClick={() => setIsLogPanelOpen(false)}
                                className="text-gray-400 hover:text-white text-lg leading-none px-2"
                            >
                                ×
                            </button>
                        </div>
                        <div ref={logContainerRef} className="overflow-y-auto p-4 font-mono text-sm flex-1"
                             style={{fontFamily: "'Inter', 'Pretendard', monospace"}}>
                            {logs.length === 0 ? (
                                <div className="text-gray-500 text-center mt-8">백테스팅을 실행하면 로그가 여기에 표시됩니다.</div>
                            ) : (
                                logs.map((log, index) => {
                                    if (log.level === 'SEPARATOR') {
                                        return (
                                            <div key={index} className="my-2">
                                                <div style={{
                                                    whiteSpace: 'pre',
                                                    fontFamily: "'Inter', 'Pretendard', monospace",
                                                    fontSize: '14px',
                                                    lineHeight: '1.4',
                                                    color: 'rgba(255, 215, 0, 0.4)',
                                                }}>{'─'.repeat(150)}</div>
                                            </div>
                                        );
                                    }

                                    // 메시지 내용에서 날짜 및 파일:라인 패턴 하이라이트
                                    const highlightMessage = () => {
                                        const message = log.message;
                                        const elements: React.ReactNode[] = [];
                                        const logLevelColor = getLogColor(log.level);

                                        // 대괄호 포함/미포함 모두 처리: 날짜와 파일:라인 패턴
                                        const regex = /(\[?\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}]?)|(\[?[\w.\/\\-]+\.(?:cpp|hpp):\d+]?)/g;
                                        let lastIndex = 0;
                                        let key = 0;

                                        for (const match of message.matchAll(regex)) {
                                            const matchIndex = match.index ?? 0;

                                            // 일반 텍스트
                                            if (lastIndex < matchIndex) {
                                                const text = message.slice(lastIndex, matchIndex);
                                                elements.push(<span key={key++}
                                                                    style={{color: logLevelColor}}>{text}</span>);
                                            }

                                            const token = match[0];

                                            // 대괄호 제거 후 판단용 문자열
                                            const stripped = token.replace(/^\[/, '').replace(/]$/, '');

                                            if (/^\d{4}-\d{2}-\d{2}/.test(stripped)) {
                                                // 날짜 패턴
                                                elements.push(<span key={key++}
                                                                    style={{color: 'rgb(106, 153, 85)'}}>{token}</span>);
                                            } else {
                                                // 파일:라인 패턴
                                                elements.push(<span key={key++}
                                                                    style={{color: 'rgb(86, 156, 214)'}}>{token}</span>);
                                            }

                                            lastIndex = matchIndex + match[0].length;
                                        }

                                        // 남은 일반 텍스트
                                        if (lastIndex < message.length) {
                                            const text = message.slice(lastIndex);
                                            elements.push(<span key={key++}
                                                                style={{color: logLevelColor}}>{text}</span>);
                                        }

                                        return elements;
                                    };

                                    return (
                                        <div key={index} className="mb-1">
                                            {log.timestamp &&
                                                <span style={{color: 'rgb(106, 153, 85)'}}>[{log.timestamp}]</span>}
                                            {' '}
                                            <span className="font-semibold"
                                                  style={{color: getLogColor(log.level)}}>[{log.level}]</span>
                                            {' '}
                                            {log.fileInfo && <><span
                                                style={{color: 'rgb(86, 156, 214)'}}>[{log.fileInfo}]</span></>}
                                            {' '}
                                            <span>{highlightMessage()}</span>
                                        </div>
                                    );
                                })
                            )}
                        </div>
                    </div>
                </>
            )}
        </>
    );
}
