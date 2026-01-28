import {useEffect, useRef, useState} from 'react';
import {createPortal} from 'react-dom';
import {motion} from 'framer-motion';
import {FileText, Play} from 'lucide-react';
import {useWebSocket} from '../Server/WebSocketContext';
import {BarDataType, timeframeToString, TimeframeUnit} from '@/Types/BarData.ts';
import EditorSection from './EditorSection';
import SymbolSection from './SymbolSection';
import BarDataSection from './BarDataSection';
import ConfigSection from './ConfigSection';
import ExchangeSection from './ExchangeSection';
import StrategySection from './StrategySection';
import {StrategyProvider, useStrategy} from './StrategyContext';
import '../Common/LoadingSpinner.css';
import './StrategyEditor.css';

function StrategyEditorContent({isActive, onFullyLoaded}: { isActive: boolean, onFullyLoaded?: () => void }) {
    const [titleBarPortal, setTitleBarPortal] = useState<HTMLElement | null>(null);
    const containerRef = useRef<HTMLDivElement>(null);

    useEffect(() => {
        setTitleBarPortal(document.getElementById('titlebar-actions-portal'));
    }, []);

    // 탭 전환 시 페이드 인/아웃 및 잔재 중립화 로직
    useEffect(() => {
        const fadeDuration = 300; // ms
        const listOuterSelector = '.strategy-editor-fade-area';

        const getListOuters = () => {
            if (!containerRef.current) {
                return [] as HTMLElement[];
            }

            return Array.from(containerRef.current.querySelectorAll(listOuterSelector)) as HTMLElement[];
        };

        const fadeIn = () => {
            const listOuters = getListOuters();
            if (listOuters.length === 0) {
                return;
            }

            for (const listOuter of listOuters) {
                listOuter.style.transition = `opacity ${fadeDuration}ms ease-out`;
                listOuter.style.visibility = 'visible';
                listOuter.style.pointerEvents = 'auto';
                listOuter.style.opacity = '0';

                void listOuter.offsetHeight;
                requestAnimationFrame(() => (listOuter.style.opacity = '1'));
            }
        };

        const fadeOut = () => {
            const listOuters = getListOuters();
            if (listOuters.length === 0) {
                return;
            }

            for (const listOuter of listOuters) {
                listOuter.style.transition = `opacity ${fadeDuration}ms ease-out`;
                listOuter.style.opacity = '0';
                listOuter.style.pointerEvents = 'none';
            }

            setTimeout(() => {
                for (const listOuter of listOuters) {
                    listOuter.style.visibility = 'hidden';
                }
            }, fadeDuration + 20);
        };

        // DOM subtree 내부의 transform / will-change / backface-visibility를 중립화
        const neutralize = (target: HTMLElement | null) => {
            if (!target) {
                return;
            }

            const walker = document.createTreeWalker(target, NodeFilter.SHOW_ELEMENT, null);
            const nodes: HTMLElement[] = [];

            let cur = walker.currentNode as HTMLElement | null;
            while (cur) {
                nodes.push(cur);
                cur = walker.nextNode() as HTMLElement | null;
            }

            for (const el of nodes) {
                try {
                    const cs = window.getComputedStyle(el);
                    const transformStr = cs.transform || '';
                    const willChangeTransform = cs.willChange && cs.willChange.includes('transform');
                    const backfaceHidden = cs.backfaceVisibility === 'hidden';

                    // translate(...) 또는 matrix(...)에서 실질적인 translation이 포함된 경우에만 처리
                    const isTranslateOrMatrixWithTranslation = (t: string) => {
                        if (!t || t === 'none') {
                            return false;
                        }

                        if (/\btranslate(?:3d)?\b/i.test(t)) {
                            return true;
                        }

                        const m = t.match(/matrix\(([^)]+)\)/);
                        if (m) {
                            const parts = m[1].split(',').map(p => p.trim());
                            if (parts.length === 6) {
                                const tx = parseFloat(parts[4]) || 0;
                                const ty = parseFloat(parts[5]) || 0;
                                return Math.abs(tx) > 0.0001 || Math.abs(ty) > 0.0001;
                            }
                        }
                        return false;
                    };

                    const shouldNeutralizeTransform = isTranslateOrMatrixWithTranslation(transformStr);

                    // 인터랙티브 요소 및 명시적 UI 클래스는 유지
                    const skipSelector = 'button, a, input, textarea, select, .strategy-editor-path-reset-button, .strategy-editor-file-selector-button, .strategy-editor-button, .strategy-editor-toggle-slider, .strategy-editor-log-resize-handle';
                    const isInteractive = el.matches && el.matches(skipSelector);

                    if ((shouldNeutralizeTransform && !isInteractive) || willChangeTransform || backfaceHidden) {
                        if (shouldNeutralizeTransform) {
                            el.style.transform = 'none';
                        }

                        if (willChangeTransform) {
                            el.style.willChange = 'auto';
                        }

                        if (backfaceHidden) {
                            (el.style as any).backfaceVisibility = 'visible';
                        }
                    }
                } catch (e) {
                    // 접근 불가 요소 무시
                }
            }

            void target.offsetHeight; // 강제 리플로우
        };

        const handleTabActive = () => {
            // App의 탭 애니메이션이 끝난 뒤 실행
            setTimeout(() => {
                fadeIn();

                setTimeout(() => {
                    const listOuters = getListOuters();
                    for (const listOuter of listOuters) {
                        neutralize(listOuter);
                    }
                }, fadeDuration + 30);
            }, 550);
        };

        const handleTabInactive = () => {
            fadeOut();
        };

        const tabContentElement = containerRef.current?.closest('.tab-content');
        if (tabContentElement) {
            tabContentElement.addEventListener('tabActive', handleTabActive);
            tabContentElement.addEventListener('tabInactive', handleTabInactive);
        }

        return () => {
            if (tabContentElement) {
                tabContentElement.removeEventListener('tabActive', handleTabActive);
                tabContentElement.removeEventListener('tabInactive', handleTabInactive);
            }
        };
    }, []);

    // StrategyEditor의 진입 애니메이션(탭 활성화 + fade + motion)이 완전히 끝난 뒤에만 준비 완료 신호 전송
    useEffect(() => {
        if (!isActive || fullyLoadedCalledRef.current || !onFullyLoaded) {
            return;
        }

        let fallbackTimer: number | null = null;
        let scheduleTimer: number | null = null;
        let tabHandled = false;

        const scheduleFullyLoaded = () => {
            if (fullyLoadedCalledRef.current) {
                return;
            }

            // 안전 마진: App 탭 딜레이(≈550ms) + fade/motion(≈400ms) + 여유
            scheduleTimer = window.setTimeout(() => {
                fullyLoadedCalledRef.current = true;
                try {
                    onFullyLoaded();
                } catch (e) {
                    // 안전하게 무시
                }
            }, 1200);
        };

        const tabContent = containerRef.current?.closest('.tab-content');
        const onTabActive = () => {
            tabHandled = true;

            if (fallbackTimer) {
                clearTimeout(fallbackTimer);
                fallbackTimer = null;
            }

            scheduleFullyLoaded();
        };

        if (tabContent) {
            tabContent.addEventListener('tabActive', onTabActive, {once: true});
        }

        // 초기 마운트 시 tabActive 이벤트가 발생하지 않을 수 있으므로 짧게 대기 후 스케줄
        fallbackTimer = window.setTimeout(() => {
            if (!tabHandled) {
                scheduleFullyLoaded();
            }
        }, 50);

        return () => {
            if (fallbackTimer) {
                clearTimeout(fallbackTimer);
            }

            if (scheduleTimer) {
                clearTimeout(scheduleTimer);
            }

            if (tabContent) {
                tabContent.removeEventListener('tabActive', onTabActive as EventListener);
            }
        };
    }, [isActive, onFullyLoaded]);

    const {
        addLog,
        clearLogs,
        exchangeConfig,
        symbolConfigs,
        barDataConfigs,
        fundingRatesDirectory,
        engineConfig,
        strategyConfig
    } = useStrategy();

    const {ws} = useWebSocket();

    // 설정의 이전 해시값 저장 (변경 감지용)
    const previousConfigHash = useRef<string>('');

    // 준비 완료 신호 중복 방지를 위한 플래그
    const fullyLoadedCalledRef = useRef(false);

    const [isLogPanelOpen, setIsLogPanelOpen] = useState(false);
    const [logPanelHeight, setLogPanelHeight] = useState(400);

    // 바 데이터 설정 해시 계산 (심볼 리스트 + 바 데이터 설정 + 바 돋보기 상태)
    const calculateConfigHash = () => {
        const config = {
            symbols: [...symbolConfigs],  // 심볼 추가 순서가 의미 있으므로 순서를 보존하여 비교
            barData: barDataConfigs.map(c => ({
                timeframe: timeframeToString(c.timeframe),
                klinesDirectory: c.klinesDirectory,
                barDataType: c.barDataType
            })),
            useBarMagnifier: engineConfig.useBarMagnifier
        };

        return JSON.stringify(config);
    };

    // 백테스팅 실행
    const handleRunSingleBacktesting = async () => {
        // 백테스팅 시작 시간
        const backtestingStartTime = Date.now();

        // 로그 초기화
        clearLogs();

        // 로그 패널이 닫혀있으면 자동으로 열기
        if (!isLogPanelOpen) {
            setIsLogPanelOpen(true);
        }

        if (!ws || ws.readyState !== WebSocket.OPEN) {
            addLog('ERROR', 'WebSocket 연결이 없습니다.');
            return;
        }

        if (!exchangeConfig.apiKeyEnvVar.trim()) {
            addLog('ERROR', 'Binance API Key의 환경 변수 이름을 입력해 주세요.');
            return;
        }

        if (!exchangeConfig.apiSecretEnvVar.trim()) {
            addLog('ERROR', 'Binance API Secret의 환경 변수 이름을 입력해 주세요.');
            return;
        }

        if (!exchangeConfig.exchangeInfoPath.trim()) {
            addLog('ERROR', '거래소 정보 파일 경로를 입력해 주세요.');
            return;
        }

        if (!exchangeConfig.leverageBracketPath.trim()) {
            addLog('ERROR', '레버리지 구간 파일 경로를 입력해 주세요.');
            return;
        }

        if (symbolConfigs.length === 0) {
            addLog('ERROR', '백테스팅을 실행하려면 최소 1개의 심볼이 필요합니다. 심볼을 추가해 주세요.');
            return;
        }

        const configsToValidate = barDataConfigs.filter(config =>
            config.barDataType !== BarDataType.MAGNIFIER || engineConfig.useBarMagnifier
        );

        for (const config of configsToValidate) {
            if (!config.klinesDirectory.trim()) {
                addLog('ERROR', `${config.barDataType} 바 데이터의 폴더를 입력해 주세요.`);
                return;
            }
        }

        // 타임프레임 유효성 검사
        for (const config of configsToValidate) {
            const tf = config.timeframe;
            if (!tf || tf.value === null) {
                addLog('ERROR', `${config.barDataType} 바 데이터의 타임프레임 값을 입력해 주세요.`);
                return;
            }

            if (!tf || tf.unit === TimeframeUnit.NULL) {
                addLog('ERROR', `${config.barDataType} 바 데이터의 타임프레임 단위를 입력해 주세요.`);
                return;
            }
        }

        if (!fundingRatesDirectory.trim()) {
            addLog('ERROR', '펀딩 비율 폴더를 입력해 주세요.');
            return;
        }

        // 엔진 설정 검사
        // 백테스팅 기간 체크 박스 검증: 체크 안되어있는데 칸이 비워져 있는 경우
        if (!engineConfig.useBacktestPeriodStart && !engineConfig.backtestPeriodStart.trim()) {
            addLog('ERROR', '백테스팅 시작 시간을 입력하거나 [처음부터] 체크 박스를 선택해 주세요.');
            return;
        }

        if (!engineConfig.useBacktestPeriodEnd && !engineConfig.backtestPeriodEnd.trim()) {
            addLog('ERROR', '백테스팅 종료 시간을 입력하거나 [끝까지] 체크 박스를 선택해 주세요.');
            return;
        }

        if (!engineConfig.initialBalance) {
            addLog('ERROR', '초기 자금을 입력해 주세요.');
            return;
        }

        if (!engineConfig.takerFeePercentage) {
            addLog('ERROR', '테이커 수수료율을 입력해 주세요.');
            return;
        }

        if (!engineConfig.makerFeePercentage) {
            addLog('ERROR', '메이커 수수료율을 입력해 주세요.');
            return;
        }

        if (engineConfig.slippageModel == 'PercentageSlippage') {
            if (!engineConfig.slippageTakerPercentage) {
                addLog('ERROR', '테이커 슬리피지율을 입력해 주세요.');
                return;
            }

            if (!engineConfig.slippageMakerPercentage) {
                addLog('ERROR', '메이커 슬리피지율을 입력해 주세요.');
                return;
            }
        } else if (engineConfig.slippageModel == 'MarketImpactSlippage') {
            if (!engineConfig.slippageStressMultiplier) {
                addLog('ERROR', '슬리피지 스트레스 계수를 입력해 주세요.');
                return;
            }
        }

        // 전략 빌드 먼저 수행
        if (strategyConfig && strategyConfig.strategySourcePath) {
            addLog('INFO', `[${strategyConfig.name}] 전략의 빌드를 시작합니다.`);

            // strategyHeaderPath 검증
            if (!strategyConfig.strategyHeaderPath) {
                addLog('ERROR', '전략 헤더 파일 경로가 존재하지 않습니다.');
                return;
            }

            try {
                const buildResponse = await fetch('/api/strategy/build', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify({
                        strategySourcePath: strategyConfig.strategySourcePath,
                        strategyHeaderPath: strategyConfig.strategyHeaderPath,
                        indicatorHeaderDirs: strategyConfig.indicatorHeaderDirs,
                        indicatorSourceDirs: strategyConfig.indicatorSourceDirs
                    })
                });

                const buildResult = await buildResponse.json();

                if (buildResult.success) {
                    addLog("SEPARATOR", "");
                } else {
                    // 오류 로그는 서버에서 기록
                    return;
                }
            } catch (err: any) {
                addLog('ERROR', `빌드 요청 실패: ${err.message}`);
                return;
            }
        } else {
            addLog('ERROR', '전략을 추가해 주세요.');
            return;
        }

        // =============================================================================================================
        const configsToSend = barDataConfigs.filter(config =>
            config.barDataType !== BarDataType.MAGNIFIER || engineConfig.useBarMagnifier
        );

        // 현재 바 데이터 설정의 해시값 계산
        const currentHash = calculateConfigHash();

        // 해시가 없거나 (최초 실행) 해시가 변경된 경우에만 clear and add
        const needsClearAndAdd = previousConfigHash.current === '' ||
            currentHash !== previousConfigHash.current;

        ws.send(JSON.stringify({
            action: 'runSingleBacktesting',
            backtestingStartTime: backtestingStartTime,
            symbolConfigs: symbolConfigs,
            barDataConfigs: configsToSend.map(config => ({
                timeframe: timeframeToString(config.timeframe),
                klinesDirectory: (config.klinesDirectory || '').replace(/\\/g, '/'),
                barDataType: config.barDataType
            })),
            fundingRatesDirectory: (fundingRatesDirectory || '').replace(/\\/g, '/'),
            useBarMagnifier: engineConfig.useBarMagnifier,
            clearAndAddBarData: needsClearAndAdd,
            strategyConfig: strategyConfig
        }));

        // 현재 해시값 저장
        previousConfigHash.current = currentHash;
    };

    // 데이터 다운로드 / 업데이트
    const handleFetchOrUpdateBarData = async (operation: 'download' | 'update') => {
        const confirmMsg = operation === 'download' ? '추가된 심볼들의 바 데이터를 다운로드하시겠습니까?' : '추가된 심볼들의 바 데이터를 업데이트하시겠습니까?';

        // Electron 환경이면 네이티브 다이얼로그 사용, 아니면 브라우저 confirm으로 폴백
        let confirmed: boolean;
        try {
            if (window && (window as any).electronAPI && (window as any).electronAPI.showConfirm) {
                confirmed = await (window as any).electronAPI.showConfirm({
                    title: 'BackBoard',
                    message: confirmMsg
                });
            } else {
                confirmed = confirm(confirmMsg);
            }
        } catch (e) {
            // 실패 시 안전하게 폴백
            confirmed = confirm(confirmMsg);
        }

        if (!confirmed) {
            return;
        }

        // 로그 초기화
        clearLogs();

        // 로그 패널이 닫혀있으면 자동으로 열기
        if (!isLogPanelOpen) {
            setIsLogPanelOpen(true);
        }

        if (!ws || ws.readyState !== WebSocket.OPEN) {
            addLog('ERROR', 'WebSocket 연결이 없습니다.');
            return;
        }

        if (!symbolConfigs || symbolConfigs.length === 0) {
            addLog('ERROR', operation === 'download' ? '바 데이터를 다운로드하려면 최소 1개의 심볼이 필요합니다. 심볼을 추가해 주세요.' : '바 데이터를 업데이트하려면 최소 1개의 심볼이 필요합니다. 심볼을 추가해 주세요.');
            return;
        }

        const payloadBarDataConfigs = barDataConfigs.map(c => ({
            barDataType: c.barDataType,
            timeframe: timeframeToString(c.timeframe)
        }));

        ws.send(JSON.stringify({
            action: 'fetchOrUpdateBarData',
            operation: operation === 'download' ? 'download' : 'update',
            symbolConfigs: symbolConfigs,
            barDataConfigs: payloadBarDataConfigs
        }));
    };

    return (
        <div ref={(el) => {
            containerRef.current = el;
        }} className="flex flex-col h-full w-full">
            {/* 메인 콘텐츠 영역 */}
            <div className="strategy-editor-main-content h-full w-full flex flex-col p-4 overflow-y-auto"
                 style={{
                     height: isLogPanelOpen ? `calc(100% - ${logPanelHeight}px)` : '100%'
                 }}>
                {/* 제목 영역 */}
                <div style={{
                    position: 'relative',
                    marginBottom: '25px',
                    zIndex: 100
                }}>
                    <motion.h2
                        initial={{opacity: 0, x: -20}}
                        animate={{opacity: 1, x: 0}}
                        transition={{delay: 0.1, duration: 0.5}}
                        style={{
                            color: 'white',
                            fontSize: '2.5rem',
                            fontWeight: 700,
                            textAlign: 'left',
                            marginLeft: '35px',
                            marginTop: '10px',
                            paddingBottom: '8px',
                            display: 'inline-block',
                            position: 'relative',
                        }}
                    >
                        전략 에디터
                        {/* 밑줄 */}
                        <motion.span
                            initial={{width: 0}}
                            animate={{width: '100%'}}
                            transition={{delay: 0.3, duration: 0.5}}
                            style={{
                                position: 'absolute',
                                bottom: 0,
                                left: 0,
                                right: 0,
                                height: '2px',
                                background: 'rgba(255, 215, 0, 0.4)',
                            }}
                        />
                    </motion.h2>
                </div>

                {isActive && titleBarPortal && createPortal(
                    <div
                        className="flex items-center gap-2"
                        style={{
                            WebkitAppRegion: 'no-drag',
                            gap: '12px'
                        } as any}
                    >
                        <button
                            type="button"
                            onClick={() => setIsLogPanelOpen(prev => !prev)}
                            title={isLogPanelOpen ? '로그 숨기기' : '로그 표시'}
                            aria-pressed={isLogPanelOpen}
                            aria-label={isLogPanelOpen ? '로그 숨기기' : '로그 표시'}
                            className="strategy-editor-action-button log"
                        >
                            <FileText size={16}/>
                        </button>

                        <button
                            type="button"
                            onClick={handleRunSingleBacktesting}
                            title="백테스팅 실행"
                            aria-label="백테스팅 실행"
                            className="strategy-editor-action-button play"
                        >
                            <Play size={16}/>
                        </button>
                    </div>,
                    titleBarPortal
                )}

                <div className="strategy-editor-fade-area" style={{width: '100%'}}>
                    <div className="grid grid-cols-1 lg:grid-cols-2 gap-4">
                        {/* 거래소 설정 */}
                        <div className="h-full">
                            <ExchangeSection/>
                        </div>

                        {/* 심볼 섹션 */}
                        <div className="h-full">
                            <SymbolSection/>
                        </div>
                    </div>

                    {/* 바 데이터 설정 */}
                    <BarDataSection onFetchOrUpdateBarData={handleFetchOrUpdateBarData}/>

                    <div
                        className="grid grid-cols-1 lg:grid-cols-2 gap-4"
                        style={{
                            marginTop: '32px',
                            marginBottom: '32px'
                        }}
                    >
                        {/* 엔진 설정 */}
                        <div className="h-full">
                            <ConfigSection/>
                        </div>

                        {/* 전략 설정 */}
                        <div className="h-full">
                            <StrategySection/>
                        </div>
                    </div>
                </div>
            </div>

            {/* 에디터 설정 */}
            <EditorSection
                isLogPanelOpen={isLogPanelOpen}
                setIsLogPanelOpen={setIsLogPanelOpen}
                logPanelHeight={logPanelHeight}
                setLogPanelHeight={setLogPanelHeight}
            />
        </div>
    );
}

/**
 * 전략 에디터 컴포넌트
 * 백테스팅 전략을 편집하고 실행할 수 있는 UI 제공
 */
export default function StrategyEditor({isActive, onFullyLoaded}: { isActive?: boolean, onFullyLoaded?: () => void }) {
    return (
        <StrategyProvider>
            <StrategyEditorContent isActive={isActive ?? true} onFullyLoaded={onFullyLoaded}/>
        </StrategyProvider>
    );
}
