import React, {memo, useEffect, useRef, useState} from "react"
import 'react-datepicker/dist/react-datepicker.css'
import {Button} from "@/Components/UI/Button.tsx"
import {AnimatePresence, motion} from 'framer-motion'
import {useLogo} from "@/Contexts/LogoContext"
import FilterSection from "./FilterSection";
import './Sidebar.css';

// 탭 순서 정의 (애니메이션 방향 결정에 사용)
const tabOrder = ["StrategyEditor", "Overview", "Performance", "Plot", "Chart", "TradeList", "Config", "Log"];

// Plot 탭 내부 순서 정의
const plotTypeOrder = ["equity-drawdown", "profit-loss-comparison", "holding-time-pnl-distribution", "symbol-performance"];

// 사이드바 아이콘은 서버 API를 통해 가져옴
const getSidebarIconUrl = (name: string) => {
    return `/api/icon?name=${encodeURIComponent(name)}`;
};

// Sidebar 전용 아이콘 컴포넌트: 이미지 로드 실패 시 회색 라운드 박스로 대체
const SidebarIcon: React.FC<{ name: string; alt?: string; className?: string }> = ({name, alt, className}) => {
    const [failed, setFailed] = useState(false);
    const src = getSidebarIconUrl(name);

    if (failed) {
        return null;
    }

    return (
        <img
            src={src}
            alt={alt}
            className={className}
            onError={() => setFailed(true)}
            style={{width: '27px', height: '27px', verticalAlign: 'middle'}}
        />
    );
};

/**
 * 사이드바 컴포넌트 Props 인터페이스
 * @property onSelectTab - 탭 선택 시 호출될 콜백 함수, 탭 이름, 차트 설정, 애니메이션 방향을 받을 수 있음
 * @property activeTab - 현재 활성화된 탭 이름
 * @property config - 백테스팅 설정 및 심볼 정보를 포함한 설정 객체
 * @property isChartLoading - 차트 로딩 중 여부
 * @property activeSymbol - 현재 활성화된 차트 심볼
 * @property activePlotType - 활성화된 Plot 타입 추가
 * @property isAnimating - 탭 전환 애니메이션 중인지 여부
 * @property timeframe - 차트의 타임프레임
 */
interface SidebarProps {
    onSelectTab: (tab: string, configParam?: {
        symbol?: string,
        timeframe?: string,
        priceStep?: number,
        pricePrecision?: number,
        plotType?: string
    }, direction?: 'left' | 'right') => void
    activeTab: string
    config: any;
    isChartLoading?: boolean;
    activeSymbol?: string;
    activePlotType?: string; // 활성화된 Plot 타입 추가
    isAnimating?: boolean; // 탭 전환 애니메이션 중인지 여부 추가
    timeframe?: string;
}

// 로고 이미지 컴포넌트 - 로고 로딩 및 폴백 처리
const SymbolLogoImage = memo(({
                                  symbolName,
                                  isActive,
                                  style
                              }: {
    symbolName: string,
    isActive: boolean,
    style?: React.CSSProperties
}) => {
    const {getLogoUrl} = useLogo();
    const logoUrl = getLogoUrl(symbolName);

    return (
        <div
            className={`symbol-icon-wrapper ${isActive ? 'symbol-icon-wrapper-active' : ''}`}
            style={style}
        >
            <img
                className="symbol-icon-small"
                src={logoUrl}
                alt={symbolName}
                onError={(e) => {
                    // 이미지 로드 실패 시 fallback 이미지로 대체
                    (e.target as HTMLImageElement).src = "/Logos/USDT.png";
                }}
            />
        </div>
    );
});

/**
 * 사이드바 컴포넌트
 * 백테스팅 애플리케이션의 메인 네비게이션 사이드바로, 다양한 섹션으로 이동할 수 있는 버튼들 제공
 *
 * @param onSelectTab - 탭 선택 시 호출될 콜백 함수
 * @param activeTab - 현재 활성화된 탭 이름
 * @param config - 백테스팅 설정 및 심볼 정보를 포함한 설정 객체
 * @param isChartLoading - 차트 로딩 중 여부
 * @param activeSymbol - 현재 활성화된 차트 심볼
 * @param activePlotType - 활성화된 Plot 타입 props 추가
 */
