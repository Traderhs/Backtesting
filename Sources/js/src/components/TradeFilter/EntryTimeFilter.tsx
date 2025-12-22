import React, {useState, useEffect, useMemo} from "react";
import {useTradeFilter} from "../TradeFilter";
import FilterCalendar from "./FilterCalendar";
import ResetFilterButton from "./ResetFilterButton";
import {RESET_ENTRY_TIME_FILTER} from "./FilterResetEvent";
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

interface EntryTimeFilterProps {
    timeframe?: string;
}

const EntryTimeFilter: React.FC<EntryTimeFilterProps> = ({timeframe = "1h"}) => {
    const {filter, setFilter, openCalendar, setOpenCalendar, allTrades} = useTradeFilter();
    const [isEntryTimeMinOpen, setEntryTimeMinOpen] = useState(false);
    const [isEntryTimeMaxOpen, setEntryTimeMaxOpen] = useState(false);
    const [buttonPressed, setButtonPressed] = useState<string | null>(null);

    // 실제 거래(거래 번호 0 제외)가 존재하는지 여부 확인
    const hasValidTrades = useMemo(() => allTrades.some(trade => Number(trade["거래 번호"]) !== 0), [allTrades]);

    // 진입 시간 관련 모든 필터 초기화 함수
    const resetFilter = () => {
        // 거래 번호 0번을 제외한 실제 거래 데이터만 필터링
        const validTrades = allTrades.filter(trade => {
            const tradeNumber = Number(trade["거래 번호"]);
            return tradeNumber !== 0 && trade["진입 시간"];
        });

        if (validTrades.length === 0) {
            // 거래가 없는 경우에는 아무것도 하지 않음 (TradeFilterProvider가 이미 처리)
            return;
        }

        // 전체 데이터에서 실제 최소/최대 진입 시간 찾기 (GMT 기준으로 처리)
        const entryTimes = validTrades.map(trade => String(trade["진입 시간"])).filter(time => time && time !== "-");

        if (entryTimes.length === 0) {
            return;
        }

        // 전체 데이터를 순회하면서 실제 최소/최대값 찾기
        let minEntryTimeStr = entryTimes[0];
        let maxEntryTimeStr = entryTimes[0];
        for (const time of entryTimes) {
            if (time < minEntryTimeStr) minEntryTimeStr = time;
            if (time > maxEntryTimeStr) maxEntryTimeStr = time;
        }

        // ISO 문자열이면 그대로 사용하고, 아니면 GMT로 파싱
        const startTime = minEntryTimeStr.includes('T')
            ? minEntryTimeStr
            : new Date(minEntryTimeStr + 'Z').toISOString(); // Z를 추가하여 GMT로 파싱
        const endTime = maxEntryTimeStr.includes('T')
            ? maxEntryTimeStr
            : new Date(maxEntryTimeStr + 'Z').toISOString(); // Z를 추가하여 GMT로 파싱

        // 고급 필터 계산 - 모든 데이터 선택
        // 연도 범위를 결정하기 위한 최소/최대 연도 찾기
        let minYear = Number.MAX_SAFE_INTEGER;
        let maxYear = Number.MIN_SAFE_INTEGER;

        validTrades.forEach(trade => {
            const entryTimeStr = String(trade["진입 시간"]);
            const entryDate = new Date(entryTimeStr.includes('T') ? entryTimeStr : entryTimeStr + 'Z');
            const year = entryDate.getUTCFullYear();

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

        // 모든 진입 시간 관련 필터 초기화
        setFilter(prev => ({
            ...prev,
            entryTimeMin: startTime,
            entryTimeMax: endTime,
            entryYears: yearsArray,
            entryMonths: monthsArray,
            entryDays: daysArray,
            entryDayOfWeeks: dayOfWeeksArray,
            entryHours: hoursArray,
            entryMinutes: minutesArray,
            entrySeconds: secondsArray
        }));
    };

    // 컴포넌트 마운트 시 기본값 설정
    useEffect(() => {
        // 이미 필터 값이 설정되어 있으면 무시
        if (filter.entryTimeMin || filter.entryTimeMax) {
            return;
        }

        resetFilter();
    }, [allTrades, filter.entryTimeMin, filter.entryTimeMax, setFilter]);

    // 전역 달력 상태와 동기화
    useEffect(() => {
        if (openCalendar !== 'entryTimeMin' && isEntryTimeMinOpen) {
            setEntryTimeMinOpen(false);
        }
        if (openCalendar !== 'entryTimeMax' && isEntryTimeMaxOpen) {
            setEntryTimeMaxOpen(false);
        }
    }, [openCalendar, isEntryTimeMinOpen, isEntryTimeMaxOpen]);

    // 외부에서 resetFilter를 호출할 수 있도록 이벤트 리스너 등록
    useEffect(() => {
        const handleResetEvent = () => {
            resetFilter();
        };

        // 이벤트 리스너 등록
        document.addEventListener(RESET_ENTRY_TIME_FILTER, handleResetEvent);

        // 클린업 함수
        return () => {
            document.removeEventListener(RESET_ENTRY_TIME_FILTER, handleResetEvent);
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

    const handleEntryTimeMinSelected = (date: Date, time: string) => {
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
            entryTimeMin: isoString
        }));
        setEntryTimeMinOpen(false);
        setOpenCalendar(null);
    };

    const handleEntryTimeMaxSelected = (date: Date, time: string) => {
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
            entryTimeMax: isoString
        }));
        setEntryTimeMaxOpen(false);
        setOpenCalendar(null);
    };

    const handleEntryTimeMinMouseDown = () => {
        setButtonPressed('entryTimeMin');
    };

    const handleEntryTimeMinMouseUp = () => {
        if (buttonPressed === 'entryTimeMin') {
            // 이미 열려있으면 닫고, 닫혀있으면 엽니다
            if (isEntryTimeMinOpen) {
                setEntryTimeMinOpen(false);
                setOpenCalendar(null);
            } else {
                setOpenCalendar('entryTimeMin');
                setEntryTimeMinOpen(true);
            }
        }
        setButtonPressed(null);
    };

    const handleEntryTimeMinMouseLeave = () => {
        setButtonPressed(null);
    };

    const handleEntryTimeMaxMouseDown = () => {
        setButtonPressed('entryTimeMax');
    };

    const handleEntryTimeMaxMouseUp = () => {
        if (buttonPressed === 'entryTimeMax') {
            // 이미 열려있으면 닫고, 닫혀있으면 엽니다
            if (isEntryTimeMaxOpen) {
                setEntryTimeMaxOpen(false);
                setOpenCalendar(null);
            } else {
                setOpenCalendar('entryTimeMax');
                setEntryTimeMaxOpen(true);
            }
        }
        setButtonPressed(null);
    };

    const handleEntryTimeMaxMouseLeave = () => {
        setButtonPressed(null);
    };

    const handleEntryTimeMinClose = () => {
        setEntryTimeMinOpen(false);
        setOpenCalendar(null);
    };

    const handleEntryTimeMaxClose = () => {
        setEntryTimeMaxOpen(false);
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
                <span>진입 시간</span>
            </div>

            <div className="time-filter-buttons">
                <div className="time-filter-button-group-vertical">
                    <motion.div
                        variants={itemVariants}
                        initial="hidden"
                        animate="visible"
                        whileHover="hover"
                        whileTap="tap"
                        custom={{isActive: isEntryTimeMinOpen}}
                        className={`sidebar-button-container ${isEntryTimeMinOpen ? "active-sidebar-button" : ""}`}
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
                            onMouseDown={handleEntryTimeMinMouseDown}
                            onMouseUp={handleEntryTimeMinMouseUp}
                            onMouseLeave={handleEntryTimeMinMouseLeave}
                            style={{
                                display: 'flex',
                                flexDirection: 'column' as const,
                                gap: '3px',
                                padding: '8px 12px',
                                minHeight: '55px',
                                whiteSpace: 'nowrap',
                                width: '100%',
                                backgroundColor: isEntryTimeMinOpen ? 'rgba(255, 215, 0, 0.4)' : 'rgba(17, 17, 17, 1)',
                                borderColor: isEntryTimeMinOpen ? 'rgba(255, 215, 0, 0.8)' : 'rgba(255, 215, 0, 0.4)',
                                borderWidth: '1px',
                                borderStyle: 'solid',
                                borderRadius: '8px',
                                fontWeight: isEntryTimeMinOpen ? 600 : 400,
                                color: 'white',
                                outline: 'none',
                                margin: 0,
                                flex: 1
                            }}
                        >
                            <span style={{fontSize: '14px', fontWeight: '500'}}>시작 시간</span>
                            <span style={{fontSize: '13px', opacity: 0.8}}>
                                {formatDate(filter.entryTimeMin)}
                            </span>
                        </Button>
                    </motion.div>

                    <motion.div
                        variants={itemVariants}
                        initial="hidden"
                        animate="visible"
                        whileHover="hover"
                        whileTap="tap"
                        custom={{isActive: isEntryTimeMaxOpen}}
                        className={`sidebar-button-container ${isEntryTimeMaxOpen ? "active-sidebar-button" : ""}`}
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
                            onMouseDown={handleEntryTimeMaxMouseDown}
                            onMouseUp={handleEntryTimeMaxMouseUp}
                            onMouseLeave={handleEntryTimeMaxMouseLeave}
                            style={{
                                display: 'flex',
                                flexDirection: 'column',
                                gap: '3px',
                                padding: '8px 12px',
                                minHeight: '55px',
                                whiteSpace: 'nowrap',
                                width: '100%',
                                backgroundColor: isEntryTimeMaxOpen ? 'rgba(255, 215, 0, 0.4)' : 'rgba(17, 17, 17, 1)',
                                borderColor: isEntryTimeMaxOpen ? 'rgba(255, 215, 0, 0.8)' : 'rgba(255, 215, 0, 0.4)',
                                borderWidth: '1px',
                                borderStyle: 'solid',
                                borderRadius: '8px',
                                fontWeight: isEntryTimeMaxOpen ? 600 : 400,
                                color: 'white',
                                outline: 'none',
                                margin: 0,
                                flex: 1
                            }}
                        >
                            <span style={{fontSize: '14px', fontWeight: '500'}}>종료 시간</span>
                            <span style={{fontSize: '13px', opacity: 0.8}}>
                                {formatDate(filter.entryTimeMax)}
                            </span>
                        </Button>
                    </motion.div>
                </div>
            </div>

            {/* 캘린더 컴포넌트 */}
            {isEntryTimeMinOpen && (
                <FilterCalendar
                    title="[진입 시간]  시작 시간"
                    timeframe={timeframe}
                    onClose={handleEntryTimeMinClose}
                    onDateTimeSelected={handleEntryTimeMinSelected}
                    lastSelectedDate={getLastSelectedDate(filter.entryTimeMin)}
                    lastSelectedTime={getLastSelectedTime(filter.entryTimeMin)}
                />
            )}
            {isEntryTimeMaxOpen && (
                <FilterCalendar
                    title="[진입 시간]  종료 시간"
                    timeframe={timeframe}
                    onClose={handleEntryTimeMaxClose}
                    onDateTimeSelected={handleEntryTimeMaxSelected}
                    lastSelectedDate={getLastSelectedDate(filter.entryTimeMax)}
                    lastSelectedTime={getLastSelectedTime(filter.entryTimeMax)}
                />
            )}
        </div>
    );
};

export default EntryTimeFilter;
