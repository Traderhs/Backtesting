import React, { createContext } from "react";

// 필터 타입 정의
export type FilterType = 
    | 'tradeNumber'
    | 'strategy'
    | 'symbol'
    | 'entryName'
    | 'exitName'
    | 'entryDirection'
    | 'entryTime'
    | 'exitTime'
    | 'holdingTime'
    | 'leverage'
    | 'entryPrice'
    | 'entryQuantity'
    | 'exitPrice'
    | 'exitQuantity'
    | 'forcedLiquidationPrice'
    | 'fundingReceiveCount'
    | 'fundingReceiveFee'
    | 'fundingPayCount'
    | 'fundingPayFee'
    | 'fundingCount'
    | 'fundingFee'
    | 'entryFee'
    | 'exitFee'
    | 'forcedLiquidationFee'
    | 'profitLoss'
    | 'netProfitLoss'
    | 'individualProfitRate'
    | 'overallProfitRate'
    | 'currentCapital'
    | 'highestCapital'
    | 'drawdown'
    | 'maxDrawdown'
    | 'accumulatedProfitLoss'
    | 'accumulatedProfitRate'
    | 'heldSymbolsCount';

export type TradeFilter = {
    // 자금 재계산 여부 (기본값 true)
    recalculateBalance?: boolean;

    tradeNumberMin?: string | number;
    tradeNumberMax?: string | number;
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
    leverageMin?: string | number;
    leverageMax?: string | number;
    entryPriceMin?: string | number;
    entryPriceMax?: string | number;
    entryQuantityMin?: string | number;
    entryQuantityMax?: string | number;
    exitPriceMin?: string | number;
    exitPriceMax?: string | number;
    exitQuantityMin?: string | number;
    exitQuantityMax?: string | number;
    forcedLiquidationPriceMin?: string | number;
    forcedLiquidationPriceMax?: string | number;
    fundingReceiveCountMin?: string | number;
    fundingReceiveCountMax?: string | number;
    fundingReceiveFeeMin?: string | number;
    fundingReceiveFeeMax?: string | number;
    fundingPayCountMin?: string | number;
    fundingPayCountMax?: string | number;
    fundingPayFeeMin?: string | number;
    fundingPayFeeMax?: string | number;
    fundingCountMin?: string | number;
    fundingCountMax?: string | number;
    fundingFeeMin?: string | number;
    fundingFeeMax?: string | number;
    entryFeeMin?: string | number;
    entryFeeMax?: string | number;
    exitFeeMin?: string | number;
    exitFeeMax?: string | number;
    forcedLiquidationFeeMin?: string | number;
    forcedLiquidationFeeMax?: string | number;
    profitLossMin?: string | number;
    profitLossMax?: string | number;
    netProfitLossMin?: string | number;
    netProfitLossMax?: string | number;
    individualProfitRateMin?: string | number;
    individualProfitRateMax?: string | number;
    overallProfitRateMin?: string | number;
    overallProfitRateMax?: string | number;
    currentCapitalMin?: string | number;
    currentCapitalMax?: string | number;
    highestCapitalMin?: string | number;
    highestCapitalMax?: string | number;
    drawdownMin?: string | number;
    drawdownMax?: string | number;
    maxDrawdownMin?: string | number;
    maxDrawdownMax?: string | number;
    accumulatedProfitLossMin?: string | number;
    accumulatedProfitLossMax?: string | number;
    accumulatedProfitRateMin?: string | number;
    accumulatedProfitRateMax?: string | number;
    heldSymbolsCountMin?: string | number;
    heldSymbolsCountMax?: string | number;
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
    isFiltering: boolean;
    filteringProgress: number; // 필터링 진행률 (0-100) 추가
    options: {
        symbols: FilterOption[];
        strategies: FilterOption[];
        entryNames: FilterOption[];
        exitNames: FilterOption[];
    };
    filterExpanded: boolean;
    setFilterExpanded: React.Dispatch<React.SetStateAction<boolean>>;
    // 달력 상태 관리
    openCalendar: string | null; // 현재 열린 달력 ID
    setOpenCalendar: React.Dispatch<React.SetStateAction<string | null>>;
}

export const TradeFilterContext = createContext<TradeFilterContextType | undefined>(undefined);