export default function Sidebar({
                                    onSelectTab,
                                    activeTab,
                                    config,
                                    isChartLoading: externalChartLoading,
                                    activeSymbol,
                                    activePlotType, // 활성화된 Plot 타입 props 추가
                                    isAnimating,
                                    timeframe
                                }: SidebarProps) {
    const [chartExpanded, setChartExpanded] = useState(false);
    const [plotExpanded, setPlotExpanded] = useState(false); // Plot 확장 상태 추가
    const [internalChartLoading, setInternalChartLoading] = useState(false);
    const isChartLoading = externalChartLoading !== undefined ? externalChartLoading : internalChartLoading;
    const chartLoadingTimerRef = useRef<NodeJS.Timeout | null>(null);
    const [logosPreloaded, setLogosPreloaded] = useState(false);
    const symbolListRef = useRef<HTMLDivElement>(null);
    const {preloadLogos} = useLogo();
    const sidebarRef = useRef<HTMLDivElement>(null);

    // 이전 활성 탭 인덱스를 저장하기 위한 Ref
    const prevActiveTabIndexRef = useRef<number | null>(null);

    // 컴포넌트 마운트 시 및 activeTab 변경 시 이전 탭 인덱스 초기화/업데이트
    useEffect(() => {
        const initialIndex = tabOrder.indexOf(activeTab);
        prevActiveTabIndexRef.current = initialIndex !== -1 ? initialIndex : null;
    }, [activeTab]); // activeTab이 바뀔 때마다 Ref 업데이트

    // 다른 탭으로 이동 시 로딩 상태 즉시 해제
    useEffect(() => {
        if (activeTab !== "Chart" && externalChartLoading === undefined && internalChartLoading) {
            setInternalChartLoading(false);
            if (chartLoadingTimerRef.current) {
                clearTimeout(chartLoadingTimerRef.current);
                chartLoadingTimerRef.current = null;
            }
        }
    }, [activeTab, externalChartLoading, internalChartLoading]);

    // 컴포넌트 마운트 시 심볼 로고 프리로딩
    useEffect(() => {
        if (config && config["심볼"] && !logosPreloaded) {
            // 모든 심볼 이름 추출
            const symbolNames = config["심볼"].map((sym: any) => sym["심볼 이름"]);

            // 로고 프리로딩 함수 호출
            preloadLogos(symbolNames);

            // 프리로딩 완료 상태 업데이트
            setLogosPreloaded(true);
        }
    }, [config, preloadLogos, logosPreloaded]);

    // 차트 확장 상태가 변경될 때 심볼 로고 프리로딩
    useEffect(() => {
        if (chartExpanded && config && config["심볼"]) {
            // 차트 확장 시 심볼 로고 프리로딩 재시도
            const symbolNames = config["심볼"].map((sym: any) => sym["심볼 이름"]);
            preloadLogos(symbolNames);
        }
    }, [chartExpanded, config, preloadLogos]);

    /**
     * 탭 전환 시 로딩 상태 및 애니메이션 방향 관리 함수
     * 차트 탭으로 이동 시 로딩 애니메이션을 보여주고 일정 시간 후 로딩 상태 해제
     *
     * @param tab - 이동할 탭 이름
     * @param chartConfig - 차트 탭인 경우 필요한 차트 설정 객체
     * @param plotType - EquityCurve 탭인 경우 드로우다운 플롯 여부 결정
     */
    const handleTabChange = (tab: string, chartConfig?: any, plotType?: string) => {
        // 애니메이션 중에는 탭 전환 무시
        if (isAnimating) {
            return;
        }

        if (chartLoadingTimerRef.current) {
            clearTimeout(chartLoadingTimerRef.current);
            chartLoadingTimerRef.current = null;
        }

        // 애니메이션 방향 결정 로직을 완전히 새롭게 구성
        let direction: 'left' | 'right' | undefined = undefined;

        // 현재 활성 탭과 이동하려는 탭의 인덱스
        const currentTabIndex = tabOrder.indexOf(activeTab);
        const targetTabIndex = tabOrder.indexOf(tab);

        // 심볼 변경이 아닌 실제 탭 전환일 경우에만 방향 결정
        const isSymbolChange = tab === "Chart" && activeTab === "Chart" && chartConfig;
        const isPlotTypeChange = tab === "Plot" && activeTab === "Plot" && plotType; // Plot 타입 변경 체크

        if (isPlotTypeChange) {
            const currentPlotTypeIndex = plotTypeOrder.indexOf(activePlotType || '');
            const targetPlotTypeIndex = plotTypeOrder.indexOf(plotType);

            if (currentPlotTypeIndex !== -1 && targetPlotTypeIndex !== -1) {
                if (targetPlotTypeIndex > currentPlotTypeIndex) {
                    direction = 'right';
                } else if (targetPlotTypeIndex < currentPlotTypeIndex) {
                    direction = 'left';
                }
            }
        } else if (!isSymbolChange && !isPlotTypeChange && currentTabIndex !== -1 && targetTabIndex !== -1) {
            // 단순히 인덱스 비교로 방향 결정 (왼쪽->오른쪽 또는 오른쪽->왼쪽)
            if (targetTabIndex > currentTabIndex) {
                direction = 'right'; // 오른쪽으로 이동
            } else if (targetTabIndex < currentTabIndex) {
                direction = 'left'; // 왼쪽으로 이동
            }
        }

        // 차트 로딩 처리 및 onSelectTab 호출 로직
        if (tab === "Chart" && chartConfig) {
            // 외부 로딩 상태가 없는 경우에만 내부 로딩 상태 사용 (즉시 해제)
            if (externalChartLoading === undefined) {
                setInternalChartLoading(false);
            }
            // 방향 파라미터 전달
            onSelectTab(tab, chartConfig, direction);
        } else if (tab === "Plot" && plotType) {
            // Plot 타입을 전달 - 타입 오류 수정 (객체 형태로 전달하지 않고 undefined 전달)
            onSelectTab(tab, {plotType: plotType}, direction);
            // 여기서는 별도의 객체 없이 plotType 정보만 활용
        } else {
            // 다른 탭 처리
            if (externalChartLoading === undefined) {
                setInternalChartLoading(false);
            }
            // 방향 파라미터 전달
            onSelectTab(tab, undefined, direction);
        }

        // prevActiveTabIndexRef는 단순히 추적 용도로만 사용, 로직에는 직접 영향 없음
        if (targetTabIndex !== -1) {
            prevActiveTabIndexRef.current = targetTabIndex;
        }
    };

    /**
     * 거래 차트 버튼 클릭 핸들러
     * 선택한 심볼에 대한 차트를 로드하고 차트 탭으로 이동 (애니메이션 방향 포함)
     *
     * @param sym - 선택한 심볼 정보 객체
     */
    const handleChartSymbolClick = (sym: any) => {
        // 애니메이션 중이거나 Chart 탭에서 차트 로딩 중이면 클릭 무시
        if (isAnimating || (activeTab === "Chart" && isChartLoading)) return;

        const newSymbol = sym["심볼 이름"];
        // 같은 심볼 재클릭 방지
        if (activeTab === "Chart" && newSymbol === activeSymbol) {
            return;
        }

        // Chart 탭으로 이동하되, 심볼만 변경하는 경우는 방향 애니메이션 필요 없음
        handleTabChange("Chart", {
            symbol: newSymbol,
            timeframe: sym["트레이딩 바 데이터"]["타임프레임"],
            priceStep: sym["거래소 정보"]["가격 최소 단위"],
            pricePrecision: sym["거래소 정보"]["가격 소수점 정밀도"],
        });
    };

    /**
     * 거래 차트 섹션 토글 함수
     * 차트 섹션의 확장/축소 상태를 변경하고 확장 시 로고 프리로딩
     */
    const handleChartToggle = () => {
        // 애니메이션 중이면 토글 무시
        if (isAnimating) return;

        // 애니메이션 프레임과 동기화하여 상태 업데이트 (브라우저 렌더링 주기에 맞춤)
        requestAnimationFrame(() => {
            const newExpanded = !chartExpanded;
            setChartExpanded(newExpanded);

            // 차트 확장 시 심볼 로고 프리로딩
            if (newExpanded && config && config["심볼"]) {
                // 상태 업데이트 후 비동기적으로 로고 로딩 처리
                Promise.resolve().then(() => {
                    const symbolNames = config["심볼"].map((sym: any) => sym["심볼 이름"]);
                    preloadLogos(symbolNames);
                });
            }
        });
    };

    /**
     * 분석 그래프 섹션 토글 함수
     * 그래프 섹션의 확장/축소 상태를 변경
     */
    const handlePlotToggle = () => {
        // 애니메이션 중이면 토글 무시
        if (isAnimating) return;

        // 애니메이션 프레임과 동기화하여 상태 업데이트 (브라우저 렌더링 주기에 맞춤)
        requestAnimationFrame(() => {
            const newExpanded = !plotExpanded;
            setPlotExpanded(newExpanded);
        });
    };

    /**
     * 플롯 타입 클릭 핸들러
     * 선택한 플롯 타입에 대한 분석 그래프를 로드하고 분석 그래프 탭으로 이동
     *
     * @param plotType - 선택한 플롯 타입
     */
    const handlePlotTypeClick = (plotType: string) => {
        // 애니메이션 중이면 클릭 무시
        if (isAnimating) {
            return;
        }

        // 같은 플롯 타입 재클릭 방지
        if (activeTab === "Plot" && plotType === activePlotType) {
            return;
        }

        // Plot 탭으로 이동하되, 플롯 타입만 변경
        handleTabChange("Plot", undefined, plotType);
    };

    /**
     * 아이템 애니메이션 변형 정의
     * 버튼 등 UI 요소의 다양한 상태에 따른 애니메이션 효과 설정
     *
     * hidden: 초기 숨김 상태
     * visible: 보이는 상태 (지연 효과로 순차적 등장)
     * hover: 마우스 오버 시 효과
     * tap: 클릭 시 효과
     */
    const itemVariants = {
        hidden: {opacity: 0, x: -20},
        visible: (custom: { index: number, isActive: boolean }) => ({
            opacity: 1,
            x: 0,
            transition: {
                delay: 0.1 * custom.index,
                duration: 0.3,
            }
        }),
        hover: {
            borderColor: 'rgba(255, 215, 0, 0.7)',
            boxShadow: '0 0 9px rgba(255, 215, 0, 0.5)',
            scale: 1.03,
            transition: {duration: 0.2}
        },
        tap: (custom: { index: number, isActive: boolean }) => ({
            backgroundColor: 'rgba(52, 46, 14, 1)', // 누르고 있을 때 배경색을 52, 46, 14로 설정
            // 배경색 대신 inset 그림자로 틴트 효과 주기
            boxShadow: custom.isActive
                ? 'inset 0 0 0 1000px rgba(255, 215, 0, 0.2), 0 0 8px rgba(255, 215, 0, 0.3)'
                : 'inset 0 0 0 1000px rgba(255, 215, 0, 0.15), 0 0 8px rgba(255, 215, 0, 0.3)',
            scale: 0.98,
            transition: {duration: 0.1}
        })
    };

    /**
     * 심볼 개수에 따른 동적 지연 시간 계산 함수
     * 심볼이 많을수록 지연 시간을 짧게 조절
     */
    const getSymbolDelay = (index: number, totalSymbols: number) => {
        if (totalSymbols <= 5) return 0.1 * index; // 5개 이하면 0.1초
        if (totalSymbols <= 10) return 0.08 * index; // 10개 이하면 0.03초
        return 0.06 * index; // 10개 초과면 0.06초
    };

    return (
        <div
            ref={sidebarRef}
            className="sidebar-container custom-scrollbar"
        >
            {/* 사이드바 콘텐츠 영역 */}
            <div
                className="sidebar-content"
            >
                {/* 사이드바 상단 헤더 영역 - 로고 */}
                <div className="sidebar-header">
                    <motion.h2
                        initial={{opacity: 0}}
                        animate={{opacity: 1}}
                        transition={{delay: 0.2}}
                        className="sidebar-logo"
                    >
                        <span className="icon-placeholder"></span>
                        BackBoard
                    </motion.h2>
                </div>

                {/* 사이드바 내비게이션 버튼 영역 */}
                <div className="space-y-5 mt-2 flex flex-col">
                    {/* 전략 에디터 버튼 - 백테스팅 전략 편집 및 실행 */}
                    <motion.div
                        custom={{index: 0, isActive: activeTab === "StrategyEditor"}}
                        variants={itemVariants}
                        initial="hidden"
                        animate="visible"
                        whileHover="hover"
                        whileTap="tap"
                        className={`sidebar-button-container main-button-container ${activeTab === "StrategyEditor" ? "active-sidebar-button" : ""}`}
                    >
                        <Button
                            variant={activeTab === "StrategyEditor" ? "default" : "ghost"}
                            className={`w-full justify-start sidebar-button ${activeTab === "StrategyEditor" ? "active" : ""}`}
                            onClick={() => handleTabChange("StrategyEditor")}
                        >
                            <SidebarIcon name={'strategy_editor.ico'} alt="StrategyEditor" className="sidebar-icon"/>
                            <span className="ml-2 button-text">전략 에디터</span>
                        </Button>
                    </motion.div>

                    {/* 전체 요약 버튼 - 백테스팅 결과의 전체적인 요약 정보 제공 */}
                    <motion.div
                        custom={{index: 1, isActive: activeTab === "Overview"}}
                        variants={itemVariants}
                        initial="hidden"
                        animate="visible"
                        whileHover="hover"
                        whileTap="tap"
                        className={`sidebar-button-container main-button-container ${activeTab === "Overview" ? "active-sidebar-button" : ""}`}
                    >
                        <Button
                            variant={activeTab === "Overview" ? "default" : "ghost"}
                            className={`w-full justify-start sidebar-button ${activeTab === "Overview" ? "active" : ""}`}
                            onClick={() => handleTabChange("Overview")}
                        >
                            <SidebarIcon name={'overview.ico'} alt="Overview" className="sidebar-icon"/>
                            <span className="ml-2 button-text">전체 요약</span>
                        </Button>
                    </motion.div>

                    {/* 성과 지표 버튼 - 백테스팅 성능 메트릭 및 통계 지표 제공 */}
                    <motion.div
                        custom={{index: 2, isActive: activeTab === "Performance"}}
                        variants={itemVariants}
                        initial="hidden"
                        animate="visible"
                        whileHover="hover"
                        whileTap="tap"
                        className={`sidebar-button-container main-button-container ${activeTab === "Performance" ? "active-sidebar-button" : ""}`}
                    >
                        <Button
                            variant={activeTab === "Performance" ? "default" : "ghost"}
                            className={`w-full justify-start sidebar-button ${activeTab === "Performance" ? "active" : ""}`}
                            onClick={() => handleTabChange("Performance")}
                        >
                            <SidebarIcon name={'performance.ico'} alt="Performance" className="sidebar-icon"/>
                            <span className="ml-2 button-text">성과 지표</span>
                        </Button>
                    </motion.div>

                    {/* 분석 그래프 버튼 - 백테스팅 결과를 시각화한 그래프 모음 제공 */}
                    <motion.div
                        custom={{index: 3, isActive: activeTab === "Plot"}}
                        variants={itemVariants}
                        initial="hidden"
                        animate="visible"
                        whileHover="hover"
                        whileTap="tap"
                        className={`sidebar-button-container main-button-container ${activeTab === "Plot" || plotExpanded ? "active-sidebar-button" : ""} ${plotExpanded ? "plot-expanded-button" : ""}`}
                    >
                        <Button
                            variant={activeTab === "Plot" ? "default" : "ghost"}
                            className={`w-full justify-start sidebar-button ${activeTab === "Plot" || plotExpanded ? "active" : ""}`}
                            onClick={handlePlotToggle}
                        >
                            <SidebarIcon name={'plot.ico'} alt="Plot" className="sidebar-icon"/>
                            <span className="ml-2 button-text">분석 그래프</span>
                            <span
                                className={`expand-arrow ${plotExpanded ? "expand-arrow-expanded" : "expand-arrow-collapsed"}`}>
                                {plotExpanded ? "▼" : "▶"}
                            </span>
                        </Button>
                    </motion.div>

                    {/* 분석 그래프 유형 클릭 핸들러 */}
                    <AnimatePresence initial={false} mode="wait" onExitComplete={() => {
                    }}>
                        {plotExpanded && (
                            <motion.div
                                key="plot-type-list"
                                initial={{opacity: 0, height: 0, marginTop: 0, marginBottom: 0}}
                                animate={{
                                    opacity: 1,
                                    height: 'auto',
                                    marginTop: '6px',
                                    marginBottom: '-5px'
                                }}
                                exit={{
                                    opacity: 0,
                                    height: 0,
                                    marginTop: 0,
                                    marginBottom: 0,
                                    y: -15,
                                    transition: {
                                        opacity: {duration: 0.25},
                                        height: {duration: 0.4},
                                        marginTop: {duration: 0.35},
                                        marginBottom: {duration: 0.4},
                                        y: {duration: 0.4}
                                    }
                                }}
                                transition={{
                                    duration: 0.35,
                                    ease: [0.4, 0, 0.2, 1],
                                    height: {duration: 0.35},
                                    marginBottom: {duration: 0.35}
                                }}
                                className="sub-section-container"
                            >
                                {/* 자금 & 드로우다운 탭 */}
                                <div key="plot-equity-drawdown" className="sub-section-item">
                                    <motion.div
                                        custom={{
                                            index: 0,
                                            isActive: activeTab === "Plot" && activePlotType === "equity-drawdown"
                                        }}
                                        variants={itemVariants}
                                        initial="hidden"
                                        animate="visible"
                                        whileHover="hover"
                                        whileTap="tap"
                                        className={`symbol-button-container sub-button-container ${activeTab === "Plot" && activePlotType === "equity-drawdown" ? "active-symbol-button" : ""}`}
                                    >
                                        <Button
                                            variant="ghost"
                                            className={`w-full justify-center symbol-button ${activeTab === "Plot" && activePlotType === "equity-drawdown" ? "active" : ""}`}
                                            onClick={() => handlePlotTypeClick("equity-drawdown")}
                                        >
                                            <span className="sub-button-text">자금 & 드로우다운</span>
                                        </Button>
                                    </motion.div>
                                </div>

                                {/* 손익 비교 탭 추가 */}
                                <div key="plot-profit-loss" className="sub-section-item">
                                    <motion.div
                                        custom={{
                                            index: 1,
                                            isActive: activeTab === "Plot" && activePlotType === "profit-loss-comparison"
                                        }}
                                        variants={itemVariants}
                                        initial="hidden"
                                        animate="visible"
                                        whileHover="hover"
                                        whileTap="tap"
                                        className={`symbol-button-container sub-button-container ${activeTab === "Plot" && activePlotType === "profit-loss-comparison" ? "active-symbol-button" : ""}`}
                                    >
                                        <Button
                                            variant="ghost"
                                            className={`w-full justify-center symbol-button ${activeTab === "Plot" && activePlotType === "profit-loss-comparison" ? "active" : ""}`}
                                            onClick={() => handlePlotTypeClick("profit-loss-comparison")}
                                        >
                                            <span className="sub-button-text">시간별 순손익 비교</span>
                                        </Button>
                                    </motion.div>
                                </div>

                                {/* 보유 시간별 순손익 분포 탭 */}
                                <div key="plot-holding-time-pnl" className="sub-section-item">
                                    <motion.div
                                        custom={{
                                            index: 2,
                                            isActive: activeTab === "Plot" && activePlotType === "holding-time-pnl-distribution"
                                        }}
                                        variants={itemVariants}
                                        initial="hidden"
                                        animate="visible"
                                        whileHover="hover"
                                        whileTap="tap"
                                        className={`symbol-button-container sub-button-container ${activeTab === "Plot" && activePlotType === "holding-time-pnl-distribution" ? "active-symbol-button" : ""}`}
                                    >
                                        <Button
                                            variant="ghost"
                                            className={`w-full justify-center symbol-button ${activeTab === "Plot" && activePlotType === "holding-time-pnl-distribution" ? "active" : ""}`}
                                            onClick={() => handlePlotTypeClick("holding-time-pnl-distribution")}
                                        >
                                            <span className="sub-button-text">보유 시간 순손익 분포</span>
                                        </Button>
                                    </motion.div>
                                </div>

                                {/* 심볼별 성과 추이 탭 */}
                                <div key="plot-symbol-performance" className="sub-section-item">
                                    <motion.div
                                        custom={{
                                            index: 3,
                                            isActive: activeTab === "Plot" && activePlotType === "symbol-performance"
                                        }}
                                        variants={itemVariants}
                                        initial="hidden"
                                        animate="visible"
                                        whileHover="hover"
                                        whileTap="tap"
                                        className={`symbol-button-container sub-button-container ${activeTab === "Plot" && activePlotType === "symbol-performance" ? "active-symbol-button" : ""}`}
                                    >
                                        <Button
                                            variant="ghost"
                                            className={`w-full justify-center symbol-button ${activeTab === "Plot" && activePlotType === "symbol-performance" ? "active" : ""}`}
                                            onClick={() => handlePlotTypeClick("symbol-performance")}
                                        >
                                            <span className="sub-button-text">심볼별 성과 추이</span>
                                        </Button>
                                    </motion.div>
                                </div>
                            </motion.div>
                        )}
                    </AnimatePresence>

                    {/* 거래 차트 버튼 - 개별 심볼의 가격 차트와 거래 포인트 표시 */}
                    <motion.div
                        custom={{index: 4, isActive: activeTab === "Chart"}}
                        variants={itemVariants}
                        initial="hidden"
                        animate="visible"
                        whileHover="hover"
                        whileTap="tap"
                        className={`sidebar-button-container main-button-container ${activeTab === "Chart" || chartExpanded ? "active-sidebar-button" : ""} ${chartExpanded ? "plot-expanded-button" : ""}`}
                    >
                        <Button
                            variant={activeTab === "Chart" ? "default" : "ghost"}
                            className={`w-full justify-start sidebar-button ${activeTab === "Chart" || chartExpanded ? "active" : ""}`}
                            onClick={handleChartToggle}
                        >
                            <SidebarIcon name={'chart.ico'} alt="Chart" className="sidebar-icon"/>
                            <span className="ml-2 button-text">거래 차트</span>
                            <span
                                className={`expand-arrow ${chartExpanded ? "expand-arrow-expanded" : "expand-arrow-collapsed"}`}>
                                {chartExpanded ? "▼" : "▶"}
                            </span>
                        </Button>
                    </motion.div>

                    {/* AnimatePresence와 심볼 리스트 */}
                    <AnimatePresence initial={false} mode="wait" onExitComplete={() => {
                    }}>
                        {chartExpanded && config && config["심볼"] && (
                            <motion.div
                                ref={symbolListRef}
                                key="chart-symbol-list"
                                initial={{opacity: 0, height: 0, marginTop: 0, marginBottom: 0}}
                                animate={{
                                    opacity: 1,
                                    height: 'auto',
                                    marginTop: '6px',
                                    marginBottom: '-5px'
                                }}
                                exit={{
                                    opacity: 0,
                                    height: 0,
                                    marginTop: 0,
                                    marginBottom: 0,
                                    y: -15,
                                    transition: {
                                        opacity: {duration: 0.25},
                                        height: {duration: 0.4},
                                        marginTop: {duration: 0.35},
                                        marginBottom: {duration: 0.4},
                                        y: {duration: 0.4}
                                    }
                                }}
                                transition={{
                                    duration: 0.35,
                                    ease: [0.4, 0, 0.2, 1],
                                    height: {duration: 0.35},
                                    marginBottom: {duration: 0.35}
                                }}
                                className="sub-section-container"
                            >
                                {config["심볼"].map((sym: any, index: number) => {
                                    const symbolName = sym["심볼 이름"];
                                    const isCurrentSymbolActive = activeTab === "Chart" && symbolName === activeSymbol;
                                    const totalSymbols = config["심볼"].length;

                                    return (
                                        <div key={`symbol-wrapper-${index}`} className="sub-section-item">
                                            <motion.div
                                                key={index}
                                                custom={{index: index, isActive: isCurrentSymbolActive}}
                                                initial={{opacity: 0, x: -15}}
                                                animate={{
                                                    opacity: 1,
                                                    x: 0,
                                                    transition: {
                                                        delay: getSymbolDelay(index, totalSymbols),
                                                        duration: 0.2,
                                                    }
                                                }}
                                                whileHover={{
                                                    borderColor: 'rgba(255, 215, 0, 0.7)',
                                                    boxShadow: '0 0 9px rgba(255, 215, 0, 0.5)',
                                                    scale: 1.03,
                                                    transition: {duration: 0.2}
                                                }}
                                                whileTap={{
                                                    backgroundColor: 'rgba(52, 46, 14, 1)',
                                                    boxShadow: isCurrentSymbolActive
                                                        ? 'inset 0 0 0 1000px rgba(255, 215, 0, 0.2), 0 0 8px rgba(255, 215, 0, 0.3)'
                                                        : 'inset 0 0 0 1000px rgba(255, 215, 0, 0.15), 0 0 8px rgba(255, 215, 0, 0.3)',
                                                    scale: 0.98,
                                                    transition: {duration: 0.1}
                                                }}
                                                className={`symbol-button-container sub-button-container ${isCurrentSymbolActive ? "active-symbol-button" : ""}`}
                                            >
                                                <Button
                                                    variant="ghost"
                                                    disabled={activeTab === "Chart" && isChartLoading}
                                                    className={`w-full justify-start symbol-button ${isCurrentSymbolActive ? "active" : ""} ${activeTab === "Chart" && isChartLoading ? "chart-loading-disabled cursor-not-allowed" : ""}`}
                                                    onClick={() => handleChartSymbolClick(sym)}
                                                >
                                                    <SymbolLogoImage
                                                        symbolName={symbolName}
                                                        isActive={isCurrentSymbolActive}
                                                    />
                                                    <span
                                                        className="sub-button-text symbol-text-limited">{symbolName}</span>
                                                </Button>
                                            </motion.div>
                                        </div>
                                    );
                                })}
                            </motion.div>
                        )}
                    </AnimatePresence>

                    {/* --- 하단 탭 그룹 --- */}
                    <div
                        className="flex flex-col space-y-5"
                    >
                        {/* 거래 내역 버튼 */}
                        <motion.div
                            custom={{index: 5, isActive: activeTab === "TradeList"}}
                            variants={itemVariants}
                            initial="hidden"
                            animate="visible"
                            whileHover="hover"
                            whileTap="tap"
                            className={`sidebar-button-container main-button-container ${activeTab === "TradeList" ? "active-sidebar-button" : ""}`}
                        >
                            <Button
                                variant={activeTab === "TradeList" ? "default" : "ghost"}
                                className={`w-full justify-start sidebar-button ${activeTab === "TradeList" ? "active" : ""}`}
                                onClick={() => handleTabChange("TradeList")}
                            >
                                <SidebarIcon name={'trade_list.ico'} alt="Trade List" className="sidebar-icon"/>
                                <span className="ml-2 button-text">거래 내역</span>
                            </Button>
                        </motion.div>

                        {/* 거래 필터 섹션 */}
                        <FilterSection
                            timeframe={timeframe}
                        />

                        {/* 백테스팅 설정 버튼 */}
                        <motion.div
                            custom={{index: 6, isActive: activeTab === "Config"}}
                            variants={itemVariants}
                            initial="hidden"
                            animate="visible"
                            whileHover="hover"
                            whileTap="tap"
                            className={`sidebar-button-container main-button-container ${activeTab === "Config" ? "active-sidebar-button" : ""}`}
                        >
                            <Button
                                variant={activeTab === "Config" ? "default" : "ghost"}
                                className={`w-full justify-start sidebar-button ${activeTab === "Config" ? "active" : ""}`}
                                onClick={() => handleTabChange("Config")}
                            >
                                <SidebarIcon name={'config.ico'} alt="Config" className="sidebar-icon"/>
                                <span className="ml-2 button-text">백테스팅 설정</span>
                            </Button>
                        </motion.div>

                        {/* 백테스팅 로그 버튼 */}
                        <motion.div
                            custom={{index: 7, isActive: activeTab === "Log"}}
                            variants={itemVariants}
                            initial="hidden"
                            animate="visible"
                            whileHover="hover"
                            whileTap="tap"
                            className={`sidebar-button-container main-button-container last-button ${activeTab === "Log" ? "active-sidebar-button" : ""}`}
                        >
                            <Button
                                variant={activeTab === "Log" ? "default" : "ghost"}
                                className={`w-full justify-start sidebar-button ${activeTab === "Log" ? "active" : ""}`}
                                onClick={() => handleTabChange("Log")}
                            >
                                <SidebarIcon name={'log.ico'} alt="Log" className="sidebar-icon"/>
                                <span className="ml-2 button-text">백테스팅 로그</span>
                            </Button>
                        </motion.div>

                        {/* 백테스팅 로그 버튼 밑 공간 */}
                        <div style={{height: '50px'}}></div>
                    </div>
                    {/* --- 하단 탭 그룹 끝 --- */}
                </div>
            </div>
        </div>
    )
}

export {SidebarIcon}
