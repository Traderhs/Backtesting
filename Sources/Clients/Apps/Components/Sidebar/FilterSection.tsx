import {memo, useCallback, useEffect, useRef, useState} from "react"
import {AnimatePresence, motion} from 'framer-motion'
import {Button} from "@/Components/UI/Button.tsx"
import {SidebarIcon} from "@/Components/Sidebar/Sidebar.tsx"
import {
    AdvancedEntryTimeFilter,
    AdvancedExitTimeFilter,
    EntryDirectionFilter,
    EntryNameFilter,
    EntryTimeFilter,
    ExitNameFilter,
    ExitTimeFilter,
    HoldingTimeFilter,
    NumericFilters,
    RecalculateBalanceCheckbox,
    ResetAllFiltersButton,
    StrategyFilter,
    SymbolFilter,
    useTradeFilter
} from "../TradeFilter";
import './FilterSection.css';

// 중립 상태에서 체크 해제될 때 애니메이션을 적용하는 함수 추가
const applyShakeAnimationWhenUnchecked = () => {
    // 체크박스 상태 변경 핸들러
    const handleCheckboxChange = (event: Event) => {
        const checkbox = event.target as HTMLInputElement;
        const previousState = checkbox.getAttribute('data-previous-state');
        const currentState = checkbox.indeterminate ? 'indeterminate' : checkbox.checked ? 'checked' : 'unchecked';

        // 중립 상태에서 체크 해제 상태로 변경된 경우
        if (previousState === 'indeterminate' && currentState === 'unchecked') {
            // 애니메이션 재적용을 위해 클래스 제거 후 다시 추가
            checkbox.classList.remove('shake-animation');
            // 강제 리플로우를 위한 임시 값 읽기
            void checkbox.offsetWidth;
            // 애니메이션 클래스 추가
            checkbox.classList.add('shake-animation');
        }

        // 현재 상태 저장
        checkbox.setAttribute('data-previous-state', currentState);
    };

    // 이벤트 리스너가 추가된 체크박스 추적
    const registeredCheckboxes = new Set<HTMLInputElement>();

    // 모든 체크박스에 대한 이벤트 리스너 등록을 위한 함수
    const setupCheckboxListeners = () => {
        // 모든 체크박스 선택 (커스텀 체크박스 클래스 사용)
        const checkboxes = document.querySelectorAll('.custom-checkbox');

        // 각 체크박스에 변경 이벤트 리스너 추가
        checkboxes.forEach(checkbox => {
            if (checkbox instanceof HTMLInputElement && !registeredCheckboxes.has(checkbox)) {
                // 체크박스의 이전 상태를 추적하기 위한 속성 추가
                checkbox.setAttribute('data-previous-state', checkbox.indeterminate ? 'indeterminate' : checkbox.checked ? 'checked' : 'unchecked');

                // 새 리스너 추가
                checkbox.addEventListener('change', handleCheckboxChange);

                // 등록된 체크박스 추적
                registeredCheckboxes.add(checkbox);
            }
        });
    };

    // DOM이 준비되면 체크박스 리스너 설정
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', setupCheckboxListeners);
    } else {
        setupCheckboxListeners();
    }

    // MutationObserver를 사용하여 동적으로 추가되는 체크박스에도 리스너 설정
    const observer = new MutationObserver((mutations) => {
        mutations.forEach(mutation => {
            if (mutation.type === 'childList' && mutation.addedNodes.length > 0) {
                setupCheckboxListeners();
            }
        });
    });

    // 옵저버 시작
    observer.observe(document.body, {childList: true, subtree: true});

    // 필요한 CSS 스타일 추가
    const addShakeAnimationStyle = () => {
        // 이미 스타일이 있는지 확인
        if (document.getElementById('shake-animation-style')) return;

        const style = document.createElement('style');
        style.id = 'shake-animation-style';
        style.textContent = `
            .shake-animation {
                animation: shake-checkbox 0.4s ease-in-out;
            }
        `;
        document.head.appendChild(style);
    };

    addShakeAnimationStyle();

    // 정리 함수와 함께 옵저버 참조 반환
    return {
        observer,
        cleanup: () => {
            observer.disconnect();

            // 모든 등록된 체크박스에서 이벤트 리스너 제거
            registeredCheckboxes.forEach(checkbox => {
                checkbox.removeEventListener('change', handleCheckboxChange);
            });

            // DOM 로드 이벤트 리스너 제거
            document.removeEventListener('DOMContentLoaded', setupCheckboxListeners);
        }
    };
};

