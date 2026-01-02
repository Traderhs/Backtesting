import {memo, useCallback, useEffect, useRef, useState} from "react"
import {AnimatePresence, motion} from "framer-motion"
import {Box, FormControl, InputLabel, MenuItem, Select, SelectChangeEvent} from '@mui/material';
import {useLogo} from "@/Contexts/LogoContext";
import "./SymbolCard.css";

interface BarData {
    period: {
        start: string
        end: string
    }
    timeframe: string
    count: number
    missing: {
        count: number
        times: string[]
    }
    path: string
}

interface ExchangeInfo {
    dataPath: string
    priceStep: number
    pricePrecision: number
    qtyStep: number
    qtyPrecision: number
    maxMarketOrderQty: number
    minMarketOrderQty: number
    maxOrderQty: number
    minOrderQty: number
    minNotional: number
    liquidationFee: number
}

interface LeverageBracket {
    bracketNum: number
    minNotional: number
    maxNotional: number
    maxLeverage: number
    maintMarginRatio: number
    maintAmount: number
}

interface LeverageBrackets {
    dataPath: string
    brackets: LeverageBracket[]
}

interface FundingRates {
    dataPath: string
    period: {
        start: string
        end: string
    }
    totalCount: number
    positiveCount: number
    negativeCount: number
    averageFundingRate: number
    maxFundingRate: number
    minFundingRate: number
}

interface SymbolData {
    symbol: string
    exchangeInfo?: ExchangeInfo
    leverageBrackets?: LeverageBrackets
    fundingRates?: FundingRates
    trading: BarData
    magnifier?: BarData
    reference: BarData[]
    mark: BarData
}

interface SymbolCardProps {
    symbols: SymbolData[]
    initialSymbol?: string
}

// 소수점 끝 0 제거하는 함수
const trimEndZeros = (num: number): string => {
    if (Number.isInteger(num)) {
        return num.toString();
    }
    return num.toString().replace(/\.?0+$/, '');
};

