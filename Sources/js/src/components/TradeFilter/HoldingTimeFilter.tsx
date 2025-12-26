import React, {forwardRef, useCallback, useEffect, useMemo, useRef, useState} from "react";
import {NumberFormatValues, NumericFormat} from 'react-number-format';
import {useTradeFilter} from "../TradeFilter";
import "./HoldingTimeFilter.css";
import "./NumericFilters.css";
import ResetFilterButton from "./ResetFilterButton";
import {RESET_HOLDING_TIME_FILTER} from "./FilterResetEvent";

const HoldingTimeFilter: React.FC = () => {
    const {filter, setFilter} = useTradeFilter();

    // 로컬 상태로 값 관리 (입력 중 리렌더링 방지)
    const [localMinValue, setLocalMinValue] = useState<number | undefined>(
        filter.holdingTimeMin !== undefined ? filter.holdingTimeMin / 86400 : undefined
    );
    const [localMinUnit, setLocalMinUnit] = useState<string>("일");
    const [localMaxValue, setLocalMaxValue] = useState<number | undefined>(
        filter.holdingTimeMax !== undefined ? filter.holdingTimeMax / 86400 : undefined
    );
    const [localMaxUnit, setLocalMaxUnit] = useState<string>("일");

    // 디바운싱 타이머 저장
    const debounceTimerRef = useRef<NodeJS.Timeout | null>(null);

    // 초기화 함수
    const resetFilter = () => {
        // 디바운스 타이머가 있으면 취소
        if (debounceTimerRef.current) {
            clearTimeout(debounceTimerRef.current);
            debounceTimerRef.current = null;
        }

        // 필드를 비우고 시간 단위를 '일'로 설정
        setLocalMinValue(undefined);
        setLocalMinUnit("일");
        setLocalMaxValue(undefined);
        setLocalMaxUnit("일");

        // 필터 상태 업데이트
        setFilter(prev => ({
            ...prev,
            holdingTimeMin: undefined,
            holdingTimeMax: undefined
        }));

        // 이전 값 업데이트
        prevValuesRef.current = {
            minValue: undefined,
            minUnit: "일",
            maxValue: undefined,
            maxUnit: "일"
        };
    };

    // 이전 값을 기억하여 불필요한 업데이트 방지
    const prevValuesRef = useRef<{
        minValue?: number;
        minUnit: string;
        maxValue?: number;
        maxUnit: string;
    }>({
        minValue: localMinValue,
        minUnit: localMinUnit,
        maxValue: localMaxValue,
        maxUnit: localMaxUnit
    });

    // 현재 포커스 요소 ID 저장
    const activeFocusRef = useRef<string | null>(null);

    // 로컬 임시 상태 저장 객체 (커서 위치 복원용)
    const localValuesRef = useRef<{ [key: string]: string }>({});

    const [isMinDropdownOpen, setIsMinDropdownOpen] = useState(false);
    const [isMaxDropdownOpen, setIsMaxDropdownOpen] = useState(false);
    const minDropdownRef = useRef<HTMLDivElement>(null);
    const maxDropdownRef = useRef<HTMLDivElement>(null);
    const minOptionsRef = useRef<HTMLDivElement>(null);
    const maxOptionsRef = useRef<HTMLDivElement>(null);

    const units = ["초", "분", "시간", "일", "주", "개월", "년"];

    // 단위를 기준으로 입력된 값을 초 단위로 변환하는 함수
    const convertToSeconds = (value: number | undefined, unit: string): number | undefined => {
        if (value === undefined) return undefined;

        // C++ 백엔드와 동일한 시간 상수들 (밀리초 단위)
        const kSecond = 1000;
        const kMinute = 60 * kSecond;        // 60,000
        const kHour = 60 * kMinute;          // 3,600,000
        const kDay = 24 * kHour;             // 86,400,000
        const kWeek = 7 * kDay;              // 604,800,000
        const kMonth = 30 * kDay;            // 2,592,000,000 (30일로 가정)
        const kYear = 12 * kMonth;           // 31,104,000,000

        switch (unit) {
            case "초":
                return Math.floor((value * kSecond) / 1000);
            case "분":
                return Math.floor((value * kMinute) / 1000);
            case "시간":
                return Math.floor((value * kHour) / 1000);
            case "일":
                return Math.floor((value * kDay) / 1000);
            case "주":
                return Math.floor((value * kWeek) / 1000);
            case "개월":
                return Math.floor((value * kMonth) / 1000);
            case "년":
                return Math.floor((value * kYear) / 1000); // 밀리초를 초로 변환
            default:
                return value;
        }
    };

    // 디바운싱 필터 업데이트 함수
    const debounceUpdateFilter = useCallback(() => {
        // 이전 타이머가 있으면 취소
        if (debounceTimerRef.current) {
            clearTimeout(debounceTimerRef.current);
        }

        // 새 타이머 설정 (300ms 디바운스)
        debounceTimerRef.current = setTimeout(() => {
            // 현재 값과 이전 값 비교
            if (
                localMinValue === prevValuesRef.current.minValue &&
                localMinUnit === prevValuesRef.current.minUnit &&
                localMaxValue === prevValuesRef.current.maxValue &&
                localMaxUnit === prevValuesRef.current.maxUnit
            ) {
                // 변경 사항이 없으면 업데이트 하지 않음
                return;
            }

            // 값이 변경된 경우 필터 업데이트
            const minSeconds = localMinValue === 0 ? 0 : convertToSeconds(localMinValue, localMinUnit);
            const maxSeconds = localMaxValue === 0 ? 0 : convertToSeconds(localMaxValue, localMaxUnit);

            setFilter((prev) => ({
                ...prev,
                holdingTimeMin: minSeconds,
                holdingTimeMax: maxSeconds,
            }));

            // 현재 값을 이전 값으로 저장
            prevValuesRef.current = {
                minValue: localMinValue,
                minUnit: localMinUnit,
                maxValue: localMaxValue,
                maxUnit: localMaxUnit
            };
        }, 300);
    }, [localMinValue, localMinUnit, localMaxValue, localMaxUnit, setFilter]);

    // 값이 변경된 경우에만 필터 업데이트 실행 (즉시 실행 버전)
    const updateFilter = useCallback(() => {
        // 디바운스 타이머가 있으면 취소
        if (debounceTimerRef.current) {
            clearTimeout(debounceTimerRef.current);
            debounceTimerRef.current = null;
        }

        // 현재 값과 이전 값 비교
        if (
            localMinValue === prevValuesRef.current.minValue &&
            localMinUnit === prevValuesRef.current.minUnit &&
            localMaxValue === prevValuesRef.current.maxValue &&
            localMaxUnit === prevValuesRef.current.maxUnit
        ) {
            // 변경 사항이 없으면 업데이트 하지 않음
            return;
        }

        // 값이 변경된 경우 필터 업데이트
        const minSeconds = localMinValue === 0 ? 0 : convertToSeconds(localMinValue, localMinUnit);
        const maxSeconds = localMaxValue === 0 ? 0 : convertToSeconds(localMaxValue, localMaxUnit);

        setFilter((prev) => ({
            ...prev,
            holdingTimeMin: minSeconds,
            holdingTimeMax: maxSeconds,
        }));

        // 현재 값을 이전 값으로 저장
        prevValuesRef.current = {
            minValue: localMinValue,
            minUnit: localMinUnit,
            maxValue: localMaxValue,
            maxUnit: localMaxUnit
        };
    }, [localMinValue, localMinUnit, localMaxValue, localMaxUnit, setFilter]);

    // 값이 변경될 때마다 디바운싱 업데이트 호출
    useEffect(() => {
        debounceUpdateFilter();
    }, [localMinValue, localMinUnit, localMaxValue, localMaxUnit, debounceUpdateFilter]);

    // 초기화 이벤트 리스너 추가
    useEffect(() => {
        // 초기화 이벤트 핸들러
        const handleResetEvent = () => {
            resetFilter();
        };

        // 이벤트 리스너 등록
        document.addEventListener(RESET_HOLDING_TIME_FILTER, handleResetEvent);

        // 컴포넌트 언마운트 시 이벤트 리스너 제거
        return () => {
            document.removeEventListener(RESET_HOLDING_TIME_FILTER, handleResetEvent);
        };
    }, []);

    // 컴포넌트 언마운트 시 타이머 정리
    useEffect(() => {
        return () => {
            if (debounceTimerRef.current) {
                clearTimeout(debounceTimerRef.current);
            }
        };
    }, []);

    // 포커스가 빠져나갈 때 필터 즉시 업데이트
    const handleBlur = (e: React.FocusEvent<HTMLInputElement>) => {
        // 현재 활성 포커스가 이 요소인 경우에만 초기화
        if (activeFocusRef.current === e.currentTarget.id) {
            activeFocusRef.current = null;
        }

        // 즉시 업데이트 (디바운싱 무시)
        updateFilter();
    };

    // 드롭다운에서 선택 후 필터 업데이트
    useEffect(() => {
        // 드롭다운이 닫히면 필터 즉시 업데이트
        if (!isMinDropdownOpen && !isMaxDropdownOpen) {
            updateFilter();
        }
    }, [isMinDropdownOpen, isMaxDropdownOpen, updateFilter]);

    // 최소 시간 드롭다운 열릴 때 스크롤 조정
    useEffect(() => {
        if (isMinDropdownOpen && minOptionsRef.current) {
            requestAnimationFrame(() => {
                if (minOptionsRef.current) {
                    const optionElements = minOptionsRef.current.getElementsByClassName('holding-time-option');

                    for (let i = 0; i < optionElements.length; i++) {
                        const element = optionElements[i] as HTMLElement;

                        if (element.textContent === localMinUnit) {
                            element.scrollIntoView({block: 'nearest'});
                            break;
                        }
                    }
                }
            });
        }
    }, [isMinDropdownOpen, localMinUnit]);

    // 최대 시간 드롭다운 열릴 때 스크롤 조정
    useEffect(() => {
        if (isMaxDropdownOpen && maxOptionsRef.current) {
            requestAnimationFrame(() => {
                if (maxOptionsRef.current) {
                    const optionElements = maxOptionsRef.current.getElementsByClassName('holding-time-option');

                    for (let i = 0; i < optionElements.length; i++) {
                        const element = optionElements[i] as HTMLElement;

                        if (element.textContent === localMaxUnit) {
                            element.scrollIntoView({block: 'nearest'});
                            break;
                        }
                    }
                }
            });
        }
    }, [isMaxDropdownOpen, localMaxUnit]);

    // 드롭다운 외부 클릭 감지
    useEffect(() => {
        const handleClickOutside = (event: MouseEvent) => {
            if (minDropdownRef.current && !minDropdownRef.current.contains(event.target as Node)) {
                setIsMinDropdownOpen(false);
            }

            if (maxDropdownRef.current && !maxDropdownRef.current.contains(event.target as Node)) {
                setIsMaxDropdownOpen(false);
            }
        };

        document.addEventListener('mousedown', handleClickOutside);
        return () => {
            document.removeEventListener('mousedown', handleClickOutside);
        };
    }, []);

    // 키 입력 이벤트를 처리하는 함수
    // 클릭 이벤트 처리 함수 - 클릭 시 즉시 포커스 설정
    const handleClick = (e: React.MouseEvent<HTMLInputElement>) => {
        // 다른 필드에 포커스가 있으면 해제
        const input = e.currentTarget;

        if (activeFocusRef.current && activeFocusRef.current !== input.id) {
            const prevInput = document.getElementById(activeFocusRef.current) as HTMLInputElement;

            if (prevInput) {
                prevInput.blur();
            }
        }

        // 클릭 시 즉시 포커스 설정 (중요: stopPropagation으로 이벤트 버블링 방지)
        e.stopPropagation();
        input.focus();

        // 현재 입력 필드를 활성 포커스로 설정
        if (input.id) {
            activeFocusRef.current = input.id;
        }

        // 커서를 끝으로 이동
        const length = input.value.length;
        input.setSelectionRange(length, length);
    };

    // 포커스 이벤트 핸들러
    const handleFocus = (e: React.FocusEvent<HTMLInputElement>) => {
        const input = e.currentTarget;

        if (input.id) {
            activeFocusRef.current = input.id;
        }
    };

    // 필드 클릭 시 부모 컨테이너 클릭 이벤트가 방해하지 않도록 처리
    useEffect(() => {
        const parentFields = document.querySelectorAll('.holding-time-field');

        const preventParentClick = (e: Event) => {
            if (e.target instanceof HTMLInputElement) {
                e.stopPropagation();
            }
        };

        parentFields.forEach(field => {
            field.addEventListener('click', preventParentClick, true);
        });

        return () => {
            parentFields.forEach(field => {
                field.removeEventListener('click', preventParentClick, true);
            });
        };
    }, []);

    // NumericFormat 값 변경 핸들러
    const handleNumericFormatChange = (setter: React.Dispatch<React.SetStateAction<number | undefined>>) =>
        (values: NumberFormatValues) => {
            const val = values.value ? Number(values.value) : undefined;
            setter(val);
        };

    // 쉼표 로직 포함한 CustomInput (간소화 버전)
    const SimpleCustomInput = useMemo(() => {
        return forwardRef<HTMLInputElement, React.InputHTMLAttributes<HTMLInputElement>>((props, ref) => {
            const {onKeyDown, ...rest} = props;

            // 커서 위치 복원을 위한 ref
            const inputRef = useRef<HTMLInputElement | null>(null);

            // input ref 연결
            const handleRef = (element: HTMLInputElement | null) => {
                inputRef.current = element;

                // 부모로부터 전달된 ref 처리
                if (typeof ref === 'function') {
                    ref(element);
                } else if (ref) {
                    ref.current = element;
                }
            };

            // 커서 위치 복원 처리
            useEffect(() => {
                if (inputRef.current && inputRef.current.id) {
                    const cursorKey = `${inputRef.current.id}_cursor`;
                    const savedPosition = localValuesRef.current[cursorKey];
                    const cursorMode = localValuesRef.current[`${cursorKey}_mode`];

                    if (savedPosition) {
                        const pos = parseInt(savedPosition, 10);

                        if (!isNaN(pos)) {
                            const setCursorPosition = () => {
                                if (inputRef.current && inputRef.current === document.activeElement) {
                                    let targetPos = pos;

                                    // digit_count 모드: 숫자 개수를 기반으로 커서 위치 계산
                                    if (cursorMode === 'digit_count') {
                                        const currentValue = inputRef.current.value;
                                        let digitCount = 0;
                                        targetPos = 0;

                                        for (let i = 0; i < currentValue.length; i++) {
                                            if (/\d/.test(currentValue[i])) {
                                                if (digitCount === pos) {
                                                    targetPos = i;
                                                    break;
                                                }

                                                digitCount++;
                                            }
                                            targetPos = i + 1;
                                        }
                                    }

                                    inputRef.current.setSelectionRange(targetPos, targetPos);
                                }
                            };

                            // 여러 프레임에 걸쳐 강제 설정
                            setCursorPosition();
                            requestAnimationFrame(() => {
                                setCursorPosition();
                                requestAnimationFrame(() => {
                                    setCursorPosition();
                                    delete localValuesRef.current[cursorKey];
                                    delete localValuesRef.current[`${cursorKey}_mode`];
                                });
                            });
                        }
                    }
                }
            });

            const internalKeyDown = (e: React.KeyboardEvent<HTMLInputElement>) => {
                const input = e.currentTarget;
                const value = input.value;
                const selectionStart = input.selectionStart || 0;
                const selectionEnd = input.selectionEnd || 0;
                const selectionLength = selectionEnd - selectionStart;

                // 쉼표 직접 입력 차단
                if (e.key === ',') {
                    e.preventDefault();
                    return;
                }

                // 정수 필드에서 이미 다른 숫자가 있을 때 맨 앞에 0 추가 방지
                if (e.key === "0" && value !== "") {
                    const hasSelection = selectionLength > 0;

                    // 커서가 맨 앞에 있고 선택 영역이 없을 때 0 입력 방지
                    if (selectionStart === 0 && !hasSelection) {
                        e.preventDefault();
                        return;
                    }
                }

                // 0으로 시작한 후 또 다른 0 입력 방지 (00, 000 등)
                if (e.key === "0" && value.startsWith("0") && !value.includes(".")) {
                    const hasSelection = selectionLength > 0;

                    // 커서가 0 바로 뒤에 있고 선택 영역이 없을 때 0 입력 방지
                    if (selectionStart === 1 && !hasSelection) {
                        e.preventDefault();
                        return;
                    }
                }

                // 선택 영역이 있을 때: 쉼표만 선택된 경우 삭제 방지
                if ((e.key === 'Backspace' || e.key === 'Delete') && selectionLength > 0 && value.includes(',')) {
                    const selectedText = value.substring(selectionStart, selectionEnd);

                    if (selectedText.replace(/,/g, '').length === 0) {
                        // 선택된 부분이 쉼표로만 구성
                        e.preventDefault();
                        return;
                    }
                }

                // 단일 백스페이스 / Delete 시 쉼표 삭제 방지 및 숫자 삭제 로직
                if ((e.key === 'Backspace' || e.key === 'Delete') && selectionLength === 0 && value.includes(',')) {
                    if (e.key === 'Backspace') {
                        const charBeforeCursor = selectionStart > 0 ? value[selectionStart - 1] : '';

                        if (charBeforeCursor === ',') {
                            e.preventDefault();

                            // 쉼표 왼쪽 숫자 찾기
                            let deletePos = selectionStart - 2;
                            while (deletePos >= 0 && !/\d/.test(value[deletePos])) {
                                deletePos--;
                            }

                            if (deletePos >= 0) {
                                const newVal = value.substring(0, deletePos) + value.substring(deletePos + 1);
                                const pureVal = newVal.replace(/,/g, '');
                                const numericVal = pureVal === '' ? undefined : Number(pureVal);

                                if (input.id.includes('-min-')) {
                                    setLocalMinValue(numericVal);
                                } else {
                                    setLocalMaxValue(numericVal);
                                }

                                // 커서 위치 저장 (useEffect에서 복원)
                                const cursorKey = `${input.id}_cursor`;
                                localValuesRef.current[cursorKey] = String(deletePos);
                            }
                            return;
                        }
                    } else if (e.key === 'Delete') {
                        const charAtCursor = selectionStart < value.length ? value[selectionStart] : '';

                        if (charAtCursor === ',') {
                            e.preventDefault();

                            // 쉼표 오른쪽 숫자 찾기
                            let deletePos = selectionStart + 1;
                            while (deletePos < value.length && !/\d/.test(value[deletePos])) {
                                deletePos++;
                            }

                            if (deletePos < value.length) {
                                const newVal = value.substring(0, deletePos) + value.substring(deletePos + 1);
                                const pureVal = newVal.replace(/,/g, '');
                                const numericVal = pureVal === '' ? undefined : Number(pureVal);

                                if (input.id.includes('-min-')) {
                                    setLocalMinValue(numericVal);
                                } else {
                                    setLocalMaxValue(numericVal);
                                }

                                // 커서 위치 계산: 현재 커서 왼쪽에 있는 숫자의 개수를 센다
                                let digitCountBeforeCursor = 0;
                                for (let i = 0; i < selectionStart; i++) {
                                    if (/\d/.test(value[i])) {
                                        digitCountBeforeCursor++;
                                    }
                                }

                                // 커서 위치 저장 (숫자 개수 기반)
                                const cursorKey = `${input.id}_cursor`;
                                localValuesRef.current[cursorKey] = String(digitCountBeforeCursor);
                                localValuesRef.current[`${cursorKey}_mode`] = 'digit_count';
                            }
                            return;
                        }
                    }
                }
                if (onKeyDown) onKeyDown(e);
            };

            return <input {...rest} ref={handleRef} onKeyDown={internalKeyDown}/>;
        });
    }, []);

    // 입력 컴포넌트를 useMemo로 감싸서 리렌더링 최적화
    const inputComponents = useMemo(() => {
        const minInput = (
            <NumericFormat
                id="HoldingTimeFilter-min-value"
                name="HoldingTimeFilter-min-value"
                value={localMinValue !== undefined ? String(localMinValue) : ""}
                thousandSeparator=","
                allowNegative={false}
                allowLeadingZeros={true}
                decimalScale={0}
                onValueChange={handleNumericFormatChange(setLocalMinValue)}
                onBlur={handleBlur}
                onFocus={handleFocus}
                onClick={handleClick}
                placeholder="최소 시간"
                className="common-numeric-input"
                autoComplete="off"
                customInput={SimpleCustomInput}
            />
        );

        const maxInput = (
            <NumericFormat
                id="HoldingTimeFilter-max-value"
                name="HoldingTimeFilter-max-value"
                value={localMaxValue !== undefined ? String(localMaxValue) : ""}
                thousandSeparator=","
                allowNegative={false}
                allowLeadingZeros={true}
                decimalScale={0}
                onValueChange={handleNumericFormatChange(setLocalMaxValue)}
                onBlur={handleBlur}
                onFocus={handleFocus}
                onClick={handleClick}
                placeholder="최대 시간"
                className="common-numeric-input"
                autoComplete="off"
                customInput={SimpleCustomInput}
            />
        );

        return {minInput, maxInput};
    }, [localMinValue, localMaxValue]); // 의존성 배열에 필요한 상태만 포함

    return (
        <div className="filter-component" style={{marginBottom: '5px'}}>
            <div className="filter-section-title" style={{display: 'flex', alignItems: 'center', marginBottom: '8px'}}>
                <ResetFilterButton onClick={resetFilter}/>
                <span>보유 시간</span>
            </div>

            <div className="holding-time-container">
                <div className="holding-time-row">
                    {/* 최소 */}
                    <div className="holding-time-field">
                        {inputComponents.minInput}
                    </div>

                    <div className="holding-time-field" ref={minDropdownRef}>
                        <div
                            className="holding-time-select"
                            onClick={() => setIsMinDropdownOpen(!isMinDropdownOpen)}
                        >
                            {localMinUnit}
                        </div>
                        {isMinDropdownOpen && (
                            <div className="holding-time-options" ref={minOptionsRef}>
                                {units.map(unit => (
                                    <div
                                        key={unit}
                                        className={`holding-time-option ${unit === localMinUnit ? 'selected' : ''}`}
                                        onClick={() => {
                                            setLocalMinUnit(unit);
                                            setIsMinDropdownOpen(false);
                                        }}
                                    >
                                        {unit}
                                    </div>
                                ))}
                            </div>
                        )}
                    </div>
                </div>

                <div className="holding-time-row">
                    {/* 최대 */}
                    <div className="holding-time-field">
                        {inputComponents.maxInput}
                    </div>

                    <div className="holding-time-field" ref={maxDropdownRef}>
                        <div
                            className="holding-time-select"
                            onClick={() => setIsMaxDropdownOpen(!isMaxDropdownOpen)}
                        >
                            {localMaxUnit}
                        </div>
                        {isMaxDropdownOpen && (
                            <div className="holding-time-options" ref={maxOptionsRef}>
                                {units.map(unit => (
                                    <div
                                        key={unit}
                                        className={`holding-time-option ${unit === localMaxUnit ? 'selected' : ''}`}
                                        onClick={() => {
                                            setLocalMaxUnit(unit);
                                            setIsMaxDropdownOpen(false);
                                        }}
                                    >
                                        {unit}
                                    </div>
                                ))}
                            </div>
                        )}
                    </div>
                </div>
            </div>
        </div>
    );
};

export default HoldingTimeFilter;