interface FilterSectionProps {
    timeframe?: string;
}

// debounce 함수를 컴포넌트 외부로 이동하여 재생성 방지
const debounce = <T extends (...args: any[]) => any>(func: T, wait: number): (...args: Parameters<T>) => void => {
    let timeout: ReturnType<typeof setTimeout> | null = null;
    return (...args: Parameters<T>): void => {
        const later = () => {
            timeout = null;
            func(...args);
        };
        if (timeout !== null) {
            clearTimeout(timeout);
        }
        timeout = setTimeout(later, wait);
    };
};

// throttle 함수 추가
const throttle = <T extends (...args: any[]) => any>(func: T, limit: number): (...args: Parameters<T>) => void => {
    let inThrottle: boolean;
    return (...args: Parameters<T>): void => {
        if (!inThrottle) {
            func(...args);
            inThrottle = true;
            setTimeout(() => inThrottle = false, limit);
        }
    };
};

/**
 * 거래 필터 UI 컴포넌트 - 거래 필터 토글 버튼과 확장 시 표시되는 필터 옵션들을 포함
 * 독립적인 컴포넌트로 분리하여 상태 변경이 부모 컴포넌트에 영향을 주지 않게 함
 */
const FilterSection = memo(({timeframe}: FilterSectionProps) => {
    // 각 탭 컨텐츠에 대한 refs
    const basicTabRef = useRef<HTMLDivElement>(null);
    const timeTabRef = useRef<HTMLDivElement>(null);
    const numericTabRef = useRef<HTMLDivElement>(null);

    // 애니메이션 프레임 ID 추적
    const animationFrameId = useRef<number | null>(null);

    // TradeFilter context - 컴포넌트 최상위에서 호출
    const {filterExpanded, setFilterExpanded, options, allTrades} = useTradeFilter();

    // 현재 활성화된 탭 상태 관리
    const [activeTab, setActiveTab] = useState<string>("basic");

    // 이전 활성화 탭 (애니메이션 용도)
    const [prevTab, setPrevTab] = useState<string>("basic");

    // 애니메이션 방향
    const [animationDirection, setAnimationDirection] = useState<string>("");

    // 애니메이션 상태
    const [isAnimating, setIsAnimating] = useState<boolean>(false);

    // 애니메이션 타이머 관리
    const animationTimeoutRef = useRef<ReturnType<typeof setTimeout> | null>(null);

    // 중립 상태에서 체크 해제될 때 애니메이션 적용
    useEffect(() => {
        // 옵저버 참조 저장
        const {cleanup} = applyShakeAnimationWhenUnchecked();

        // 컴포넌트 언마운트 시 정리
        return () => {
            // 옵저버 해제
            cleanup();

            // 애니메이션 프레임 정리
            if (animationFrameId.current) {
                cancelAnimationFrame(animationFrameId.current);
            }

            // 애니메이션 타이머 정리
            if (animationTimeoutRef.current) {
                clearTimeout(animationTimeoutRef.current);
            }
        };
    }, []);

    // 세로선 높이 계산 함수 - DOM 렌더링 전 사전 계산 (데이터 기반)
    const updateVerticalLineHeights = useCallback(throttle((filterOptions: typeof options) => {
        // 실제 측정으로 확정된 정확한 수치
        const CHECKBOX_HEIGHT = 29;    // padding 포함된 실제 높이
        const MARGIN_BOTTOM = 6;       // 체크박스 margin-bottom
        const VERTICAL_MARGIN_TOP = 4; // 세로선 margin-top
        const HEADER_MARGIN_BOTTOM = 8; // 헤더 margin-bottom
        const ITEM_HEIGHT = CHECKBOX_HEIGHT + MARGIN_BOTTOM; // 35px

        // 각 섹션별 아이템 개수와 CSS 변수 매핑
        const sectionData = {
            'strategy-section': {
                count: filterOptions.strategies?.length || 1,
                selector: '.filter-section.strategy-section'
            },
            'symbol-section': {
                count: filterOptions.symbols?.length || 1,
                selector: '.filter-section.symbol-section'
            },
            'entry-name-section': {
                count: filterOptions.entryNames?.length || 1,
                selector: '.filter-section.entry-name-section'
            },
            'exit-name-section': {
                count: filterOptions.exitNames?.length || 1,
                selector: '.filter-section.exit-name-section'
            },
            'direction-section': {
                count: 2, // 매수/매도 고정
                selector: '.filter-section.direction-section'
            }
        };

        // 각 섹션별로 높이 계산 및 적용
        Object.entries(sectionData).forEach(([_sectionName, data]) => {
            // 최종 공식: (개수-1) × 35px + 14.5px - 4px + 8px
            const lineHeight = data.count > 0
                ? Math.max(0, (data.count - 1) * ITEM_HEIGHT + (CHECKBOX_HEIGHT / 2) - VERTICAL_MARGIN_TOP + HEADER_MARGIN_BOTTOM)
                : 0;

            // 각 섹션에 개별 CSS 변수 설정
            const section = document.querySelector(data.selector);
            if (section) {
                (section as HTMLElement).style.setProperty('--vertical-line-height', `${lineHeight}px`);
            }
        });

    }, 16), []);

    // 세로선 높이 동적 계산을 위한 useEffect 최적화
    useEffect(() => {
        // 디바운스된 업데이트 함수
        const debouncedUpdate = debounce(() => updateVerticalLineHeights(options), 100);

        // 탭 전환 애니메이션 완료 후 계산
        const timeoutId = setTimeout(debouncedUpdate, isAnimating ? 300 : 50);

        return () => {
            clearTimeout(timeoutId);
            if (animationFrameId.current) {
                cancelAnimationFrame(animationFrameId.current);
            }
        };
    }, [filterExpanded, activeTab, isAnimating, options, updateVerticalLineHeights]);

    // 필터 확장 상태 토글 함수 최적화
    const onToggleFilter = useCallback(() => {
        // 이미 애니메이션 중이면 무시
        if (isAnimating) return;

        // requestAnimationFrame을 사용하여 다음 프레임에서 상태 변경
        requestAnimationFrame(() => {
            setFilterExpanded(prev => !prev);
        });
    }, [setFilterExpanded, isAnimating]);

    // 탭 변경 처리 함수 최적화
    const handleTabChange = useCallback((newTab: string) => {
        if (newTab === activeTab) return;

        // 기존 애니메이션 타이머가 있으면 정리
        if (animationTimeoutRef.current) {
            clearTimeout(animationTimeoutRef.current);
            animationTimeoutRef.current = null;
        }

        // 애니메이션 방향 결정
        const tabOrder = ["basic", "time", "numeric"];
        const prevIndex = tabOrder.indexOf(activeTab);
        const newIndex = tabOrder.indexOf(newTab);
        const direction = prevIndex < newIndex ? "slide-in-right" : "slide-in-left";

        // requestAnimationFrame으로 상태 업데이트 최적화
        requestAnimationFrame(() => {
            // 애니메이션 시작
            setIsAnimating(true);
            setAnimationDirection(direction);
            setPrevTab(activeTab);
            setActiveTab(newTab);

            // 애니메이션 종료 후 상태 정리
            animationTimeoutRef.current = setTimeout(() => {
                requestAnimationFrame(() => {
                    setIsAnimating(false);
                    setAnimationDirection("");
                });
            }, 500);
        });
    }, [activeTab]);

    // 탭 컨텐츠 클래스 결정 함수 최적화
    const getTabContentClass = useCallback((tabName: string) => {
        const baseClass = "tab-content";

        if (!isAnimating) {
            return tabName === activeTab ? baseClass : `${baseClass} tab-content-hidden`;
        } else {
            if (tabName === activeTab) {
                return `${baseClass} tab-content-enter`;
            } else if (tabName === prevTab) {
                return `${baseClass} tab-content-exit`;
            } else {
                return `${baseClass} tab-content-hidden`;
            }
        }
    }, [isAnimating, activeTab, prevTab]);

    // 거래가 없는 경우 구분선을 숨기는 클래스 결정
    const getFilterSectionClass = (baseClasses: string) => {
        // 거래가 없는 경우 (거래 번호 0번만 있는 경우)
        if (allTrades.length <= 1) {
            return `${baseClasses} no-border`;
        }
        return baseClasses;
    };

    return (
        <div className="filter-section-container">
            {/* 거래 필터 버튼 - 거래 내역 필터링을 위한 옵션 제공 */}
            <motion.div
                custom={5}
                variants={{
                    hidden: {opacity: 0, x: -20},
                    visible: (i: number) => ({
                        opacity: 1,
                        x: 0,
                        transition: {
                            delay: 0.1 * i,
                            duration: 0.3,
                        }
                    }),
                    hover: {
                        scale: 1.03,
                        borderColor: 'rgba(255, 215, 0, 0.7)',
                        boxShadow: '0 0 9px rgba(255, 215, 0, 0.5)',
                        transition: {duration: 0.2}
                    },
                    tap: {
                        scale: 0.98,
                        backgroundColor: 'rgba(255, 215, 0, 0.15)',
                        boxShadow: '0 0 5px rgba(255, 215, 0, 0.3), inset 0 0 0 1.5px rgba(255, 255, 255, 0.6)',
                        transition: {duration: 0.1}
                    }
                }}
                initial="hidden"
                animate="visible"
                whileHover="hover"
                whileTap="tap"
                className={`sidebar-button-container filter-toggle-container ${filterExpanded ? "active-sidebar-button" : ""}`}
            >
                <Button
                    variant={filterExpanded ? "default" : "ghost"}
                    className={`w-full justify-start sidebar-button ${filterExpanded ? "active" : ""}`}
                    onClick={onToggleFilter}
                >
                    <div className="flex items-center justify-between w-full">
                        <div className="flex items-center">
                            <SidebarIcon name={'filter.ico'} alt="Filter" className="sidebar-icon"/>
                            <span className="ml-2 filter-toggle-text">거래 필터</span>
                        </div>
                        <span className={`filter-toggle-arrow ${filterExpanded ? "expanded" : ""}`}>
                            {filterExpanded ? "▼" : "▶"}
                        </span>
                    </div>
                </Button>
            </motion.div>

            {/* 필터 옵션 영역 - 필터 버튼 확장 시 표시되는 다양한 필터 컴포넌트 */}
            <AnimatePresence>
                {filterExpanded && (
                    <motion.div
                        initial={{opacity: 0, height: 0, marginTop: 0, marginBottom: 0}}
                        animate={{
                            opacity: 1,
                            height: 'auto',
                            marginTop: '12px',
                            marginBottom: '0px'
                        }}
                        exit={{
                            opacity: 0,
                            height: 0,
                            marginTop: 0,
                            marginBottom: 0
                        }}
                        transition={{
                            duration: 0.5,
                            ease: [0.4, 0, 0.2, 1],
                            height: {duration: 0.5},
                            marginBottom: {duration: 0.5},
                            marginTop: {duration: 0.5}
                        }}
                        className="flex flex-col space-y-2 relative z-50 filter-panel filter-panel-container"
                        style={{
                            pointerEvents: 'auto',
                            overflow: 'visible'
                        }}
                    >
                        {/* 자금 재계산 체크박스 - 거래 필터 변경 시 잔고 재계산 여부 설정 */}
                        <div className="filter-panel-content">
                            <RecalculateBalanceCheckbox/>
                        </div>

                        {/* 필터 초기화 버튼 */}
                        <div className="filter-panel-content">
                            <ResetAllFiltersButton/>
                        </div>

                        {/* 탭 인터페이스 */}
                        <div className="filter-panel-content">
                            {/* 탭 헤더 */}
                            <div className="tabs-header mb-3 tabs-header-container">
                                <div className="tabs-button-row">
                                    {['basic', 'time', 'numeric'].map((tab, index) => (
                                        <motion.div
                                            key={tab}
                                            className={`sidebar-button-container tab-button-wrapper ${
                                                index === 0 ? 'first' : index === 2 ? 'last' : 'middle'
                                            } ${activeTab === tab ? 'active-sidebar-button' : ''}`}
                                            whileHover={{scale: 1.03}}
                                            whileTap={{scale: 0.98}}
                                        >
                                            <Button
                                                variant="ghost"
                                                onClick={() => handleTabChange(tab)}
                                                className={`justify-center tab-filter-button ${activeTab === tab ? 'active' : ''}`}
                                            >
                                                {tab === 'basic' ? '기본\n필터' : tab === 'time' ? '시간\n필터' : '수치\n필터'}
                                            </Button>
                                        </motion.div>
                                    ))}
                                </div>
                            </div>

                            {/* 탭 컨텐츠 컨테이너 */}
                            <div className={`tab-container tab-container-wrapper ${animationDirection}`}>
                                {/* 기본 필터 탭 */}
                                <div
                                    ref={basicTabRef}
                                    className={`${getTabContentClass("basic")} tab-content-basic`}
                                    style={{
                                        display: activeTab === "basic" || prevTab === "basic" ? 'block' : 'none',
                                        position: isAnimating && prevTab === "basic" ? 'absolute' :
                                            activeTab === "basic" ? 'relative' : 'absolute',
                                        top: 0,
                                        left: 0,
                                        width: '100%'
                                    }}
                                >
                                    <div>
                                        {/* 전략 필터 */}
                                        <div className="filter-section strategy-section">
                                            <StrategyFilter/>
                                        </div>

                                        {/* 심볼 필터 */}
                                        <div className={getFilterSectionClass("filter-section symbol-section")}>
                                            <SymbolFilter/>
                                        </div>

                                        {/* 진입 이름 필터 */}
                                        <div className={getFilterSectionClass("filter-section entry-name-section")}>
                                            <EntryNameFilter/>
                                        </div>

                                        {/* 청산 이름 필터 */}
                                        <div className={getFilterSectionClass("filter-section exit-name-section")}>
                                            <ExitNameFilter/>
                                        </div>

                                        {/* 진입 방향 필터 */}
                                        <div className="filter-section direction-section">
                                            <EntryDirectionFilter/>
                                        </div>
                                    </div>
                                </div>

                                {/* 시간 필터 탭 */}
                                <div
                                    ref={timeTabRef}
                                    className={`${getTabContentClass("time")} tab-content-time`}
                                    style={{
                                        display: activeTab === "time" || prevTab === "time" ? 'block' : 'none',
                                        position: isAnimating && prevTab === "time" ? 'absolute' :
                                            activeTab === "time" ? 'relative' : 'absolute',
                                        top: 0,
                                        left: 0,
                                        width: '100%'
                                    }}
                                >
                                    <div>
                                        {/* 진입 시간 필터 섹션 */}
                                        <div className={getFilterSectionClass("filter-section")}>
                                            <EntryTimeFilter timeframe={timeframe}/>
                                            <AdvancedEntryTimeFilter/>
                                        </div>

                                        {/* 청산 시간 필터 섹션 */}
                                        <div className={getFilterSectionClass("filter-section")}>
                                            <ExitTimeFilter timeframe={timeframe}/>
                                            <AdvancedExitTimeFilter/>
                                        </div>

                                        {/* 보유 시간 필터 */}
                                        <div className={getFilterSectionClass("filter-section")}>
                                            <HoldingTimeFilter/>
                                        </div>
                                    </div>
                                </div>

                                {/* 수치 필터 탭 */}
                                <div
                                    ref={numericTabRef}
                                    className={`${getTabContentClass("numeric")} tab-content-numeric`}
                                    style={{
                                        display: activeTab === "numeric" || prevTab === "numeric" ? 'block' : 'none',
                                        position: isAnimating && prevTab === "numeric" ? 'absolute' :
                                            activeTab === "numeric" ? 'relative' : 'absolute',
                                        top: 0,
                                        left: 0,
                                        width: '100%'
                                    }}
                                >
                                    <div className="filter-section">
                                        <NumericFilters/>
                                    </div>
                                </div>
                            </div>
                        </div>
                    </motion.div>
                )}
            </AnimatePresence>
        </div>
    );
});

export default FilterSection;
