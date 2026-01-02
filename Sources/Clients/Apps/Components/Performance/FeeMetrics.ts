import {TradeItem} from "@/Components/TradeFilter/TradeFilterContext";
import {safeNumber} from './Utils.ts';

export interface FeeMetricsResult {
    totalEntryFee: number;
    totalExitFee: number;
    totalLiquidationFee: number;
    totalFee: number;
}

export const calculateFeeMetrics = (
    actualTrades: TradeItem[]
): FeeMetricsResult => {
    // 진입 수수료는 같은 거래 번호에 대해 한 번만 계산
    const tradeNumberToEntryFee = new Map<number, number>();
    let totalExitFee = 0;
    let totalLiquidationFee = 0;

    actualTrades.forEach((trade: TradeItem) => {
        const tradeNumber = safeNumber(trade["거래 번호"]);

        // 0번 거래 번호 제외
        if (tradeNumber === 0) return;

        // 진입 수수료는 동일한 거래 번호에 대해 한 번만 저장
        const entryFee = safeNumber(trade["진입 수수료"]);
        if (!tradeNumberToEntryFee.has(tradeNumber) && entryFee > 0) {
            tradeNumberToEntryFee.set(tradeNumber, entryFee);
        }

        // 청산 수수료와 강제 청산 수수료는 모든 항목 합산
        totalExitFee += safeNumber(trade["청산 수수료"]);
        totalLiquidationFee += safeNumber(trade["강제 청산 수수료"]);
    });

    // 진입 수수료 합계 계산
    const totalEntryFee = Array.from(tradeNumberToEntryFee.values()).reduce((sum, fee) => sum + fee, 0);
    const totalFee = totalEntryFee + totalExitFee + totalLiquidationFee;

    return {
        totalEntryFee,
        totalExitFee,
        totalLiquidationFee,
        totalFee
    };
};
