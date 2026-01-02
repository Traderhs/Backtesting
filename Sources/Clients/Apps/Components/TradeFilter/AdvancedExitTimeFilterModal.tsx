import React, {useCallback, useEffect, useMemo, useRef, useState} from "react";
import {Button} from "@/Components/UI/Button.tsx";
import {TradeItem} from "./TradeFilterContext";
import TimeFilterCheckboxes from "./TimeFilterCheckboxes";
import './Modal.css'
import './AdvancedFilterModal.css'
import './MasterCheckboxHover.css'
import {
    getDayOptions,
    getHourOptions,
    getMinuteSecondOptions,
    getMonthOptions,
    getYearOptions,
} from "./TimeFilterOptions";
import {useTradeFilter} from "./index.ts";
import {createPortal} from 'react-dom';

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
    const {filter, setFilter} = useTradeFilter();
    const [isDragging, setIsDragging] = useState(false);
    const [startDragPos, setStartDragPos] = useState({x: 0, y: 0, initialLeft: 0, initialTop: 0});
    const [isInitialized, setIsInitialized] = useState(false);

    const modalRef = useRef<HTMLDivElement>(null);
    const dragRef = useRef<HTMLDivElement>(null);
    const resizeTimeoutRef = useRef<NodeJS.Timeout | null>(null);
    const initTimeoutRef = useRef<NodeJS.Timeout | null>(null);

    // 최신 값들을 참조하기 위한 ref
    const latestValues = useRef(values);
    const latestFilter = useRef(filter);

    // 옵션들을 메모화하여 성능 최적화
    const timeOptions = useMemo(() => ({
        yearOptions: getYearOptions(tradeData, ["진입 시간", "청산 시간"]),
        monthOptions: getMonthOptions(),
        dayOptions: getDayOptions(),
        hourOptions: getHourOptions(),
        minuteSecondOptions: getMinuteSecondOptions(),
    }), [tradeData]);

    // 최신 값들 업데이트
    useEffect(() => {
        latestValues.current = values;
        latestFilter.current = filter;
    });

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
            setIsInitialized(false);
        }
    }, [isOpen, filter.exitYears, filter.exitMonths, filter.exitDays, filter.exitDayOfWeeks, filter.exitHours, filter.exitMinutes, filter.exitSeconds, setValues]);

    // 섹션 높이 계산 최적화 - requestAnimationFrame 사용
    const calculateSectionHeights = useCallback(() => {
        if (!modalRef.current) return;

        requestAnimationFrame(() => {
            const sections = modalRef.current?.querySelectorAll('.advanced-filter-section');
            if (!sections || sections.length === 0) return;

            let maxHeight = 0;
            sections.forEach(section => {
                const sectionHeight = section.scrollHeight;
                maxHeight = Math.max(maxHeight, sectionHeight);
            });

            sections.forEach(section => {
                (section as HTMLElement).style.setProperty('--max-section-height', `${maxHeight}px`);
            });

            // 콘텐츠 영역의 최소 높이 설정
            const contentElement = modalRef.current?.querySelector('.advanced-filter-content');
            if (contentElement) {
                (contentElement as HTMLElement).style.maxHeight = 'calc(70vh - 120px)';
                (contentElement as HTMLElement).style.overflowY = 'auto';
            }
        });
    }, []);

    // 모달이 열릴 때 초기화 - 지연 로딩으로 성능 개선
    useEffect(() => {
        if (isOpen && modalRef.current && !isInitialized) {
            // 최소 지연으로 렌더링 차단 방지
            initTimeoutRef.current = setTimeout(() => {
                calculateSectionHeights();
                setIsInitialized(true);
            }, 50);

            return () => {
                if (initTimeoutRef.current) {
                    clearTimeout(initTimeoutRef.current);
                }
            };
        }
    }, [isOpen, calculateSectionHeights, isInitialized]);

    // 디바운스된 리사이즈 핸들러
    const debouncedResizeHandler = useCallback(() => {
        if (!isOpen) return;

        if (resizeTimeoutRef.current) {
            clearTimeout(resizeTimeoutRef.current);
        }

        resizeTimeoutRef.current = setTimeout(() => {
            calculateSectionHeights();
        }, 150);
    }, [isOpen, calculateSectionHeights]);

    // 윈도우 리사이즈 시 디바운스 적용
    useEffect(() => {
        if (!isOpen) return;

        window.addEventListener('resize', debouncedResizeHandler);
        return () => {
            window.removeEventListener('resize', debouncedResizeHandler);
            if (resizeTimeoutRef.current) {
                clearTimeout(resizeTimeoutRef.current);
            }
        };
    }, [isOpen, debouncedResizeHandler]);

    // 모달 중앙 정렬 최적화
    const centerModal = useCallback(() => {
        if (!modalRef.current || !isOpen) return;

        requestAnimationFrame(() => {
            if (!modalRef.current) return;

            const sidebarWidthRem = 18;
            const sidebarWidthPx = sidebarWidthRem * 16;

            const windowWidth = window.innerWidth;
            const windowHeight = window.innerHeight;

            const mainContentWidth = windowWidth - sidebarWidthPx;

            const modalWidth = modalRef.current.offsetWidth;
            const modalHeight = modalRef.current.offsetHeight;

            const centerLeft = Math.max(0, (mainContentWidth - modalWidth) / 2);
            const centerTop = Math.max(0, (windowHeight - modalHeight) / 2);

            modalRef.current.style.position = 'absolute';
            modalRef.current.style.left = `${centerLeft}px`;
            modalRef.current.style.top = `${centerTop}px`;
        });
    }, [isOpen]);

    // 윈도우 리사이즈 시 모달 위치 중앙으로 재조정
    useEffect(() => {
        if (!isOpen) return;

        window.addEventListener('resize', centerModal);
        centerModal(); // 초기 중앙 정렬

        return () => {
            window.removeEventListener('resize', centerModal);
        };
    }, [isOpen, centerModal]);

    // 안정적인 함수들 생성
    const handleApply = useCallback(() => {
        setFilter(prev => ({
            ...prev,
            exitYears: latestValues.current.exitYears || [],
            exitMonths: latestValues.current.exitMonths || [],
            exitDays: latestValues.current.exitDays || [],
            exitDayOfWeeks: latestValues.current.exitDayOfWeeks || [],
            exitHours: latestValues.current.exitHours || [],
            exitMinutes: latestValues.current.exitMinutes || [],
            exitSeconds: latestValues.current.exitSeconds || [],
        }));
        onClose();
    }, [setFilter, onClose]);

    const handleCancel = useCallback(() => {
        setValues({
            exitYears: latestFilter.current.exitYears,
            exitMonths: latestFilter.current.exitMonths,
            exitDays: latestFilter.current.exitDays,
            exitDayOfWeeks: latestFilter.current.exitDayOfWeeks,
            exitHours: latestFilter.current.exitHours,
            exitMinutes: latestFilter.current.exitMinutes,
            exitSeconds: latestFilter.current.exitSeconds,
        });
        onClose();
    }, [setValues, onClose]);

    // ESC 키 및 Enter 키 처리
    useEffect(() => {
        const handleKeyDown = (event: KeyboardEvent) => {
            if (event.key === 'Escape') {
                handleCancel();
            } else if (event.key === 'Enter') {
                handleApply();
            }
        };

        if (isOpen) {
            document.addEventListener('keydown', handleKeyDown);
        }
        return () => {
            document.removeEventListener('keydown', handleKeyDown);
        };
    }, [isOpen, handleCancel, handleApply]);

    // 모달 외부 클릭 감지
    const isMouseDownOutside = useRef(false);
    useEffect(() => {
        const handleMouseDownOutside = (event: MouseEvent) => {
            isMouseDownOutside.current = modalRef.current ? !modalRef.current.contains(event.target as Node) : false;
        };

        const handleMouseUpOutside = (event: MouseEvent) => {
            if (isMouseDownOutside.current && modalRef.current && !modalRef.current.contains(event.target as Node)) {
                handleCancel();
            }
            isMouseDownOutside.current = false; // 상태 초기화
        };

        if (isOpen) {
            document.addEventListener('mousedown', handleMouseDownOutside);
            document.addEventListener('mouseup', handleMouseUpOutside);
        }
        return () => {
            document.removeEventListener('mousedown', handleMouseDownOutside);
            document.removeEventListener('mouseup', handleMouseUpOutside);
        };
    }, [isOpen, handleCancel]);

    // 드래그 기능 구현
    useEffect(() => {
        const handleMouseMove = (e: MouseEvent) => {
            if (isDragging) {
                const deltaX = e.clientX - startDragPos.x;
                const deltaY = e.clientY - startDragPos.y;

                if (modalRef.current) {
                    const sidebarWidthPx = 18 * 16; // 18rem = 288px
                    const windowWidth = window.innerWidth;
                    const windowHeight = window.innerHeight;
                    const mainContentWidth = windowWidth - sidebarWidthPx;
                    const modalWidth = modalRef.current.offsetWidth;
                    const modalHeight = modalRef.current.offsetHeight;

                    let newLeft = startDragPos.initialLeft + deltaX;
                    let newTop = startDragPos.initialTop + deltaY;

                    newLeft = Math.max(0, Math.min(newLeft, mainContentWidth - modalWidth));
                    newTop = Math.max(0, Math.min(newTop, windowHeight - modalHeight));

                    modalRef.current.style.position = 'absolute';
                    modalRef.current.style.left = `${newLeft}px`;
                    modalRef.current.style.top = `${newTop}px`;
                }
            }
        };

        const handleMouseUp = () => {
            setIsDragging(false);
            if (dragRef.current) {
                dragRef.current.style.cursor = 'grab';
            }
        };

        if (isDragging) {
            document.addEventListener('mousemove', handleMouseMove);
            document.addEventListener('mouseup', handleMouseUp);
            if (dragRef.current) {
                dragRef.current.style.cursor = 'grabbing';
            }
        }

        return () => {
            document.removeEventListener('mousemove', handleMouseMove);
            document.removeEventListener('mouseup', handleMouseUp);
        };
    }, [isDragging, startDragPos]);

    // 조건부 리턴문을 모든 훅 호출 이후로 이동
    if (!isOpen) return null;
    if (tradeData.length === 0) {
        console.error("tradeData가 비어있습니다.");
        return null;
    }

    // 옵션 추출
    const yearOptions = timeOptions.yearOptions;
    if (yearOptions.length === 0) {
        console.error("유효한 연도 데이터가 없습니다.");
        return null;
    }
    const monthOptions = timeOptions.monthOptions;
    const dayOptions = timeOptions.dayOptions;
    const hourOptions = timeOptions.hourOptions;
    const minuteSecondOptions = timeOptions.minuteSecondOptions;

    // 요일 이름 배열 변경 - 월요일부터 시작하도록
    const dayOfWeekLabels = ['월요일', '화요일', '수요일', '목요일', '금요일', '토요일', '일요일'];

    // 요일 옵션 재정렬 (0->월요일, 1->화요일, ..., 6->일요일)
    const reorderedDayOfWeekOptions = () => {
        return [1, 2, 3, 4, 5, 6, 0]; // 월, 화, 수, 목, 금, 토, 일 순서로 변경
    };

    const onCheckboxChange = (
        key: keyof AdvancedExitTimeFilterValues,
        option: number,
        checked: boolean
    ) => {
        const currentValues = values[key] || [];
        const newValues = checked
            ? [...currentValues, option]
            : currentValues.filter(v => v !== option);
        setValues({...values, [key]: newValues});
    };

    // 드래그 시작 핸들러
    const handleMouseDown = (e: React.MouseEvent) => {
        const target = e.target as HTMLElement;
        if (target.closest('button') || target.closest('.checkbox-container') || target.closest('input')) {
            return;
        }

        if (dragRef.current) {
            setIsDragging(true);
            setStartDragPos({
                x: e.clientX,
                y: e.clientY,
                initialLeft: modalRef.current?.offsetLeft || 0,
                initialTop: modalRef.current?.offsetTop || 0
            });
        }
    };

    // X 버튼 클릭 핸들러
    const handleCloseButtonClick = (e: React.MouseEvent) => {
        e.stopPropagation();
        handleCancel();
    };

    // 섹션 렌더링 함수 - 헤더 제거, 버튼 왼쪽 배치
    const renderTimeSection = (
        label: string,
        options: number[],
        valueKey: keyof AdvancedExitTimeFilterValues
    ) => {
        const currentValues = values[valueKey] || [];
        const isAllSelected = currentValues.length === options.length;
        const isIndeterminate = currentValues.length > 0 && currentValues.length < options.length;

        const handleCheckboxClick = (e: React.MouseEvent) => {
            e.stopPropagation(); // 이벤트 버블링 방지

            if (isAllSelected) {
                setValues({...values, [valueKey]: []});
            } else {
                setValues({...values, [valueKey]: options});
            }
        };

        return (
            <div className="advanced-filter-section">
                <div
                    className="filter-header-with-checkbox"
                    onClick={handleCheckboxClick}
                >
                    <input
                        type="checkbox"
                        className="custom-checkbox header-checkbox"
                        checked={isAllSelected}
                        ref={(input) => {
                            if (input) {
                                input.indeterminate = isIndeterminate;
                            }
                        }}
                        onChange={(e) => {
                            if (e.target.checked) {
                                setValues({...values, [valueKey]: options});
                            } else {
                                setValues({...values, [valueKey]: []});
                            }
                        }}
                        onClick={(e) => e.stopPropagation()} // 체크박스 자체 클릭 이벤트는 상위로 전파되지 않도록
                    />
                    <span className="filter-section-title">{label}</span>
                </div>
                <TimeFilterCheckboxes
                    label={label}
                    options={options}
                    selectedValues={currentValues}
                    onChange={(option, checked) => onCheckboxChange(valueKey, option, checked)}
                />
            </div>
        );
    };

    // 요일 섹션 수정
    const renderDayOfWeekSection = () => {
        const options = reorderedDayOfWeekOptions();
        const currentValues = values.exitDayOfWeeks || [];
        const isAllSelected = currentValues.length === options.length;
        const isIndeterminate = currentValues.length > 0 && currentValues.length < options.length;

        const handleDayOfWeekCheckboxClick = (e: React.MouseEvent) => {
            e.stopPropagation(); // 이벤트 버블링 방지

            if (isAllSelected) {
                setValues({...values, exitDayOfWeeks: []});
            } else {
                setValues({...values, exitDayOfWeeks: options});
            }
        };

        return (
            <div className="advanced-filter-section">
                <div
                    className="filter-header-with-checkbox"
                    onClick={handleDayOfWeekCheckboxClick}
                >
                    <input
                        type="checkbox"
                        className="custom-checkbox header-checkbox"
                        checked={isAllSelected}
                        ref={(input) => {
                            if (input) {
                                input.indeterminate = isIndeterminate;
                            }
                        }}
                        onChange={(e) => {
                            if (e.target.checked) {
                                setValues({...values, exitDayOfWeeks: options});
                            } else {
                                setValues({...values, exitDayOfWeeks: []});
                            }
                        }}
                        onClick={(e) => e.stopPropagation()} // 체크박스 자체 클릭 이벤트는 상위로 전파되지 않도록
                    />
                    <span className="filter-section-title">요일</span>
                </div>
                <TimeFilterCheckboxes
                    label="요일"
                    options={options}
                    selectedValues={currentValues}
                    onChange={(option, checked) => onCheckboxChange("exitDayOfWeeks", option, checked)}
                    dayOfWeekLabels={dayOfWeekLabels}
                />
            </div>
        );
    };

    const modalElement = (
        <div className="filter-calendar-overlay">
            <div
                ref={modalRef}
                className="advanced-filter-modal-container"
            >
                <div
                    ref={dragRef}
                    className="advanced-filter-header"
                    style={{cursor: isDragging ? 'grabbing' : 'grab'}}
                    onMouseDown={handleMouseDown}
                >
                    <div className="advanced-filter-title-row">
                        <h2 className="advanced-filter-title">[청산 시간] 고급 필터</h2>
                        <button
                            className="advanced-close-button"
                            onClick={handleCloseButtonClick}
                        >×
                        </button>
                    </div>
                </div>

                <div className="advanced-filter-content">
                    {/* 연도 */}
                    {renderTimeSection("연도", yearOptions, "exitYears")}

                    {/* 월 */}
                    {renderTimeSection("월", monthOptions, "exitMonths")}

                    {/* 일 */}
                    {renderTimeSection("일", dayOptions, "exitDays")}

                    {/* 요일 - 커스텀 렌더링 함수 사용 */}
                    {renderDayOfWeekSection()}

                    {/* 시 */}
                    {renderTimeSection("시간", hourOptions, "exitHours")}

                    {/* 분 */}
                    {renderTimeSection("분", minuteSecondOptions, "exitMinutes")}

                    {/* 초 */}
                    {renderTimeSection("초", minuteSecondOptions, "exitSeconds")}
                </div>

                <div className="advanced-filter-footer">
                    <Button
                        onClick={handleApply}
                        className="advanced-filter-footer-button apply"
                    >
                        적용
                    </Button>
                    <Button
                        onClick={handleCancel}
                        className="advanced-filter-footer-button cancel"
                    >
                        취소
                    </Button>
                </div>
            </div>
        </div>
    );

    return createPortal(modalElement, document.body);
};

export default AdvancedExitTimeFilterModal;
