import React, {useCallback, useEffect, useMemo, useRef, useState} from 'react';
import EquityCurve from './EquityCurve';
import {useTradeFilter} from '@/Components/TradeFilter';
import LoadingSpinner from '@/Components/Common/LoadingSpinner';
import NoDataMessage from '@/Components/Common/NoDataMessage';
import NetProfitLossComparison from './NetProfitLossComparison.tsx';
import HoldingTimePnlDistribution from "./HoldingTimePnlDistribution.tsx";
import SymbolPerformance from "./SymbolPerformance.tsx";

// EquityCurve 메모이제이션
const MemoizedEquityCurve = React.memo(EquityCurve);

// 필터와 연결된 EquityCurve 래퍼 컴포넌트
const FilteredEquityCurveWrapper = React.memo(() => {
    const [_isReady, setIsReady] = useState(false);
    const [isLoading, setIsLoading] = useState(true);

    // 컴포넌트 마운트/언마운트 추적
    const isMountedRef = useRef(true);

    // 필터링된 거래 목록 사용 (App.tsx의 메인 TradeFilterProvider에서)
    const {filteredTrades} = useTradeFilter();

    // 필터링된 거래 ref - 렌더링 최적화
    const filteredTradesRef = useRef(filteredTrades);

    // 마지막 렌더키를 저장하는 ref - 리렌더링 루프 방지
    const lastRenderKeyRef = useRef(`equity-curve-${filteredTrades?.length || 0}`);

    // 필터 변경 감지를 위한 키 생성 - 자금 재계산 시 변경되는 필드도 포함
    const renderKey = useMemo(() => {
        // 거래 수와 자금 필드의 해시로 키 생성
        const currentCapitalSum = filteredTrades?.reduce((sum, t) => sum + (Number(t["현재 자금"]) || 0), 0) || 0;
        const newKey = `equity-curve-${filteredTrades?.length || 0}-${currentCapitalSum}`;

        // 키가 실제로 변경된 경우에만 업데이트
        if (newKey !== lastRenderKeyRef.current) {
            lastRenderKeyRef.current = newKey;
            return newKey;
        }

        return lastRenderKeyRef.current;
    }, [filteredTrades]);

    // 컴포넌트 마운트/언마운트 관리
    useEffect(() => {
        isMountedRef.current = true;

        return () => {
            isMountedRef.current = false;
        };
    }, []);

    // filteredTrades가 변경될 때마다 ref 업데이트
    useEffect(() => {
        // 실제 거래 수가 변경된 경우에만 로그 출력
        if (filteredTradesRef.current?.length !== filteredTrades?.length) {

            filteredTradesRef.current = filteredTrades;
        }
    }, [filteredTrades]);

    // 데이터 로딩 상태 관리 함수 - useCallback으로 최적화
    const updateLoadingState = useCallback(() => {
        // 컴포넌트가 마운트된 상태일 때만 상태 업데이트
        if (!isMountedRef.current) return;

        setIsLoading(true);

        if (Array.isArray(filteredTradesRef.current) && filteredTradesRef.current.length > 0) {
            setIsReady(true);
            setIsLoading(false);
        } else {
            setIsReady(false);
            setIsLoading(false);
        }
    }, []);

    // 데이터 로딩 상태 관리 - filteredTrades.length만 의존성에 추가
    useEffect(() => {
        // 마운트된 상태에서만 타이머 설정
        if (!isMountedRef.current) return;

        const timer = setTimeout(updateLoadingState, 300);
        return () => {
            clearTimeout(timer);
        };
    }, [filteredTrades?.length, updateLoadingState]);

    // 로딩 중이면 스피너 표시
    if (isLoading) {
        return <LoadingSpinner/>;
    }

    // 거래 데이터가 없는 경우
    if (!filteredTrades || filteredTrades.length === 1) {
        return <NoDataMessage message="거래 내역이 존재하지 않습니다."/>;
    }

    // 준비되면 EquityCurve 렌더링 - renderKey로 필터 변경 시만 리렌더링
    return (
        <div style={{
            width: '100%',
            height: '100%',
            minHeight: '400px',
            position: 'relative',
            isolation: 'isolate'
        }}>
            {/* renderKey를 통해 필터 변경 시에만 리렌더링 */}
            <MemoizedEquityCurve key={renderKey} showMaxBalance={true} showDrawdown={true}/>
        </div>
    );
});

