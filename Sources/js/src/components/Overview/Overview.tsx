'use client'
import React, {useCallback, useEffect, useMemo, useRef, useState} from 'react';
import {motion} from 'framer-motion';
import {useTradeFilter} from '@/components/TradeFilter';
import {formatDollar, formatPercent} from '@/components/Performance/Utils';
import EquityCurve from '@/components/Plot/EquityCurve';
import {calculatePerformanceMetrics} from '@/components/Performance/Report';
import LoadingSpinner from '@/components/Common/LoadingSpinner';
import NoDataMessage from '@/components/Common/NoDataMessage';

// 지표 정보 정의
const metrics = [
    {id: 'totalProfitLoss', title: '순손익'},
    {id: 'totalProfitLossPercent', title: '순손익률'},
    {id: 'mdd', title: 'MDD'},
    {id: 'winRate', title: '승률'},
    {id: 'profitFactor', title: '손익비'},
    {id: 'sharpeRatio', title: '샤프 지수'},
    {id: 'totalTradeCount', title: '진입 횟수'}
];

// 메트릭스 카드 컴포넌트 (별도 컴포넌트로 분리하고 memo로 최적화)
interface MetricsCardProps {
    metric: { id: string; title: string };
    index: number;
    metricsData: any;
}

const MetricsCard = React.memo(({metric, index, metricsData}: MetricsCardProps) => {
    const [fontSize, setFontSize] = useState(27);
    const [displayValue, setDisplayValue] = useState('');
    const valueRef = useRef<HTMLDivElement>(null);

    // 지표 데이터 매핑 함수 (소수점 제거 옵션 추가)
    const getMetricValue = (metricId: string, data: any, removeDecimal = false) => {
        switch (metricId) {
            case 'totalProfitLoss':
                const profitLoss = parseFloat(data.totalProfitLossMetrics.totalProfitLoss);
                if (removeDecimal) {
                    return '$' + Math.round(profitLoss).toLocaleString('en-US');
                }
                return formatDollar(profitLoss);
            case 'totalProfitLossPercent':
                const percent = data.totalProfitLossMetrics.totalProfitLossPercent;
                if (removeDecimal) {
                    const numVal = parseFloat(percent);
                    return isNaN(numVal) ? '0%' : Math.round(numVal).toLocaleString('en-US') + '%';
                }
                return formatPercent(percent);
            case 'mdd':
                const mdd = data.riskRewardMetrics.mdd;
                if (removeDecimal) {
                    const numVal = parseFloat(mdd);
                    return isNaN(numVal) ? '0%' : Math.round(numVal).toLocaleString('en-US') + '%';
                }
                return formatPercent(mdd);
            case 'winRate':
                const winRate = data.riskRewardMetrics.winRate;
                if (removeDecimal) {
                    const numVal = parseFloat(winRate);
                    return isNaN(numVal) ? '0%' : Math.round(numVal).toLocaleString('en-US') + '%';
                }
                return formatPercent(winRate);
            case 'profitFactor':
                const profitFactor = data.riskRewardMetrics.profitFactor;
                if (profitFactor === "∞") return "∞";
                const numVal = parseFloat(profitFactor);
                if (isNaN(numVal)) return '0.00';
                if (removeDecimal) {
                    return Math.round(numVal).toLocaleString('en-US');
                }
                return numVal.toLocaleString('en-US', {
                    minimumFractionDigits: 2,
                    maximumFractionDigits: 2
                });
            case 'sharpeRatio':
                const sharpeRatio = parseFloat(data.riskAdjustedReturnMetrics.sharpeRatio);
                if (isNaN(sharpeRatio)) return '0.00';
                if (removeDecimal) {
                    return Math.round(sharpeRatio).toLocaleString('en-US');
                }
                return sharpeRatio.toLocaleString('en-US', {
                    minimumFractionDigits: 2,
                    maximumFractionDigits: 2
                });
            case 'totalTradeCount':
                const tradeCount = parseInt(data.tradeCountMetrics.totalTradeCount);
                return isNaN(tradeCount) ? '0회' : tradeCount.toLocaleString('en-US') + '회';
            default:
                return '0';
        }
    };

    // 텍스트 크기 조정 함수
    const adjustTextSize = useCallback(() => {
        if (!valueRef.current) return;

        const container = valueRef.current.closest('div[style*="padding: 1.2rem"]');
        if (!container) return;

        // 카드 고정 너비 (패딩 제외한 실제 텍스트 공간)
        const maxWidth = container.clientWidth - 40; // 패딩 2.4rem = 38.4px 정도

        // 원본 값 시도
        let currentValue = getMetricValue(metric.id, metricsData, false);
        let currentFontSize = 27; // Adjusted font size

        // 임시 측정용 엘리먼트 생성
        const measureElement = document.createElement('div');
        measureElement.style.position = 'absolute';
        measureElement.style.visibility = 'hidden';
        measureElement.style.whiteSpace = 'nowrap';
        measureElement.style.fontWeight = '700';
        measureElement.style.fontFamily = getComputedStyle(valueRef.current).fontFamily;
        document.body.appendChild(measureElement);

        // 먼저 원본 값으로 시도
        measureElement.style.fontSize = currentFontSize + 'px';
        measureElement.textContent = currentValue;

        if (measureElement.scrollWidth > maxWidth) {
            // 소수점 제거된 값으로 시도
            const valueWithoutDecimal = getMetricValue(metric.id, metricsData, true);
            measureElement.textContent = valueWithoutDecimal;

            if (measureElement.scrollWidth <= maxWidth) {
                // 소수점만 제거해도 되는 경우
                currentValue = valueWithoutDecimal;
            } else {
                // 소수점 제거해도 안 되면 폰트 크기 점진적 감소 (소수점 제거된 값으로)
                currentValue = valueWithoutDecimal;

                while (measureElement.scrollWidth > maxWidth && currentFontSize > 12) {
                    currentFontSize -= 1;
                    measureElement.style.fontSize = currentFontSize + 'px';
                }
            }
        }

        document.body.removeChild(measureElement);

        setDisplayValue(currentValue);
        setFontSize(currentFontSize);
    }, [metric.id, metricsData]);

    // 메트릭 데이터 변경 시 텍스트 크기 조정
    useEffect(() => {
        adjustTextSize();
    }, [adjustTextSize]);

    // 윈도우 리사이즈 시 텍스트 크기 재조정
    useEffect(() => {
        const handleResize = () => adjustTextSize();
        window.addEventListener('resize', handleResize);
        return () => window.removeEventListener('resize', handleResize);
    }, [adjustTextSize]);

    // 지표 색상 결정 함수
    const getMetricColor = (metricId: string, data: any) => {
        const isProfitPositive = Number(data.totalProfitLossMetrics.totalProfitLoss) >= 0;

        if (metricId === 'totalProfitLoss') {
            return isProfitPositive ? '#4caf50' : '#f23645';
        } else if (metricId === 'totalProfitLossPercent') {
            return isProfitPositive ? '#4caf50' : '#f23645';
        } else if (metricId === 'mdd') {
            return parseFloat(data.riskRewardMetrics.mdd) === 0 ? '#008000' : '#f23645';
        } else {
            return '#ffffff';
        }
    };

    // 애니메이션 variants 정의
    const cardVariants = {
        initial: {opacity: 0, y: 20},
        animate: {
            opacity: 1,
            y: 0,
            transition: {delay: 0.1 * (index + 1), duration: 0.5}
        },
        hover: {
            scale: 1.05,
            // 호버 시 테두리 색상 변경 및 발광 효과 추가 (기존 그림자 유지, 강도 약화)
            borderColor: 'rgba(255, 215, 0, 0.6)', // 투명도 감소
            boxShadow: '0 4px 10px rgba(0, 0, 0, 0.3), 0 0 8px rgba(255, 215, 0, 0.4)', // 그림자 크기 및 투명도 감소
            transition: {duration: 0.15, ease: 'easeInOut'}
        },
        exit: {
            scale: 1,
            // 나갈 때 테두리 색상과 그림자 원래대로 복구 (animate 상태 또는 style의 기본값으로 돌아감)
            borderColor: 'rgba(255, 215, 0, 0.4)',
            boxShadow: '0 4px 10px rgba(0, 0, 0, 0.3)',
            transition: {duration: 0.03, ease: 'easeIn'} // 마우스를 뗐을 때 더 빠른 복귀
        }
    };

    const borderVariants = {
        initial: {opacity: 0},
        hover: {
            opacity: 1, // 투명도만 조절 (그림자는 cardVariants에서 처리)
            scale: 1.05,
            boxShadow: 'none', // 그림자 효과 제거
            transition: {duration: 0.15, ease: 'easeInOut'}
        },
        exit: {
            opacity: 0,
            scale: 1,
            transition: {duration: 0.03, ease: 'easeIn'} // 마우스를 뗐을 때 더 빠른 복귀
        }
    };

    return (
        <div style={{
            position: 'relative',
            padding: '5px', // 패딩 추가하여 카드 주변에 여유 공간 확보
            overflow: 'visible', // 부모 요소에도 overflow visible 적용
            width: '200px', // 카드 컨테이너 너비 고정
            minWidth: '200px',
            maxWidth: '200px'
        }}>
            {/* 호버 효과용 컨테이너 */}
            <motion.div
                variants={cardVariants}
                initial="initial"
                animate="animate"
                whileHover="hover"
                exit="exit"
                style={{
                    position: 'relative',
                    borderRadius: '8px',
                    height: '140px',
                    width: '200px', // 카드 너비 고정
                    minWidth: '200px',
                    maxWidth: '200px',
                    overflow: 'hidden', // 텍스트 오버플로우 방지
                    // 테두리를 여기에 직접 적용 - 카드의 가장 바깥쪽 요소
                    border: '1.2px solid rgba(255, 215, 0, 0.4)',
                    // 그림자로 테두리 강화
                    boxShadow: '0 4px 10px rgba(0, 0, 0, 0.3)',
                    transformOrigin: 'center center',
                    background: '#111111'
                }}
            >
                {/* 카드 내용물 */}
                <div style={{
                    padding: '1.2rem',
                    height: '100%',
                    display: 'flex',
                    flexDirection: 'column',
                    justifyContent: 'space-between',
                    position: 'relative',
                    zIndex: 1,
                    width: '200px', // 카드 너비 고정
                    minWidth: '200px',
                    maxWidth: '200px'
                }}>
                    <div>
                        <h3 style={{
                            fontSize: '22px',
                            fontWeight: 700,
                            color: 'rgba(255,255,255,0.9)',
                            marginBottom: '0.5rem',
                            whiteSpace: 'nowrap',
                            overflow: 'hidden',
                            textOverflow: 'ellipsis'
                        }}>
                            {metric.title}
                        </h3>
                    </div>
                    <div style={{
                        display: 'flex',
                        alignItems: 'center',
                        minHeight: '32px'
                    }}>
                        <div ref={valueRef} style={{
                            fontSize: fontSize + 'px',
                            fontWeight: 700,
                            color: getMetricColor(metric.id, metricsData),
                            whiteSpace: 'nowrap',
                            overflow: 'hidden',
                            textOverflow: 'ellipsis',
                            lineHeight: '1.2',
                            transform: 'translateY(-3px)'
                        }}>
                            {displayValue || getMetricValue(metric.id, metricsData)}
                        </div>
                    </div>
                </div>
            </motion.div>

            {/* 호버 시 활성화되는 외부 테두리 효과 (고정 위치) */}
            <motion.div
                variants={borderVariants}
                initial="initial"
                whileHover="hover"
                exit="exit"
                style={{
                    position: 'absolute',
                    top: 0,
                    left: 0,
                    right: 0,
                    bottom: 0,
                    borderRadius: '8px',
                    pointerEvents: 'none',
                    zIndex: 10,
                    transformOrigin: 'center center'
                }}
            />
        </div>
    );
});

