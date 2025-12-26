import React, {useEffect} from "react";
import {toggleOption, useTradeFilter} from "../TradeFilter";
import {RESET_SYMBOL_NAME_FILTER} from "./FilterResetEvent";

const SymbolFilter: React.FC = () => {
    const {filter, setFilter, options} = useTradeFilter();

    // 심볼 필터 초기화 함수
    const resetFilter = () => {
        setFilter(prev => ({
            ...prev,
            symbols: options.symbols.map(option => option.name)
        }));
    };

    // 외부에서 resetFilter를 호출할 수 있도록 이벤트 리스너 등록
    useEffect(() => {
        const handleResetEvent = () => {
            resetFilter();
        };

        // 이벤트 리스너 등록
        document.addEventListener(RESET_SYMBOL_NAME_FILTER, handleResetEvent);

        // 클린업 함수
        return () => {
            document.removeEventListener(RESET_SYMBOL_NAME_FILTER, handleResetEvent);
        };
    }, [resetFilter]);

    // 헤더 체크박스 상태 계산
    const totalOptions = options.symbols.length;
    const checkedOptions = options.symbols.filter(option => filter.symbols.includes(option.name)).length;
    const isAllChecked = totalOptions > 0 && checkedOptions === totalOptions;
    const isIndeterminate = checkedOptions > 0 && checkedOptions < totalOptions;

    // 전체 선택/해제 핸들러
    const handleSelectAll = (checked: boolean) => {
        if (checked) {
            // 모든 옵션 선택
            setFilter(prev => ({
                ...prev,
                symbols: options.symbols.map(option => option.name)
            }));
        } else {
            // 모든 옵션 해제
            setFilter(prev => ({
                ...prev,
                symbols: []
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
        toggleOption("symbols", optionName, !currentChecked, setFilter);
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

    return (
        <div>
            <div
                className="filter-header-with-checkbox"
                onClick={handleHeaderContainerClick}
            >
                <input
                    id="SymbolFilter-selectAll"
                    name="SymbolFilter-selectAll"
                    type="checkbox"
                    checked={isAllChecked}
                    ref={(el) => {
                        if (el) el.indeterminate = isIndeterminate;
                    }}
                    onChange={e => handleSelectAll(e.target.checked)}
                    className="custom-checkbox header-checkbox"
                />
                <label htmlFor="SymbolFilter-selectAll" className="filter-section-title">
                    심볼 이름
                </label>
            </div>
            {options.symbols.map(option => (
                <div
                    key={option.name}
                    className="checkbox-container"
                    onClick={(e) => handleContainerClick(option.name, filter.symbols.includes(option.name), e)}
                >
                    <input
                        id={`SymbolFilter-${option.name}`}
                        name={`SymbolFilter-${option.name}`}
                        type="checkbox"
                        checked={filter.symbols.includes(option.name)}
                        onChange={e => {
                            toggleOption("symbols", option.name, e.target.checked, setFilter);
                        }}
                        className="custom-checkbox"
                    />
                    <label htmlFor={`SymbolFilter-${option.name}`} className="checkbox-label">{option.name}</label>
                </div>
            ))}
        </div>
    );
};

export default SymbolFilter;
