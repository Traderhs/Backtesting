import {memo, useCallback, useEffect, useRef, useState} from "react"
import {motion} from "framer-motion"
import {formatDuration, parseDate} from "../Performance/Utils";
import "./EngineCard.css";

interface EngineSettings {
    [key: string]: unknown
}

interface EngineCardProps {
    settings: EngineSettings
}

// 소수점 끝 0 제거하는 함수
const trimEndZeros = (num: number): string => {
    if (Number.isInteger(num)) {
        return num.toString();
    }
    return num.toString().replace(/\.?0+$/, '');
};

// 문자열에서 trailing zeros 제거하는 함수
const trimStringEndZeros = (str: string): string => {
    // 숫자, 쉼표, 소수점 추출 (통화 포맷 고려)
    const numericMatch = str.match(/[\d,]+\.?\d*/);
    if (numericMatch) {
        // 쉼표 제거 후 숫자로 변환
        const numStr = numericMatch[0].replace(/,/g, '');
        const num = parseFloat(numStr);
        const trimmed = trimEndZeros(num);

        // 원래 문자열의 포맷 유지 (통화 기호, 퍼센트 등)
        // 다만 숫자 부분은 천단위 쉼표를 포함하도록 포맷
        const parts = String(trimmed).split('.');
        const intPart = parts[0];
        const decPart = parts[1];

        // 음수 처리
        const sign = intPart.startsWith('-') ? '-' : '';
        const absInt = sign ? intPart.slice(1) : intPart;
        const intWithCommas = absInt.replace(/\B(?=(\d{3})+(?!\d))/g, ',');
        const formattedNumber = decPart ? `${sign}${intWithCommas}.${decPart}` : `${sign}${intWithCommas}`;
        return str.replace(/[\d,]+\.?\d*/, formattedNumber);
    }
    return str;
};

// 특정 키들에 대해 trailing zeros 제거가 필요한지 확인하는 함수
const shouldTrimTrailingZeros = (key: string): boolean => {
    const trimKeys = ['초기 자금', '테이커 수수료율', '메이커 수수료율', '테이커 슬리피지율', '메이커 슬리피지율'];
    return trimKeys.includes(key);
};