// 탭 상태 및 차트 인스턴스에 접근하기 위한 전역 객체
const ChartInstances = {
    // EquityCurve의 차트 인스턴스를 저장하는 필드
    equityChart: null as any,
    // ProfitLossComparison의 차트 인스턴스를 저장하는 필드
    profitLossChart: null as any,
    // HoldingTimePnlDistribution의 차트 인스턴스를 저장하는 필드
    holdingTimePnlChart: null as any,
    // 마지막으로 렌더링된 탭 상태
    lastVisibleTab: null as string | null,
    // EquityCurve 마운트 여부
    equityMounted: false,
    // NetProfitLossComparison 마운트 여부
    profitLossMounted: false,
    // HoldingTimePnlDistribution 마운트 여부
    holdingTimePnlMounted: false
};

// 글로벌에 차트 인스턴스에 접근하기 위한 함수 생성
(window as any).getChartInstances = () => ChartInstances;

// EquityCurve 차트에 접근하기 위한 이벤트 핸들러 (EquityCurve.tsx에서 호출됨)
(window as any).registerEquityChartInstance = (chart: any) => {
    ChartInstances.equityChart = chart;
    ChartInstances.equityMounted = true;
};

// NetProfitLossComparison 차트에 접근하기 위한 이벤트 핸들러
(window as any).registerNetProfitLossChartInstance = (container: any) => {
    ChartInstances.profitLossChart = container;
    ChartInstances.profitLossMounted = true;
};

// HoldingTimePnlDistribution 차트에 접근하기 위한 이벤트 핸들러
(window as any).registerHoldingTimePnlChartInstance = (container: any) => {
    ChartInstances.holdingTimePnlChart = container;
    ChartInstances.holdingTimePnlMounted = true;
};

// 플롯 컴포넌트에 추가할 프로퍼티 정의
interface PlotProps {
    plotType?: string; // Plot 타입 (예: "equity-drawdown")
    config?: any; // config prop 추가
}

