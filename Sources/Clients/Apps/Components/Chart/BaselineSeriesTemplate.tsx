import {forwardRef, useEffect, useImperativeHandle, useRef} from 'react';
import {BaselineSeries, IChartApi, LineStyle, LineType, LineWidth} from 'lightweight-charts';

export interface DataPoint {
    time: number;
    value: number | null;
    color?: string;
}

export interface BaselineSeriesProps {
    chart: IChartApi;
    paneIndex: number;
    baseValue: number;
    topFillColor1: string;
    topFillColor2: string;
    topLineColor: string;
    bottomFillColor1: string;
    bottomFillColor2: string;
    bottomLineColor: string;
    lineStyle: LineStyle;
    lineWidth: LineWidth;
    lineType: LineType;
    pointMarkersVisible: boolean;
    pointMarkersRadius?: number;
    indicatorName: string;
    initialData?: DataPoint[];
    priceStep: number;
    pricePrecision: number;
}

export interface BaselineSeriesHandle {
    updateData: (newIndicatorData: DataPoint[], options?: { reset?: boolean }) => void;
    getDataCache: () => DataPoint[];
}

const BaselineSeriesTemplate = forwardRef<BaselineSeriesHandle, BaselineSeriesProps>((props, ref) => {
    const {
        chart,
        paneIndex,
        baseValue,
        topFillColor1,
        topFillColor2,
        topLineColor,
        bottomFillColor1,
        bottomFillColor2,
        bottomLineColor,
        lineStyle,
        lineWidth,
        lineType,
        pointMarkersVisible,
        pointMarkersRadius,
        indicatorName,
        initialData = [],
        priceStep,
        pricePrecision
    } = props;

    const seriesRef = useRef<any>(null);
    const dataCacheRef = useRef<DataPoint[]>(initialData);
    const isInitializedRef = useRef<boolean>(false);
    const isUpdatingRef = useRef<boolean>(false);
    const pendingUpdateRef = useRef<{ data: DataPoint[] } | null>(null);

    useEffect(() => {
        if (!chart) return;

        seriesRef.current = chart.addSeries(BaselineSeries, {
            baseValue: {type: 'price', price: baseValue},
            topFillColor1: topFillColor1,
            topFillColor2: topFillColor2,
            topLineColor: topLineColor,
            bottomFillColor1: bottomFillColor1,
            bottomFillColor2: bottomFillColor2,
            bottomLineColor: bottomLineColor,
            lineStyle: lineStyle,
            lineWidth: lineWidth,
            lineType: lineType,
            pointMarkersVisible: pointMarkersVisible,
            pointMarkersRadius: pointMarkersRadius,
            priceFormat: {
                type: 'price',
                minMove: priceStep,
                precision: pricePrecision
            },
            lastValueVisible: false,
            priceLineVisible: false,
            crosshairMarkerVisible: false,
        }, paneIndex);

        if (initialData.length > 0) {
            const formattedData = initialData
                .filter(pt => !(pt.value === null || pt.value === undefined || isNaN(pt.value as any) || !isFinite(pt.value as any)))
                .map(pt => ({time: pt.time, value: pt.value as number}));

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
                    seriesType: "Baseline",
                    lineColor: topLineColor,
                    baseValue: baseValue,
                    topLineColor: topLineColor,
                    bottomLineColor: bottomLineColor
                };

                // 한 번만 초기화
                if (!isInitializedRef.current) {
                    isInitializedRef.current = true;
                }
            }
        }

        // 컴포넌트 언마운트 시 시리즈 제거
        return () => {
            if (seriesRef.current && chart) {
                try {
                    chart.removeSeries(seriesRef.current);
                } catch (e) {
                    // 차트가 이미 제거된 경우 무시
                }

                seriesRef.current = null;
            }
        };
    }, [
        chart,
        paneIndex,
        baseValue,
        topFillColor1,
        topFillColor2,
        topLineColor,
        bottomFillColor1,
        bottomFillColor2,
        bottomLineColor,
        lineStyle,
        lineWidth,
        lineType,
        pointMarkersVisible,
        pointMarkersRadius,
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

    // 내부 데이터 업데이트 함수 (빠른 concat 병합)
    const updateDataInternal = async (newIndicatorData: DataPoint[], reset = false) => {
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

                const formattedData = dataCacheRef.current
                    .filter(pt => !(pt.value === null || pt.value === undefined || isNaN(pt.value as any) || !isFinite(pt.value as any)))
                    .map(pt => ({time: pt.time, value: pt.value as number}));

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
                        seriesType: "Baseline",
                        baseValue: baseValue,
                        topLineColor: topLineColor,
                        bottomLineColor: bottomLineColor
                    };
                }

                isUpdatingRef.current = false;
                processPendingUpdate();
            });
        } catch (error) {
            console.error(`[BaselineSeriesTemplate] ${indicatorName} 데이터 병합 오류:`, error);
            isUpdatingRef.current = false;
            processPendingUpdate();
        }
    };

    useImperativeHandle(ref, () => ({
        updateData(newIndicatorData: DataPoint[], options?: { reset?: boolean }) {
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

export default BaselineSeriesTemplate;
