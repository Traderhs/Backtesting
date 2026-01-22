import React, {forwardRef, useEffect, useMemo, useRef} from "react";
import {NumberFormatValues, NumericFormat} from 'react-number-format';

interface NumericInputProps {
    id?: string;
    name?: string;
    placeholder?: string;
    value: string | number;
    onChange: (value: string) => void;
    onBlur?: () => void;
    unit?: string; // '$', '%', 'x', '#', '회', '개' 등
    allowNegative?: boolean;
    isIntegerOnly?: boolean;
    className?: string;
    allowedChars?: string; // 허용된 문자 (예: "0123456789.-")
}

interface CustomInputProps extends React.InputHTMLAttributes<HTMLInputElement> {
    onCustomBlur?: () => void;
}

const NumericInput: React.FC<NumericInputProps> = ({
                                                       id,
                                                       name,
                                                       placeholder,
                                                       value,
                                                       onChange,
                                                       onBlur,
                                                       unit,
                                                       allowNegative = false,
                                                       isIntegerOnly = false,
                                                       className = "common-numeric-input",
                                                       allowedChars
                                                   }) => {
    // 로컬 임시 상태 저장 객체
    const localValuesRef = useRef<{ [key: string]: string }>({});
    // 현재 포커스 요소 ID 저장
    const activeFocusRef = useRef<string | null>(null);

    // 값 변경 핸들러
    const handleNumericFormatChange = (values: NumberFormatValues) => {
        // 입력 중에는 ref에만 값을 저장 (리렌더링 없음)
        localValuesRef.current['value'] = values.value;

        // **핵심: - 입력시 즉시 포맷 처리**
        if (values.value === '-' && activeFocusRef.current) {
            const activeInput = document.getElementById(activeFocusRef.current) as HTMLInputElement;
            if (activeInput && allowNegative) {
                const {prefix: prefixCalc, suffix: suffixCalc} = getPrefixSuffix(unit);

                // 접두사나 접미사가 있는 경우에만 즉시 포맷
                if (prefixCalc || suffixCalc) {
                    const newFormattedValue = prefixCalc ? `-${prefixCalc}${suffixCalc}` : `-${suffixCalc}`;

                    // 다음 틱에 DOM 직접 조작
                    setTimeout(() => {
                        if (activeInput && document.activeElement === activeInput) {
                            activeInput.value = newFormattedValue;

                            // 커서 위치 설정
                            const targetPos = prefixCalc ? 2 : 1;
                            activeInput.setSelectionRange(targetPos, targetPos);

                            // 이전 값 업데이트
                            activeInput.setAttribute('data-prev-value', newFormattedValue);

                            // **포맷된 상태를 로컬에 저장 (리렌더링 방지)**
                            localValuesRef.current['value'] = '-';
                            const inputId = activeInput.id;
                            if (inputId) {
                                localValuesRef.current[`${inputId}_formatted`] = newFormattedValue;
                            }
                        }
                    }, 0);

                    // **상태 업데이트는 하지 않음 - 리렌더링 방지**
                    return;
                }
            }
        }

        // 즉시 onChange 콜백 호출
        onChange(values.value);

        // 현재 활성화된 입력 필드가 있는지 확인
        if (activeFocusRef.current) {
            // 현재 활성화된 필드 찾기
            const activeInput = document.getElementById(activeFocusRef.current) as HTMLInputElement;
            if (activeInput) {
                // 새로운 값 저장 (중요: 모든 경우에 이전 값 업데이트 필요)
                activeInput.setAttribute('data-prev-value', values.formattedValue);
            }
        }
    };

    // 포커스가 빠져나갈 때 실제 상태 업데이트
    const handleBlurEvent = () => {
        const valueStr = localValuesRef.current['value'];

        // 값이 없으면 처리하지 않음
        if (valueStr === undefined) return;

        // 빈 값인 경우
        if (valueStr === '') {
            onChange('');
            if (onBlur) onBlur();
            return;
        }

        // **- 단독 입력의 경우 빈 값으로 처리**
        if (valueStr === '-') {
            onChange('');
            if (onBlur) onBlur();
            return;
        }

        // **-0 단독 입력의 경우 0으로 처리**
        if (valueStr === '-0') {
            onChange('0');
            if (onBlur) onBlur();
            return;
        }

        // **blur 시 leading/trailing zeros 모두 제거**
        let processedValue = valueStr;

        // 숫자 변환 후 다시 문자열로 변환하여 leading/trailing zeros 제거
        if (valueStr.includes('.') ||
            (valueStr.startsWith('0') && valueStr[1] !== '.') ||
            (valueStr.startsWith('-0') && valueStr[2] !== '.')) {
            const numValue = Number(valueStr);
            if (!isNaN(numValue)) {
                processedValue = String(numValue);
            }
        }

        // 원본 입력값과 처리된 값이 다르면 업데이트
        if (valueStr !== processedValue) {
            onChange(processedValue);
        }

        if (onBlur) onBlur();
    };

    // 접두사/접미사 위치 결정 ($, # 앞에, 나머지는 뒤에)
    const getPrefixSuffix = (unit?: string) => {
        if (!unit) return {prefix: '', suffix: ''};

        if (unit === '$' || unit === '#') {
            return {prefix: unit, suffix: ''};
        } else {
            return {prefix: '', suffix: unit};
        }
    };

    const {prefix, suffix} = getPrefixSuffix(unit);

    // **🎯 DOM Range API 기반 정밀 텍스트 위치 계산 (공통 함수)**
    const getAccurateTextPosition = (input: HTMLInputElement, clickX: number): number => {
        const value = input.value;
        if (!value) return 0;

        // **임시 측정용 요소 생성**
        const measurer = document.createElement('span');
        const computedStyle = window.getComputedStyle(input);

        // **input의 모든 텍스트 관련 스타일 복사**
        measurer.style.font = computedStyle.font;
        measurer.style.fontSize = computedStyle.fontSize;
        measurer.style.fontFamily = computedStyle.fontFamily;
        measurer.style.fontWeight = computedStyle.fontWeight;
        measurer.style.fontStyle = computedStyle.fontStyle;
        measurer.style.letterSpacing = computedStyle.letterSpacing;
        measurer.style.wordSpacing = computedStyle.wordSpacing;
        measurer.style.textTransform = computedStyle.textTransform;
        measurer.style.textAlign = computedStyle.textAlign;
        measurer.style.whiteSpace = 'pre'; // 공백 보존
        measurer.style.position = 'absolute';
        measurer.style.left = '-9999px';
        measurer.style.top = '-9999px';
        measurer.style.visibility = 'hidden';
        measurer.style.pointerEvents = 'none';
        measurer.style.zIndex = '-1';

        document.body.appendChild(measurer);

        try {
            // **input의 패딩/보더 계산**
            const paddingLeft = parseFloat(computedStyle.paddingLeft) || 0;
            const borderLeft = parseFloat(computedStyle.borderLeftWidth) || 0;
            const leftOffset = paddingLeft + borderLeft;

            // **클릭 위치에서 오프셋 제거**
            const adjustedClickX = Math.max(0, clickX - leftOffset);

            let bestPosition = 0;
            let minDistance = Infinity;

            // **각 문자 위치별로 정확한 측정**
            for (let i = 0; i <= value.length; i++) {
                // **현재 위치까지의 텍스트**
                const textUpToPosition = value.substring(0, i);
                measurer.textContent = textUpToPosition || '\u00A0'; // 빈 텍스트면 공백 사용

                // **Range API로 정확한 너비 계산**
                const range = document.createRange();
                range.selectNodeContents(measurer);
                const rect = range.getBoundingClientRect();
                const textWidth = textUpToPosition ? rect.width : 0;

                // **클릭 위치와의 거리 계산**
                const distance = Math.abs(adjustedClickX - textWidth);

                if (distance < minDistance) {
                    minDistance = distance;
                    bestPosition = i;
                }

                // **문자 중간 지점도 고려 (더 정밀한 계산)**
                if (i < value.length) {
                    measurer.textContent = value.substring(0, i + 1);
                    range.selectNodeContents(measurer);
                    const nextRect = range.getBoundingClientRect();
                    const nextWidth = nextRect.width;

                    // **문자의 중간 지점**
                    const midPoint = (textWidth + nextWidth) / 2;
                    const midDistance = Math.abs(adjustedClickX - midPoint);

                    if (midDistance < minDistance) {
                        minDistance = midDistance;
                        bestPosition = adjustedClickX > midPoint ? i + 1 : i;
                    }
                }
            }

            return bestPosition;
        } finally {
            // **측정용 요소 제거 (에러 상황에서도 확실히 제거)**
            document.body.removeChild(measurer);
        }
    };

    // 커스텀 입력 컴포넌트 정의
    const CustomInput = useMemo(() => {
        return forwardRef<HTMLInputElement, CustomInputProps>(
            (props, ref) => {
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

                const handleClick = (e: React.MouseEvent<HTMLInputElement>) => {
                    const input = e.currentTarget;
                    const value = input.value;

                    // **쉼표 위치 클릭 처리**
                    if (value && value.includes(',')) {
                        // 정확한 클릭 위치 계산
                        const rect = input.getBoundingClientRect();
                        const clickX = e.clientX - rect.left;
                        const actualClickPosition = getAccurateTextPosition(input, clickX);

                        // 쉼표 위치들 찾기
                        const commaPositions = [];
                        for (let i = 0; i < value.length; i++) {
                            if (value[i] === ',') {
                                commaPositions.push(i);
                            }
                        }

                        // 가장 가까운 쉼표 위치 찾기
                        let closestCommaPos = -1;
                        let minDistance = Infinity;

                        for (const commaPos of commaPositions) {
                            const distance = Math.abs(actualClickPosition - commaPos);
                            if (distance < minDistance) {
                                minDistance = distance;
                                closestCommaPos = commaPos;
                            }
                        }

                        // 쉼표 근처 클릭 시 정확한 위치 결정
                        if (closestCommaPos !== -1 && minDistance <= 0.5) {
                            e.preventDefault();
                            e.stopPropagation();

                            let targetPosition;
                            if (actualClickPosition < closestCommaPos + 0.5) {
                                // 쉼표 왼쪽 클릭 -> 쉼표 앞으로
                                targetPosition = closestCommaPos;
                            } else {
                                // 쉼표 오른쪽 클릭 -> 쉼표 뒤로
                                targetPosition = closestCommaPos + 1;
                            }

                            input.setSelectionRange(targetPosition, targetPosition);
                            return;
                        }
                    }

                    // **추가 클릭 검증 - mousedown에서 놓친 경우를 대비한 최종 차단**
                    if (value && (value.includes('$') || value.includes('#') || value.includes('%') || value.includes('x') || value.includes('회') || value.includes('개'))) {
                        const cursorPosition = input.selectionStart || 0;

                        // 허용된 영역 계산
                        let allowedStart = 0;
                        let allowedEnd = value.length;

                        // 접두사 처리
                        if (value.startsWith('-$') || value.startsWith('-#')) {
                            allowedStart = 2;
                        } else if (value.startsWith('$') || value.startsWith('#')) {
                            allowedStart = 1;
                        } else if (value.startsWith('-') && /[%x회개]$/.test(value)) {
                            allowedStart = 1; // 음수 접미사는 - 뒤부터 허용
                        }

                        // 접미사 처리
                        if (value.endsWith('%') || value.endsWith('x') || value.endsWith('회') || value.endsWith('개')) {
                            allowedEnd = value.length - 1;
                        }

                        // **허용되지 않는 영역에 커서가 있으면 즉시 차단하고 이동**
                        if (cursorPosition < allowedStart || cursorPosition > allowedEnd) {
                            e.preventDefault();
                            e.stopPropagation();

                            const targetPosition = cursorPosition < allowedStart ? allowedStart : allowedEnd;
                            input.setSelectionRange(targetPosition, targetPosition);

                            return;
                        }
                    }

                    // 다른 필드에 포커스가 있으면 해제
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
                        localValuesRef.current[input.id] = 'focused';
                    }

                    // **완전한 클릭 위치 제어 - 가장 가까운 허용 영역으로 이동**
                    setTimeout(() => {
                        if (input === document.activeElement && value && (value.includes('$') || value.includes('#') || value.includes('%') || value.includes('x') || value.includes('회') || value.includes('개'))) {
                            const cursorPosition = input.selectionStart || 0;

                            // 허용된 영역 계산
                            let allowedStart = 0;
                            let allowedEnd = value.length;

                            // 접두사 처리
                            if (value.startsWith('-$') || value.startsWith('-#')) {
                                allowedStart = 2;
                            } else if (value.startsWith('$') || value.startsWith('#')) {
                                allowedStart = 1;
                            } else if (value.startsWith('-') && /[%x회개]$/.test(value)) {
                                allowedStart = 1; // 음수 접미사는 - 뒤부터 허용
                            }

                            // 접미사 처리
                            if (value.endsWith('%') || value.endsWith('x') || value.endsWith('회') || value.endsWith('개')) {
                                allowedEnd = value.length - 1;
                            }

                            // **허용되지 않는 영역 클릭 시 가장 가까운 위치로 이동**
                            let targetPosition = cursorPosition;
                            let needsAdjustment = false;

                            // 허용 영역 밖을 클릭한 경우
                            if (cursorPosition < allowedStart) {
                                // 접두사 영역 클릭 → 허용 영역 시작점으로
                                targetPosition = allowedStart;
                                needsAdjustment = true;
                            } else if (cursorPosition > allowedEnd) {
                                // 접미사 영역 클릭 → 허용 영역 끝점으로
                                targetPosition = allowedEnd;
                                needsAdjustment = true;
                            }


                            // **커서 위치 조정이 필요한 경우**
                            if (needsAdjustment) {
                                input.setSelectionRange(targetPosition, targetPosition);

                                // 확실히 적용되도록 여러 번 호출
                                requestAnimationFrame(() => {
                                    if (input === document.activeElement) {
                                        input.setSelectionRange(targetPosition, targetPosition);
                                    }
                                });
                            }
                        }
                    }, 0);

                    // 기존 onClick 이벤트 호출
                    if (props.onClick) props.onClick(e);
                };

                const handleFocus = (e: React.FocusEvent<HTMLInputElement>) => {
                    const input = e.currentTarget;
                    const value = input.value;

                    // 현재 입력 필드를 활성 포커스로 설정
                    if (input.id) {
                        activeFocusRef.current = input.id;
                        localValuesRef.current[input.id] = 'focused';
                    }

                    // **포커스 시에도 커서 위치 검증 및 조정**
                    setTimeout(() => {
                        if (input === document.activeElement && value && (value.includes('$') || value.includes('#') || value.includes('%') || value.includes('x') || value.includes('회') || value.includes('개'))) {
                            const cursorPosition = input.selectionStart || 0;

                            // 허용된 영역 계산
                            let allowedStart = 0;
                            let allowedEnd = value.length;

                            // 접두사 처리
                            if (value.startsWith('-$') || value.startsWith('-#')) {
                                allowedStart = 2;
                            } else if (value.startsWith('$') || value.startsWith('#')) {
                                allowedStart = 1;
                            } else if (value.startsWith('-') && /[%x회개]$/.test(value)) {
                                allowedStart = 1; // 음수 접미사는 - 뒤부터 허용
                            }

                            // 접미사 처리
                            if (value.endsWith('%') || value.endsWith('x') || value.endsWith('회') || value.endsWith('개')) {
                                allowedEnd = value.length - 1;
                            }

                            let targetPosition = cursorPosition;
                            let needsAdjustment = false;

                            // **허용 영역을 벗어난 경우 가장 가까운 위치로 이동**
                            if (cursorPosition < allowedStart || cursorPosition > allowedEnd) {
                                targetPosition = cursorPosition < allowedStart ? allowedStart : allowedEnd;
                                needsAdjustment = true;
                            }

                            // **쉼표 위치 포커스 처리 - 자연스러운 커서 위치**
                            if (value.includes(',')) {
                                const charAtCursor = cursorPosition < value.length ? value[cursorPosition] : '';
                                if (charAtCursor === ',') {
                                    // 쉼표 위치에 포커스한 경우 쉼표 뒤로 이동 (기본 동작)
                                    targetPosition = cursorPosition + 1;
                                    needsAdjustment = true;
                                }
                            }

                            if (needsAdjustment) {
                                input.setSelectionRange(targetPosition, targetPosition);
                            }
                        }
                    }, 0);

                    // 기존 onFocus 이벤트 호출
                    if (props.onFocus) props.onFocus(e);
                };

                const handleBlur = (e: React.FocusEvent<HTMLInputElement>) => {
                    // **실제 input value로 leading/trailing zeros 처리**
                    const input = e.currentTarget;
                    const currentValue = input.value;

                    // 접두사/접미사 제거하여 순수 값 추출
                    let pureValue = currentValue;
                    // 음수 부호는 남겨두고 접두사 제거
                    if (currentValue.startsWith('-$')) {
                        pureValue = '-' + currentValue.substring(2);
                    } else if (currentValue.startsWith('$') || currentValue.startsWith('#')) {
                        pureValue = currentValue.substring(1);
                    }
                    pureValue = pureValue.replace(/[%x회개]$/, '');

                    // leading/trailing zeros 제거 필요한지 확인
                    if (pureValue && pureValue !== '-' && pureValue !== '.') {
                        const numValue = Number(pureValue);
                        if (!isNaN(numValue)) {
                            const processedPureValue = String(numValue);
                            if (pureValue !== processedPureValue) {
                                // 접두사/접미사 복원
                                let newFormattedValue = processedPureValue;

                                // 음수 부호 처리
                                const isNegative = processedPureValue.startsWith('-');
                                const numberPart = isNegative ? processedPureValue.substring(1) : processedPureValue;

                                if (currentValue.startsWith('-$') || (currentValue.startsWith('$') && isNegative)) {
                                    newFormattedValue = `-$${numberPart}`;
                                } else if (currentValue.startsWith('$')) {
                                    newFormattedValue = `$${numberPart}`;
                                } else if (currentValue.startsWith('-#') || (currentValue.startsWith('#') && isNegative)) {
                                    newFormattedValue = `-#${numberPart}`;
                                } else if (currentValue.startsWith('#')) {
                                    newFormattedValue = `#${numberPart}`;
                                }

                                if (currentValue.endsWith('%')) newFormattedValue += '%';
                                else if (currentValue.endsWith('x')) newFormattedValue += 'x';
                                else if (currentValue.endsWith('회')) newFormattedValue += '회';
                                else if (currentValue.endsWith('개')) newFormattedValue += '개';

                                // input 값 즉시 업데이트
                                input.value = newFormattedValue;

                                // 상태도 업데이트
                                const event = new Event('input', {bubbles: true});
                                input.dispatchEvent(event);
                            }
                        }
                    }

                    // 커스텀 onBlur 이벤트 호출
                    if (props.onCustomBlur) {
                        props.onCustomBlur();
                    }

                    // ID를 저장에서 제거
                    if (e.currentTarget.id) {
                        delete localValuesRef.current[e.currentTarget.id];

                        // 현재 활성 포커스가 이 요소인 경우에만 초기화
                        if (activeFocusRef.current === e.currentTarget.id) {
                            activeFocusRef.current = null;
                        }
                    }

                    // 기존 onBlur 이벤트 호출
                    if (props.onBlur) {
                        props.onBlur(e);
                    }
                };

                // 커서 위치 복원 처리 및 글로벌 마우스 이벤트 처리
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

                                            // 접미사 존재 여부 확인
                                            const hasSuffix = /[%x회개]$/.test(currentValue);
                                            const suffixStartPos = hasSuffix ? currentValue.search(/[%x회개]$/) : currentValue.length;

                                            for (let i = 0; i < currentValue.length; i++) {
                                                // 접미사 영역에 도달하면 중단
                                                if (hasSuffix && i >= suffixStartPos) {
                                                    break;
                                                }

                                                if (/\d/.test(currentValue[i])) {
                                                    if (digitCount === pos) {
                                                        targetPos = i;
                                                        break;
                                                    }
                                                    digitCount++;
                                                }
                                                targetPos = i + 1;
                                            }

                                            // 접미사가 있는 경우 최대 위치 제한
                                            if (hasSuffix) {
                                                targetPos = Math.min(targetPos, suffixStartPos);
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

                    // 커스텀 드래그 시스템
                    const input = inputRef.current;
                    if (input) {
                        let customDragActive = false;
                        let dragStartPos = -1;
                        let dragStartValue = '';
                        let allowedStart = 0;
                        let allowedEnd = 0;

                        // 브라우저의 텍스트 선택 동작 차단
                        const blockAllSelection = (e: Event) => {
                            const target = e.target as HTMLInputElement;
                            const value = target.value;

                            if (value && (value.includes('$') || value.includes('#') || value.includes('%') || value.includes('x') || value.includes('회') || value.includes('개'))) {
                                e.preventDefault();
                                e.stopPropagation();
                                e.stopImmediatePropagation();
                                return false;
                            }
                        };

                        // 커스텀 드래그 시작
                        const handleCustomMouseDown = (e: MouseEvent) => {
                            if (e.target !== input) return;

                            // 트리플 클릭 이상일 때 기본 동작(줄 전체 선택)을 막고 더블 클릭 상태 유지
                            if (e.detail >= 3) {
                                e.preventDefault();
                                return;
                            }

                            const value = input.value;
                            if (value && (value.includes('$') || value.includes('#') || value.includes('%') || value.includes('x') || value.includes('회') || value.includes('개'))) {
                                e.preventDefault();
                                e.stopPropagation();
                                e.stopImmediatePropagation();

                                if (document.activeElement !== input) {
                                    input.focus();
                                }

                                // 허용 영역 계산
                                allowedStart = 0;
                                allowedEnd = value.length;

                                if (value.startsWith('-$') || value.startsWith('-#')) {
                                    allowedStart = 2;
                                } else if (value.startsWith('$') || value.startsWith('#')) {
                                    allowedStart = 1;
                                } else if (value.startsWith('-') && /[%x회개]$/.test(value)) {
                                    allowedStart = 1;
                                }

                                if (value.endsWith('%') || value.endsWith('x') || value.endsWith('회') || value.endsWith('개')) {
                                    allowedEnd = value.length - 1;
                                }

                                // 정확한 클릭 위치 계산
                                const rect = input.getBoundingClientRect();
                                const clickX = e.clientX - rect.left;
                                const clickPosition = getAccurateTextPosition(input, clickX);

                                // 접두사/접미사 밖 클릭 시 스마트하게 가장 가까운 문자 영역으로
                                let smartClickPosition = clickPosition;

                                if (clickPosition < allowedStart) {
                                    smartClickPosition = allowedStart;
                                } else if (clickPosition > allowedEnd) {
                                    smartClickPosition = allowedEnd;
                                }

                                // 커스텀 드래그 시작
                                customDragActive = true;
                                dragStartPos = smartClickPosition;
                                dragStartValue = value;

                                input.setSelectionRange(smartClickPosition, smartClickPosition);

                                return false;
                            }
                        };

                        // 커스텀 드래그 진행
                        const handleCustomMouseMove = (e: MouseEvent) => {
                            if (!customDragActive) return;

                            const value = input.value;
                            if (value !== dragStartValue) {
                                customDragActive = false;
                                return;
                            }

                            e.preventDefault();
                            e.stopPropagation();
                            e.stopImmediatePropagation();

                            const rect = input.getBoundingClientRect();
                            const mouseX = e.clientX - rect.left;
                            const mousePosition = getAccurateTextPosition(input, mouseX);

                            // 스마트 마우스 위치 조정
                            let smartMousePosition = mousePosition;

                            if (mousePosition < allowedStart) {
                                smartMousePosition = allowedStart;
                            } else if (mousePosition > allowedEnd) {
                                smartMousePosition = allowedEnd;
                            }

                            // 선택 범위 계산
                            const selectionStart = Math.min(dragStartPos, smartMousePosition);
                            const selectionEnd = Math.max(dragStartPos, smartMousePosition);

                            if (selectionStart >= allowedStart && selectionEnd <= allowedEnd) {
                                if (selectionStart === selectionEnd) {
                                    input.setSelectionRange(selectionStart, selectionStart);
                                } else {
                                    input.setSelectionRange(selectionStart, selectionEnd);
                                }
                            }

                            return false;
                        };

                        // 커스텀 드래그 종료
                        const handleCustomMouseUp = (e: MouseEvent) => {
                            if (customDragActive) {
                                customDragActive = false;
                                dragStartPos = -1;
                                dragStartValue = '';

                                e.preventDefault();
                                e.stopPropagation();

                                // click 이벤트가 선택을 해제하는 것을 막기 위해
                                // 이벤트를 한 번 더 차단합니다.
                                const blockClick = (clickEvent: MouseEvent) => {
                                    clickEvent.stopPropagation();
                                    clickEvent.preventDefault();
                                    document.removeEventListener('click', blockClick, true);
                                };
                                document.addEventListener('click', blockClick, {capture: true, once: true});

                                return false;
                            }
                        };

                        // 이벤트 등록 - 중복 제거
                        const events = [
                            'selectstart', 'dragstart', 'drag', 'dragend',
                            'touchstart', 'touchmove', 'touchend'
                        ];

                        events.forEach(eventName => {
                            input.addEventListener(eventName, blockAllSelection, {capture: true, passive: false});
                        });

                        // mousedown만 input에 등록
                        input.addEventListener('mousedown', handleCustomMouseDown, {capture: true, passive: false});

                        // 글로벌 이벤트는 document에만 등록 (window 중복 제거)
                        document.addEventListener('mousemove', handleCustomMouseMove, {capture: true, passive: false});
                        document.addEventListener('mouseup', handleCustomMouseUp, {capture: true, passive: false});

                        return () => {
                            events.forEach(eventName => {
                                input.removeEventListener(eventName, blockAllSelection, {capture: true});
                            });

                            input.removeEventListener('mousedown', handleCustomMouseDown, {capture: true});
                            document.removeEventListener('mousemove', handleCustomMouseMove, {capture: true});
                            document.removeEventListener('mouseup', handleCustomMouseUp, {capture: true});
                        };
                    }

                    // 필드 클릭 시 부모 컨테이너 클릭 이벤트가 방해하지 않도록 처리
                    const parentFields = document.querySelectorAll('.numeric-field');

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
                });

                const handleKeyDown = (e: React.KeyboardEvent<HTMLInputElement>) => {
                    const input = e.currentTarget;
                    const value = input.value;

                    // 허용된 제어 키 목록
                    const allowedControlKeys = [
                        'Backspace', 'Delete', 'Tab', 'Enter', 'Escape',
                        'ArrowLeft', 'ArrowRight', 'ArrowUp', 'ArrowDown', 'Home', 'End'
                    ];

                    // 제어 키, 탐색 키, 또는 수정자 키(Ctrl, Alt, Shift, Meta) 조합이 아닌 경우에만 문자 입력을 검사
                    if (!(e.ctrlKey || e.metaKey || e.altKey || e.shiftKey || allowedControlKeys.includes(e.key))) {
                        // allowedChars가 설정된 경우에만 검사
                        if (allowedChars) {
                            // 허용된 문자가 아니면 입력 차단
                            if (e.key.length === 1 && !allowedChars.includes(e.key)) {
                                e.preventDefault();
                                return;
                            }
                        }
                    }

                    // **쉼표 직접 입력 차단**
                    if (e.key === ',') {
                        e.preventDefault();
                        return;
                    }

                    // **STEP 1: 제한 영역 포함하는 선택만 정밀 차단 (허용 영역 내 선택은 허용)**
                    if (value && (value.includes('$') || value.includes('#') || value.includes('%') || value.includes('x') || value.includes('회') || value.includes('개'))) {
                        // 허용된 선택 범위 계산
                        let allowedStart = 0;
                        let allowedEnd = value.length;

                        // 접두사 처리
                        if (value.startsWith('-$') || value.startsWith('-#')) {
                            allowedStart = 2;
                        } else if (value.startsWith('$') || value.startsWith('#')) {
                            allowedStart = 1;
                        } else if (value.startsWith('-') && /[%x회개]$/.test(value)) {
                            allowedStart = 1; // 음수 접미사는 - 뒤부터 허용
                        }

                        // 접미사 처리
                        if (value.endsWith('%') || value.endsWith('x') || value.endsWith('회') || value.endsWith('개')) {
                            allowedEnd = value.length - 1;
                        }

                        // **핵심: 제한 영역을 포함하는 선택만 차단하고 허용된 영역 내 선택은 허용**

                        // Ctrl+A / Cmd+A: 전체 선택 → 허용된 영역만 선택
                        if (e.key.toLowerCase() === 'a' && (e.ctrlKey || e.metaKey)) {
                            e.preventDefault();
                            e.stopPropagation();

                            // **강제로 허용된 영역만 선택**
                            input.setSelectionRange(allowedStart, allowedEnd);

                            // 확실히 적용되도록 여러 번 호출
                            setTimeout(() => {
                                if (input === document.activeElement) {
                                    input.setSelectionRange(allowedStart, allowedEnd);
                                }
                            }, 0);

                            return;
                        }

                        // Shift 조합 - 제한 영역 포함 여부만 체크
                        if (e.shiftKey && (e.key === 'ArrowLeft' || e.key === 'ArrowRight' || e.key === 'ArrowUp' || e.key === 'ArrowDown' ||
                            e.key === 'Home' || e.key === 'End' || (e.ctrlKey || e.metaKey))) {

                            // **Shift + 위/아래 방향키: 브라우저 기본 동작 차단하고 커서 위치 기준 제한된 선택**
                            if (e.key === 'ArrowUp' || e.key === 'ArrowDown') {
                                e.preventDefault();
                                e.stopPropagation();

                                const currentStart = input.selectionStart || 0;
                                const currentEnd = input.selectionEnd || 0;

                                let newStart, newEnd;

                                if (e.key === 'ArrowUp') {
                                    // 위쪽: 시작점을 허용된 영역 시작점으로
                                    if (currentStart === currentEnd) {
                                        // 커서 상태: 현재 위치에서 허용된 시작점까지 선택
                                        newStart = allowedStart;
                                        newEnd = currentEnd;
                                    } else {
                                        // 이미 선택된 상태: 시작점을 허용된 시작점으로 확장
                                        newStart = allowedStart;
                                        newEnd = currentEnd;
                                    }
                                } else { // ArrowDown
                                    // 아래쪽: 끝점을 허용된 영역 끝점으로
                                    if (currentStart === currentEnd) {
                                        // 커서 상태: 현재 위치에서 허용된 끝점까지 선택
                                        newStart = currentStart;
                                        newEnd = allowedEnd;
                                    } else {
                                        // 이미 선택된 상태: 끝점을 허용된 끝점으로 확장
                                        newStart = currentStart;
                                        newEnd = allowedEnd;
                                    }
                                }

                                // 제한된 범위 내에서만 선택
                                newStart = Math.max(newStart, allowedStart);
                                newEnd = Math.min(newEnd, allowedEnd);

                                if (newStart < newEnd) {
                                    input.setSelectionRange(newStart, newEnd);
                                }

                                return;
                            }

                            const currentStart = input.selectionStart || 0;
                            const currentEnd = input.selectionEnd || 0;

                            // 예상 선택 영역 계산 (좌/우만)
                            let newStart = currentStart;
                            let newEnd = currentEnd;

                            if (e.key === 'ArrowLeft') {
                                if (currentStart === currentEnd) {
                                    newStart = Math.max(0, currentStart - 1);
                                } else {
                                    newStart = Math.max(0, currentStart - 1);
                                }
                            } else if (e.key === 'ArrowRight') {
                                if (currentStart === currentEnd) {
                                    newEnd = Math.min(value.length, currentEnd + 1);
                                } else {
                                    newEnd = Math.min(value.length, currentEnd + 1);
                                }
                            } else if (e.key === 'Home') {
                                newStart = 0;
                            } else if (e.key === 'End') {
                                newEnd = value.length;
                            } else if ((e.ctrlKey || e.metaKey) && (e.key === 'ArrowLeft' || e.key === 'ArrowRight')) {
                                // Ctrl+Shift+방향키는 복잡하므로 아예 차단
                                e.preventDefault();
                                e.stopPropagation();
                                return;
                            }

                            // 제한 영역을 포함하는지 체크
                            if (newStart < allowedStart || newEnd > allowedEnd) {
                                e.preventDefault();
                                e.stopPropagation();

                                // 허용된 영역만 선택하도록 조정
                                const adjustedStart = Math.max(newStart, allowedStart);
                                const adjustedEnd = Math.min(newEnd, allowedEnd);

                                // 조정된 범위가 의미있으면 선택, 아니면 그냥 차단
                                if (adjustedStart < adjustedEnd) {
                                    input.setSelectionRange(adjustedStart, adjustedEnd);
                                }

                                return;
                            }

                            // 허용된 영역 내의 선택이면 기본 동작 허용
                        }
                    }
                    const cursorPosition = input.selectionStart || 0;
                    const selectionLength = (input.selectionEnd || 0) - cursorPosition;
                    const hasSelection = selectionLength > 0;

                    // 소수점(.) 키 통합 처리 시스템
                    if (e.key === '.' && !isIntegerOnly) {
                        // 빈 필드에 . 입력
                        if (!value) {
                            e.preventDefault();
                            input.value = `${prefix}0.${suffix}`;
                            localValuesRef.current['value'] = '0.';
                            onChange('0.');
                            const targetPos = prefix.length + 2;
                            const cursorKey = `${input.id}_cursor`;
                            localValuesRef.current[cursorKey] = String(targetPos);
                            return;
                        }

                        // 이미 소수점이 있는 경우 - 소수점 이동 처리
                        if (value.includes('.')) {
                            e.preventDefault();

                            const currentDotIndex = value.indexOf('.');
                            if (cursorPosition !== currentDotIndex) {
                                // 현재 순수 값 가져오기
                                let pureValue = value;
                                let isNegative: boolean;

                                // 음수 여부 확인 및 접두사 제거
                                if (prefix) {
                                    isNegative = value.startsWith('-' + prefix) || value.startsWith(prefix + '-');
                                    pureValue = pureValue.replace(/^-?\$/, '').replace(/^\$-?/, '');
                                    pureValue = pureValue.replace(/^-?#/, '').replace(/^#-?/, '');
                                } else if (suffix) {
                                    isNegative = value.startsWith('-');
                                    pureValue = pureValue.replace(/^-/, '');
                                } else {
                                    isNegative = value.startsWith('-');
                                    pureValue = pureValue.replace(/^-/, '');
                                }

                                pureValue = pureValue.replace(/[%x회개]$/, '');

                                // 소수점과 쉼표 제거하여 순수 숫자만 추출
                                let newPureValue = pureValue.replace(/[.,]/g, '');

                                // 커서 위치 조정 (순수 값 기준)
                                let adjustedPosition = cursorPosition;
                                if (value.startsWith('-$')) adjustedPosition -= 2;
                                else if (value.startsWith('$') || value.startsWith('#')) adjustedPosition -= 1;
                                else if (value.startsWith('-') && suffix) adjustedPosition -= 1;

                                if (prefix && cursorPosition === 0 && !value.startsWith('-')) {
                                    adjustedPosition = 0;
                                }

                                const pureDotIndex = pureValue.indexOf('.');
                                if (adjustedPosition > pureDotIndex && pureDotIndex !== -1) {
                                    adjustedPosition -= 1;
                                }

                                const pureValueBeforeCursor = pureValue.substring(0, Math.min(adjustedPosition, pureValue.length));
                                const commaCount = (pureValueBeforeCursor.match(/,/g) || []).length;
                                adjustedPosition -= commaCount;

                                // 새 위치에 소수점 삽입
                                if (hasSelection) {
                                    const selectionEndAdjusted = adjustedPosition + selectionLength;
                                    newPureValue = newPureValue.substring(0, adjustedPosition) + '.' +
                                        newPureValue.substring(selectionEndAdjusted);
                                } else {
                                    newPureValue = newPureValue.substring(0, adjustedPosition) + '.' +
                                        newPureValue.substring(adjustedPosition);
                                }

                                if (newPureValue.startsWith('.')) {
                                    newPureValue = '0' + newPureValue;
                                }

                                if (/^0\d+\./.test(newPureValue)) {
                                    newPureValue = newPureValue.substring(1);
                                }

                                if (isNegative) {
                                    newPureValue = '-' + newPureValue;
                                }

                                localValuesRef.current['value'] = newPureValue;
                                onChange(newPureValue);

                                const pureNumberPart = newPureValue.startsWith('-') ? newPureValue.substring(1) : newPureValue;
                                const dotIndexInNumber = pureNumberPart.indexOf('.');
                                const digitsBeforeDot = pureNumberPart.substring(0, dotIndexInNumber).replace(/[^0-9]/g, '').length;

                                const cursorKey = `${input.id}_cursor`;
                                localValuesRef.current[cursorKey] = String(digitsBeforeDot);
                                localValuesRef.current[`${cursorKey}_mode`] = 'digit_count';
                            }
                            return;
                        } else {
                            // 새로운 소수점 추가 로직
                            let pureValue = value;
                            let isNegative = false;
                            let cleanNumber: string;

                            // 음수 여부 및 순수 값 추출
                            if (value.startsWith('-$') || value.startsWith('-#')) {
                                isNegative = true;
                                pureValue = pureValue.replace(/^-[$#]/, '');
                            } else if (value.startsWith('-') && /[%x회개]$/.test(value)) {
                                isNegative = true;
                                pureValue = pureValue.replace(/^-/, '');
                            } else if (value.startsWith('$') || value.startsWith('#')) {
                                pureValue = pureValue.replace(/^[$#]/, '');
                            }

                            pureValue = pureValue.replace(/[%x회개]$/, '');
                            cleanNumber = pureValue.replace(/,/g, '');

                            // 커서 위치별 소수점 처리
                            let shouldProcess = false;
                            let newPureValue = '';
                            let targetPos = 0;

                            // Case 1: 음수 접두사 필드 (-$123)
                            if (isNegative && prefix) {
                                if (value === `-${prefix}${suffix}`) {
                                    // -$만 있는 상태
                                    shouldProcess = true;
                                    newPureValue = '-0.';
                                    targetPos = prefix.length + 3;
                                } else if (cursorPosition === 1 || cursorPosition === prefix.length + 1) {
                                    // - 뒤 또는 $ 뒤에 . 입력
                                    shouldProcess = true;
                                    newPureValue = `-0.${cleanNumber}`;
                                    targetPos = prefix.length + 3;
                                }
                            }
                            // Case 2: 음수 접미사 필드 (-123%)
                            else if (isNegative && suffix) {
                                if (pureValue === '' && cursorPosition === 1) {
                                    // -% 상태에서 - 뒤에 . 입력
                                    shouldProcess = true;
                                    newPureValue = '-0.';
                                    targetPos = 3;
                                } else if ((cursorPosition === 0 || cursorPosition === 1) && cleanNumber) {
                                    // -1,234% 상태에서 맨 앞(position 0) 또는 - 뒤(position 1)에 . 입력 시 -0.1234%로 변환
                                    shouldProcess = true;
                                    newPureValue = `-0.${cleanNumber}`;
                                    targetPos = 3;
                                }
                            }
                            // Case 3: 양수 접두사 필드 ($123)
                            else if (!isNegative && prefix) {
                                if (cursorPosition === 0 || cursorPosition === prefix.length) {
                                    // 맨 앞 또는 $ 뒤에 . 입력
                                    shouldProcess = true;
                                    newPureValue = `0.${cleanNumber}`;
                                    targetPos = prefix.length + 2;
                                }
                            }
                            // Case 4: 양수 접미사 또는 일반 필드 (123%)
                            else if (!isNegative) {
                                if (cursorPosition === 0) {
                                    // 맨 앞에 . 입력
                                    shouldProcess = true;
                                    newPureValue = `0.${cleanNumber}`;
                                    targetPos = 2;
                                }
                            }

                            if (shouldProcess) {
                                e.preventDefault();
                                input.value = `${prefix}${newPureValue}${suffix}`;
                                localValuesRef.current['value'] = newPureValue;
                                onChange(newPureValue);
                                const cursorKey = `${input.id}_cursor`;
                                localValuesRef.current[cursorKey] = String(targetPos);
                                return;
                            }
                        }
                    }

                    // 0 뒤에 1-9 숫자 입력 - 즉시 0 제거 (모든 필드에 적용)
                    if (/^[1-9]$/.test(e.key) && value && !isIntegerOnly) {

                        // 접두사/접미사 제거하여 순수 값 확인
                        let pureValue = value;
                        let isNegative = false;

                        // 음수 여부 확인
                        if (value.startsWith('-$') || value.startsWith('-#')) {
                            isNegative = true;
                            pureValue = pureValue.replace(/^-[$#]/, '');
                        } else if (value.startsWith('-') && /[%x회개]$/.test(value)) {
                            isNegative = true;
                            pureValue = pureValue.replace(/^-/, '');
                        } else {
                            pureValue = pureValue.replace(/^[$#]/, '');
                        }
                        pureValue = pureValue.replace(/[%x회개]$/, '');

                        // 0으로 시작하고 0. 이 아닌 경우 (예: "0", "$0", "0%", "-$0")
                        if (pureValue === '0' && !hasSelection) {
                            e.preventDefault();

                            // 즉시 값 변경 (0을 새 숫자로 교체, 음수 부호 유지)
                            const newPureValue = isNegative ? `-${e.key}` : e.key;
                            input.value = `${prefix}${newPureValue}${suffix}`;

                            // 상태 업데이트
                            localValuesRef.current['value'] = newPureValue;
                            onChange(newPureValue);

                            // 커서 위치 설정 (음수 부호 고려)
                            const targetPos = prefix.length + (isNegative ? 2 : 1); // 음수면 -1, 양수면 1
                            const cursorKey = `${input.id}_cursor`;
                            localValuesRef.current[cursorKey] = String(targetPos);

                            return;
                        }
                    }

                    // 소수점 키 처리 - 이미 소수점이 있는 경우에도 새로운 위치에 소수점 입력 처리
                    if (e.key === '.' && value.includes('.')) {
                        e.preventDefault();

                        // 현재 소수점 위치
                        const currentDotIndex = value.indexOf('.');

                        // 소수점 이동 처리
                        if (cursorPosition !== currentDotIndex) {
                            // 현재 순수 값 가져오기
                            let pureValue = value;
                            // **마이너스 여부 확인 - 접두사/접미사 고려**
                            let isNegative: boolean;

                            // 접두사가 있는 경우 (-$123, $-123 등)
                            if (prefix) {
                                isNegative = value.startsWith('-' + prefix) || value.startsWith(prefix + '-');
                                pureValue = pureValue.replace(/^-?\$/, '').replace(/^\$-?/, '');
                                pureValue = pureValue.replace(/^-?#/, '').replace(/^#-?/, '');
                            }
                            // 접미사만 있는 경우 (-123%, 123-% 등)
                            else if (suffix) {
                                isNegative = value.startsWith('-') || value.includes('-');
                                pureValue = pureValue.replace(/^-/, '').replace(/-/, '');
                            }
                            // 접두사/접미사가 없는 경우
                            else {
                                isNegative = value.startsWith('-');
                                pureValue = pureValue.replace(/^-/, '');
                            }

                            // 접미사 제거
                            pureValue = pureValue.replace(/[%x회개]$/, '');

                            // 소수점 제거
                            let newPureValue = pureValue.replace('.', '');

                            // 원래 커서 위치 조정 (순수 값 기준)
                            let adjustedPosition = cursorPosition;
                            // 접두사 길이만큼 빼기
                            if (value.startsWith('-$')) adjustedPosition -= 2;
                            else if (value.startsWith('$') || value.startsWith('#')) adjustedPosition -= 1;
                            else if (value.startsWith('-') && suffix) adjustedPosition -= 1;

                            // 기존 소수점보다 뒤에 있었다면 조정
                            const pureDotIndex = pureValue.indexOf('.');
                            if (adjustedPosition > pureDotIndex) {
                                adjustedPosition -= 1;
                            }

                            // 새 위치에 소수점 삽입
                            if (hasSelection) {
                                const selectionEndAdjusted = adjustedPosition + selectionLength;
                                newPureValue = newPureValue.substring(0, adjustedPosition) + '.' +
                                    newPureValue.substring(selectionEndAdjusted);
                            } else {
                                newPureValue = newPureValue.substring(0, adjustedPosition) + '.' +
                                    newPureValue.substring(adjustedPosition);
                            }

                            // 소수점이 맨 앞에 오면 0 추가
                            if (newPureValue.startsWith('.')) {
                                newPureValue = '0' + newPureValue;
                            }

                            // 01.23 같은 형태면 앞의 0 제거
                            if (/^0\d+\./.test(newPureValue)) {
                                newPureValue = newPureValue.substring(1);
                            }

                            // **마이너스 복원**
                            if (isNegative) {
                                newPureValue = '-' + newPureValue;
                            }

                            // 로컬 값 저장
                            localValuesRef.current['value'] = newPureValue;

                            // 상태 즉시 업데이트
                            onChange(newPureValue);

                            // 커서 위치 계산 (소수점 바로 뒤)
                            // newPureValue에서 마이너스를 제외한 순수 숫자 부분의 소수점 위치 찾기
                            const pureNumberPart = newPureValue.startsWith('-') ? newPureValue.substring(1) : newPureValue;
                            const dotIndexInNumber = pureNumberPart.indexOf('.');

                            // 전체 표시 값에서의 커서 위치 계산
                            let finalCursorPos = dotIndexInNumber + 1; // 소수점 바로 뒤

                            // **접두사/접미사 처리 - 마이너스 고려**
                            if (value.startsWith('-$')) {
                                // -$인 경우: -$ (2글자) + 숫자부분의 소수점 위치 + 1
                                finalCursorPos += 2;
                            } else if (value.startsWith('$') || value.startsWith('#')) {
                                // $인 경우: $ (1글자) + 숫자부분의 소수점 위치 + 1
                                finalCursorPos += 1;
                            } else if (value.startsWith('-') && suffix) {
                                // -123% 경우: - (1글자) + 숫자부분의 소수점 위치 + 1
                                finalCursorPos += 1;
                            }

                            // 커서 위치를 ref에 저장 (getInputRef에서 사용)
                            const cursorKey = `${input.id}_cursor`;
                            localValuesRef.current[cursorKey] = String(finalCursorPos);

                            return;
                        }
                    }

                    // -$ 형식 특수 처리
                    const isNegativeWithPrefix = value.startsWith('-$');

                    // Home 키 처리
                    if (e.key === 'Home') {
                        // 접미사 음수 처리가 우선 (-%, -x, -개)
                        if (value && value.startsWith('-') && /[%x회개]$/.test(value)) {
                            e.preventDefault();
                            // 항상 - 뒤로 이동
                            input.setSelectionRange(1, 1);
                            return;
                        } else if (isNegativeWithPrefix) {
                            e.preventDefault();
                            // -$ 다음 위치로 커서 이동
                            input.setSelectionRange(2, 2);
                            return;
                        } else if (value && (value.startsWith('$') || value.startsWith('#'))) {
                            e.preventDefault();
                            input.setSelectionRange(1, 1);
                            return;
                        }
                    }

                    // End 키 처리
                    if (e.key === 'End') {
                        // 접미사 음수 처리가 우선 (-%, -x, -개)
                        if (value && value.startsWith('-') && /[%x회개]$/.test(value)) {
                            e.preventDefault();
                            const suffixIndex = value.search(/[%x회개]$/);
                            if (suffixIndex !== -1) {
                                input.setSelectionRange(suffixIndex, suffixIndex);
                            }
                            return;
                        }
                        // 일반 접미사 처리 (%, x 등)
                        else if (value && /[^0-9.]$/.test(value)) {
                            const lastNumberIndex = value.search(/[^0-9.]+$/);
                            if (lastNumberIndex !== -1) {
                                e.preventDefault();
                                input.setSelectionRange(lastNumberIndex, lastNumberIndex);
                                return;
                            }
                        }
                    }

                    // 일반 방향키 처리 (커서 이동) - Shift가 있으면 제외 (선택 로직과 충돌 방지)
                    if ((e.key === 'ArrowLeft' || e.key === 'ArrowRight' || e.key === 'ArrowUp' || e.key === 'ArrowDown') && !e.shiftKey) {
                        // **쉼표 위치에서의 커서 이동 처리**
                        if (value && value.includes(',')) {
                            if (e.key === 'ArrowLeft') {
                                // 왼쪽 방향키로 쉼표 바로 뒤에서 이동하려는 경우, 쉼표 앞으로 건너뛰기
                                const charBeforeCursor = cursorPosition > 0 ? value[cursorPosition - 1] : '';
                                if (charBeforeCursor === ',') {
                                    e.preventDefault();
                                    input.setSelectionRange(cursorPosition - 1, cursorPosition - 1);
                                    return;
                                }
                            } else if (e.key === 'ArrowRight') {
                                // 오른쪽 방향키로 쉼표 바로 앞에서 이동하려는 경우, 쉼표 뒤로 건너뛰기
                                const charAtCursor = cursorPosition < value.length ? value[cursorPosition] : '';
                                if (charAtCursor === ',') {
                                    e.preventDefault();
                                    input.setSelectionRange(cursorPosition + 1, cursorPosition + 1);
                                    return;
                                }
                            }
                        }

                        // 선택 영역이 있을 때 방향키 처리
                        if (hasSelection) {
                            if (e.key === 'ArrowLeft') {
                                e.preventDefault();
                                input.setSelectionRange(input.selectionStart, input.selectionStart);
                                return;
                            }
                            if (e.key === 'ArrowRight') {
                                e.preventDefault();
                                input.setSelectionRange(input.selectionEnd, input.selectionEnd);
                                return;
                            }
                        }

                        // 접두사 처리 ($, #) 및 -$ 형식 처리
                        if (isNegativeWithPrefix) {
                            // 왼쪽 화살표 키로 -$ 앞으로 이동하려는 경우 방지
                            if (e.key === 'ArrowLeft' && cursorPosition <= 2) {
                                e.preventDefault();
                                input.setSelectionRange(2, 2);
                                return;
                            }

                            // 전체 선택 후 왼쪽 화살표 키를 누른 경우
                            if (e.key === 'ArrowLeft' && hasSelection) {
                                e.preventDefault();
                                input.setSelectionRange(2, 2);
                                return;
                            }

                            // 윗 방향키(Up Arrow) 처리
                            if (e.key === 'ArrowUp') {
                                e.preventDefault();
                                // -$ 다음으로 강제 이동
                                input.setSelectionRange(2, 2);
                                return;
                            }
                        }
                        // 접미사 음수 처리 (-%, -x, -개) - 커서 차단
                        else if (value && value.startsWith('-') && /[%x회개]$/.test(value)) {
                            // 왼쪽 화살표 키로 - 앞으로 이동하려는 경우 차단
                            if (e.key === 'ArrowLeft' && cursorPosition <= 1) {
                                e.preventDefault();
                                input.setSelectionRange(1, 1); // - 뒤로 강제 이동
                                return;
                            }

                            // 전체 선택 후 왼쪽 화살표 키를 누른 경우
                            if (e.key === 'ArrowLeft' && hasSelection) {
                                e.preventDefault();
                                input.setSelectionRange(1, 1); // - 뒤로 강제 이동
                                return;
                            }

                            // 위 방향키(Up Arrow) 처리
                            if (e.key === 'ArrowUp') {
                                e.preventDefault();
                                // - 뒤로 강제 이동
                                input.setSelectionRange(1, 1);
                                return;
                            }

                            // 아래 방향키(Down Arrow) 처리
                            if (e.key === 'ArrowDown') {
                                e.preventDefault();
                                const hasNumbers = /\d/.test(value);
                                if (hasNumbers) {
                                    // 숫자가 있으면 숫자와 접미사 사이로 이동
                                    const suffixIndex = value.search(/[%x회개]$/);
                                    if (suffixIndex !== -1) {
                                        input.setSelectionRange(suffixIndex, suffixIndex);
                                    }
                                } else {
                                    // 숫자가 없으면 - 뒤로 이동
                                    input.setSelectionRange(1, 1);
                                }
                                return;
                            }

                            // 오른쪽 방향키 처리
                            if (e.key === 'ArrowRight') {
                                const hasNumbers = /\d/.test(value);
                                if (hasNumbers) {
                                    // 숫자가 있으면 기본 동작 허용 (숫자 영역 내에서 이동)
                                } else {
                                    // 숫자가 없으면 반응 없음 (현재 위치 유지)
                                    e.preventDefault();
                                    return;
                                }
                            }
                        }


                        // **완전한 접미사 처리 - 모든 방향키에 대해 제한**
                        if (value && /[^0-9.]$/.test(value)) {
                            const lastNumberIndex = value.search(/[^0-9.]+$/);

                            if (lastNumberIndex !== -1) {
                                // **오른쪽 화살표 키로 접미사 영역으로 이동하려는 경우 방지**
                                if (e.key === 'ArrowRight' && cursorPosition >= lastNumberIndex) {
                                    e.preventDefault();
                                    input.setSelectionRange(lastNumberIndex, lastNumberIndex);
                                    return;
                                }

                                // **전체 선택 후 오른쪽 화살표 키를 누른 경우**
                                if (e.key === 'ArrowRight' && hasSelection) {
                                    e.preventDefault();
                                    input.setSelectionRange(lastNumberIndex, lastNumberIndex);
                                    return;
                                }

                                // **아래 화살표 키가 접미사 뒤로 이동하려는 경우 방지**
                                if (e.key === 'ArrowDown') {
                                    e.preventDefault();
                                    // 접미사 앞으로 강제 이동
                                    input.setSelectionRange(lastNumberIndex, lastNumberIndex);
                                    return;
                                }
                            }
                        }

                        // **일반 양수 접두사 처리 강화**
                        else if (value && (value.startsWith('$') || value.startsWith('#'))) {
                            // **왼쪽 화살표 키로 접두사 앞으로 이동하려는 경우 방지**
                            if (e.key === 'ArrowLeft' && cursorPosition <= 1) {
                                e.preventDefault();
                                input.setSelectionRange(1, 1);
                                return;
                            }

                            // **전체 선택 후 왼쪽 화살표 키를 누른 경우**
                            if (e.key === 'ArrowLeft' && hasSelection) {
                                e.preventDefault();
                                input.setSelectionRange(1, 1);
                                return;
                            }

                            // **위 방향키(Up Arrow) 처리**
                            if (e.key === 'ArrowUp') {
                                e.preventDefault();
                                // 접두사 뒤로 강제 이동
                                input.setSelectionRange(1, 1);
                                return;
                            }
                        }
                    }

                    // Backspace/Delete 키 처리
                    if (e.key === 'Backspace' || e.key === 'Delete') {
                        // **쉼표 스마트 삭제 로직**
                        if (value && value.includes(',') && selectionLength === 0) {
                            if (e.key === 'Backspace') {
                                // 쉼표 오른쪽에서 백스페이스 -> 쉼표 왼쪽 숫자 삭제
                                const charBeforeCursor = cursorPosition > 0 ? value[cursorPosition - 1] : '';
                                if (charBeforeCursor === ',') {
                                    e.preventDefault();

                                    // 쉼표 왼쪽 숫자 찾기
                                    let deletePos = cursorPosition - 2; // 쉼표 바로 앞
                                    while (deletePos >= 0 && !/\d/.test(value[deletePos])) {
                                        deletePos--;
                                    }

                                    if (deletePos >= 0 && /\d/.test(value[deletePos])) {
                                        // 숫자 삭제
                                        const newValue = value.substring(0, deletePos) + value.substring(deletePos + 1);

                                        // 순수 숫자 값 추출 (접두사/접미사 제거, 음수 부호 보존)
                                        let pureValue = newValue;
                                        let isNegative = false;

                                        // 음수 여부 확인 및 접두사 제거
                                        if (newValue.startsWith('-$') || newValue.startsWith('-#')) {
                                            isNegative = true;
                                            pureValue = newValue.substring(2);
                                        } else if (newValue.startsWith('$') || newValue.startsWith('#')) {
                                            pureValue = newValue.substring(1);
                                        } else if (newValue.startsWith('-') && /[%x회개]$/.test(newValue)) {
                                            isNegative = true;
                                            pureValue = newValue.substring(1);
                                        }

                                        // 접미사 제거
                                        pureValue = pureValue.replace(/[%x회개]$/, '');

                                        // 쉼표 제거하여 순수 숫자만 추출
                                        pureValue = pureValue.replace(/,/g, '');

                                        // 음수 부호 복원
                                        if (isNegative && pureValue && pureValue !== '0') {
                                            pureValue = '-' + pureValue;
                                        }

                                        // 상태 즉시 업데이트
                                        localValuesRef.current['value'] = pureValue;
                                        onChange(pureValue);

                                        // 커서 위치 조정 (삭제된 위치 기준)
                                        const cursorKey = `${input.id}_cursor`;
                                        localValuesRef.current[cursorKey] = String(deletePos);
                                    }
                                    return;
                                }
                            } else if (e.key === 'Delete') {
                                // 쉼표 왼쪽에서 delete -> 쉼표 오른쪽 숫자 삭제
                                const charAtCursor = cursorPosition < value.length ? value[cursorPosition] : '';
                                if (charAtCursor === ',') {
                                    e.preventDefault();

                                    // 쉼표 오른쪽 숫자 찾기
                                    let deletePos = cursorPosition + 1; // 쉼표 바로 뒤
                                    while (deletePos < value.length && !/\d/.test(value[deletePos])) {
                                        deletePos++;
                                    }

                                    if (deletePos < value.length && /\d/.test(value[deletePos])) {
                                        // 숫자 삭제
                                        const newValue = value.substring(0, deletePos) + value.substring(deletePos + 1);

                                        // 순수 숫자 값 추출 (접두사/접미사 제거, 음수 부호 보존)
                                        let pureValue = newValue;
                                        let isNegative = false;

                                        // 음수 여부 확인 및 접두사 제거
                                        if (newValue.startsWith('-$') || newValue.startsWith('-#')) {
                                            isNegative = true;
                                            pureValue = newValue.substring(2);
                                        } else if (newValue.startsWith('$') || newValue.startsWith('#')) {
                                            pureValue = newValue.substring(1);
                                        } else if (newValue.startsWith('-') && /[%x회개]$/.test(newValue)) {
                                            isNegative = true;
                                            pureValue = newValue.substring(1);
                                        }

                                        // 접미사 제거
                                        pureValue = pureValue.replace(/[%x회개]$/, '');

                                        // 쉼표 제거하여 순수 숫자만 추출
                                        pureValue = pureValue.replace(/,/g, '');

                                        // 음수 부호 복원
                                        if (isNegative && pureValue && pureValue !== '0') {
                                            pureValue = '-' + pureValue;
                                        }

                                        // 상태 즉시 업데이트
                                        localValuesRef.current['value'] = pureValue;
                                        onChange(pureValue);

                                        // 커서 위치 계산: 현재 커서 왼쪽에 있는 숫자의 개수를 센다
                                        let digitCountBeforeCursor = 0;
                                        for (let i = 0; i < cursorPosition; i++) {
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

                        // **선택 영역에 쉼표가 포함된 경우 삭제 방지**
                        if (selectionLength > 0 && value && value.includes(',')) {
                            const selectedText = value.substring(cursorPosition, cursorPosition + selectionLength);
                            // 선택된 텍스트가 쉼표로만 이루어져 있는지 확인
                            if (selectedText.replace(/,/g, '').length === 0) {
                                // 쉼표만 선택된 경우 삭제 방지
                                e.preventDefault();
                                return;
                            }
                            // 숫자와 쉼표가 함께 선택된 경우, 기본 동작(삭제)을 허용하면
                            // onValueChange가 트리거되어 자동으로 쉼표가 재배치됩니다.
                        }

                        // 백스페이스만 처리 (Delete는 기본 동작)
                        if (e.key === 'Backspace' && selectionLength === 0) {

                            // 접두사 필드 처리 ($, #)
                            if (value.startsWith('$') || value.startsWith('#')) {
                                const prefix = value.charAt(0);
                                const hasNumbers = /\d/.test(value);

                                // 숫자가 있을 때: $|123 -> 반응 없음
                                if (hasNumbers && cursorPosition === 1) {
                                    e.preventDefault();
                                    return;
                                }

                                // 값이 없을 때: $| -> $ 삭제 (필드 완전히 비우기)
                                if (!hasNumbers && cursorPosition === 1 && value === prefix) {
                                    e.preventDefault();

                                    // 필드 비우기
                                    input.value = '';

                                    // 변경 이벤트를 발생시켜 onValueChange -> onChange를 통해 상태를 업데이트한다.
                                    const event = new Event('input', {bubbles: true});
                                    Object.defineProperty(event, 'target', {writable: false, value: {value: ''}});
                                    input.dispatchEvent(event);

                                    return;
                                }
                            }

                            // 음수 접두사 필드 처리 (-$, -#)
                            else if (value.startsWith('-$') || value.startsWith('-#')) {
                                const hasNumbers = /\d/.test(value);

                                // 숫자가 있을 때: -$|123 -> -만 삭제
                                if (hasNumbers && cursorPosition === 2) {
                                    e.preventDefault();

                                    const formattedValue = value.replace(/^-[$#]/, ''); // ex: "1,234"
                                    const unformattedValue = formattedValue.replace(/,/g, ''); // ex: "1234"

                                    onChange(unformattedValue);

                                    // 커서 위치 복원 예약 ($ 뒤)
                                    const cursorKey = `${input.id}_cursor`;
                                    localValuesRef.current[cursorKey] = '1';
                                    return;
                                }

                                // 값이 없을 때: -$| -> 필드 완전히 비우기
                                if (!hasNumbers && cursorPosition === 2 && (value === '-$' || value === '-#')) {
                                    e.preventDefault();

                                    // 필드 비우기
                                    input.value = '';

                                    // **핵심 수정: activeFocusRef 초기화하여 NumericFormat 상태 문제 해결**
                                    if (input.id && activeFocusRef.current === input.id) {
                                        activeFocusRef.current = null;
                                    }

                                    // 변경 이벤트를 발생시켜 onValueChange -> onChange를 통해 상태를 업데이트한다.
                                    const event = new Event('input', {bubbles: true});
                                    Object.defineProperty(event, 'target', {writable: false, value: {value: ''}});
                                    input.dispatchEvent(event);

                                    // **추가: 필드 비우기 후 강제로 포커스 재설정 및 NumericFormat 상태 복구**
                                    setTimeout(() => {
                                        if (input === document.activeElement) {
                                            if (input.id) {
                                                activeFocusRef.current = input.id;
                                                localValuesRef.current[input.id] = 'focused';

                                                // **핵심: NumericFormat 상태 강제 복구**
                                                // input에 focus를 다시 주어 NumericFormat이 제대로 초기화되도록 함
                                                input.blur();
                                                input.focus();
                                            }
                                        }
                                    }, 10); // 약간의 딜레이를 주어 DOM 업데이트 완료 후 실행

                                    return;
                                }
                            }

                            // 접미사 필드 처리 (%, x, 개)
                            else if (/[%x회개]$/.test(value)) {
                                const suffix = value.charAt(value.length - 1);
                                const hasNumbers = /\d/.test(value);

                                // 음수 접미사 처리
                                if (value.startsWith('-')) {
                                    // 숫자가 있을 때: |-123% -> 반응 없음
                                    if (hasNumbers && cursorPosition === 0) {
                                        e.preventDefault();
                                        return;
                                    }

                                    // 값이 없을 때: -|% -> 필드 완전히 비우기
                                    if (!hasNumbers && cursorPosition === 1 && value === `-${suffix}`) {
                                        e.preventDefault();

                                        // 필드 완전히 비우기
                                        input.value = '';

                                        // **핵심 수정: activeFocusRef 초기화하여 NumericFormat 상태 문제 해결**
                                        if (input.id && activeFocusRef.current === input.id) {
                                            activeFocusRef.current = null;
                                        }

                                        // 변경 이벤트 발생
                                        const event = new Event('input', {bubbles: true});
                                        Object.defineProperty(event, 'target', {writable: false, value: {value: ''}});
                                        input.dispatchEvent(event);

                                        // **추가: 필드 비우기 후 강제로 포커스 재설정 및 NumericFormat 상태 복구**
                                        setTimeout(() => {
                                            if (input === document.activeElement) {
                                                if (input.id) {
                                                    activeFocusRef.current = input.id;
                                                    localValuesRef.current[input.id] = 'focused';

                                                    // **핵심: NumericFormat 상태 강제 복구**
                                                    // input에 focus를 다시 주어 NumericFormat이 제대로 초기화되도록 함
                                                    input.blur();
                                                    input.focus();
                                                }
                                            }
                                        }, 10); // 약간의 딜레이를 주어 DOM 업데이트 완료 후 실행

                                        return;
                                    }

                                    // 값이 없을 때: |-% -> 반응 없음
                                    if (!hasNumbers && cursorPosition === 0 && value === suffix) {
                                        e.preventDefault();
                                        return;
                                    }
                                }
                                // 양수 접미사 처리
                                else {
                                    // 숫자가 있을 때: |123% -> 반응 없음
                                    if (hasNumbers && cursorPosition === 0) {
                                        e.preventDefault();
                                        return;
                                    }

                                    // 값이 없을 때: |% -> 반응 없음
                                    if (!hasNumbers && cursorPosition === 0 && value === suffix) {
                                        e.preventDefault();
                                        return;
                                    }
                                }
                            } else {
                                // 접두사/접미사가 없는 일반 필드에서 커서가 맨 앞에 있을 때 Backspace 방지
                                if (cursorPosition === 0) {
                                    e.preventDefault();
                                    return;
                                }
                            }
                        }
                    }

                    // Ctrl+방향키 조합에 대한 특별 처리 (단어 단위 이동 제어)
                    if ((e.ctrlKey || e.metaKey) && (e.key === 'ArrowLeft' || e.key === 'ArrowRight')) {
                        // 접미사 처리
                        if (value && /[%x회개]$/.test(value)) {
                            e.preventDefault();
                            const lastNumberIndex = value.search(/[^0-9.]+$/);

                            if (e.key === 'ArrowRight') {
                                // Ctrl+Right -> End 키처럼 동작
                                if (lastNumberIndex !== -1) {
                                    input.setSelectionRange(lastNumberIndex, lastNumberIndex);
                                }
                            } else { // Ctrl+Left
                                // Home 키처럼 동작
                                let homePosition = 0;
                                if (value.startsWith('-')) { // 음수 접미사 (-123%)
                                    homePosition = 1; // - 뒤로 이동
                                }
                                input.setSelectionRange(homePosition, homePosition);
                            }
                            return;
                        }

                        // 접두사 처리
                        if (value && (value.startsWith('$') || value.startsWith('#') || value.startsWith('-$') || value.startsWith('-#'))) {
                            e.preventDefault();

                            if (e.key === 'ArrowLeft') {
                                // Ctrl+Left -> Home 키처럼 동작 (접두사 뒤로)
                                let homePosition = 0;
                                if (value.startsWith('-$') || value.startsWith('-#')) {
                                    homePosition = 2;
                                } else if (value.startsWith('$') || value.startsWith('#')) {
                                    homePosition = 1;
                                }
                                input.setSelectionRange(homePosition, homePosition);
                            } else { // Ctrl+Right
                                // End 키처럼 동작 (값의 끝으로)
                                input.setSelectionRange(value.length, value.length);
                            }
                            return;
                        }
                    }

                    // **최종 안전장치 - 모든 커서 이동 후 허용 영역 검증**
                    if ((e.key === 'ArrowLeft' || e.key === 'ArrowRight' || e.key === 'ArrowUp' || e.key === 'ArrowDown' ||
                        e.key === 'Home' || e.key === 'End') && !e.shiftKey) {

                        // 키 처리 후 커서 위치 검증 및 조정
                        setTimeout(() => {
                            if (input === document.activeElement && value && (value.includes('$') || value.includes('#') || value.includes('%') || value.includes('x') || value.includes('개'))) {
                                const currentPosition = input.selectionStart || 0;

                                // 허용된 영역 계산
                                let allowedStart = 0;
                                let allowedEnd = value.length;

                                // 접두사 처리
                                if (value.startsWith('-$') || value.startsWith('-#')) {
                                    allowedStart = 2;
                                } else if (value.startsWith('$') || value.startsWith('#')) {
                                    allowedStart = 1;
                                } else if (value.startsWith('-') && /[%x회개]$/.test(value)) {
                                    allowedStart = 1; // 음수 접미사는 - 뒤부터 허용
                                }

                                // 접미사 처리
                                if (value.endsWith('%') || value.endsWith('x') || value.endsWith('회') || value.endsWith('개')) {
                                    allowedEnd = value.length - 1;
                                }

                                // **허용 영역을 벗어난 경우 강제 조정**
                                if (currentPosition < allowedStart || currentPosition > allowedEnd) {
                                    let targetPosition = currentPosition;

                                    if (currentPosition < allowedStart) {
                                        targetPosition = allowedStart;
                                    } else if (currentPosition > allowedEnd) {
                                        targetPosition = allowedEnd;
                                    }

                                    input.setSelectionRange(targetPosition, targetPosition);
                                }
                            }
                        }, 0);
                    }

                    // 기존 onKeyDown 이벤트 호출
                    if (props.onKeyDown) props.onKeyDown(e);
                };

                // input 이벤트를 위한 핸들러 추가
                const handleInput = (e: React.FormEvent<HTMLInputElement>) => {
                    // ID를 포커스된 요소로 저장
                    const input = e.currentTarget;
                    const value = input.value;

                    if (input.id) {
                        activeFocusRef.current = input.id;
                        localValuesRef.current[input.id] = 'focused';
                    }

                    // 숫자가 없는 접두사/접미사만 있는 경우 필드 비우기 (단일 - 제외)
                    // 중요: 이전에 숫자가 있었던 경우에만 지우기 적용
                    const shouldClearField = (value === '-$' || value === '$-' || value === '-%' || value === '%' ||
                            (value.includes('-') && (value.includes('$') || value.includes('%')) && !/\d/.test(value))) &&
                        value !== '-';

                    if (shouldClearField) {
                        // 이전 값 확인 (입력 기록이 있는지)
                        const prevValue = input.getAttribute('data-prev-value') || '';
                        const hadDigitsBefore = /\d/.test(prevValue);

                        // 이전에 숫자가 있었던 경우에만 필드 비우기 적용
                        if (hadDigitsBefore) {
                            // 값 비우기
                            input.value = '';

                            // 변경 이벤트 발생
                            const event = new Event('input', {bubbles: true});
                            input.dispatchEvent(event);

                            // 임시 저장소 업데이트
                            localValuesRef.current['value'] = '';

                            // 상태 즉시 업데이트
                            onChange('');
                        }
                    }

                    // 기존 onInput 이벤트 호출
                    if (props.onInput) {
                        props.onInput(e as any);
                    }
                };

                // 드래그 영역 제한을 위한 선택 영역 조정 핸들러
                const adjustSelection = (input: HTMLInputElement) => {
                    if (!input || !input.value) return;

                    const value = input.value;
                    const selectionStart = input.selectionStart || 0;
                    const selectionEnd = input.selectionEnd || 0;

                    if (!value) return;

                    let newStart = selectionStart;
                    let newEnd = selectionEnd;
                    let needsAdjustment = false;

                    // 양수 접두사 ($123) -> 123만 선택 가능
                    if (value.startsWith('$') && !value.startsWith('-$')) {
                        if (newStart < 1) {
                            newStart = 1;
                            needsAdjustment = true;
                        }
                        if (newEnd < 1) {
                            newEnd = 1;
                            needsAdjustment = true;
                        }
                    }
                    // 음수 접두사 (-$123) -> 123만 선택 가능
                    else if (value.startsWith('-$')) {
                        if (newStart < 2) {
                            newStart = 2;
                            needsAdjustment = true;
                        }
                        if (newEnd < 2) {
                            newEnd = 2;
                            needsAdjustment = true;
                        }
                    }
                    // 양수 접두사 (#123) -> 123만 선택 가능
                    else if (value.startsWith('#') && !value.startsWith('-#')) {
                        if (newStart < 1) {
                            newStart = 1;
                            needsAdjustment = true;
                        }
                        if (newEnd < 1) {
                            newEnd = 1;
                            needsAdjustment = true;
                        }
                    }
                    // 음수 접두사 (-#123) -> 123만 선택 가능
                    else if (value.startsWith('-#')) {
                        if (newStart < 2) {
                            newStart = 2;
                            needsAdjustment = true;
                        }
                        if (newEnd < 2) {
                            newEnd = 2;
                            needsAdjustment = true;
                        }
                    }

                    // 접미사 처리
                    const suffixPattern = /[%x회개]$/;
                    const suffixMatch = value.match(suffixPattern);

                    if (suffixMatch) {
                        const suffixIndex = value.lastIndexOf(suffixMatch[0]);

                        // 음수 접미사 (-123%) -> 123만 선택 가능 (- 제외)
                        if (value.startsWith('-')) {
                            // 선택 시작이 1보다 작으면 1로 (- 뒤부터)
                            if (newStart < 1) {
                                newStart = 1;
                                needsAdjustment = true;
                            }
                            // 선택 끝이 접미사 위치보다 크면 접미사 앞으로
                            if (newEnd > suffixIndex) {
                                newEnd = suffixIndex;
                                needsAdjustment = true;
                            }
                        }
                        // 양수 접미사 (123%) -> 123만 선택 가능
                        else {
                            // 선택 시작이 0보다 작으면 0으로
                            if (newStart < 0) {
                                newStart = 0;
                                needsAdjustment = true;
                            }
                            // 선택 끝이 접미사 위치보다 크면 접미사 앞으로
                            if (newEnd > suffixIndex) {
                                newEnd = suffixIndex;
                                needsAdjustment = true;
                            }
                        }
                    }

                    // 조정이 필요한 경우 실시간으로 선택 영역 변경
                    if (needsAdjustment) {
                        // 즉시 선택 영역 조정
                        input.setSelectionRange(newStart, newEnd);
                    }
                };

                const handleSelect = (e: React.SyntheticEvent<HTMLInputElement>) => {
                    const input = e.currentTarget;

                    // 기존 조정 로직 (제한 영역이 없는 경우)
                    adjustSelection(input);
                };

                const handleMouseUp = (e: React.MouseEvent<HTMLInputElement>) => {
                    const input = e.currentTarget;

                    // **드래그 정보 정리**
                    if ((input as any).__dragStartInfo) {
                        delete (input as any).__dragStartInfo;
                    }

                    // 기존 onMouseUp 이벤트 호출
                    if (props.onMouseUp) props.onMouseUp(e);
                };

                const handleMouseLeave = (e: React.MouseEvent<HTMLInputElement>) => {
                    // 기존 onMouseLeave 이벤트 호출
                    if (props.onMouseLeave) props.onMouseLeave(e);
                };

                const handleMouseDown = (e: React.MouseEvent<HTMLInputElement>) => {
                    // 기존 onMouseDown 이벤트 호출
                    if (props.onMouseDown) props.onMouseDown(e);
                };


                const handleMouseMove = (e: React.MouseEvent<HTMLInputElement>) => {
                    // 기존 onMouseMove 이벤트 호출
                    if (props.onMouseMove) props.onMouseMove(e);
                };

                const handleDoubleClick = (e: React.MouseEvent<HTMLInputElement>) => {
                    const input = e.currentTarget;
                    const value = input.value;

                    // 제한 영역이 있는 경우에만 더블 클릭 처리
                    if (value && (value.includes('$') || value.includes('#') || value.includes('%') || value.includes('x') || value.includes('회') || value.includes('개'))) {
                        e.preventDefault();
                        e.stopPropagation();

                        // 허용된 영역 계산
                        let allowedStart = 0;
                        let allowedEnd = value.length;

                        if (value.startsWith('-$') || value.startsWith('-#')) {
                            allowedStart = 2;
                        } else if (value.startsWith('$') || value.startsWith('#')) {
                            allowedStart = 1;
                        } else if (value.startsWith('-') && /[%x개회]$/.test(value)) {
                            allowedStart = 1;
                        }

                        if (value.endsWith('%') || value.endsWith('x') || value.endsWith('회') || value.endsWith('개')) {
                            allowedEnd = value.length - 1;
                        }

                        // 허용된 영역만 선택
                        input.setSelectionRange(allowedStart, allowedEnd);
                    }

                    if (props.onDoubleClick) props.onDoubleClick(e);
                };

                // 커스텀 입력 컴포넌트에서 전달한 속성 제외하고 나머지만 input에 전달
                const {onCustomBlur, ...inputProps} = props;

                return (
                    <input
                        {...inputProps}
                        ref={handleRef}
                        onClick={handleClick}
                        onFocus={handleFocus}
                        onBlur={handleBlur}
                        onKeyDown={handleKeyDown}
                        onInput={handleInput}
                        onSelect={handleSelect}
                        onMouseDown={handleMouseDown}
                        onMouseMove={handleMouseMove}
                        onMouseUp={handleMouseUp}
                        onMouseLeave={handleMouseLeave}
                        onDoubleClick={handleDoubleClick}
                        autoComplete="off"
                    />
                );
            }
        );
    }, [unit, allowedChars, allowNegative, isIntegerOnly, prefix, suffix]);

    const NumericFormatAny: any = NumericFormat;

    return (
        <NumericFormatAny
            id={id}
            name={name}
            placeholder={placeholder}
            value={value}
            prefix={prefix}
            suffix={suffix}
            allowNegative={allowNegative}
            allowLeadingZeros={true}
            allowedDecimalSeparators={isIntegerOnly ? [] : ['.']}
            valueIsNumericString={true}
            thousandSeparator=','
            format={(numStr: string) => {
                if (!numStr) return '';

                // 소수점 처리
                if (numStr === '.') return `${prefix}0.${suffix}`;
                if (numStr.startsWith('.')) return `${prefix}0${numStr}${suffix}`;

                // 음수 처리
                if (numStr === '-') return '-';
                if (numStr === '-.') {
                    if (prefix) return `-${prefix}0.${suffix}`;
                    else if (suffix) return `-0.${suffix}`;
                    else return '-0.';
                }
                if (numStr.startsWith('-.')) {
                    if (prefix) return `-${prefix}0${numStr.substring(1)}${suffix}`;
                    else if (suffix) return `-0${numStr.substring(1)}${suffix}`;
                    else return `-0${numStr.substring(1)}`;
                }

                // 0으로 시작하는 경우 처리
                if (numStr.startsWith('0') && numStr.length > 1) {
                    // 0. 형태는 그대로 유지
                    if (numStr[1] === '.') {
                        return `${prefix}${numStr}${suffix}`;
                    }
                    // 0 뒤에 숫자가 오는 경우 0 제거 (01, 02, 03, 01.23 등)
                    if (/^0\d/.test(numStr)) {
                        numStr = numStr.substring(1);
                    }
                }

                // 일반 숫자 포맷 - 음수 처리
                if (numStr.startsWith('-')) {
                    const positiveNum = numStr.substring(1);
                    if (prefix) return `-${prefix}${positiveNum}${suffix}`;
                    else if (suffix) return `-${positiveNum}${suffix}`;
                    else return numStr;
                }

                return `${prefix}${numStr}${suffix}`;
            }}
            onValueChange={handleNumericFormatChange}
            className={className}
            customInput={CustomInput}
            onCustomBlur={handleBlurEvent}
            decimalScale={isIntegerOnly ? 0 : 20}
            fixedDecimalScale={false}
        />
    );
};

export default NumericInput;
