/**
 * 모든 전역 차트 캐시를 제거/초기화하는 함수
 */
export function clearChartGlobals(): void {
    try {
        if (typeof window === 'undefined') {
            return;
        }

        // 1단계: 시리즈 dispose (delete 하기 전에)
        try {
            const refs = (window as any).indicatorSeriesRefs || {};
            Object.keys(refs).forEach(k => {
                try {
                    const r = refs[k];
                    if (r && typeof r.dispose === 'function') {
                        r.dispose();
                    }
                } catch (e) {
                    // 무시
                }
            });
        } catch (e) {
            // 무시
        }

        // 2단계: 페인 DIV 정리 (delete 하기 전에)
        try {
            const divs = (window as any).paneIndicatorDivs || {};
            Object.values(divs).forEach((div: any) => {
                try {
                    if (div instanceof HTMLElement && div.parentNode) {
                        div.parentNode.removeChild(div);
                    }
                } catch (e) {
                    // 무시
                }
            });
        } catch (e) {
            // 무시
        }

        // 3단계: 지표 혹은 페인 관련 전역 변수 삭제
        try {
            delete (window as any).indicatorSeriesRefs;
        } catch (e) {
            // 무시
        }

        try {
            delete (window as any).indicatorPaths;
        } catch (e) {
            // 무시
        }

        try {
            delete (window as any).indicatorData;
        } catch (e) {
            // 무시
        }

        try {
            delete (window as any).indicatorSeriesInfo;
        } catch (e) {
            // 무시
        }

        // 페인/레이아웃 관련
        try {
            delete (window as any).paneIndicatorDivs;
        } catch (e) {
            // 무시
        }
        try {
            (window as any).paneCount = undefined;
        } catch (e) {
            // 무시
        }

        // 캔들/메인 시리즈 관련
        try {
            delete (window as any).mainSeries;
        } catch (e) {
            // 무시
        }
        try {
            delete (window as any).mainSeriesBySymbol;
        } catch (e) {
            // 무시
        }

        // 차트/포털 관련
        try {
            delete (window as any).chartRef;
        } catch (e) {
            // 무시
        }
        try {
            delete (window as any).symbol;
        } catch (e) {
            // 무시
        }

        // 캘린더/플로우 관련 플래그
        try {
            (window as any).calendarDataResetComplete = false;
        } catch (e) {
            // 무시
        }
        try {
            (window as any).lastCalendarDateRequest = false;
        } catch (e) {
            // 무시
        }

        // isRecreatingCandleSeries 플래그 초기화
        try {
            (window as any).isRecreatingCandleSeries = false;
        } catch (e) {
            // 무시
        }

        // 렌더링 제어 함수
        try {
            delete (window as any).showIndicators;
        } catch (e) {
            // 무시
        }
    } catch (err) {
        // cleanup은 항상 실패 허용
    }
}
