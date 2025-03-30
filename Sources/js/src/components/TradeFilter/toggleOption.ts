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
            // 숫자 필드: 빈 문자열이면 undefined, 아니면 숫자로 변환
            return {
                ...prevFilter,
                [type]: name.trim() === "" ? undefined : Number(name),
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
