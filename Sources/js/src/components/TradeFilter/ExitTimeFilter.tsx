import React, {useState, useEffect, useMemo} from "react";
import {useTradeFilter} from "../TradeFilter";
import FilterCalendar from "./FilterCalendar";
import ResetFilterButton from "./ResetFilterButton";
import {RESET_EXIT_TIME_FILTER} from "./FilterResetEvent";
import {Button} from "../ui/button.tsx";
import {motion} from "framer-motion";

// Sidebar와 동일한 애니메이션 변형 정의
const itemVariants = {
    hidden: {opacity: 1, x: 0},
    visible: {
        opacity: 1,
        x: 0
    },
    hover: {
        scale: 1.03,
        borderColor: 'rgba(255, 215, 0, 0.7)',
        boxShadow: '0 0 5px rgba(255, 215, 0, 0.5)',
        transition: {duration: 0.2}
    },
    tap: (custom: { isActive: boolean }) => ({
        scale: 0.98,
        backgroundColor: 'rgba(52, 46, 14, 1)',
        boxShadow: custom.isActive
            ? 'inset 0 0 0 1000px rgba(255, 215, 0, 0.2), 0 0 5px rgba(255, 215, 0, 0.3)'
            : 'inset 0 0 0 1000px rgba(255, 215, 0, 0.15), 0 0 5px rgba(255, 215, 0, 0.3)',
        transition: {duration: 0.1}
    })
};

interface ExitTimeFilterProps {
    timeframe?: string;
}

