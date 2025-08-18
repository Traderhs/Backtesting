import { TradeItem } from "@/components/TradeFilter/TradeFilterContext";
import { safeNumber, parseDate } from './Utils.ts';

export interface MaxProfitLossMetricsResult {
    maxProfit: number | string;
    maxProfitIndividualRate: string;
    maxProfitTotalRate: string;
    maxLoss: number | string;
    maxLossIndividualRate: string;
    maxLossTotalRate: string;
}

export const calculateMaxProfitLossMetrics = (
    actualTrades: TradeItem[]
): MaxProfitLossMetricsResult => {
    if (!actualTrades || actualTrades.length === 0) {
        return {
            maxProfit: 0,
            maxProfitIndividualRate: '0.00%',
            maxProfitTotalRate: '0.00%',
            maxLoss: '-',
            maxLossIndividualRate: '-',
            maxLossTotalRate: '-',
        };
    }

    let maxProfitTrade = null;
    let maxLossTrade = null;
    let hasNegativeTrade = false;
    let hasPositiveTrade = false;
    let maxProfit = -Infinity;
    let maxLoss = 0;

    // 모든 거래를 순회하며 최대 순수익과 최대 순손실 거래 찾기
    for (const trade of actualTrades) {
        const pnl = safeNumber(trade["순손익"]);
        const tradeDate = parseDate(String(trade["청산 시간"]));
        
        // 최대 순수익 거래 업데이트 (0과 같거나 큰 거래만)
        if (pnl >= 0) {
            hasPositiveTrade = true;
            if (pnl > maxProfit || 
                (pnl === maxProfit && 
                 tradeDate && 
                 maxProfitTrade && 
                 parseDate(String(maxProfitTrade["청산 시간"])) !== null && 
                 tradeDate > parseDate(String(maxProfitTrade["청산 시간"]))!)) {
                maxProfitTrade = trade;
                maxProfit = pnl;
            }
        }
        
        // 최대 순손실 거래 업데이트 (음수인 손실 거래만 대상)
        if (pnl < 0 && (pnl < maxLoss || 
            (pnl === maxLoss && 
             tradeDate && 
             maxLossTrade && 
             parseDate(String(maxLossTrade["청산 시간"])) !== null && 
             tradeDate > parseDate(String(maxLossTrade["청산 시간"]))!))) {
            maxLossTrade = trade;
            maxLoss = pnl;
            hasNegativeTrade = true;
        }
    }

    // 값 추출 및 사용
    const maxProfitValue = hasPositiveTrade && maxProfitTrade ? safeNumber(maxProfitTrade["순손익"]) : '-';
    
    // 음수 거래가 있을 때만 실제 값 사용, 아니면 '-' 반환
    const maxLossValue = hasNegativeTrade && maxLossTrade ? safeNumber(maxLossTrade["순손익"]) : '-';
    
    // 거래 데이터에서 직접 개별/전체 순손익률 값 사용
    const maxProfitIndividualRate = hasPositiveTrade && maxProfitTrade ? String(maxProfitTrade["개별 순손익률"] || '0.00%') : '-';
    const maxProfitTotalRate = hasPositiveTrade && maxProfitTrade ? String(maxProfitTrade["전체 순손익률"] || '0.00%') : '-';
    
    const maxLossIndividualRate = hasNegativeTrade && maxLossTrade ? String(maxLossTrade["개별 순손익률"] || '0.00%') : '-';
    const maxLossTotalRate = hasNegativeTrade && maxLossTrade ? String(maxLossTrade["전체 순손익률"] || '0.00%') : '-';
    
    return {
        maxProfit: maxProfitValue,
        maxProfitIndividualRate,
        maxProfitTotalRate,
        maxLoss: maxLossValue,
        maxLossIndividualRate,
        maxLossTotalRate,
    };
}; 