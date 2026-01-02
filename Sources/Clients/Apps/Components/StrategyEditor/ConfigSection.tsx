import React, {useEffect} from 'react';
import {Button} from '@/Components/UI/Button.tsx';
import {Input} from '@/Components/UI/Input.tsx';
import {useWebSocket} from '../Server/WebSocketContext';
import {BarDataConfig, parseTimeframeString, timeframeToString} from '@/Types/BarData.ts';

interface Props {
    projectDirectory: string;
    setProjectDirectory: React.Dispatch<React.SetStateAction<string>>;
    projectDirectoryInput: string;
    setProjectDirectoryInput: React.Dispatch<React.SetStateAction<string>>;
    showProjectDialog: boolean;
    setShowProjectDialog: React.Dispatch<React.SetStateAction<boolean>>;
    useBarMagnifier: boolean;
    setUseBarMagnifier: React.Dispatch<React.SetStateAction<boolean>>;
    symbolConfigs: string[];
    setSymbolConfigs: React.Dispatch<React.SetStateAction<string[]>>;
    selectedPair: string;
    setSelectedPair: React.Dispatch<React.SetStateAction<string>>;
    customPairs: string[];
    setCustomPairs: React.Dispatch<React.SetStateAction<string[]>>;
    barDataConfigs: BarDataConfig[];
    setBarDataConfigs: React.Dispatch<React.SetStateAction<BarDataConfig[]>>;
    isLogPanelOpen: boolean;
    setIsLogPanelOpen: React.Dispatch<React.SetStateAction<boolean>>;
    logPanelHeight: number;
    setLogPanelHeight: React.Dispatch<React.SetStateAction<number>>;
    configLoaded: boolean;
    setConfigLoaded: React.Dispatch<React.SetStateAction<boolean>>;
    addLog: (level: string, message: string, timestamp?: string | null, fileInfo?: string | null) => void;
}

/**
 * 설정 및 프로젝트 디렉토리 관리 컴포넌트
 * editor.json 저장, 프로젝트 폴더 입력 다이얼로그 관리
 */
