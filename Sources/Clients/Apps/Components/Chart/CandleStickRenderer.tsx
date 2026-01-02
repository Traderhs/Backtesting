import {CandlestickSeries, HistogramSeries, IChartApi, UTCTimestamp} from 'lightweight-charts';

export interface CandleData {
    time: UTCTimestamp;
    open: number;
    high: number;
    low: number;
    close: number;
    volume: number;
}

interface CandleStickRendererProps {
    chart: IChartApi | null;
    priceStep: number;
    pricePrecision: number;
}

// 데이터 처리 후 콜백을 위한 타입
export interface PositionSettings {
    fitContent?: boolean;
    setVisibleLogicalRange?: {
        from: number;
        to: number;
    };
}

class CandleStickRenderer {
    private readonly chart: IChartApi | null;
    private mainSeries: ReturnType<IChartApi['addSeries']> | null = null;
    private volumeSeries: ReturnType<IChartApi['addSeries']> | null = null;
    private readonly priceStep: number;
    private readonly pricePrecision: number;
    private firstUpdate: boolean = true;

    constructor({chart, priceStep, pricePrecision}: CandleStickRendererProps) {
        this.chart = chart;
        this.priceStep = priceStep;
        this.pricePrecision = pricePrecision;
    }

    public initSeries() {
        if (!this.chart) return;

        // 이미 초기화되어 있으면 재사용
        if (this.mainSeries || this.volumeSeries) {
            console.warn("[CandleStickRenderer] Series already initialized.");
            return;
        }

        try {
            // 볼륨 시리즈 생성
            this.volumeSeries = this.chart.addSeries(HistogramSeries, {
                priceScaleId: "volume",
                priceFormat: {
                    type: "volume",
                    minMove: this.priceStep,
                    precision: this.pricePrecision
                },
                lastValueVisible: false,
                priceLineVisible: false
            }, 0);

            this.volumeSeries.priceScale().applyOptions({
                scaleMargins: {
                    top: 0.75,
                    bottom: 0.0
                }
            });

            // 메인 캔들스틱 시리즈 생성
            this.mainSeries = this.chart.addSeries(CandlestickSeries, {
                upColor: "#4caf50",
                downColor: "#f23645",
                borderUpColor: "#4caf50",
                borderDownColor: "#f23645",
                wickUpColor: "#4caf50",
                wickDownColor: "#f23645",
                priceFormat: {
                    type: 'price',
                    minMove: this.priceStep,
                    precision: this.pricePrecision
                },
                lastValueVisible: false,
                priceLineVisible: false
            }, 0);

            window.mainSeries = this.mainSeries;

            // 차트 DOM 요소 스타일 설정
            const chartContainer = document.getElementById('chart-main-container');
            if (chartContainer) {
                const chartElement = chartContainer.querySelector('.tv-lightweight-charts');
                if (chartElement) {
                    (chartElement as HTMLElement).style.zIndex = '20';
                    (chartElement as HTMLElement).style.position = 'relative';
                    (chartElement as HTMLElement).style.display = 'block';
                    (chartElement as HTMLElement).style.visibility = 'visible';
                }
            }

            // 강제 리플로우
            if (chartContainer) {
                void chartContainer.offsetHeight;
            }
        } catch (error) {
            console.error("[CandleStickRenderer] 시리즈 초기화 중 오류 발생:", error);
            this.mainSeries = null;
            this.volumeSeries = null;
        }
    }

    public removeSeries() {
        if (!this.chart) return;

        if (this.mainSeries) {
            this.chart.removeSeries(this.mainSeries);
            this.mainSeries = null;
        }

        if (this.volumeSeries) {
            this.chart.removeSeries(this.volumeSeries);
            this.volumeSeries = null;
        }
    }

    // 특수 상황에서 시리즈를 재생성하는 메서드 (첫 심볼 로딩시 지표와 순서 충돌 시 활용)
    public recreateSeries() {
        if (!this.chart) return;

        // 기존 시리즈 제거
        this.removeSeries();

        // 새 시리즈 초기화
        this.initSeries();
    }

