import React, {useCallback, useEffect, useRef, useState} from 'react';
import {
    BaselineData,
    BaselineSeries,
    ChartOptions,
    ColorType,
    createChart,
    CrosshairMode,
    DeepPartial,
    IChartApi,
    LineStyle,
    MouseEventParams,
    PriceScaleMode,
    Time,
    LineSeries,
    HistogramSeries
} from 'lightweight-charts';
import {useTradeFilter} from '@/components/TradeFilter';
import NoDataMessage from '@/components/Common/NoDataMessage';

// 가격 포맷 함수 (달러 표시) - 0 미만에서 빈 문자열 반환
const formatPrice = (price: number): string => {
    if (price < 0) return ''; // 0 미만에서 틱 값 숨김

    const formattedPrice = Math.abs(price)
        .toLocaleString(undefined, {minimumFractionDigits: 2, maximumFractionDigits: 2});

    return `$${formattedPrice}`;
};

// 드로우다운 포맷 함수 (% 표시) - 0 미만 또는 100 초과에서 빈 문자열 반환
const formatDrawdown = (value: number): string => {
    if (value < 0 || value > 100) return ''; // 0 미만 또는 100 초과에서 틱 값 숨김

    return `${value.toFixed(2)}%`;
};

// 1. seriesData 구조 변경 (tradeNum 추가)
interface CustomBaselineData extends BaselineData {
    tradeNum?: string; // 툴팁용 거래 번호 추가
    maxBalance?: number; // 최고 자금 필드 추가
    drawdown?: number; // 드로우다운 필드 추가
    maxDrawdown?: number; // 최고 드로우다운 필드 추가
}

interface EquityCurveProps {
    showMaxBalance?: boolean; // 최고 자금 라인 표시 여부
    showDrawdown?: boolean; // 드로우다운 표시 여부
}

