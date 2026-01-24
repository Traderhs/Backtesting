import React, {useEffect, useRef, useState} from 'react';
import {useWebSocket} from '../Server/WebSocketContext';
import {parseTimeframeString, timeframeToString} from '@/Types/BarData.ts';
import {useStrategy} from './StrategyContext';

interface Props {
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
        logs, addLog,
        configLoaded, setConfigLoaded,
        exchangeConfig, setExchangeConfig,
        lastDataUpdates, setLastDataUpdates,
        symbolConfigs, setSymbolConfigs,
        selectedPair, setSelectedPair,
        customPairs, setCustomPairs,
        barDataConfigs, setBarDataConfigs,
        fundingRatesDirectory, setFundingRatesDirectory,
        engineConfig, setEngineConfig,
        strategyConfig, setStrategyConfig,
    } = useStrategy();

    const {ws} = useWebSocket();

    const [isResizing, setIsResizing] = useState(false);
    const logContainerRef = useRef<HTMLDivElement | null>(null);

    const logPanelWrapperRef = useRef<HTMLDivElement | null>(null);
    const mainContentRef = useRef<HTMLElement | null>(null);

    const resizeLastYRef = useRef<number>(0);
    const resizeRafRef = useRef<number | null>(null);

    // 컴포넌트 마운트 시 editor.json 로드
    useEffect(() => {
        if (!ws) {
            return;
        }

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
                } else if (data.action === 'editorConfigLoaded') {  // Config 처리
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

                        // 펀딩 비율 설정
                        const fundingSection = config['펀딩 비율 설정'] || {};
                        const projectDir = (engineConfig.projectDirectory || '').replace(/\\/g, '/').replace(/\/+$/, '');

                        const joinPosix = (basePath: string, childPath: string) => {
                            const base = (basePath || '').replace(/\\/g, '/').replace(/\/\/+$/, '');
                            const child = (childPath || '').replace(/\\/g, '/').replace(/^\/\+/, '');

                            if (!base) {
                                return child;
                            }

                            return `${base}/${child}`;
                        };

                        const toAbs = (p: any) => {
                            if (!p) {
                                return null;
                            }

                            const posix = String(p).replace(/\\/g, '/');
                            if (/^[A-Za-z]:\//.test(posix) || posix.startsWith('//') || posix.startsWith('/')) {
                                return posix;
                            }

                            if (!projectDir) {
                                return posix;
                            }

                            return joinPosix(projectDir, posix);
                        };

                        // 펀딩 비율 폴더 우선순위:
                        // 1) 설정 파일에 값이 있으면 그것을 절대경로로 변환하여 사용
                        // 2) 없으면 프로젝트 디렉토리를 기준으로 Data/Funding Rates 를 기본값으로 사용
                        let fundingDir = '';
                        if (typeof fundingSection['펀딩 비율 폴더'] === 'string' && fundingSection['펀딩 비율 폴더'].trim()) {
                            fundingDir = toAbs(fundingSection['펀딩 비율 폴더']) || '';
                        }

                        if (!fundingDir) {
                            fundingDir = projectDir ? joinPosix(projectDir, 'Data/Funding Rates') : '';
                        }

                        setFundingRatesDirectory(fundingDir);

                        // 엔진 설정
                        const engine = config['엔진 설정'] || {};
                        const newEngineConfig = {
                            projectDirectory: typeof engine['프로젝트 폴더'] === 'string' ? engine['프로젝트 폴더'] : '',

                            useBacktestPeriodStart: engine['백테스팅 기간 처음부터'] === "사용",
                            useBacktestPeriodEnd: engine['백테스팅 기간 끝까지'] === "사용",
                            backtestPeriodStart: typeof engine['백테스팅 기간 시작'] === 'string' ? engine['백테스팅 기간 시작'] : '',
                            backtestPeriodEnd: typeof engine['백테스팅 기간 종료'] === 'string' ? engine['백테스팅 기간 종료'] : '',
                            backtestPeriodFormat: typeof engine['백테스팅 기간 형식'] === 'string' ? engine['백테스팅 기간 형식'] : '%Y-%m-%d %H:%M:%S',

                            useBarMagnifier: engine['바 돋보기 기능'] === "활성화",

                            initialBalance: typeof engine['초기 자금'] === 'number' ? engine['초기 자금'] : undefined,

                            takerFeePercentage: typeof engine['테이커 수수료율'] === 'number' ? engine['테이커 수수료율'] : undefined,
                            makerFeePercentage: typeof engine['메이커 수수료율'] === 'number' ? engine['메이커 수수료율'] : undefined,

                            slippageModel: typeof engine['슬리피지 모델'] === 'string' ? engine['슬리피지 모델'] as 'PercentageSlippage' | 'MarketImpactSlippage' : 'MarketImpactSlippage',
                            slippageTakerPercentage: typeof engine['테이커 슬리피지율'] === 'number' ? engine['테이커 슬리피지율'] : undefined,
                            slippageMakerPercentage: typeof engine['메이커 슬리피지율'] === 'number' ? engine['메이커 슬리피지율'] : undefined,
                            slippageStressMultiplier: typeof engine['슬리피지 스트레스 계수'] === 'number' ? engine['슬리피지 스트레스 계수'] : undefined,

                            checkMarketMaxQty: engine['시장가 최대 수량 검사'] === "활성화",
                            checkMarketMinQty: engine['시장가 최소 수량 검사'] === "활성화",
                            checkLimitMaxQty: engine['지정가 최대 수량 검사'] === "활성화",
                            checkLimitMinQty: engine['지정가 최소 수량 검사'] === "활성화",
                            checkMinNotionalValue: engine['최소 명목 가치 검사'] === "활성화",

                            checkSameBarDataWithTarget: engine['마크 가격 바 데이터와 목표 바 데이터 중복 검사'] === "활성화",
                            checkSameBarDataTrading: engine['심볼 간 트레이딩 바 데이터 중복 검사'] === "활성화",
                            checkSameBarDataMagnifier: engine['심볼 간 돋보기 바 데이터 중복 검사'] === "활성화",
                            checkSameBarDataReference: engine['심볼 간 참조 바 데이터 중복 검사'] === "활성화",
                            checkSameBarDataMarkPrice: engine['심볼 간 마크 가격 바 데이터 중복 검사'] === "활성화",
                        };

                        setEngineConfig(newEngineConfig);

                        // 전략 설정
                        const strategySection = config['전략 설정'];

                        const normalizeDirs = (p: any[]) => {
                            if (!Array.isArray(p)) {
                                return [];
                            }

                            return p
                                .map((s: any) => String(s).replace(/\\/g, '/'))
                                .map((s: string) => toAbs(s) || '')
                                .filter((s: string) => !!s);
                        };

                        if (strategySection && typeof strategySection === 'object') {
                            const selected = strategySection['선택된 전략'];

                            const strategyHeaderDirs = normalizeDirs(strategySection['전략 헤더 폴더']);
                            const strategySourceDirs = normalizeDirs(strategySection['전략 소스 폴더']);
                            const indicatorHeaderDirs = normalizeDirs(strategySection['지표 헤더 폴더']);
                            const indicatorSourceDirs = normalizeDirs(strategySection['지표 소스 폴더']);

                            if (selected && typeof selected === 'object') {
                                setStrategyConfig({
                                    strategyHeaderDirs: strategyHeaderDirs,
                                    strategySourceDirs: strategySourceDirs,
                                    indicatorHeaderDirs: indicatorHeaderDirs,
                                    indicatorSourceDirs: indicatorSourceDirs,

                                    name: selected['이름'] || '',
                                    dllPath: toAbs(selected['DLL 파일 경로']) || null,
                                    strategyHeaderPath: selected['헤더 파일 경로'] || null,
                                    strategySourcePath: selected['소스 파일 경로'] || null
                                });
                            } else {
                                // 선택된 전략 정보가 없더라도 기본 폴더는 유지
                                setStrategyConfig({
                                    strategyHeaderDirs: strategyHeaderDirs,
                                    strategySourceDirs: strategySourceDirs,
                                    indicatorHeaderDirs: indicatorHeaderDirs,
                                    indicatorSourceDirs: indicatorSourceDirs,

                                    name: '',
                                    dllPath: null,
                                    strategyHeaderPath: null,
                                    strategySourcePath: null
                                });
                            }
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
    }, [ws, addLog, setIsLogPanelOpen, setLogPanelHeight, setExchangeConfig, setLastDataUpdates, setSymbolConfigs, setSelectedPair, setCustomPairs, setBarDataConfigs, setEngineConfig, setStrategyConfig, setConfigLoaded, configLoaded, engineConfig, exchangeConfig]);


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
                '펀딩 비율 설정': {
                    '펀딩 비율 폴더': fundingRatesDirectory ? fundingRatesDirectory.replace(/\\/g, '/') : ''
                },
                '엔진 설정': {
                    '프로젝트 폴더': engineConfig.projectDirectory ? engineConfig.projectDirectory.replace(/\\/g, '/') : engineConfig.projectDirectory,

                    '백테스팅 기간 처음부터': engineConfig.useBacktestPeriodStart ? "사용" : "미사용",
                    '백테스팅 기간 끝까지': engineConfig.useBacktestPeriodEnd ? "사용" : "미사용",
                    '백테스팅 기간 시작': engineConfig.backtestPeriodStart,
                    '백테스팅 기간 종료': engineConfig.backtestPeriodEnd,
                    '백테스팅 기간 형식': engineConfig.backtestPeriodFormat,

                    '바 돋보기 기능': engineConfig.useBarMagnifier ? "활성화" : "비활성화",

                    '초기 자금': engineConfig.initialBalance,

                    '테이커 수수료율': engineConfig.takerFeePercentage,
                    '메이커 수수료율': engineConfig.makerFeePercentage,

                    '슬리피지 모델': engineConfig.slippageModel,
                    '테이커 슬리피지율': engineConfig.slippageTakerPercentage,
                    '메이커 슬리피지율': engineConfig.slippageMakerPercentage,
                    '슬리피지 스트레스 계수': engineConfig.slippageStressMultiplier,

                    '시장가 최대 수량 검사': engineConfig.checkMarketMaxQty ? "활성화" : "비활성화",
                    '시장가 최소 수량 검사': engineConfig.checkMarketMinQty ? "활성화" : "비활성화",
                    '지정가 최대 수량 검사': engineConfig.checkLimitMaxQty ? "활성화" : "비활성화",
                    '지정가 최소 수량 검사': engineConfig.checkLimitMinQty ? "활성화" : "비활성화",
                    '최소 명목 가치 검사': engineConfig.checkMinNotionalValue ? "활성화" : "비활성화",

                    '마크 가격 바 데이터와 목표 바 데이터 중복 검사': engineConfig.checkSameBarDataWithTarget ? "활성화" : "비활성화",
                    '심볼 간 트레이딩 바 데이터 중복 검사': engineConfig.checkSameBarDataTrading ? "활성화" : "비활성화",
                    '심볼 간 돋보기 바 데이터 중복 검사': engineConfig.checkSameBarDataMagnifier ? "활성화" : "비활성화",
                    '심볼 간 참조 바 데이터 중복 검사': engineConfig.checkSameBarDataReference ? "활성화" : "비활성화",
                    '심볼 간 마크 가격 바 데이터 중복 검사': engineConfig.checkSameBarDataMarkPrice ? "활성화" : "비활성화",
                },
                '전략 설정': {
                    '전략 헤더 폴더': strategyConfig?.strategyHeaderDirs || [],
                    '전략 소스 폴더': strategyConfig?.strategySourceDirs || [],
                    '지표 헤더 폴더': strategyConfig?.indicatorHeaderDirs || [],
                    '지표 소스 폴더': strategyConfig?.indicatorSourceDirs || [],

                    '선택된 전략': strategyConfig ? {
                        '이름': strategyConfig.name,
                        'DLL 파일 경로': strategyConfig.dllPath,
                        '헤더 파일 경로': strategyConfig.strategyHeaderPath,
                        '소스 파일 경로': strategyConfig.strategySourcePath
                    } : null
                },
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
        isLogPanelOpen,
        logPanelHeight,
        configLoaded,
        exchangeConfig,
        lastDataUpdates,
        symbolConfigs,
        selectedPair,
        customPairs,
        barDataConfigs,
        fundingRatesDirectory,
        engineConfig,
        strategyConfig
    ]);

    const getLogColor = (level: string) => {
        switch (level) {
            case 'INFO': {
                return 'rgb(255, 255, 255)';
            }

            case 'BALANCE':
            case 'DEBUG': {
                return 'rgb(128, 128, 128)';
            }

            case 'WARN': {
                return 'rgb(229, 192, 123)';
            }

            case 'ERROR': {
                return 'rgb(224, 108, 117)';
            }

            default: {
                return '#ffffff';
            }
        }
    };

    const handleResizeMouseDown = (e: React.MouseEvent) => {
        e.preventDefault();
        // 초기 Y 위치 설정: 이후 mouseup이 윈도우 바깥에서 발생하더라도 마지막 알려진 위치를 사용
        resizeLastYRef.current = e.clientY;
        setIsResizing(true);

        try {
            document.body.style.cursor = 'ns-resize';
        } catch (err) {
            // 무시
        }
    };

    // 드래그 중에는 DOM에 직접 높이를 적용하여 React 상태 변경을 줄임
    useEffect(() => {
        if (!isResizing) {
            return;
        }

        const MIN_HEIGHT = 200;
        const MAX_FACTOR = 0.8;

        // 마운트 시 메인 콘텐츠 요소 참조 확보
        if (!mainContentRef.current) {
            mainContentRef.current = document.querySelector('.strategy-editor-main-content');
        }

        const handleMouseMove = (e: MouseEvent) => {
            resizeLastYRef.current = e.clientY;

            if (resizeRafRef.current === null) {
                resizeRafRef.current = requestAnimationFrame(() => {
                    const lastY = resizeLastYRef.current;
                    const newHeight = window.innerHeight - lastY;
                    const clamped = Math.max(MIN_HEIGHT, Math.min(newHeight, window.innerHeight * MAX_FACTOR));

                    const wrapper = logPanelWrapperRef.current;
                    if (wrapper) {
                        // 로그 패널 높이 직접 적용
                        wrapper.style.height = `${clamped}px`;
                    }

                    const main = mainContentRef.current;
                    if (main) {
                        // 메인 콘텐츠도 inline style로 동기화 (기존 UX 유지)
                        main.style.height = `calc(100% - ${clamped}px)`;
                    }

                    resizeRafRef.current = null;
                });
            }
        };

        const handleMouseUp = (e?: MouseEvent | PointerEvent | FocusEvent | null) => {
            // 이벤트에서 clientY가 전달되면 그 값을 사용하고, 없으면 마지막으로 기록된 값을 사용
            let lastY = resizeLastYRef.current;

            if (e && (e as MouseEvent | PointerEvent).clientY !== undefined) {
                const clientY = (e as MouseEvent | PointerEvent).clientY;
                // 윈도우 범위를 벗어나면 클램프하여 과도한 collapse 발생 방지
                lastY = Math.max(0, Math.min(clientY, window.innerHeight));
            }

            const finalHeight = Math.max(MIN_HEIGHT, Math.min(window.innerHeight - lastY, window.innerHeight * MAX_FACTOR));

            setIsResizing(false);
            try {
                document.body.style.cursor = '';
            } catch (err) {
                // 무시
            }

            if (resizeRafRef.current !== null) {
                cancelAnimationFrame(resizeRafRef.current);
                resizeRafRef.current = null;
            }

            // 최종 값만 상태로 반영하여 불필요한 재렌더링 방지
            setLogPanelHeight(finalHeight);

            // 상태가 반영된 다음 프레임에 임시 inline 스타일 제거
            const wrapper = logPanelWrapperRef.current;
            const main = mainContentRef.current;

            // 마지막으로 적용한 inline 높이가 존재하면 먼저 동기화
            if (wrapper) {
                wrapper.style.height = `${finalHeight}px`;
            }

            if (main) {
                main.style.height = `calc(100% - ${finalHeight}px)`;
            }
        };

        document.addEventListener('mousemove', handleMouseMove);
        document.addEventListener('mouseup', handleMouseUp);

        window.addEventListener('pointerup', handleMouseUp as EventListener);
        window.addEventListener('blur', handleMouseUp as EventListener);

        return () => {
            document.removeEventListener('mousemove', handleMouseMove);
            document.removeEventListener('mouseup', handleMouseUp);

            window.removeEventListener('pointerup', handleMouseUp as EventListener);
            window.removeEventListener('blur', handleMouseUp as EventListener);

            if (resizeRafRef.current !== null) {
                cancelAnimationFrame(resizeRafRef.current);
                resizeRafRef.current = null;
            }

            try {
                document.body.style.cursor = '';
            } catch (err) {
                // 무시
            }
        };
    }, [isResizing, setLogPanelHeight]);

    // 사용자가 직접 스크롤하면 자동으로 맨 아래로 내리는 동작을 중지
    const isAutoScrollRef = React.useRef(true);
    const isProgrammaticScrollRef = React.useRef(false);
    const AUTO_SCROLL_THRESHOLD = 40; // 바닥에서 이 픽셀 이하이면 자동 스크롤 허용

    // 로그 컨테이너 스크롤 이벤트 처리 (사용자 조작 판단)
    const handleLogScroll = (e: React.UIEvent<HTMLDivElement>) => {
        // 프로그램적 스크롤 중에는 사용자 조작으로 판단하지 않음
        if (isProgrammaticScrollRef.current) {
            return;
        }

        const el = e.currentTarget as HTMLDivElement;
        const distanceFromBottom = el.scrollHeight - (el.scrollTop + el.clientHeight);
        isAutoScrollRef.current = distanceFromBottom < AUTO_SCROLL_THRESHOLD;
    };

    // 로그가 추가될 때, 사용자가 자동 스크롤을 허용한 경우에만 맨 아래로 스크롤
    useEffect(() => {
        if (!logContainerRef.current) {
            return;
        }

        if (isAutoScrollRef.current) {
            isProgrammaticScrollRef.current = true;
            logContainerRef.current.scrollTop = logContainerRef.current.scrollHeight;

            // 다음 프레임에 플래그 해제 (스크롤 이벤트 처리 후)
            requestAnimationFrame(() => {
                isProgrammaticScrollRef.current = false;
            });
        }
    }, [logs]);

    // 로그 패널을 열 때는 자동 스크롤을 재활성화하고 맨 아래로 이동
    useEffect(() => {
        if (isLogPanelOpen && logContainerRef.current) {
            isAutoScrollRef.current = true;
            isProgrammaticScrollRef.current = true;
            logContainerRef.current.scrollTop = logContainerRef.current.scrollHeight;

            requestAnimationFrame(() => {
                isProgrammaticScrollRef.current = false;
            });
        }
    }, [isLogPanelOpen]);

    return (
        <>

            {/* 로그 패널 */}
            {isLogPanelOpen && (
                <>
                    <div
                        role="separator"
                        aria-label="로그 패널 크기 조절"
                        aria-orientation="horizontal"
                        className={`strategy-editor-log-resize-handle strategy-editor-fade-area ${isResizing ? 'dragging' : ''}`}
                        onMouseDown={handleResizeMouseDown}
                        style={{userSelect: 'none'}}
                        tabIndex={0}
                    />

                    <div
                        ref={logPanelWrapperRef}
                        className="strategy-editor-log-panel strategy-editor-fade-area flex flex-col"
                        style={{height: `${logPanelHeight}px`}}
                    >
                        <div className="px-4 py-2 strategy-editor-log-header flex items-center justify-end">
                            <button
                                onClick={() => setIsLogPanelOpen(false)}
                                className="strategy-editor-dropdown-option-remove strategy-editor-log-close-large"
                                aria-label="로그 숨기기"
                                title="로그 숨기기"
                            >
                                ×
                            </button>
                        </div>
                        <div ref={logContainerRef} onScroll={handleLogScroll}
                             className="overflow-y-auto p-4 font-mono text-sm flex-1"
                             style={{fontFamily: "'Inter', 'Pretendard', monospace"}}>
                            {logs.length === 0 ? null : (
                                logs.map((log, index) => {
                                    if (log.level === 'SEPARATOR') {
                                        return (
                                            <div key={index} className="my-0">
                                                <div style={{
                                                    whiteSpace: 'pre',
                                                    fontFamily: "'Inter', 'Pretendard', monospace",
                                                    fontSize: '14px',
                                                    lineHeight: '1.4',
                                                    color: 'rgba(255, 215, 0, 0.4)',
                                                    marginTop: '-6px',
                                                    marginBottom: '1px',
                                                    padding: 0,
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
                                        <div key={index} className="mb-2">
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
