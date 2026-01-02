import React, {useEffect} from "react";
import {toggleOption, useTradeFilter} from "./index.ts";
import {RESET_STRATEGY_NAME_FILTER} from "./FilterResetEvent";

const StrategyFilter: React.FC = () => {
    const {filter, setFilter, options} = useTradeFilter();

    // 전략 필터 초기화 함수
    const resetFilter = () => {
        setFilter(prev => ({
            ...prev,
            strategies: options.strategies.map(option => option.name)
        }));
    };

    // 외부에서 resetFilter를 호출할 수 있도록 이벤트 리스너 등록
    useEffect(() => {
        const handleResetEvent = () => {
            resetFilter();
        };

        // 이벤트 리스너 등록
        document.addEventListener(RESET_STRATEGY_NAME_FILTER, handleResetEvent);

        // 클린업 함수
        return () => {
            document.removeEventListener(RESET_STRATEGY_NAME_FILTER, handleResetEvent);
        };
    }, [resetFilter]);

    // 헤더 체크박스 상태 계산
    const totalOptions = options.strategies.length;
    const checkedOptions = options.strategies.filter(option => filter.strategies.includes(option.name)).length;
    const isAllChecked = totalOptions > 0 && checkedOptions === totalOptions;
    const isIndeterminate = checkedOptions > 0 && checkedOptions < totalOptions;

    // 전체 선택/해제 핸들러
    const handleSelectAll = (checked: boolean) => {
        if (checked) {
            // 모든 옵션 선택
            setFilter(prev => ({
                ...prev,
                strategies: options.strategies.map(option => option.name)
            }));
        } else {
            // 모든 옵션 해제
            setFilter(prev => ({
                ...prev,
                strategies: []
            }));
        }
    };

    // 컨테이너 클릭 핸들러 - 체크박스 상태 토글
    const handleContainerClick = (optionName: string, currentChecked: boolean, e: React.MouseEvent) => {
        // 체크박스 또는 라벨을 직접 클릭한 경우는 무시 (이벤트 중복 방지)
        if (
            (e.target as HTMLElement).tagName === 'INPUT' ||
            (e.target as HTMLElement).tagName === 'LABEL'
        ) {
            return;
        }

        // 체크박스 상태 토글
        toggleOption("strategies", optionName, !currentChecked, setFilter);
    };

    // 마스터 체크박스 컨테이너 클릭 핸들러
    const handleHeaderContainerClick = (e: React.MouseEvent) => {
        // 체크박스 또는 라벨을 직접 클릭한 경우는 무시 (이벤트 중복 방지)
        if (
            (e.target as HTMLElement).tagName === 'INPUT' ||
            (e.target as HTMLElement).tagName === 'LABEL'
        ) {
            return;
        }

        // 현재 체크 상태 토글
        handleSelectAll(!isAllChecked);
    };

    // 개별 체크박스 변경 핸들러
    const handleCheckboxChange = (optionName: string, checked: boolean) => {
        toggleOption("strategies", optionName, checked, setFilter);
    };

    return (
        <div>
            <div
                className="filter-header-with-checkbox"
                onClick={handleHeaderContainerClick}
            >
                <input
                    id="StrategyFilter-selectAll"
                    name="StrategyFilter-selectAll"
                    type="checkbox"
                    checked={isAllChecked}
                    ref={(el) => {
                        if (el) el.indeterminate = isIndeterminate;
                    }}
                    onChange={e => handleSelectAll(e.target.checked)}
                    className="custom-checkbox header-checkbox"
                />
                <label htmlFor="StrategyFilter-selectAll" className="filter-section-title">
                    전략 이름
                </label>
            </div>
            {options.strategies.map(option => (
                <div
                    key={option.name}
                    className="checkbox-container"
                    onClick={(e) => handleContainerClick(option.name, filter.strategies.includes(option.name), e)}
                >
                    <input
                        id={`StrategyFilter-${option.name}`}
                        name={`StrategyFilter-${option.name}`}
                        type="checkbox"
                        checked={filter.strategies.includes(option.name)}
                        onChange={e => handleCheckboxChange(option.name, e.target.checked)}
                        className="custom-checkbox"
                    />
                    <label htmlFor={`StrategyFilter-${option.name}`} className="checkbox-label">{option.name}</label>
                </div>
            ))}
        </div>
    );
};

export default StrategyFilter;
