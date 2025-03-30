import React, { useEffect } from "react";
import { Button } from "../ui/button.tsx";
import { TradeItem } from "./TradeFilterContext";
import TimeFilterCheckboxes from "./TimeFilterCheckboxes";
import './modal.css'
import {
    getYearOptions,
    getMonthOptions,
    getDayOptions,
    getDayOfWeekOptions,
    getHourOptions,
    getMinuteSecondOptions,
} from "./TimeFilterOptions";
import { useTradeFilter } from "../TradeFilter";

export interface AdvancedEntryTimeFilterValues {
    entryYears?: number[];
    entryMonths?: number[];
    entryDays?: number[];
    entryDayOfWeeks?: number[];
    entryHours?: number[];
    entryMinutes?: number[];
    entrySeconds?: number[];
}

interface AdvancedEntryTimeFilterModalProps {
    isOpen: boolean;
    onClose: () => void;
    values: AdvancedEntryTimeFilterValues;
    setValues: (values: AdvancedEntryTimeFilterValues) => void;
    tradeData: TradeItem[];
}

const AdvancedEntryTimeFilterModal: React.FC<AdvancedEntryTimeFilterModalProps> = ({
                                                                                       isOpen,
                                                                                       onClose,
                                                                                       values,
                                                                                       setValues,
                                                                                       tradeData,
                                                                                   }) => {
    const { filter, setFilter } = useTradeFilter();

    if (!isOpen) return null;
    if (tradeData.length === 0) {
        console.error("tradeData가 비어있습니다.");
        return null;
    }

    // 옵션 추출
    const yearOptions = getYearOptions(tradeData, ["진입 시간", "청산 시간"]);
    if (yearOptions.length === 0) {
        console.error("유효한 연도 데이터가 없습니다.");
        return null;
    }
    const monthOptions = getMonthOptions();
    const dayOptions = getDayOptions();
    const dayOfWeekOptions = getDayOfWeekOptions();
    const hourOptions = getHourOptions();
    const minuteSecondOptions = getMinuteSecondOptions();

    const onCheckboxChange = (
        key: keyof AdvancedEntryTimeFilterValues,
        option: number,
        checked: boolean
    ) => {
        const currentValues = values[key] || [];
        const newValues = checked
            ? [...currentValues, option]
            : currentValues.filter(v => v !== option);
        setValues({ ...values, [key]: newValues });
    };

    // 모달 오픈 시 로컬 상태를 글로벌 필터 값으로 초기화
    useEffect(() => {
        if (isOpen) {
            setValues({
                entryYears: filter.entryYears,
                entryMonths: filter.entryMonths,
                entryDays: filter.entryDays,
                entryDayOfWeeks: filter.entryDayOfWeeks,
                entryHours: filter.entryHours,
                entryMinutes: filter.entryMinutes,
                entrySeconds: filter.entrySeconds,
            });
        }
    }, [isOpen, filter, setValues]);

    // 적용 및 취소 함수
    const onApply = () => {
        setFilter(prev => ({
            ...prev,
            entryYears: values.entryYears || [],
            entryMonths: values.entryMonths || [],
            entryDays: values.entryDays || [],
            entryDayOfWeeks: values.entryDayOfWeeks || [],
            entryHours: values.entryHours || [],
            entryMinutes: values.entryMinutes || [],
            entrySeconds: values.entrySeconds || [],
        }));
        onClose();
    };

    const onCancel = () => {
        // 로컬 상태를 글로벌 필터 상태로 재설정 후 닫기
        setValues({
            entryYears: filter.entryYears,
            entryMonths: filter.entryMonths,
            entryDays: filter.entryDays,
            entryDayOfWeeks: filter.entryDayOfWeeks,
            entryHours: filter.entryHours,
            entryMinutes: filter.entryMinutes,
            entrySeconds: filter.entrySeconds,
        });
        onClose();
    };

    return (
        <div className="fixed inset-0 z-50 flex items-center justify-center bg-black bg-opacity-50">
            {/* 모달 컨테이너에 커스텀 CSS 클래스 적용 */}
            <div className="modal-container bg-white rounded p-6 text-black space-y-4">
                <h2 className="text-lg font-bold text-center">고급 진입 시간 필터 설정</h2>

                {/* 연 */}
                <div>
                    <div className="flex justify-between items-center mb-1">
                        <span className="font-bold">연</span>
                        <div className="space-x-2">
                            <Button onClick={() => setValues({ ...values, entryYears: yearOptions })}>
                                전체 선택
                            </Button>
                            <Button onClick={() => setValues({ ...values, entryYears: [] })}>
                                전체 해제
                            </Button>
                        </div>
                    </div>
                    <TimeFilterCheckboxes
                        label="연"
                        options={yearOptions}
                        selectedValues={values.entryYears || []}
                        onChange={(option, checked) => onCheckboxChange("entryYears", option, checked)}
                    />
                </div>

                {/* 월 */}
                <div>
                    <div className="flex justify-between items-center mb-1">
                        <span className="font-bold">월</span>
                        <div className="space-x-2">
                            <Button onClick={() => setValues({ ...values, entryMonths: monthOptions })}>
                                전체 선택
                            </Button>
                            <Button onClick={() => setValues({ ...values, entryMonths: [] })}>
                                전체 해제
                            </Button>
                        </div>
                    </div>
                    <TimeFilterCheckboxes
                        label="월"
                        options={monthOptions}
                        selectedValues={values.entryMonths || []}
                        onChange={(option, checked) => onCheckboxChange("entryMonths", option, checked)}
                    />
                </div>

                {/* 일 */}
                <div>
                    <div className="flex justify-between items-center mb-1">
                        <span className="font-bold">일</span>
                        <div className="space-x-2">
                            <Button onClick={() => setValues({ ...values, entryDays: dayOptions })}>
                                전체 선택
                            </Button>
                            <Button onClick={() => setValues({ ...values, entryDays: [] })}>
                                전체 해제
                            </Button>
                        </div>
                    </div>
                    <TimeFilterCheckboxes
                        label="일"
                        options={dayOptions}
                        selectedValues={values.entryDays || []}
                        onChange={(option, checked) => onCheckboxChange("entryDays", option, checked)}
                    />
                </div>

                {/* 요일 */}
                <div>
                    <div className="flex justify-between items-center mb-1">
                        <span className="font-bold">요일 (0~6)</span>
                        <div className="space-x-2">
                            <Button onClick={() => setValues({ ...values, entryDayOfWeeks: dayOfWeekOptions })}>
                                전체 선택
                            </Button>
                            <Button onClick={() => setValues({ ...values, entryDayOfWeeks: [] })}>
                                전체 해제
                            </Button>
                        </div>
                    </div>
                    <TimeFilterCheckboxes
                        label="요일 (0~6)"
                        options={dayOfWeekOptions}
                        selectedValues={values.entryDayOfWeeks || []}
                        onChange={(option, checked) => onCheckboxChange("entryDayOfWeeks", option, checked)}
                    />
                </div>

                {/* 시 */}
                <div>
                    <div className="flex justify-between items-center mb-1">
                        <span className="font-bold">시</span>
                        <div className="space-x-2">
                            <Button onClick={() => setValues({ ...values, entryHours: hourOptions })}>
                                전체 선택
                            </Button>
                            <Button onClick={() => setValues({ ...values, entryHours: [] })}>
                                전체 해제
                            </Button>
                        </div>
                    </div>
                    <TimeFilterCheckboxes
                        label="시"
                        options={hourOptions}
                        selectedValues={values.entryHours || []}
                        onChange={(option, checked) => onCheckboxChange("entryHours", option, checked)}
                    />
                </div>

                {/* 분 */}
                <div>
                    <div className="flex justify-between items-center mb-1">
                        <span className="font-bold">분</span>
                        <div className="space-x-2">
                            <Button onClick={() => setValues({ ...values, entryMinutes: minuteSecondOptions })}>
                                전체 선택
                            </Button>
                            <Button onClick={() => setValues({ ...values, entryMinutes: [] })}>
                                전체 해제
                            </Button>
                        </div>
                    </div>
                    <TimeFilterCheckboxes
                        label="분"
                        options={minuteSecondOptions}
                        selectedValues={values.entryMinutes || []}
                        onChange={(option, checked) => onCheckboxChange("entryMinutes", option, checked)}
                    />
                </div>

                {/* 초 */}
                <div>
                    <div className="flex justify-between items-center mb-1">
                        <span className="font-bold">초</span>
                        <div className="space-x-2">
                            <Button onClick={() => setValues({ ...values, entrySeconds: minuteSecondOptions })}>
                                전체 선택
                            </Button>
                            <Button onClick={() => setValues({ ...values, entrySeconds: [] })}>
                                전체 해제
                            </Button>
                        </div>
                    </div>
                    <TimeFilterCheckboxes
                        label="초"
                        options={minuteSecondOptions}
                        selectedValues={values.entrySeconds || []}
                        onChange={(option, checked) => onCheckboxChange("entrySeconds", option, checked)}
                    />
                </div>

                <div className="flex justify-end space-x-2">
                    <Button onClick={onCancel}>닫기</Button>
                    <Button onClick={onApply}>적용</Button>
                </div>
            </div>
        </div>
    );
};

export default AdvancedEntryTimeFilterModal;
