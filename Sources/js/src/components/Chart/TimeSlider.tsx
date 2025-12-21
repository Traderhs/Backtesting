import React, { useEffect, useRef, useState } from "react";
import { IChartApi } from "lightweight-charts";
import "./TimeSlider.css";

interface TimeSliderProps {
    chart: IChartApi | null;
    candleStickData: any[]; // 각 데이터 객체는 time 속성을 포함 (숫자 또는 ISO 문자열)
    containerRef: React.RefObject<HTMLDivElement | null>;
    isLocked?: boolean; // 슬라이더 이동 잠금 여부
    targetTime?: number; // 목표 시간 (타임스탬프)
}

const TimeSlider: React.FC<TimeSliderProps> = ({ chart, candleStickData, containerRef, isLocked, targetTime }) => {
    // 슬라이더 값(state)은 candleStickData의 인덱스(0 ~ length-1)를 사용합니다.
    const [sliderValue, setSliderValue] = useState(0);
    const sliderRef = useRef<HTMLInputElement>(null);
    const sliderDateRef = useRef<HTMLDivElement>(null);
    const userDraggedRef = useRef<boolean>(false); // 사용자가 드래그 중인지 추적

    // 특정 시간으로 슬라이더 이동 (타임스탬프 기준)
    const moveToTime = (timestamp: number) => {
        if (!chart || candleStickData.length === 0) {
            return;
        }
        
        // 해당 timestamp에 가장 가까운 캔들 찾기
        let targetIndex = 0;
        let closestDiff = Number.MAX_SAFE_INTEGER;
        
        candleStickData.forEach((candle, index) => {
            const candleTime = typeof candle.time === "string" 
                ? new Date(candle.time).getTime() / 1000
                : candle.time;
            
            const diff = Math.abs(candleTime - timestamp);
            if (diff < closestDiff) {
                closestDiff = diff;
                targetIndex = index;
            }
        });
        
        // 슬라이더 값 업데이트
        setSliderValue(targetIndex);
        
        // 차트의 visibleRange도 함께 업데이트
        if (chart) {
            const timeScale = chart.timeScale();
            const logicalRange = timeScale.getVisibleLogicalRange();
            
            if (logicalRange) {
                const visibleCount = logicalRange.to - logicalRange.from;
                const newFrom = targetIndex - visibleCount / 2;
                const newTo = targetIndex + visibleCount / 2;
                
                // 차트 위치 업데이트
                timeScale.setVisibleLogicalRange({ from: newFrom, to: newTo });
            }
        }
    };

    // targetTime prop이 변경되면 슬라이더 위치 업데이트 (targetTime 변경 시에만)
    useEffect(() => {
        if (targetTime !== undefined) {
            // targetTime이 변경될 때마다 moveToTime 호출
            moveToTime(targetTime);
        }
    }, [targetTime]); // candleStickData 의존성 제거

    // 슬라이더 레이아웃 업데이트: left/right price 스케일 너비 반영
    const updateSliderLayout = () => {
        if (!sliderRef.current || !chart) {
            return;
        }

        const timeScale = chart.timeScale();
        if (!timeScale) {
            return;
        }

        const totalCount = candleStickData.length;

        const leftScale = chart.priceScale("left");
        const rightScale = chart.priceScale("right");
        if (!leftScale || !rightScale) {
            return;
        }

        const leftWidth = leftScale.width();
        const rightWidth = rightScale.width();
        const timeWidth = timeScale.width();

        sliderRef.current.min = "0";
        sliderRef.current.max = (totalCount - 1).toString();
        
        // 현재 슬라이더 값도 유효한 범위 내에 있는지 확인하고 필요시 조정
        if (sliderValue < 0 || sliderValue > totalCount - 1) {
            const safeValue = Math.max(0, Math.min(sliderValue, totalCount - 1));
            setSliderValue(safeValue);
        }
        
        sliderRef.current.style.left = `${leftWidth}px`;
        sliderRef.current.style.right = `${rightWidth}px`;
        sliderRef.current.style.width = `${timeWidth}px`;
    };

    // 날짜 라벨(tooltip) 업데이트
    const updateSliderDate = () => {
        if (!sliderDateRef.current || candleStickData.length === 0) {
            return;
        }
        
        // 더 강력한 경계 검사
        const safeIndex = Math.max(0, Math.min(Math.floor(sliderValue), candleStickData.length - 1));
        
        // 인덱스가 유효한지 확인
        if (safeIndex < 0 || !candleStickData[safeIndex]) {
            return;
        }
        
        const bar = candleStickData[safeIndex];
        if (!bar || bar.time === undefined) {
            return;
        }
        
        const date = new Date(typeof bar.time === "string" ? bar.time : bar.time * 1000);
        const days = ["일", "월", "화", "수", "목", "금", "토"];
        sliderDateRef.current.innerText =
            `${date.getUTCFullYear()}-${String(date.getUTCMonth() + 1).padStart(2, "0")}-${String(date.getUTCDate()).padStart(2, "0")} ` +
            `${String(date.getUTCHours()).padStart(2, "0")}:${String(date.getUTCMinutes()).padStart(2, "0")} ` +
            `(${days[date.getUTCDay()]})`;

        // tooltip 위치 재조정
        if (sliderRef.current && containerRef.current && sliderDateRef.current && chart) {
            const containerRect = containerRef.current.getBoundingClientRect();
            const sliderRect = sliderRef.current.getBoundingClientRect();
            const minVal = parseInt(sliderRef.current.min);
            const maxVal = parseInt(sliderRef.current.max);
            const percent = (sliderValue - minVal) / (maxVal - minVal);
            const thumbWidth = 120; // CSS에 지정한 thumb 너비 (TimeSlider.css 참고)
            const x = sliderRect.left - containerRect.left + percent * (sliderRect.width - thumbWidth) + thumbWidth / 2;
            
            // 실제 너비를 측정하기 위해 일시적으로 표시
            const originalDisplay = sliderDateRef.current.style.display;
            sliderDateRef.current.style.display = 'block';

            const tooltipRect = sliderDateRef.current.getBoundingClientRect();
            sliderDateRef.current.style.display = originalDisplay;
            
            const tooltipWidth = tooltipRect.width;
            
            // 차트의 가격 축 너비 가져오기
            const rightScale = chart.priceScale('right');
            const rightScaleWidth = rightScale ? rightScale.width() : 0;
            const safetyMargin = 1; // 오른쪽 경계에 대한 안전 마진 (픽셀 단위)
            const adjustedRightScaleWidth = rightScaleWidth + safetyMargin;
            
            // 컨테이너의 실제 가용 너비 (오른쪽 가격 축 및 안전 마진 고려)
            const effectiveContainerWidth = containerRect.width - adjustedRightScaleWidth;
            
            // 왼쪽 경계를 벗어나는 경우
            if (x - tooltipWidth / 2 < 0) {
                sliderDateRef.current.style.left = '0px';
                sliderDateRef.current.style.right = 'auto';
                sliderDateRef.current.style.transform = 'none'; // transform 제거
            } 
            // 오른쪽 경계를 벗어나는 경우 (가격 축 및 안전 마진 고려)
            else if (x + tooltipWidth / 2 > effectiveContainerWidth) {
                sliderDateRef.current.style.left = 'auto';
                sliderDateRef.current.style.right = `${adjustedRightScaleWidth}px`; // 조정된 가격 축 너비 사용
                sliderDateRef.current.style.transform = 'none'; // transform 제거
            } 
            // 정상적인 경우
            else {
                sliderDateRef.current.style.left = `${x}px`;
                sliderDateRef.current.style.right = 'auto';
                sliderDateRef.current.style.transform = 'translateX(-50%)';
            }
        }
    };

    // 차트의 visible range 변경 시 슬라이더 중앙 바의 인덱스로 업데이트 (양방향 동기화)
    useEffect(() => {
        const updateFromVisibleRange = () => {
            if (!chart) return; 
            
            // isLocked가 true이더라도 사용자가 차트를 직접 드래그 중일 때는 업데이트 허용
            // 데이터 로드 중(isLocked=true)이고 사용자 드래그가 아닌 경우에만 업데이트 무시
            if (isLocked && !userDraggedRef.current) return;
            
            const logicalRange = chart.timeScale().getVisibleLogicalRange();
            if (!logicalRange) return;
            const center = Math.round((logicalRange.from + logicalRange.to) / 2);
            
            // center 값이 유효한 범위(0 ~ candleStickData.length-1) 내에 있도록 제한
            const safeCenter = Math.max(0, Math.min(center, candleStickData.length - 1));
            setSliderValue(safeCenter);
        };

        updateFromVisibleRange();
        if (chart) {
            // 차트의 마우스 이벤트를 추적하여 사용자 드래그 감지
            const chartElement = chart.chartElement();
            if (chartElement) {
                // 마우스 다운 시 사용자 드래그 시작으로 간주
                const handleMouseDown = () => {
                    userDraggedRef.current = true;
                };
                
                // 마우스 업 시 사용자 드래그 종료로 간주
                const handleMouseUp = () => {
                    userDraggedRef.current = false;
                };
                
                chartElement.addEventListener('mousedown', handleMouseDown);
                document.addEventListener('mouseup', handleMouseUp);
                
                // 이벤트 구독
                chart.timeScale().subscribeVisibleLogicalRangeChange(updateFromVisibleRange);
                
                return () => {
                    chartElement.removeEventListener('mousedown', handleMouseDown);
                    document.removeEventListener('mouseup', handleMouseUp);
                    chart.timeScale().unsubscribeVisibleLogicalRangeChange(updateFromVisibleRange);
                };
            } else {
                chart.timeScale().subscribeVisibleLogicalRangeChange(updateFromVisibleRange);

                return () => {
                    chart.timeScale().unsubscribeVisibleLogicalRangeChange(updateFromVisibleRange);
                };
            }
        }
        
        return () => {};
    }, [chart, isLocked, candleStickData.length]);

    // 슬라이더 변경 시 차트의 visible range 업데이트
    const handleSliderChange = (e: React.ChangeEvent<HTMLInputElement>) => {
        // 입력값이 유효한 범위(0 ~ candleStickData.length-1) 내에 있도록 제한
        const rawValue = Number(e.target.value);
        const value = Math.max(0, Math.min(rawValue, candleStickData.length - 1));
        
        setSliderValue(value);
        if (!chart) {
            return;
        }

        const lr = chart.timeScale().getVisibleLogicalRange();
        if (!lr) {
            return;
        }

        const visibleCount = lr.to - lr.from;
        const newFrom = value - visibleCount / 2;
        const newTo = value + visibleCount / 2;

        chart.timeScale().setVisibleLogicalRange({ from: newFrom, to: newTo });
    };

    // 슬라이더 드래그 시작 시 툴팁 표시
    const handleSliderMouseDown = () => {
        if (sliderDateRef.current) {
            sliderDateRef.current.style.display = "block";
        }
    };

    // 슬라이더 드래그 종료 시 툴팁 숨기기
    const handleSliderMouseUp = () => {
        if (sliderDateRef.current) {
            sliderDateRef.current.style.display = "none";
        }
    };

    // sliderValue나 candleStickData, chart 변경 시 레이아웃과 툴팁 갱신
    useEffect(() => {
        updateSliderLayout();
        updateSliderDate();
    }, [sliderValue, candleStickData, chart, isLocked]);

    // 창 크기 변경 시 레이아웃과 툴팁 위치 재설정
    useEffect(() => {
        window.addEventListener("resize", updateSliderLayout);
        window.addEventListener("resize", updateSliderDate);

        return () => {
            window.removeEventListener("resize", updateSliderLayout);
            window.removeEventListener("resize", updateSliderDate);
        };
    }, []);

    return (
        <>
            <input
                id="timeSlider"
                type="range"
                step="1"
                ref={sliderRef}
                className="time-slider"
                value={sliderValue}
                onChange={handleSliderChange}
                onMouseDown={handleSliderMouseDown}
                onMouseUp={handleSliderMouseUp}
                onTouchStart={handleSliderMouseDown}
                onTouchEnd={handleSliderMouseUp}
            />
            <div id="sliderDate" ref={sliderDateRef} className="slider-date" style={{ display: "none" }}></div>
        </>
    );
};

export default TimeSlider;
