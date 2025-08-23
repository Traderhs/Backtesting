import React, {useCallback, useEffect, useMemo, useRef, useState} from "react"
import {useTradeFilter} from "@/components/TradeFilter/useTradeFilter.ts";
import {motion} from "framer-motion";
import LoadingSpinner from "@/components/Common/LoadingSpinner";
import NoDataMessage from "@/components/Common/NoDataMessage";
import "./TradeList.css";
import {formatDateTimeWithWeekday} from "@/components/Performance/Utils";

interface TradeItem {
    "거래 번호": number

    [key: string]: string | number
}

// 컴포넌트 props 인터페이스 추가
interface TradeListProps {
    config: any;
}

// 상수들을 모듈 레벨로 이동하여 재생성 방지
const DOLLAR_FIELDS = new Set([
    "진입 수수료",
    "청산 수수료",
    "강제 청산 수수료",
    "펀딩비 수령",
    "펀딩비 지불",
    "펀딩비",
    "손익",
    "순손익",
    "현재 자금",
    "최고 자금",
    "누적 손익",
]);

const PERCENT_FIELDS = new Set([
    "개별 순손익률",
    "전체 순손익률",
    "드로우다운",
    "최고 드로우다운",
    "누적 손익률",
]);

const PRICE_FIELDS = new Set([
    "진입 가격",
    "청산 가격",
    "강제 청산 가격"
]);

const QUANTITY_FIELDS = new Set([
    "진입 수량",
    "청산 수량"
]);

const COMMA_FIELDS = new Set([
    "거래 번호",
    "진입 가격",
    "진입 수량",
    "청산 가격",
    "청산 수량",
    "강제 청산 가격",
    "펀딩 수령 횟수",
    "펀딩비 수령",
    "펀딩 지불 횟수",
    "펀딩비 지불",
    "펀딩 횟수",
    "펀딩비",
    "진입 수수료",
    "청산 수수료",
    "강제 청산 수수료",
    "손익",
    "순손익",
    "개별 순손익률",
    "전체 순손익률",
    "현재 자금",
    "최고 자금",
    "누적 손익",
    "누적 손익률",
    "드로우다운",
    "최고 드로우다운",
    "레버리지"
]);

// 경계선 스타일을 위한 Set - 컴포넌트 밖에서 정의하여 재생성 방지
const THICK_BORDER_COLUMNS = new Set([
    "거래 번호", "심볼 이름", "진입 방향", "보유 시간", "레버리지", "진입 수량",
    "청산 수량", "강제 청산 가격", "펀딩비 수령", "펀딩비 지불", "강제 청산 수수료",
    "순손익", "전체 순손익률", "최고 자금", "최고 드로우다운", "누적 손익률"
]);

// 열 그룹 정의 - 점선으로 연결된 열들을 하나의 그룹으로 묶음
const COLUMN_GROUPS = [
    ["거래 번호"],
    ["전략 이름", "심볼 이름"],
    ["진입 이름", "청산 이름", "진입 방향"],
    ["진입 시간", "청산 시간", "보유 시간"],
    ["레버리지"],
    ["진입 가격", "진입 수량"],
    ["청산 가격", "청산 수량"],
    ["강제 청산 가격"],
    ["펀딩 수령 횟수", "펀딩비 수령"],
    ["펀딩 지불 횟수", "펀딩비 지불"],
    ["펀딩 횟수", "펀딩비", "진입 수수료", "청산 수수료", "강제 청산 수수료"],
    ["손익", "순손익"],
    ["개별 순손익률", "전체 순손익률"],
    ["현재 자금", "최고 자금"],
    ["드로우다운", "최고 드로우다운"],
    ["누적 손익", "누적 손익률"]
];

// 열이 속한 그룹을 찾는 함수
const getColumnGroup = (columnKey: string): string[] => {
    for (const group of COLUMN_GROUPS) {
        if (group.includes(columnKey)) {
            return group;
        }
    }
    return [columnKey]; // 그룹에 속하지 않으면 자기 자신만 포함
};

// 천단위 쉼표 포맷 함수 - 숫자 타입 체크 최적화
const formatWithCommas = (value: string | number, precision: number = 0): string => {
    if (value === undefined || value === null || String(value) === "-") return "-";

    const num = typeof value === "number" ? value : parseFloat(String(value));
    if (isNaN(num)) return String(value);

    // 소수점 처리 - 더 효율적인 방법
    const fixedValue = precision > 0 ? num.toFixed(precision) : Math.round(num).toString();

    // 정수부와 소수부 분리
    const parts = fixedValue.split('.');
    const integerPart = parts[0];
    const decimalPart = parts[1];

    // 정수부에 천단위 쉼표 추가
    const formattedInteger = integerPart.replace(/\B(?=(\d{3})+(?!\d))/g, ',');

    // 소수부가 있으면 합치기
    return decimalPart ? `${formattedInteger}.${decimalPart}` : formattedInteger;
}

// 심볼별 precision 정보를 가져오는 함수 - 캐싱 추가
const precisionCache = new Map<string, { pricePrecision: number, qtyPrecision: number }>();

const getSymbolPrecision = (config: any, symbol: string): { pricePrecision: number, qtyPrecision: number } => {
    if (precisionCache.has(symbol)) {
        return precisionCache.get(symbol)!;
    }

    if (!config || !config["심볼"]) {
        const defaultPrecision = {pricePrecision: 2, qtyPrecision: 3};
        precisionCache.set(symbol, defaultPrecision);
        return defaultPrecision;
    }

    const symbolInfo = config["심볼"].find((s: any) => s["심볼 이름"] === symbol);

    if (!symbolInfo || !symbolInfo["거래소 정보"]) {
        const defaultPrecision = {pricePrecision: 2, qtyPrecision: 3};
        precisionCache.set(symbol, defaultPrecision);
        return defaultPrecision;
    }

    const exchangeInfo = symbolInfo["거래소 정보"];

    // 가격 precision: "소수점 정밀도" 필드 사용
    const pricePrecision = exchangeInfo["소수점 정밀도"] || 2;

    // 수량 precision: "수량 최소 단위"에서 소수점 자릿수 계산
    const qtyStep = exchangeInfo["수량 최소 단위"] || 0.001;
    const qtyPrecision = qtyStep.toString().includes('.') ?
        qtyStep.toString().split('.')[1].length : 0;

    const result = {
        pricePrecision: pricePrecision,
        qtyPrecision: qtyPrecision
    };

    precisionCache.set(symbol, result);
    return result;
}

// 거래 데이터에서 심볼 이름을 추출하는 함수
const getSymbolFromTrade = (trade: TradeItem): string => {
    return String(trade["심볼 이름"] || "");
}

