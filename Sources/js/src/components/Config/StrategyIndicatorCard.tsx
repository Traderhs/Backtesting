import React, {memo, useCallback, useEffect, useRef, useState} from "react"
import {AnimatePresence, motion} from "framer-motion"
import {Prism as SyntaxHighlighter} from 'react-syntax-highlighter';
import {oneDark} from 'react-syntax-highlighter/dist/esm/styles/prism';
import {FormControl, InputLabel, MenuItem, Select, SelectChangeEvent} from '@mui/material';
import NoDataMessage from '../Common/NoDataMessage';
import "./StrategyIndicatorCard.css";

// CSS 스타일 추가
const styles = {
    infoRow: {
        display: 'flex',
        alignItems: 'center',
        width: '100%',
        minHeight: '24px'
    },
    infoRowInner: {
        display: 'flex',
        alignItems: 'center',
        width: '100%'
    },
    infoBullet: {
        display: 'flex',
        alignItems: 'center',
        marginRight: '8px'
    },
    infoLabel: {
        display: 'flex',
        alignItems: 'center'
    },
    infoValue: {
        display: 'flex',
        alignItems: 'center'
    }
};

// HEX 색상 포맷을 확인하는 함수 추가
const isHexColor = (value: string): boolean => {
    // 정규식을 개선하여 다양한 HEX 포맷 감지
    // #RGB, #RGBA, #RRGGBB, #RRGGBBAA 형식 모두 지원
    return /^#([0-9A-F]{3}|[0-9A-F]{4}|[0-9A-F]{6}|[0-9A-F]{8})$/i.test(value);
};

interface StrategyIndicatorCardProps {
    name: string
    className?: string   // 전략 클래스 이름
    sourcePath?: string  // 전략 소스 파일 경로
    headerPath?: string  // 전략 헤더 파일 경로
    indicators?: {
        name: string
        timeframe: string
        sourcePath?: string  // 소스 파일 경로
        headerPath?: string  // 헤더 파일 경로 
        className?: string   // 클래스 이름
    }[]
}

