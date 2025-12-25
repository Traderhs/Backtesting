import {TradeItem} from "@/components/TradeFilter/TradeFilterContext";
import {formatDateTimeWithWeekday, parseDate, safeNumber} from './Utils.ts';

export interface MaxRateResult {
    value: string;
    pnl: string;
    time: string;
}

export interface MaxProfitLossRateMetricsResult {
    maxIndividualProfitRate: MaxRateResult;
    maxTotalProfitRate: MaxRateResult;
    maxIndividualLossRate: MaxRateResult;
    maxTotalLossRate: MaxRateResult;
}

export const calculateMaxProfitLossRateMetrics = (
    actualTrades: TradeItem[]
): MaxProfitLossRateMetricsResult => {
    const initialResult: MaxRateResult = {
        value: '0.00%',
        pnl: '0',
        time: '-',
    };

    if (!actualTrades || actualTrades.length === 0) {
        return {
            maxIndividualProfitRate: {...initialResult},
            maxTotalProfitRate: {...initialResult},
            maxIndividualLossRate: {...initialResult},
            maxTotalLossRate: {...initialResult},
        };
    }

    let maxIndividualProfitTrade = null;
    let maxTotalProfitTrade = null;
    let maxIndividualLossTrade = null;
    let maxTotalLossTrade = null;

    let maxIndividualProfitRate = -Infinity;
    let maxTotalProfitRate = -Infinity;
    let maxIndividualLossRate = 0;
    let maxTotalLossRate = 0;

    // 음수 거래가 있는지 체크하는 플래그
    let hasNegativeIndividualRate = false;
    let hasNegativeTotalRate = false;
    let hasPositiveIndividualRate = false;
    let hasPositiveTotalRate = false;

    for (const trade of actualTrades) {
        // 숫자로 변환하여 사용
        const individualRate = safeNumber(trade["개별 순손익률"]);
        const totalRate = safeNumber(trade["전체 순손익률"]);
        const tradeDate = parseDate(String(trade["청산 시간"]));

        // 최대 개별 순수익률 거래 업데이트 (0과 같거나 큰 거래만) (같은 값일 경우 더 최신 거래 선택)
        if (individualRate >= 0) {
            hasPositiveIndividualRate = true;
            if (individualRate > maxIndividualProfitRate ||
                (individualRate === maxIndividualProfitRate &&
                    tradeDate &&
                    maxIndividualProfitTrade &&
                    parseDate(String(maxIndividualProfitTrade["청산 시간"])) !== null &&
                    tradeDate > parseDate(String(maxIndividualProfitTrade["청산 시간"]))!)) {
                maxIndividualProfitRate = individualRate;
                maxIndividualProfitTrade = trade;
            }
        }

        // 최대 전체 순수익률 거래 업데이트 (0과 같거나 큰 거래만) (같은 값일 경우 더 최신 거래 선택)
        if (totalRate >= 0) {
            hasPositiveTotalRate = true;
            if (totalRate > maxTotalProfitRate ||
                (totalRate === maxTotalProfitRate &&
                    tradeDate &&
                    maxTotalProfitTrade &&
                    parseDate(String(maxTotalProfitTrade["청산 시간"])) !== null &&
                    tradeDate > parseDate(String(maxTotalProfitTrade["청산 시간"]))!)) {
                maxTotalProfitRate = totalRate;
                maxTotalProfitTrade = trade;
            }
        }

        // 최대 개별 순손실률 거래 업데이트 (손실은 음수여야 함) (같은 값일 경우 더 최신 거래 선택)
        if (individualRate < 0 && (individualRate < maxIndividualLossRate ||
            (individualRate === maxIndividualLossRate &&
                tradeDate &&
                maxIndividualLossTrade &&
                parseDate(String(maxIndividualLossTrade["청산 시간"])) !== null &&
                tradeDate > parseDate(String(maxIndividualLossTrade["청산 시간"]))!))) {
            maxIndividualLossRate = individualRate;
            maxIndividualLossTrade = trade;
            hasNegativeIndividualRate = true;
        }

        // 최대 전체 순손실률 거래 업데이트 (손실은 음수여야 함) (같은 값일 경우 더 최신 거래 선택)
        if (totalRate < 0 && (totalRate < maxTotalLossRate ||
            (totalRate === maxTotalLossRate &&
                tradeDate &&
                maxTotalLossTrade &&
                parseDate(String(maxTotalLossTrade["청산 시간"])) !== null &&
                tradeDate > parseDate(String(maxTotalLossTrade["청산 시간"]))!))) {
            maxTotalLossRate = totalRate;
            maxTotalLossTrade = trade;
            hasNegativeTotalRate = true;
        }
    }

    const formatResult = (trade: TradeItem | null, rateKey: string): MaxRateResult => {
        if (!trade) return {...initialResult};
        return {
            value: `${safeNumber(trade[rateKey]).toFixed(2)}%`,
            pnl: safeNumber(trade["순손익"]).toLocaleString(),
            time: formatDateTimeWithWeekday(parseDate(String(trade["청산 시간"]))),
        };
    };

    return {
        maxIndividualProfitRate: hasPositiveIndividualRate
            ? formatResult(maxIndividualProfitTrade, "개별 순손익률")
            : {value: '-', pnl: '-', time: '-'},
        maxTotalProfitRate: hasPositiveTotalRate
            ? formatResult(maxTotalProfitTrade, "전체 순손익률")
            : {value: '-', pnl: '-', time: '-'},
        maxIndividualLossRate: hasNegativeIndividualRate
            ? formatResult(maxIndividualLossTrade, "개별 순손익률")
            : {value: '-', pnl: '-', time: '-'},
        maxTotalLossRate: hasNegativeTotalRate
            ? formatResult(maxTotalLossTrade, "전체 순손익률")
            : {value: '-', pnl: '-', time: '-'},
    };
};
