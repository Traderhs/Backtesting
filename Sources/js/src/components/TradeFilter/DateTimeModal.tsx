import React, {useState} from "react";
import {useTradeFilter, toggleOption} from "../TradeFilter";
import {TradeFilter, TradeItem} from "./TradeFilterContext";
import {
    getYearOptions,
    getMonthOptions,
    getDayOptions,
    getHourOptions,
    getMinuteSecondOptions
} from "./TimeFilterOptions";

interface DateTimeModalProps {
    type: keyof TradeFilter;
    isOpen: boolean;
    onClose: () => void;
    tradeData: TradeItem[];
}

const DateTimeModal: React.FC<DateTimeModalProps> = ({type, isOpen, onClose, tradeData}) => {
    const {setFilter} = useTradeFilter();

    const [year, setYear] = useState<string>("");
    const [month, setMonth] = useState<string>("");
    const [day, setDay] = useState<string>("");
    const [hour, setHour] = useState<string>("00");
    const [minute, setMinute] = useState<string>("00");
    const [second, setSecond] = useState<string>("00");

    if (tradeData.length === 0) {
        console.error("tradeData가 비어있습니다. 연도 범위를 계산할 수 없습니다.");
        return null;
    }

    // "진입 시간"과 "청산 시간"을 모두 고려하여 연도 옵션 생성
    const yearOptions = getYearOptions(tradeData, ["진입 시간", "청산 시간"]).map(String);
    if (yearOptions.length === 0) {
        console.error("유효한 날짜 데이터가 없습니다.");
        return null;
    }

    // 월, 일, 시, 분, 초 옵션은 기본 배열을 생성 후 문자열로 변환 (padStart)
    const monthOptions = getMonthOptions().map(m => m.toString().padStart(2, "0"));
    const dayOptions = getDayOptions().map(d => d.toString().padStart(2, "0"));
    const hourOptions = getHourOptions().map(h => h.toString().padStart(2, "0"));
    const minutesAndSecondsOptions = getMinuteSecondOptions().map(n => n.toString().padStart(2, "0"));

    const handleApply = () => {
        if (year && month && day && hour && minute && second) {
            const utcTimestamp = Date.UTC(
                parseInt(year, 10),
                parseInt(month, 10) - 1,
                parseInt(day, 10),
                parseInt(hour, 10),
                parseInt(minute, 10),
                parseInt(second, 10)
            );
            const gmtDateTime = new Date(utcTimestamp);
            const datetime = gmtDateTime.toISOString().split(".")[0] + "Z";
            toggleOption(type, datetime, true, setFilter);
            onClose();
        }
    };

    if (!isOpen) return null;

    return (
        <div className="fixed inset-0 z-50 flex items-center justify-center bg-black bg-opacity-50">
            <div className="bg-white rounded p-6 w-96 text-black">
                <h2 className="text-lg font-bold mb-4">날짜 및 시간 선택</h2>

                <div className="flex items-center space-x-4 mb-4">
                    <div className="flex items-center space-x-1">
                        <select
                            id={`${type}-year-select`}
                            name={`${type}-year-select`}
                            value={year}
                            onChange={(e) => setYear(e.target.value)}
                            className="p-1 border rounded"
                        >
                            <option value="">연</option>
                            {yearOptions.map((y) => (
                                <option key={y} value={y}>
                                    {y}
                                </option>
                            ))}
                        </select>
                        <select
                            id={`${type}-month-select`}
                            name={`${type}-month-select`}
                            value={month}
                            onChange={(e) => setMonth(e.target.value)}
                            className="p-1 border rounded"
                        >
                            <option value="">월</option>
                            {monthOptions.map((m) => (
                                <option key={m} value={m}>
                                    {m}
                                </option>
                            ))}
                        </select>
                        <select
                            id={`${type}-day-select`}
                            name={`${type}-day-select`}
                            value={day}
                            onChange={(e) => setDay(e.target.value)}
                            className="p-1 border rounded"
                        >
                            <option value="">일</option>
                            {dayOptions.map((d) => (
                                <option key={d} value={d}>
                                    {d}
                                </option>
                            ))}
                        </select>
                    </div>

                    <div className="flex items-center space-x-1">
                        <select
                            id={`${type}-hour-select`}
                            name={`${type}-hour-select`}
                            value={hour}
                            onChange={(e) => setHour(e.target.value)}
                            className="p-1 border rounded"
                        >
                            <option value="">시간</option>
                            {hourOptions.map((h) => (
                                <option key={h} value={h}>
                                    {h}
                                </option>
                            ))}
                        </select>
                        <select
                            id={`${type}-minute-select`}
                            name={`${type}-minute-select`}
                            value={minute}
                            onChange={(e) => setMinute(e.target.value)}
                            className="p-1 border rounded"
                        >
                            <option value="">분</option>
                            {minutesAndSecondsOptions.map((m) => (
                                <option key={m} value={m}>
                                    {m}
                                </option>
                            ))}
                        </select>
                        <select
                            id={`${type}-second-select`}
                            name={`${type}-second-select`}
                            value={second}
                            onChange={(e) => setSecond(e.target.value)}
                            className="p-1 border rounded"
                        >
                            <option value="">초</option>
                            {minutesAndSecondsOptions.map((s) => (
                                <option key={s} value={s}>
                                    {s}
                                </option>
                            ))}
                        </select>
                    </div>
                </div>

                <div className="flex justify-end space-x-2">
                    <button onClick={onClose} className="px-4 py-2 bg-gray-300 rounded text-black">
                        취소
                    </button>
                    <button onClick={handleApply} className="px-4 py-2 bg-blue-500 text-white rounded">
                        적용
                    </button>
                </div>
            </div>
        </div>
    );
};

export default DateTimeModal;
