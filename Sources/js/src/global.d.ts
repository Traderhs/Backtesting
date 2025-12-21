import {ISeriesApi, SeriesType} from "lightweight-charts";
import React from "react";

declare global {
    interface Window {
        // Plotly 프리로딩
        plotlyPreload: Promise<typeof Plot | null>;

        // 지표의 개수를 저장
        paneCount?: number;

        // 지표 데이터 경로를 저장하는 객체 (key: 지표명, value: 경로)
        indicatorPaths: { [key: string]: string };

        // 지표 시리즈의 참조를 저장하는 객체 (key: 지표명, value: 시리즈 참조)
        indicatorSeriesRefs: { [key: string]: any };
        
        // 지표 표시 함수 (모든 렌더링이 완료된 후 지표를 보이게 함)
        showIndicators?: () => void;

        // 캔들스틱 시리즈 재생성 중복 실행 방지 플래그
        isRecreatingCandleSeries?: boolean;

        // 차트 참조를 저장하는 전역 변수
        chartRef: React.RefObject<IChartApi | null>;
        
        // 캔들스틱 메인 시리즈 참조를 저장하는 전역 변수
        mainSeries?: ISeriesApi<SeriesType>;
        mainSeriesBySymbol: { [symbol: string]: any };

        // 시리즈별 정보(라인 색, 지표명 등)
        indicatorSeriesInfo?: {
            [key: string]: {
                name: string;
                pane: number;
                seriesType: 'Area' | 'Baseline' | 'Histogram' | 'Line';
                lineColor?: string;
                baseValue?: number;
                topLineColor?: string;
                bottomLineColor?: string;
            }
        };
        
        // 지표 데이터 관련 전역 변수
        indicatorData?: {
            [key: string]: {
                time: number | string;
                value: number | null;
                color?: string;
            }[]
        };
        
        // 페인 지표 DIV 관련 전역 변수
        paneIndicatorDivs?: {
            [key: number]: HTMLElement;
        };
    }
}

// '사용하지 않는 인터페이스 Window' 경고 제거용
void (window as Window);

export {};