// MetricsGrid 컴포넌트를 분리하여 최적화
interface MetricsGridProps {
    metricsData: any;
}

const MetricsGrid = React.memo(({metricsData}: MetricsGridProps) => {
    return (
        <motion.div
            initial={{opacity: 0}}
            animate={{opacity: 1}}
            transition={{delay: 0.2, duration: 0.6}}
            className="flex gap-1 min-w-max w-full"
            style={{
                display: 'grid',
                gridTemplateColumns: 'repeat(7, 200px)',
                gap: '25px',
                width: 'fit-content',
                overflow: 'visible',
                paddingLeft: '10px' // 왼쪽 여백 추가로 카드 시작점 이동
            }}
        >
            {metrics.map((metric, index) => (
                <MetricsCard
                    key={metric.id}
                    metric={metric}
                    index={index}
                    metricsData={metricsData}
                />
            ))}
        </motion.div>
    );
});

// 필터링된 EquityCurve 래퍼 컴포넌트
const FilteredEquityCurveWrapper = React.memo(() => {
    const [isReady, setIsReady] = useState(false);
    const [isLoading, setIsLoading] = useState(true);

    // 필터링된 거래 목록 사용
    const {filteredTrades} = useTradeFilter();

    // 필터링된 거래 ref - 렌더링 최적화
    const filteredTradesRef = useRef(filteredTrades);

    // 자금 재계산 시 변경되는 필드를 포함한 키 생성 (Hook은 최상위에서 호출)
    const equityCurveKey = useMemo(() => {
        const currentCapitalSum = filteredTrades?.reduce((sum, t) => sum + (Number(t["현재 자금"]) || 0), 0) || 0;
        return `equity-curve-${filteredTrades?.length || 0}-${currentCapitalSum}`;
    }, [filteredTrades]);

    // 컴포넌트 마운트/언마운트 관리
    useEffect(() => {

        return () => {
        };
    }, []);

    // filteredTrades가 변경될 때마다 ref 업데이트
    useEffect(() => {

        filteredTradesRef.current = filteredTrades;
    }, [filteredTrades]);

    // 데이터 로딩 상태 관리 함수 - useCallback으로 최적화
    const updateLoadingState = useCallback(() => {
        setIsLoading(true);

        if (Array.isArray(filteredTradesRef.current) && filteredTradesRef.current.length > 0) {
            setIsReady(true);
            setIsLoading(false);
        } else {
            setIsReady(false);
            setIsLoading(false);
        }
    }, []);

    // 데이터 로딩 상태 관리
    useEffect(() => {
        const timer = setTimeout(updateLoadingState, 300);
        return () => {
            clearTimeout(timer);
        };
    }, [filteredTrades, updateLoadingState]);

    // 로딩 중이면 스피너 표시
    if (isLoading) {
        return <LoadingSpinner/>;
    }

    // 준비되면 EquityCurve 렌더링
    return (
        <div style={{
            width: '100%',
            height: '100%',
            minHeight: '400px',
            position: 'relative',
            isolation: 'isolate'
        }}>
            {isReady && <EquityCurve key={equityCurveKey}/>}
        </div>
    );
});

