import {TradeItem} from "@/components/TradeFilter/TradeFilterContext";
import {safeNumber} from './Utils.ts';

export interface AverageProfitLossMetricsResult {
    avgPnL: number;
    avgProfit: number | string;
    avgLoss: number | string;
    avgIndividualPnLRate: string;
    avgProfitIndividualRate: string;
    avgLossIndividualRate: string;
    avgTotalPnLRate: string;
    avgProfitTotalRate: string;
    avgLossTotalRate: string;
}

export const calculateAverageProfitLossMetrics = (
    actualTrades: TradeItem[]
): AverageProfitLossMetricsResult => {
    // 거래 번호별로 데이터를 그룹화
    const tradeGroups: {
        [key: number]: {
            pnl: number,
            individualPnLRate: number,
            totalPnLRate: number
        }
    } = {};

    // 거래 번호별로 데이터 그룹화 및 합산
    actualTrades.forEach(trade => {
        const tradeNumber = safeNumber(trade["거래 번호"]);

        // 0번 거래 번호 제외
        if (tradeNumber === 0) return;

        const netPnL = safeNumber(trade["순손익"]);
        const individualPnLRate = safeNumber(trade["개별 순손익률"] || 0);
        const totalPnLRate = safeNumber(trade["전체 순손익률"] || 0);

        // 해당 거래 번호가 없으면 초기화
        if (!tradeGroups[tradeNumber]) {
            tradeGroups[tradeNumber] = {
                pnl: 0,
                individualPnLRate: 0,
                totalPnLRate: 0
            };
        }

        // 순손익과 비율 합산
        tradeGroups[tradeNumber].pnl += netPnL;
        tradeGroups[tradeNumber].individualPnLRate += individualPnLRate;
        tradeGroups[tradeNumber].totalPnLRate += totalPnLRate;
    });

    // 그룹화된 거래 데이터 배열로 변환
    const groupedTrades = Object.values(tradeGroups);

    // 평균 계산을 위한 변수들
    let totalPnL = 0;
    let totalProfit = 0;
    let totalLoss = 0;
    let profitTradeCount = 0;
    let lossTradeCount = 0;
    let totalIndividualPnLRate = 0;
    let totalProfitIndividualRate = 0;
    let totalLossIndividualRate = 0;
    let totalTotalPnLRate = 0;
    let totalProfitTotalRate = 0;
    let totalLossTotalRate = 0;

    // 그룹화된 거래 데이터 처리
    groupedTrades.forEach(trade => {
        const netPnL = trade.pnl;
        const individualPnLRate = trade.individualPnLRate;
        const totalPnLRate = trade.totalPnLRate;

        // 모든 거래 합산
        totalPnL += netPnL;

        // 전체 거래에 대한 평균 계산용 누적
        totalIndividualPnLRate += individualPnLRate;
        totalTotalPnLRate += totalPnLRate;

        if (netPnL >= 0) {
            totalProfit += netPnL;
            profitTradeCount++;

            // 수익 거래에 대한 평균 계산용 누적
            totalProfitIndividualRate += individualPnLRate;
            totalProfitTotalRate += totalPnLRate;
        } else if (netPnL < 0) {
            totalLoss += netPnL; // 음수 그대로 합산
            lossTradeCount++;

            // 손실 거래에 대한 평균 계산용 누적
            totalLossIndividualRate += individualPnLRate;
            totalLossTotalRate += totalPnLRate;
        }
    });

    const totalTradeCount = groupedTrades.length;

    // 평균 손익 계산
    const avgPnL = totalTradeCount > 0
        ? totalPnL / totalTradeCount
        : 0;

    // 평균 수익 계산 수정: 수익 거래 없으면 '-' 반환
    const avgProfit = profitTradeCount > 0
        ? totalProfit / profitTradeCount
        : '-';

    // 평균 손실 계산 수정: 손실 거래 없으면 '-' 반환
    const avgLoss = lossTradeCount > 0
        ? totalLoss / lossTradeCount
        : '-';

    // 개별 손익률 평균
    const avgIndividualPnLRate = totalTradeCount > 0
        ? `${(totalIndividualPnLRate / totalTradeCount).toFixed(2)}%`
        : "-";

    // 평균 수익 개별 손익률 수정: 수익 거래 없으면 '-' 반환
    const avgProfitIndividualRate = profitTradeCount > 0
        ? `${(totalProfitIndividualRate / profitTradeCount).toFixed(2)}%`
        : "-";

    const avgLossIndividualRate = lossTradeCount > 0
        ? `${(totalLossIndividualRate / lossTradeCount).toFixed(2)}%`
        : "-";

    // 전체 손익률 평균
    const avgTotalPnLRate = totalTradeCount > 0
        ? `${(totalTotalPnLRate / totalTradeCount).toFixed(2)}%`
        : "-";

    // 평균 수익 전체 손익률 수정: 수익 거래 없으면 '-' 반환
    const avgProfitTotalRate = profitTradeCount > 0
        ? `${(totalProfitTotalRate / profitTradeCount).toFixed(2)}%`
        : "-";

    const avgLossTotalRate = lossTradeCount > 0
        ? `${(totalLossTotalRate / lossTradeCount).toFixed(2)}%`
        : "-";

    return {
        avgPnL,
        avgProfit,
        avgLoss,
        avgIndividualPnLRate,
        avgProfitIndividualRate,
        avgLossIndividualRate,
        avgTotalPnLRate,
        avgProfitTotalRate,
        avgLossTotalRate
    };
}; 