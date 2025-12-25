import {TradeItem} from "@/components/TradeFilter/TradeFilterContext";
import {safeNumber} from './Utils.ts';

export interface TotalProfitLossMetricsResult {
    initialBalance: number;
    finalBalance: number;
    totalProfitLoss: number;
    totalProfitLossPercent: string;
    totalProfit: number | string;
    totalProfitPercent: string;
    totalLoss: number | string;
    totalLossPercent: string;
    profitTradeCount: number;
    lossTradeCount: number;
}

export const calculateTotalProfitLossMetrics = (
    actualTrades: TradeItem[],
    initialBalance: number = 0
): TotalProfitLossMetricsResult => {
    const lastTrade = actualTrades.length > 0 ? actualTrades[actualTrades.length - 1] : null;
    const finalBalance = lastTrade ? safeNumber(lastTrade["현재 자금"]) : initialBalance;

    const totalProfitLoss = finalBalance - initialBalance;
    const totalProfitLossPercent = initialBalance > 0
        ? `${((totalProfitLoss / initialBalance) * 100).toFixed(2)}%`
        : "0%";

    // 순수익과 순손실 계산
    let totalProfit = 0;
    let totalLoss = 0;

    // 거래 횟수 계산
    let profitTradeCount = 0;
    let lossTradeCount = 0;

    actualTrades.forEach(trade => {
        const netPnL = safeNumber(trade["순손익"]);

        if (netPnL >= 0) {
            totalProfit += netPnL;
            profitTradeCount++;
        } else if (netPnL < 0) {
            totalLoss += netPnL; // 음수 그대로 합산
            lossTradeCount++;
        }
    });

    // 수익 거래가 없을 경우 totalProfit을 '-'로 설정
    const formattedTotalProfit = profitTradeCount > 0 ? totalProfit : '-';
    const totalProfitPercent = initialBalance > 0 && profitTradeCount > 0
        ? `${((totalProfit / initialBalance) * 100).toFixed(2)}%`
        : profitTradeCount > 0 ? "0%" : "-";

    const totalLossPercent = initialBalance > 0 && lossTradeCount > 0
        ? `${((totalLoss / initialBalance) * 100).toFixed(2)}%`
        : lossTradeCount > 0 ? "0%" : "-";

    // 음수 거래가 없으면 totalLoss를 '-'로 설정
    const formattedTotalLoss = lossTradeCount > 0 ? totalLoss : '-';

    return {
        initialBalance,
        finalBalance,
        totalProfitLoss,
        totalProfitLossPercent,
        totalProfit: formattedTotalProfit,
        totalProfitPercent,
        totalLoss: formattedTotalLoss,
        totalLossPercent,
        profitTradeCount,
        lossTradeCount
    };
};