// Overview 컴포넌트에 onAnimationStateChange prop 추가
interface OverviewProps {
    onAnimationStateChange?: (isAnimating: boolean) => void;
}

const Overview = React.memo(({}: OverviewProps) => {
    const {filteredTrades, loading} = useTradeFilter();
    const [metricsData, setMetricsData] = useState<ReturnType<typeof calculatePerformanceMetrics> | null>(null);
    const [dataLoading, setDataLoading] = useState(true);
    const [isMounted, setIsMounted] = useState(false);
    const [sharpeLoading, setSharpeLoading] = useState(false);
    const [hasEverHadData, setHasEverHadData] = useState(false); // 한번이라도 데이터가 있었는지

    // 거래 목록의 고유 해시값을 생성 (실제 내용이 바뀔 때만 변경됨)
    const tradesHash = useMemo(() => {
        if (!filteredTrades || filteredTrades.length === 0) {
            return 'empty';
        }

        try {
            // 핵심 속성들로만 해시 생성 (자금 재계산 시 변경되는 필드도 포함)
            const essentialData = filteredTrades.map(trade => ({
                id: trade.id,
                entryTime: trade.entryTime,
                exitTime: trade.exitTime,
                profitLoss: trade.profitLoss,
                currentCapital: trade["현재 자금"],  // 자금 재계산 시 변경
                accumulatedProfitLoss: trade["누적 손익"],  // 자금 재계산 시 변경
                maxDrawdown: trade["최고 드로우다운"]  // 자금 재계산 시 변경
            }));
            return JSON.stringify(essentialData);
        } catch {
            return `fallback-${filteredTrades.length}-${Date.now()}`;
        }
    }, [filteredTrades]);

    // 이전 해시값을 저장
    const prevTradesHashRef = useRef<string>('');
    const initialLoadRef = useRef(true);

    // 마운트 상태 추적
    useEffect(() => {
        setIsMounted(true);
        return () => setIsMounted(false);
    }, []);

    // 샤프 지수가 로딩 중인지 확인하는 함수
    const isSharpeLoading = useCallback((data: any) => {
        return data?.riskAdjustedReturnMetrics?.isLoading === true;
    }, []);

    // 데이터 로딩 상태 및 계산 관련 Effect - useCallback으로 최적화
    const calculateMetricsData = useCallback(() => {
        try {
            setDataLoading(true);
            if (filteredTrades && filteredTrades.length > 0) {
                const calculated = calculatePerformanceMetrics(filteredTrades);

                // 샤프 지수 로딩 상태 확인
                const isSharpeStillLoading = isSharpeLoading(calculated);
                setSharpeLoading(isSharpeStillLoading);

                setMetricsData(calculated);

                // 샤프 지수가 로딩 중이면 dataLoading 상태 유지
                if (!isSharpeStillLoading) {
                    setDataLoading(false);
                } else {
                    // 샤프 지수 로딩이 끝났는지 주기적으로 확인
                    const checkSharpeInterval = setInterval(() => {
                        const newCalculated = calculatePerformanceMetrics(filteredTrades);
                        const stillLoading = isSharpeLoading(newCalculated);

                        if (!stillLoading) {
                            clearInterval(checkSharpeInterval);
                            setMetricsData(newCalculated);
                            setSharpeLoading(false);
                            setDataLoading(false);
                        }
                    }, 500); // 0.5초마다 확인

                    // 최대 10초 후에는 무조건 로딩 종료 (안전장치)
                    setTimeout(() => {
                        clearInterval(checkSharpeInterval);
                        setSharpeLoading(false);
                        setDataLoading(false);
                    }, 10000);

                    return () => clearInterval(checkSharpeInterval);
                }
            } else {
                setMetricsData(null);
                setDataLoading(false);
            }
        } catch (error) {
            console.error('Overview 성능 계산 오류:', error);
            setMetricsData(null);
            setDataLoading(false);
        }
    }, [filteredTrades, isSharpeLoading]);

    // tradesHash가 실제로 변경되었을 때만 계산 수행
    useEffect(() => {
        // 초기 로드이거나 loading 상태면 로딩 표시
        if (loading) {
            setDataLoading(true);
            return;
        }

        // 해시값이 이전과 같다면 (거래 목록이 실제로 바뀌지 않았다면) 아무것도 하지 않음
        if (!initialLoadRef.current && tradesHash === prevTradesHashRef.current) {
            return;
        }

        // 해시값이 바뀌었으므로 이전 값 업데이트
        prevTradesHashRef.current = tradesHash;

        // 0.3초 지연으로 불필요한 계산 방지
        const timer = setTimeout(calculateMetricsData, 300);

        // 초기 로드 완료 표시
        if (initialLoadRef.current) {
            initialLoadRef.current = false;
        }

        return () => clearTimeout(timer);
    }, [tradesHash, loading, calculateMetricsData]);

    // metricsData가 있지만 샤프 지수가 로딩 중인 경우에도 로딩 스피너 표시
    useEffect(() => {
        if (sharpeLoading) {
            setDataLoading(true);
        }
    }, [sharpeLoading]);

    // 데이터가 있을 때 추적
    if (metricsData && !hasEverHadData) {
        setHasEverHadData(true);
    }

    // 초기 로딩 중이거나 실제 데이터 로딩 중이면 로딩 스피너 표시
    // hasEverHadData가 false면 아직 초기 로딩 중, true면 필터링으로 인한 상태
    if (dataLoading || sharpeLoading || loading || (!hasEverHadData && !metricsData && !loading && isMounted)) {
        return <LoadingSpinner/>;
    }

    // 데이터가 없거나 에러가 발생한 경우
    if (!metricsData) {
        return <NoDataMessage message={"거래 내역이 존재하지 않습니다."}/>;
    }

    // 실제 렌더링 콘텐츠
    return (
        <div className="h-full w-full flex flex-col p-4 overflow-y-auto">
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
                    전체 요약
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

            {/* 상단 메트릭스 카드 섹션 - 화면 채우기 (z-index 추가) */}
            <div className="mb-10 pb-4" style={{
                position: 'relative',
                zIndex: 1,
                overflow: 'visible',
                padding: '0 0 0 10px', // 왼쪽 패딩만 적용
                maxWidth: '100%'
            }}>
                {/* MetricsGrid에 isCardAnimationPlaying 상태 전달 (필요하다면 하위 컴포넌트에서도 사용 가능) */}
                <MetricsGrid metricsData={metricsData}/>
            </div>

            {/* 하단 차트 섹션 */}
            <motion.div
                initial={{opacity: 0, scale: 0.98}}
                animate={{opacity: 1, scale: 1}}
                transition={{delay: 0.8, duration: 0.5}}
                style={{
                    flex: 1,
                    borderRadius: '8px',
                    overflow: 'visible',
                    background: '#111111',
                    boxShadow: '0 4px 10px rgba(0, 0, 0, 0.3)',
                    border: '1.2px solid rgba(255, 215, 0, 0.4)',
                    marginTop: '10px',
                    marginBottom: '2.5rem',
                    position: 'relative',
                    top: '-15px',
                    minHeight: '500px',
                    margin: '0 20px',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center'
                }}
            >
                <div style={{
                    padding: '0.5rem',
                    position: 'relative',
                    zIndex: 1,
                    display: 'flex',
                    justifyContent: 'center',
                    alignItems: 'center',
                    marginTop: '20px',
                    marginRight: '15px',
                    width: '97%',
                    height: '94%',
                    minHeight: '500px'
                }}>
                    {/* 마운트된 경우에만 EquityCurveWrapper 렌더링 */}
                    {isMounted && <FilteredEquityCurveWrapper/>}
                </div>
            </motion.div>

            <div dangerouslySetInnerHTML={{
                __html: `
        <style>
          @keyframes spin {
            0% { transform: rotate(0deg); }
            100% { transform: rotate(360deg); }
          }
        </style>
      `
            }}/>
        </div>
    );
});

export default Overview;