// 프로퍼티 없이도 동작하도록 기본값 설정
const Plot: React.FC<PlotProps> = ({plotType = "equity-drawdown", config}) => {
    // 마운트 추적을 위한 state 및 ref
    const [isMounted, setIsMounted] = useState(false);
    const [isLoading, setIsLoading] = useState(true);
    const [equityInitialized, setEquityInitialized] = useState(false);
    const [netProfitLossInitialized, setNetProfitLossInitialized] = useState(false);
    const [holdingTimePnlInitialized, setHoldingTimePnlInitialized] = useState(false);
    const [symbolPerformanceInitialized, setSymbolPerformanceInitialized] = useState(false);

    // 필터링된 거래 목록 사용
    const {filteredTrades} = useTradeFilter();

    // 컴포넌트 마운트 시 로그 출력 및 로딩 처리
    useEffect(() => {
        setIsMounted(true);
        setIsLoading(true);

        // 최소 1초 로딩
        const timer = setTimeout(() => {
            setIsLoading(false);
        }, 1000);

        return () => {
            setIsMounted(false);
            clearTimeout(timer);
        }
    }, []);

    // plotType이 변경될 때 차트 상태 처리
    useEffect(() => {
        // 처음에는 그냥 현재 탭을 설정
        if (!ChartInstances.lastVisibleTab) {
            ChartInstances.lastVisibleTab = plotType;

            // 현재 표시된 컴포넌트가 아직 초기화되지 않았다면 초기화
            if (plotType === 'equity-drawdown' && !equityInitialized) {
                setEquityInitialized(true);
            } else if (plotType === 'profit-loss-comparison' && !netProfitLossInitialized) {
                setNetProfitLossInitialized(true);
            } else if (plotType === 'holding-time-pnl-distribution' && !holdingTimePnlInitialized) {
                setHoldingTimePnlInitialized(true);
            } else if (plotType === 'symbol-performance' && !symbolPerformanceInitialized) {
                setSymbolPerformanceInitialized(true);
            }
            return;
        }

        // 탭이 변경된 경우에만 처리
        if (ChartInstances.lastVisibleTab !== plotType) {
            // 탭이 변경되었을 때 현재 컴포넌트가 아직 초기화되지 않았다면 초기화
            if (plotType === 'equity-drawdown' && !equityInitialized) {
                setEquityInitialized(true);
            } else if (plotType === 'profit-loss-comparison' && !netProfitLossInitialized) {
                setNetProfitLossInitialized(true);
            } else if (plotType === 'holding-time-pnl-distribution' && !holdingTimePnlInitialized) {
                setHoldingTimePnlInitialized(true);
            } else if (plotType === 'symbol-performance' && !symbolPerformanceInitialized) {
                setSymbolPerformanceInitialized(true);
            }

            // 탭 변경 후 약간의 지연시간을 주고 차트 호출
            setTimeout(() => {
                if (plotType === 'equity-drawdown' && ChartInstances.equityChart) {
                    // EquityCurve 차트가 있다면 applyOptions나 fitContent 메소드 호출 (있는 경우에만)
                    try {
                        if (typeof ChartInstances.equityChart.applyOptions === 'function') {
                            // 차트 옵션 다시 적용 (크기 등)
                            ChartInstances.equityChart.applyOptions({
                                width: document.querySelector('#plot-chart-container > div[style*="display: block"]')?.clientWidth || undefined,
                                height: document.querySelector('#plot-chart-container > div[style*="display: block"]')?.clientHeight || undefined,
                            });
                        }

                        // 이미 마운트된 차트에는 표시만 다시 해주기
                        if (typeof ChartInstances.equityChart.timeScale === 'function' &&
                            typeof ChartInstances.equityChart.timeScale().fitContent === 'function') {
                            // 줌 상태 유지를 위해 fitContent는 호출하지 않음
                            // ChartInstances.equityChart.timeScale().fitContent();
                        }
                    } catch (e) {
                        console.error('EquityCurve 차트 업데이트 중 오류:', e);
                    }
                } else if (plotType === 'profit-loss-comparison' && ChartInstances.profitLossChart) {
                    // NetProfitLossComparison 차트의 경우 (Recharts)
                    try {
                        // Recharts는 일반적으로 컨테이너를 통해 관리됨
                        if (ChartInstances.profitLossChart && ChartInstances.profitLossChart.parentElement) {
                            // 여기서는 차트가 컨테이너에 맞게 자동으로 조정됨
                            // 아무 작업도 필요 없음 (체크만 수행)
                        }
                    } catch (e) {
                        console.error('NetProfitLossComparison 차트 업데이트 중 오류:', e);
                    }
                } else if (plotType === 'holding-time-pnl-distribution' && ChartInstances.holdingTimePnlChart) {
                    try {
                        if (ChartInstances.holdingTimePnlChart && ChartInstances.holdingTimePnlChart.parentElement) {
                            // Recharts와 유사한 차트, 리사이즈에 별도 조치 필요 없음
                        }
                    } catch (e) {
                        console.error('HoldingTimePnlDistribution 차트 업데이트 중 오류:', e);
                    }
                }
            }, 50);

            // 마지막 탭 업데이트
            ChartInstances.lastVisibleTab = plotType;
        }
    }, [plotType, equityInitialized, netProfitLossInitialized, holdingTimePnlInitialized, symbolPerformanceInitialized]);

    // 플롯 타입에 따른 제목 반환 함수
    const getPlotTitle = (type: string): string => {
        switch (type) {
            case "equity-drawdown":
                return "자금 & 드로우다운";
            case "profit-loss-comparison":
                return "시간별 순손익 비교";
            case "holding-time-pnl-distribution":
                return "보유 시간 순손익 분포";
            case "symbol-performance":
                return "심볼별 성과 추이";
            default:
                return "분석 그래프"; // 기본 제목
        }
    };

    // UI 렌더링을 메모이제이션
    return useMemo(() => {
        // 거래 데이터가 없는 경우
        if (!filteredTrades || filteredTrades.length === 1) {
            return <NoDataMessage message="거래 내역이 존재하지 않습니다."/>;
        }

        // 로딩 중이면 스피너 표시
        if (isLoading) {
            return <LoadingSpinner/>;
        }

        return (
            <div className="h-full w-full flex flex-col p-4 overflow-y-auto">
                {/* 제목 */}
                <div style={{
                    position: 'relative',
                    marginBottom: '25px',
                    zIndex: 100
                }}>
                    <h2
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
                        {/* 플롯 타입에 따라 동적으로 제목 변경 */}
                        {getPlotTitle(plotType)}
                        {/* 밑줄 */}
                        <span
                            style={{
                                position: 'absolute',
                                bottom: 0,
                                left: 0,
                                right: 0,
                                height: '2px',
                                background: 'rgba(255, 215, 0, 0.4)',
                                width: '100%',
                            }}
                        />
                    </h2>
                </div>

                {/* 에쿼티 곡선 (자금 변화 및 드로우다운) - Overview.tsx 스타일 카드 적용 */}
                <div
                    style={{
                        flex: 1,
                        borderRadius: '8px',
                        overflow: 'visible',
                        background: '#111111',
                        boxShadow: '0 4px 10px rgba(0, 0, 0, 0.3)',
                        border: '1.2px solid rgba(255, 215, 0, 0.4)',
                        marginTop: '8px',
                        marginLeft: '20px',
                        marginRight: '10px',
                        marginBottom: '15px',
                        width: 'calc(100% - 40px)',
                        position: 'relative',
                        minHeight: '500px',
                        display: 'flex',
                    }}
                >
                    <div
                        id="plot-chart-container"
                        style={{
                            padding: '0.5rem',
                            position: 'relative',
                            zIndex: 1,
                            display: 'flex',
                            justifyContent: 'center',
                            alignItems: 'center',
                            marginTop: '30px',
                            marginLeft: '10px',
                            width: '97%',
                            height: '94%',
                            minHeight: '500px'
                        }}
                    >
                        {/* PlotType에 따라 다른 컴포넌트 렌더링 -> display 속성으로 제어 */}
                        {/* 자금 & 드로우다운 */}
                        <div style={{
                            display: plotType === 'equity-drawdown' ? 'block' : 'none',
                            width: '100%',
                            height: '100%'
                        }}>
                            {isMounted && (equityInitialized || plotType === 'equity-drawdown') && (
                                <FilteredEquityCurveWrapper/>
                            )}
                        </div>

                        {/* 시간별 순손익 비교 */}
                        <div style={{
                            display: plotType === 'profit-loss-comparison' ? 'block' : 'none',
                            width: '100%',
                            height: '100%'
                        }}>
                            {isMounted && (netProfitLossInitialized || plotType === 'profit-loss-comparison') && (
                                <NetProfitLossComparison/>
                            )}
                        </div>

                        {/* 보유 시간 순손익 분포 */}
                        <div style={{
                            display: plotType === 'holding-time-pnl-distribution' ? 'block' : 'none',
                            width: '100%',
                            height: '100%'
                        }}>
                            {isMounted && (holdingTimePnlInitialized || plotType === 'holding-time-pnl-distribution') && (
                                <HoldingTimePnlDistribution/>
                            )}
                        </div>

                        {/* 심볼별 성과 추이 */}
                        <div style={{
                            display: plotType === 'symbol-performance' ? 'block' : 'none',
                            width: '100%',
                            height: '100%'
                        }}>
                            {isMounted && (symbolPerformanceInitialized || plotType === 'symbol-performance') && (
                                <SymbolPerformance config={config}/>
                            )}
                        </div>
                    </div>
                </div>
            </div>
        );
    }, [filteredTrades, isMounted, plotType, equityInitialized, netProfitLossInitialized, holdingTimePnlInitialized, symbolPerformanceInitialized, config, isLoading]);
};

export default Plot;