// 엔진 카드 컴포넌트
const EngineCard = memo(({
                             settings
                         }: EngineCardProps) => {
    const [containerHeight, setContainerHeight] = useState<number | null>(null);
    const rightSectionRef = useRef<HTMLDivElement>(null);
    const leftSectionRef = useRef<HTMLDivElement>(null);
    const scrollContainerRef = useRef<HTMLDivElement>(null);

    // 초기 컨테이너 높이 설정
    useEffect(() => {
        if (scrollContainerRef.current && !containerHeight) {
            setContainerHeight(scrollContainerRef.current.offsetHeight);
        }
    }, [containerHeight]);

    // 스크롤바 높이 조정 및 동기화 함수
    const syncEngineScroll = useCallback(() => {
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

            // 스크롤바 감지 및 클래스 추가
            if (hasHorizontalScrollbar) {
                rightSectionRef.current.classList.add('scrollable-x');
            } else {
                rightSectionRef.current.classList.remove('scrollable-x');
            }
        }
    }, []);

    // 왼쪽 영역 휠 이벤트 처리
    const handleEngineLeftScroll = useCallback(() => {
        if (leftSectionRef.current && rightSectionRef.current) {
            // 왼쪽 영역 스크롤 발생 시 오른쪽 영역 스크롤 동기화
            rightSectionRef.current.scrollTop = leftSectionRef.current.scrollTop;
        }
    }, []);

    // 왼쪽 영역 너비 자동 조정 함수
    const adjustEngineLeftWidth = useCallback(() => {
        if (!leftSectionRef.current) return;

        // 왼쪽 영역의 모든 라벨 요소 가져오기
        const labels = leftSectionRef.current.querySelectorAll('.engine-card-label');

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
        leftSectionRef.current.style.width = `${finalWidth}px`;
        leftSectionRef.current.style.minWidth = `${finalWidth}px`;
    }, []);

    // 마운트 및 업데이트 시 설정
    useEffect(() => {
        // 왼쪽 너비 조정 및 스크롤바 공간 조정
        setTimeout(() => {
            adjustEngineLeftWidth(); // 왼쪽 너비 먼저 조정
            syncEngineScroll();      // 그 다음 스크롤 동기화
        }, 100);

        // 윈도우 리사이즈 이벤트에도 대응
        const handleResize = () => {
            adjustEngineLeftWidth(); // 리사이즈 시에도 너비 재조정
            syncEngineScroll();
        };
        window.addEventListener('resize', handleResize);

        // 스크롤 동기화 이벤트 리스너 등록
        const rightSection = rightSectionRef.current;
        if (rightSection) {
            rightSection.addEventListener('scroll', syncEngineScroll);
        }

        return () => {
            window.removeEventListener('resize', handleResize);
            if (rightSection) {
                rightSection.removeEventListener('scroll', syncEngineScroll);
            }
        };
    }, [syncEngineScroll, adjustEngineLeftWidth]);

    // 애니메이션 변형 객체
    const pageTransition = {
        duration: 0.5, // 500ms
        ease: [0.16, 1, 0.3, 1]
    };

// 엔진 설정 렌더링
    const renderEngineSettings = () => {
        if (!settings || Object.keys(settings).length === 0) return null;

        // 중첩 객체인지 확인하는 함수
        const isNestedObject = (value: any) => {
            return typeof value === "object" && value !== null && !Array.isArray(value);
        };

        // 왼쪽에 표시할 빈 공간 렌더링 (중첩 객체가 있을 때)
        const renderLeftSpacer = (key: string, value: any) => {
            // "심볼 간 바 데이터 중복 검사"만 중첩 객체로 처리
            if (key === "심볼 간 바 데이터 중복 검사" && isNestedObject(value)) {
                // 오른쪽 박스와 동일한 구조로 렌더링 (보이지 않게)
                return (
                    <div className="engine-card-nested-object" style={{visibility: 'hidden', pointerEvents: 'none'}}>
                        <div className="engine-card-nested-left">
                            {Object.keys(value as Record<string, unknown>).map((nestedKey, nestedIndex) => (
                                <div className="engine-card-nested-row" key={`nested-key-${nestedIndex}`}>
                                    <span className="engine-card-nested-key">{nestedKey}</span>
                                </div>
                            ))}
                        </div>
                        <div className="engine-card-separator"/>
                        <div className="engine-card-nested-right">
                            {Object.values(value as Record<string, unknown>).map((nestedValue, nestedIndex) => (
                                <div className="engine-card-nested-row" key={`nested-value-${nestedIndex}`}>
                                    <span className="engine-card-nested-value">{String(nestedValue)}</span>
                                </div>
                            ))}
                        </div>
                    </div>
                );
            }
            return null;
        };

        // '백테스팅 기간' 커스텀 렌더링
        const renderValue = (key: string, value: any) => {
            // 백테스팅 기간은 일렬로 표시
            if (key === '백테스팅 기간' && value && typeof value === 'object' && value['시작'] && value['종료']) {
                const startDate = parseDate(value['시작']);
                const endDate = parseDate(value['종료']);
                let rangeStr = '-';
                if (startDate && endDate) {
                    // yyyy-mm-dd hh:mm:ss - yyyy-mm-dd hh:mm:ss
                    const startStr = value['시작'];
                    const endStr = value['종료'];
                    const durationStr = formatDuration(startDate, endDate);
                    rangeStr = `${startStr} - ${endStr} (${durationStr})`;
                }
                return <span className="engine-card-value">{rangeStr}</span>;
            }

            // "심볼 간 바 데이터 중복 검사"만 중첩 객체로 표시
            if (key === "심볼 간 바 데이터 중복 검사" && typeof value === "object" && value !== null && !Array.isArray(value)) {
                return (
                    <div className="engine-card-nested-object">
                        <div className="engine-card-nested-left">
                            {Object.keys(value as Record<string, unknown>).map((nestedKey, nestedIndex) => (
                                <div className="engine-card-nested-row" key={`nested-key-${nestedIndex}`}>
                                    <span className="engine-card-nested-key">{nestedKey}</span>
                                </div>
                            ))}
                        </div>
                        <div className="engine-card-separator"/>
                        <div className="engine-card-nested-right">
                            {Object.values(value as Record<string, unknown>).map((nestedValue, nestedIndex) => (
                                <div className="engine-card-nested-row" key={`nested-value-${nestedIndex}`}>
                                    <span className="engine-card-nested-value">{String(nestedValue)}</span>
                                </div>
                            ))}
                        </div>
                    </div>
                );
            }

            // 숫자인 경우 소수점 처리
            if (typeof value === 'number') {
                return <span className="engine-card-value">{trimEndZeros(value)}</span>;
            }
            // 문자열인 경우 특정 키들에 대해 trailing zeros 제거
            if (typeof value === 'string' && shouldTrimTrailingZeros(key)) {
                return <span className="engine-card-value">{trimStringEndZeros(value)}</span>;
            }
            // 기본
            return <span className="engine-card-value">{String(value)}</span>;
        };

        return (
            <motion.div
                className="engine-card-container"
                ref={scrollContainerRef}
                initial={{opacity: 0}}
                animate={{opacity: 1}}
                transition={pageTransition}
            >
                <div className="engine-card-border"/>
                <motion.div
                    className="engine-card-header"
                    initial={{opacity: 0}}
                    animate={{opacity: 1}}
                    transition={pageTransition}
                >
                    엔진 설정
                </motion.div>
                <motion.div
                    className="engine-card-content"
                    initial={{opacity: 0}}
                    animate={{opacity: 1}}
                    transition={pageTransition}
                    onAnimationComplete={() => {
                        // 애니메이션 완료 후 너비 조정 및 스크롤바 공간 조정
                        setTimeout(() => {
                            adjustEngineLeftWidth(); // 왼쪽 너비 먼저 조정
                            syncEngineScroll();      // 그 다음 스크롤 동기화
                        }, 100);
                    }}
                >
                    <div
                        className="engine-card-scroll"
                    >
                        <div
                            className="engine-card-left-section"
                            ref={leftSectionRef}
                            onScroll={handleEngineLeftScroll}
                        >
                            {Object.entries(settings).map(([key, value], index) => (
                                <div className="engine-card-row" key={`key-${index}`}>
                                    <div className="engine-card-row-inner">
                                        <span className="engine-card-bullet">&bull;</span>
                                        <span className="engine-card-label">{key}</span>
                                        {renderLeftSpacer(key, value)}
                                    </div>
                                </div>
                            ))}
                        </div>

                        <div
                            className="engine-card-right-section"
                            ref={rightSectionRef}
                            onScroll={syncEngineScroll}
                        >
                            {Object.entries(settings).map(([key, value], index) => (
                                <div className="engine-card-row" key={`value-${index}`}>
                                    <div className="engine-card-row-inner">
                                        {renderValue(key, value)}
                                    </div>
                                </div>
                            ))}
                        </div>
                    </div>
                </motion.div>
            </motion.div>
        );
    };

    return renderEngineSettings();
});

export default EngineCard;
