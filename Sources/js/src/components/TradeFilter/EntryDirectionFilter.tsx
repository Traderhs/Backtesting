import React, {useEffect} from "react";
import {toggleOption, useTradeFilter} from "../TradeFilter";
import {RESET_ENTRY_DIRECTION_FILTER} from "./FilterResetEvent";

const EntryDirectionFilter: React.FC = () => {
    const {filter, setFilter, allTrades} = useTradeFilter();

    // 거래가 없는 경우 (거래 번호 0번만 있는 경우) 필터를 렌더링하지 않음
    if (allTrades.length <= 1) {
        return null;
    }

    // 진입 방향 옵션들
    const directions = ["매수", "매도"];

    // 진입 방향 필터 초기화 함수
    const resetFilter = () => {
        setFilter(prev => ({
            ...prev,
            entryDirections: [...directions]
        }));
    };

    // 외부에서 resetFilter를 호출할 수 있도록 이벤트 리스너 등록
    useEffect(() => {
        const handleResetEvent = () => {
            resetFilter();
        };

        // 이벤트 리스너 등록
        document.addEventListener(RESET_ENTRY_DIRECTION_FILTER, handleResetEvent);

        // 클린업 함수
        return () => {
            document.removeEventListener(RESET_ENTRY_DIRECTION_FILTER, handleResetEvent);
        };
    }, [resetFilter]);

    // 헤더 체크박스 상태 계산
    const totalOptions = directions.length;
    const checkedOptions = directions.filter(direction => filter.entryDirections.includes(direction)).length;
    const isAllChecked = totalOptions > 0 && checkedOptions === totalOptions;
    const isIndeterminate = checkedOptions > 0 && checkedOptions < totalOptions;

    // 전체 선택/해제 핸들러
    const handleSelectAll = (checked: boolean) => {
        if (checked) {
            // 모든 옵션 선택
            setFilter(prev => ({
                ...prev,
                entryDirections: [...directions]
            }));
        } else {
            // 모든 옵션 해제
            setFilter(prev => ({
                ...prev,
                entryDirections: []
            }));
        }
    };

    // 컨테이너 클릭 핸들러 - 체크박스 상태 토글
    const handleContainerClick = (direction: string, currentChecked: boolean, e: React.MouseEvent) => {
        // 체크박스 또는 라벨을 직접 클릭한 경우는 무시 (이벤트 중복 방지)
        if (
            (e.target as HTMLElement).tagName === 'INPUT' ||
            (e.target as HTMLElement).tagName === 'LABEL'
        ) {
            return;
        }

        // 체크박스 상태 토글
        toggleOption("entryDirections", direction, !currentChecked, setFilter);
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
                    id="EntryDirectionFilter-selectAll"
                    name="EntryDirectionFilter-selectAll"
                    type="checkbox"
                    checked={isAllChecked}
                    ref={(el) => {
                        if (el) el.indeterminate = isIndeterminate;
                    }}
                    onChange={e => handleSelectAll(e.target.checked)}
                    className="custom-checkbox header-checkbox"
                />
                <label htmlFor="EntryDirectionFilter-selectAll" className="filter-section-title">
                    진입 방향
                </label>
            </div>
            {directions.map(direction => (
                <div
                    key={direction}
                    className="checkbox-container"
                    onClick={(e) => handleContainerClick(direction, filter.entryDirections.includes(direction), e)}
                >
                    <input
                        id={`EntryDirectionFilter-${direction}`}
                        name={`EntryDirectionFilter-${direction}`}
                        type="checkbox"
                        checked={filter.entryDirections.includes(direction)}
                        onChange={e => {
                            toggleOption("entryDirections", direction, e.target.checked, setFilter);
                        }}
                        className="custom-checkbox"
                    />
                    <label htmlFor={`EntryDirectionFilter-${direction}`} className="checkbox-label">{direction}</label>
                </div>
            ))}
        </div>
    );
};

export default EntryDirectionFilter;