// 너비 측정을 위한 헬퍼 함수 - formatWithTooltip과 동일한 로직 사용
const getDisplayStringForMeasuring = (value: string | number | undefined, key: string, config?: any, symbol?: string): string => {
    if (value === undefined || value === null) return "";
    if (key === 'originalIdxForSort') return ""; // 이 키는 표시되지 않음

    // 거래 번호
    if (key === "거래 번호") {
        return `#${formatWithCommas(value, 0)}`;
    }
    // 보유 심볼 수
    if (key === "보유 심볼 수") {
        if (String(value) === "-") {
            return "-";
        }
        return `${value}개`;
    }
    // 펀딩 횟수
    if (key === "펀딩 수령 횟수" || key === "펀딩 지불 횟수" || key === "펀딩 횟수") {
        if (String(value) === "-") {
            return "-";
        }
        return `${formatWithCommas(value, 0)}회`;
    }
    // 진입 시간과 청산 시간 처리
    if (key === "진입 시간" || key === "청산 시간") {
        if (!value || String(value) === "-") {
            return "-";
        }
        const date = new Date(String(value));
        return formatDateTimeWithWeekday(date);
    }

    // 가격 필드들은 심볼별 precision 사용 + 천단위 쉼표
    if (PRICE_FIELDS.has(key)) {
        if (!value || String(value) === "-") return "-";
        const num = typeof value === "number" ? value : parseFloat(String(value));
        if (isNaN(num)) return String(value);

        const precision = getSymbolPrecision(config, symbol || "");
        return formatWithCommas(num, precision.pricePrecision);
    }

    // 수량 필드들은 심볼별 precision 사용 + 천단위 쉼표
    if (QUANTITY_FIELDS.has(key)) {
        if (!value || String(value) === "-") return "-";
        const num = typeof value === "number" ? value : parseFloat(String(value));
        if (isNaN(num)) return String(value);

        const precision = getSymbolPrecision(config, symbol || "");
        return formatWithCommas(num, precision.qtyPrecision);
    }

    const num = typeof value === "number" ? value : parseFloat(String(value));
    if (isNaN(num)) return String(value);

    // formatWithTooltip과 동일한 방식으로 처리
    const short = num.toFixed(2);

    if (DOLLAR_FIELDS.has(key)) {
        const formattedValue = formatWithCommas(Math.abs(Number(short)), 2);
        return num < 0 ? `-$${formattedValue}` : `$${formattedValue}`;
    } else if (PERCENT_FIELDS.has(key)) {
        return `${formatWithCommas(num, 2)}%`;
    } else if (key === "레버리지") {
        return `${Math.round(num)}x`;
    } else if (COMMA_FIELDS.has(key)) {
        // 천단위 쉼표가 필요한 다른 필드들
        return formatWithCommas(num, 2);
    } else {
        return String(value); // 소수점 자르지 않고 원래 값 표시
    }
}

// 포맷 함수 메모이제이션을 위한 캐시 - 단순한 Map 사용으로 성능 개선
const formatCache = new Map<string, React.ReactElement>();
const MAX_CACHE_SIZE = 10000; // 캐시 크기 제한

const formatWithTooltip = (value: string | number, key: string, config?: any, symbol?: string): React.ReactElement => {
    // 캐시 키 생성 - 모든 매개변수를 포함하여 고유성 보장
    const cacheKey = `${key}:${value}:${symbol}:${config?.심볼?.length || 0}`;

    if (formatCache.has(cacheKey)) {
        return formatCache.get(cacheKey)!;
    }

    let result: React.ReactElement;

    // 거래 번호인 경우 특별 처리하기
    if (key === "거래 번호") {
        const display = `#${formatWithCommas(value, 0)}`
        result = <span title={display}>{display}</span>
    }
    // 보유 심볼 수 처리 ('개' 추가)
    else if (key === "보유 심볼 수") {
        if (String(value) === "-") {
            result = <span title="-">-</span>;
        } else {
            result = <span title={`${value}개`}>{`${value}개`}</span>
        }
    }
    // 펀딩 횟수 처리 ('회' 추가)
    else if (key === "펀딩 수령 횟수" || key === "펀딩 지불 횟수" || key === "펀딩 횟수") {
        if (String(value) === "-") {
            result = <span title="-">-</span>;
        } else {
            const display = `${formatWithCommas(value, 0)}회`;
            result = <span title={display}>{display}</span>
        }
    }
    // 진입 시간과 청산 시간 처리
    else if (key === "진입 시간" || key === "청산 시간") {
        if (!value || String(value) === "-") {
            result = <span title="-">-</span>;
        } else {
            const date = new Date(String(value));
            const formattedDate = formatDateTimeWithWeekday(date);
            result = <span title={formattedDate}
                           style={{display: 'flex', justifyContent: 'center', width: '100%'}}>{formattedDate}</span>;
        }
    }
    // 가격 필드들은 심볼별 precision 사용 + 천단위 쉼표
    else if (PRICE_FIELDS.has(key)) {
        if (value === undefined || value === null || String(value) === "-") {
            result = <span title="-">-</span>;
        } else {
            const num = typeof value === "number" ? value : parseFloat(String(value));
            if (isNaN(num)) {
                result = <span title={String(value)}>{String(value)}</span>;
            } else {
                const precision = getSymbolPrecision(config, symbol || "");
                const display = formatWithCommas(num, precision.pricePrecision);
                const tooltip = formatWithCommas(num, precision.pricePrecision); // 툴팁에도 precision 적용 (정해진 소수점 유지)
                result = <span title={tooltip}>{display}</span>;
            }
        }
    }
    // 수량 필드들은 심볼별 precision 사용 + 천단위 쉼표
    else if (QUANTITY_FIELDS.has(key)) {
        if (value === undefined || value === null || String(value) === "-") {
            result = <span title="-">-</span>;
        } else {
            const num = typeof value === "number" ? value : parseFloat(String(value));
            if (isNaN(num)) {
                result = <span title={String(value)}>{String(value)}</span>;
            } else {
                const precision = getSymbolPrecision(config, symbol || "");
                const display = formatWithCommas(num, precision.qtyPrecision);
                const tooltip = formatWithCommas(num, precision.qtyPrecision); // 툴팁에도 precision 적용 (정해진 소수점 유지)
                result = <span title={tooltip}>{display}</span>;
            }
        }
    } else {
        const num = typeof value === "number" ? value : parseFloat(String(value))
        if (isNaN(num)) {
            result = <span title={String(value)}>{String(value)}</span> // 숫자가 아닌 경우 title 추가
        } else {
            const short = num.toFixed(2)
            const roundedFull = parseFloat(Number(num.toPrecision(15)).toFixed(10)).toString()

            let display: string
            if (DOLLAR_FIELDS.has(key)) {
                const formattedValue = formatWithCommas(Math.abs(Number(short)), 2);
                display = num < 0 ? `-$${formattedValue}` : `$${formattedValue}`
            } else if (PERCENT_FIELDS.has(key)) {
                display = `${formatWithCommas(num, 2)}%`
            } else if (key === "레버리지") {
                display = `${Math.round(num)}x`
            } else if (COMMA_FIELDS.has(key)) {
                // 천단위 쉼표가 필요한 다른 필드들
                display = formatWithCommas(num, 2);
            } else { // 다른 숫자 형식은 기본값 사용하기
                display = String(value); // 소수점 자르지 않고 원래 값 표시하기
            }

            let tooltip: string
            if (DOLLAR_FIELDS.has(key)) {
                tooltip = num < 0 ? `-$${Math.abs(num)}` : `$${num}`
            } else if (PERCENT_FIELDS.has(key)) {
                tooltip = `${num}%`
            } else if (key === "레버리지") {
                tooltip = `${num}x`
            } else if (COMMA_FIELDS.has(key)) {
                tooltip = String(num);
            } else {
                tooltip = String(value);
            }

            // 툴팁과 표시값이 다르고, 원래 값과 툴팁이 다를 때만 툴팁 표시하기
            if (key === "레버리지") {
                result = <span title={`${roundedFull}x`}>{display}</span>
            } else {
                result = display !== tooltip && String(value) !== tooltip
                    ? <span title={tooltip}>{display}</span>
                    : <span title={String(value)}>{display}</span> // 기본적으로 원래 값을 title로 가지도록 하기
            }
        }
    }

    // 캐시에 저장 - 크기 제한으로 메모리 사용량 관리
    if (formatCache.size < MAX_CACHE_SIZE) {
        formatCache.set(cacheKey, result);
    } else if (formatCache.size >= MAX_CACHE_SIZE) {
        // 캐시 크기 제한 도달 시 일부 제거
        const keysToDelete = Array.from(formatCache.keys()).slice(0, Math.floor(MAX_CACHE_SIZE * 0.1));
        keysToDelete.forEach(key => formatCache.delete(key));
        formatCache.set(cacheKey, result);
    }

    return result;
}