export default function ConfigSection({
                                          projectDirectory,
                                          setProjectDirectory,
                                          projectDirectoryInput,
                                          setProjectDirectoryInput,
                                          showProjectDialog,
                                          setShowProjectDialog,
                                          useBarMagnifier,
                                          setUseBarMagnifier,
                                          symbolConfigs,
                                          setSymbolConfigs,
                                          selectedPair,
                                          setSelectedPair,
                                          customPairs,
                                          setCustomPairs,
                                          barDataConfigs,
                                          setBarDataConfigs,
                                          isLogPanelOpen,
                                          setIsLogPanelOpen,
                                          logPanelHeight,
                                          setLogPanelHeight,
                                          configLoaded,
                                          setConfigLoaded,
                                          addLog
                                      }: Props) {
    const {ws, clearProjectDirectoryRequest, projectDirectoryRequested} = useWebSocket();

    // 컴포넌트 마운트 시 editor.json 로드 (ws가 CONNECTING 상태일 때 open 이벤트도 처리)
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

        // 즉시 시도
        trySendLoad();

        // 아직 OPEN이 아니면 open 이벤트에서 재시도
        const onOpen = () => {
            trySendLoad();
        };

        ws.addEventListener('open', onOpen);

        return () => {
            try {
                ws.removeEventListener('open', onOpen);
            } catch (e) {
                // 무시
            }
        };
    }, [ws, configLoaded]);

    // WebSocket 메시지 수신
    useEffect(() => {
        if (!ws) {
            return;
        }

        const handleMessage = (event: MessageEvent) => {
            try {
                const data = JSON.parse(event.data);

                if (data.action === 'requestProjectDirectory') {
                    setShowProjectDialog(true);
                } else if (data.action === 'projectDirectoryInvalid') {
                    setShowProjectDialog(true);
                } else if (data.action === 'editorConfigLoaded') {
                    const config = data.config;

                    if (config && typeof config === 'object' && (config['에디터 설정'] || config['엔진 설정'])) {
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

                        // 엔진 설정
                        const engine = config['엔진 설정'] || {};

                        if (typeof engine['프로젝트 폴더'] === 'string') {
                            setProjectDirectory(engine['프로젝트 폴더']);
                        }

                        if (typeof engine['바 돋보기 기능'] === 'boolean') {
                            setUseBarMagnifier(engine['바 돋보기 기능']);
                        } else if (typeof engine['바 돋보기 기능'] === 'string') {
                            setUseBarMagnifier(engine['바 돋보기 기능'] === "활성화");
                        }

                        // 심볼 설정
                        const symbolSection = config['심볼 설정'];

                        if (symbolSection && typeof symbolSection === 'object') {
                            // 심볼 배열 파싱
                            if (Array.isArray(symbolSection['심볼'])) {
                                setSymbolConfigs(symbolSection['심볼']);
                            } else {
                                setSymbolConfigs([]);
                            }

                            // 페어 파싱
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
                    }

                    setConfigLoaded(true);
                }
            } catch (err) {
                console.error('메시지 파싱 오류:', err);
            }
        };

        ws.addEventListener('message', handleMessage);

        return () => ws.removeEventListener('message', handleMessage);
    }, [ws, setProjectDirectory, setIsLogPanelOpen, setLogPanelHeight, setUseBarMagnifier, setSymbolConfigs, setSelectedPair, setCustomPairs, setBarDataConfigs, setConfigLoaded, setShowProjectDialog]);

    // 서버가 프로젝트 폴더 입력을 요청한 경우
    useEffect(() => {
        if (projectDirectoryRequested && !showProjectDialog) {
            setShowProjectDialog(true);
        }
    }, [projectDirectoryRequested, showProjectDialog, setShowProjectDialog]);

    // editor.json 저장 함수
    const saveConfig = () => {
        if (!ws || ws.readyState !== WebSocket.OPEN) {
            return;
        }

        ws.send(JSON.stringify({
            action: 'saveEditorConfig',
            config: {
                '에디터 설정': {
                    '로그 패널 열림': isLogPanelOpen,
                    '로그 패널 높이': logPanelHeight,
                },
                '엔진 설정': {
                    '프로젝트 폴더': projectDirectory ? projectDirectory.replace(/\\/g, '/') : projectDirectory,
                    '바 돋보기 기능': useBarMagnifier,
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
            }
        }));
    };

    // 설정이 변경될 때마다 자동 저장 (초기 로드 제외)
    useEffect(() => {
        if (!configLoaded) {
            return;
        }

        const timer = setTimeout(() => {
            saveConfig();
        }, 300);

        return () => clearTimeout(timer);
    }, [
        useBarMagnifier,
        symbolConfigs,
        selectedPair,
        customPairs,
        barDataConfigs,
        isLogPanelOpen,
        logPanelHeight
    ]);

    // 프로젝트 디렉토리 제공
    const handleProvideProjectDirectory = () => {
        if (!ws || ws.readyState !== WebSocket.OPEN) {
            return;
        }

        if (!projectDirectoryInput || projectDirectoryInput.trim() === '') {
            addLog('ERROR', '프로젝트 폴더 경로를 입력해주세요.');
            return;
        }

        const normalizedProjectDir = projectDirectoryInput.replace(/\\/g, '/');
        ws.send(JSON.stringify({action: 'provideProjectDirectory', projectDirectory: normalizedProjectDir}));

        setProjectDirectory(normalizedProjectDir);
        setShowProjectDialog(false);

        // 서버 요청 플래그 해제
        try {
            clearProjectDirectoryRequest();
        } catch (e) {
            // 무시
        }
    };

    // 프로젝트 디렉토리 다이얼로그
    if (!showProjectDialog) {
        return null;
    }

    return (
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
    );
}
