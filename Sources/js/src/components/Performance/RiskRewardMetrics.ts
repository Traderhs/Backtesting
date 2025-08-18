import { TradeItem } from "@/components/TradeFilter/TradeFilterContext";
import { safeNumber } from './Utils.ts';
import { AverageProfitLossMetricsResult } from './AverageProfitLossMetrics.ts';

export interface RiskRewardMetricsResult {
    mdd: string;              // Maximum Drawdown (최대 낙폭)
    winRate: string;          // 승률
    profitFactor: string;     // 손익비
}

export const calculateRiskRewardMetrics = (
    actualTrades: TradeItem[],
    avgMetrics: AverageProfitLossMetricsResult // AverageProfitLossMetrics 결과 받기
): RiskRewardMetricsResult => {
    // MDD - 마지막 거래의 "최고 드로우다운" 값 사용
    let mdd = "0%";
    if (actualTrades.length > 0) {
        const lastTrade = actualTrades[actualTrades.length - 1];
        const maxDrawdown = safeNumber(lastTrade["최고 드로우다운"] || 0);
        mdd = `${maxDrawdown.toFixed(2)}%`;
    }
    
    // 승률 계산: 거래 번호별로 순손익을 합산하여 한 거래로 취급
    // 거래 번호별 순손익 합계를 저장할 객체
    const tradeNumberToPnL: { [key: number]: number } = {};
    
    // 각 거래 항목의 순손익을 거래 번호별로 합산
    actualTrades.forEach(trade => {
        const tradeNumber = safeNumber(trade["거래 번호"]);
        
        if (tradeNumber === 0) return; // 0번 거래 제외
        
        const netPnL = safeNumber(trade["순손익"]);
        
        // 해당 거래 번호가 객체에 없으면 0으로 초기화
        if (!tradeNumberToPnL[tradeNumber]) {
            tradeNumberToPnL[tradeNumber] = 0;
        }
        
        // 해당 거래 번호의 순손익 합산
        tradeNumberToPnL[tradeNumber] += netPnL;
    });
    
    // 순손익 합계가 0 이상인 거래 번호는 수익 거래, 미만인 거래 번호는 손실 거래로 카운트
    let profitTradeCount = 0;
    let totalTradeCount = 0;
    
    Object.values(tradeNumberToPnL).forEach(totalPnL => {
        totalTradeCount++;
        if (totalPnL >= 0) {
            profitTradeCount++;
        }
    });
    
    // 승률 계산
    const winRate = totalTradeCount > 0 
        ? `${((profitTradeCount / totalTradeCount) * 100).toFixed(2)}%` 
        : "0%";
    
    // 손익비 계산 - 전달받은 avgMetrics 사용
    const avgProfit = avgMetrics.avgProfit;
    const avgLoss = avgMetrics.avgLoss;
    
    // 손익비 계산을 위한 숫자 타입 변수 초기화
    let avgProfitNumber = 0;
    let avgLossNumber = 0;
    
    // avgProfit 타입 체크 후 숫자 변환
    if (typeof avgProfit === 'number') {
        avgProfitNumber = avgProfit;
    }
    
    // avgLoss 타입 체크 후 숫자 변환 (절대값 사용)
    if (typeof avgLoss === 'number') {
        avgLossNumber = Math.abs(avgLoss);
    }
    
    // 손익비 계산
    let profitFactor: string;
    if (avgLossNumber > 0) {
        // avgProfit도 숫자인 경우에만 계산
        profitFactor = avgProfitNumber > 0 
            ? (avgProfitNumber / avgLossNumber).toFixed(2)
            : "0.00"; // 평균 수익이 0 이하면 0.00으로 처리
    } else {
        // 손실이 없으면 평균 수익이 0 초과일 경우 무한대, 아니면 0.00
        profitFactor = avgProfitNumber > 0 ? "∞" : "0.00";
    }
    
    return {
        mdd,
        winRate,
        profitFactor
    };
}; 