// 행 컴포넌트를 분리하여 React.memo로 최적화
interface TradeRowProps {
    row: TradeItem;
    originalIndex: number;
    allHeaders: string[];
    columnWidths: { [key: string]: string };
    rowHeight: number;
    isActiveRow: boolean;
    hoverTradeNo: number | null;
    hoverColumn: string | null;
    hoverColumnGroup: string[]; // 호버된 열 그룹
    selectedColumnGroup: string[]; // 선택된 열 그룹
    initialBalance: number;
    config: any;
    isDifferentFromNext: boolean;
    isLastRowOfDataset: boolean;
    selectedCell: { rowIndex: number, columnKey: string, tradeNo: number } | null;
    onMouseEnter: (tradeNo: number) => void;
    onMouseLeave: () => void;
    onCellClick: (rowIndex: number, columnKey: string, tradeNo: number) => void;
    onCellHover: (columnKey: string | null) => void;
}

const TradeRow = React.memo<TradeRowProps>(({
                                                row,
                                                originalIndex,
                                                allHeaders,
                                                columnWidths,
                                                rowHeight,
                                                isActiveRow,
                                                hoverTradeNo,
                                                hoverColumn,
                                                hoverColumnGroup,
                                                selectedColumnGroup,
                                                initialBalance,
                                                config,
                                                isDifferentFromNext,
                                                isLastRowOfDataset,
                                                selectedCell,
                                                onMouseEnter,
                                                onMouseLeave,
                                                onCellClick,
                                                onCellHover
                                            }) => {
    const currentTradeNo = row["거래 번호"];
    const isHovered = hoverTradeNo === currentTradeNo;
    const symbol = getSymbolFromTrade(row);

    let rowClass = "tr";
    if (isLastRowOfDataset) rowClass += " last-row";
    if (!isDifferentFromNext && !isLastRowOfDataset) rowClass += " same-trade";
    else if (isDifferentFromNext || isLastRowOfDataset) rowClass += " last-group-row";

    if (isActiveRow) rowClass += " active-row";

    // 선택된 셀과 같은 거래번호인지 확인
    const isSelectedTradeRow = selectedCell && selectedCell.tradeNo === currentTradeNo;
    if (isSelectedTradeRow) rowClass += " selected-trade-row";

    // 셀 클릭 핸들러
    const handleCellClick = useCallback((columnKey: string) => {
        onCellClick(originalIndex, columnKey, currentTradeNo);
    }, [originalIndex, currentTradeNo, onCellClick]);

    // 각 셀을 미리 계산하여 메모이제이션 최적화
    const cellContents = useMemo(() => {
        return allHeaders.map((key) => {
            const value = Number(row[key]);
            let textColor = "";

            if (row["거래 번호"] === 0 &&
                (key === "손익" ||
                    key === "순손익" ||
                    key === "개별 순손익률" ||
                    key === "전체 순손익률")) {
                textColor = "white";
            } else if (key === "펀딩비 수령" || key === "펀딩비 지불" || key === "펀딩비") {
                if (row[key] === "-" || row[key] === undefined || row[key] === null) {
                    textColor = "";
                } else if (value === 0) {
                    textColor = "white";
                } else {
                    textColor = value >= 0 ? "#4caf50" : "#f23645";
                }
            } else if (key === "순손익" || key === "개별 순손익률" || key === "전체 순손익률" ||
                key === "손익" || key === "누적 손익" || key === "누적 손익률" || key === "펀딩비 수령" || key === "펀딩비 지불" || key === "펀딩비") {
                textColor = value >= 0 ? "#4caf50" : "#f23645";
            } else if (key === "현재 자금") {
                textColor = value >= initialBalance ? "#4caf50" : "#f23645";
            } else if (key === "드로우다운") {
                textColor = value === 0 ? "#4caf50" : "#f23645";
            } else if (key === "최고 자금") {
                textColor = "#008000";
            } else if (key === "최고 드로우다운") {
                textColor = value === 0 ? "#008000" : "#a01722";
            }

            return {
                key,
                value: row[key],
                textColor,
                formattedContent: formatWithTooltip(row[key], key, config, symbol)
            };
        });
    }, [allHeaders, row, initialBalance, config, symbol]);

    return (
        <div
            className={rowClass}
            data-trade-no={currentTradeNo}
            onClick={(e) => e.stopPropagation()} // 컨테이너 클릭 이벤트 방지
            onContextMenu={(e) => e.stopPropagation()} // 컨테이너 우클릭 이벤트 방지
            onMouseEnter={() => {
                if (!selectedCell) {
                    onMouseEnter(currentTradeNo);
                }
            }}
            onMouseLeave={() => {
                if (!selectedCell) {
                    onMouseLeave();
                }
            }}
            onMouseMove={() => {
                // 선택 해제 직후 마우스가 이미 행 위에 있을 때 호버 상태 즉시 적용
                if (!selectedCell && hoverTradeNo !== currentTradeNo) {
                    onMouseEnter(currentTradeNo);
                }
            }}
            style={{
                position: 'absolute',
                top: `${Math.floor(originalIndex * rowHeight)}px`,
                left: 0,
                right: 0,
                height: `${Math.floor(rowHeight)}px`,
                width: '100%',
                display: 'flex',
                // 픽셀 정렬 강제
                backfaceVisibility: 'hidden',
                transform: 'translateZ(0)'
            }}
        >
            {cellContents.map((cell, colIndex) => {
                // 기본 배경색 설정
                let calculatedBackground = 'transparent';

                // 선택된 셀인지 확인
                const isSelectedCell = selectedCell &&
                    selectedCell.rowIndex === originalIndex &&
                    selectedCell.columnKey === cell.key;

                // 선택된 셀과 같은 행인지 확인 (행 그룹)
                const isSelectedRow = selectedCell &&
                    selectedCell.tradeNo === currentTradeNo;

                // 선택된 열 그룹에 속하는지 확인
                // selectedColumnGroup (state) 또는 selectedCell의 columnKey에서 직접 그룹을 계산하여
                // 상태 업데이트 순서로 인한 일시적 불일치 문제를 방지
                const isSelectedColumnGroup = selectedColumnGroup.includes(cell.key)
                    || (selectedCell ? getColumnGroup(selectedCell.columnKey).includes(cell.key) : false);

                // 호버된 열인지 확인
                const isHoverColumn = hoverColumn === cell.key;

                // 그룹 호버 확인
                const isHoverColumnGroup = hoverColumnGroup.includes(cell.key);

                // 디버깅: 겹치는 셀 조건 확인
                const isIntersection = isSelectedRow && isSelectedColumnGroup;

                // 호버 모드 (선택된 셀 없을 때)
                if (!selectedCell) {
                    // 행+열 동시 호버: 더 진한 색상 (0.15 투명도)
                    if (isHovered && (isHoverColumn || isHoverColumnGroup)) {
                        calculatedBackground = 'rgba(255, 215, 0, 0.15)';
                    }
                    // 행 호버: 같은 거래 번호 행 전체에 호버 효과
                    else if (isHovered) {
                        calculatedBackground = 'rgba(255, 215, 0, 0.1)';
                    }
                    // 열 호버: 같은 열 그룹에 호버 효과
                    else if (isHoverColumn || isHoverColumnGroup) {
                        calculatedBackground = 'rgba(255, 215, 0, 0.1)';
                    }
                }
                // 선택 모드 (선택된 셀 있을 때)
                else {
                    // 선택한 셀과 같은 행 그룹 및 열 그룹이 겹치는 셀에 강조색 적용
                    if (isSelectedCell) {
                        calculatedBackground = 'rgba(255, 215, 0, 0.25)';
                    } else if (isIntersection) {
                        calculatedBackground = 'rgba(255, 215, 0, 0.25)';
                    }
                    // 행 그룹 및 열 그룹 호버색 고정 (겹치지 않는 부분)
                    else if (isSelectedRow || isSelectedColumnGroup) {
                        calculatedBackground = 'rgba(255, 215, 0, 0.1)';
                    }
                }

                // 경계선 클래스 결정 - 모듈 레벨 Set 사용
                const borderClass = colIndex < allHeaders.length - 1 ?
                    (THICK_BORDER_COLUMNS.has(cell.key) ? 'thick-border' : 'thin-border') : '';

                // 셀 클래스 결정
                let cellClass = `td cell-width ${borderClass}`;
                if (isSelectedCell) cellClass += ' selected-cell';
                if (isSelectedRow) cellClass += ' selected-row';
                if (isSelectedColumnGroup) cellClass += ' selected-column-group';
                if (isHoverColumn) cellClass += ' hover-column';
                if (isHoverColumnGroup) cellClass += ' hover-column-group';
                if (isSelectedRow && isSelectedColumnGroup) cellClass += ' selected-intersection';

                return (
                    <div
                        key={cell.key}
                        className={cellClass}
                        data-column={cell.key}
                        data-trade-no={String(currentTradeNo)}
                        onClick={(e) => {
                            e.stopPropagation(); // 컨테이너 클릭 이벤트 방지
                            handleCellClick(cell.key);
                        }}
                        onContextMenu={(e) => {
                            e.stopPropagation(); // 컨테이너 우클릭 이벤트 방지
                        }}
                        onMouseEnter={() => {
                            // 선택된 셀이 있으면 호버 완전 차단
                            if (!selectedCell) {
                                onCellHover(cell.key);
                            }
                        }}
                        onMouseLeave={() => {
                            // 선택된 셀이 있으면 호버 완전 차단
                            if (!selectedCell) {
                                onCellHover(null);
                            }
                        }}
                        onMouseMove={() => {
                            // 선택된 셀이 있으면 호버 완전 차단
                            if (!selectedCell && hoverColumn !== cell.key) {
                                onCellHover(cell.key);
                            }
                        }}
                        style={{
                            color: cell.textColor || 'inherit',
                            // use backgroundColor for more specific control and avoid invalid '!important' strings
                            backgroundColor: calculatedBackground,
                            width: columnWidths[cell.key] || 'auto',
                            minWidth: columnWidths[cell.key] || 'auto',
                            maxWidth: columnWidths[cell.key] || 'auto',
                            flexBasis: columnWidths[cell.key] || 'auto',
                            cursor: 'pointer',
                            // selected intersection fallback: provide non-important inline zIndex/position
                            ...(isIntersection ? {zIndex: 50, position: 'relative'} : {})
                        }}
                    >
                        {cell.formattedContent}
                    </div>
                );
            })}
        </div>
    );
}, (prevProps, nextProps) => {
    // 깊은 비교를 위한 커스텀 비교 함수
    return (
        prevProps.row === nextProps.row &&
        prevProps.originalIndex === nextProps.originalIndex &&
        prevProps.isActiveRow === nextProps.isActiveRow &&
        prevProps.hoverTradeNo === nextProps.hoverTradeNo &&
        prevProps.hoverColumn === nextProps.hoverColumn &&
        prevProps.hoverColumnGroup === nextProps.hoverColumnGroup &&
        prevProps.selectedColumnGroup === nextProps.selectedColumnGroup &&
        prevProps.selectedCell === nextProps.selectedCell &&
        prevProps.isDifferentFromNext === nextProps.isDifferentFromNext &&
        prevProps.isLastRowOfDataset === nextProps.isLastRowOfDataset &&
        prevProps.columnWidths === nextProps.columnWidths &&
        prevProps.config === nextProps.config
    );
});

