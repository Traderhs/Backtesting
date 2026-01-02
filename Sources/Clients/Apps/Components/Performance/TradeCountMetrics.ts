import {TradeItem} from "@/Components/TradeFilter/TradeFilterContext";
import {safeNumber} from './Utils.ts';

export interface TradeCountMetricsResult {
    totalTradeCount: string;
    profitTradeCount: string;
    lossTradeCount: string;
    liquidationCount: string;
    dailyTradeCount: string;
    weeklyTradeCount: string;
    monthlyTradeCount: string;
    yearlyTradeCount: string;
}

export const calculateTradeCountMetrics = (
    actualTrades: TradeItem[],
    startDate: Date | null,
    endDate: Date | null
): TradeCountMetricsResult => {
    // 거래 데이터가 없는 경우 기본값 반환
    if (actualTrades.length === 0 || !startDate || !endDate) {
        return {
            totalTradeCount: "0",
            dailyTradeCount: "-",
            weeklyTradeCount: "-",
            monthlyTradeCount: "-",
            yearlyTradeCount: "-",
            profitTradeCount: "0",
            lossTradeCount: "0",
            liquidationCount: "0"
        };
    }

    // 1. 총 거래 횟수 계산 - 같은 "거래 번호"는 한 번으로 카운트 (중복 제거)
    const uniqueTradeNumbers = new Set<number>();
    actualTrades.forEach(trade => {
        const tradeNumber = safeNumber(trade["거래 번호"]);
        if (tradeNumber !== 0) { // 0번 거래 번호 제외
            uniqueTradeNumbers.add(tradeNumber);
        }
    });
    const totalTradeCount = uniqueTradeNumbers.size;

    // 2. 수익/손실 거래 횟수 - 같은 "거래 번호"는 한 번으로 카운트하고 순손익을 합산하여 판단
    // 거래 번호별 순손익 합계를 저장할 객체
    const tradeNumberToPnL: { [key: number]: number } = {};

    // 각 거래 항목의 순손익을 거래 번호별로 합산
    actualTrades.forEach(trade => {
        const tradeNumber = safeNumber(trade["거래 번호"]);
        if (tradeNumber !== 0) { // 0번 거래 번호 제외
            const pnl = safeNumber(trade["순손익"]);

            // 해당 거래 번호가 객체에 없으면 0으로 초기화
            if (!tradeNumberToPnL[tradeNumber]) {
                tradeNumberToPnL[tradeNumber] = 0;
            }

            // 해당 거래 번호의 순손익 합산
            tradeNumberToPnL[tradeNumber] += pnl;
        }
    });

    // 순손익 합계가 0 이상인 거래 번호는 수익 거래, 미만인 거래 번호는 손실 거래로 카운트
    let profitTradeCount = 0;
    let lossTradeCount = 0;

    Object.values(tradeNumberToPnL).forEach(totalPnL => {
        if (totalPnL >= 0) {
            profitTradeCount += 1;
        } else {
            lossTradeCount += 1;
        }
    });

    // 3. 강제 청산 횟수 - "청산 이름"에 "강제 청산" 단어가 포함된 거래 카운트 (거래 번호 상관 없음)
    const liquidationCount = actualTrades.filter(trade => {
        const exitName = String(trade["청산 이름"] || "");
        return exitName.includes("강제 청산");
    }).length;

    // 4. 기간별 평균 거래 횟수 계산
    // 4.1 총 일수 계산
    const totalDays = Math.max(1, (endDate.getTime() - startDate.getTime()) / (1000 * 60 * 60 * 24));

    // 4.2 각 기간별 평균 거래 횟수 계산
    // 일간 평균은 항상 계산
    const dailyTradeCount = (totalTradeCount / totalDays).toFixed(2);

    // 주간 평균: 최소 7일 이상 거래 기간 필요
    const weeklyTradeCount = totalDays >= 7
        ? (totalTradeCount / (totalDays / 7)).toFixed(2)
        : "-";

    // 월간 평균: 최소 약 30.44일 이상 거래 기간 필요
    const monthlyTradeCount = totalDays >= 30.4375
        ? (totalTradeCount / (totalDays / 30.4375)).toFixed(2)
        : "-";

    // 연간 평균: 최소 약 365.25일 이상 거래 기간 필요
    const yearlyTradeCount = totalDays >= 365.25
        ? (totalTradeCount / (totalDays / 365.25)).toFixed(2)
        : "-";

    return {
        totalTradeCount: totalTradeCount.toString(),
        dailyTradeCount,
        weeklyTradeCount,
        monthlyTradeCount,
        yearlyTradeCount,
        profitTradeCount: profitTradeCount.toString(),
        lossTradeCount: lossTradeCount.toString(),
        liquidationCount: liquidationCount.toString()
    };
};
