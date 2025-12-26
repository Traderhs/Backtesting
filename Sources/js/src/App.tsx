import {lazy, Suspense, useEffect, useRef, useState} from "react";
import {useWebSocket, WebSocketProvider} from '@/components/Server/WebSocketContext';
import ServerAlert from "@/components/Server/ServerAlert.tsx";
import {TradeFilterProvider} from "@/components/TradeFilter/TradeFilterProvider.tsx";
import {LogoProvider, useLogo} from "@/contexts/LogoContext";
import Sidebar from "@/components/Sidebar/Sidebar";
import LoadingSpinner from '@/components/Common/LoadingSpinner';

// Plotly 미리 로딩 (앱 시작 시)
const plotlyPreloadPromise = import('react-plotly.js').then(module => {
    return module.default;
}).catch(err => {
    console.warn('[App] Plotly preload failed:', err);
    return null;
});

// 전역에서 Plotly 접근 가능하도록 window 객체에 추가
window.plotlyPreload = plotlyPreloadPromise;

// 코드 스플리팅으로 탭 컴포넌트들을 lazy 로딩
const StrategyEditor = lazy(() => import("@/components/StrategyEditor/StrategyEditor"));
const Overview = lazy(() => import("@/components/Overview/Overview"));
const Performance = lazy(() => import("@/components/Performance/Performance"));
const Plot = lazy(() => import("@/components/Plot/Plot"));
const Chart = lazy(() => import("@/components/Chart/Chart.tsx"));
const TradeList = lazy(() => import("@/components/TradeList/TradeList"));
const Config = lazy(() => import("@/components/Config/Config"));
const Log = lazy(() => import("@/components/Log/Log"));
const StarField = lazy(() => import("@/components/StarField").then(module => ({
    default: module.StarField
})));

/**
 * 메인 콘텐츠 영역 스타일을 조정하는 함수
 * 고정된 사이드바 너비 기준으로 메인 콘텐츠의 마진과 너비를 설정
 *
 * @returns 메인 콘텐츠에 적용할 스타일 객체
 */
const getMainContentStyle = () => {
    return {
        marginLeft: "18rem",
        width: "calc(100% - 18rem)",
        display: "flex" as const,
        flex: 1,
        justifyContent: "flex-start" as const,
        alignItems: "stretch" as const,
        position: "absolute" as const,
        top: 0,
        right: 0,
        bottom: 0,
        overflow: "auto" as const,
    };
};