const ExitTimeFilter: React.FC<ExitTimeFilterProps> = ({timeframe = "1h"}) => {
    const {filter, setFilter, openCalendar, setOpenCalendar, allTrades} = useTradeFilter();
    const [isExitTimeMinOpen, setExitTimeMinOpen] = useState(false);
    const [isExitTimeMaxOpen, setExitTimeMaxOpen] = useState(false);
    const [buttonPressed, setButtonPressed] = useState<string | null>(null);

    // 실제 거래(거래 번호 0 제외)가 존재하는지 여부 확인
    const hasValidTrades = useMemo(() => allTrades.some(trade => Number(trade["거래 번호"]) !== 0), [allTrades]);

    // 청산 시간 관련 모든 필터 초기화 함수
    const resetFilter = () => {
        // 거래 번호 0번을 제외한 실제 거래 데이터만 필터링
        const validTrades = allTrades.filter(trade => {
            const tradeNumber = Number(trade["거래 번호"]);
            return tradeNumber !== 0 && trade["청산 시간"];
        });

        if (validTrades.length === 0) {
            // 거래가 없는 경우에는 아무것도 하지 않음 (TradeFilterProvider가 이미 처리)
            return;
        }

        // 전체 데이터에서 실제 최소/최대 청산 시간 찾기 (GMT 기준으로 처리)
        const exitTimes = validTrades.map(trade => String(trade["청산 시간"])).filter(time => time && time !== "-");
        
        if (exitTimes.length === 0) {
            return;
        }

        // 전체 데이터를 순회하면서 실제 최소/최대값 찾기
        let minExitTimeStr = exitTimes[0];
        let maxExitTimeStr = exitTimes[0];
        for (const time of exitTimes) {
            if (time < minExitTimeStr) minExitTimeStr = time;
            if (time > maxExitTimeStr) maxExitTimeStr = time;
        }

        // ISO 문자열이면 그대로 사용하고, 아니면 GMT로 파싱
        const startTime = minExitTimeStr.includes('T')
            ? minExitTimeStr
            : new Date(minExitTimeStr + 'Z').toISOString(); // Z를 추가하여 GMT로 파싱
        const endTime = maxExitTimeStr.includes('T')
            ? maxExitTimeStr
            : new Date(maxExitTimeStr + 'Z').toISOString(); // Z를 추가하여 GMT로 파싱

        // 고급 필터 계산 - 모든 데이터 선택
        // 연도 범위를 결정하기 위한 최소/최대 연도 찾기
        let minYear = Number.MAX_SAFE_INTEGER;
        let maxYear = Number.MIN_SAFE_INTEGER;

        validTrades.forEach(trade => {
            const exitTimeStr = String(trade["청산 시간"]);
            const exitDate = new Date(exitTimeStr.includes('T') ? exitTimeStr : exitTimeStr + 'Z');
            const year = exitDate.getUTCFullYear();

            minYear = Math.min(minYear, year);
            maxYear = Math.max(maxYear, year);
        });

        // 연도는 첫 거래부터 마지막 거래까지의 범위로 설정(중간 빈 연도 포함)
        const yearsArray = minYear !== Number.MAX_SAFE_INTEGER ?
            Array.from({length: maxYear - minYear + 1}, (_, i) => minYear + i) : [];

        // 나머지는 모든 가능한 값 포함
        const monthsArray = Array.from({length: 12}, (_, i) => i + 1); // 1-12월
        const daysArray = Array.from({length: 31}, (_, i) => i + 1); // 1-31일
        const dayOfWeeksArray = Array.from({length: 7}, (_, i) => i); // 0(일)-6(토)
        const hoursArray = Array.from({length: 24}, (_, i) => i); // 0-23시
        const minutesArray = Array.from({length: 60}, (_, i) => i); // 0-59분
        const secondsArray = Array.from({length: 60}, (_, i) => i); // 0-59초

        // 모든 청산 시간 관련 필터 초기화
        setFilter(prev => ({
            ...prev,
            exitTimeMin: startTime,
            exitTimeMax: endTime,
            exitYears: yearsArray,
            exitMonths: monthsArray,
            exitDays: daysArray,
            exitDayOfWeeks: dayOfWeeksArray,
            exitHours: hoursArray,
            exitMinutes: minutesArray,
            exitSeconds: secondsArray
        }));
    };

    // 컴포넌트 마운트 시 기본값 설정
    useEffect(() => {
        // 이미 필터 값이 설정되어 있으면 무시
        if (filter.exitTimeMin || filter.exitTimeMax) {
            return;
        }

        resetFilter();
    }, [allTrades, filter.exitTimeMin, filter.exitTimeMax, setFilter]);

    // 전역 달력 상태와 동기화
    useEffect(() => {
        if (openCalendar !== 'exitTimeMin' && isExitTimeMinOpen) {
            setExitTimeMinOpen(false);
        }
        if (openCalendar !== 'exitTimeMax' && isExitTimeMaxOpen) {
            setExitTimeMaxOpen(false);
        }
    }, [openCalendar, isExitTimeMinOpen, isExitTimeMaxOpen]);

    // 외부에서 resetFilter를 호출할 수 있도록 이벤트 리스너 등록
    useEffect(() => {
        const handleResetEvent = () => {
            resetFilter();
        };

        // 이벤트 리스너 등록
        document.addEventListener(RESET_EXIT_TIME_FILTER, handleResetEvent);

        // 클린업 함수
        return () => {
            document.removeEventListener(RESET_EXIT_TIME_FILTER, handleResetEvent);
        };
    }, [resetFilter]);

    const formatDate = (dateStr: string | undefined) => {
        if (!dateStr) return "";
        const date = new Date(dateStr);
        // GMT 기준으로 포맷팅
        const year = date.getUTCFullYear();
        const month = String(date.getUTCMonth() + 1).padStart(2, '0');
        const day = String(date.getUTCDate()).padStart(2, '0');
        const hours = String(date.getUTCHours()).padStart(2, '0');
        const minutes = String(date.getUTCMinutes()).padStart(2, '0');
        const seconds = String(date.getUTCSeconds()).padStart(2, '0');
        return `${year}-${month}-${day} ${hours}:${minutes}:${seconds}`;
    };

    const handleExitTimeMinSelected = (date: Date, time: string) => {
        // GMT 기준으로 정확한 ISO string 생성
        const [hours, minutes] = time.split(':').map(Number);
        const utcDate = new Date(Date.UTC(
            date.getUTCFullYear(),
            date.getUTCMonth(),
            date.getUTCDate(),
            hours,
            minutes,
            0,
            0
        ));
        const isoString = utcDate.toISOString();
        setFilter(prev => ({
            ...prev,
            exitTimeMin: isoString
        }));
        setExitTimeMinOpen(false);
        setOpenCalendar(null);
    };

    const handleExitTimeMaxSelected = (date: Date, time: string) => {
        // GMT 기준으로 정확한 ISO string 생성
        const [hours, minutes] = time.split(':').map(Number);
        const utcDate = new Date(Date.UTC(
            date.getUTCFullYear(),
            date.getUTCMonth(),
            date.getUTCDate(),
            hours,
            minutes,
            0,
            0
        ));
        const isoString = utcDate.toISOString();
        setFilter(prev => ({
            ...prev,
            exitTimeMax: isoString
        }));
        setExitTimeMaxOpen(false);
        setOpenCalendar(null);
    };

    const handleExitTimeMinMouseDown = () => {
        setButtonPressed('exitTimeMin');
    };

    const handleExitTimeMinMouseUp = () => {
        if (buttonPressed === 'exitTimeMin') {
            // 이미 열려있으면 닫고, 닫혀있으면 엽니다
            if (isExitTimeMinOpen) {
                setExitTimeMinOpen(false);
                setOpenCalendar(null);
            } else {
                setOpenCalendar('exitTimeMin');
                setExitTimeMinOpen(true);
            }
        }
        setButtonPressed(null);
    };

    const handleExitTimeMinMouseLeave = () => {
        setButtonPressed(null);
    };

    const handleExitTimeMaxMouseDown = () => {
        setButtonPressed('exitTimeMax');
    };

    const handleExitTimeMaxMouseUp = () => {
        if (buttonPressed === 'exitTimeMax') {
            // 이미 열려있으면 닫고, 닫혀있으면 엽니다
            if (isExitTimeMaxOpen) {
                setExitTimeMaxOpen(false);
                setOpenCalendar(null);
            } else {
                setOpenCalendar('exitTimeMax');
                setExitTimeMaxOpen(true);
            }
        }
        setButtonPressed(null);
    };

    const handleExitTimeMaxMouseLeave = () => {
        setButtonPressed(null);
    };

    const handleExitTimeMinClose = () => {
        setExitTimeMinOpen(false);
        setOpenCalendar(null);
    };

    const handleExitTimeMaxClose = () => {
        setExitTimeMaxOpen(false);
        setOpenCalendar(null);
    };

    const getLastSelectedDate = (dateStr: string | undefined) => {
        if (!dateStr) return null;
        // GMT 기준으로 날짜 파싱 (시간대 차이 방지)
        const date = new Date(dateStr);
        return new Date(Date.UTC(
            date.getUTCFullYear(),
            date.getUTCMonth(),
            date.getUTCDate()
        ));
    };

    const getLastSelectedTime = (dateStr: string | undefined) => {
        if (!dateStr) return "00:00";
        const date = new Date(dateStr);
        return `${date.getUTCHours().toString().padStart(2, '0')}:${date.getUTCMinutes().toString().padStart(2, '0')}`;
    };

    // 실제 거래가 없으면 필터 UI를 렌더링하지 않음 (모든 훅 호출 이후)
    if (!hasValidTrades) {
        return null;
    }

    return (
        <div>
            <div className="filter-section-title" style={{display: 'flex', alignItems: 'center', marginBottom: '8px'}}>
                <ResetFilterButton onClick={resetFilter}/>
                <span>청산 시간</span>
            </div>

            <div className="time-filter-buttons">
                <div className="time-filter-button-group-vertical">
                    <motion.div
                        variants={itemVariants}
                        initial="hidden"
                        animate="visible"
                        whileHover="hover"
                        whileTap="tap"
                        custom={{isActive: isExitTimeMinOpen}}
                        className={`gpu-accelerated sidebar-button-container ${isExitTimeMinOpen ? "active-sidebar-button" : ""}`}
                        style={{
                            width: '100%',
                            height: '55px',
                            borderRadius: '8px',
                            overflow: 'visible',
                            display: 'flex'
                        }}
                    >
                        <Button
                            variant="ghost"
                            className="time-filter-button"
                            onMouseDown={handleExitTimeMinMouseDown}
                            onMouseUp={handleExitTimeMinMouseUp}
                            onMouseLeave={handleExitTimeMinMouseLeave}
                            style={{
                                display: 'flex',
                                flexDirection: 'column' as const,
                                gap: '3px',
                                padding: '8px 12px',
                                minHeight: '55px',
                                whiteSpace: 'nowrap',
                                width: '100%',
                                backgroundColor: isExitTimeMinOpen ? 'rgba(255, 215, 0, 0.4)' : 'rgba(17, 17, 17, 1)',
                                borderColor: isExitTimeMinOpen ? 'rgba(255, 215, 0, 0.8)' : 'rgba(255, 215, 0, 0.4)',
                                borderWidth: '1px',
                                borderStyle: 'solid',
                                borderRadius: '8px',
                                fontWeight: isExitTimeMinOpen ? 600 : 400,
                                color: 'white',
                                outline: 'none',
                                margin: 0,
                                flex: 1
                            }}
                        >
                            <span style={{fontSize: '14px', fontWeight: '500'}}>시작 시간</span>
                            <span style={{fontSize: '13px', opacity: 0.8}}>
                                {formatDate(filter.exitTimeMin)}
                            </span>
                        </Button>
                    </motion.div>

                    <motion.div
                        variants={itemVariants}
                        initial="hidden"
                        animate="visible"
                        whileHover="hover"
                        whileTap="tap"
                        custom={{isActive: isExitTimeMaxOpen}}
                        className={`gpu-accelerated sidebar-button-container ${isExitTimeMaxOpen ? "active-sidebar-button" : ""}`}
                        style={{
                            width: '100%',
                            height: '55px',
                            borderRadius: '8px',
                            overflow: 'visible',
                            display: 'flex'
                        }}
                    >
                        <Button
                            variant="ghost"
                            className="time-filter-button"
                            onMouseDown={handleExitTimeMaxMouseDown}
                            onMouseUp={handleExitTimeMaxMouseUp}
                            onMouseLeave={handleExitTimeMaxMouseLeave}
                            style={{
                                display: 'flex',
                                flexDirection: 'column' as const,
                                gap: '3px',
                                padding: '8px 12px',
                                minHeight: '55px',
                                whiteSpace: 'nowrap',
                                width: '100%',
                                backgroundColor: isExitTimeMaxOpen ? 'rgba(255, 215, 0, 0.4)' : 'rgba(17, 17, 17, 1)',
                                borderColor: isExitTimeMaxOpen ? 'rgba(255, 215, 0, 0.8)' : 'rgba(255, 215, 0, 0.4)',
                                borderWidth: '1px',
                                borderStyle: 'solid',
                                borderRadius: '8px',
                                fontWeight: isExitTimeMaxOpen ? 600 : 400,
                                color: 'white',
                                outline: 'none',
                                margin: 0,
                                flex: 1
                            }}
                        >
                            <span style={{fontSize: '14px', fontWeight: '500'}}>종료 시간</span>
                            <span style={{fontSize: '13px', opacity: 0.8}}>
                                {formatDate(filter.exitTimeMax)}
                            </span>
                        </Button>
                    </motion.div>
                </div>
            </div>

            {/* 청산 시작 시간 달력 */}
            {isExitTimeMinOpen && (
                <FilterCalendar
                    title="[청산 시간]  시작 시간"
                    timeframe={timeframe}
                    onClose={handleExitTimeMinClose}
                    onDateTimeSelected={handleExitTimeMinSelected}
                    lastSelectedDate={getLastSelectedDate(filter.exitTimeMin)}
                    lastSelectedTime={getLastSelectedTime(filter.exitTimeMin)}
                />
            )}

            {/* 청산 끝 시간 달력 */}
            {isExitTimeMaxOpen && (
                <FilterCalendar
                    title="[청산 시간]  종료 시간"
                    timeframe={timeframe}
                    onClose={handleExitTimeMaxClose}
                    onDateTimeSelected={handleExitTimeMaxSelected}
                    lastSelectedDate={getLastSelectedDate(filter.exitTimeMax)}
                    lastSelectedTime={getLastSelectedTime(filter.exitTimeMax)}
                />
            )}
        </div>
    );
};

export default ExitTimeFilter;
