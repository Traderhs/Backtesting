// Trade Filter Utils - 멀티 워커 필터링을 위한 유틸리티 함수들
import {tradeFilterManager} from './tradeFilterManager';

interface TradeItem {
    [key: string]: any;
}

interface TradeFilter {
    recalculateBalance: boolean;
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
    entryYears: number[];
    entryMonths: number[];
    entryDays: number[];
    entryDayOfWeeks: number[];
    entryHours: number[];
    entryMinutes: number[];
    entrySeconds: number[];
    exitYears: number[];
    exitMonths: number[];
    exitDays: number[];
    exitDayOfWeeks: number[];
    exitHours: number[];
    exitMinutes: number[];
    exitSeconds: number[];
    holdingTimeMin?: number;
    holdingTimeMax?: number;
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
    fundingReceiveCountMin?: number;
    fundingReceiveCountMax?: number;
    fundingReceiveFeeMin?: number;
    fundingReceiveFeeMax?: number;
    fundingPayCountMin?: number;
    fundingPayCountMax?: number;
    fundingPayFeeMin?: number;
    fundingPayFeeMax?: number;
    fundingCountMin?: number;
    fundingCountMax?: number;
    fundingFeeMin?: number;
    fundingFeeMax?: number;
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
}

/**
 * 거래 데이터를 필터링합니다 (멀티 워커 버전)
 * @param trades 필터링할 거래 데이터
 * @param filter 필터 조건
 * @returns 필터링된 거래 데이터
 */
export async function filterTradesAsync(
    trades: TradeItem[],
    filter: TradeFilter
): Promise<TradeItem[]> {
    try {
        return await tradeFilterManager.filterTrades(trades, filter);
    } catch (error) {
        console.error('필터링 중 오류:', error);
        throw error;
    }
}

/**
 * 필터 매니저를 종료합니다 (페이지 언로드 시 호출)
 */
export function destroyFilterManager(): void {
    tradeFilterManager.destroy();
}

// 페이지 언로드 시 자동으로 워커들을 정리
if (typeof window !== 'undefined') {
    window.addEventListener('beforeunload', () => {
        destroyFilterManager();
    });
}
