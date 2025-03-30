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

export interface AdvancedExitTimeFilterValues {
    exitYears?: number[];
    exitMonths?: number[];
    exitDays?: number[];
    exitDayOfWeeks?: number[];
    exitHours?: number[];
    exitMinutes?: number[];
    exitSeconds?: number[];
}

interface AdvancedExitTimeFilterModalProps {
    isOpen: boolean;
    onClose: () => void;
    values: AdvancedExitTimeFilterValues;
    setValues: (values: AdvancedExitTimeFilterValues) => void;
    tradeData: TradeItem[];
}

const AdvancedExitTimeFilterModal: React.FC<AdvancedExitTimeFilterModalProps> = ({
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
        key: keyof AdvancedExitTimeFilterValues,
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
                exitYears: filter.exitYears,
                exitMonths: filter.exitMonths,
                exitDays: filter.exitDays,
                exitDayOfWeeks: filter.exitDayOfWeeks,
                exitHours: filter.exitHours,
                exitMinutes: filter.exitMinutes,
                exitSeconds: filter.exitSeconds,
            });
        }
    }, [isOpen, filter, setValues]);

    const onApply = () => {
        setFilter(prev => ({
            ...prev,
            exitYears: values.exitYears || [],
            exitMonths: values.exitMonths || [],
            exitDays: values.exitDays || [],
            exitDayOfWeeks: values.exitDayOfWeeks || [],
            exitHours: values.exitHours || [],
            exitMinutes: values.exitMinutes || [],
            exitSeconds: values.exitSeconds || [],
        }));
        onClose();
    };

    const onCancel = () => {
        setValues({
            exitYears: filter.exitYears,
            exitMonths: filter.exitMonths,
            exitDays: filter.exitDays,
            exitDayOfWeeks: filter.exitDayOfWeeks,
            exitHours: filter.exitHours,
            exitMinutes: filter.exitMinutes,
            exitSeconds: filter.exitSeconds,
        });
        onClose();
    };

    return (
        <div className="fixed inset-0 z-50 flex items-center justify-center bg-black bg-opacity-50">
            {/* 모달 컨테이너에 커스텀 CSS 클래스 적용 */}
            <div className="modal-container bg-white rounded p-6 text-black space-y-4">
                <h2 className="text-lg font-bold text-center">고급 청산 시간 필터 설정</h2>

                {/* 연 */}
                <div>
                    <div className="flex justify-between items-center mb-1">
                        <span className="font-bold">연</span>
                        <div className="space-x-2">
                            <Button onClick={() => setValues({ ...values, exitYears: yearOptions })}>
                                전체 선택
                            </Button>
                            <Button onClick={() => setValues({ ...values, exitYears: [] })}>
                                전체 해제
                            </Button>
                        </div>
                    </div>
                    <TimeFilterCheckboxes
                        label="연"
                        options={yearOptions}
                        selectedValues={values.exitYears || []}
                        onChange={(option, checked) => onCheckboxChange("exitYears", option, checked)}
                    />
                </div>

                {/* 월 */}
                <div>
                    <div className="flex justify-between items-center mb-1">
                        <span className="font-bold">월</span>
                        <div className="space-x-2">
                            <Button onClick={() => setValues({ ...values, exitMonths: monthOptions })}>
                                전체 선택
                            </Button>
                            <Button onClick={() => setValues({ ...values, exitMonths: [] })}>
                                전체 해제
                            </Button>
                        </div>
                    </div>
                    <TimeFilterCheckboxes
                        label="월"
                        options={monthOptions}
                        selectedValues={values.exitMonths || []}
                        onChange={(option, checked) => onCheckboxChange("exitMonths", option, checked)}
                    />
                </div>

                {/* 일 */}
                <div>
                    <div className="flex justify-between items-center mb-1">
                        <span className="font-bold">일</span>
                        <div className="space-x-2">
                            <Button onClick={() => setValues({ ...values, exitDays: dayOptions })}>
                                전체 선택
                            </Button>
                            <Button onClick={() => setValues({ ...values, exitDays: [] })}>
                                전체 해제
                            </Button>
                        </div>
                    </div>
                    <TimeFilterCheckboxes
                        label="일"
                        options={dayOptions}
                        selectedValues={values.exitDays || []}
                        onChange={(option, checked) => onCheckboxChange("exitDays", option, checked)}
                    />
                </div>

                {/* 요일 */}
                <div>
                    <div className="flex justify-between items-center mb-1">
                        <span className="font-bold">요일 (0~6)</span>
                        <div className="space-x-2">
                            <Button onClick={() => setValues({ ...values, exitDayOfWeeks: dayOfWeekOptions })}>
                                전체 선택
                            </Button>
                            <Button onClick={() => setValues({ ...values, exitDayOfWeeks: [] })}>
                                전체 해제
                            </Button>
                        </div>
                    </div>
                    <TimeFilterCheckboxes
                        label="요일 (0~6)"
                        options={dayOfWeekOptions}
                        selectedValues={values.exitDayOfWeeks || []}
                        onChange={(option, checked) => onCheckboxChange("exitDayOfWeeks", option, checked)}
                    />
                </div>

                {/* 시 */}
                <div>
                    <div className="flex justify-between items-center mb-1">
                        <span className="font-bold">시</span>
                        <div className="space-x-2">
                            <Button onClick={() => setValues({ ...values, exitHours: hourOptions })}>
                                전체 선택
                            </Button>
                            <Button onClick={() => setValues({ ...values, exitHours: [] })}>
                                전체 해제
                            </Button>
                        </div>
                    </div>
                    <TimeFilterCheckboxes
                        label="시"
                        options={hourOptions}
                        selectedValues={values.exitHours || []}
                        onChange={(option, checked) => onCheckboxChange("exitHours", option, checked)}
                    />
                </div>

                {/* 분 */}
                <div>
                    <div className="flex justify-between items-center mb-1">
                        <span className="font-bold">분</span>
                        <div className="space-x-2">
                            <Button onClick={() => setValues({ ...values, exitMinutes: minuteSecondOptions })}>
                                전체 선택
                            </Button>
                            <Button onClick={() => setValues({ ...values, exitMinutes: [] })}>
                                전체 해제
                            </Button>
                        </div>
                    </div>
                    <TimeFilterCheckboxes
                        label="분"
                        options={minuteSecondOptions}
                        selectedValues={values.exitMinutes || []}
                        onChange={(option, checked) => onCheckboxChange("exitMinutes", option, checked)}
                    />
                </div>

                {/* 초 */}
                <div>
                    <div className="flex justify-between items-center mb-1">
                        <span className="font-bold">초</span>
                        <div className="space-x-2">
                            <Button onClick={() => setValues({ ...values, exitSeconds: minuteSecondOptions })}>
                                전체 선택
                            </Button>
                            <Button onClick={() => setValues({ ...values, exitSeconds: [] })}>
                                전체 해제
                            </Button>
                        </div>
                    </div>
                    <TimeFilterCheckboxes
                        label="초"
                        options={minuteSecondOptions}
                        selectedValues={values.exitSeconds || []}
                        onChange={(option, checked) => onCheckboxChange("exitSeconds", option, checked)}
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

export default AdvancedExitTimeFilterModal;
