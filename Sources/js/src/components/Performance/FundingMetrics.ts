export const calculateFundingMetrics = (trades: any[]) => {
    if (!trades || trades.length === 0) {
        return {
            totalFundingCount: 0,
            receivedFundingCount: 0,
            paidFundingCount: 0,
        };
    }

    // 거래 번호별로 그룹화
    const groupedByTradeNumber = trades.reduce((groups, trade) => {
        const tradeNumber = trade['거래 번호'];

        if (!groups[tradeNumber]) {
            groups[tradeNumber] = [];
        }

        groups[tradeNumber].push(trade);

        return groups;
    }, {} as Record<string, any[]>);

    let totalFundingCount = 0;
    let receivedFundingCount = 0;
    let paidFundingCount = 0;

    // 각 거래 번호별로 처리
    Object.values(groupedByTradeNumber).forEach((tradeGroup) => {
        // 펀딩 횟수가 가장 많은 거래를 선택
        // 펀딩 횟수가 같은 경우 청산 시간이 더 큰(최신) 거래를 선택
        const selectedTrade = (tradeGroup as any[]).reduce((best: any, current: any) => {
            const currentFundingCount = Number(current['펀딩 횟수']) || 0;
            const bestFundingCount = Number(best['펀딩 횟수']) || 0;

            if (currentFundingCount > bestFundingCount) {
                return current;
            } else if (currentFundingCount === bestFundingCount) {
                // 청산 시간이 더 큰(최신) 거래를 선택
                const currentExitTime = current['청산 시간'];
                const bestExitTime = best['청산 시간'];

                return currentExitTime > bestExitTime ? current : best;
            }
            return best;
        });

        // 선택된 거래의 펀딩 정보를 누적
        totalFundingCount += Number(selectedTrade['펀딩 횟수']) || 0;
        receivedFundingCount += Number(selectedTrade['펀딩 수령 횟수']) || 0;
        paidFundingCount += Number(selectedTrade['펀딩 지불 횟수']) || 0;
    });

    // 펀딩비 합계 및 평균 계산 (모든 거래 단순 합산)
    let totalFundingFeeSum = 0;
    let receivedFundingFeeSum = 0;
    let paidFundingFeeSum = 0;

    trades.forEach(trade => {
        totalFundingFeeSum += Number(trade['펀딩비']) || 0;
        receivedFundingFeeSum += Number(trade['펀딩비 수령']) || 0;
        paidFundingFeeSum += Number(trade['펀딩비 지불']) || 0;
    });

    const avgTotalFundingFee = trades.length > 0 ? totalFundingFeeSum / trades.length : 0;
    const avgReceivedFundingFee = trades.length > 0 ? receivedFundingFeeSum / trades.length : 0;
    const avgPaidFundingFee = trades.length > 0 ? paidFundingFeeSum / trades.length : 0;

    return {
        totalFundingCount,
        receivedFundingCount,
        paidFundingCount,
        totalFundingFeeSum,
        receivedFundingFeeSum,
        paidFundingFeeSum,
        avgTotalFundingFee,
        avgReceivedFundingFee,
        avgPaidFundingFee,
    };
};
