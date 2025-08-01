import React, { useCallback, useMemo, useEffect, useRef, useState } from "react";
import './TimeFilterVertical.css'; // 새로운 CSS 스타일 임포트

interface TimeFilterCheckboxesProps {
    label: string;
    options: number[];
    selectedValues?: number[];
    onChange: (option: number, checked: boolean) => void;
    dayOfWeekLabels?: string[]; // 요일 표시를 위한 추가 prop
}

// 개별 체크박스 컴포넌트 - 지연 렌더링 적용
const CheckboxItem: React.FC<{
    option: number;
    label: string;
    isSelected: boolean;
    onChange: (option: number, checked: boolean) => void;
    getLabel: (option: number) => string;
}> = React.memo(({ option, label, isSelected, onChange, getLabel }) => {
    const [isVisible, setIsVisible] = useState(false);
    const itemRef = useRef<HTMLDivElement>(null);
    
    useEffect(() => {
        const observer = new IntersectionObserver(
            (entries) => {
                if (entries[0].isIntersecting) {
                    setIsVisible(true);
                    observer.disconnect();
                }
            },
            { rootMargin: '100px' } // 뷰포트 진입 100px 전에 렌더링
        );
        
        if (itemRef.current) {
            observer.observe(itemRef.current);
        }
        
        return () => observer.disconnect();
    }, []);
    
    const handleContainerClick = useCallback((e: React.MouseEvent) => {
        if ((e.target as HTMLElement).tagName === 'INPUT' || 
            (e.target as HTMLElement).tagName === 'LABEL') {
            return;
        }
        onChange(option, !isSelected);
    }, [option, isSelected, onChange]);
    
    const handleCheckboxClick = useCallback((e: React.MouseEvent) => {
        e.stopPropagation();
    }, []);
    
    const handleCheckboxChange = useCallback((e: React.ChangeEvent<HTMLInputElement>) => {
        onChange(option, e.target.checked);
    }, [option, onChange]);
    
    return (
        <div 
            ref={itemRef}
            className="checkbox-container m-1 py-2 px-3 advanced-filter-checkbox vertical-checkbox"
            onClick={handleContainerClick}
        >
            {isVisible ? (
                <>
                    <input 
                        id={`TimeFilterCheckboxes-${label}-${option}`}
                        name={`TimeFilterCheckboxes-${label}-${option}`}
                        type="checkbox"
                        checked={isSelected}
                        onChange={handleCheckboxChange}
                        className="custom-checkbox"
                        onClick={handleCheckboxClick}
                    />
                    <label 
                        htmlFor={`TimeFilterCheckboxes-${label}-${option}`} 
                        className="checkbox-label ml-2"
                    >
                        {getLabel(option)}
                    </label>
                </>
            ) : null}
        </div>
    );
});

CheckboxItem.displayName = 'CheckboxItem';

const TimeFilterCheckboxes: React.FC<TimeFilterCheckboxesProps> = React.memo(({
    label,
    options,
    selectedValues = [],
    onChange,
    dayOfWeekLabels,
}) => {
    // 라벨 생성 로직을 메모화
    const getLabel = useCallback((option: number): string => {
        if (dayOfWeekLabels && label.includes("요일")) {
            // 요일 옵션이 0(일요일)~6(토요일)이지만,
            // dayOfWeekLabels는 0(월요일)~6(일요일) 순서이므로 매핑 필요
            const mappedIndex = option === 0 ? 6 : option - 1;
            return dayOfWeekLabels[mappedIndex];
        }
        
        // 각 숫자 뒤에 단위 붙이기
        if (label.includes("연도")) return `${option}년`;
        if (label.includes("월")) return `${option}월`;
        if (label.includes("일")) return `${option}일`;
        if (label.includes("시")) return `${option}시`;
        if (label.includes("분")) return `${option}분`;
        if (label.includes("초")) return `${option}초`;
        
        return String(option);
    }, [label, dayOfWeekLabels]);

    // 선택된 값들을 Set으로 변환하여 조회 성능 최적화
    const selectedValuesSet = useMemo(() => new Set(selectedValues), [selectedValues]);

    // 옵션 렌더링을 메모화
    const renderedOptions = useMemo(() => 
        options.map((option) => (
            <CheckboxItem
                key={option}
                option={option}
                label={label}
                isSelected={selectedValuesSet.has(option)}
                onChange={onChange}
                getLabel={getLabel}
            />
        )), [options, selectedValuesSet, label, onChange, getLabel]);

    return (
        <div className="time-filter-vertical-container">
            <div className="time-filter-column">
                {renderedOptions}
            </div>
        </div>
    );
});

TimeFilterCheckboxes.displayName = 'TimeFilterCheckboxes';

export default TimeFilterCheckboxes;