    public updateData(data: CandleData[], positionSettings?: PositionSettings) {
        if (!this.chart) return;

        // 시리즈가 없으면 초기화
        if (!this.mainSeries || !this.volumeSeries) {
            this.initSeries();
            if (!this.mainSeries || !this.volumeSeries) {
                console.error("[CandleStickRenderer] Failed to create series");
                return;
            }
        }

        try {
            // 볼륨 데이터 업데이트
            if (this.volumeSeries) {
                this.volumeSeries.setData(data.map((bar, index) => {
                    if (bar.close === null || bar.open === null) {
                        return {
                            time: bar.time,
                            value: bar.volume === null ? NaN : bar.volume,
                            color: "#08998180"
                        };
                    }

                    let color: string;

                    // 시가 == 종가인 경우 (도지 캔들)
                    if (bar.open === bar.close) {
                        // 첫 번째 캔들이거나 이전 종가가 없으면 기본 캔들 색상 로직 사용
                        if (index === 0 || data[index - 1].close === null) {
                            color = bar.close >= bar.open ? "#08998180" : "#ef535080";
                        } else {
                            // 이전 종가와 비교
                            const previousClose = data[index - 1].close;
                            color = bar.close >= previousClose ? "#08998180" : "#ef535080";
                        }
                    } else {
                        // 기존 방식: 시가 vs 종가
                        color = bar.close >= bar.open ? "#08998180" : "#ef535080";
                    }

                    return {
                        time: bar.time,
                        value: bar.volume === null ? NaN : bar.volume,
                        color: color
                    };
                }));
            }

            // 캔들스틱 데이터 업데이트
            if (this.mainSeries) {
                this.mainSeries.setData(data.map(bar => ({
                    time: bar.time,
                    open: bar.open,
                    high: bar.high,
                    low: bar.low,
                    close: bar.close
                })));
            }

            // 위치 설정이 있다면 적용
            if (positionSettings) {
                if (positionSettings.fitContent) {
                    this.chart.timeScale().fitContent();
                }

                if (positionSettings.setVisibleLogicalRange) {
                    this.chart.timeScale().setVisibleLogicalRange(positionSettings.setVisibleLogicalRange);
                }
            }

            // 첫 업데이트 플래그 해제 및 첫 로딩 시 시리즈 재생성
            if (this.firstUpdate) {
                this.firstUpdate = false;

                setTimeout(() => {
                    this.recreateSeries();

                    // 시리즈 재생성 후 데이터 다시 업데이트
                    if (this.mainSeries && this.volumeSeries) {
                        this.volumeSeries.setData(data.map((bar, index) => {
                            if (bar.close === null || bar.open === null) {
                                return {
                                    time: bar.time,
                                    value: bar.volume,
                                    color: "#08998180"
                                };
                            }

                            let color: string;

                            // 시가 == 종가인 경우 (도지 캔들)
                            if (bar.open === bar.close) {
                                // 첫 번째 캔들이거나 이전 종가가 없으면 기본 캔들 색상 로직 사용
                                if (index === 0 || data[index - 1].close === null) {
                                    color = bar.close >= bar.open ? "#08998180" : "#ef535080";
                                } else {
                                    // 이전 종가와 비교
                                    const previousClose = data[index - 1].close;
                                    color = bar.close >= previousClose ? "#08998180" : "#ef535080";
                                }
                            } else {
                                // 기존 방식: 시가 vs 종가
                                color = bar.close >= bar.open ? "#08998180" : "#ef535080";
                            }

                            return {
                                time: bar.time,
                                value: bar.volume,
                                color: color
                            };
                        }));

                        this.mainSeries.setData(data.map(bar => ({
                            time: bar.time,
                            open: bar.open,
                            high: bar.high,
                            low: bar.low,
                            close: bar.close
                        })));
                    }
                }, 100);
            }

            // 차트 업데이트 후 z-index 확인 및 재조정
            const chartContainer = document.getElementById('chart-main-container');
            if (chartContainer) {
                const chartElement = chartContainer.querySelector('.tv-lightweight-charts');
                if (chartElement) {
                    (chartElement as HTMLElement).style.zIndex = '20';
                    (chartElement as HTMLElement).style.position = 'relative';
                    (chartElement as HTMLElement).style.display = 'block';
                    (chartElement as HTMLElement).style.visibility = 'visible';
                }

                // 강제 리플로우
                void chartContainer.offsetHeight;
            }
        } catch (error) {
            console.error("[CandleStickRenderer] 데이터 업데이트 중 오류 발생:", error);
        }
    }

    public dispose() {
        this.removeSeries();
    }
}

export default CandleStickRenderer;
