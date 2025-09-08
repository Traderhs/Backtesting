import React, {useEffect, useState, JSX, useRef} from 'react';
import {IChartApi, LineStyle, LineType} from 'lightweight-charts';
import LineSeriesTemplate from './LineSeriesTemplate';
import BaselineSeriesTemplate from './BaselineSeriesTemplate';
import AreaSeriesTemplate from './AreaSeriesTemplate';
import HistogramSeriesTemplate from './HistogramSeriesTemplate';

import {IndicatorDataPoint} from './Chart';

const mapLineStyle = (styleStr: string): LineStyle => {
    switch (styleStr) {
        case "실선":
            return LineStyle.Solid;
        case "점선":
            return LineStyle.Dotted;
        case "파선":
            return LineStyle.Dashed;
        case "넓은 점선":
            return LineStyle.SparseDotted;
        case "넓은 파선":
            return LineStyle.LargeDashed;
        default:
            return LineStyle.Solid;
    }
};
const mapLineType = (typeStr: string): LineType => {
    switch (typeStr) {
        case "직선":
            return LineType.Simple;
        case "계단선":
            return LineType.WithSteps;
        case "곡선":
            return LineType.Curved;
        default:
            return LineType.Simple;
    }
};
const mapPointMarkersVisible = (str: string): boolean => {
    return str === "활성화";
};

interface IndicatorSeriesContainerProps {
    chart: IChartApi | null;
    indicatorDataMap: { [indicatorName: string]: IndicatorDataPoint[] };
    tickSize: number;
    precision: number;
}