const EquityCurve: React.FC<EquityCurveProps> = ({showMaxBalance = false, showDrawdown = false}) => {
    const chartContainerRef = useRef<HTMLDivElement>(null);
    const chartRef = useRef<IChartApi | null>(null);
    const baselineSeriesRef = useRef<any | null>(null); // 타입을 any로 변경하여 우회
    const initialBalanceLineRef = useRef<any | null>(null);
    const maxBalanceLineRef = useRef<any | null>(null); // 최고 자금 라인 ref 추가
    const maxDrawdownLineRef = useRef<any | null>(null); // 최고 드로우다운 라인 ref 추가
    const drawdownSeriesRef = useRef<any | null>(null); // 드로우다운 시리즈 ref 추가
    const drawdownPaneRef = useRef<any | null>(null); // 드로우다운 pane ref 추가
    const timeAxisLabelRef = useRef<HTMLDivElement | null>(null);
    const priceAxisLabelRef = useRef<HTMLDivElement | null>(null);
    const tooltipRef = useRef<HTMLDivElement | null>(null); // 마우스 툴팁을 위한 ref 추가
    const scaleButtonRef = useRef<HTMLDivElement | null>(null); // 로그 스케일 버튼 ref 추가
    const yearGridContainerRef = useRef<HTMLDivElement | null>(null); // 년도 그리드 컨테이너 ref 추가
    const isComponentMounted = useRef(true);
    const seriesDataRef = useRef<CustomBaselineData[]>([]); // seriesData를 위한 ref 추가
    const hasTooltipAppeared = useRef(false); // 툴팁 첫 등장 여부 추적 ref 추가
    const {filteredTrades} = useTradeFilter();
    const filteredTradesRef = useRef(filteredTrades); // filteredTrades를 ref로 저장
    const prevFilteredTradesLengthRef = useRef(filteredTrades.length); // 이전 filteredTrades 길이를 저장하는 ref 추가
    const [isLogScale, setIsLogScale] = useState(false); // 로그 스케일 상태 추가
    const initialDrawdownHeightSet = useRef(false); // 드로우다운 초기 높이 설정 여부 ref 추가

    // 로그 스케일 토글 함수
    const toggleLogScale = useCallback(() => {

        setIsLogScale(prev => {
            const newValue = !prev;

            // 차트가 존재하면 바로 적용
            if (chartRef.current && chartContainerRef.current) {
                try {
                    // 메인 페인(인덱스 0)의 스케일 변경
                    chartRef.current.priceScale('left').applyOptions({
                        mode: newValue ? PriceScaleMode.Logarithmic : PriceScaleMode.Normal,
                    });

                    // 드로우다운 페인(인덱스 1)의 스케일 변경 - showDrawdown이 true일 때만
                    if (showDrawdown && drawdownPaneRef.current) {
                        try {
                            chartRef.current.priceScale('left', 1).applyOptions({
                                mode: newValue ? PriceScaleMode.Logarithmic : PriceScaleMode.Normal,
                            });
                        } catch (error) {
                            console.error('드로우다운 페인 로그 스케일 적용 중 오류:', error);
                        }
                    }
                } catch (error) {
                    console.error('로그 스케일 적용 중 오류:', error);
                }
            }

            // 상태 변경 전에 버튼 스타일 직접 업데이트 (클래스 사용)
            if (scaleButtonRef.current) {
                const scaleButton = scaleButtonRef.current;
                if (newValue) {
                    scaleButton.classList.remove('log-scale-inactive', 'log-scale-hover');
                    scaleButton.classList.add('log-scale-active');
                } else {
                    scaleButton.classList.remove('log-scale-active');
                    scaleButton.classList.add('log-scale-inactive');
                }
            }

            return newValue;
        });

    }, [showDrawdown]);

    // 데이터 변환 로직을 별도 함수로 분리 (중복 제거)
    const processChartData = useCallback(() => {
        try {
            // 초기 자금 계산
            const trades = filteredTradesRef.current;
            const initialTrade = trades.find(trade => trade["거래 번호"] === 0);
            const initialBalance: number = initialTrade
                ? Number(initialTrade["현재 자금"]) || 0
                : (trades.length > 0 ? Number(trades[0]["현재 자금"]) || 0 : 0);

            // 거래 번호 0 제외 및 유효한 청산 시간만 포함 (먼저 필터링하여 validTrades 계산)
            const validTrades = trades.filter(trade => {
                return trade["거래 번호"] !== 0 &&
                    trade["청산 시간"] &&
                    trade["청산 시간"] !== '-';
            });

            // Lightweight Charts용 데이터 포맷 구성
            const seriesData: CustomBaselineData[] = []; // 타입 변경
            const usedTimestamps = new Set<number>();

            const getUniqueTimestamp = (originalTimestamp: number): number => {
                let uniqueTimestamp = originalTimestamp;
                // 이미 사용된 타임스탬프면 약간의 오프셋(0.1초)을 추가
                while (usedTimestamps.has(uniqueTimestamp)) {
                    uniqueTimestamp += 0.1; // 0.1초 추가 (차트상 거의 차이 안 보임)
                }
                usedTimestamps.add(uniqueTimestamp);
                return uniqueTimestamp;
            };

            // 초기 자금 데이터 포인트 추가 로직
            let initialTimestamp: Time | null = null;
            const firstActualTrade = trades.find(trade => trade["거래 번호"] === 1);

            if (firstActualTrade && firstActualTrade["진입 시간"] && firstActualTrade["진입 시간"] !== '-') {
                try {
                    const entryTimeStr = String(firstActualTrade["진입 시간"]);
                    const entryDate = new Date(entryTimeStr);
                    if (!isNaN(entryDate.getTime())) {
                        initialTimestamp = entryDate.getTime() / 1000 as Time;
                    } else {
                        console.error("거래 1번의 잘못된 진입 시간 형식:", entryTimeStr);
                    }
                } catch (e) {
                    console.error("거래 1번 진입 시간 변환 오류:", e);
                }
            } else if (validTrades.length > 0) {
                // 거래 1번 또는 진입 시간이 없으면, 유효한 첫 거래의 청산 시간 사용 (약간 이전으로)
                try {
                    const firstValidExitTimeStr = String(validTrades[0]["청산 시간"]);
                    const firstValidExitDate = new Date(firstValidExitTimeStr);
                    if (!isNaN(firstValidExitDate.getTime())) {
                        initialTimestamp = (firstValidExitDate.getTime() / 1000 - 0.2) as Time; // 0.2초 빼기
                    } else {
                        console.error("첫 유효 거래의 잘못된 청산 시간 형식:", firstValidExitTimeStr);
                    }
                } catch (e) {
                    console.error("Fallback 초기 시간 계산 오류:", e)
                }
            }

            // 유효한 초기 타임스탬프가 있으면 데이터 추가
            if (initialTimestamp !== null) {
                const uniqueInitialTimestamp = getUniqueTimestamp(initialTimestamp as number) as Time;
                // 거래 번호 0번 거래에서 최고 자금 값을 가져옴
                const initialMaxBalance = initialTrade && initialTrade["최고 자금"]
                    ? Number(initialTrade["최고 자금"])
                    : initialBalance; // 없으면 초기 자금과 동일하게 설정

                // 거래 번호 0번 거래에서 드로우다운 값을 가져옴
                const initialDrawdown = initialTrade && initialTrade["드로우다운"]
                    ? Number(initialTrade["드로우다운"])
                    : 0; // 없으면 0으로 설정

                // 거래 번호 0번 거래에서 최고 드로우다운 값을 가져옴
                const initialMaxDrawdown = initialTrade && initialTrade["최고 드로우다운"]
                    ? Number(initialTrade["최고 드로우다운"])
                    : 0; // 없으면 0으로 설정

                seriesData.push({
                    time: uniqueInitialTimestamp,
                    value: initialBalance,
                    tradeNum: '0', // 거래 번호 '0'으로 설정
                    maxBalance: initialMaxBalance, // 최고 자금 정보 추가
                    drawdown: initialDrawdown, // 드로우다운 정보 추가
                    maxDrawdown: initialMaxDrawdown // 최고 드로우다운 정보 추가
                });
            }

            // 각 데이터 추출 (validTrades 사용)
            validTrades.forEach((trade) => {
                try {
                    const exitTime = String(trade["청산 시간"]);
                    const dateObject = new Date(exitTime); // 먼저 Date 객체 생성

                    // 유효한 날짜인지 확인
                    if (isNaN(dateObject.getTime())) {
                        console.error("잘못된 날짜 형식 발견:", exitTime, "해당 거래:", trade);
                        return; // 이 데이터 포인트는 건너뛰기
                    }

                    // 원본 타임스탬프 계산
                    const originalTimestamp = dateObject.getTime() / 1000;

                    // 중복 없는 고유 타임스탬프 가져오기
                    const uniqueTimestamp = getUniqueTimestamp(originalTimestamp) as Time;
                    const balance = Number(trade["현재 자금"]);
                    const tradeNum = String(trade["거래 번호"]); // 거래 번호 가져오기
                    const maxBalance = trade["최고 자금"] ? Number(trade["최고 자금"]) : balance; // 최고 자금 필드 추가
                    const drawdown = trade["드로우다운"] ? Number(trade["드로우다운"]) : 0; // 드로우다운 필드 그대로 사용
                    const maxDrawdown = trade["최고 드로우다운"] ? Number(trade["최고 드로우다운"]) : 0; // 최고 드로우다운 필드 추가

                    seriesData.push({
                        time: uniqueTimestamp,
                        value: balance,
                        tradeNum: tradeNum, // 거래 번호 추가
                        maxBalance: maxBalance, // 최고 자금 추가
                        drawdown: drawdown, // 드로우다운 정보 추가
                        maxDrawdown: maxDrawdown // 최고 드로우다운 정보 추가
                    });
                } catch (e) {
                    // 에러 로그에 원본 시간과 거래 정보 포함
                    console.error("데이터 변환 오류", trade["청산 시간"], trade, e);
                }
            });

            return {seriesData, initialBalance};
        } catch (error) {
            console.error("차트 데이터 처리 중 오류:", error);
            return {seriesData: [], initialBalance: 0};
        }
    }, []);

    // 툴팁 숨김 함수 추가
    const hideTooltips = useCallback(() => {
        if (!timeAxisLabelRef.current || !priceAxisLabelRef.current || !tooltipRef.current) return;

        const timeAxisLabel = timeAxisLabelRef.current;
        const priceAxisLabel = priceAxisLabelRef.current;
        const tooltip = tooltipRef.current;

        timeAxisLabel.style.display = 'none';
        priceAxisLabel.style.display = 'none';
        tooltip.style.opacity = '0';
        tooltip.style.transform = 'translate3d(0, 0, 0) scale(0.95)';

        // 애니메이션 완료 후 visibility 숨김
        setTimeout(() => {
            if (tooltipRef.current) {
                tooltipRef.current.style.visibility = 'hidden';
            }
        }, 200); // 페이드 아웃 애니메이션 시간과 일치시킴
    }, []);

    // 년도 세로 그리드를 그리는 함수
    const drawYearGridLines = useCallback(() => {
        if (!chartRef.current || !chartContainerRef.current || !seriesDataRef.current.length) return;

        // 기존 그리드 컨테이너 제거
        if (yearGridContainerRef.current) {
            yearGridContainerRef.current.remove();
            yearGridContainerRef.current = null;
        }

        const seriesData = seriesDataRef.current;
        const chart = chartRef.current;
        const container = chartContainerRef.current;

        // 년도가 바뀌는 지점 찾기
        const yearChangePoints: { year: number; time: Time }[] = [];
        let lastYear: number | null = null;

        for (const point of seriesData) {
            const date = new Date(Number(point.time) * 1000);
            const year = date.getFullYear();

            if (lastYear !== null && year !== lastYear) {
                yearChangePoints.push({ year, time: point.time });
            }
            lastYear = year;
        }

        // 년도 변경 지점이 없으면 종료
        if (yearChangePoints.length === 0) return;

        // 그리드 라인을 담을 컨테이너 생성
        const gridContainer = document.createElement('div');
        gridContainer.style.position = 'absolute';
        gridContainer.style.top = '0';
        gridContainer.style.left = '0';
        gridContainer.style.width = '100%';
        gridContainer.style.height = '100%';
        gridContainer.style.pointerEvents = 'none';
        gridContainer.style.zIndex = '1';
        container.appendChild(gridContainer);
        yearGridContainerRef.current = gridContainer;

        // y축 너비와 x축 높이 가져오기
        const leftPriceScaleWidth = chart.priceScale('left').width() || 0;
        const timeScaleHeight = chart.timeScale().height() || 0;
        const containerWidth = container.clientWidth;

        // 각 년도 변경 지점에 세로선 그리기
        for (const point of yearChangePoints) {
            const coordinate = chart.timeScale().timeToCoordinate(point.time);
            if (coordinate === null) continue;

            // x축 포함한 실제 좌표
            const actualX = coordinate + leftPriceScaleWidth;

            // y축 영역을 침범하거나 차트 오른쪽을 벗어나면 건너뛰기
            if (actualX < leftPriceScaleWidth || actualX > containerWidth) continue;

            const line = document.createElement('div');
            line.style.position = 'absolute';
            line.style.left = `${actualX}px`;
            line.style.top = '0';
            line.style.width = '1px';
            line.style.height = `calc(100% - ${timeScaleHeight}px)`; // x축 높이만큼 빼기
            line.style.backgroundColor = 'rgba(128, 128, 128, 0.3)';
            line.style.pointerEvents = 'none';
            gridContainer.appendChild(line);
        }
    }, []);

    // 차트 시리즈 제거 함수 추가
    const removeChartSeries = useCallback((chart: IChartApi, seriesRef: React.RefObject<any>, errorMessage: string) => {
        if (!seriesRef.current) return;

        try {
            const series = seriesRef.current;
            if (chart && series && typeof series === 'object') {
                chart.removeSeries(series);
            }
        } catch (error) {
            // 이미 제거된 시리즈인 경우 무시
            console.warn(errorMessage, error);
        }
        // RefObject는 readonly이므로 직접 null 할당 불가
        // seriesRef.current = null;
    }, []);

    // 차트에 시리즈 추가 함수
    const updateChartSeries = useCallback((seriesData: CustomBaselineData[], initialBalance: number) => {
        if (!chartRef.current) return;

        try {
            // 기존 라인 시리즈 제거 (함수화된 코드 사용)
            removeChartSeries(chartRef.current, initialBalanceLineRef, "초기 자금 라인 시리즈 제거 중 오류:");
            removeChartSeries(chartRef.current, maxBalanceLineRef, "최고 자금 라인 시리즈 제거 중 오류:");
            removeChartSeries(chartRef.current, maxDrawdownLineRef, "최고 드로우다운 라인 시리즈 제거 중 오류:");
            removeChartSeries(chartRef.current, drawdownSeriesRef, "드로우다운 시리즈 제거 중 오류:");

            // Ref 업데이트
            seriesDataRef.current = seriesData;

            // 베이스라인 시리즈에 새로운 데이터 설정
            if (baselineSeriesRef.current) {
                baselineSeriesRef.current.setData(seriesData);
            }

            // 초기 자금 라인 다시 추가
            if (seriesData.length > 0) {
                const startTime = seriesData[0].time;
                const endTime = seriesData[seriesData.length - 1].time;
                const initialBalanceLineData = [
                    {time: startTime, value: initialBalance},
                    {time: endTime, value: initialBalance},
                ];

                const initialBalanceLine = chartRef.current.addSeries(LineSeries, {
                    priceScaleId: 'left', // 스케일 ID 지정 (기본 왼쪽 스케일)
                    color: 'rgb(255, 255, 255)',
                    lineWidth: 2,
                    lineStyle: LineStyle.Dashed,
                    lastValueVisible: false,
                    priceLineVisible: false,
                    crosshairMarkerVisible: false, // 십자선 마커 비활성화하기
                });
                initialBalanceLine.setData(initialBalanceLineData);
                initialBalanceLineRef.current = initialBalanceLine;
            }

            // 최고 자금 라인 추가 (showMaxBalance가 true일 때만)
            if (showMaxBalance && seriesData.length > 0) {
                try {
                    // CustomBaselineData의 maxBalance 필드를 사용하여 최고 자금 라인 데이터 생성
                    const maxBalanceLineData = seriesData.map(item => ({
                        time: item.time,
                        value: item.maxBalance !== undefined ? item.maxBalance : item.value
                    })).filter(data => !isNaN(data.value)); // 유효한 데이터만 필터링

                    if (maxBalanceLineData.length > 0) {
                        const maxBalanceLine = chartRef.current.addSeries(LineSeries, {
                            priceScaleId: 'left', // 스케일 ID 지정 (기본 왼쪽 스케일)
                            color: 'rgba(0, 128, 0, 0.9)', // 진한 초록색
                            lineWidth: 2,
                            lastValueVisible: false,
                            priceLineVisible: false,
                            crosshairMarkerVisible: true, // 십자선 마커 활성화
                            priceFormat: { // 가격 포맷 적용
                                type: 'custom',
                                formatter: formatPrice,
                            },
                        });

                        maxBalanceLine.setData(maxBalanceLineData);
                        maxBalanceLineRef.current = maxBalanceLine;
                    }
                } catch (error) {
                    console.error("최고 자금 라인 추가 중 오류:", error);
                }
            }

            // 드로우다운 차트 추가 (showDrawdown이 true일 때만)
            if (showDrawdown && seriesData.length > 0) {
                try {
                    // CustomBaselineData의 drawdown 필드를 사용하여 드로우다운 데이터 생성 (색상 포함)
                    const drawdownData = seriesData.map(item => ({
                        time: item.time,
                        value: item.drawdown !== undefined ? item.drawdown : 0,
                        color: (item.drawdown === 0 || item.drawdown === undefined) ? '#4caf50' : '#f23645' // 0 또는 undefined면 초록색, 아니면 빨간색
                    })).filter(data => !isNaN(data.value)); // 유효한 데이터만 필터링

                    if (drawdownData.length > 0) {
                        // 히스토그램 시리즈 생성 (paneIndex 1에 추가)
                        const drawdownSeries = chartRef.current.addSeries(HistogramSeries, {
                            priceScaleId: 'left', // 왼쪽 스케일 사용
                            priceFormat: {
                                type: 'custom',
                                formatter: formatDrawdown, // 정수 % 포맷 사용
                            },
                            lastValueVisible: false,
                            priceLineVisible: false,
                        }, 1); // paneIndex 1에 추가

                        drawdownSeries.setData(drawdownData); // 색상이 포함된 데이터 설정
                        drawdownSeriesRef.current = drawdownSeries;

                        // 최고 드로우다운 라인 추가
                        const maxDrawdownData = seriesData.map(item => ({
                            time: item.time,
                            value: item.maxDrawdown !== undefined ? item.maxDrawdown : 0
                        })).filter(data => !isNaN(data.value)); // 유효한 데이터만 필터링

                        if (maxDrawdownData.length > 0) {
                            const maxDrawdownLine = chartRef.current.addSeries(BaselineSeries, {
                                baseValue: {type: 'price', price: 1e-14}, // 기준선 0에 매우 가깝게 설정 (과학 표기법)
                                topLineColor: '#a01722',
                                bottomLineColor: '#008000',
                                topFillColor1: 'transparent',
                                topFillColor2: 'transparent',
                                bottomFillColor1: 'transparent',
                                bottomFillColor2: 'transparent',
                                lineWidth: 2,
                                priceScaleId: 'left', // 스케일 ID 지정
                                lastValueVisible: false,
                                priceLineVisible: false,
                                crosshairMarkerVisible: true,
                                priceFormat: {
                                    type: 'custom',
                                    formatter: formatDrawdown, // 정수 % 포맷 사용
                                },
                            }, 1); // paneIndex 1에 추가

                            maxDrawdownLine.setData(maxDrawdownData);
                            maxDrawdownLineRef.current = maxDrawdownLine;
                        }

                        // pane 객체 가져오기
                        const panes = chartRef.current.panes();
                        if (panes.length > 1) {
                            drawdownPaneRef.current = panes[1];

                            // 드로우다운 페인 높이를 초기 설정 시에만 계산하고 설정
                            if (!initialDrawdownHeightSet.current) {
                                const containerHeight = chartContainerRef.current?.clientHeight || 0;
                                const drawdownHeight = Math.max(Math.round(containerHeight * 0.3), 30); // 최소 30px 보장

                                // 디바운스 없이 바로 높이 설정
                                if (drawdownPaneRef.current) {
                                    try {
                                        drawdownPaneRef.current.setHeight(drawdownHeight);
                                    } catch (error) {
                                        console.error('드로우다운 페인 높이 설정 중 오류:', error);
                                    }
                                }
                                initialDrawdownHeightSet.current = true; // 초기 높이 설정 완료 플래그
                            }

                            // 드로우다운 페인의 priceScale 마진 설정
                            chartRef.current.priceScale('left', 1).applyOptions({
                                scaleMargins: {
                                    top: 0.1,
                                    bottom: 0,
                                },
                            });

                            // 타임스케일 마진 설정 (하단 마진 0)
                            chartRef.current.timeScale().applyOptions({
                                borderVisible: true,
                                barSpacing: 8,
                                timeVisible: true,
                                secondsVisible: true,
                                fixLeftEdge: true,
                                fixRightEdge: true,
                                rightOffset: 5,
                            });

                            // 레이아웃 옵션 업데이트
                            chartRef.current.applyOptions({
                                layout: {
                                    background: {type: ColorType.Solid, color: 'transparent'},
                                    textColor: '#ffffff',
                                    fontFamily: "'Inter', 'Pretendard', sans-serif",
                                    fontSize: 14,
                                },
                            });
                        }
                    }
                } catch (error) {
                    console.error("드로우다운 차트 추가 중 오류:", error);
                }
            }

            // 시간 스케일 조정
            chartRef.current.timeScale().fitContent();

            // 년도 그리드 다시 그리기
            drawYearGridLines();
        } catch (error) {
            console.error("차트 시리즈 업데이트 중 오류:", error);
        }
    }, [removeChartSeries, showDrawdown, showMaxBalance, drawYearGridLines]);

    // DOM 요소 제거 함수 추가
    const removeDOMElement = useCallback((container: HTMLDivElement, elementRef: React.RefObject<HTMLDivElement | null>) => {
        if (elementRef.current && container.contains(elementRef.current)) {
            try {
                container.removeChild(elementRef.current);
            } catch (error) {
                // 이미 제거된 요소인 경우 무시
                console.warn('DOM 요소 제거 중 오류 (이미 제거됨):', error);
            }
            // RefObject는 readonly이므로 직접 null 할당 불가
            // elementRef.current = null;
        }
    }, []);

    // filteredTrades가 변경될 때마다 ref 업데이트하고 차트 업데이트
    useEffect(() => {
        // 실제 filteredTrades 데이터가 변경된 경우에만 차트 업데이트
        // 길이 비교는 단순한 검사지만 빠르게 대부분의 변경사항을 감지할 수 있음
        const isFilterChanged = prevFilteredTradesLengthRef.current !== filteredTrades.length;

        // 필터 변경이 있을 때만 차트 업데이트 수행
        if (isFilterChanged) {
            // filteredTrades가 변경될 때 filteredTradesRef를 업데이트
            filteredTradesRef.current = filteredTrades;
            prevFilteredTradesLengthRef.current = filteredTrades.length;

            // 차트가 이미 존재하면 데이터 업데이트
            if (chartRef.current && baselineSeriesRef.current) {
                try {
                    // 데이터 처리하여 차트 업데이트
                    const {seriesData, initialBalance} = processChartData();
                    updateChartSeries(seriesData, initialBalance);
                } catch (error) {
                    console.error("차트 데이터 업데이트 중 오류:", error);
                }
            }
        }
    }, [filteredTrades, processChartData, updateChartSeries]);

    // 크로스헤어 이동 핸들러 - useCallback으로 메모이제이션 (중복 코드 수정)
    const handleCrosshairMove = useCallback((param: MouseEventParams) => {
        if (!isComponentMounted.current || !chartRef.current || !baselineSeriesRef.current || !timeAxisLabelRef.current || !priceAxisLabelRef.current || !tooltipRef.current) return;

        const timeAxisLabel = timeAxisLabelRef.current;
        const priceAxisLabel = priceAxisLabelRef.current;
        const tooltip = tooltipRef.current;

        // param.point (마우스 좌표)가 없으면 숨김
        if (param.point === undefined) {
            hideTooltips();
            return;
        }

        // 마우스 x 좌표로 가장 가까운 데이터 포인트 찾기
        const currentSeriesData = seriesDataRef.current;
        if (!currentSeriesData.length) {
            hideTooltips();
            return;
        }

        // 가장 가까운 데이터 포인트 찾기
        let closestPointIndex = -1;
        let minDistance = Infinity;

        // logical 인덱스가 유효하면 해당 인덱스 사용, 아니면 좌표로 찾기
        if (param.logical !== undefined && param.logical >= 0 && param.logical < currentSeriesData.length) {
            closestPointIndex = param.logical;
        } else {
            // 각 데이터 포인트를 timeScale 좌표로 변환하여 가장 가까운 포인트 찾기
            for (let i = 0; i < currentSeriesData.length; i++) {
                const time = currentSeriesData[i].time;
                if (time) {
                    // time 값을 x 좌표로 변환
                    const coordinate = chartRef.current.timeScale().timeToCoordinate(time);
                    if (coordinate !== null) {
                        const distance = Math.abs(coordinate - param.point.x);
                        if (distance < minDistance) {
                            minDistance = distance;
                            closestPointIndex = i;
                        }
                    }
                }
            }
        }

        if (closestPointIndex === -1) {
            hideTooltips();
            return;
        }

        const pointData = currentSeriesData[closestPointIndex];
        if (!pointData) {
            hideTooltips();
            return;
        }

        // initialBalance 계산 로직 추가 - ref에서 데이터 가져오기
        const trades = filteredTradesRef.current;
        const initialTrade = trades.find(trade => trade["거래 번호"] === 0);
        const initialBalance: number = initialTrade
            ? Number(initialTrade["현재 자금"]) || 0
            : (trades.length > 0 ? Number(trades[0]["현재 자금"]) || 0 : 0);

        // 가장 가까운 데이터 포인트의 x 좌표 계산
        const closestXCoordinate = chartRef.current.timeScale().timeToCoordinate(pointData.time);
        if (closestXCoordinate === null) {
            timeAxisLabel.style.display = 'none';
            return;
        }

        // 커스텀 축 레이블 업데이트
        if (timeAxisLabel && pointData.time) {
            const date = new Date(Number(pointData.time) * 1000);
            // Helper function to pad numbers with leading zero
            const pad = (num: number) => String(num).padStart(2, '0');
            const yy = String(date.getFullYear()); // 연도
            const mm = pad(date.getMonth() + 1); // 월 (0부터 시작하므로 +1)
            const dd = pad(date.getDate()); // 일
            const hh = pad(date.getHours()); // 시
            const mi = pad(date.getMinutes()); // 분
            const ss = pad(date.getSeconds()); // 초

            // 요일 계산 추가
            const days = ['일', '월', '화', '수', '목', '금', '토'];
            const dayOfWeek = days[date.getDay()];

            // 시간 포맷에 요일 추가
            timeAxisLabel.innerHTML = `${yy}-${mm}-${dd} ${hh}:${mi}:${ss} (${dayOfWeek})`;
            timeAxisLabel.style.display = 'block';
            timeAxisLabel.style.bottom = 'auto';

            const container = chartContainerRef.current;
            const timeScaleHeight = chartRef.current?.timeScale().height();

            if (container && timeScaleHeight !== undefined) {
                // 컨테이너 높이에서 시간 축 높이를 빼서 시간 축의 상단 위치 계산
                // showDrawdown 값에 따라 y 오프셋 조정
                const yOffset = showDrawdown ? 1 : 2; // 드로우다운 안 그릴 때 1px 더 내리기
                timeAxisLabel.style.top = `${container.clientHeight - timeScaleHeight + yOffset}px`;
            } else {
                // 혹시 계산 못하면 예전처럼 아래쪽에 두기 ( fallback )
                timeAxisLabel.style.bottom = '8px';
            }

            // 툴팁 너비를 계산하여 데이터 포인트 위치에 맞게 배치
            const labelWidth = timeAxisLabel.offsetWidth;
            const leftPriceScaleWidth = chartRef.current?.priceScale('left').width() || 0;

            // 툴팁 위치 계산 - param.point.x를 사용하여 y축 너비 변화에 대응
            // param.point.x는 차트 영역 내 좌표이므로 y축 너비를 더하고 레이블 중앙 정렬을 위해 labelWidth / 2를 뺌
            const containerForLabel = chartContainerRef.current;
            let labelX = (param.point.x as number) + leftPriceScaleWidth - labelWidth / 2;

            if (containerForLabel) {
                const containerWidth = containerForLabel.clientWidth;
                // 오른쪽 경계에서 label이 넘지 않게 고정
                labelX = Math.min(labelX, containerWidth - labelWidth);
                // 왼쪽 경계도 체크 - 왼쪽 price scale 너비 고려
                labelX = Math.max(labelX, leftPriceScaleWidth);
            }

            timeAxisLabel.style.left = `${labelX}px`;
        }

        if (priceAxisLabel) {
            // 현재 페인의 인덱스 확인 (드로우다운 페인인지 체크)
            const isDrawdownPane = param.paneIndex === 1;

            // 마우스 y 좌표로부터 가격 값을 얻어오기
            let price = null;

            if (isDrawdownPane) {
                // 드로우다운 페인인 경우 (paneIndex: 1)
                if (drawdownSeriesRef.current && param.point) {
                    // 드로우다운 시리즈 참조를 사용해 가격(퍼센트) 값 얻기
                    price = drawdownSeriesRef.current.coordinateToPrice(param.point.y);

                    if (price !== null && price !== undefined) {
                        // 드로우다운용 백분율 포맷 적용
                        priceAxisLabel.innerHTML = formatDrawdown(price);
                        priceAxisLabel.style.color = '#ffffff'; // 드로우다운 글자색도 흰색으로 통일
                    }
                }
            } else {
                // 기본 페인인 경우 (paneIndex: 0 또는 undefined)
                if (baselineSeriesRef.current && param.point) {
                    price = baselineSeriesRef.current.coordinateToPrice(param.point.y);

                    if (price !== null && price !== undefined) {
                        // 기존 달러 포맷 적용
                        priceAxisLabel.innerHTML = formatPrice(price);
                        priceAxisLabel.style.color = '#ffffff'; // 기본 색상으로 복원
                    }
                }
            }

            // 가격 값이 유효한 경우에만 툴팁 업데이트
            if (price !== null && price !== undefined) {
                // y축 툴팁 숨김 조건 추가
                let shouldHide: boolean;

                if (isDrawdownPane) {
                    // 드로우다운 페인: 0 미만 또는 100 초과에서 숨김
                    shouldHide = price < 0 || price > 100;
                } else {
                    // 자금 페인: 0 미만에서 숨김
                    shouldHide = price < 0;
                }

                if (shouldHide) {
                    priceAxisLabel.style.display = 'none';
                } else {
                    priceAxisLabel.style.display = 'block';

                    // Y좌표 계산
                    let yPosition: number = param.point.y as number; // 기본값은 현재 y 좌표 (타입 number로 명시하고 캐스팅)

                    // 드로우다운 페인인 경우 좌표 조정
                    if (isDrawdownPane && chartRef.current && param.point) {
                        try {
                            const panes = chartRef.current.panes();
                            if (panes.length > 1) {
                                // 0번 페인(메인 차트 페인)의 높이 가져오기
                                const mainPaneHeight = panes[0].getHeight();
                                // 드로우다운 페인 내 마우스 Y좌표 기준으로 최종 위치 조정
                                yPosition = mainPaneHeight + (param.point.y as number) + 1; // 페인 구분선 1px가 있기 때문에 y값에 1px 더함
                            }
                        } catch (e) {
                            console.error("툴팁 위치 조정 중 오류:", e);
                        }
                    }

                    priceAxisLabel.style.top = `${yPosition - priceAxisLabel.offsetHeight / 2}px`;
                    priceAxisLabel.style.right = 'auto';

                    const priceScaleWidth = chartRef.current?.priceScale('left').width();
                    if (priceScaleWidth !== undefined) {
                        // 가격 축 너비에서 레이블 너비를 뺌
                        priceAxisLabel.style.left = `${priceScaleWidth - priceAxisLabel.offsetWidth - 2}px`;
                    } else {
                        priceAxisLabel.style.left = '3px';
                    }
                }
            } else {
                // 가격 값을 얻지 못하면 툴팁 숨기기
                priceAxisLabel.style.display = 'none';
            }
        }

        // 마우스를 따라다니는 툴팁 업데이트
        if (tooltip && pointData.tradeNum) {
            // 최고 자금 정보 생성 (showMaxBalance가 true일 때만)
            const maxBalanceHtml = showMaxBalance ? `
        <div style="display: flex; justify-content: space-between; align-items: baseline; margin-bottom: 8px;">
          <span style="color: #aaa; font-size: 13px; padding: 0 6px; margin-right: 8px; position: relative; left: -6px;">최고 자금</span>
          <strong style="color: #008000; font-weight: 600; font-size: 14px;">${'$' + (pointData.maxBalance || pointData.value).toLocaleString(undefined, {
                minimumFractionDigits: 2,
                maximumFractionDigits: 2
            })}</strong>
        </div>
      ` : '';

            // 드로우다운 정보 추가
            const drawdownColor = (pointData.drawdown === 0 || pointData.drawdown === undefined) ? '#4caf50' : '#f23645';
            const drawdownHtml = showDrawdown ? `
        <div style="display: flex; justify-content: space-between; align-items: baseline; margin-bottom: 4px;">
          <span style="color: #aaa; font-size: 13px; padding: 0 6px; margin-right: 8px; position: relative; left: -6px;">드로우다운</span>
          <strong style="color: ${drawdownColor}; font-weight: 600; font-size: 14px;">${
                (pointData.drawdown !== undefined ? Number(pointData.drawdown).toFixed(2) : '0.00') + '%'
            }</strong>
        </div>
      ` : '';

            // 최고 드로우다운 정보 추가
            const maxDrawdownColor = (pointData.maxDrawdown === 0 || pointData.maxDrawdown === undefined) ? '#008000' : '#a01722';
            const maxDrawdownHtml = showDrawdown ? `
        <div style="display: flex; justify-content: space-between; align-items: baseline; margin-bottom: 4px;">
          <span style="color: #aaa; font-size: 13px; padding: 0 6px; margin-right: 8px; position: relative; left: -6px;">최고 드로우다운</span>
          <strong style="color: ${maxDrawdownColor}; font-weight: 600; font-size: 14px;">${
                (pointData.maxDrawdown !== undefined ? Number(pointData.maxDrawdown).toFixed(2) : '0.00') + '%'
            }</strong>
        </div>
      ` : '';

            // 구분선 HTML (최고 자금과 드로우다운 사이)
            const dividerHtml = (showMaxBalance && showDrawdown) ? `
        <div style="border-bottom: 1px solid rgba(255, 215, 0, 0.3); margin-bottom: 8px;"></div>
      ` : '';

            // 내용 업데이트
            tooltip.innerHTML = `
        <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 4px; border-bottom: 1px solid rgba(255, 215, 0, 0.3); padding-bottom: 5px;">
           <strong style="color: #ffffff; font-size: 15px; font-weight: 600;">거래 번호 #${Number(pointData.tradeNum).toLocaleString()}</strong>
        </div>
        <div style="margin: 8px 0 0 0;">
          <div style="display: flex; justify-content: space-between; align-items: baseline; margin-bottom: 4px;">
            <span style="color: #aaa; font-size: 13px; padding: 0 6px; margin-right: 8px; position: relative; left: -6px;">현재 자금</span>
            <strong style="color: ${pointData.value >= initialBalance ? '#4caf50' : '#f23645'}; font-weight: 600; font-size: 14px;">${'$' + pointData.value.toLocaleString(undefined, {
                minimumFractionDigits: 2,
                maximumFractionDigits: 2
            })}</strong>
          </div>
          ${maxBalanceHtml}
          ${dividerHtml}
          ${drawdownHtml}
          ${maxDrawdownHtml}
        </div>
      `;

            // 가장 가까운 데이터 포인트의 위치를 기준으로 툴팁 위치 조정
            // param.point.x를 사용하여 y축 너비 변화에 영향받지 않도록 수정
            const leftPriceScaleWidth = chartRef.current?.priceScale('left').width() || 0;
            const pointX = (param.point.x as number) + leftPriceScaleWidth;
            const pointY = param.point.y;

            requestAnimationFrame(() => {
                if (!isComponentMounted.current || !tooltipRef.current) return;

                // 위치 업데이트 먼저 수행 (십자선 우하단에 표시)
                let finalX = pointX + 25; // 마우스 커서에서 오른쪽으로 이동
                let finalY = pointY + 25; // 마우스 커서에서 아래쪽으로 이동

                // 드로우다운 페인인 경우 Y좌표 조정
                const isDrawdownPane = param.paneIndex === 1;
                if (isDrawdownPane && chartRef.current && param.point) {
                    try {
                        const panes = chartRef.current.panes();
                        if (panes.length > 1) {
                            // 0번 페인(메인 차트 페인)의 높이 가져오기
                            const mainPaneHeight = panes[0].getHeight();
                            // 드로우다운 페인 내 마우스 Y좌표 기준으로 최종 위치 조정
                            finalY = mainPaneHeight + (param.point.y as number) + 25;
                        }
                    } catch (e) {
                        console.error("툴팁 위치 조정 중 오류:", e);
                    }
                }

                // 툴팁이 화면 밖으로 나가지 않도록 위치 조정
                const container = chartContainerRef.current;
                if (container) {
                    const containerRect = container.getBoundingClientRect();
                    const tooltipWidth = tooltip.offsetWidth;
                    const tooltipHeight = tooltip.offsetHeight;

                    // 오른쪽 경계 체크
                    const rightEdge = finalX + tooltipWidth;
                    if (rightEdge > containerRect.width) {
                        finalX = pointX - tooltipWidth - 25; // 오른쪽으로 벗어나면 왼쪽에 표시
                    }

                    // 아래쪽 경계 체크
                    const bottomEdge = finalY + tooltipHeight;
                    const timeScaleHeight = chartRef.current?.timeScale().height() ?? 0;
                    const chartBottomBoundary = containerRect.height - timeScaleHeight;

                    if (bottomEdge > chartBottomBoundary) {
                        // 드로우다운 페인인 경우와 아닌 경우 분리해서 처리
                        if (isDrawdownPane && chartRef.current && param.point) {
                            try {
                                const panes = chartRef.current.panes();
                                if (panes.length > 1) {
                                    const mainPaneHeight = panes[0].getHeight();
                                    // 드로우다운 페인에서는 현재 Y 위치에서 툴팁 높이만큼 위로 이동
                                    finalY = mainPaneHeight + (param.point.y as number) - tooltipHeight - 25;
                                }
                            } catch (e) {
                                console.error("툴팁 위치 조정 중 오류:", e);
                            }
                        } else {
                            // 기본 페인에서는 원래 로직 사용
                            finalY = pointY - tooltipHeight - 25; // 아래로 벗어나면 마우스 위에 표시
                        }
                    }
                }

                // 위치 설정
                tooltip.style.left = `${finalX}px`;
                tooltip.style.top = `${finalY}px`;

                // 트랜지션 동적 설정 (위치 설정 후)
                if (!hasTooltipAppeared.current) {
                    // 첫 등장: top/left 애니메이션 없음
                    tooltip.style.transition = 'opacity 0.2s ease-in-out, transform 0.2s cubic-bezier(0.23, 1, 0.32, 1)';
                    hasTooltipAppeared.current = true; // 첫 등장 플래그 설정 (스타일 적용 직후)
                } else {
                    // 이후 등장: top/left 애니메이션 포함
                    tooltip.style.transition = 'opacity 0.2s ease-in-out, top 0.25s cubic-bezier(0.23, 1, 0.32, 1), left 0.25s cubic-bezier(0.23, 1, 0.32, 1), transform 0.2s cubic-bezier(0.23, 1, 0.32, 1)';
                }

                // 툴팁 표시 및 애니메이션 효과 적용
                tooltip.style.visibility = 'visible'; // visibility를 먼저 변경
                tooltip.style.opacity = '1';
                tooltip.style.transform = 'translate3d(0, 0, 0) scale(1)';
            });
        }

    }, [hideTooltips, showMaxBalance, showDrawdown]);

    // 차트 옵션 생성 함수 추가
    const createChartOptions = useCallback((width: number, height: number, showPanes: boolean = false, useLogScale: boolean = isLogScale): DeepPartial<ChartOptions> => {

        return {
            width,
            height,
            layout: {
                background: {type: ColorType.Solid, color: 'transparent'},
                textColor: '#ffffff',
                fontSize: 14,
                fontFamily: "'Inter', 'Pretendard', sans-serif",
                ...(showPanes ? {
                    panes: {
                        separatorColor: 'rgba(255, 215, 0, 0.3)', // pane 구분선 색상 (테두리와 같게)
                        separatorHoverColor: 'transparent', // hover 색상을 투명하게 설정
                    }
                } : {})
            },
            grid: {
                vertLines: {visible: false},
                horzLines: {visible: false},
            },
            leftPriceScale: {
                borderColor: '#ffffff',
                scaleMargins: {
                    top: 0.1,
                    bottom: 0.1,
                },
                mode: useLogScale ? PriceScaleMode.Logarithmic : PriceScaleMode.Normal,
                borderVisible: true,
                visible: true,
                entireTextOnly: true,
            },
            rightPriceScale: {
                visible: false,
            },
            timeScale: {
                borderColor: '#ffffff',
                timeVisible: true,
                secondsVisible: true,
                borderVisible: true,
                fixLeftEdge: true,
                fixRightEdge: true,
                lockVisibleTimeRangeOnResize: true,
                allowBoldLabels: false,
            },
            crosshair: {
                mode: CrosshairMode.Normal,
                vertLine: {
                    color: 'rgba(255, 215, 0, 0.5)',
                    width: 1,
                    style: LineStyle.Dashed,
                    labelVisible: false,
                    visible: true
                },
                horzLine: {
                    color: 'rgba(255, 215, 0, 0.5)',
                    width: 1,
                    style: LineStyle.Dashed,
                    labelVisible: false,
                    visible: true
                }
            }
        };
    }, []); // 모든 의존성 제거

    // 차트 생성 및 데이터 처리
    useEffect(() => {
        if (!chartContainerRef.current) {
            return;
        }


        isComponentMounted.current = true;
        const container = chartContainerRef.current;
        initialDrawdownHeightSet.current = false; // 차트 재생성 시 플래그 초기화

        let resizeObserver: ResizeObserver | null = null;

        // 크기가 없으면 차트 생성 지연 및 타이머 설정 (수정된 부분)
        const width = container.clientWidth || container.offsetWidth;
        const height = container.clientHeight || container.offsetHeight;


        // 컨테이너 크기가 없으면 타이머로 재시도 추가
        if (!width || !height) {
            const sizeCheckTimer = setTimeout(() => {
                if (isComponentMounted.current) {
                    // 컴포넌트가 여전히 마운트되어 있으면 한 번 더 시도
                    const newWidth = container.clientWidth || container.offsetWidth;
                    const newHeight = container.clientHeight || container.offsetHeight;


                    if (newWidth && newHeight) {
                        // 크기가 이제 있으면 차트 생성을 위해 useEffect 다시 호출
                        const event = new Event('resize');
                        window.dispatchEvent(event);
                    }
                }
            }, 300); // 300ms 후에 다시 시도

            return () => {
                clearTimeout(sizeCheckTimer);
            };
        }

        // 1. 이전 차트 및 DOM 요소 정리
        const cleanUp = () => {
            try {

                if (resizeObserver) {
                    resizeObserver.disconnect();
                    resizeObserver = null;
                }

                const chart = chartRef.current; // 지역 변수로 할당
                if (chart) {

                    try {
                        chart.unsubscribeCrosshairMove(handleCrosshairMove);
                    } catch (error) {
                        console.warn('크로스헤어 이벤트 해제 중 오류 (무시됨):', error);
                    }

                    // 시리즈 제거 함수로 대체
                    try {
                        removeChartSeries(chart, initialBalanceLineRef, "초기 자금 라인 시리즈 제거 중 오류:");
                        removeChartSeries(chart, maxBalanceLineRef, "최고 자금 라인 시리즈 제거 중 오류:");
                        removeChartSeries(chart, maxDrawdownLineRef, "최고 드로우다운 라인 시리즈 제거 중 오류:");
                        removeChartSeries(chart, drawdownSeriesRef, "드로우다운 시리즈 제거 중 오류:");
                    } catch (error) {
                        console.error('차트 시리즈 제거 중 오류:', error);
                    }

                    try {
                        chart.remove();
                        chartRef.current = null;
                        drawdownPaneRef.current = null; // pane도 차트와 함께 제거됨
                    } catch (error) {
                        console.warn('차트 제거 중 오류 (무시됨):', error);
                    }
                }

                // DOM 요소 제거 함수 사용
                if (container) {
                    try {
                        removeDOMElement(container, timeAxisLabelRef);
                        removeDOMElement(container, priceAxisLabelRef);
                        removeDOMElement(container, tooltipRef);
                        removeDOMElement(container, scaleButtonRef);
                        
                        // 년도 그리드 컨테이너 제거
                        if (yearGridContainerRef.current && container.contains(yearGridContainerRef.current)) {
                            container.removeChild(yearGridContainerRef.current);
                            yearGridContainerRef.current = null;
                        }
                    } catch (error) {
                        console.warn('DOM 요소 제거 중 오류 (무시됨):', error);
                    }
                }

                baselineSeriesRef.current = null;
                initialDrawdownHeightSet.current = false; // 정리 시 플래그 초기화
            } catch (error) {
                console.error("차트 정리 중 오류:", error);
            }
        };

        // 이전 차트 정리 부분
        cleanUp();

        try {
            // 공통 데이터 처리 로직 사용
            const {seriesData, initialBalance} = processChartData();

            // 차트 옵션 함수 사용 (panes 포함)
            const chartOptions = createChartOptions(width, height, true, isLogScale);

            // 6. 차트 생성
            const chart = createChart(container, chartOptions);
            chartRef.current = chart;

            // 차트 인스턴스를 전역 객체에 등록
            if (typeof (window as any).registerEquityChartInstance === 'function') {
                (window as any).registerEquityChartInstance(chart);
            }

            // 축 옵션 적용 (눈금 추가)
            chart.priceScale('left').applyOptions({
                ticksVisible: true,
            });
            chart.timeScale().applyOptions({
                ticksVisible: true,
            });

            // 7. Baseline Series 추가
            const baselineSeries = chart.addSeries(BaselineSeries, {
                priceScaleId: 'left', // 스케일 ID 지정 (기본 왼쪽 스케일)
                baseValue: {type: 'price', price: initialBalance},
                topLineColor: '#4caf50', // 초기 자금 이상일 때 초록색
                topFillColor1: 'rgba(76, 175, 80, 0.6)', // 위쪽 영역 그라데이션 시작 색 (선 색 기반)
                topFillColor2: 'rgba(76, 175, 80, 0.05)', // 위쪽 영역 그라데이션 끝 색 (투명하게)
                bottomLineColor: '#f23645', // 초기 자금 미만일 때 빨간색
                bottomFillColor1: 'rgba(242, 54, 69, 0.05)', // 아래쪽 영역 그라데이션 시작 색 (투명하게)
                bottomFillColor2: 'rgba(242, 54, 69, 0.6)', // 아래쪽 영역 그라데이션 끝 색 (선 색 기반)
                lineWidth: 1,
                lastPriceAnimation: 0, // 애니메이션 비활성화
                lastValueVisible: false, // 마지막 값 라벨 숨김
                priceLineVisible: false, // 마지막 값 라인 숨김
                priceFormat: { // 가격 포맷 적용
                    type: 'custom',
                    formatter: formatPrice,
                    minMove: 1, // 최소 이동 단위를 1로 설정 (정수)
                }
            });
            baselineSeriesRef.current = baselineSeries;

            // 8. 데이터 설정 및 시리즈 추가 (공통 함수 사용)
            baselineSeries.setData(seriesData);
            updateChartSeries(seriesData, initialBalance);

            // 9. 커스텀 축 레이블 추가
            const timeAxisLabel = document.createElement('div');
            timeAxisLabel.style.position = 'absolute';
            timeAxisLabel.style.display = 'none';
            timeAxisLabel.style.top = 'auto';
            timeAxisLabel.style.backgroundColor = 'rgba(28, 28, 36, 0.95)';
            timeAxisLabel.style.color = '#ffffff';
            timeAxisLabel.style.padding = '2px 7px 3px 7px';
            timeAxisLabel.style.borderRadius = '3px';
            timeAxisLabel.style.fontSize = '14px';
            timeAxisLabel.style.fontWeight = 'normal';
            timeAxisLabel.style.zIndex = '60';
            timeAxisLabel.style.border = '1px solid rgba(255, 215, 0, 0.4)';
            timeAxisLabel.style.whiteSpace = 'nowrap';
            timeAxisLabel.style.fontFamily = "'Inter', 'Pretendard', sans-serif"; // 폰트 적용
            container.appendChild(timeAxisLabel);
            timeAxisLabelRef.current = timeAxisLabel;

            const priceAxisLabel = document.createElement('div');
            priceAxisLabel.style.position = 'absolute';
            priceAxisLabel.style.display = 'none';
            priceAxisLabel.style.right = 'auto';
            priceAxisLabel.style.backgroundColor = 'rgba(28, 28, 36, 0.95)';
            priceAxisLabel.style.color = '#ffffff';
            priceAxisLabel.style.padding = '2px 7px 3px 7px';
            priceAxisLabel.style.borderRadius = '3px';
            priceAxisLabel.style.fontSize = '14px';
            priceAxisLabel.style.fontWeight = 'normal';
            priceAxisLabel.style.zIndex = '51';
            priceAxisLabel.style.border = '1px solid rgba(255, 215, 0, 0.4)';
            priceAxisLabel.style.whiteSpace = 'nowrap';
            priceAxisLabel.style.fontFamily = "'Inter', 'Pretendard', sans-serif"; // 폰트 적용
            container.appendChild(priceAxisLabel);
            priceAxisLabelRef.current = priceAxisLabel;

            // 마우스를 따라다니는 툴팁 추가
            const tooltip = document.createElement('div');
            tooltip.style.position = 'absolute';
            tooltip.style.padding = '10px 15px 5px 15px';
            tooltip.style.boxSizing = 'border-box';
            tooltip.style.fontSize = '12.5px';
            tooltip.style.color = '#eee';
            tooltip.style.background = 'rgba(28, 28, 36, 0.95)';
            tooltip.style.borderRadius = '6px';
            tooltip.style.border = '1px solid rgba(255, 215, 0, 0.4)';
            tooltip.style.boxShadow = '0 4px 12px rgba(0, 0, 0, 0.6)';
            tooltip.style.pointerEvents = 'none';
            tooltip.style.zIndex = '100';
            tooltip.style.fontFamily = "'Inter', 'Pretendard', sans-serif"; // 폰트 적용 (기존에도 있었지만 명시적으로 다시 설정)
            tooltip.style.lineHeight = '1.6';
            tooltip.style.opacity = '0';
            tooltip.style.visibility = 'hidden'; // 초기에는 완전히 숨김
            tooltip.style.transform = 'translate3d(0, 0, 0) scale(0.95)'; // 하드웨어 가속 활성화 및 초기 약간 작게 시작
            tooltip.style.left = '-9999px'; // 화면 바깥으로 초기 위치 이동
            tooltip.style.top = '-9999px'; // 화면 바깥으로 초기 위치 이동
            tooltip.style.whiteSpace = 'nowrap'; // 줄바꿈 방지 추가
            container.appendChild(tooltip);
            tooltipRef.current = tooltip;

            // 로그 스케일 토글 버튼 추가
            const scaleButton = document.createElement('div');
            scaleButton.style.position = 'absolute';
            scaleButton.style.top = '5px';
            scaleButton.style.right = '5px';
            scaleButton.style.width = '25px';
            scaleButton.style.height = '25px';
            scaleButton.style.display = 'flex';
            scaleButton.style.alignItems = 'center';
            scaleButton.style.justifyContent = 'center';
            scaleButton.style.color = '#fff'; // 항상 흰색 텍스트
            scaleButton.style.border = '1px solid #ffffff'; // 흰색 테두리
            scaleButton.style.borderRadius = '4px';
            scaleButton.style.cursor = 'pointer';
            scaleButton.style.zIndex = '100';
            scaleButton.style.fontSize = '14px';
            scaleButton.style.fontFamily = "'Inter', 'Pretendard', sans-serif";
            scaleButton.style.fontWeight = 'bold';
            scaleButton.style.transition = 'background-color 0.2s ease-in-out, transform 0.1s ease';
            scaleButton.textContent = 'L';

            // CSS 스타일을 head에 추가
            const styleElement = document.createElement('style');
            styleElement.textContent = `
        .log-scale-inactive {
          background-color: transparent !important;
        }
        .log-scale-active {
          background-color: rgba(255, 215, 0, 0.7) !important;
        }
        .log-scale-hover {
          background-color: rgba(255, 215, 0, 0.4) !important;
        }
        /* 스케일 전환 애니메이션 스타일 추가 (제거됨) */
        /*
        #equity-curve-container {
          transition: opacity 0.3s ease-in-out;
        }
        #equity-curve-container.chart-scale-transition {
          opacity: 0.5;
        }
        */
      `;
            document.head.appendChild(styleElement);

            // 초기 클래스 설정
            scaleButton.classList.add(isLogScale ? 'log-scale-active' : 'log-scale-inactive');

            // 호버 효과 추가
            scaleButton.addEventListener('mouseenter', () => {
                if (!isLogScale) {
                    scaleButton.classList.remove('log-scale-inactive');
                    scaleButton.classList.add('log-scale-hover');
                }
            });

            scaleButton.addEventListener('mouseleave', () => {
                if (!isLogScale) {
                    scaleButton.classList.remove('log-scale-hover');
                    scaleButton.classList.add('log-scale-inactive');
                }
            });

            // 클릭 이벤트 핸들러
            scaleButton.addEventListener('click', () => {
                toggleLogScale();
            });

            container.appendChild(scaleButton);
            scaleButtonRef.current = scaleButton;

            // 11. 차트 크기 변경 및 데이터에 맞게 조정
            chart.timeScale().fitContent();

            // 년도 그리드 그리기
            drawYearGridLines();

            // 12. 이벤트 구독
            chart.subscribeCrosshairMove(handleCrosshairMove);

            // timeScale visible range 변경 감지 (줌/팬 시 그리드 업데이트)
            chart.timeScale().subscribeVisibleLogicalRangeChange(() => {
                requestAnimationFrame(() => {
                    drawYearGridLines();
                });
            });

            // ResizeObserver 설정
            resizeObserver = new ResizeObserver(entries => {
                if (!chartRef.current) return; // 차트가 없으면 종료

                const {width, height} = entries[0].contentRect;
                chartRef.current.applyOptions({width, height});
                
                // 리사이즈 시 년도 그리드 다시 그리기
                requestAnimationFrame(() => {
                    drawYearGridLines();
                });
            });

            resizeObserver.observe(container);

            // 초기 렌더링에서 차트가 보이도록 확인 (수정된 부분)
            setTimeout(() => {
                if (chartRef.current && container) {
                    const currentWidth = container.clientWidth || container.offsetWidth;
                    const currentHeight = container.clientHeight || container.offsetHeight;

                    if (currentWidth > 0 && currentHeight > 0) {
                        chartRef.current.applyOptions({
                            width: currentWidth,
                            height: currentHeight
                        });
                        chartRef.current.timeScale().fitContent(); // 내용에 맞게 스케일 조정
                    }
                }
            }, 100); // 더 긴 지연 시간으로 변경

            // 13. 정리 함수 반환
            return () => {
                isComponentMounted.current = false;

                // 이벤트 핸들러 등록 해제를 가장 먼저
                if (chartRef.current) {
                    try {
                        chartRef.current.unsubscribeCrosshairMove(handleCrosshairMove);
                    } catch (error) {
                        console.error("이벤트 해제 오류:", error);
                    }
                }

                // 약간의 지연 후에 차트 제거 (비동기 작업이 완료될 시간을 주기 위함)
                setTimeout(() => {
                    try {
                        cleanUp();
                        hasTooltipAppeared.current = false; // 차트 정리 시 툴팁 등장 플래그 초기화
                    } catch (error) {
                        console.error("차트 제거 중 오류 발생:", error);
                    }
                }, 0);
            };
        } catch (error) {
            console.error("차트 생성 중 오류 발생:", error);
            return () => {
                isComponentMounted.current = false;
            };
        }
    }, [handleCrosshairMove, showMaxBalance, showDrawdown, processChartData, updateChartSeries, removeChartSeries, removeDOMElement, createChartOptions]); // isLogScale 의존성 제거

    // 로그 스케일 상태 변경시 버튼 스타일 업데이트
    useEffect(() => {

        if (scaleButtonRef.current) {
            const scaleButton = scaleButtonRef.current;
            // 클래스 업데이트
            if (isLogScale) {
                scaleButton.classList.remove('log-scale-inactive', 'log-scale-hover');
                scaleButton.classList.add('log-scale-active');
            } else {
                scaleButton.classList.remove('log-scale-active');
                scaleButton.classList.add('log-scale-inactive');
            }
        }

        // 로그 스케일 변경 시 차트 priceScale 모드도 업데이트
        if (chartRef.current) {
            try {
                // 메인 페인 스케일 모드 업데이트
                chartRef.current.priceScale('left').applyOptions({
                    mode: isLogScale ? PriceScaleMode.Logarithmic : PriceScaleMode.Normal,
                });

                // 드로우다운 페인(인덱스 1)의 스케일 모드 업데이트 - showDrawdown이 true일 때만
                if (showDrawdown && drawdownPaneRef.current) {
                    try {
                        chartRef.current.priceScale('left', 1).applyOptions({
                            mode: isLogScale ? PriceScaleMode.Logarithmic : PriceScaleMode.Normal,
                        });
                    } catch (error) {
                        console.error('드로우다운 페인 스케일 모드 업데이트 중 오류:', error);
                    }
                }

                // 추가: 차트에 변경된 상태를 명확하게 알리기 위해 fitContent 호출
                chartRef.current.timeScale().fitContent();
            } catch (error) {
                console.error('로그 스케일 상태 변경 시 차트 업데이트 오류:', error);
            }
        }
    }, [isLogScale, showDrawdown]);

    // 컴포넌트 마운트/언마운트 시 window resize 이벤트 감지 추가
    useEffect(() => {
        const handleResize = () => {
            if (chartRef.current && chartContainerRef.current) {
                const container = chartContainerRef.current;
                const width = container.clientWidth || container.offsetWidth;
                const height = container.clientHeight || container.offsetHeight;

                if (width > 0 && height > 0) {
                    chartRef.current.applyOptions({width, height});
                    chartRef.current.timeScale().fitContent();

                    // 드로우다운 페인이 존재하면 높이 재계산 및 디바운스된 함수로 적용
                    if (showDrawdown && drawdownPaneRef.current) {
                        const containerHeight = container.clientHeight || 0;
                        const drawdownHeight = Math.max(Math.round(containerHeight * 0.3), 30);

                        // 높이 설정
                        if (drawdownPaneRef.current) {
                            try {
                                drawdownPaneRef.current.setHeight(drawdownHeight);
                            } catch (error) {
                                console.error('드로우다운 페인 높이 설정 중 오류 (Resize):', error);
                            }
                        }
                    }
                }
            }
        };

        // 디바운스 제거된 핸들러 직접 사용
        const handleResizeDirect = handleResize;

        window.addEventListener('resize', handleResizeDirect);

        // 컴포넌트 마운트 후 즉시 한 번 호출
        setTimeout(handleResize, 50);

        return () => {
            window.removeEventListener('resize', handleResizeDirect);
        };
    }, [showDrawdown]);

    // 탭 전환 감지 및 차트 크기 재조정 로직 추가
    useEffect(() => {
        // 컴포넌트가 보이는지 감지하는 함수
        const checkVisibility = () => {
            if (!chartContainerRef.current || !chartRef.current) return;

            // 요소가 보이는지 확인 (display: none이 아닌지)
            const isVisible = window.getComputedStyle(chartContainerRef.current).display !== 'none';

            if (isVisible) {
                // 이미 설정된 width/height 값이 있으면 재조정 필요 없음
                const container = chartContainerRef.current;
                const currentWidth = container.clientWidth || container.offsetWidth;
                const currentHeight = container.clientHeight || container.offsetHeight;

                // 유효한 크기가 있고, 차트의 현재 크기와 다를 때만 리사이징
                if (chartRef.current && currentWidth > 0 && currentHeight > 0) {
                    const chartWidth = chartRef.current.options().width;
                    const chartHeight = chartRef.current.options().height;

                    // 차트 크기가 컨테이너와 다른 경우에만 업데이트
                    if (chartWidth !== currentWidth || chartHeight !== currentHeight) {
                        chartRef.current.applyOptions({
                            width: currentWidth,
                            height: currentHeight
                        });
                        chartRef.current.timeScale().fitContent();

                        // 드로우다운 페인이 존재하면 높이 재계산 및 디바운스된 함수로 적용
                        if (showDrawdown && drawdownPaneRef.current) {
                            const containerHeight = container.clientHeight || 0;
                            const drawdownHeight = Math.max(Math.round(containerHeight * 0.3), 30);

                            // 디바운스 없이 바로 높이 설정
                            if (drawdownPaneRef.current) {
                                try {
                                    drawdownPaneRef.current.setHeight(drawdownHeight);
                                } catch (error) {
                                    console.error('드로우다운 페인 높이 설정 중 오류 (Visibility):', error);
                                }
                            }
                        }
                    }
                }
            }
        };

        // MutationObserver로 display 속성 변경 감지 - visibility 관련 변화만 감지하도록 수정
        const observer = new MutationObserver((mutations) => {
            for (const mutation of mutations) {
                if (mutation.type === 'attributes' && mutation.attributeName === 'style') {
                    // style 변경 중에서도 visibility나 display 관련 속성이 변경됐을 때만 실행
                    const target = mutation.target as HTMLElement;
                    const computedStyle = window.getComputedStyle(target);
                    const oldVisibility = target.dataset.prevVisibility || '';
                    const oldDisplay = target.dataset.prevDisplay || '';
                    const newVisibility = computedStyle.visibility;
                    const newDisplay = computedStyle.display;

                    // 이전 값과 새 값이 다른 경우에만 처리 (display나 visibility가 변경된 경우)
                    if (oldVisibility !== newVisibility || oldDisplay !== newDisplay) {
                        // 새 값 저장
                        target.dataset.prevVisibility = newVisibility;
                        target.dataset.prevDisplay = newDisplay;

                        // display나 visibility가 변경된 경우에만 체크
                        if (newDisplay !== 'none' && (oldDisplay === 'none' || oldVisibility === 'hidden')) {
                            checkVisibility();
                        }
                    }
                }
            }
        });

        // 부모 요소들 모니터링 (최상위 요소만 관찰)
        if (chartContainerRef.current) {
            let topParent = chartContainerRef.current.parentElement;
            // 상위 요소가 있을 때까지 올라가기
            while (topParent && topParent.parentElement &&
            !topParent.classList.contains('tab-content') &&
            topParent.tagName !== 'BODY' &&
            topParent.id !== 'root') {
                topParent = topParent.parentElement;
            }

            // 최상위 부모 요소만 관찰
            if (topParent) {
                // 초기 상태 저장
                const computedStyle = window.getComputedStyle(topParent);
                topParent.dataset.prevVisibility = computedStyle.visibility;
                topParent.dataset.prevDisplay = computedStyle.display;

                observer.observe(topParent, {attributes: true});
            }
        }

        // 초기 실행
        setTimeout(checkVisibility, 100);

        return () => {
            observer.disconnect();
        };
    }, [showDrawdown]);

    // 거래 데이터가 없으면 차트 생성하지 않고 메시지만 표시
    if (!filteredTrades || filteredTrades.length === 1) {
        return (
            <div
                id="equity-curve-container"
                style={{
                    width: '100%',
                    height: '100%',
                    position: 'relative',
                    overflow: 'visible',
                    margin: 'auto',
                    boxSizing: 'border-box',
                    minHeight: '400px',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center'
                }}
            >
                <NoDataMessage message="거래 내역이 존재하지 않습니다."/>
            </div>
        );
    }

    return (
        <div
            id="equity-curve-container"
            ref={chartContainerRef}
            style={{
                width: '100%',
                height: '100%',
                position: 'relative',
                overflow: 'visible',
                margin: 'auto',
                boxSizing: 'border-box',
                minHeight: '400px'
            }}
        />
    );
};

export default EquityCurve;