// 최적화된 SymbolCard 컴포넌트
const SymbolCard = memo(({
                             symbols,
                             initialSymbol
                         }: SymbolCardProps) => {
    const [selectedSymbol, setSelectedSymbol] = useState<string>(initialSymbol || (symbols.length > 0 ? symbols[0].symbol : ''));
    const [isDropdownOpen, setIsDropdownOpen] = useState(false);
    const {getLogoUrl} = useLogo();
    const [containerHeight, setContainerHeight] = useState<number | null>(null);
    const containerRef = useRef<HTMLDivElement>(null);

    // 이전 심볼과 현재 심볼을 추적하기 위한 ref
    const prevSymbolRef = useRef<string | null>(null);
    const nextSymbolRef = useRef<string | null>(null);

    // 현재 표시 중인 데이터 상태
    const [currentData, setCurrentData] = useState<SymbolData & { isLoading: boolean }>(() => {
        const initialData = symbols.find(s => s.symbol === selectedSymbol) || symbols[0];
        return {
            ...initialData,
            isLoading: false
        };
    });

    // 초기 컨테이너 높이 설정
    useEffect(() => {
        if (containerRef.current && !containerHeight) {
            setContainerHeight(containerRef.current.offsetHeight);
        }
    }, [containerHeight]);

    useEffect(() => {
        // 새 심볼로 데이터가 변경되었고, 애니메이션이 끝난 후 스크롤 위치 복원
        if (currentData.symbol && prevSymbolRef.current !== currentData.symbol) {
            prevSymbolRef.current = currentData.symbol;
        }
    }, [currentData.symbol]);

    // 스크롤바 감지 및 클래스 추가 함수
    const checkScrollbars = useCallback(() => {
        // 모든 스크롤 가능한 요소 선택
        const verticalScrollElements = document.querySelectorAll('.symbol-card-content, .exchange-info-content, .leverage-bracket-content, .leverage-bracket-scroll, .symbol-card-missing-list');
        const horizontalScrollElements = document.querySelectorAll('.symbol-card-left-section, .symbol-card-right-section');

        // 세로 스크롤바 확인 및 클래스 추가
        verticalScrollElements.forEach((element) => {
            const el = element as HTMLElement;
            const hasVerticalScrollbar = el.scrollHeight > el.clientHeight;

            if (hasVerticalScrollbar) {
                el.classList.add('scrollable-y');
            } else {
                el.classList.remove('scrollable-y');
            }
        });

        // 가로 스크롤바 확인 및 클래스 추가
        horizontalScrollElements.forEach((element) => {
            const el = element as HTMLElement;
            const hasHorizontalScrollbar = el.scrollWidth > el.clientWidth;

            if (hasHorizontalScrollbar) {
                el.classList.add('scrollable-x');
            } else {
                el.classList.remove('scrollable-x');
            }
        });
    }, []);

    // 데이터가 변경되거나 창 크기가 변경될 때 스크롤바 감지
    useEffect(() => {
        // 초기 로드 및 데이터 변경 시 스크롤바 확인
        const timer = setTimeout(checkScrollbars, 200);

        // 창 크기 변경 시 스크롤바 확인
        window.addEventListener('resize', checkScrollbars);

        return () => {
            clearTimeout(timer);
            window.removeEventListener('resize', checkScrollbars);
        };
    }, [currentData, checkScrollbars]);

    // 심볼 변경 시 데이터 업데이트 및 스크롤바 재확인
    const updateSymbolData = useCallback((symbolName: string) => {
        // 이미 선택된 심볼이면 스킵
        if (symbolName === currentData.symbol) {
            return;
        }

        // 현재 심볼과 다음 심볼을 기록
        prevSymbolRef.current = currentData.symbol;
        nextSymbolRef.current = symbolName;

        // 심볼 데이터 찾기
        const symbolData = symbols.find(s => s.symbol === symbolName);
        if (!symbolData) {
            return;
        }

        // 즉시 데이터 변경 (로딩 상태 없음)
        setCurrentData({
            ...symbolData,
            isLoading: false
        });

        // 데이터 변경 후 스크롤바 다시 확인
        setTimeout(checkScrollbars, 200);
    }, [currentData.symbol, symbols, checkScrollbars]);

    // 심볼 선택 변경 핸들러
    const handleSymbolChange = useCallback((event: SelectChangeEvent) => {
        const newValue = event.target.value;
        setSelectedSymbol(newValue);
        updateSymbolData(newValue);
    }, [updateSymbolData]);

    const toggleDropdown = useCallback(() => {
        setIsDropdownOpen(prev => !prev);
    }, []);

    // 드롭다운 열릴 때 선택된 옵션으로 스크롤
    useEffect(() => {
        if (isDropdownOpen) {
            // DOM이 렌더링된 후 실행
            requestAnimationFrame(() => {
                const selectedMenuItem = document.querySelector('.symbol-dropdown-paper .mui-menu-item-root.Mui-selected');
                if (selectedMenuItem) {
                    selectedMenuItem.scrollIntoView({block: 'nearest'});
                }
            });
        }
    }, [isDropdownOpen]);

    const hasMagnifier = currentData.magnifier !== undefined;
    const hasExchangeInfo = currentData.exchangeInfo !== undefined;
    const hasLeverageBrackets = currentData.leverageBrackets !== undefined && currentData.leverageBrackets.brackets.length > 0;
    const hasFundingRates = currentData.fundingRates !== undefined;

    // 애니메이션 변형 객체
    const pageTransition = {
        duration: 0.5, // 500ms
        ease: [0.16, 1, 0.3, 1] as const
    };

    const renderBar = (title: string, data?: BarData | BarData[]) => {
        if (!data ||
            (Array.isArray(data) && data.length === 0) ||
            (!Array.isArray(data) && Object.keys(data).length === 0)) {
            return null
        }

        const bars = Array.isArray(data) ? data : [data]

        return (
            <motion.div
                className="symbol-card-container"
                initial={{opacity: 1}}
                animate={{opacity: 1}}
                transition={pageTransition}
            >
                <div className="symbol-card-border"/>
                <motion.div
                    className="symbol-card-header"
                    initial={{opacity: 1}}
                    animate={{opacity: 1}}
                    transition={pageTransition}
                >
                    {title}
                </motion.div>
                {bars.map((bar, idx) => {
                    const key = `${title}-${idx}`
                    return (
                        <motion.div
                            key={key}
                            className="symbol-card-content"
                            initial={{opacity: 1}}
                            animate={{opacity: 1}}
                            transition={pageTransition}
                        >
                            <div className="symbol-card-inner">
                                <div className="symbol-card-left-section">
                                    <div className="symbol-card-row">
                                        <div className="symbol-card-row-inner">
                                            <span className="symbol-card-bullet">&bull;</span>
                                            <span className="symbol-card-label">데이터 경로</span>
                                        </div>
                                    </div>
                                    <div className="symbol-card-row">
                                        <div className="symbol-card-row-inner">
                                            <span className="symbol-card-bullet">&bull;</span>
                                            <span className="symbol-card-label">데이터 기간</span>
                                        </div>
                                    </div>
                                    <div className="symbol-card-row">
                                        <div className="symbol-card-row-inner">
                                            <span className="symbol-card-bullet">&bull;</span>
                                            <span className="symbol-card-label">타임프레임</span>
                                        </div>
                                    </div>
                                    <div className="symbol-card-row">
                                        <div className="symbol-card-row-inner">
                                            <span className="symbol-card-bullet">&bull;</span>
                                            <span className="symbol-card-label">전체 바 개수</span>
                                        </div>
                                    </div>
                                    <div className="symbol-card-row">
                                        <div className="symbol-card-row-inner">
                                            <span className="symbol-card-bullet">&bull;</span>
                                            <span className="symbol-card-label">누락된 바 개수</span>
                                        </div>
                                    </div>
                                </div>

                                <div className="symbol-card-right-section">
                                    <div className="symbol-card-row">
                                        <div className="symbol-card-row-inner">
                                            <span className="symbol-card-path-value">{bar.path}</span>
                                        </div>
                                    </div>
                                    <div className="symbol-card-row">
                                        <div className="symbol-card-row-inner">
                                            <span
                                                className="symbol-card-value">{bar.period.start} - {bar.period.end}</span>
                                        </div>
                                    </div>
                                    <div className="symbol-card-row">
                                        <div className="symbol-card-row-inner">
                                            <span className="symbol-card-value">{bar.timeframe}</span>
                                        </div>
                                    </div>
                                    <div className="symbol-card-row">
                                        <div className="symbol-card-row-inner">
                                            <span className="symbol-card-value">{bar.count.toLocaleString()}개</span>
                                        </div>
                                    </div>
                                    <div className="symbol-card-row" style={{
                                        marginBottom: (bar.missing.count > 0 && bar.missing.times.length > 0) ? '10px' : undefined
                                    }}>
                                        <div className="symbol-card-row-inner">
                                            <span
                                                className="symbol-card-value">{bar.missing.count.toLocaleString()}개</span>
                                        </div>
                                    </div>

                                    {bar.missing.count > 0 && bar.missing.times.length > 0 && (
                                        <ul className="symbol-card-missing-list">
                                            {bar.missing.times.map((t, i) => (
                                                <li key={i} className="symbol-card-missing-item">
                                                    <span className="symbol-card-missing-bullet">&bull;</span>
                                                    <span className="symbol-card-missing-text">{t}</span>
                                                </li>
                                            ))}
                                        </ul>
                                    )}
                                </div>
                            </div>
                        </motion.div>
                    )
                })}
            </motion.div>
        )
    }

    // 거래소 정보 렌더링 함수
    const renderExchangeInfo = (data?: ExchangeInfo) => {
        if (!data) {
            return null;
        }

        // 스크롤 동기화를 위한 ref들
        const exchangeLeftRef = useRef<HTMLDivElement>(null);
        const exchangeRightRef = useRef<HTMLDivElement>(null);

        // 스크롤바 높이 조정 및 동기화 함수
        const syncExchangeScroll = useCallback(() => {
            if (exchangeRightRef.current && exchangeLeftRef.current) {
                // 스크롤 위치 동기화
                exchangeLeftRef.current.scrollTop = exchangeRightRef.current.scrollTop;

                // 가로 스크롤바 공간 보정
                const hasHorizontalScrollbar = exchangeRightRef.current.scrollWidth > exchangeRightRef.current.clientWidth;
                const scrollbarHeight = hasHorizontalScrollbar ? exchangeRightRef.current.offsetHeight - exchangeRightRef.current.clientHeight : 0;

                if (scrollbarHeight > 0) {
                    // 왼쪽 영역 하단에 스크롤바 높이만큼 패딩 추가
                    exchangeLeftRef.current.style.paddingBottom = `${scrollbarHeight}px`;
                } else {
                    exchangeLeftRef.current.style.paddingBottom = '0px';
                }

                // 스크롤바 감지 및 클래스 추가
                if (hasHorizontalScrollbar) {
                    exchangeRightRef.current.classList.add('scrollable-x');
                } else {
                    exchangeRightRef.current.classList.remove('scrollable-x');
                }
            }
        }, []);

        // 왼쪽 영역 휠 이벤트 처리
        const handleExchangeLeftScroll = useCallback(() => {
            if (exchangeLeftRef.current && exchangeRightRef.current) {
                // 왼쪽 영역 스크롤 발생 시 오른쪽 영역 스크롤 동기화
                exchangeRightRef.current.scrollTop = exchangeLeftRef.current.scrollTop;
            }
        }, []);

        // 왼쪽 영역 너비 자동 조정 함수
        const adjustExchangeLeftWidth = useCallback(() => {
            if (!exchangeLeftRef.current) {
                return;
            }

            // 왼쪽 영역의 모든 라벨 요소 가져오기
            const labels = exchangeLeftRef.current.querySelectorAll('.exchange-info-label');

            // 가장 긴 텍스트를 가진 라벨 찾기
            let maxWidth = 0;
            labels.forEach(label => {
                const labelWidth = (label as HTMLElement).offsetWidth;
                maxWidth = Math.max(maxWidth, labelWidth);
            });

            // 기본 패딩과 여백 고려 (불릿 + 여백 + 패딩)
            const padding = 32 + 24; // 패딩, 마진, 글머리 기호 등의 공간

            // 너비 계산 및 적용
            const finalWidth = maxWidth + padding;
            exchangeLeftRef.current.style.width = `${finalWidth}px`;
            exchangeLeftRef.current.style.minWidth = `${finalWidth}px`;
        }, []);

        // 마운트 및 업데이트 시 설정
        useEffect(() => {
            // 왼쪽 너비 조정 및 스크롤바 공간 조정
            setTimeout(() => {
                adjustExchangeLeftWidth(); // 왼쪽 너비 먼저 조정
                syncExchangeScroll();      // 그 다음 스크롤 동기화
            }, 100);

            // 윈도우 리사이즈 이벤트에도 대응
            const handleResize = () => {
                adjustExchangeLeftWidth(); // 리사이즈 시에도 너비 재조정
                syncExchangeScroll();
            };
            window.addEventListener('resize', handleResize);

            // 스크롤 동기화 이벤트 리스너 등록
            const rightSection = exchangeRightRef.current;
            if (rightSection) {
                rightSection.addEventListener('scroll', syncExchangeScroll);
            }

            return () => {
                window.removeEventListener('resize', handleResize);
                if (rightSection) {
                    rightSection.removeEventListener('scroll', syncExchangeScroll);
                }
            };
        }, [syncExchangeScroll, adjustExchangeLeftWidth]);

        return (
            <motion.div
                className="symbol-card-container"
                initial={{opacity: 1}}
                animate={{opacity: 1}}
                transition={pageTransition}
                onAnimationComplete={() => {
                    // 애니메이션 완료 후 너비 조정 및 스크롤바 공간 조정
                    setTimeout(() => {
                        adjustExchangeLeftWidth(); // 왼쪽 너비 먼저 조정
                        syncExchangeScroll();      // 그 다음 스크롤 동기화
                    }, 100);
                }}
            >
                <div className="symbol-card-border"/>
                <motion.div
                    className="symbol-card-header"
                    initial={{opacity: 1}}
                    animate={{opacity: 1}}
                    transition={pageTransition}
                >
                    거래소 정보
                </motion.div>
                <motion.div
                    className="exchange-info-content"
                    initial={{opacity: 1}}
                    animate={{opacity: 1}}
                    transition={pageTransition}
                >
                    <div className="exchange-info-item">
                        <div
                            className="exchange-info-left"
                            ref={exchangeLeftRef}
                            onScroll={handleExchangeLeftScroll}
                        >
                            <div className="exchange-info-row">
                                <div className="exchange-info-row-inner">
                                    <span className="exchange-info-bullet">•</span>
                                    <span className="exchange-info-label">데이터 경로</span>
                                </div>
                            </div>
                            <div className="exchange-info-row">
                                <div className="exchange-info-row-inner">
                                    <span className="exchange-info-bullet">•</span>
                                    <span className="exchange-info-label">가격 최소 단위</span>
                                </div>
                            </div>
                            <div className="exchange-info-row">
                                <div className="exchange-info-row-inner">
                                    <span className="exchange-info-bullet">•</span>
                                    <span className="exchange-info-label">가격 소수점 정밀도</span>
                                </div>
                            </div>
                            <div className="exchange-info-row">
                                <div className="exchange-info-row-inner">
                                    <span className="exchange-info-bullet">•</span>
                                    <span className="exchange-info-label">수량 최소 단위</span>
                                </div>
                            </div>
                            <div className="exchange-info-row">
                                <div className="exchange-info-row-inner">
                                    <span className="exchange-info-bullet">•</span>
                                    <span className="exchange-info-label">수량 소수점 정밀도</span>
                                </div>
                            </div>
                            <div className="exchange-info-row">
                                <div className="exchange-info-row-inner">
                                    <span className="exchange-info-bullet">•</span>
                                    <span className="exchange-info-label">시장가 최대 수량</span>
                                </div>
                            </div>
                            <div className="exchange-info-row">
                                <div className="exchange-info-row-inner">
                                    <span className="exchange-info-bullet">•</span>
                                    <span className="exchange-info-label">시장가 최소 수량</span>
                                </div>
                            </div>
                            <div className="exchange-info-row">
                                <div className="exchange-info-row-inner">
                                    <span className="exchange-info-bullet">•</span>
                                    <span className="exchange-info-label">지정가 최대 수량</span>
                                </div>
                            </div>
                            <div className="exchange-info-row">
                                <div className="exchange-info-row-inner">
                                    <span className="exchange-info-bullet">•</span>
                                    <span className="exchange-info-label">지정가 최소 수량</span>
                                </div>
                            </div>
                            <div className="exchange-info-row">
                                <div className="exchange-info-row-inner">
                                    <span className="exchange-info-bullet">•</span>
                                    <span className="exchange-info-label">최소 명목 가치</span>
                                </div>
                            </div>
                            <div className="exchange-info-row">
                                <div className="exchange-info-row-inner">
                                    <span className="exchange-info-bullet">•</span>
                                    <span className="exchange-info-label">강제 청산 수수료율</span>
                                </div>
                            </div>
                        </div>

                        <div
                            className="exchange-info-right"
                            ref={exchangeRightRef}
                            onScroll={syncExchangeScroll}
                        >
                            <div className="exchange-info-row">
                                <div className="exchange-info-row-inner">
                                    <span className="exchange-info-value">{data.dataPath}</span>
                                </div>
                            </div>
                            <div className="exchange-info-row">
                                <div className="exchange-info-row-inner">
                                    <span className="exchange-info-value">{trimEndZeros(data.priceStep)}</span>
                                </div>
                            </div>
                            <div className="exchange-info-row">
                                <div className="exchange-info-row-inner">
                                    <span className="exchange-info-value">{trimEndZeros(data.pricePrecision)}</span>
                                </div>
                            </div>
                            <div className="exchange-info-row">
                                <div className="exchange-info-row-inner">
                                    <span className="exchange-info-value">{trimEndZeros(data.qtyStep)}</span>
                                </div>
                            </div>
                            <div className="exchange-info-row">
                                <div className="exchange-info-row-inner">
                                    <span className="exchange-info-value">{trimEndZeros(data.qtyPrecision)}</span>
                                </div>
                            </div>
                            <div className="exchange-info-row">
                                <div className="exchange-info-row-inner">
                                    <span
                                        className="exchange-info-value">{data.maxMarketOrderQty.toLocaleString()}</span>
                                </div>
                            </div>
                            <div className="exchange-info-row">
                                <div className="exchange-info-row-inner">
                                    <span className="exchange-info-value">{trimEndZeros(data.minMarketOrderQty)}</span>
                                </div>
                            </div>
                            <div className="exchange-info-row">
                                <div className="exchange-info-row-inner">
                                    <span className="exchange-info-value">{data.maxOrderQty.toLocaleString()}</span>
                                </div>
                            </div>
                            <div className="exchange-info-row">
                                <div className="exchange-info-row-inner">
                                    <span className="exchange-info-value">{trimEndZeros(data.minOrderQty)}</span>
                                </div>
                            </div>
                            <div className="exchange-info-row">
                                <div className="exchange-info-row-inner">
                                    <span className="exchange-info-value">${data.minNotional.toLocaleString()}</span>
                                </div>
                            </div>
                            <div className="exchange-info-row">
                                <div className="exchange-info-row-inner">
                                    <span
                                        className="exchange-info-value">{trimEndZeros((data.liquidationFee * 100))}%</span>
                                </div>
                            </div>
                        </div>
                    </div>
                </motion.div>
            </motion.div>
        )
    }

    // 레버리지 구간 렌더링 함수
    const renderLeverageBrackets = (data?: LeverageBrackets) => {
        if (!data || !data.brackets || data.brackets.length === 0) {
            return null;
        }

        // 가로선 너비 계산을 위한 ref들
        const scrollContainerRef = useRef<HTMLDivElement>(null);
        const rightSectionRef = useRef<HTMLDivElement>(null);
        const leftSectionRef = useRef<HTMLDivElement>(null);
        const separatorRefs = useRef<Array<HTMLDivElement | null>>([]);

        // 가로선 너비를 조정하는 함수
        const adjustSeparatorWidths = () => {
            if (!rightSectionRef.current) {
                return;
            }

            const rightWidth = rightSectionRef.current.scrollWidth;
            const visibleWidth = rightSectionRef.current.clientWidth;

            // 각 구분선의 너비 업데이트
            separatorRefs.current.forEach((separator) => {
                if (!separator) {
                    return;
                }

                // 스크롤 영역이 컨테이너보다 넓으면 스크롤 너비를, 아니면 컨테이너 너비를 사용
                const width = Math.max(rightWidth, visibleWidth);
                separator.style.width = `${width}px`;
            });
        };

        // 스크롤바 높이 조정 및 동기화 함수
        const syncScroll = () => {
            if (rightSectionRef.current && leftSectionRef.current) {
                // 스크롤 위치 동기화
                leftSectionRef.current.scrollTop = rightSectionRef.current.scrollTop;

                // 가로 스크롤바 공간 보정
                const hasHorizontalScrollbar = rightSectionRef.current.scrollWidth > rightSectionRef.current.clientWidth;
                const scrollbarHeight = hasHorizontalScrollbar ? rightSectionRef.current.offsetHeight - rightSectionRef.current.clientHeight : 0;

                if (scrollbarHeight > 0) {
                    // 왼쪽 영역 하단에 스크롤바 높이만큼 패딩 추가
                    leftSectionRef.current.style.paddingBottom = `${scrollbarHeight}px`;
                } else {
                    leftSectionRef.current.style.paddingBottom = '0px';
                }
            }
        };

        // 왼쪽 영역 휠 이벤트 처리
        const handleLeftSectionScroll = () => {
            if (leftSectionRef.current && rightSectionRef.current) {
                // 왼쪽 영역 스크롤 발생 시 오른쪽 영역 스크롤 동기화
                rightSectionRef.current.scrollTop = leftSectionRef.current.scrollTop;
            }
        };

        // 마운트 및 업데이트 시 설정
        useEffect(() => {
            // 초기화
            separatorRefs.current = Array(data.brackets.length).fill(null);

            // 너비 조정 및 스크롤바 공간 조정
            setTimeout(() => {
                adjustSeparatorWidths();
                syncScroll();
            }, 100);

            // 윈도우 리사이즈 이벤트에도 대응
            window.addEventListener('resize', () => {
                adjustSeparatorWidths();
                syncScroll();
            });

            // 스크롤 동기화 이벤트 리스너 등록
            const rightSection = rightSectionRef.current;
            if (rightSection) {
                rightSection.addEventListener('scroll', syncScroll);
            }

            return () => {
                window.removeEventListener('resize', adjustSeparatorWidths);
                if (rightSection) {
                    rightSection.removeEventListener('scroll', syncScroll);
                }
            };
        }, [data.brackets.length]);

        const setSeparatorRef = (idx: number) => (el: HTMLDivElement | null) => {
            separatorRefs.current[idx] = el;
            if (el) adjustSeparatorWidths();
        };

        return (
            <motion.div
                className="leverage-bracket-container symbol-card-container"
                initial={{opacity: 1}}
                animate={{opacity: 1}}
                transition={pageTransition}
                onAnimationComplete={() => {
                    // 애니메이션 완료 후 스크롤바 공간 조정
                    setTimeout(() => {
                        syncScroll();
                        adjustSeparatorWidths();
                    }, 100);
                }}
            >
                <div className="symbol-card-border"/>
                <motion.div
                    className="symbol-card-header"
                    initial={{opacity: 1}}
                    animate={{opacity: 1}}
                    transition={pageTransition}
                >
                    레버리지 구간
                </motion.div>
                <motion.div
                    className="leverage-bracket-content"
                    initial={{opacity: 1}}
                    animate={{opacity: 1}}
                    transition={pageTransition}
                >
                    <div
                        className="leverage-bracket-scroll"
                        ref={scrollContainerRef}
                    >
                        <div
                            className="leverage-bracket-left"
                            ref={leftSectionRef}
                            onScroll={handleLeftSectionScroll}
                        >
                            <div className="symbol-card-row">
                                <div className="symbol-card-row-inner">
                                    <span className="symbol-card-bullet">&bull;</span>
                                    <span className="symbol-card-label">데이터 경로</span>
                                </div>
                            </div>

                            <div className="leverage-bracket-separator"
                                 style={{marginTop: '15px', marginBottom: '15px'}}/>

                            {data.brackets.map((_, idx) => (
                                <div key={`left-${idx}`}>
                                    <div className="symbol-card-row">
                                        <div className="symbol-card-row-inner">
                                            <span className="symbol-card-bullet">&bull;</span>
                                            <span className="symbol-card-label">구간 번호</span>
                                        </div>
                                    </div>
                                    <div className="symbol-card-row">
                                        <div className="symbol-card-row-inner">
                                            <span className="symbol-card-bullet">&bull;</span>
                                            <span className="symbol-card-label">명목 가치 범위</span>
                                        </div>
                                    </div>
                                    <div className="symbol-card-row">
                                        <div className="symbol-card-row-inner">
                                            <span className="symbol-card-bullet">&bull;</span>
                                            <span className="symbol-card-label">최대 레버리지</span>
                                        </div>
                                    </div>
                                    <div className="symbol-card-row">
                                        <div className="symbol-card-row-inner">
                                            <span className="symbol-card-bullet">&bull;</span>
                                            <span className="symbol-card-label">유지 마진율</span>
                                        </div>
                                    </div>
                                    <div className="symbol-card-row">
                                        <div className="symbol-card-row-inner">
                                            <span className="symbol-card-bullet">&bull;</span>
                                            <span className="symbol-card-label">유지 금액</span>
                                        </div>
                                    </div>

                                    {idx < data.brackets.length - 1 && (
                                        <div className="leverage-bracket-separator"
                                             style={{marginTop: '15px', marginBottom: '15px'}}/>
                                    )}
                                </div>
                            ))}
                        </div>

                        <div
                            className="leverage-bracket-right"
                            ref={rightSectionRef}
                            onScroll={() => {
                                adjustSeparatorWidths();
                                syncScroll();
                            }}
                        >
                            <div className="symbol-card-row">
                                <div className="symbol-card-row-inner">
                                    <span className="symbol-card-value">{data.dataPath}</span>
                                </div>
                            </div>

                            <div
                                className="leverage-bracket-separator"
                                style={{marginTop: '15px', marginBottom: '15px'}}
                                ref={(el) => {
                                    if (el && rightSectionRef.current) {
                                        setTimeout(() => {
                                            if (rightSectionRef.current) {
                                                const rightWidth = rightSectionRef.current.scrollWidth;
                                                const visibleWidth = rightSectionRef.current.clientWidth;
                                                const width = Math.max(rightWidth, visibleWidth);
                                                el.style.width = `${width}px`;
                                            }
                                        }, 100);
                                    }
                                }}
                            />

                            {data.brackets.map((bracket, idx) => (
                                <div key={`right-${idx}`}>
                                    <div className="symbol-card-row">
                                        <div className="symbol-card-row-inner">
                                            <span
                                                className="symbol-card-value">{bracket.bracketNum.toLocaleString()}번</span>
                                        </div>
                                    </div>
                                    <div className="symbol-card-row">
                                        <div className="symbol-card-row-inner">
                                            <span
                                                className="symbol-card-value">${bracket.minNotional.toLocaleString()} - ${bracket.maxNotional.toLocaleString()}</span>
                                        </div>
                                    </div>
                                    <div className="symbol-card-row">
                                        <div className="symbol-card-row-inner">
                                            <span
                                                className="symbol-card-value">{bracket.maxLeverage.toLocaleString()}x</span>
                                        </div>
                                    </div>
                                    <div className="symbol-card-row">
                                        <div className="symbol-card-row-inner">
                                            <span
                                                className="symbol-card-value">{trimEndZeros(bracket.maintMarginRatio * 100)}%</span>
                                        </div>
                                    </div>
                                    <div className="symbol-card-row">
                                        <div className="symbol-card-row-inner">
                                            <span
                                                className="symbol-card-value">${bracket.maintAmount.toLocaleString()}</span>
                                        </div>
                                    </div>

                                    {idx < data.brackets.length - 1 && (
                                        <div
                                            className="leverage-bracket-separator"
                                            ref={setSeparatorRef(idx)}
                                            style={{marginTop: '15px', marginBottom: '15px'}}
                                        />
                                    )}
                                </div>
                            ))}
                        </div>
                    </div>
                </motion.div>
            </motion.div>
        )
    }

    // 펀딩 비율 렌더링 함수
    const renderFundingRates = (data?: FundingRates) => {
        if (!data) {
            return null;
        }

        return (
            <motion.div
                className="symbol-card-container"
                initial={{opacity: 1}}
                animate={{opacity: 1}}
                transition={pageTransition}
            >
                <div className="symbol-card-border"/>
                <motion.div
                    className="symbol-card-header"
                    initial={{opacity: 1}}
                    animate={{opacity: 1}}
                    transition={pageTransition}
                >
                    펀딩 비율
                </motion.div>
                <motion.div
                    className="symbol-card-content"
                    initial={{opacity: 1}}
                    animate={{opacity: 1}}
                    transition={pageTransition}
                >
                    <div className="symbol-card-inner">
                        <div className="symbol-card-left-section">
                            <div className="symbol-card-row">
                                <div className="symbol-card-row-inner">
                                    <span className="symbol-card-bullet">&bull;</span>
                                    <span className="symbol-card-label">데이터 경로</span>
                                </div>
                            </div>
                            <div className="symbol-card-row">
                                <div className="symbol-card-row-inner">
                                    <span className="symbol-card-bullet">&bull;</span>
                                    <span className="symbol-card-label">데이터 기간</span>
                                </div>
                            </div>
                            <div className="symbol-card-row">
                                <div className="symbol-card-row-inner">
                                    <span className="symbol-card-bullet">&bull;</span>
                                    <span className="symbol-card-label">합계 펀딩 횟수</span>
                                </div>
                            </div>
                            <div className="symbol-card-row">
                                <div className="symbol-card-row-inner">
                                    <span className="symbol-card-bullet">&bull;</span>
                                    <span className="symbol-card-label">양수 펀딩 횟수</span>
                                </div>
                            </div>
                            <div className="symbol-card-row">
                                <div className="symbol-card-row-inner">
                                    <span className="symbol-card-bullet">&bull;</span>
                                    <span className="symbol-card-label">음수 펀딩 횟수</span>
                                </div>
                            </div>
                            <div className="symbol-card-row">
                                <div className="symbol-card-row-inner">
                                    <span className="symbol-card-bullet">&bull;</span>
                                    <span className="symbol-card-label">평균 펀딩 비율</span>
                                </div>
                            </div>
                            <div className="symbol-card-row">
                                <div className="symbol-card-row-inner">
                                    <span className="symbol-card-bullet">&bull;</span>
                                    <span className="symbol-card-label">최고 펀딩 비율</span>
                                </div>
                            </div>
                            <div className="symbol-card-row">
                                <div className="symbol-card-row-inner">
                                    <span className="symbol-card-bullet">&bull;</span>
                                    <span className="symbol-card-label">최저 펀딩 비율</span>
                                </div>
                            </div>
                        </div>

                        <div className="symbol-card-right-section">
                            <div className="symbol-card-row">
                                <div className="symbol-card-row-inner">
                                    <span className="symbol-card-path-value">{data.dataPath}</span>
                                </div>
                            </div>
                            <div className="symbol-card-row">
                                <div className="symbol-card-row-inner">
                                    <span className="symbol-card-value">{data.period.start} - {data.period.end}</span>
                                </div>
                            </div>
                            <div className="symbol-card-row">
                                <div className="symbol-card-row-inner">
                                    <span className="symbol-card-value">{data.totalCount.toLocaleString()}회</span>
                                </div>
                            </div>
                            <div className="symbol-card-row">
                                <div className="symbol-card-row-inner">
                                    <span className="symbol-card-value">{data.positiveCount.toLocaleString()}회</span>
                                </div>
                            </div>
                            <div className="symbol-card-row">
                                <div className="symbol-card-row-inner">
                                    <span className="symbol-card-value">{data.negativeCount.toLocaleString()}회</span>
                                </div>
                            </div>
                            <div className="symbol-card-row">
                                <div className="symbol-card-row-inner">
                                    <span
                                        className="symbol-card-value">{(data.averageFundingRate * 100).toFixed(6).replace(/\.?0+$/, '')}%</span>
                                </div>
                            </div>
                            <div className="symbol-card-row">
                                <div className="symbol-card-row-inner">
                                    <span
                                        className="symbol-card-value">{trimEndZeros(data.maxFundingRate * 100)}%</span>
                                </div>
                            </div>
                            <div className="symbol-card-row">
                                <div className="symbol-card-row-inner">
                                    <span
                                        className="symbol-card-value">{trimEndZeros(data.minFundingRate * 100)}%</span>
                                </div>
                            </div>
                        </div>
                    </div>
                </motion.div>
            </motion.div>
        )
    }

    // 심볼 아이콘 컴포넌트
    const SymbolIcon = ({symbolName, size = 28, isDropdownOption = false, isLarge = false, isSelected = false}: {
        symbolName: string,
        size?: number,
        isLarge?: boolean,
        isDropdownOption?: boolean,
        isSelected?: boolean
    }) => {
        const logoUrl = getLogoUrl(symbolName);

        return (
            <div
                className={isDropdownOption ? "symbol-dropdown-icon-wrapper" : "symbol-icon-wrapper"}
                style={isDropdownOption ? {
                    width: `${size - 6}px`,
                    height: `${size - 6}px`,
                    borderRadius: '50%',
                    backgroundColor: 'rgba(10, 10, 10, 0.3)',
                    display: 'flex',
                    justifyContent: 'center',
                    alignItems: 'center',
                    marginRight: '12px',
                    boxShadow: isSelected
                        ? '0 0 6px rgba(255, 215, 0, 0.5)'
                        : '0 2px 4px rgba(0, 0, 0, 0.2)',
                    transition: 'all 0.2s ease',
                    border: isSelected
                        ? '1px solid rgba(255, 215, 0, 0.8)'
                        : '1px solid transparent',
                    padding: '2px',
                    position: 'relative',
                } : isLarge ? {
                    width: `${size}px`,
                    height: `${size}px`,
                    borderRadius: '50%',
                    display: 'flex',
                    justifyContent: 'center',
                    alignItems: 'center',
                    marginRight: '8px',
                    marginLeft: '12px',
                    boxShadow: 'none',
                    border: 'none',
                    position: 'relative',
                } : {
                    width: `${size}px`,
                    height: `${size}px`,
                    borderRadius: '50%',
                    backgroundColor: 'transparent',
                    display: 'flex',
                    justifyContent: 'center',
                    alignItems: 'center',
                    marginRight: '4px',
                    marginLeft: '14px',
                    boxShadow: 'none',
                    transition: 'all 0.2s ease',
                    border: 'none',
                    position: 'relative',
                }}
            >
                <img
                    className={isDropdownOption ? "symbol-dropdown-icon" : "symbol-icon"}
                    src={logoUrl}
                    alt={symbolName}
                    style={isLarge ? {
                        width: `${size - 6}px`,
                        height: `${size - 6}px`,
                        borderRadius: '50%',
                        objectFit: 'cover',
                        border: '1px solid rgba(255, 215, 0, 0.6)',
                        padding: '2px',
                        backgroundColor: 'rgba(10, 10, 10, 0.3)',
                        boxShadow: '0 0 5px rgba(255, 215, 0, 0.3)',
                        boxSizing: 'border-box',
                    } : isDropdownOption ? {
                        width: `${size - 12}px`,
                        height: `${size - 12}px`,
                        borderRadius: '50%',
                        objectFit: 'cover',
                        filter: 'none',
                        border: 'none',
                        boxShadow: 'none',
                        transition: 'transform 0.3s ease',
                    } : {
                        width: `${size - 6}px`,
                        height: `${size - 6}px`,
                        borderRadius: '50%',
                        objectFit: 'cover',
                        filter: 'none',
                        border: 'none',
                        boxShadow: 'none',
                        transition: 'transform 0.3s ease',
                    }}
                    onError={(e) => {
                        (e.target as HTMLImageElement).src = "/Logos/USDT.png";
                    }}
                />
            </div>
        );
    };

    // 렌더링 후 스크롤바 다시 확인
    useEffect(() => {
        checkScrollbars();
    }, [checkScrollbars]);

    // 애니메이션 완료 후에도 스크롤바 확인
    const handleAnimationComplete = () => {
        // 애니메이션 완료 후 높이 업데이트
        if (containerRef.current) {
            setContainerHeight(null); // 강제로 높이 재계산 트리거
        }

        // 스크롤바 다시 확인
        setTimeout(checkScrollbars, 200);
    };

    useEffect(() => {
        if (symbols.length > 0) {
            // 심볼 설정 로직
        }
    }, [symbols, initialSymbol]);

    return (
        <motion.div
            className="overflow-hidden relative"
            initial={{opacity: 0}}
            animate={{opacity: 1}}
            transition={pageTransition}
            exit="exit"
            style={{
                borderRadius: '8px',
                background: '#111111',
                border: '1.2px solid rgba(255, 215, 0, 0.4)',
                boxShadow: '0 4px 10px rgba(0, 0, 0, 0.3)',
                marginTop: '7px',
                marginLeft: '4px',
                marginRight: '4px',
                overflow: 'hidden',
                backfaceVisibility: 'hidden'
            }}
        >
            <div className="bg-gradient-to-r from-[#1A1A1A] to-[#111111] relative p-4">
                <div className="flex items-center" style={{
                    display: 'flex',
                    alignItems: 'center',
                    marginTop: '10px'
                }}>
                    <SymbolIcon symbolName={currentData.symbol} size={48} isLarge={true}/>
                    <div
                        className="text-2xl text-[#FFD700] font-bold"
                    >{currentData.symbol}</div>
                </div>

                <Box
                    sx={{
                        position: 'absolute',
                        top: 25,
                        right: 20,
                        zIndex: 10,
                        display: 'flex',
                        alignItems: 'center',
                        gap: 1.5,
                        backgroundColor: 'transparent',
                        padding: '4px 8px',
                        borderRadius: '6px',
                    }}
                >
                    <FormControl variant="outlined" size="small" sx={{minWidth: 120}}>
                        <InputLabel id="symbol-label" sx={{
                            color: 'white',
                            fontSize: '16px',
                            fontWeight: 400,
                            fontFamily: "'Inter', 'Pretendard', sans-serif",
                            '&.Mui-focused': {
                                color: isDropdownOpen ? 'rgba(255, 215, 0, 0.8)' : 'white',
                            }
                        }}>심볼</InputLabel>
                        <Select
                            labelId="symbol-label"
                            value={selectedSymbol}
                            onChange={handleSymbolChange}
                            onOpen={toggleDropdown}
                            onClose={toggleDropdown}
                            label="심볼"
                            sx={{
                                backgroundColor: '#111111',
                                color: 'white',
                                fontSize: '15px',
                                fontWeight: 500,
                                fontFamily: "'Inter', 'Pretendard', sans-serif",
                                boxShadow: isDropdownOpen ? '0 0 8px rgba(255, 215, 0, 0.4)' : 'none',
                                transition: 'box-shadow 0.3s cubic-bezier(0.4, 0, 0.2, 1), border-color 0.3s cubic-bezier(0.4, 0, 0.2, 1)',
                                '.MuiSelect-select': {
                                    padding: '8px 32px 8px 14px',
                                },
                                '& .MuiOutlinedInput-notchedOutline': {
                                    borderColor: 'rgba(255, 215, 0, 0.4)',
                                    transition: 'border-color 0.3s cubic-bezier(0.4, 0, 0.2, 1)'
                                },
                                '&.Mui-focused .MuiOutlinedInput-notchedOutline': {
                                    borderColor: isDropdownOpen ? 'rgba(255, 215, 0, 0.8)' : 'rgba(255, 215, 0, 0.4)',
                                    borderWidth: '1px',
                                },
                                '&:hover .MuiOutlinedInput-notchedOutline': {
                                    borderColor: 'rgba(255, 215, 0, 0.8)',
                                },
                                '& .MuiSvgIcon-root': {
                                    color: 'rgba(255, 215, 0, 0.8)',
                                    transition: 'transform 0.3s cubic-bezier(0.4, 0, 0.2, 1)'
                                },
                                '&.Mui-focused .MuiSvgIcon-root': {
                                    transform: isDropdownOpen ? 'rotate(180deg)' : 'rotate(0deg)',
                                }
                            }}
                            MenuProps={{
                                className: 'symbol-dropdown-menu',
                                disablePortal: false,
                                PaperProps: {
                                    className: 'symbol-dropdown-paper',
                                    sx: {
                                        backgroundColor: '#111111',
                                        border: '1px solid rgba(255, 215, 0, 0.4)',
                                        borderRadius: '6px',
                                        boxShadow: '0 4px 20px rgba(0, 0, 0, 0.8)',
                                        mt: 0.5,
                                        maxHeight: '300px',
                                        '& .MuiMenuItem-root': {
                                            color: 'white',
                                            fontSize: '15px',
                                            fontFamily: "'Inter', 'Pretendard', sans-serif",
                                            minHeight: 'auto',
                                            transition: 'background-color 0.2s, color 0.2s',
                                            '&:hover': {
                                                backgroundColor: 'rgba(255, 215, 0, 0.15)',
                                            },
                                            '&.Mui-selected': {
                                                backgroundColor: 'rgba(255, 215, 0, 0.2)',
                                                color: 'rgba(255, 215, 0, 0.9)',
                                                '&:hover': {
                                                    backgroundColor: 'rgba(255, 215, 0, 0.25)',
                                                }
                                            },
                                            '& .MuiTouchRipple-root': {
                                                display: 'none'
                                            }
                                        },
                                        transform: 'translateY(-8px)',
                                        opacity: 0,
                                        '&.MuiPopover-paper': {
                                            transform: 'translateY(0)',
                                            opacity: 1
                                        }
                                    }
                                },
                                TransitionProps: {
                                    timeout: 300
                                }
                            }}
                        >
                            {symbols.map((symbolData: SymbolData) => (
                                <MenuItem key={symbolData.symbol} value={symbolData.symbol}
                                          sx={{display: 'flex', alignItems: 'center'}}>
                                    <div style={{display: 'flex', alignItems: 'center'}}>
                                        <SymbolIcon
                                            symbolName={symbolData.symbol}
                                            size={32}
                                            isDropdownOption={true}
                                            isSelected={symbolData.symbol === selectedSymbol}
                                        />
                                        <span>{symbolData.symbol}</span>
                                    </div>
                                </MenuItem>
                            ))}
                        </Select>
                    </FormControl>
                </Box>
            </div>
            <div className="pt-2 pr-5 pl-5 pb-8">
                <div className="overflow-hidden" ref={containerRef} style={{
                    minHeight: containerHeight ? `${containerHeight}px` : 'auto',
                    height: 'auto',
                    position: 'relative',
                    backgroundColor: '#111111', // 배경색 지정으로 흰색 깜빡임 방지
                    overflow: 'hidden'
                }}>
                    <AnimatePresence
                        mode="wait"
                        initial={false}
                        onExitComplete={() => {
                            setTimeout(() => {
                                checkScrollbars();
                            }, 150);
                        }}
                    >
                        <motion.div
                            key={currentData.symbol}
                            initial={{
                                opacity: 0
                            }}
                            animate={{
                                opacity: 1
                            }}
                            exit={{
                                opacity: 0
                            }}
                            transition={{
                                duration: 0.25,
                                ease: "easeOut"
                            }}
                            className="grid grid-cols-1 gap-4 lg:grid-cols-2 symbol-card-grid"
                            style={{
                                width: "100%",
                                backgroundColor: '#111111', // 배경색 지정으로 흰색 깜빡임 방지
                                willChange: 'transform, opacity'
                            }}
                            onAnimationComplete={handleAnimationComplete}
                        >
                            {hasExchangeInfo && renderExchangeInfo(currentData.exchangeInfo)}
                            {hasLeverageBrackets && renderLeverageBrackets(currentData.leverageBrackets)}
                            {renderBar("트레이딩 바 데이터", currentData.trading)}
                            {hasMagnifier && renderBar("돋보기 바 데이터", currentData.magnifier)}
                            {currentData.reference && currentData.reference.map((refBar, index) =>
                                renderBar(currentData.reference.length === 1 ? "참조 바 데이터" : `참조 바 데이터 ${index + 1}`, refBar)
                            )}
                            {renderBar("마크 가격 바 데이터", currentData.mark)}
                            {hasFundingRates && renderFundingRates(currentData.fundingRates)}
                        </motion.div>
                    </AnimatePresence>
                </div>
            </div>

            {/* CSS 스타일 추가 */}
            <div dangerouslySetInnerHTML={{
                __html: `
                <style>
                    /* 드롭다운 아이콘 스타일 */
                    .symbol-dropdown-icon {
                        width: 16px;
                        height: 16px;
                        border-radius: 50%;
                        object-fit: cover;
                        transition: transform 0.3s ease;
                    }
                    
                    /* 호버 시 로고에 금색 테두리 추가 */
                    .mui-menu-item-root:hover .symbol-dropdown-icon-wrapper {
                        border-color: rgba(255, 215, 0, 0.6) !important;
                        box-shadow: 0 0 6px rgba(255, 215, 0, 0.3) !important;
                    }
                    
                    .mui-menu-item-root:hover .symbol-dropdown-icon {
                        transform: scale(1.1);
                        filter: brightness(1.1);
                        transition: all 0.2s ease;
                    }
                    
                    /* 선택된 메뉴 아이템 스타일 - 확대 효과 없음 */
                    .Mui-selected .symbol-dropdown-icon-wrapper {
                        border-color: rgba(255, 215, 0, 0.8) !important;
                        box-shadow: 0 0 6px rgba(255, 215, 0, 0.5) !important;
                    }
                </style>
                `
            }}/>
        </motion.div>
    )
});

export default SymbolCard;
