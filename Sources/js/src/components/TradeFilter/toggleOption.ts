import { TradeFilter } from "./TradeFilterContext";
import { Dispatch, SetStateAction } from "react";

const numericFields = new Set<keyof TradeFilter>([
    "tradeNumberMin",
    "tradeNumberMax",
    "leverageMin",
    "leverageMax",
    "entryPriceMin",
    "entryPriceMax",
    "entryQuantityMin",
    "entryQuantityMax",
    "exitPriceMin",
    "exitPriceMax",
    "exitQuantityMin",
    "exitQuantityMax",
    "forcedLiquidationPriceMin",
    "forcedLiquidationPriceMax",
    "fundingReceiveCountMin",
    "fundingReceiveCountMax",
    "fundingReceiveFeeMin",
    "fundingReceiveFeeMax",
    "fundingPayCountMin",
    "fundingPayCountMax",
    "fundingPayFeeMin",
    "fundingPayFeeMax",
    "fundingCountMin",
    "fundingCountMax",
    "fundingFeeMin",
    "fundingFeeMax",
    "entryFeeMin",
    "entryFeeMax",
    "exitFeeMin",
    "exitFeeMax",
    "forcedLiquidationFeeMin",
    "forcedLiquidationFeeMax",
    "profitLossMin",
    "profitLossMax",
    "netProfitLossMin",
    "netProfitLossMax",
    "individualProfitRateMin",
    "individualProfitRateMax",
    "overallProfitRateMin",
    "overallProfitRateMax",
    "currentCapitalMin",
    "currentCapitalMax",
    "highestCapitalMin",
    "highestCapitalMax",
    "drawdownMin",
    "drawdownMax",
    "maxDrawdownMin",
    "maxDrawdownMax",
    "accumulatedProfitLossMin",
    "accumulatedProfitLossMax",
    "accumulatedProfitRateMin",
    "accumulatedProfitRateMax",
    "heldSymbolsCountMin",
    "heldSymbolsCountMax",
]);

export const toggleOption = (
    type: keyof TradeFilter,
    name: string,
    checked: boolean,
    setFilter: Dispatch<SetStateAction<TradeFilter>>
) => {
    setFilter((prevFilter) => {
        if (numericFields.has(type)) {
            // 숫자 필드 처리
            const trimmedName = name.trim();
            
            // 빈 문자열이면 undefined
            if (trimmedName === "") {
                return {
                    ...prevFilter,
                    [type]: undefined,
                };
            }
            
            // '-'만 있는 경우는 그대로 유지
            if (trimmedName === "-") {
                return {
                    ...prevFilter,
                    [type]: trimmedName,
                };
            }
            
            // **'-0'인 경우 문자열로 유지 (실시간 변환 방지)**
            // 단, 블러 시에는 $0로 처리
            if (trimmedName === "-0") {
                return {
                    ...prevFilter,
                    [type]: trimmedName,
                };
            }
            
            // '.'만 있는 경우도 그대로 유지
            if (trimmedName === ".") {
                return {
                    ...prevFilter,
                    [type]: trimmedName,
                };
            }
            
            // 맨 뒤에 소수점이 있는 경우 문자열로 유지 (중요!)
            if (trimmedName.endsWith('.') && trimmedName !== '.') {
                return {
                    ...prevFilter,
                    [type]: trimmedName,
                };
            }
            
            // **trailing zeros가 있는 경우 문자열로 유지 (예: "1.0", "2.00")**
            if (/\.\d*0$/.test(trimmedName)) {
                return {
                    ...prevFilter,
                    [type]: trimmedName,
                };
            }
            
            // **leading zeros가 있는 경우 문자열로 유지 (예: "01", "001.23")**
            if (/^0\d/.test(trimmedName)) {
                return {
                    ...prevFilter,
                    [type]: trimmedName,
                };
            }
            
            // **음수 leading zeros가 있는 경우 문자열로 유지 (예: "-01", "-001.23")**
            if (/^-0\d/.test(trimmedName)) {
                return {
                    ...prevFilter,
                    [type]: trimmedName,
                };
            }
            
            // **소수점으로 시작하는 경우 문자열로 유지 (예: ".123", ".5")**
            if (/^\./.test(trimmedName)) {
                return {
                    ...prevFilter,
                    [type]: trimmedName,
                };
            }
            
            // **음수 소수점으로 시작하는 경우 문자열로 유지 (예: "-.123", "-.5")**
            if (/^-\./.test(trimmedName)) {
                return {
                    ...prevFilter,
                    [type]: trimmedName,
                };
            }
            
            // **음수 trailing zeros가 있는 경우 문자열로 유지 (예: "-1.0", "-2.00")**
            if (/^-.*\.\d*0$/.test(trimmedName)) {
                return {
                    ...prevFilter,
                    [type]: trimmedName,
                };
            }
            
            // 숫자로 변환
            const numValue = Number(trimmedName);
            
            // NaN인 경우 undefined 처리
            if (isNaN(numValue)) {
                return {
                    ...prevFilter,
                    [type]: undefined,
                };
            }
            
            // 정상적인 숫자값 할당
            return {
                ...prevFilter,
                [type]: numValue,
            };
        } else if (
            type === "entryTimeMin" ||
            type === "entryTimeMax" ||
            type === "exitTimeMin" ||
            type === "exitTimeMax"
        ) {
            // 날짜 필드는 문자열로 처리
            return {
                ...prevFilter,
                [type]: name.trim() === "" ? undefined : name,
            };
        } else {
            // 나머지는 string[] 타입으로 처리 (체크박스용)
            const current = (prevFilter[type] as string[]) || [];
            const updated = checked ? [...current, name] : current.filter((item) => item !== name);
            return {
                ...prevFilter,
                [type]: updated,
            };
        }
    });
};
