import {forwardRef, useEffect, useImperativeHandle, useRef} from 'react';
import {HistogramSeries, IChartApi} from 'lightweight-charts';

export interface DataPoint {
    time: number;
    value: number | null;
    color?: string;
}

export interface HistogramDataPoint extends DataPoint {
    color?: string;
}

export interface HistogramSeriesProps {
    chart: IChartApi;
    paneIndex: number;
    baseValue: number;
    color: string;
    bearishColor?: string;
    indicatorName: string;
    initialData?: HistogramDataPoint[];
    priceStep: number;
    pricePrecision: number;
}

export interface HistogramSeriesHandle {
    updateData: (newIndicatorData: HistogramDataPoint[], options?: { reset?: boolean }) => void;
    getDataCache: () => HistogramDataPoint[];
}

const HistogramSeriesTemplate = forwardRef<HistogramSeriesHandle, HistogramSeriesProps>((props, ref) => {
    const {
        chart,
        paneIndex,
        baseValue,
        color,
        bearishColor,
        indicatorName,
        initialData = [],
        priceStep,
        pricePrecision
    } = props;

    const seriesRef = useRef<any>(null);
    const dataCacheRef = useRef<HistogramDataPoint[]>(initialData);
    const isInitializedRef = useRef<boolean>(false);
    const isUpdatingRef = useRef<boolean>(false);
    const pendingUpdateRef = useRef<{ data: HistogramDataPoint[] } | null>(null);

    // 값에 따라 적절한 색상을 반환하는 함수
    const getColorForValue = (value: number | null): string | undefined => {
        if (value === null || isNaN(value)) return undefined;
        
        if (bearishColor) {
            // baseValue보다 낮으면 bearishColor, 높으면 기본 color
            return value < baseValue ? bearishColor : color;
        }
        
        return undefined; // 기본 시리즈 색상 사용
    };

    useEffect(() => {
        if (!chart) return;

        seriesRef.current = chart.addSeries(HistogramSeries, {
            base: baseValue,
            color: color,
            priceFormat: {
                type: 'price',
                minMove: priceStep,
                precision: pricePrecision
            },
            lastValueVisible: false,
            priceLineVisible: false,
        }, paneIndex);

        if (initialData.length > 0) {
            const formattedData = initialData.map(pt => {
                const value = pt.value === null ? NaN : pt.value;
                const result: any = {
                    time: pt.time,
                    value: value,
                };
                
                // 기존 색상이 있으면 사용, 없으면 값에 따라 색상 결정
                if (pt.color) {
                    result.color = pt.color;
                } else {
                    const autoColor = getColorForValue(pt.value);
                    if (autoColor) {
                        result.color = autoColor;
                    }
                }
                
                return result;
            });

            seriesRef.current.setData(formattedData);

            // window.indicatorData 업데이트 (중요: 병합된 전체 데이터를 설정)
            if (typeof window !== 'undefined') {
                if (!window.indicatorData) {
                    window.indicatorData = {};
                }
                // 병합된 전체 데이터를 window.indicatorData에 설정
                window.indicatorData[indicatorName] = [...dataCacheRef.current];

                // window.indicatorSeriesInfo 설정도 유지
                if (!window.indicatorSeriesInfo) {
                    window.indicatorSeriesInfo = {};
                }
                window.indicatorSeriesInfo[indicatorName] = {
                    name: indicatorName,
                    pane: paneIndex,
                    seriesType: "Histogram",
                    baseValue: baseValue
                };

                // 한 번만 초기화
                if (!isInitializedRef.current) {
                    isInitializedRef.current = true;
                }
            }
        }
    }, [
        chart,
        paneIndex,
        baseValue,
        color,
        bearishColor,
        indicatorName,
        priceStep,
        pricePrecision
    ]);

    // 대기 중인 업데이트가 있으면 처리하는 함수
    const processPendingUpdate = () => {
        if (pendingUpdateRef.current && !isUpdatingRef.current) {
            const {data} = pendingUpdateRef.current;
            pendingUpdateRef.current = null;
            updateDataInternal(data).then();
        }
    };

    // 내부 데이터 업데이트 함수 (Web Worker 활용)
    const updateDataInternal = async (newIndicatorData: HistogramDataPoint[], reset = false) => {
        if (isUpdatingRef.current) {
            // 이미 업데이트 중이면 나중에 처리하도록 보관
            pendingUpdateRef.current = {data: newIndicatorData};
            return;
        }

        isUpdatingRef.current = true;

        try {
            // 빠른 병합: 파일이 시간 순으로 정렬되어 있으므로 단순히 concat 사용
            if (newIndicatorData.length > 0) {
                if (reset || dataCacheRef.current.length === 0) {
                    dataCacheRef.current = [...newIndicatorData];
                } else {
                    dataCacheRef.current = dataCacheRef.current.concat(newIndicatorData);
                }
            }

            // 레이아웃 계산 최적화를 위해 requestAnimationFrame 사용
            requestAnimationFrame(() => {
                if (!seriesRef.current) {
                    isUpdatingRef.current = false;
                    processPendingUpdate();
                    return;
                }

                const formattedData = dataCacheRef.current.map(pt => {
                    const value = pt.value === null ? NaN : pt.value;
                    const result: any = {
                        time: pt.time,
                        value: value,
                    };
                    
                    // 기존 색상이 있으면 사용, 없으면 값에 따라 색상 결정
                    if (pt.color) {
                        result.color = pt.color;
                    } else {
                        const autoColor = getColorForValue(pt.value);
                        if (autoColor) {
                            result.color = autoColor;
                        }
                    }
                    
                    return result;
                });

                // 차트 데이터 업데이트
                seriesRef.current.setData(formattedData);

                // window.indicatorData 업데이트 (중요: 병합된 전체 데이터를 설정)
                if (typeof window !== 'undefined') {
                    if (!window.indicatorData) {
                        window.indicatorData = {};
                    }
                    // 병합된 전체 데이터를 window.indicatorData에 설정
                    window.indicatorData[indicatorName] = [...dataCacheRef.current];

                    // window.indicatorSeriesInfo 설정도 유지
                    if (!window.indicatorSeriesInfo) {
                        window.indicatorSeriesInfo = {};
                    }
                    window.indicatorSeriesInfo[indicatorName] = {
                        name: indicatorName,
                        pane: paneIndex,
                        seriesType: "Histogram",
                        baseValue: baseValue
                    };
                }

                isUpdatingRef.current = false;
                processPendingUpdate();
            });
        } catch (error) {
            console.error(`[HistogramSeriesTemplate] ${indicatorName} 데이터 병합 오류:`, error);
            isUpdatingRef.current = false;
            processPendingUpdate();
        }
    };

    useImperativeHandle(ref, () => ({
        updateData(newIndicatorData: HistogramDataPoint[], options?: { reset?: boolean }) {
            const reset = options?.reset || false;
            updateDataInternal(newIndicatorData, reset).then();
        },

        // getDataCache 메서드 추가 - TopInfo에서 최신 데이터를 얻을 수 있도록
        getDataCache() {
            return [...dataCacheRef.current];
        }
    }));

    return null;
});

export default HistogramSeriesTemplate;
