import {TradeItem} from "@/components/TradeFilter/TradeFilterContext";
import {safeNumber, parseDate, formatDateTimeWithWeekday, calculatePercentage} from './Utils.ts';

export interface BalanceMetricsResult {
    initialBalance: number;
    endingBalance: number;
    highestBalanceValue: number;
    highestBalanceTime: string;
    highestBalancePercentage: string;
    lowestBalanceValue: number;
    lowestBalanceTime: string;
    lowestBalancePercentage: string;
}

export const calculateBalanceMetrics = (
    filteredTrades: TradeItem[]
): BalanceMetricsResult => {
    const actualTrades = filteredTrades.length > 1 ? filteredTrades.slice(1) : [];
    const lastTrade = actualTrades.length > 0 ? actualTrades[actualTrades.length - 1] : null;

    const initialBalance = filteredTrades.length > 0 ? safeNumber(filteredTrades[0]["현재 자금"]) : 0;
    const endingBalance = lastTrade ? safeNumber(lastTrade["현재 자금"]) : initialBalance;

    let highestBalanceValue = initialBalance;
    let lowestBalanceValue = initialBalance;
    let highestBalanceTrade: TradeItem | null = filteredTrades.length > 0 ? filteredTrades[0] : null;
    let lowestBalanceTrade: TradeItem | null = filteredTrades.length > 0 ? filteredTrades[0] : null;

    filteredTrades.forEach((trade: TradeItem) => {
        const currentBalance = safeNumber(trade["현재 자금"]);
        const tradeDate = parseDate(String(trade["청산 시간"]));
        
        // 현재 자금이 최고 자금보다 크거나, 같지만 더 최신인 경우 업데이트
        if (currentBalance > highestBalanceValue || 
            (currentBalance === highestBalanceValue && 
             tradeDate && 
             highestBalanceTrade && 
             parseDate(String(highestBalanceTrade["청산 시간"])) !== null && 
             tradeDate > parseDate(String(highestBalanceTrade["청산 시간"]))!)) {
            highestBalanceValue = currentBalance;
            highestBalanceTrade = trade;
        }
        
        // 현재 자금이 최저 자금보다 작거나, 같지만 더 최신인 경우 업데이트
        if (currentBalance < lowestBalanceValue || 
            (currentBalance === lowestBalanceValue && 
             tradeDate && 
             lowestBalanceTrade && 
             parseDate(String(lowestBalanceTrade["청산 시간"])) !== null && 
             tradeDate > parseDate(String(lowestBalanceTrade["청산 시간"]))!)) {
            lowestBalanceValue = currentBalance;
            lowestBalanceTrade = trade;
        }
    });

    const highestBalanceTime = highestBalanceTrade && Number(highestBalanceTrade["거래 번호"]) !== 0
        ? formatDateTimeWithWeekday(parseDate(String(highestBalanceTrade["청산 시간"])))
        : '-';
    const lowestBalanceTime = lowestBalanceTrade && Number(lowestBalanceTrade["거래 번호"]) !== 0
        ? formatDateTimeWithWeekday(parseDate(String(lowestBalanceTrade["청산 시간"])))
        : '-';

    const highestBalancePercentage = calculatePercentage(highestBalanceValue, initialBalance);
    const lowestBalancePercentage = calculatePercentage(lowestBalanceValue, initialBalance);

    return {
        initialBalance,
        endingBalance,
        highestBalanceValue,
        highestBalanceTime,
        highestBalancePercentage,
        lowestBalanceValue,
        lowestBalanceTime,
        lowestBalancePercentage
    };
}; 