// 최적화된 StrategyIndicatorCard 컴포넌트
const StrategyIndicatorCard = memo(({
                                        name,
                                        className,
                                        sourcePath,
                                        headerPath,
                                        indicators = []
                                    }: StrategyIndicatorCardProps) => {
    // 지표가 있는지 확인하는 변수
    const hasIndicators = indicators.length > 0;

    const [strategyHeader, setStrategyHeader] = useState<string | null | undefined>(undefined)
    const [strategySource, setStrategySource] = useState<string | null | undefined>(undefined)
    const [selectedIndicator, setSelectedIndicator] = useState<string | null>(hasIndicators ? indicators[0].name : null)
    const [indicatorHeader, setIndicatorHeader] = useState<string | null | undefined>(undefined)
    const [indicatorSource, setIndicatorSource] = useState<string | null | undefined>(undefined)
    const [isDropdownOpen, setIsDropdownOpen] = useState(false);
    const containerRef = useRef<HTMLDivElement>(null);
    // 애니메이션 방향 설정을 위한 상태 추가
    const [initialAnimation, setInitialAnimation] = useState(true);

    // 이전 지표와 현재 지표를 추적하기 위한 ref
    const prevIndicatorRef = useRef<string | null>(null);
    const nextIndicatorRef = useRef<string | null>(null);
    // 현재 선택된 지표의 정보 상태
    const [selectedIndicatorInfo, setSelectedIndicatorInfo] = useState<any>(null);
    // 지표 왼쪽 영역 너비를 저장하는 ref 추가
    const indicatorLeftWidthRef = useRef<number>(0);

    // 스크롤 및 가로선 관련 ref
    const scrollContainerRef = useRef<HTMLDivElement>(null);
    const separatorRefs = useRef<Array<HTMLDivElement | null>>([]);

    // 왼쪽 및 오른쪽 영역 ref
    const infoLeftRef = useRef<HTMLDivElement>(null);
    const infoRightRef = useRef<HTMLDivElement>(null);
    // 지표 정보 영역을 위한 별도 ref 추가
    const indicatorInfoLeftRef = useRef<HTMLDivElement>(null);
    const indicatorInfoRightRef = useRef<HTMLDivElement>(null);
    const codeLeftRef = useRef<HTMLDivElement>(null);
    const codeRightRef = useRef<HTMLDivElement>(null);
    const headerCodeLeftRef = useRef<HTMLDivElement>(null);
    const headerCodeRightRef = useRef<HTMLDivElement>(null);
    const indicatorHeaderCodeLeftRef = useRef<HTMLDivElement>(null);
    const indicatorHeaderCodeRightRef = useRef<HTMLDivElement>(null);
    const indicatorCodeLeftRef = useRef<HTMLDivElement>(null);
    const indicatorCodeRightRef = useRef<HTMLDivElement>(null);

    // 전략과 지표 정보 요소 참조를 위한 ref 추가
    const strategyInfoItemRef = useRef<HTMLDivElement>(null);
    const indicatorInfoItemRef = useRef<HTMLDivElement>(null);

    // 가로선 너비를 조정하는 함수
    const adjustSeparatorWidths = useCallback(() => {
        if (!infoRightRef.current) return;

        const rightWidth = infoRightRef.current.scrollWidth;
        const visibleWidth = infoRightRef.current.clientWidth;

        // 각 구분선의 너비 업데이트
        separatorRefs.current.forEach((separator) => {
            if (!separator) return;

            // 스크롤 영역이 컨테이너보다 넓으면 스크롤 너비를, 아니면 컨테이너 너비를 사용
            const width = Math.max(rightWidth, visibleWidth);
            separator.style.width = `${width}px`;
        });

        // info-separator-right 요소 너비도 업데이트
        if (indicatorInfoItemRef.current) {
            const infoSeparator = indicatorInfoItemRef.current.querySelector('.info-separator-right') as HTMLElement;
            if (infoSeparator) {
                const indicatorRightSection = indicatorInfoItemRef.current.querySelector('.info-right') as HTMLElement;
                if (indicatorRightSection) {
                    const rightScrollWidth = indicatorRightSection.scrollWidth;
                    const rightClientWidth = indicatorRightSection.clientWidth;
                    const width = Math.max(rightScrollWidth, rightClientWidth);
                    infoSeparator.style.width = `${width}px`;
                    infoSeparator.style.minWidth = `${width}px`;
                }
            }
        }
    }, []);

    // 스크롤바 감지 및 클래스 추가 함수
    const checkScrollbars = useCallback(() => {
        // 전략 정보 영역 스크롤바 확인
        if (infoRightRef.current && strategyInfoItemRef.current) {
            const strategyInfoRight = strategyInfoItemRef.current.querySelector('.info-right') as HTMLElement;
            if (strategyInfoRight) {
                const hasHorizontalScrollbar = strategyInfoRight.scrollWidth > strategyInfoRight.clientWidth;

                if (hasHorizontalScrollbar) {
                    strategyInfoRight.classList.add('scrollable-x');
                } else {
                    strategyInfoRight.classList.remove('scrollable-x');
                }
            }
        }

        // 지표 정보 영역 스크롤바 확인
        if (indicatorInfoRightRef.current && indicatorInfoItemRef.current) {
            const indicatorInfoRight = indicatorInfoItemRef.current.querySelector('.info-right') as HTMLElement;
            if (indicatorInfoRight) {
                const hasHorizontalScrollbar = indicatorInfoRight.scrollWidth > indicatorInfoRight.clientWidth;

                if (hasHorizontalScrollbar) {
                    indicatorInfoRight.classList.add('scrollable-x');
                } else {
                    indicatorInfoRight.classList.remove('scrollable-x');
                }
            }
        }
    }, []);

    // 스크롤바 높이 조정 및 동기화 함수
    const syncScroll = useCallback(() => {
        if (infoRightRef.current && infoLeftRef.current) {
            // 스크롤 위치 동기화
            infoLeftRef.current.scrollTop = infoRightRef.current.scrollTop;

            // 가로 스크롤바 공간 보정
            const hasHorizontalScrollbar = infoRightRef.current.scrollWidth > infoRightRef.current.clientWidth;
            const scrollbarHeight = hasHorizontalScrollbar ? infoRightRef.current.offsetHeight - infoRightRef.current.clientHeight : 0;

            if (scrollbarHeight > 0) {
                // 왼쪽 영역 하단에 스크롤바 높이만큼 패딩 추가
                infoLeftRef.current.style.paddingBottom = `${scrollbarHeight}px`;
            } else {
                infoLeftRef.current.style.paddingBottom = '0px';
            }

            // 스크롤바 감지 및 클래스 추가
            checkScrollbars();

            // 스크롤 시에도 구분선 너비 조정
            adjustSeparatorWidths();
        }
    }, [checkScrollbars, adjustSeparatorWidths]);

    // 지표 정보 영역 스크롤 동기화 함수 (새로 추가)
    const syncIndicatorScroll = useCallback(() => {
        if (indicatorInfoRightRef.current && indicatorInfoLeftRef.current) {
            // 스크롤 위치 동기화
            indicatorInfoLeftRef.current.scrollTop = indicatorInfoRightRef.current.scrollTop;

            // 가로 스크롤바 공간 보정
            const hasHorizontalScrollbar = indicatorInfoRightRef.current.scrollWidth > indicatorInfoRightRef.current.clientWidth;
            const scrollbarHeight = hasHorizontalScrollbar ? indicatorInfoRightRef.current.offsetHeight - indicatorInfoRightRef.current.clientHeight : 0;

            if (scrollbarHeight > 0) {
                // 왼쪽 영역 하단에 스크롤바 높이만큼 패딩 추가
                indicatorInfoLeftRef.current.style.paddingBottom = `${scrollbarHeight}px`;
            } else {
                indicatorInfoLeftRef.current.style.paddingBottom = '0px';
            }

            // 스크롤바 감지 및 클래스 추가
            checkScrollbars();

            // 스크롤 시에도 구분선 너비 조정
            adjustSeparatorWidths();
        }
    }, [checkScrollbars, adjustSeparatorWidths]);

    // 코드 영역 스크롤 동기화 함수 (새로 추가)
    const syncCodeScroll = (leftRef: React.MutableRefObject<HTMLDivElement | null>, rightRef: React.MutableRefObject<HTMLDivElement | null>) => {
        if (!leftRef.current || !rightRef.current) return;

        // 왼쪽 영역 스크롤 위치 동기화 (오른쪽 영역 기준)
        leftRef.current.scrollTop = rightRef.current.scrollTop;

        // 가로 스크롤바 공간 보정
        const hasHorizontalScrollbar = rightRef.current.scrollWidth > rightRef.current.clientWidth;

        // 스크롤바 높이 계산 (브라우저마다 다를 수 있음)
        const scrollbarHeight = hasHorizontalScrollbar ? rightRef.current.offsetHeight - rightRef.current.clientHeight : 0;

        // 마지막 요소까지 정확히 보이도록 패딩 조정
        const extraPadding = Math.max(0, scrollbarHeight);

        if (extraPadding > 0) {
            // 스크롤바가 있을 경우 추가 패딩 적용
            leftRef.current.style.paddingBottom = `${18 + extraPadding}px`;
        } else {
            // 스크롤바가 없을 경우 기본 패딩 유지
            leftRef.current.style.paddingBottom = '18px';
        }
    };

    // 왼쪽 영역 휠 이벤트 처리
    const handleLeftSectionScroll = () => {
        if (infoLeftRef.current && infoRightRef.current) {
            // 왼쪽 영역 스크롤 발생 시 오른쪽 영역 스크롤 동기화
            infoRightRef.current.scrollTop = infoLeftRef.current.scrollTop;
        }
    };

    // 지표 정보 영역 왼쪽 휠 이벤트 처리 (새로 추가)
    const handleIndicatorLeftSectionScroll = () => {
        if (indicatorInfoLeftRef.current && indicatorInfoRightRef.current) {
            // 지표 정보 영역 왼쪽에서 스크롤 발생 시 오른쪽 영역 스크롤 동기화
            indicatorInfoRightRef.current.scrollTop = indicatorInfoLeftRef.current.scrollTop;
        }
    };

    // 왼쪽 코드 영역 휠 이벤트 처리 (새로 추가)
    const handleLeftCodeSectionScroll = (
        leftRef: React.MutableRefObject<HTMLDivElement | null>,
        rightRef: React.MutableRefObject<HTMLDivElement | null>
    ) => {
        if (leftRef.current && rightRef.current) {
            rightRef.current.scrollTop = leftRef.current.scrollTop;
        }
    };

    // 구분선 참조 설정 함수
    const setSeparatorRef = (idx: number) => (el: HTMLDivElement | null) => {
        separatorRefs.current[idx] = el;
        if (el) adjustSeparatorWidths();
    };

    // 메모이제이션 함수들
    const fetchSourceCode = useCallback(async (path: string | undefined): Promise<string | null> => {
        try {
            const response = await fetch(`/api/get-source-code?filePath=${encodeURIComponent(path || '')}`);
            const data = await response.json();

            if (response.ok) {
                return data.content;
            } else {
                console.error("소스 코드 로드 오류:", data.error);
                return null;
            }
        } catch (error) {
            console.error("소스 코드 로드 오류:", error);
            return null;
        }
    }, []);

    // useEffect 내부 로직 최적화
    useEffect(() => {
        // 컴포넌트 마운트 시 자동으로 전략 소스 코드 로드
        if (sourcePath) {
            fetchSourceCode(sourcePath).then(content => {
                setStrategySource(content);
            });

            if (headerPath) {
                fetchSourceCode(headerPath).then(content => {
                    setStrategyHeader(content);
                });
            }
        }

        // 컴포넌트 마운트 시 첫 번째 지표의 소스 코드 로드
        if (indicators.length > 0 && selectedIndicator) {
            const indicator = indicators.find(i => i.name === selectedIndicator);
            if (indicator?.sourcePath) {
                fetchSourceCode(indicator.sourcePath).then(content => {
                    setIndicatorSource(content);
                });

                if (indicator.headerPath) {
                    fetchSourceCode(indicator.headerPath).then(content => {
                        setIndicatorHeader(content);
                    });
                }
            }
        }
    }, [sourcePath, headerPath, indicators, selectedIndicator, fetchSourceCode]);

    // 전략 정보 데이터 (구성 파일에서 추출된 값 사용)
    const strategyInfo = {
        headerPath: headerPath || "알 수 없음",
        sourcePath: sourcePath || "알 수 없음",
        className: className || "알 수 없음",
        name: name || "알 수 없음"
    };

    // 지표 왼쪽 영역 너비 계산 및 저장 함수 추가
    const calculateIndicatorLeftWidth = useCallback(() => {
        if (!indicatorInfoItemRef.current) return;

        const leftSection = indicatorInfoItemRef.current.querySelector('.info-left') as HTMLElement;
        if (!leftSection) return;

        // 왼쪽 영역의 모든 라벨 요소 가져오기
        const labels = leftSection.querySelectorAll('.info-label');

        // 가장 긴 텍스트를 가진 라벨 찾기
        let maxWidth = 0;
        labels.forEach(label => {
            const labelWidth = (label as HTMLElement).offsetWidth;
            maxWidth = Math.max(maxWidth, labelWidth);
        });

        // 기본 패딩과 여백 고려
        const padding = 32 + 24; // 패딩, 마진, 글머리 기호 등의 공간

        // 너비 계산 및 저장
        indicatorLeftWidthRef.current = maxWidth + padding;
    }, []);

    // 드롭다운 열릴 때 선택된 옵션으로 스크롤
    useEffect(() => {
        if (isDropdownOpen) {
            // DOM이 렌더링된 후 실행
            requestAnimationFrame(() => {
                const selectedMenuItem = document.querySelector('.MuiPaper-root .MuiMenuItem-root.Mui-selected');
                if (selectedMenuItem) {
                    selectedMenuItem.scrollIntoView({block: 'nearest'});
                }
            });
        }
    }, [isDropdownOpen]);

    // 선택된 지표가 변경될 때 구분선 너비 업데이트
    useEffect(() => {
        if (selectedIndicator) {
            setTimeout(() => {
                adjustSeparatorWidths();
            }, 200);
        }
    }, [selectedIndicator]);

    // 초기 애니메이션 상태를 확인하는 useEffect
    useEffect(() => {
        // 컴포넌트가 마운트된 후 초기 애니메이션 상태를 false로 변경
        setInitialAnimation(false);
    }, []);

    // 선택된 지표가 변경될 때 스크롤 위치 초기화 및 너비 리셋
    useEffect(() => {
        if (selectedIndicator) {
            // 지표 변경 시 너비를 리셋하여 깜빡임 방지
            indicatorLeftWidthRef.current = 0;

            // 지표가 변경되면 스크롤 위치를 맨 위로 초기화
            if (infoRightRef.current) {
                infoRightRef.current.scrollTop = 0;
            }
            if (infoLeftRef.current) {
                infoLeftRef.current.scrollTop = 0;
            }
            if (indicatorInfoRightRef.current) {
                indicatorInfoRightRef.current.scrollTop = 0;
            }
            if (indicatorInfoLeftRef.current) {
                indicatorInfoLeftRef.current.scrollTop = 0;
            }
        }
    }, [selectedIndicator]);

    // 컴포넌트 마운트 및 데이터 변경 시 지표 왼쪽 영역 너비 계산
    useEffect(() => {
        if (selectedIndicatorInfo !== null && indicatorSource !== undefined) {
            // 데이터가 로드된 후 약간의 지연을 두고 너비 계산
            setTimeout(() => {
                calculateIndicatorLeftWidth();
            }, 100);
        }
    }, [selectedIndicatorInfo, indicatorSource, calculateIndicatorLeftWidth]);

    // 리사이즈 이벤트에서도 지표 왼쪽 영역 너비 재계산
    useEffect(() => {
        const handleResize = () => {
            calculateIndicatorLeftWidth();
        };

        window.addEventListener('resize', handleResize);
        return () => {
            window.removeEventListener('resize', handleResize);
        };
    }, [calculateIndicatorLeftWidth]);

    // 코드 라인 렌더링 함수
    const renderCodeLines = (code: string | null | undefined, isLoading: boolean = false, isHeader: boolean = false, isIndicator: boolean = false) => {
        if (isLoading) {
            return <div className="card-loading">코드 로딩 중...</div>;
        }

        if (!code) {
            return (
                <div className="no-data-container">
                    <NoDataMessage message="코드를 찾을 수 없습니다"/>
                </div>
            );
        }

        // 코드를 줄 단위로 분리
        const codeLines = code.split('\n');

        // 사용할 ref 결정
        let leftRef: React.MutableRefObject<HTMLDivElement | null>;
        let rightRef: React.MutableRefObject<HTMLDivElement | null>;

        if (isIndicator) {
            if (isHeader) {
                leftRef = indicatorHeaderCodeLeftRef;
                rightRef = indicatorHeaderCodeRightRef;
            } else {
                leftRef = indicatorCodeLeftRef;
                rightRef = indicatorCodeRightRef;
            }
        } else {
            if (isHeader) {
                leftRef = headerCodeLeftRef;
                rightRef = headerCodeRightRef;
            } else {
                leftRef = codeLeftRef;
                rightRef = codeRightRef;
            }
        }

        return (
            <div className="code-container">
                <div
                    className="code-left"
                    ref={leftRef}
                    onScroll={() => handleLeftCodeSectionScroll(leftRef, rightRef)}
                >
                    {codeLines.map((_, idx) => (
                        <div key={idx} className="code-line-number">
                            <span className="line-number">{idx + 1}</span>
                        </div>
                    ))}
                </div>
                <div
                    className="code-right"
                    ref={rightRef}
                    onScroll={() => {
                        syncCodeScroll(leftRef, rightRef);
                        checkScrollbars();
                    }}
                >
                    {codeLines.map((line, idx) => (
                        <div key={idx} className="code-line-content">
                            <SyntaxHighlighter
                                language="cpp"
                                style={oneDark}
                                showLineNumbers={false}
                                customStyle={{
                                    margin: 0,
                                    padding: 0,
                                    backgroundColor: 'transparent',
                                    border: 'none',
                                    width: '100%',
                                    overflow: 'visible',
                                    whiteSpace: 'pre'
                                }}
                                wrapLines={false}
                                wrapLongLines={false}
                            >
                                {line}
                            </SyntaxHighlighter>
                        </div>
                    ))}
                </div>
            </div>
        );
    };

    // 컴포넌트 마운트 시 및 지표 변경 시 config.json에서 지표 정보 가져오기
    const loadIndicatorConfigInfo = useCallback(async (indicatorName: string) => {
        try {
            const response = await fetch('/api/config');
            const config = await response.json();

            // config.json에서 지표 정보 찾기
            const indicatorInfo = config["지표"]?.find((indicator: any) => indicator["지표 이름"] === indicatorName);

            if (indicatorInfo) {
                // 지표 정보 설정
                setSelectedIndicatorInfo({
                    dataPath: indicatorInfo["데이터 경로"],
                    headerPath: indicatorInfo["헤더 파일 경로"],
                    sourcePath: indicatorInfo["소스 파일 경로"],
                    className: indicatorInfo["지표 클래스 이름"],
                    name: indicatorInfo["지표 이름"],
                    timeframe: indicatorInfo["타임프레임"],
                    plot: indicatorInfo["플롯"]
                });

                // 지표 정보가 로드된 후 스크롤 위치를 맨 위로 초기화
                setTimeout(() => {
                    if (indicatorInfoRightRef.current) {
                        indicatorInfoRightRef.current.scrollTop = 0;
                    }
                    if (indicatorInfoLeftRef.current) {
                        indicatorInfoLeftRef.current.scrollTop = 0;
                    }
                }, 100);
            } else {
                console.error(`지표 정보를 찾을 수 없습니다: ${indicatorName}`);
            }
        } catch (error) {
            console.error("config.json 로드 오류:", error);
        }
    }, []);

    useEffect(() => {
        // 선택된 지표가 변경되면 해당 지표의 소스 코드 및 헤더 로드
        if (selectedIndicator) {
            // 이전 지표와 현재 지표를 기록
            prevIndicatorRef.current = prevIndicatorRef.current || null;
            nextIndicatorRef.current = selectedIndicator;

            // 현재 지표를 이전 지표로 저장
            prevIndicatorRef.current = selectedIndicator;

            // config.json에서 지표 정보 로드
            loadIndicatorConfigInfo(selectedIndicator).then();

            const indicator = indicators.find(i => i.name === selectedIndicator);

            if (indicator) {
                if (indicator.sourcePath) {
                    fetchSourceCode(indicator.sourcePath).then(content => {
                        setIndicatorSource(content);
                    });

                    if (indicator.headerPath) {
                        fetchSourceCode(indicator.headerPath).then(content => {
                            setIndicatorHeader(content);
                        });
                    }
                }
            }
        } else {
            prevIndicatorRef.current = null;
            setIndicatorSource(undefined);
            setIndicatorHeader(undefined);
            setSelectedIndicatorInfo(null);
        }
    }, [selectedIndicator, indicators, loadIndicatorConfigInfo]);

    // 컴포넌트 마운트 시 첫 번째 지표 정보 로드
    useEffect(() => {
        if (indicators.length > 0 && selectedIndicator) {
            loadIndicatorConfigInfo(selectedIndicator).then();
        }
    }, [loadIndicatorConfigInfo, indicators, selectedIndicator]);

    // 마운트 및 업데이트 시 가로선 너비 조정
    useEffect(() => {
        // 구분선은 2개만 사용함 (헤더 코드 전, 소스 코드 전)
        separatorRefs.current = Array(2).fill(null);

        // 스크롤 핸들러 함수 정의
        const handleScroll = () => {
            syncScroll();
        };

        // 지표 정보 영역 스크롤 핸들러 함수 정의
        const handleIndicatorScroll = () => {
            syncIndicatorScroll();
        };

        // 너비 조정
        setTimeout(() => {
            adjustSeparatorWidths();
            syncScroll();
        }, 100);

        // 스크롤 동기화 이벤트 리스너 등록
        const rightSection = infoRightRef.current;
        const indicatorRightSection = indicatorInfoRightRef.current;
        
        if (rightSection) {
            rightSection.addEventListener('scroll', handleScroll);
        }
        
        if (indicatorRightSection) {
            indicatorRightSection.addEventListener('scroll', handleIndicatorScroll);
        }

        // 윈도우 리사이즈 이벤트에도 대응
        const handleResize = () => {
            adjustSeparatorWidths();
            syncScroll();
        };
        window.addEventListener('resize', handleResize);

        return () => {
            window.removeEventListener('resize', handleResize);
            if (rightSection) {
                rightSection.removeEventListener('scroll', handleScroll);
            }
            if (indicatorRightSection) {
                indicatorRightSection.removeEventListener('scroll', handleIndicatorScroll);
            }
        };
    }, [syncScroll, syncIndicatorScroll, adjustSeparatorWidths]);

    // 코드 영역 스크롤 동기화 이벤트 리스너 등록 (새로 추가)
    useEffect(() => {
        // 모든 코드 영역에 스크롤 동기화 적용
        const codeRefs: Array<{
            left: React.MutableRefObject<HTMLDivElement | null>,
            right: React.MutableRefObject<HTMLDivElement | null>
        }> = [
            {left: codeLeftRef, right: codeRightRef},
            {left: headerCodeLeftRef, right: headerCodeRightRef},
            {left: indicatorCodeLeftRef, right: indicatorCodeRightRef},
            {left: indicatorHeaderCodeLeftRef, right: indicatorHeaderCodeRightRef}
        ];

        // 각 코드 영역 쌍마다 이벤트 리스너 등록
        const cleanups: (() => void)[] = [];

        codeRefs.forEach(({right}) => {
            const rightEl = right.current;
            if (rightEl) {
                const handleScroll = () => {
                    if (right === codeRightRef && codeLeftRef.current) {
                        syncCodeScroll(codeLeftRef, codeRightRef);
                    } else if (right === headerCodeRightRef && headerCodeLeftRef.current) {
                        syncCodeScroll(headerCodeLeftRef, headerCodeRightRef);
                    } else if (right === indicatorCodeRightRef && indicatorCodeLeftRef.current) {
                        syncCodeScroll(indicatorCodeLeftRef, indicatorCodeRightRef);
                    } else if (right === indicatorHeaderCodeRightRef && indicatorHeaderCodeLeftRef.current) {
                        syncCodeScroll(indicatorHeaderCodeLeftRef, indicatorHeaderCodeRightRef);
                    }
                };

                rightEl.addEventListener('scroll', handleScroll);
                cleanups.push(() => rightEl.removeEventListener('scroll', handleScroll));
            }
        });

        // 컴포넌트 언마운트 시 이벤트 리스너 제거
        return () => {
            cleanups.forEach(cleanup => cleanup());
        };
    }, [strategySource, strategyHeader, indicatorSource, indicatorHeader]);

    // 데이터 변경 시 패딩 조정 실행
    useEffect(() => {
        // 데이터 로드 후 약간의 지연을 두고 실행
        if (strategySource !== undefined || strategyHeader !== undefined || indicatorSource !== undefined || indicatorHeader !== undefined || selectedIndicator !== null) {
            setTimeout(() => {
                adjustSeparatorWidths();
                syncScroll();

                // 모든 코드 영역 스크롤 동기화도 실행 (새로 추가)
                if (codeLeftRef.current && codeRightRef.current) {
                    syncCodeScroll(codeLeftRef, codeRightRef);
                }
                if (headerCodeLeftRef.current && headerCodeRightRef.current) {
                    syncCodeScroll(headerCodeLeftRef, headerCodeRightRef);
                }
                if (indicatorCodeLeftRef.current && indicatorCodeRightRef.current) {
                    syncCodeScroll(indicatorCodeLeftRef, indicatorCodeRightRef);
                }
                if (indicatorHeaderCodeLeftRef.current && indicatorHeaderCodeRightRef.current) {
                    syncCodeScroll(indicatorHeaderCodeLeftRef, indicatorHeaderCodeRightRef);
                }

                // info-separator-right 너비 조정
                if (indicatorInfoItemRef.current) {
                    const infoSeparator = indicatorInfoItemRef.current.querySelector('.info-separator-right') as HTMLElement;
                    if (infoSeparator && infoRightRef.current) {
                        const rightScrollWidth = infoRightRef.current.scrollWidth;
                        const rightClientWidth = infoRightRef.current.clientWidth;
                        const width = Math.max(rightScrollWidth, rightClientWidth);
                        infoSeparator.style.width = `${width}px`;
                        infoSeparator.style.minWidth = `${width}px`;
                    }
                }

                // 약간의 지연 후 한 번 더 스크롤 동기화 실행 (레이아웃 완전히 계산된 후)
                setTimeout(() => {
                    if (codeLeftRef.current && codeRightRef.current) {
                        syncCodeScroll(codeLeftRef, codeRightRef);
                    }
                    if (headerCodeLeftRef.current && headerCodeRightRef.current) {
                        syncCodeScroll(headerCodeLeftRef, headerCodeRightRef);
                    }
                    if (indicatorCodeLeftRef.current && indicatorCodeRightRef.current) {
                        syncCodeScroll(indicatorCodeLeftRef, indicatorCodeRightRef);
                    }
                    if (indicatorHeaderCodeLeftRef.current && indicatorHeaderCodeRightRef.current) {
                        syncCodeScroll(indicatorHeaderCodeLeftRef, indicatorHeaderCodeRightRef);
                    }

                    // 한 번 더 info-separator-right 너비 조정
                    adjustSeparatorWidths();
                }, 500);
            }, 300);
        }
    }, [strategySource, strategyHeader, indicatorSource, indicatorHeader, selectedIndicator, syncScroll, adjustSeparatorWidths]);

    // 왼쪽 영역 너비 계산 및 설정 함수 추가
    const setLeftColumnWidth = () => {
        document.querySelectorAll('.info-item').forEach(item => {
            const leftSection = item.querySelector('.info-left') as HTMLElement;
            if (!leftSection) return;

            // 왼쪽 영역의 모든 라벨 요소 가져오기
            const labels = leftSection.querySelectorAll('.info-label');

            // 가장 긴 텍스트를 가진 라벨 찾기
            let maxWidth = 0;
            labels.forEach(label => {
                const labelWidth = (label as HTMLElement).offsetWidth;
                maxWidth = Math.max(maxWidth, labelWidth);
            });

            // 기본 패딩과 여백 고려
            const padding = 32 + 24; // 패딩, 마진, 글머리 기호 등의 공간

            // 너비 설정
            const finalWidth = maxWidth + padding;
            leftSection.style.width = `${finalWidth}px`;
            leftSection.style.minWidth = `${finalWidth}px`;
        });
    };

    // 컴포넌트 마운트 및 데이터 변경 시 왼쪽 영역 너비 설정
    useEffect(() => {
        if (strategySource !== undefined || strategyHeader !== undefined || indicatorSource !== undefined || indicatorHeader !== undefined || selectedIndicator !== null) {
            // 데이터가 로드된 후 약간의 지연을 두고 너비 계산
            setTimeout(() => {
                setLeftColumnWidth();
            }, 300);
        }
    }, [strategySource, strategyHeader, indicatorSource, indicatorHeader, selectedIndicator]);

    // 리사이즈 이벤트에서도 너비 유지
    useEffect(() => {
        window.addEventListener('resize', setLeftColumnWidth);
        return () => {
            window.removeEventListener('resize', setLeftColumnWidth);
        };
    }, []);

    // 데이터 변경 시 스크롤바 감지
    useEffect(() => {
        // 초기 로드 및 데이터 변경 시 스크롤바 확인
        const timer = setTimeout(checkScrollbars, 300);

        // 창 크기 변경 시 스크롤바 확인
        window.addEventListener('resize', checkScrollbars);

        return () => {
            clearTimeout(timer);
            window.removeEventListener('resize', checkScrollbars);
        };
    }, [strategySource, strategyHeader, indicatorSource, indicatorHeader, selectedIndicator, checkScrollbars]);

    // 색상 값이 HEX 형식인 경우 색상 사각형을 렌더링하는 함수
    const renderColorValueWithBox = (value: string) => {
        if (isHexColor(value)) {
            return (
                <div style={{
                    display: 'flex',
                    alignItems: 'center',
                    height: '100%'
                }}>
                    <div
                        style={{
                            width: '16px',
                            height: '16px',
                            backgroundColor: value,
                            marginRight: '8px',
                            border: '1px solid rgba(255, 255, 255, 0.2)',
                            borderRadius: '3px',
                            display: 'inline-block',
                            verticalAlign: 'middle',
                            flexShrink: 0
                        }}
                    />
                    <span style={{
                        display: 'inline-block',
                        verticalAlign: 'middle',
                        lineHeight: '16px'
                    }}>{value}</span>
                </div>
            );
        }

        return value;
    };

    return (
        <motion.div
            className="card-wrapper"
            initial={initialAnimation ?
                {opacity: 0, x: 300, y: 0} :
                {opacity: 0, y: 10}
            }
            animate={{opacity: 1, y: 0, x: 0}}
            transition={{duration: 0.2, ease: "easeOut"}}
            exit="exit"
            ref={containerRef}
            style={{
                overflow: 'hidden',
                backfaceVisibility: 'hidden',
                willChange: 'transform, opacity',
                position: 'relative'
            }}
            onAnimationComplete={() => {
                setTimeout(adjustSeparatorWidths, 200);
                setTimeout(() => {
                    // 지표 정보 영역 처리
                    if (indicatorInfoItemRef.current) {
                        const indicatorRightSection = indicatorInfoItemRef.current.querySelector('.info-right') as HTMLElement;
                        if (indicatorRightSection && indicatorRightSection.scrollWidth > indicatorRightSection.clientWidth) {
                            indicatorRightSection.style.overflowY = 'auto';
                        }
                    }

                    // 전략 정보 영역 처리
                    if (strategyInfoItemRef.current) {
                        const strategyRightSection = strategyInfoItemRef.current.querySelector('.info-right') as HTMLElement;
                        if (strategyRightSection && strategyRightSection.scrollWidth > strategyRightSection.clientWidth) {
                            strategyRightSection.style.overflowY = 'auto';
                        }
                    }

                    // 스크롤바 감지 및 클래스 추가
                    checkScrollbars();
                }, 250);
            }}
        >
            {/* 헤더 부분 */}
            <div className="header">
                <div className="flex items-center" style={{
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: hasIndicators ? 'space-between' : 'flex-start',
                    width: '100%'
                }}>
                    <div className="header-name">{name}</div>
                    {/* 지표 드롭다운 - 지표가 있을 때만 렌더링 */}
                    {hasIndicators && (
                        <FormControl variant="outlined" size="small" sx={{minWidth: 120, marginRight: '12px'}}>
                            <InputLabel id="indicator-label" sx={{
                                color: 'white',
                                fontSize: '16px',
                                fontWeight: 400,
                                fontFamily: "'Inter', 'Pretendard', sans-serif",
                                '&.Mui-focused': {
                                    color: isDropdownOpen ? 'rgba(255, 215, 0, 0.8)' : 'white',
                                }
                            }}>지표</InputLabel>
                            <Select
                                labelId="indicator-label"
                                value={selectedIndicator || ''}
                                onChange={(event: SelectChangeEvent) => {
                                    const newIndicator = event.target.value;

                                    setSelectedIndicator(newIndicator);
                                }}
                                onOpen={() => setIsDropdownOpen(true)}
                                onClose={() => setIsDropdownOpen(false)}
                                label="지표"
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
                                    disablePortal: false,
                                    PaperProps: {
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
                                {indicators.map((indicator, idx) => (
                                    <MenuItem key={idx} value={indicator.name}>
                                        <div style={{display: 'flex', alignItems: 'center'}}>
                                            <span>{indicator.name}</span>
                                        </div>
                                    </MenuItem>
                                ))}
                            </Select>
                        </FormControl>
                    )}
                </div>
            </div>

            {/* 좌우 분할 레이아웃 */}
            <div className="card-layout">
                {/* 왼쪽 카드 - 전략 정보 */}
                <div className={hasIndicators ? "card-left" : "card-left card-full-width"}>
                    <motion.div
                        className="card-container"
                        initial={{opacity: 0}}
                        animate={{opacity: 1}}
                        transition={{duration: 0.3}}
                    >
                        <div className="card-border"/>
                        <motion.div
                            className="card-header"
                            initial={{opacity: 0}}
                            animate={{opacity: 1}}
                            transition={{duration: 0.2, ease: "easeOut"}}
                        >
                            전략 정보
                        </motion.div>
                        <div
                            className="card-content info-content"
                            ref={scrollContainerRef}
                            style={{overflow: 'hidden'}}
                            onScroll={() => {
                                // 컨테이너 스크롤 시 높이 및 스크롤바 업데이트
                                adjustSeparatorWidths();
                            }}
                        >
                            {/* 전략 정보 항목 */}
                            <div
                                className="info-item"
                                ref={strategyInfoItemRef}
                            >
                                <div
                                    className="info-left"
                                    ref={infoLeftRef}
                                    onScroll={handleLeftSectionScroll}
                                >
                                    <div className="info-row" style={styles.infoRow}>
                                        <div className="info-row-inner" style={styles.infoRowInner}>
                                            <span className="info-bullet" style={styles.infoBullet}>&bull;</span>
                                            <span className="info-label" style={styles.infoLabel}>헤더 파일 경로</span>
                                        </div>
                                    </div>
                                    <div className="info-row" style={styles.infoRow}>
                                        <div className="info-row-inner" style={styles.infoRowInner}>
                                            <span className="info-bullet" style={styles.infoBullet}>&bull;</span>
                                            <span className="info-label" style={styles.infoLabel}>소스 파일 경로</span>
                                        </div>
                                    </div>
                                    <div className="info-row" style={styles.infoRow}>
                                        <div className="info-row-inner" style={styles.infoRowInner}>
                                            <span className="info-bullet" style={styles.infoBullet}>&bull;</span>
                                            <span className="info-label" style={styles.infoLabel}>전략 클래스 이름</span>
                                        </div>
                                    </div>
                                    <div className="info-row" style={styles.infoRow}>
                                        <div className="info-row-inner" style={styles.infoRowInner}>
                                            <span className="info-bullet" style={styles.infoBullet}>&bull;</span>
                                            <span className="info-label" style={styles.infoLabel}>전략 이름</span>
                                        </div>
                                    </div>
                                </div>

                                <div
                                    className="info-right"
                                    ref={infoRightRef}
                                    onScroll={() => {
                                        adjustSeparatorWidths();
                                        syncScroll();
                                        checkScrollbars();

                                        // info-separator-right 너비 업데이트
                                        if (indicatorInfoItemRef.current) {
                                            const infoSeparator = indicatorInfoItemRef.current.querySelector('.info-separator-right') as HTMLElement;
                                            if (infoSeparator) {
                                                const rightScrollWidth = infoRightRef.current?.scrollWidth || 0;
                                                const rightClientWidth = infoRightRef.current?.clientWidth || 0;
                                                const width = Math.max(rightScrollWidth, rightClientWidth);
                                                infoSeparator.style.width = `${width}px`;
                                                infoSeparator.style.minWidth = `${width}px`;
                                            }
                                        }
                                    }}
                                >
                                    <div className="info-row" style={styles.infoRow}>
                                        <div className="info-row-inner" style={styles.infoRowInner}>
                                            <span className="info-value" style={styles.infoValue}>
                                                {strategyInfo.headerPath ?
                                                    (isHexColor(strategyInfo.headerPath) ?
                                                        renderColorValueWithBox(strategyInfo.headerPath) :
                                                        strategyInfo.headerPath) :
                                                    "알 수 없음"}
                                            </span>
                                        </div>
                                    </div>
                                    <div className="info-row" style={styles.infoRow}>
                                        <div className="info-row-inner" style={styles.infoRowInner}>
                                            <span className="info-value" style={styles.infoValue}>
                                                {strategyInfo.sourcePath ?
                                                    (isHexColor(strategyInfo.sourcePath) ?
                                                        renderColorValueWithBox(strategyInfo.sourcePath) :
                                                        strategyInfo.sourcePath) :
                                                    "알 수 없음"}
                                            </span>
                                        </div>
                                    </div>
                                    <div className="info-row" style={styles.infoRow}>
                                        <div className="info-row-inner" style={styles.infoRowInner}>
                                            <span className="info-value" style={styles.infoValue}>
                                                {strategyInfo.className ?
                                                    (isHexColor(strategyInfo.className) ?
                                                        renderColorValueWithBox(strategyInfo.className) :
                                                        strategyInfo.className) :
                                                    "알 수 없음"}
                                            </span>
                                        </div>
                                    </div>
                                    <div className="info-row" style={styles.infoRow}>
                                        <div className="info-row-inner" style={styles.infoRowInner}>
                                            <span className="info-value" style={styles.infoValue}>
                                                {strategyInfo.name ?
                                                    (isHexColor(strategyInfo.name) ?
                                                        renderColorValueWithBox(strategyInfo.name) :
                                                        strategyInfo.name) :
                                                    "알 수 없음"}
                                            </span>
                                        </div>
                                    </div>
                                </div>
                            </div>

                            {/* 첫 번째 가로 구분선 */}
                            <div
                                className="separator"
                                ref={setSeparatorRef(0)}
                            />

                            {/* 헤더 코드 */}
                            <div className="code-wrapper">
                                {strategyHeader === undefined ? (
                                    <div className="card-loading">전략 헤더 코드 로딩 중...</div>
                                ) : strategyHeader === null ? (
                                    <div className="no-data-container">
                                        <NoDataMessage message="전략 헤더 코드를 찾을 수 없습니다." fontSize="18px"/>
                                    </div>
                                ) : (
                                    renderCodeLines(strategyHeader, false, true, false)
                                )}
                            </div>

                            {/* 두 번째 가로 구분선 */}
                            <div
                                className="separator"
                                ref={setSeparatorRef(1)}
                            />

                            {/* 소스 코드 */}
                            <div className="code-wrapper">
                                {strategySource === undefined ? (
                                    <div className="card-loading">전략 소스 코드 로딩 중...</div>
                                ) : strategySource === null ? (
                                    <div className="no-data-container">
                                        <NoDataMessage message="전략 소스 코드를 찾을 수 없습니다." fontSize="18px"/>
                                    </div>
                                ) : (
                                    renderCodeLines(strategySource, false, false, false)
                                )}
                            </div>
                        </div>
                    </motion.div>
                </div>

                {/* 오른쪽 카드 - 지표 (지표가 있을 때만 렌더링) */}
                {hasIndicators && (
                    <div className="card-right">
                        <AnimatePresence
                            mode="wait"
                            onExitComplete={() => {
                                setTimeout(() => {
                                    adjustSeparatorWidths();
                                }, 150);
                            }}
                        >
                            <motion.div
                                key={selectedIndicator}
                                className="card-container"
                                initial={{opacity: 0}}
                                animate={{opacity: 1}}
                                exit={{opacity: 0}}
                                transition={{duration: 0.25, ease: "easeOut"}}
                                onAnimationStart={() => {
                                    // 애니메이션 시작 시 너비 리셋
                                    indicatorLeftWidthRef.current = 0;
                                }}
                                onAnimationComplete={() => {
                                    // 애니메이션 완료 후 구분선 너비 조정
                                    setTimeout(adjustSeparatorWidths, 200);

                                    // 지표 정보 영역 높이가 변경되어도 코드 영역이 제대로 표시되도록 조정
                                    setTimeout(() => {
                                        if (indicatorInfoItemRef.current) {
                                            // 지표 정보 영역 아래 요소들 위치 조정
                                            const infoItem = indicatorInfoItemRef.current;
                                            const parent = infoItem.parentElement;
                                            if (parent) {
                                                // 구분선과 코드 영역 찾기
                                                const separators = parent.querySelectorAll('.strategy-indicator-separator');

                                                // 첫 번째 구분선이 지표 정보 영역 바로 아래에 위치하도록 조정
                                                if (separators.length > 0) {
                                                    const firstSeparator = separators[0] as HTMLElement;
                                                    firstSeparator.style.marginTop = '20px';
                                                }
                                            }
                                        }
                                    }, 300);
                                }}
                            >
                                <div className="card-border"/>
                                <motion.div
                                    className="card-header"
                                    initial={{opacity: 0}}
                                    animate={{opacity: 1}}
                                    transition={{duration: 0.2, ease: "easeOut"}}
                                >
                                    지표 정보
                                </motion.div>
                                <div className="card-content" style={{overflow: 'hidden'}}>
                                    <div
                                        className="info-item"
                                        ref={indicatorInfoItemRef}
                                    >
                                        <div
                                            className="info-left"
                                            ref={indicatorInfoLeftRef}
                                            onScroll={handleIndicatorLeftSectionScroll}
                                            style={indicatorLeftWidthRef.current > 0 ? {
                                                width: `${indicatorLeftWidthRef.current}px`,
                                                minWidth: `${indicatorLeftWidthRef.current}px`,
                                                transition: 'width 0.2s ease-out, min-width 0.2s ease-out'
                                            } : {}}
                                        >
                                            {selectedIndicatorInfo && (
                                                <>
                                                    <div className="info-row" style={styles.infoRow}>
                                                        <div className="info-row-inner" style={styles.infoRowInner}>
                                                            <span className="info-bullet"
                                                                  style={styles.infoBullet}>&bull;</span>
                                                            <span className="info-label"
                                                                  style={styles.infoLabel}>데이터 경로</span>
                                                        </div>
                                                    </div>
                                                    <div className="info-row" style={styles.infoRow}>
                                                        <div className="info-row-inner" style={styles.infoRowInner}>
                                                            <span className="info-bullet"
                                                                  style={styles.infoBullet}>&bull;</span>
                                                            <span className="info-label"
                                                                  style={styles.infoLabel}>헤더 파일 경로</span>
                                                        </div>
                                                    </div>
                                                    <div className="info-row" style={styles.infoRow}>
                                                        <div className="info-row-inner" style={styles.infoRowInner}>
                                                            <span className="info-bullet"
                                                                  style={styles.infoBullet}>&bull;</span>
                                                            <span className="info-label"
                                                                  style={styles.infoLabel}>소스 파일 경로</span>
                                                        </div>
                                                    </div>
                                                    <div className="info-row" style={styles.infoRow}>
                                                        <div className="info-row-inner" style={styles.infoRowInner}>
                                                            <span className="info-bullet"
                                                                  style={styles.infoBullet}>&bull;</span>
                                                            <span className="info-label"
                                                                  style={styles.infoLabel}>지표 클래스 이름</span>
                                                        </div>
                                                    </div>
                                                    <div className="info-row" style={styles.infoRow}>
                                                        <div className="info-row-inner" style={styles.infoRowInner}>
                                                            <span className="info-bullet"
                                                                  style={styles.infoBullet}>&bull;</span>
                                                            <span className="info-label"
                                                                  style={styles.infoLabel}>지표 이름</span>
                                                        </div>
                                                    </div>
                                                    <div className="info-row" style={styles.infoRow}>
                                                        <div className="info-row-inner" style={styles.infoRowInner}>
                                                            <span className="info-bullet"
                                                                  style={styles.infoBullet}>&bull;</span>
                                                            <span className="info-label"
                                                                  style={styles.infoLabel}>타임프레임</span>
                                                        </div>
                                                    </div>

                                                    <div className="info-separator-left"
                                                         style={{marginTop: '12px', marginBottom: '15px'}}/>

                                                    {selectedIndicatorInfo.plot && (
                                                        <>
                                                            {Object.keys(selectedIndicatorInfo.plot).map((key, index) => (
                                                                <div key={index} className="info-row"
                                                                     style={styles.infoRow}>
                                                                    <div className="info-row-inner"
                                                                         style={styles.infoRowInner}>
                                                                        <span className="info-bullet"
                                                                              style={styles.infoBullet}>&bull;</span>
                                                                        <span className="info-label"
                                                                              style={styles.infoLabel}>{key}</span>
                                                                    </div>
                                                                </div>
                                                            ))}
                                                        </>
                                                    )}
                                                </>
                                            )}
                                        </div>

                                        <div
                                            className="info-right"
                                            ref={indicatorInfoRightRef}
                                            onScroll={() => {
                                                adjustSeparatorWidths();
                                                syncIndicatorScroll();
                                                checkScrollbars();
                                            }}
                                        >
                                            {selectedIndicatorInfo && (
                                                <>
                                                    <div className="info-row" style={styles.infoRow}>
                                                        <div className="info-row-inner" style={styles.infoRowInner}>
                                                            <span className="info-value" style={styles.infoValue}>
                                                                {selectedIndicatorInfo.dataPath ?
                                                                    (isHexColor(selectedIndicatorInfo.dataPath) ?
                                                                        renderColorValueWithBox(selectedIndicatorInfo.dataPath) :
                                                                        selectedIndicatorInfo.dataPath) :
                                                                    "알 수 없음"}
                                                            </span>
                                                        </div>
                                                    </div>
                                                    <div className="info-row" style={styles.infoRow}>
                                                        <div className="info-row-inner" style={styles.infoRowInner}>
                                                            <span className="info-value" style={styles.infoValue}>
                                                                {selectedIndicatorInfo.headerPath ?
                                                                    (isHexColor(selectedIndicatorInfo.headerPath) ?
                                                                        renderColorValueWithBox(selectedIndicatorInfo.headerPath) :
                                                                        selectedIndicatorInfo.headerPath) :
                                                                    "알 수 없음"}
                                                            </span>
                                                        </div>
                                                    </div>
                                                    <div className="info-row" style={styles.infoRow}>
                                                        <div className="info-row-inner" style={styles.infoRowInner}>
                                                            <span className="info-value" style={styles.infoValue}>
                                                                {selectedIndicatorInfo.sourcePath ?
                                                                    (isHexColor(selectedIndicatorInfo.sourcePath) ?
                                                                        renderColorValueWithBox(selectedIndicatorInfo.sourcePath) :
                                                                        selectedIndicatorInfo.sourcePath) :
                                                                    "알 수 없음"}
                                                            </span>
                                                        </div>
                                                    </div>
                                                    <div className="info-row" style={styles.infoRow}>
                                                        <div className="info-row-inner" style={styles.infoRowInner}>
                                                            <span className="info-value" style={styles.infoValue}>
                                                                {selectedIndicatorInfo.className ?
                                                                    (isHexColor(selectedIndicatorInfo.className) ?
                                                                        renderColorValueWithBox(selectedIndicatorInfo.className) :
                                                                        selectedIndicatorInfo.className) :
                                                                    "알 수 없음"}
                                                            </span>
                                                        </div>
                                                    </div>
                                                    <div className="info-row" style={styles.infoRow}>
                                                        <div className="info-row-inner" style={styles.infoRowInner}>
                                                            <span className="info-value" style={styles.infoValue}>
                                                                {selectedIndicatorInfo.name ?
                                                                    (isHexColor(selectedIndicatorInfo.name) ?
                                                                        renderColorValueWithBox(selectedIndicatorInfo.name) :
                                                                        selectedIndicatorInfo.name) :
                                                                    "알 수 없음"}
                                                            </span>
                                                        </div>
                                                    </div>
                                                    <div className="info-row" style={styles.infoRow}>
                                                        <div className="info-row-inner" style={styles.infoRowInner}>
                                                            <span className="info-value" style={styles.infoValue}>
                                                                {selectedIndicatorInfo.timeframe ?
                                                                    (isHexColor(selectedIndicatorInfo.timeframe) ?
                                                                        renderColorValueWithBox(selectedIndicatorInfo.timeframe) :
                                                                        selectedIndicatorInfo.timeframe) :
                                                                    "알 수 없음"}
                                                            </span>
                                                        </div>
                                                    </div>

                                                    <div className="info-separator-right"
                                                         style={{marginTop: '12px', marginBottom: '15px'}}
                                                         ref={(el) => {
                                                             if (el && infoRightRef.current) {
                                                                 setTimeout(() => {
                                                                     if (infoRightRef.current) {
                                                                         const rightWidth = infoRightRef.current.scrollWidth;
                                                                         const visibleWidth = infoRightRef.current.clientWidth;
                                                                         // 스크롤 너비와 컨테이너 너비 중 큰 값을 사용
                                                                         const width = Math.max(rightWidth, visibleWidth);
                                                                         // 가로선이 끊기지 않도록 충분한 너비 설정
                                                                         el.style.width = `${width}px`;
                                                                         el.style.minWidth = `${width}px`;
                                                                         // 패딩이 적용된 경우에도 가로선이 끊기지 않도록 오른쪽 마진 추가
                                                                         el.style.marginRight = '0';
                                                                         el.style.boxSizing = 'border-box';
                                                                     }
                                                                 }, 100);
                                                             }
                                                         }}
                                                    />

                                                    {selectedIndicatorInfo.plot && (
                                                        <>
                                                            {Object.keys(selectedIndicatorInfo.plot).map((key, index) => (
                                                                <div key={index} className="info-row"
                                                                     style={styles.infoRow}>
                                                                    <div className="info-row-inner"
                                                                         style={styles.infoRowInner}>
                                                                        <span className="info-value"
                                                                              style={styles.infoValue}>
                                                                            {selectedIndicatorInfo.plot[key] ?
                                                                                (isHexColor(selectedIndicatorInfo.plot[key]) ?
                                                                                    renderColorValueWithBox(selectedIndicatorInfo.plot[key]) :
                                                                                    selectedIndicatorInfo.plot[key]) :
                                                                                "알 수 없음"}
                                                                        </span>
                                                                    </div>
                                                                </div>
                                                            ))}
                                                        </>
                                                    )}
                                                </>
                                            )}
                                        </div>
                                    </div>

                                    {/* 첫 번째 가로 구분선 */}
                                    <div
                                        className="separator"
                                        ref={setSeparatorRef(0)}
                                    />

                                    {/* 지표 헤더 코드 */}
                                    <div className="code-wrapper">
                                        {indicatorHeader === undefined ? (
                                            <div className="card-loading">헤더 코드 로딩 중...</div>
                                        ) : indicatorHeader === null ? (
                                            <div className="no-data-container">
                                                <NoDataMessage message="지표 헤더 코드를 찾을 수 없습니다." fontSize="18px"/>
                                            </div>
                                        ) : (
                                            renderCodeLines(indicatorHeader, false, true, true)
                                        )}
                                    </div>

                                    {/* 두 번째 가로 구분선 */}
                                    <div
                                        className="separator"
                                        ref={setSeparatorRef(1)}
                                    />

                                    {/* 지표 소스 코드 */}
                                    <div className="code-wrapper">
                                        {indicatorSource === undefined ? (
                                            <div className="card-loading">소스 코드 로딩 중...</div>
                                        ) : indicatorSource === null ? (
                                            <div className="no-data-container">
                                                <NoDataMessage message="지표 소스 코드를 찾을 수 없습니다." fontSize="18px"/>
                                            </div>
                                        ) : (
                                            renderCodeLines(indicatorSource, false, false, true)
                                        )}
                                    </div>
                                </div>
                            </motion.div>
                        </AnimatePresence>
                    </div>
                )}
            </div>
        </motion.div>
    )
});

export default StrategyIndicatorCard;