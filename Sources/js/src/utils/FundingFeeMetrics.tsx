import { formatDollar } from '../components/Performance/Utils';

export interface FundingFeeMetrics {
    totalFundingCount: string;
    receivedFundingCount: string;
    paidFundingCount: string;
    totalFundingFee: string;
    receivedFundingFee: string;
    paidFundingFee: string;
    avgFundingFee: string;
    avgReceivedFundingFee: string;
    avgPaidFundingFee: string;
}

export const calculateFundingFeeMetrics = (trades: any[]): FundingFeeMetrics => {
    if (!trades || trades.length === 0) {
        const placeholder = "-";
        return {
            totalFundingCount: placeholder,
            receivedFundingCount: placeholder,
            paidFundingCount: placeholder,
            totalFundingFee: placeholder,
            receivedFundingFee: placeholder,
            paidFundingFee: placeholder,
            avgFundingFee: placeholder,
            avgReceivedFundingFee: placeholder,
            avgPaidFundingFee: placeholder,
        };
    }

    let totalFunding = 0;
    let receivedFunding = 0;
    let paidFunding = 0;
    let receivedCount = 0;
    let paidCount = 0;
    let totalCount = 0;

    // 거래 번호별로 그룹화
    const tradeGroups = new Map<string, any[]>();
    
    trades.forEach(trade => {
        const tradeNumber = String(trade["거래 번호"] || "");
        if (!tradeGroups.has(tradeNumber)) {
            tradeGroups.set(tradeNumber, []);
        }
        tradeGroups.get(tradeNumber)!.push(trade);
    });

    // 각 거래 번호별로 처리
    tradeGroups.forEach((groupTrades) => {
        // 해당 거래 번호에서 펀딩 횟수가 최대인 거래 찾기
        let maxFundingCountTrade = null;
        let maxFundingCount = 0;
        
        groupTrades.forEach(trade => {
            const fundingCount = parseInt(String(trade["펀딩 횟수"] || 0));
            if (!isNaN(fundingCount) && fundingCount > maxFundingCount) {
                maxFundingCount = fundingCount;
                maxFundingCountTrade = trade;
            }
        });
        
        // 최대 펀딩 횟수를 가진 거래에서 펀딩비와 횟수 추출
        if (maxFundingCountTrade && maxFundingCount > 0) {
            const fundingFee = parseFloat(String(maxFundingCountTrade["펀딩비"] || 0));
            
            if (!isNaN(fundingFee)) {
                totalFunding += fundingFee;
                totalCount += maxFundingCount;
                
                if (fundingFee > 0) {
                    receivedFunding += fundingFee;
                    receivedCount += maxFundingCount;
                } else if (fundingFee < 0) {
                    paidFunding += fundingFee;
                    paidCount += maxFundingCount;
                }
            }
        }
    });

    return {
        totalFundingCount: totalCount.toLocaleString('en-US') + "회",
        receivedFundingCount: receivedCount.toLocaleString('en-US') + "회",
        paidFundingCount: paidCount.toLocaleString('en-US') + "회",
        totalFundingFee: formatDollar(totalFunding),
        receivedFundingFee: formatDollar(receivedFunding),
        paidFundingFee: formatDollar(paidFunding),
        avgFundingFee: totalCount > 0 ? formatDollar(totalFunding / totalCount) : "-",
        avgReceivedFundingFee: receivedCount > 0 ? formatDollar(receivedFunding / receivedCount) : "-",
        avgPaidFundingFee: paidCount > 0 ? formatDollar(paidFunding / paidCount) : "-",
    };
};
