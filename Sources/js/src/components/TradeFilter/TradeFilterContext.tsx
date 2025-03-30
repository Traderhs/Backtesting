import React, { createContext } from "react";

export type TradeFilter = {
    // 자금 재계산 여부 (기본값 true)
    recalculateBalance?: boolean;

    tradeNumberMin?: number;
    tradeNumberMax?: number;
    strategies: string[];
    symbols: string[];
    entryNames: string[];
    exitNames: string[];
    entryDirections: string[];
    entryTimeMin?: string;
    entryTimeMax?: string;
    exitTimeMin?: string;
    exitTimeMax?: string;
    // 진입시간 고급 필터 조건 (배열 형태)
    entryYears: number[];
    entryMonths: number[];
    entryDays: number[];
    entryDayOfWeeks: number[];
    entryHours: number[];
    entryMinutes: number[];
    entrySeconds: number[];
    // 청산시간 고급 필터 조건 (배열 형태)
    exitYears: number[];
    exitMonths: number[];
    exitDays: number[];
    exitDayOfWeeks: number[];
    exitHours: number[];
    exitMinutes: number[];
    exitSeconds: number[];
    holdingTimeMin?: number;
    holdingTimeMax?: number;

    // ───────────── 새로 추가할 숫자 범위 필터 ─────────────
    leverageMin?: number;
    leverageMax?: number;
    entryPriceMin?: number;
    entryPriceMax?: number;
    entryQuantityMin?: number;
    entryQuantityMax?: number;
    exitPriceMin?: number;
    exitPriceMax?: number;
    exitQuantityMin?: number;
    exitQuantityMax?: number;
    forcedLiquidationPriceMin?: number;
    forcedLiquidationPriceMax?: number;
    entryFeeMin?: number;
    entryFeeMax?: number;
    exitFeeMin?: number;
    exitFeeMax?: number;
    forcedLiquidationFeeMin?: number;
    forcedLiquidationFeeMax?: number;
    profitLossMin?: number;
    profitLossMax?: number;
    netProfitLossMin?: number;
    netProfitLossMax?: number;
    individualProfitRateMin?: number;
    individualProfitRateMax?: number;
    overallProfitRateMin?: number;
    overallProfitRateMax?: number;
    currentCapitalMin?: number;
    currentCapitalMax?: number;
    highestCapitalMin?: number;
    highestCapitalMax?: number;
    drawdownMin?: number;
    drawdownMax?: number;
    maxDrawdownMin?: number;
    maxDrawdownMax?: number;
    accumulatedProfitLossMin?: number;
    accumulatedProfitLossMax?: number;
    accumulatedProfitRateMin?: number;
    accumulatedProfitRateMax?: number;
    heldSymbolsCountMin?: number;
    heldSymbolsCountMax?: number;
};

export type FilterOption = {
    name: string;
};

export type TradeItem = {
    "거래 번호": number;
    "진입 방향": string;
    [key: string]: string | number;
};

export interface TradeFilterContextType {
    filter: TradeFilter;
    setFilter: React.Dispatch<React.SetStateAction<TradeFilter>>;
    allTrades: TradeItem[];
    filteredTrades: TradeItem[];
    loading: boolean;
    options: {
        symbols: FilterOption[];
        strategies: FilterOption[];
        entryNames: FilterOption[];
        exitNames: FilterOption[];
    };
}

export const TradeFilterContext = createContext<TradeFilterContextType | undefined>(undefined);