const IndicatorSeriesContainer: React.FC<IndicatorSeriesContainerProps> = ({
                                                                               chart,
                                                                               indicatorDataMap,
                                                                               tickSize,
                                                                               precision
                                                                           }) => {
    const [config, setConfig] = useState<any>(null);
    const [seriesComponents, setSeriesComponents] = useState<JSX.Element[]>([]);
    const indicatorSeriesRefs = useRef<{ [key: string]: any }>({});
    const [isVisible, setIsVisible] = useState(true);

    // Chart.tsx에서 접근할 수 있도록 전역에 등록 (초기화만)
    useEffect(() => {
        // 컴포넌트 마운트 시 전역 객체 초기화 (안전하게)
        if (typeof window !== 'undefined') {
            window.indicatorSeriesRefs = {};
        }

        // 지표 표시 여부 제어 함수 등록
        window.showIndicators = () => {
            setIsVisible(true);
        };

        // 컴포넌트 언마운트 시 정리
        return () => {
            if (typeof window !== 'undefined') {
                // 지표 시리즈 정리
                if (window.indicatorSeriesRefs) {
                    try {
                        Object.keys(window.indicatorSeriesRefs).forEach(key => {
                            const seriesRef = window.indicatorSeriesRefs[key];
                            if (seriesRef && typeof seriesRef.dispose === 'function') {
                                seriesRef.dispose();
                            }
                        });
                    } catch (e) {
                        console.error("[지표] 시리즈 정리 오류:", e);
                    }

                    // 참조 삭제
                    window.indicatorSeriesRefs = {};
                }

                // 표시 함수 정리
                if (window.showIndicators) {
                    delete window.showIndicators;
                }
            }
        };
    }, []);

    // 컴포넌트가 생성된 후 ref가 설정되면 window.indicatorSeriesRefs를 업데이트
    useEffect(() => {
        // seriesComponents가 생성되고 refs가 설정된 후에 전역 객체에 할당
        if (typeof window !== 'undefined') {
            // 기존 참조를 유지하되 새 refs로 업데이트
            const currentRefs = window.indicatorSeriesRefs || {};
            const updatedRefs = {...currentRefs, ...indicatorSeriesRefs.current};

            // 실제 업데이트가 있는지 확인 후 적용
            const currentKeys = Object.keys(currentRefs);
            const updatedKeys = Object.keys(updatedRefs);

            if (currentKeys.length !== updatedKeys.length ||
                updatedKeys.some(key => currentRefs[key] !== updatedRefs[key])) {
                window.indicatorSeriesRefs = updatedRefs;
            }
        }

        return () => {
            // 컴포넌트 언마운트 시 모든 지표 시리즈가 정리되도록 보장
            if (typeof window !== 'undefined' && window.indicatorSeriesRefs) {
                Object.keys(window.indicatorSeriesRefs).forEach(key => {
                    try {
                        const seriesRef = window.indicatorSeriesRefs[key];
                        if (seriesRef && typeof seriesRef.dispose === 'function') {
                            seriesRef.dispose();
                        }
                    } catch (e) {
                        console.error(`[IndicatorSeriesContainer] ${key} 시리즈 정리 오류:`, e);
                    }
                });
            }
        };
    }, [seriesComponents]);

    useEffect(() => {
        fetch("/Backboard/config.json")
            .then(res => res.json())
            .then(data => {
                setConfig(data);
                window.indicatorPaths = {};

                let indicatorCount = 0;

                data["지표"]?.forEach((indicator: any) => {
                    const indicatorName = indicator["지표 이름"] || "unknown_indicator";
                    const indicatorPath = indicator["데이터 경로"] || "";
                    const plotType = indicator["플롯"]?.["플롯 종류"];

                    // 플롯 종류가 비활성화인 지표는 제외
                    if (plotType !== "비활성화") {
                        window.indicatorPaths[indicatorName] = indicatorPath.replace(/\/[^\/]*$/, "");
                        indicatorCount++;
                    }
                });
            })
            .catch(err => console.error("config 로딩 오류:", err));
    }, []);

    useEffect(() => {
        if (!chart || !config) {
            return;
        }

        // 시리즈 컴포넌트 생성 전 indicatorSeriesRefs.current 초기화
        indicatorSeriesRefs.current = {};

        const paneMapping: { [paneName: string]: number } = {};
        let nextPaneIndex = 1; // 메인 차트는 paneIndex 0

        const comps: JSX.Element[] = [];
        let componentCount = 0;

        config["지표"]?.forEach((indicator: any) => {
            const plot = indicator["플롯"];
            const plotType = plot["플롯 종류"];
            const indicatorName = indicator["지표 이름"];
            const lineWidth = plot["선 굵기"];
            const lineStyle = mapLineStyle(plot["선 모양"]);
            const lineType = mapLineType(plot["선 종류"]);
            const pointMarkersVisible = mapPointMarkersVisible(plot["선 위 값에 마커 표시"]);
            const pointMarkersRadius = pointMarkersVisible ? plot["마커 반지름"] : undefined;

            let paneIndex = 0;
            if (plot["메인 차트에 지표 겹치기"] === "활성화") {
                paneIndex = 0;
            } else if (plot["메인 차트에 지표 겹치기"] === "비활성화") {
                const paneName = plot["페인 이름"];

                if (!paneMapping[paneName]) {
                    paneMapping[paneName] = nextPaneIndex++;

                    if (typeof window.paneCount === 'undefined') {
                        window.paneCount = 1;
                    } else if (window.paneCount) {
                        window.paneCount++;
                    }
                }

                paneIndex = paneMapping[paneName];
            }

            if (plotType === "영역") {
                comps.push(
                    <AreaSeriesTemplate
                        key={indicatorName}
                        ref={el => {
                            indicatorSeriesRefs.current[indicatorName] = el;
                        }}
                        chart={chart}
                        paneIndex={paneIndex}
                        topColor={plot["위쪽 그라이데이션 색상"]}
                        bottomColor={plot["아래쪽 그라이데이션 색상"]}
                        lineColor={plot["선 색상"]}
                        lineStyle={lineStyle}
                        lineWidth={lineWidth}
                        lineType={lineType}
                        pointMarkersVisible={pointMarkersVisible}
                        pointMarkersRadius={pointMarkersRadius}
                        indicatorName={indicatorName}
                        initialData={indicatorDataMap[indicatorName] || []}
                        tickSize={tickSize}
                        precision={precision}
                    />
                );
                componentCount++;
            } else if (plotType === "기준선") {
                comps.push(
                    <BaselineSeriesTemplate
                        key={indicatorName}
                        ref={el => {
                            indicatorSeriesRefs.current[indicatorName] = el
                        }}
                        chart={chart}
                        paneIndex={paneIndex}
                        baseValue={plot["위/아래 영역을 나눌 기준값"]}
                        topFillColor1={plot["기준값보다 높은 값 영역의 위쪽 그라데이션 색상"]}
                        topFillColor2={plot["기준값보다 높은 값 영역의 아래쪽 그라데이션 색상"]}
                        topLineColor={plot["기준값보다 높은 값에 대한 선 색상"]}
                        bottomFillColor1={plot["기준값보다 낮은 값 영역의 위쪽 그라데이션 색상"]}
                        bottomFillColor2={plot["기준값보다 낮은 값 영역의 아래쪽 그라데이션 색상"]}
                        bottomLineColor={plot["기준값보다 낮은 값에 대한 선 색상"]}
                        lineStyle={lineStyle}
                        lineWidth={lineWidth}
                        lineType={lineType}
                        pointMarkersVisible={pointMarkersVisible}
                        pointMarkersRadius={pointMarkersRadius}
                        indicatorName={indicatorName}
                        initialData={indicatorDataMap[indicatorName] || []}
                        tickSize={tickSize}
                        precision={precision}
                    />
                );
                componentCount++;
            } else if (plotType === "히스토그램") {
                comps.push(
                    <HistogramSeriesTemplate
                        key={indicatorName}
                        ref={el => {
                            indicatorSeriesRefs.current[indicatorName] = el
                        }}
                        chart={chart}
                        paneIndex={paneIndex}
                        baseValue={plot["기준값"]}
                        color={plot["양봉일 때 히스토그램 색상"]}
                        bearishColor={plot["음봉일 때 히스토그램 색상"]}
                        indicatorName={indicatorName}
                        initialData={indicatorDataMap[indicatorName] || []}
                        tickSize={tickSize}
                        precision={precision}
                    />
                );
                componentCount++;
            } else if (plotType === "선") {
                comps.push(
                    <LineSeriesTemplate
                        key={indicatorName}
                        ref={el => {
                            indicatorSeriesRefs.current[indicatorName] = el
                        }}
                        chart={chart}
                        paneIndex={paneIndex}
                        lineColor={plot["선 색상"]}
                        lineStyle={lineStyle}
                        lineWidth={lineWidth}
                        lineType={lineType}
                        pointMarkersVisible={pointMarkersVisible}
                        pointMarkersRadius={pointMarkersRadius}
                        indicatorName={indicatorName}
                        initialData={indicatorDataMap[indicatorName] || []}
                        tickSize={tickSize}
                        precision={precision}
                    />
                );
                componentCount++;
            }
        });

        setSeriesComponents(comps);
    }, [chart, config, indicatorDataMap, tickSize, precision]);

    return <div style={{opacity: isVisible ? 1 : 0, transition: 'opacity 0.3s ease'}}>
        {seriesComponents}
    </div>;
};

export default IndicatorSeriesContainer;
