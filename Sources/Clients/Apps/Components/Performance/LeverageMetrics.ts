import {TradeItem} from "@/Components/TradeFilter/TradeFilterContext";
import {safeNumber} from './Utils.ts';

export interface LeverageMetricsResult {
    average: string;
    max: string;
    min: string;
}

export const calculateLeverageMetrics = (
    actualTrades: TradeItem[]
): LeverageMetricsResult => {
    // 거래 번호별로 첫 번째 레버리지 값만 사용 (같은 거래 번호는 한 거래로 취급)
    const uniqueTradeLeverages = new Map<number, number>();

    actualTrades.forEach(trade => {
        const tradeNumber = safeNumber(trade["거래 번호"]);
        const leverage = safeNumber(trade["레버리지"]);

        // 0번 거래 번호 제외 및 레버리지가 유효한 값인지 확인
        if (tradeNumber !== 0 && leverage > 0) {
            // 해당 거래 번호가 아직 Map에 없을 때만 추가 (첫 번째 값만 사용)
            if (!uniqueTradeLeverages.has(tradeNumber)) {
                uniqueTradeLeverages.set(tradeNumber, leverage);
            }
        }
    });

    // Map의 values()를 배열로 변환
    const leverages = Array.from(uniqueTradeLeverages.values());

    if (leverages.length === 0) {
        return {average: '-', max: '-', min: '-'};
    }

    const sum = leverages.reduce((acc, val) => acc + val, 0);
    // 평균 계산 후 반올림
    const average = Math.round(sum / leverages.length);
    const max = Math.max(...leverages);
    const min = Math.min(...leverages);

    return {
        average: Math.round(average).toString(),
        max: Math.round(max).toString(),
        min: Math.round(min).toString()
    };
};
