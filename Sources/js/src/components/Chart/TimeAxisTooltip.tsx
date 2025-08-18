import React, {useEffect, useRef, useState} from "react";
import {IChartApi} from "lightweight-charts";
import "./TimeAxisTooltip.css";

interface Candle {
    time: number | string;
    open: number;
    high: number;
    low: number;
    close: number;
    volume: number;
}

interface TimeAxisTooltipProps {
    chart: IChartApi | null;
    candleStickData: Candle[];
    containerRef: React.RefObject<HTMLDivElement | null>;
}

const TimeAxisTooltip: React.FC<TimeAxisTooltipProps> = ({chart, candleStickData, containerRef}) => {
    const tooltipRef = useRef<HTMLDivElement>(null);
    // 차트 크로스헤어가 차트 밖에 있거나 데이터가 없을 때는 숨기기 위해 상태로 제어
    const [visible, setVisible] = useState<boolean>(false);
    const [tooltipText, setTooltipText] = useState<string>("");

    useEffect(() => {
        if (!chart) return;

        // crosshair 이벤트
        const onCrosshairMove = (param: any) => {
            if (!tooltipRef.current || !containerRef.current) return;

            const {point, time} = param;

            // 차트 영역 밖이면 툴팁 숨기기
            if (!point || !time) {
                setVisible(false);
                return;
            }

            // time에 해당하는 캔들 찾기 (없으면 마지막 캔들)
            let currentIndex = candleStickData.findIndex(item => item.time === time);
            if (currentIndex === -1) {
                currentIndex = candleStickData.length - 1;
            }

            // 캔들 데이터 배열이 비어있는지 확인
            if (currentIndex < 0 || !candleStickData[currentIndex]) {
                setVisible(false);
                return;
            }

            const bar = candleStickData[currentIndex];
            if (!bar) {
                setVisible(false);
                return;
            }

            // 날짜 포맷 생성
            const dateObj = (typeof bar.time === "string")
                ? new Date(bar.time)
                : new Date(bar.time * 1000);

            const year = dateObj.getUTCFullYear();
            const month = String(dateObj.getUTCMonth() + 1).padStart(2, "0");
            const day = String(dateObj.getUTCDate()).padStart(2, "0");
            const hours = String(dateObj.getUTCHours()).padStart(2, "0");
            const minutes = String(dateObj.getUTCMinutes()).padStart(2, "0");
            const seconds = String(dateObj.getUTCSeconds()).padStart(2, "0");

            const weekdays = ["일", "월", "화", "수", "목", "금", "토"];
            const dayOfWeek = weekdays[dateObj.getUTCDay()];

            // 예) 2025-03-18 15:02:00 (월)
            const formatted = `${year}-${month}-${day} ${hours}:${minutes}:${seconds} (${dayOfWeek})`;
            setTooltipText(formatted);

            // 툴팁의 위치 조정 (차트 컨테이너 기준)
            const containerRect = containerRef.current.getBoundingClientRect();
            const tooltipRect = tooltipRef.current.getBoundingClientRect();
            const chartLeftScaleWidth = chart.priceScale("left")?.width() || 0;
            const chartTimeScaleWidth = chart.timeScale().width() || 0;

            // x축 기준으로 중앙 정렬
            let left = point.x + chartLeftScaleWidth - tooltipRect.width / 2;
            // 좌우 경계 보정
            const maxLeft = chartLeftScaleWidth + chartTimeScaleWidth - tooltipRect.width;
            if (left < chartLeftScaleWidth) left = chartLeftScaleWidth;         // 왼쪽 경계
            if (left > maxLeft) left = maxLeft;                                 // 오른쪽 경계

            // 하단(조금 위) 위치 → 차트 높이 - 툴팁 높이 - 약간의 여유(2px 등)
            const top = containerRect.height - tooltipRect.height - 1;

            tooltipRef.current.style.left = `${left}px`;
            tooltipRef.current.style.top = `${top}px`;

            setVisible(true);
        };

        chart.subscribeCrosshairMove(onCrosshairMove);

        return () => {
            chart.unsubscribeCrosshairMove(onCrosshairMove);
        };
    }, [chart, candleStickData, containerRef]);

    return (
        <div
            ref={tooltipRef}
            className="time-axis-tooltip"
            style={{
                display: visible ? "block" : "none",
            }}
        >
            {tooltipText}
        </div>
    );
};

export default TimeAxisTooltip;
