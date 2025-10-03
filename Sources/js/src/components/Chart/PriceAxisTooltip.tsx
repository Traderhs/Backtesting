import React, {useEffect, useRef, useState} from "react";
import {IChartApi} from "lightweight-charts";
import "./PriceAxisTooltip.css";

interface PriceAxisTooltipProps {
    chart: IChartApi | null;
    containerRef: React.RefObject<HTMLDivElement | null>;
    pricePrecision: number;
}

const PriceAxisTooltip: React.FC<PriceAxisTooltipProps> = ({chart, containerRef, pricePrecision}) => {
    const tooltipRef = useRef<HTMLDivElement>(null);
    const [visible, setVisible] = useState<boolean>(false);
    const [tooltipText, setTooltipText] = useState<string>("");
    const [tooltipPosition, setTooltipPosition] = useState({top: 0, left: 0});

    useEffect(() => {
        if (!chart || !containerRef.current) {
            return;
        }

        // 툴팁 엘리먼트 생성 (마운트 시 한 번만)
        if (!tooltipRef.current) {
            console.error("[PriceAxisTooltip] 툴팁 참조가 생성되지 않음");
            return;
        }

        // crosshair 이벤트
        const onCrosshairMove = (param: any) => {
            if (!tooltipRef.current || !containerRef.current) {
                console.warn("[PriceAxisTooltip] 툴팁 또는 컨테이너 참조 없음");
                return;
            }

            const {point, paneIndex} = param;

            // 차트 영역 밖이면 툴팁 숨기기
            if (!point) {
                setVisible(false);
                return;
            }

            try {
                const currentPaneIndex = paneIndex !== undefined ? paneIndex : 0;

                // 현재 페인의 시리즈 가져오기
                const panes = chart.panes();
                
                if (!panes || !panes[currentPaneIndex]) {
                    console.warn("[PriceAxisTooltip] 현재 페인을 찾을 수 없음:", { 
                        hasPanes: !!panes, 
                        currentPaneIndex 
                    });
                    setVisible(false);
                    return;
                }

                const currentPane = panes[currentPaneIndex];
                const seriesArray = currentPane.getSeries();

                if (!seriesArray || seriesArray.length === 0) {
                    console.warn("[PriceAxisTooltip] 시리즈가 없음");
                    setVisible(false);
                    return;
                }

                // 적합한 시리즈 찾기
                // 1. 캔들스틱 시리즈 우선 사용
                // 2. 메인 시리즈 확인
                // 3. 모든 시리즈를 순회하며 유효한 가격 찾기
                let price: number | null = null;

                // 메인 시리즈 확인 (window.mainSeries가 있는 경우)
                if (panes.length <= 1 && window.mainSeries && currentPaneIndex === 0) {
                    try {
                        price = window.mainSeries.coordinateToPrice(point.y);
                    } catch (e) {
                        console.error("[PriceAxisTooltip] 메인 시리즈 좌표 변환 오류:", e);
                        price = null;
                    }
                }

                // 메인 시리즈에서 변환 실패했거나 값이 유효하지 않은 경우
                if (price === null || price === undefined) {
                    // 시리즈 유형에 따라 우선순위 부여하여 가격 찾기
                    // 우선순위: Candlestick > Bar > Area > Line > 기타
                    const seriesTypes = ["Candlestick", "Bar", "Area", "Line", "Baseline", "Histogram"];
                    
                    for (const typeToFind of seriesTypes) {
                        // 해당 유형의 시리즈 찾기
                        for (const series of seriesArray) {
                            if (series.seriesType() === typeToFind) {
                                try {
                                    const tempPrice = series.coordinateToPrice(point.y);
                                    
                                    if (tempPrice !== null && tempPrice !== undefined) {
                                        price = tempPrice;
                                        break;
                                    }
                                } catch (e) {
                                    console.error(`[PriceAxisTooltip] ${typeToFind} 시리즈 좌표 변환 오류:`, e);
                                }
                            }
                        }
                        
                        // 가격을 찾았으면 반복 중단
                        if (price !== null && price !== undefined) {
                            break;
                        }
                    }
                    
                    // 특정 유형 시리즈에서 가격을 못 찾은 경우, 모든 시리즈 순회
                    if (price === null || price === undefined) {
                        for (let i = 0; i < seriesArray.length; i++) {
                            try {
                                const tempPrice = seriesArray[i].coordinateToPrice(point.y);
                                
                                if (tempPrice !== null && tempPrice !== undefined) {
                                    price = tempPrice;
                                    break;
                                }
                            } catch (e) {
                                console.error(`[PriceAxisTooltip] 시리즈[${i}] 좌표 변환 오류:`, e);
                            }
                        }
                    }
                }

                // 가격 정보가 여전히 없는 경우
                if (price === null || price === undefined) {
                    console.warn("[PriceAxisTooltip] 유효한 가격 없음, 툴팁 숨김");
                    setVisible(false);
                    return;
                }

                // 가격 포맷 적용
                const formattedPrice = price.toLocaleString(undefined, {
                    minimumFractionDigits: pricePrecision,
                    maximumFractionDigits: pricePrecision
                });

                setTooltipText(formattedPrice);

                // Y좌표 계산 (페인 위치 고려)
                let yPosition = point.y;

                // 페인 높이 계산 및 적용 (현재 페인이 첫 번째가 아닌 경우, 이전 페인들의 높이를 더함)
                if (currentPaneIndex > 0) {
                    try {
                        // 이전 모든 페인의 높이 합계 계산
                        let previousPanesHeight = 0;

                        // 예시:
                        // 인덱스 1 (두 번째 페인): 페인 0의 높이 + 현재 좌표
                        // 인덱스 2 (세 번째 페인): 페인 0의 높이 + 페인 1의 높이 + 현재 좌표
                        for (let i = 0; i < currentPaneIndex; i++) {
                            if (panes[i]) {
                                const paneHeight = panes[i].getHeight() + 1; // 페인 사이에 1px 구분선이 있으므로 1px 더해줌
                                if (!isNaN(paneHeight)) {
                                    previousPanesHeight += paneHeight;
                                }
                            }
                        }

                        // 현재 페인 내 y좌표 + 이전 페인들의 높이 합계
                        yPosition = previousPanesHeight + point.y;
                    } catch (e) {
                        console.error("[PriceAxisTooltip] 페인 높이 계산 오류:", e);
                    }
                }

                // 툴팁 위치 계산
                const tooltipRect = tooltipRef.current.getBoundingClientRect();

                // 컨테이너 너비 가져오기
                const containerWidth = containerRef.current.clientWidth;

                // 오른쪽 가격축 너비 가져오기
                const rightPriceScaleWidth = chart.priceScale('right').width();

                setTooltipPosition({
                    top: yPosition - tooltipRect.height / 2,
                    left: containerWidth - rightPriceScaleWidth + 2
                });

                setVisible(true);
            } catch (error: any) {
                console.error("[PriceAxisTooltip] Y축 툴팁 처리 중 오류:", error, error.stack);
                setVisible(false);
            }
        };

        chart.subscribeCrosshairMove(onCrosshairMove);

        return () => {
            chart.unsubscribeCrosshairMove(onCrosshairMove);
        };
    }, [chart, containerRef, pricePrecision]);

    return (
        <div
            ref={tooltipRef}
            className="price-axis-tooltip"
            style={{
                display: visible ? "block" : "none",
                top: `${tooltipPosition.top}px`,
                left: `${tooltipPosition.left}px`
            }}
        >
            {tooltipText}
        </div>
    );
};

export default PriceAxisTooltip; 