import {forwardRef, useEffect, useImperativeHandle, useRef} from 'react';
import {AreaSeries, IChartApi, LineStyle, LineType, LineWidth} from 'lightweight-charts';

export interface DataPoint {
    time: number;
    value: number | null;
    color?: string;
}

export interface AreaSeriesProps {
    chart: IChartApi;
    paneIndex: number;
    topColor: string;
    bottomColor: string;
    lineColor: string;
    lineStyle: LineStyle;
    lineWidth: LineWidth;
    lineType: LineType;
    pointMarkersVisible: boolean;
    pointMarkersRadius?: number;
    indicatorName: string;
    initialData?: DataPoint[];
    tickSize: number;
    precision: number;
}

export interface AreaSeriesHandle {
    updateData: (newIndicatorData: DataPoint[], options?: { reset?: boolean }) => void;
    getDataCache: () => DataPoint[];
}

const AreaSeriesTemplate = forwardRef<AreaSeriesHandle, AreaSeriesProps>((props, ref) => {
    const {
        chart,
        paneIndex,
        topColor,
        bottomColor,
        lineColor,
        lineStyle,
        lineWidth,
        lineType,
        pointMarkersVisible,
        pointMarkersRadius,
        indicatorName,
        initialData = [],
        tickSize,
        precision,
    } = props;

    const seriesRef = useRef<any>(null);
    const dataCacheRef = useRef<DataPoint[]>(initialData);
    const isInitializedRef = useRef<boolean>(false);
    const isUpdatingRef = useRef<boolean>(false);
    const pendingUpdateRef = useRef<{data: DataPoint[], options?: { reset?: boolean }} | null>(null);

    // 시리즈 생성 및 초기 setData 처리
    useEffect(() => {
        if (!chart) return;

        seriesRef.current = chart.addSeries(AreaSeries, {
            topColor,
            bottomColor,
            lineColor,
            lineStyle,
            lineWidth,
            lineType,
            pointMarkersVisible,
            ...(pointMarkersVisible ? { pointMarkersRadius } : {}),
            priceFormat: {
                type: 'price',
                minMove: tickSize,
                precision: precision,
            },
            lastValueVisible: false,
            priceLineVisible: false,
            crosshairMarkerVisible: false,
        }, paneIndex);

        if (initialData.length > 0) {
            const formattedData = initialData.map(pt => ({
                time: pt.time,
                value: pt.value === null ? NaN : pt.value,
            }));
            
            seriesRef.current.setData(formattedData);
            
            // window.indicatorData 업데이트 (중요: 병합된 전체 데이터를 설정)
            if (typeof window !== 'undefined') {
                if (!window.indicatorData) {
                    window.indicatorData = {};
                }
                // 병합된 전체 데이터를 window.indicatorData에 설정
                window.indicatorData[indicatorName] = [...initialData];
                
                // window.indicatorSeriesInfo 설정도 유지
                if (!window.indicatorSeriesInfo) {
                    window.indicatorSeriesInfo = {};
                }
                // 각 속성을 개별적으로 추가하여 타입 오류 방지
                window.indicatorSeriesInfo[indicatorName] = {
                    name: indicatorName,
                    pane: paneIndex,
                    seriesType: "Area",
                    lineColor: lineColor
                };
                // 추가 속성은 별도로 설정
                window.indicatorSeriesInfo[indicatorName].topLineColor = topColor;
                window.indicatorSeriesInfo[indicatorName].bottomLineColor = bottomColor;
                
                // 한 번만 초기화
                if (!isInitializedRef.current) {
                    isInitializedRef.current = true;
                }
            }
        }
    }, [
        chart,
        paneIndex,
        topColor,
        bottomColor,
        lineColor,
        lineStyle,
        lineWidth,
        lineType,
        pointMarkersVisible,
        pointMarkersRadius,
        indicatorName,
        tickSize,
        precision
    ]);

    // 대기 중인 업데이트가 있으면 처리하는 함수
    const processPendingUpdate = () => {
        if (pendingUpdateRef.current && !isUpdatingRef.current) {
            const { data, options } = pendingUpdateRef.current;
            pendingUpdateRef.current = null;
            updateDataInternal(data, options).then();
        }
    };

    // 내부 데이터 업데이트 함수 (빠른 concat 병합)
    const updateDataInternal = async (newIndicatorData: DataPoint[], options?: { reset?: boolean }) => {
        if (isUpdatingRef.current) {
            // 이미 업데이트 중이면 나중에 처리하도록 보관
            pendingUpdateRef.current = { data: newIndicatorData, options };
            return;
        }

        isUpdatingRef.current = true;
        
        try {
            // 빠른 병합 또는 초기화
            if (newIndicatorData.length > 0) {
                if (dataCacheRef.current.length === 0 || options?.reset) {
                    // 초기 로드이거나 reset 옵션이 true인 경우 기존 데이터 교체
                    dataCacheRef.current = newIndicatorData;
                    console.log(`[AreaSeries] ${indicatorName}: 데이터 ${options?.reset ? '초기화' : '설정'}`);
                } else {
                    // 기존 방식: 데이터 병합
                    dataCacheRef.current = dataCacheRef.current.concat(newIndicatorData);
                    console.log(`[AreaSeries] ${indicatorName}: 데이터 병합`);
                }
            }
            
            // 레이아웃 계산 최적화를 위해 requestAnimationFrame 사용
            requestAnimationFrame(() => {
                if (!seriesRef.current) {
                    isUpdatingRef.current = false;
                    processPendingUpdate();
                    return;
                }
                
                const formattedData = dataCacheRef.current.map(pt => ({
                    time: pt.time,
                    value: pt.value === null ? NaN : pt.value,
                }));
                
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
                    // 각 속성을 개별적으로 추가하여 타입 오류 방지
                    window.indicatorSeriesInfo[indicatorName] = {
                        name: indicatorName,
                        pane: paneIndex,
                        seriesType: "Area",
                        lineColor: lineColor
                    };
                    // 추가 속성은 별도로 설정
                    window.indicatorSeriesInfo[indicatorName].topLineColor = topColor;
                    window.indicatorSeriesInfo[indicatorName].bottomLineColor = bottomColor;
                }
                
                isUpdatingRef.current = false;
                processPendingUpdate();
            });
        } catch (error) {
            console.error(`[AreaSeriesTemplate] ${indicatorName} 데이터 병합 오류:`, error);
            isUpdatingRef.current = false;
            processPendingUpdate();
        }
    };

    // 외부에서 updateData() 호출 시 바로 데이터 병합 후 setData 실행
    useImperativeHandle(ref, () => ({
        updateData(newIndicatorData: DataPoint[], options?: { reset?: boolean }) {
            updateDataInternal(newIndicatorData, options).then();
        },
        
        // getDataCache 메서드 추가 - TopInfo에서 최신 데이터를 얻을 수 있도록
        getDataCache() {
            return [...dataCacheRef.current];
        }
    }));

    return null;
});

export default AreaSeriesTemplate;
