import { useState, useEffect, useRef, memo } from "react"
import { motion} from "framer-motion"
import { parseDate, formatDuration } from "../Performance/Utils";
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
        return str.replace(/[\d,]+\.?\d*/, trimmed);
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
    const separatorRefs = useRef<Array<HTMLDivElement | null>>([]);

    // 초기 컨테이너 높이 설정
    useEffect(() => {
        if (scrollContainerRef.current && !containerHeight) {
            setContainerHeight(scrollContainerRef.current.offsetHeight);
        }
    }, [containerHeight]);

    // 스크롤 동기화 함수
    const syncScroll = () => {
        if (rightSectionRef.current && leftSectionRef.current) {
            // 스크롤 위치 동기화
            leftSectionRef.current.scrollTop = rightSectionRef.current.scrollTop;
        }
    };

    // 데이터가 변경되거나 창 크기가 변경될 때 스크롤바 감지
    useEffect(() => {
        // 초기화
        separatorRefs.current = Array(Object.keys(settings).length).fill(null);

        // 너비 조정 및 스크롤바 공간 조정
        setTimeout(() => {
            syncScroll();
        }, 100);

        // 윈도우 리사이즈 이벤트에도 대응
        window.addEventListener('resize', () => {
            syncScroll();
        });

        // 스크롤 동기화 이벤트 리스너 등록
        const rightSection = rightSectionRef.current;
        if (rightSection) {
            rightSection.addEventListener('scroll', syncScroll);
        }

        return () => {
            window.removeEventListener('resize', syncScroll);
            if (rightSection) {
                rightSection.removeEventListener('scroll', syncScroll);
            }
        };
    }, [Object.keys(settings).length]);

    // 애니메이션 변형 객체
    const pageTransition = {
        duration: 0.5, // 500ms
        ease: [0.16, 1, 0.3, 1]
    };

// 엔진 설정 렌더링
const renderEngineSettings = () => {
    if (!settings || Object.keys(settings).length === 0) return null;

    // '백테스팅 기간' 커스텀 렌더링
    const renderValue = (key: string, value: any) => {
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

        // 객체인 경우 처리
        if (typeof value === "object" && value !== null && !Array.isArray(value)) {
            return (
                <div className="engine-card-nested-object">
                    <div className="engine-card-nested-left">
                        {Object.keys(value as Record<string, unknown>).map((nestedKey, nestedIndex) => (
                            <div className="engine-card-nested-row" key={`nested-key-${nestedIndex}`}>
                                <span className="engine-card-nested-key">{nestedKey}</span>
                            </div>
                        ))}
                    </div>
                    <div className="engine-card-separator" />
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
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            transition={pageTransition}
        >
            <div className="engine-card-border" />
            <motion.div
                className="engine-card-header"
                initial={{ opacity: 0 }}
                animate={{ opacity: 1 }}
                transition={pageTransition}
            >
                엔진 설정
            </motion.div>
            <motion.div
                className="engine-card-content"
                initial={{ opacity: 0 }}
                animate={{ opacity: 1 }}
                transition={pageTransition}
                onAnimationComplete={() => {
                    setTimeout(() => {
                        syncScroll();
                    }, 100);
                }}
            >
                <div
                    className="engine-card-scroll"
                >
                    <div 
                        className="engine-card-left-section"
                        ref={leftSectionRef}
                    >
                        {Object.keys(settings).map((key, index) => (
                            <div className="engine-card-row" key={`key-${index}`}>
                                <div className="engine-card-row-inner">
                                    <span className="engine-card-bullet">&bull;</span>
                                    <span className="engine-card-label">{key}</span>
                                </div>
                            </div>
                        ))}
                    </div>

                    <div 
                        className="engine-card-right-section"
                        ref={rightSectionRef}
                        onScroll={() => {
                            syncScroll();
                        }}
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