function AppContent() {
    const {serverError} = useWebSocket();
    const {isGlobalLoading} = useLogo(); // 전체 로딩 상태 가져오기
    const [tab, setTab] = useState("StrategyEditor");
    const [prevTab, setPrevTab] = useState("StrategyEditor");
    const [animationDirection, setAnimationDirection] = useState<"" | "slide-in-left" | "slide-in-right">("");
    const [isAnimating, setIsAnimating] = useState(false);
    const [isLogTextOptimizing, setIsLogTextOptimizing] = useState(false); // Log 탭 텍스트 최적화 중 상태

    // 탭 순서 정의
    const tabOrder = ["StrategyEditor", "Overview", "Performance", "Plot", "Chart", "TradeList", "Config", "Log"];

    // 플롯 타입 순서 정의 (애니메이션 방향 결정용)
    const plotTypeOrder = ["equity-drawdown", "profit-loss-comparison", "holding-time-pnl-distribution", "symbol-performance"];

    // 각 탭의 방문 상태 추적
    const [visitedTabs, setVisitedTabs] = useState<Record<string, boolean>>({
        StrategyEditor: true, // 초기 탭은 기본으로 방문한 것으로 설정
        Overview: false,
        Performance: false,
        Plot: false,
        Chart: false,
        TradeList: false,
        Config: false,
        Log: false
    });

    const [chartConfig, setChartConfig] = useState<{
        symbol: string;
        timeframe: string;
        priceStep: number;
        pricePrecision: number;
    } | null>(null);
    const [isChartLoading, setIsChartLoading] = useState(false);
    const [config, setConfig] = useState<any>(null);

    // 분석 그래프(Plot) 탭의 활성 플롯 타입 상태 추가
    const [activePlotType, setActivePlotType] = useState<string>("equity-drawdown");

    // StarField 워커 참조 (전역 상태 공유를 위해)
    const starFieldWorkerRef = useRef<Worker | null>(null);

    // 애니메이션 상태 변경 시 StarField 워커에 알리기
    useEffect(() => {
        if (starFieldWorkerRef.current) {
            starFieldWorkerRef.current.postMessage({
                type: 'animatingStateChange',
                isAnimating: isAnimating
            });
        }
    }, [isAnimating]);

    // 탭 전환 함수
    const handleSelectTab = (
        tabName: string,
        configParam?: {
            symbol?: string;
            timeframe?: string;
            priceStep?: number;
            pricePrecision?: number;
            plotType?: string;
        },
        sidebarDirection?: 'left' | 'right'
    ) => {
        // 같은 탭을 클릭했고, 설정 변경이 없는 경우 무시
        if (tabName === tab && !configParam) {
            return;
        }

        // 이동 방향 설정
        let direction: "slide-in-left" | "slide-in-right";

        // Sidebar에서 전달받은 direction이 있으면 우선 사용
        if (sidebarDirection) {
            direction = sidebarDirection === 'left' ? "slide-in-left" : "slide-in-right";
        }
        // Sidebar에서 전달받은 direction이 없는 경우에만 기존 로직 사용
        else if (tabName === "Chart" && configParam && 'symbol' in configParam) {
            const symbols = config?.["심볼"] || [];

            // 현재 활성화된 심볼 (이전 심볼)
            const currentSymbol = chartConfig?.symbol;
            // 새로 선택된 심볼
            const newSymbol = configParam.symbol;

            // 현재 심볼이 없으면 (첫 차트 진입) 항상, 오른쪽에서 등장
            if (!currentSymbol) {
                direction = "slide-in-right";
            } else {
                // 인덱스 기반 방향 결정 (인덱스가 확실하게 있을 때만)
                const currentIndex = symbols.findIndex((s: any) => s["심볼 이름"] === currentSymbol);
                const newIndex = symbols.findIndex((s: any) => s["심볼 이름"] === newSymbol);

                if (currentIndex !== -1 && newIndex !== -1) {
                    // 인덱스 값이 커질수록 오른쪽에서 들어오게 (탭 이동과 일치)
                    direction = currentIndex < newIndex ? "slide-in-right" : "slide-in-left";
                } else {
                    // 인덱스를 찾지 못한 경우 기본값
                    direction = "slide-in-right";
                }
            }
        }
        // Plot 타입 변경 시 핸들링 - configParam에서 plotType 확인
        else if (tabName === "Plot" && configParam && 'plotType' in configParam && configParam.plotType && configParam.plotType !== activePlotType) {
            // Plot 탭 내에서 플롯 타입 변경 시 인덱스 기반 방향 결정
            const currentPlotTypeIndex = plotTypeOrder.indexOf(activePlotType);
            const newPlotTypeIndex = plotTypeOrder.indexOf(configParam.plotType);

            if (currentPlotTypeIndex !== -1 && newPlotTypeIndex !== -1) {
                direction = currentPlotTypeIndex < newPlotTypeIndex ? "slide-in-right" : "slide-in-left";
            } else {
                // 인덱스를 찾지 못한 경우 기본값 (오른쪽)
                direction = "slide-in-right";
            }
        } else if (tabName === tab) {
            // 같은 탭에서 다른 변경 (거의 발생하지 않음)
            direction = "slide-in-right";
        } else {
            // 다른 탭으로 이동
            const prevIndex = tabOrder.indexOf(tab);
            const newIndex = tabOrder.indexOf(tabName);
            direction = prevIndex < newIndex ? "slide-in-right" : "slide-in-left";
        }

        // 모든 필요한 상태를 미리 계산하고 저장 (DOM 읽기 작업 선행)
        const currentPrevTab = tab;
        const newTab = tabName;
        const isNewVisit = !visitedTabs[tabName];

        // 차트 설정 업데이트 필요 여부 (실제로 변경이 있을 때만)
        const needsChartUpdate =
            tabName === "Chart" &&
            configParam &&
            'symbol' in configParam &&
            'timeframe' in configParam &&
            'priceStep' in configParam &&
            'pricePrecision' in configParam &&
            (
                !chartConfig || // 차트 설정이 아직 없거나
                chartConfig.symbol !== configParam.symbol || // 심볼이 다르거나
                chartConfig.timeframe !== configParam.timeframe || // 타임프레임이 다르거나
                chartConfig.priceStep !== configParam.priceStep || // 가격 최소 단위가 다르거나
                chartConfig.pricePrecision !== configParam.pricePrecision // 가격 소수점 정밀도가 다를 때만
            );

        // 플롯 타입 업데이트 필요 여부
        const needsPlotUpdate =
            tabName === "Plot" &&
            configParam &&
            'plotType' in configParam &&
            configParam.plotType;

        // 모든 DOM 변경을 다음 프레임으로 지연 (강제 레이아웃 계산 방지)
        requestAnimationFrame(() => {
            // 애니메이션 설정
            setIsAnimating(true);
            setAnimationDirection(direction);

            // 현재 탭을 이전 상태로 저장
            setPrevTab(currentPrevTab);

            // 탭 변경 먼저 적용
            setTab(newTab);

            // Log 탭으로 전환할 때 텍스트 최적화 로딩 시작
            if (newTab === "Log") {
                setIsLogTextOptimizing(true);

                // 텍스트 최적화 완료까지 로딩 표시 (800ms 후 해제)
                setTimeout(() => {
                    setIsLogTextOptimizing(false);
                }, 800);
            }

            // 탭 방문 상태 업데이트
            if (isNewVisit) {
                setVisitedTabs(prev => ({
                    ...prev,
                    [newTab]: true
                }));
            }

            // 이전 탭 요소를 일시적으로 비활성화 (가시성 감지를 통한 성능 최적화)
            if (currentPrevTab !== newTab) {
                const prevTabElement = document.querySelector(`.tab-content[data-tab="${currentPrevTab}"]`);
                if (prevTabElement) {
                    // 이전 탭에 특별한 클래스 추가 - CSS에서 렌더링을 최적화하는 속성 적용
                    prevTabElement.classList.add('tab-inactive');

                    // 이벤트 디스패치 - 컴포넌트에게 비활성화 알림
                    prevTabElement.dispatchEvent(new CustomEvent('tabInactive', {
                        bubbles: true,
                        detail: {
                            previousTab: currentPrevTab,
                            newTab: newTab
                        }
                    }));
                }

                // 새 탭 요소 활성화
                const newTabElement = document.querySelector(`.tab-content[data-tab="${newTab}"]`);
                if (newTabElement) {
                    // 비활성 클래스 제거
                    newTabElement.classList.remove('tab-inactive');

                    // 이벤트 디스패치 - 컴포넌트에게 활성화 알림
                    newTabElement.dispatchEvent(new CustomEvent('tabActive', {
                        bubbles: true,
                        detail: {
                            previousTab: currentPrevTab,
                            newTab: newTab
                        }
                    }));
                }
            }

            // 차트 관련 설정 업데이트 (탭 변경 후)
            if (needsChartUpdate) {
                // 필요한 모든 속성이 있는 경우에만 chartConfig 업데이트
                const newChartConfig = {
                    symbol: configParam.symbol as string,
                    timeframe: configParam.timeframe as string,
                    priceStep: configParam.priceStep as number,
                    pricePrecision: configParam.pricePrecision as number,
                };

                // 차트 로딩 상태 활성화 및 새 설정 적용
                setIsChartLoading(true);
                setChartConfig(newChartConfig);
            }

            // 분석 그래프(Plot) 관련 설정 업데이트
            if (needsPlotUpdate) {
                // 플롯 타입이 있으면 그 값으로 설정
                if (configParam.plotType) {
                    setActivePlotType(configParam.plotType);
                }
            }

            // 애니메이션 종료 후 상태 정리 및 탭 리렌더링 트리거
            setTimeout(() => {
                requestAnimationFrame(() => {
                    setIsAnimating(false);
                    setAnimationDirection("");

                    // 약간의 딜레이 후 상태 변경으로 리렌더링 유도
                    setTimeout(() => {
                        requestAnimationFrame(() => {
                            // 상태 업데이트를 통해 리렌더링 트리거
                            setVisitedTabs(prev => ({...prev}));
                        });
                    }, 50);
                });
            }, 500);
        });
    };

    // 차트 로딩 완료 처리 함수
    const handleChartLoaded = () => {
        setIsChartLoading(false);
    };

    // 차트 설정이 변경될 때마다 로딩 상태 리셋
    useEffect(() => {
        if (chartConfig) {
            // 차트 설정이 변경되면(다른 심볼 선택 시) 로딩 상태 활성화
            setIsChartLoading(true);
        }
    }, [chartConfig]);

    // 문서 타이틀 동적 업데이트: 끝에 항상 "| BackBoard"를 추가
    useEffect(() => {
        const tabLabelMap: Record<string, string> = {
            StrategyEditor: '전략 에디터',
            Overview: '전체 요약',
            Performance: '성과 지표',
            Plot: '분석 그래프',
            Chart: '거래 차트',
            TradeList: '거래 내역',
            Config: '백테스팅 설정',
            Log: '백테스팅 로그'
        };

        const plotTypeMap: Record<string, string> = {
            'equity-drawdown': '자금 & 드로우다운',
            'profit-loss-comparison': '시간별 순손익 비교',
            'holding-time-pnl-distribution': '보유 시간 순손익 분포',
            'symbol-performance': '심볼별 성과 추이'
        };

        let title: string;

        if (tab === 'Chart' && chartConfig && chartConfig.symbol) {
            // '심볼 - 거래 차트' 형식
            title = `${chartConfig.symbol} - 거래 차트`;
        } else if (tab === 'Plot' && activePlotType) {
            const plotName = plotTypeMap[activePlotType] || activePlotType;

            // '세부 - 분석 그래프' 형식
            title = `${plotName} - ${tabLabelMap[tab]}`;
        } else {
            // 기본: 탭 레이블만
            title = tabLabelMap[tab] || tab;
        }

        // 항상 BackBoard 접미사 추가
        document.title = `${title} | BackBoard`;
    }, [tab, chartConfig, activePlotType]);

    // config를 백그라운드에서 로딩 (3회 재시도). 실패해도 UI 오류는 표시하지 않음
    useEffect(() => {
        let cancelled = false;

        const sleep = (ms: number) => new Promise(resolve => setTimeout(resolve, ms));

        const loadConfigWithRetries = async (maxAttempts = 3) => {
            for (let attempt = 1; attempt <= maxAttempts; attempt++) {
                try {
                    const res = await fetch('/api/config');
                    const data = await res.json();

                    if (cancelled) {
                        return;
                    }

                    // 성공 시 상태 업데이트
                    setConfig(data);

                    // config 로딩 후 window.indicatorPaths 전역 공유
                    (window as any).indicatorPaths = {};
                    data['지표']?.forEach((indicator: any) => {
                        const indicatorName = indicator['지표 이름'] || 'unknown_indicator';
                        const indicatorPath = indicator['데이터 경로'] || '';
                        const plotType = indicator['플롯']?.['플롯 종류'];

                        if (plotType !== '비활성화' && indicatorPath) {
                            (window as any).indicatorPaths[indicatorName] = indicatorPath.replace(/\/[^\/]*$/, '');
                        }
                    });

                    return; // 성공하면 종료
                } catch (err) {
                    // 마지막 시도 전에는 경고만 남기고 잠시 대기 후 재시도
                    if (attempt < maxAttempts) {
                        // 간단한 지연(지수 백오프의 단순화)
                        // 첫 재시도는 200ms, 다음은 500ms
                        await sleep(attempt === 1 ? 200 : 500);

                        continue;
                    }

                    // 모든 시도 실패: 오류 표시를 내리지 않고 조용히 처리
                    if (cancelled) {
                        return;
                    }

                    setConfig(null);
                    return;
                }
            }
        };

        loadConfigWithRetries(3).then();

        return () => {
            cancelled = true;
        };
    }, []);

    // 초기 설정 (줌 방지 등)
    useEffect(() => {
        // 줌인/아웃 방지
        const preventZoom = (event: WheelEvent | KeyboardEvent) => {
            if (
                (event instanceof WheelEvent && event.ctrlKey) ||
                (event instanceof KeyboardEvent && event.ctrlKey &&
                    (event.key === '+' || event.key === '-' || event.key === '0'))
            ) {
                event.preventDefault();
            }
        };

        window.addEventListener('wheel', preventZoom, {passive: false});
        window.addEventListener('keydown', preventZoom);

        return () => {
            window.removeEventListener('wheel', preventZoom);
            window.removeEventListener('keydown', preventZoom);
        };
    }, []);

    // 탭의 CSS 클래스 결정 (탭 전환 애니메이션 용도)
    const getTabClass = (tabName: string) => {
        const baseClass = "tab-content";
        let resultClass;

        if (!isAnimating) {
            // 애니메이션 없을 때 현재 탭은 기본 클래스만
            resultClass = tabName === tab ? baseClass : `${baseClass} tab-content-hidden`; // 비활성 탭은 숨김 클래스 추가
        } else {
            // 애니메이션 중일 때
            if (tabName === tab) { // 새로 활성화될 탭
                resultClass = `${baseClass} tab-content-enter`;
            } else if (tabName === prevTab) { // 비활성화될 탭
                resultClass = `${baseClass} tab-content-exit`;
            } else {
                // 그 외 (애니메이션과 관련 없는 탭)
                resultClass = `${baseClass} tab-content-hidden`; // 기본적으로 숨김
            }
        }

        return resultClass;
    };

    // 탭 컨텐츠의 스타일 결정
    const getTabStyle = (tabName: string) => {
        const isCurrentTab = tabName === tab;
        const isAnimatingTab = isAnimating && (tabName === tab || tabName === prevTab);

        // 타입 미리 정의
        const overflowValue: "hidden" | "auto" = isAnimating ? "hidden" : (isCurrentTab ? "auto" : "hidden");
        const pointerEventsValue: "auto" | "none" = isCurrentTab || (isAnimating && tabName === tab) ? "auto" : "none";

        // 방문한 탭은 숨기되 언마운트하지 않음 - visibility와 opacity 사용
        const visibilityValue: "visible" | "hidden" = (isCurrentTab || isAnimatingTab) ? "visible" : "hidden";
        const opacityValue = (isCurrentTab || isAnimatingTab) ? 1 : 0;

        return {
            height: "100%",
            width: "100%",
            position: "absolute" as const,
            top: 0,
            left: 0,
            right: 0,
            bottom: 0,
            backgroundColor: tabName === "Performance" ? 'transparent' : undefined, // Performance 배경 투명 유지
            overflow: overflowValue,
            pointerEvents: pointerEventsValue, // 현재 탭만 인터랙션 허용
            display: visitedTabs[tabName] ? 'block' : 'none', // 방문한 탭은 DOM에 유지
            visibility: visibilityValue, // visibility로 시각적 표시 제어
            opacity: opacityValue, // 애니메이션 자연스럽게 처리
            transition: 'opacity 0.2s ease-out', // 전환 애니메이션
        };
    };

    return (
        <div className="flex h-full w-full overflow-hidden bg-[#111111]" style={{position: "relative"}}>
            {/* 이미지 로딩 중일 때 전체 화면 로딩 스피너 표시 */}
            {isGlobalLoading && <LoadingSpinner/>}

            {/* StarField 컴포넌트는 항상 렌더링하되, Chart 탭에서는 opacity로 숨김 처리 */}
            <div style={{
                position: 'absolute',
                width: '100%',
                height: '100%',
                top: 0,
                left: 0,
                opacity: tab === "Chart" ? 0 : 1,
                transition: 'opacity 0.3s ease-in-out',
                zIndex: -1,
                // 차트 탭에서 성능 최적화를 위해 포인터 이벤트와 visibility 제어
                pointerEvents: tab === "Chart" ? 'none' : 'auto',
                visibility: tab === "Chart" ? 'hidden' : 'visible',
            }}>
                <Suspense fallback={<div/>}>
                    <StarField/>
                </Suspense>
            </div>

            {/* 서버 오류 발생 시 경고 팝업 표시 */}
            {serverError && <ServerAlert serverError={serverError}/>}

            <Sidebar
                onSelectTab={handleSelectTab}
                activeTab={tab}
                config={config || {}}
                isChartLoading={isChartLoading}
                activeSymbol={tab === "Chart" ? chartConfig?.symbol : undefined}
                activePlotType={tab === "Plot" ? activePlotType : undefined}
                isAnimating={isAnimating}
                timeframe={chartConfig?.timeframe}
            />

            <main
                className="h-full overflow-hidden flex flex-col w-full main-content"
                style={{...getMainContentStyle(), zIndex: 1}}
            >
                {/* 애니메이션 컨테이너 - 항상 overflow hidden으로 설정하여 중첩 스크롤바 방지 */}
                <div className={`tab-container ${animationDirection}`}
                     style={{overflow: "hidden", height: "100%"}}>
                    {/* 각 탭 컨텐츠를 방문 상태에 기반하여 조건부 렌더링 */}

                    <div
                        className={getTabClass("StrategyEditor")}
                        style={getTabStyle("StrategyEditor")}
                        data-tab="StrategyEditor"
                    >
                        {/* StrategyEditor는 방문한 경우에만 렌더링, 이후에는 display로 제어 */}
                        {visitedTabs["StrategyEditor"] && (
                            <Suspense fallback={<div/>}>
                                <StrategyEditor/>
                            </Suspense>
                        )}
                    </div>

                    <div
                        className={getTabClass("Overview")}
                        style={getTabStyle("Overview")}
                        data-tab="Overview"
                    >
                        {/* Overview는 방문한 경우에만 렌더링, 이후에는 display로 제어 */}
                        {visitedTabs["Overview"] && (
                            <Suspense fallback={<div/>}>
                                <Overview/>
                            </Suspense>
                        )}
                    </div>

                    <div
                        className={getTabClass("Performance")}
                        style={getTabStyle("Performance")}
                        data-tab="Performance"
                    >
                        {/* Performance는 방문한 경우에만 렌더링, 이후에는 display로 제어 */}
                        {visitedTabs["Performance"] && (
                            <Suspense fallback={<div/>}>
                                <Performance config={config || {}}/>
                            </Suspense>
                        )}
                    </div>

                    <div
                        className={getTabClass("Plot")}
                        style={getTabStyle("Plot")}
                        data-tab="Plot"
                    >
                        {/* Plot 탭은 방문한 경우에만 렌더링, 이후에는 display로 제어 */}
                        {visitedTabs["Plot"] && (
                            <Suspense fallback={<div/>}>
                                <Plot plotType={activePlotType} config={config}/>
                            </Suspense>
                        )}
                    </div>

                    <div
                        className={getTabClass("Chart")}
                        style={getTabStyle("Chart")}
                        data-tab="Chart"
                    >
                        {/* Chart 탭은 방문한 경우에만 렌더링하고 chartConfig가 있을 때만, 이후에는 display로 제어 */}
                        {visitedTabs["Chart"] && chartConfig && (
                            <Suspense fallback={<div/>}>
                                <Chart
                                    key={chartConfig.symbol} // 심볼 변경 시 강제 리마운트
                                    symbol={chartConfig.symbol}
                                    timeframe={chartConfig.timeframe}
                                    priceStep={chartConfig.priceStep}
                                    pricePrecision={chartConfig.pricePrecision}
                                    config={config || {}} // config 추가
                                    onChartLoaded={handleChartLoaded}
                                />
                            </Suspense>
                        )}
                    </div>

                    <div
                        className={getTabClass("TradeList")}
                        style={getTabStyle("TradeList")}
                        data-tab="TradeList"
                    >
                        {/* TradeList 탭은 방문한 경우에만 렌더링, 이후에는 display로 제어 */}
                        {visitedTabs["TradeList"] && (
                            <Suspense fallback={<div/>}>
                                <TradeList config={config || {}}/>
                            </Suspense>
                        )}
                    </div>

                    <div
                        className={getTabClass("Config")}
                        style={getTabStyle("Config")}
                        data-tab="Config"
                    >
                        {/* Config 탭은 방문한 경우에만 렌더링, 이후에는 display로 제어 */}
                        {visitedTabs["Config"] && (
                            <Suspense fallback={<div/>}>
                                <Config config={config || {}}/>
                            </Suspense>
                        )}
                    </div>

                    <div
                        className={getTabClass("Log")}
                        style={getTabStyle("Log")}
                        data-tab="Log"
                    >
                        {/* Log 탭은 방문한 경우에만 렌더링, 이후에는 display로 제어 */}
                        {visitedTabs["Log"] && (
                            <Suspense fallback={<div/>}>
                                <Log isTextOptimizing={isLogTextOptimizing}/>
                            </Suspense>
                        )}
                    </div>
                </div>
            </main>
        </div>
    );
}

function App() {
    return (
        <WebSocketProvider>
            <TradeFilterProvider>
                <LogoProvider>
                    <AppContent/>
                </LogoProvider>
            </TradeFilterProvider>
        </WebSocketProvider>
    );
}

export default App;