TradeRow.displayName = 'TradeRow';

/**
 * 페이지 제목 컴포넌트
 */
const PageTitle = React.memo(() => (
    <div style={{
        position: 'relative',
        marginBottom: '25px',
        zIndex: 100
    }}>
        <motion.h2
            initial={{opacity: 0, x: -20}}
            animate={{opacity: 1, x: 0}}
            transition={{delay: 0.1, duration: 0.5}}
            style={{
                color: 'white',
                fontSize: '2.5rem',
                fontWeight: 700,
                textAlign: 'left',
                marginLeft: '30px',
                marginTop: '5px',
                marginBottom: '-7px',
                paddingBottom: '8px',
                display: 'inline-block',
                position: 'relative',
            }}
        >
            거래 내역
            {/* 밑줄 */}
            <motion.span
                initial={{width: 0}}
                animate={{width: '100%'}}
                transition={{delay: 0.3, duration: 0.5}}
                style={{
                    position: 'absolute',
                    bottom: -1,
                    left: 1,
                    right: 0,
                    height: '2px',
                    background: 'rgba(255, 215, 0, 0.4)',
                }}
            />
        </motion.h2>
    </div>
));

export default function TradeList({config}: TradeListProps) {
    const {filteredTrades, allTrades} = useTradeFilter()
    const [trades, setTrades] = useState<TradeItem[]>([])
    const [isLoading, setIsLoading] = useState(true)
    const tableRef = useRef<HTMLDivElement>(null)
    const [hoverTradeNo, setHoverTradeNo] = useState<number | null>(null)
    const [hoverColumn, setHoverColumn] = useState<string | null>(null)
    const [hoverColumnGroup, setHoverColumnGroup] = useState<string[]>([]) // 호버된 열 그룹

    // 최근 마우스 위치를 추적하여 ESC 해제 시 해당 위치의 셀을 찾아 하이라이트를 복원
    const lastMousePos = useRef<{ x: number, y: number } | null>(null);

    // 선택된 셀 정보
    const [selectedCell, setSelectedCell] = useState<{
        rowIndex: number,
        columnKey: string,
        tradeNo: number
    } | null>(null)
    const [selectedColumnGroup, setSelectedColumnGroup] = useState<string[]>([]) // 선택된 열 그룹

    // ESC 키로 선택 해제
    useEffect(() => {
        const handleKeyDown = (event: KeyboardEvent) => {
            if (event.key === 'Escape' && selectedCell) {
                setSelectedCell(null);
                setSelectedColumnGroup([]);
                // 마우스 위치가 있으면 해당 위치의 셀을 찾아 행+열 하이라이트를 복원
                const pos = lastMousePos.current;
                if (pos) {
                    const el = document.elementFromPoint(pos.x, pos.y) as HTMLElement | null;
                    const cellEl = el ? el.closest('.td') as HTMLElement | null : null;
                    if (cellEl) {
                        const col = cellEl.getAttribute('data-column');
                        const tradeNoAttr = cellEl.getAttribute('data-trade-no');
                        const tradeNo = tradeNoAttr ? Number(tradeNoAttr) : null;
                        if (tradeNo !== null && !Number.isNaN(tradeNo)) {
                            setHoverTradeNo(tradeNo);
                        } else {
                            setHoverTradeNo(null);
                        }
                        if (col) {
                            setHoverColumn(col);
                            setHoverColumnGroup(getColumnGroup(col));
                        } else {
                            setHoverColumn(null);
                            setHoverColumnGroup([]);
                        }
                        return;
                    }
                }
                // 위치를 찾지 못하면 기존 동작 (호버 초기화)
                setHoverTradeNo(null);
                setHoverColumn(null);
                setHoverColumnGroup([]);
            }
        };

        document.addEventListener('keydown', handleKeyDown);
        return () => {
            document.removeEventListener('keydown', handleKeyDown);
        };
    }, [selectedCell]);

    // 전역 마우스 이동을 추적하여 lastMousePos를 갱신
    useEffect(() => {
        const handleMouseMove = (e: MouseEvent) => {
            lastMousePos.current = {x: e.clientX, y: e.clientY};
        };
        window.addEventListener('mousemove', handleMouseMove, {passive: true});
        return () => window.removeEventListener('mousemove', handleMouseMove);
    }, []);

    // 가상 스크롤 상태 및 설정
    const viewportRef = useRef<HTMLDivElement>(null);
    const [scrollTop, setScrollTop] = useState(0);
    const rowHeight = 45;
    const overscanCount = 5; // 미리 렌더링할 행 수를 대폭 축소하여 성능 최적화

    const [visibleRange, setVisibleRange] = useState({start: 0, end: 0});

    // 열 너비 관리를 위한 상태 추가
    const [columnWidths, setColumnWidths] = useState<{ [key: string]: string }>({});
    const headerRef = useRef<HTMLDivElement>(null);

    // allHeaders를 useEffect 외부로 이동하거나, 의존성 배열에서 제거
    const allHeaders = useMemo(() =>
            trades.length > 0 ? Object.keys(trades[0]).filter(key => key !== 'originalIdxForSort') : []
        , [trades]);

    // 데이터 로딩 처리 - 최적화된 데이터 변환
    const prevFilteredTradesRef = useRef<any[]>([]);
    useEffect(() => {
        if (filteredTrades === prevFilteredTradesRef.current) {
            return; // 데이터가 변경되지 않았으면 스킵
        }

        setIsLoading(true);

        // 배치 처리를 위한 setTimeout 사용
        const timeoutId = setTimeout(() => {
            const newTrades = filteredTrades.map((trade, index) => ({
                ...trade,
                originalIdxForSort: index
            }));
            setTrades(newTrades);
            prevFilteredTradesRef.current = filteredTrades;
            setIsLoading(false);
        }, 0);

        return () => clearTimeout(timeoutId);
    }, [filteredTrades]);

    // 열 너비 계산 최적화 - 샘플링 사용 및 캐싱 강화
    const columnWidthsCache = useRef<Map<string, { [key: string]: string }>>(new Map());
    const canvasRef = useRef<HTMLCanvasElement | null>(null);
    const contextRef = useRef<CanvasRenderingContext2D | null>(null);

    const calculateColumnWidths = useCallback(() => {
        if (allHeaders.length === 0) {
            setColumnWidths({});
            return;
        }

        // 캐시 키 생성 (데이터의 특성을 반영)
        const cacheKey = `${allHeaders.join(',')}_${trades.length}_${Date.now() % 3600000}`; // 1시간마다 캐시 갱신

        if (columnWidthsCache.current.has(cacheKey)) {
            setColumnWidths(columnWidthsCache.current.get(cacheKey)!);
            return;
        }

        if (trades.length === 0 || !headerRef.current) {
            const defaultWidths: { [key: string]: string } = {};
            allHeaders.forEach(header => {
                if (header === 'originalIdxForSort') return;
                defaultWidths[header] = '100px';
            });
            setColumnWidths(defaultWidths);
            return;
        }

        const newColumnWidths: { [key: string]: string } = {};
        const cellPadding = 16;
        const minCellWidth = 50;

        // Canvas context 재사용
        if (!canvasRef.current) {
            canvasRef.current = document.createElement('canvas');
            contextRef.current = canvasRef.current.getContext('2d');
        }

        const context = contextRef.current;
        const headerElement = headerRef.current;
        const sampleHeaderCell = headerElement.querySelector('.th[data-header]') as HTMLElement;

        if (!context || !sampleHeaderCell) {
            allHeaders.forEach(headerKey => {
                if (headerKey === 'originalIdxForSort') return;
                newColumnWidths[headerKey] = `${minCellWidth + cellPadding}px`;
            });
            setColumnWidths(newColumnWidths);
            return;
        }

        const computedStyle = window.getComputedStyle(sampleHeaderCell);
        context.font = computedStyle.font;

        allHeaders.forEach(headerKey => {
            if (headerKey === 'originalIdxForSort') return;

            const headerTextMetrics = context.measureText(headerKey);
            const calculatedHeaderWidth = Math.ceil(headerTextMetrics.width + cellPadding);

            let maxDataTextWidth = 0;
            if (trades.length > 0) {
                // 더 많은 샘플을 체크하여 정확한 최대값 찾기 (최대 100개 항목 체크)
                const sampleSize = Math.min(100, trades.length);
                const step = Math.max(1, Math.floor(trades.length / sampleSize));

                for (let i = 0; i < trades.length; i += step) {
                    const trade = trades[i];
                    const displayString = getDisplayStringForMeasuring(trade[headerKey], headerKey, config, getSymbolFromTrade(trade));

                    // 빈 문자열이면 스킵
                    if (!displayString) continue;

                    const dataTextMetrics = context.measureText(displayString);
                    maxDataTextWidth = Math.max(maxDataTextWidth, Math.ceil(dataTextMetrics.width));
                }

                // 추가로 처음 100개와 마지막 100개도 체크하여 극값 놓치지 않기
                const additionalChecks = Math.min(100, trades.length);
                for (let i = 0; i < additionalChecks; i++) {
                    const trade = trades[i];
                    const displayString = getDisplayStringForMeasuring(trade[headerKey], headerKey, config, getSymbolFromTrade(trade));
                    if (displayString) {
                        const dataTextMetrics = context.measureText(displayString);
                        maxDataTextWidth = Math.max(maxDataTextWidth, Math.ceil(dataTextMetrics.width));
                    }
                }

                for (let i = Math.max(0, trades.length - additionalChecks); i < trades.length; i++) {
                    const trade = trades[i];
                    const displayString = getDisplayStringForMeasuring(trade[headerKey], headerKey, config, getSymbolFromTrade(trade));
                    if (displayString) {
                        const dataTextMetrics = context.measureText(displayString);
                        maxDataTextWidth = Math.max(maxDataTextWidth, Math.ceil(dataTextMetrics.width));
                    }
                }
            }
            const calculatedMaxDataWidth = Math.ceil(maxDataTextWidth + cellPadding);

            const finalWidth = Math.ceil(Math.max(calculatedHeaderWidth, calculatedMaxDataWidth, minCellWidth));
            newColumnWidths[headerKey] = `${finalWidth}px`;
        });

        // 캐시에 저장
        columnWidthsCache.current.set(cacheKey, newColumnWidths);

        // 캐시 크기 제한
        if (columnWidthsCache.current.size > 5) {
            const firstKey = columnWidthsCache.current.keys().next().value;
            if (firstKey) {
                columnWidthsCache.current.delete(firstKey);
            }
        }

        setColumnWidths(newColumnWidths);
    }, [trades, allHeaders, config]);

    // 열 너비 계산
    useEffect(() => {
        calculateColumnWidths();
        // cleanup 불필요: 즉시 실행만
    }, [calculateColumnWidths]);

    // 보이는 행 계산 로직 최적화 - 스크롤 디바운싱 및 RAF 최적화
    const visibleRangeRef = useRef(visibleRange);
    const rafRef = useRef<number | null>(null);
    visibleRangeRef.current = visibleRange;

    useEffect(() => {
        const currentViewport = viewportRef.current;
        if (!currentViewport || trades.length === 0) {
            const newRange = {start: 0, end: Math.min(trades.length - 1, overscanCount * 2 + 10)};
            setVisibleRange(newRange);
            return;
        }

        const updateVisibleRange = () => {
            const containerHeight = currentViewport.clientHeight;

            const newStart = Math.max(0, Math.floor(scrollTop / rowHeight) - overscanCount);
            const newEnd = Math.min(
                trades.length - 1,
                Math.floor((scrollTop + containerHeight) / rowHeight) + overscanCount
            );

            const currentRange = visibleRangeRef.current;
            if (newStart !== currentRange.start || newEnd !== currentRange.end) {
                setVisibleRange({start: newStart, end: newEnd});
            }
        };

        if (rafRef.current) {
            cancelAnimationFrame(rafRef.current);
        }
        rafRef.current = requestAnimationFrame(updateVisibleRange);

        return () => {
            if (rafRef.current) {
                cancelAnimationFrame(rafRef.current);
            }
        };
    }, [scrollTop, trades.length, rowHeight, overscanCount]);

    const scrollTimeoutRef = useRef<NodeJS.Timeout | null>(null);

    const handleScroll = useCallback((event: React.UIEvent<HTMLDivElement>) => {
        const newScrollTop = event.currentTarget.scrollTop;

        // RAF를 사용하여 스크롤 최적화
        if (rafRef.current) {
            cancelAnimationFrame(rafRef.current);
        }

        rafRef.current = requestAnimationFrame(() => {
            // 디바운스로 빈번한 상태 업데이트 방지
            if (scrollTimeoutRef.current) {
                clearTimeout(scrollTimeoutRef.current);
            }

            scrollTimeoutRef.current = setTimeout(() => {
                setScrollTop(newScrollTop);
            }, 16); // 60fps로 제한하여 성능 개선
        });
    }, []);

    // cleanup 함수 추가
    useEffect(() => {
        return () => {
            if (rafRef.current) {
                cancelAnimationFrame(rafRef.current);
            }
            if (scrollTimeoutRef.current) {
                clearTimeout(scrollTimeoutRef.current);
            }
        };
    }, []);

    // 초기 자금 계산 - 캐싱 추가
    const initialBalanceCache = useRef<{ trades: any[], balance: number } | null>(null);
    const initialBalance = useMemo(() => {
        // 캐시가 있고 trades가 동일하면 캐시된 값 반환
        if (initialBalanceCache.current && initialBalanceCache.current.trades === trades) {
            return initialBalanceCache.current.balance;
        }

        const initialTrade = trades.find(trade => trade["거래 번호"] === 0);
        const balance = initialTrade ? Number(initialTrade["현재 자금"]) : 0;

        // 캐시 업데이트
        initialBalanceCache.current = {trades, balance};

        return balance;
    }, [trades]);

    // 렌더링할 가시적 거래 데이터 준비 - 최적화된 계산 및 메모리 절약
    const visibleTradesData = useMemo(() => {
        if (trades.length === 0) return [];

        const start = Math.max(0, visibleRange.start);
        const end = Math.min(visibleRange.end, trades.length - 1);

        if (start > end) return [];

        const items = new Array(end - start + 1);

        for (let i = start; i <= end; i++) {
            items[i - start] = {
                tradeData: trades[i],
                originalIndex: i
            };
        }

        return items;
    }, [trades, visibleRange.start, visibleRange.end]);

    // 마우스 이벤트 핸들러 최적화 - 디바운싱 제거하고 직접 처리
    const handleMouseEnter = useCallback((tradeNo: number) => {
        setHoverTradeNo(tradeNo);
    }, []);

    const handleMouseLeave = useCallback(() => {
        setHoverTradeNo(null);
    }, []);

    // 셀 호버 핸들러 - 그룹 단위로 호버 처리
    const handleCellHover = useCallback((columnKey: string | null) => {
        setHoverColumn(columnKey);
        if (columnKey) {
            const group = getColumnGroup(columnKey);
            setHoverColumnGroup(group);
        } else {
            setHoverColumnGroup([]);
        }
    }, []);

    // 셀 클릭 핸들러 - 헤더는 제외, 같은 셀 또는 선택된 교차 영역 클릭시 선택 해제, 그룹 단위로 선택
    const handleCellClick = useCallback((rowIndex: number, columnKey: string, tradeNo: number) => {
        // 현재 선택된 셀이 있고, 클릭한 셀이
        //  - 동일한 셀이거나
        //  - 동일한 거래(행 그룹)이고 선택된 열 그룹에 속하는 열이면 선택 해제
        const clickedCellIsSame = selectedCell &&
            selectedCell.rowIndex === rowIndex &&
            selectedCell.columnKey === columnKey &&
            selectedCell.tradeNo === tradeNo;

        const clickedCellInSelectedIntersection = selectedCell &&
            selectedCell.tradeNo === tradeNo &&
            getColumnGroup(selectedCell.columnKey).includes(columnKey);

        if (clickedCellIsSame || clickedCellInSelectedIntersection) {
            // 선택 해제
            setSelectedCell(null);
            setSelectedColumnGroup([]);
            // 선택 해제 시 현재 클릭한 셀의 행 그룹과 열 그룹을 즉시 복원하여
            // 마우스 움직임 없이도 하이라이트가 표시되도록 함
            setHoverTradeNo(tradeNo);
            setHoverColumn(columnKey);
            setHoverColumnGroup(getColumnGroup(columnKey));
        } else {
            setSelectedCell({rowIndex, columnKey, tradeNo});
            // 선택된 열의 그룹도 함께 선택
            const group = getColumnGroup(columnKey);
            setSelectedColumnGroup(group);

            // 선택 모드에서 열 그룹 호버 배경색 고정을 위해 현재 호버 상태 유지
            // 이미 호버된 열이 있으면 그대로 유지, 없으면 클릭한 열로 설정
            if (!hoverColumn) {
                setHoverColumn(columnKey);
                setHoverColumnGroup(group);
            }
        }
    }, [selectedCell, hoverColumn]);

    // 빈 공간 클릭으로 선택 해제
    const handleContainerClick = useCallback((event: React.MouseEvent) => {
        // 이벤트가 셀이나 테이블 요소에서 발생하지 않았을 때만 선택 해제
        const target = event.target as HTMLElement;
        if (!target.closest('.td') && !target.closest('.th') && selectedCell) {
            setSelectedCell(null);
            setSelectedColumnGroup([]);
            setHoverTradeNo(null);
            setHoverColumn(null);
            setHoverColumnGroup([]);
        }
    }, [selectedCell]);

    // 거래 데이터가 없으면 메시지만 표시
    if (!allTrades || allTrades.length === 0) {
        return <NoDataMessage message="거래 내역이 존재하지 않습니다."/>;
    }

    if (isLoading) return <LoadingSpinner/>

    return (
        <motion.div
            initial={{opacity: 0}}
            animate={{opacity: 1}}
            transition={{duration: 0.5}}
            style={{
                width: '100%',
                height: '100%',
                padding: '20px',
                overflow: 'visible',
                display: 'flex',
                flexDirection: 'column',
                position: 'relative',
                minWidth: '0'
            }}
            className={`trade-list-container${selectedCell ? ' has-selected-cell' : ''}`}
            onClick={handleContainerClick}
        >
            {/* 제목 */}
            <PageTitle/>

            {/* 거래 내역 테이블 */}
            <motion.div
                ref={tableRef}
                initial={{opacity: 0, y: 20}}
                animate={{opacity: 1, y: 0}}
                transition={{delay: 0.5, duration: 0.5}}
                style={{
                    width: 'calc(100% - 35px)',
                    maxHeight: '765px',
                    height: 'auto',
                    marginLeft: '17px',
                    marginTop: '17px',
                    position: 'relative',
                    flex: 'unset',
                    borderRadius: '8px',
                    display: 'flex',
                    flexDirection: 'column',
                    boxSizing: 'border-box',
                    willChange: 'transform', // GPU 가속 활성화
                    transform: 'translateZ(0)' // 하드웨어 가속 강제 적용
                }}
            >
                {/* 테두리를 위한 별도 요소 추가 */}
                <div style={{
                    position: 'absolute',
                    top: -1,
                    left: -1,
                    right: -1,
                    bottom: -1,
                    borderRadius: '8px',
                    border: '2px solid rgb(155, 132, 7)',
                    overflow: 'hidden',
                    pointerEvents: 'none',
                    zIndex: 100,
                    willChange: 'transform', // GPU 가속
                    transform: 'translateZ(0)'
                }}/>

                <div style={{
                    width: '100%',
                    height: 'auto',
                    boxSizing: 'border-box',
                    overflow: 'visible',
                    display: 'flex',
                    flexDirection: 'column',
                    position: 'relative',
                    willChange: 'transform',
                    transform: 'translateZ(0)'
                }}>
                    <div
                        ref={viewportRef}
                        onScroll={handleScroll}
                        className="scrollable-table-wrapper"
                        style={{
                            width: '100%',
                            boxSizing: 'border-box',
                            maxHeight: '765px',
                            height: 'auto',
                            overflowY: 'auto',
                            overflowX: 'auto',
                            position: 'relative',
                            backgroundColor: '#111111',
                            borderRadius: '6px',
                            willChange: 'scroll-position', // 스크롤 최적화
                            transform: 'translateZ(0)'
                        }}
                    >
                        <div className="table table-auto" style={{
                            width: 'max-content',
                            borderCollapse: 'separate',
                            borderSpacing: '0',
                            fontSize: '14px',
                            position: 'relative',
                            backgroundColor: '#111111',
                            willChange: 'transform',
                            transform: 'translateZ(0)',
                            // 픽셀 정렬 강제 (세로선 밀리므로 필수)
                            imageRendering: 'pixelated',
                            backfaceVisibility: 'hidden',
                            perspective: '1000px'
                        }}>
                            <div className="thead" ref={headerRef} style={{
                                borderRadius: '6px 6px 0 0',
                                overflow: 'hidden',
                                borderBottom: 'none',
                                willChange: 'transform',
                                transform: 'translateZ(0)'
                            }}>
                                <div className="tr">
                                    {allHeaders.map((header, index) => (
                                        <div key={header} className="th fixed-header" data-header={header} style={{
                                            width: columnWidths[header] || 'auto',
                                            minWidth: columnWidths[header] || '50px',
                                            maxWidth: columnWidths[header] || 'auto',
                                            boxSizing: 'border-box',
                                            padding: '10px',
                                            margin: '0px',
                                            fontFamily: 'inherit',
                                            color: 'rgba(255, 215, 0, 0.8)',
                                            backgroundColor: 'transparent',
                                            borderRight: index < allHeaders.length - 1 ? (
                                                header === "거래 번호" ||
                                                header === "심볼 이름" ||
                                                header === "진입 방향" ||
                                                header === "보유 시간" ||
                                                header === "레버리지" ||
                                                header === "진입 수량" ||
                                                header === "청산 수량" ||
                                                header === "강제 청산 가격" ||
                                                header === "펀딩비 수령" ||
                                                header === "펀딩비 지불" ||
                                                header === "강제 청산 수수료" ||
                                                header === "순손익" ||
                                                header === "전체 순손익률" ||
                                                header === "최고 자금" ||
                                                header === "최고 드로우다운" ||
                                                header === "누적 손익률"
                                                    ? '2px solid rgba(255, 215, 0, 0.4)'
                                                    : '1px dashed rgba(255, 215, 0, 0.4)'
                                            ) : 'none',
                                            flexBasis: columnWidths[header] || 'auto',
                                            flexGrow: 0,
                                            flexShrink: 0,
                                            // 픽셀 정렬 강제
                                            position: 'relative',
                                            overflow: 'hidden'
                                        }}>
                                            <div style={{
                                                display: 'flex',
                                                justifyContent: 'center',
                                                alignItems: 'center',
                                                width: '100%',
                                                padding: '3px 10px',
                                                fontFamily: 'inherit',
                                                fontWeight: '700',
                                            }}>
                                                <span style={{
                                                    fontSize: '14px !important;',
                                                    lineHeight: '1',
                                                }}>{header}</span>
                                            </div>
                                        </div>
                                    ))}
                                </div>
                            </div>
                            <div className="tbody" style={{
                                height: `${Math.floor(trades.length * rowHeight)}px`,
                                backgroundColor: '#111111',
                                // GPU 가속 제거하여 메모리 사용량 감소
                                position: 'relative'
                            }}>
                                {visibleTradesData.map(({tradeData: row, originalIndex}) => {
                                    const currentTradeNo = row["거래 번호"];

                                    // 조기 반환을 통한 성능 최적화
                                    const nextTradeOriginalIndex = originalIndex + 1;
                                    const nextTrade = nextTradeOriginalIndex < trades.length ? trades[nextTradeOriginalIndex] : null;
                                    const nextTradeNo = nextTrade ? nextTrade["거래 번호"] : null;
                                    const isDifferentFromNext = nextTradeNo !== null && currentTradeNo !== nextTradeNo;
                                    const isLastRowOfDataset = originalIndex === trades.length - 1;

                                    // isActiveRow 계산 최적화
                                    let isActiveRow = false;
                                    if (originalIndex === 0) {
                                        isActiveRow = currentTradeNo !== 0;
                                    } else {
                                        const prevTrade = trades[originalIndex - 1];
                                        if (prevTrade) {
                                            const prevTradeNo = prevTrade["거래 번호"];
                                            isActiveRow = currentTradeNo !== 0 && currentTradeNo !== prevTradeNo;
                                        }
                                    }

                                    return (
                                        <TradeRow
                                            key={`${originalIndex}-${currentTradeNo}`} // 키 최적화
                                            row={row}
                                            originalIndex={originalIndex}
                                            allHeaders={allHeaders}
                                            columnWidths={columnWidths}
                                            rowHeight={rowHeight}
                                            isActiveRow={isActiveRow}
                                            hoverTradeNo={hoverTradeNo}
                                            hoverColumn={hoverColumn}
                                            hoverColumnGroup={hoverColumnGroup}
                                            selectedColumnGroup={selectedColumnGroup}
                                            initialBalance={initialBalance}
                                            config={config}
                                            isDifferentFromNext={isDifferentFromNext}
                                            isLastRowOfDataset={isLastRowOfDataset}
                                            selectedCell={selectedCell}
                                            onMouseEnter={handleMouseEnter}
                                            onMouseLeave={handleMouseLeave}
                                            onCellClick={handleCellClick}
                                            onCellHover={handleCellHover}
                                        />
                                    );
                                })}
                            </div>
                        </div>
                    </div>
                </div>
            </motion.div>
        </motion.div>
    )
}
