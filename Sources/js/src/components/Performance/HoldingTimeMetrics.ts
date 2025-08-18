import { formatDuration, parseDate } from './Utils.ts';
import { parseHoldingTime } from '../TradeFilter/ParseHoldingTime';

interface HoldingTimeMetrics {
    avgHoldingTime: string;
    profitAvgHoldingTime: string;
    lossAvgHoldingTime: string;
    maxProfitHoldingTime: string;
    maxLossHoldingTime: string;
    totalHoldingTime: string;
    profitTotalHoldingTime: string;
    lossTotalHoldingTime: string;
    marketParticipationRate: string;
    profitMarketParticipationRate: string;
    lossMarketParticipationRate: string;
}

// 시간 범위를 나타내는 인터페이스
interface TimeRange {
    start: number;
    end: number;
}

// 시간 범위들을 병합하여 중복 제거하는 함수
const mergeTimeRanges = (ranges: TimeRange[]): TimeRange[] => {
    if (ranges.length === 0) return [];
    
    // 시작 시간 기준으로 정렬
    ranges.sort((a, b) => a.start - b.start);
    
    const merged: TimeRange[] = [ranges[0]];
    
    for (let i = 1; i < ranges.length; i++) {
        const current = ranges[i];
        const lastMerged = merged[merged.length - 1];
        
        // 현재 범위가 마지막 병합된 범위와 겹치거나 인접한 경우
        if (current.start <= lastMerged.end) {
            // 병합: 끝 시간을 더 큰 값으로 설정
            lastMerged.end = Math.max(lastMerged.end, current.end);
        } else {
            // 겹치지 않으면 새로운 범위로 추가
            merged.push(current);
        }
    }
    
    return merged;
};

// 병합된 시간 범위들의 총 시간을 계산하는 함수
const calculateTotalTimeFromRanges = (ranges: TimeRange[]): number => {
    return ranges.reduce((total, range) => {
        return total + (range.end - range.start);
    }, 0);
};

// 백테스팅 기간을 계산하는 함수
const calculateBacktestingPeriod = (config: any): number => {
    if (!config?.["엔진 설정"]?.["백테스팅 기간"]) {
        return 0;
    }
    
    const backtestingPeriod = config["엔진 설정"]["백테스팅 기간"];
    const startStr = backtestingPeriod["시작"];
    const endStr = backtestingPeriod["종료"];
    
    if (!startStr || !endStr) {
        return 0;
    }
    
    const startDate = parseDate(startStr);
    const endDate = parseDate(endStr);
    
    if (!startDate || !endDate) {
        return 0;
    }
    
    // 밀리초 차이를 반환
    return endDate.getTime() - startDate.getTime();
};

export const calculateHoldingTimeMetrics = (trades: any[], config?: any): HoldingTimeMetrics => {
    if (!trades || trades.length === 0) {
        return {
            avgHoldingTime: "-",
            profitAvgHoldingTime: "-",
            lossAvgHoldingTime: "-",
            maxProfitHoldingTime: "-",
            maxLossHoldingTime: "-",
            totalHoldingTime: "-",
            profitTotalHoldingTime: "-",
            lossTotalHoldingTime: "-",
            marketParticipationRate: "-",
            profitMarketParticipationRate: "-",
            lossMarketParticipationRate: "-"
        };
    }

    // 모든 거래, 수익 거래, 손실 거래 분리
    const allValidTrades = trades.filter(trade => trade["보유 시간"] !== undefined);
    const profitTrades = trades.filter(trade => 
        parseFloat(String(trade["순손익"])) > 0
    );
    const lossTrades = trades.filter(trade => 
        parseFloat(String(trade["순손익"])) <= 0
    );

    // 백테스팅 전체 기간 계산 (밀리초)
    const backtestingPeriodMs = calculateBacktestingPeriod(config);

    // 초를 사람이 읽을 수 있는 시간 형식으로 변환 (formatDuration 사용)
    const secondsToTime = (seconds: number): string => {
        if (seconds === 0) return "-";
        return formatDuration(new Date(0), new Date(seconds * 1000));
    };

    // 평균 계산 함수
    const calculateAverage = (tradeArray: any[]): string => {
        if (tradeArray.length === 0) return "-";
        
        let totalSeconds = 0;
        let validTradeCount = 0;
        
        tradeArray.forEach(trade => {
            const seconds = parseHoldingTime(String(trade["보유 시간"]));
            if (seconds !== null) {
                totalSeconds += seconds;
                validTradeCount++;
            }
        });
        
        if (validTradeCount === 0) return "-";
        return secondsToTime(Math.floor(totalSeconds / validTradeCount));
    };

    // 최대값 계산 함수
    const calculateMaximum = (tradeArray: any[]): string => {
        if (tradeArray.length === 0) return "-";
        
        let maxSeconds = 0;
        
        tradeArray.forEach(trade => {
            const seconds = parseHoldingTime(String(trade["보유 시간"]));
            if (seconds !== null && seconds > maxSeconds) {
                maxSeconds = seconds;
            }
        });
        
        if (maxSeconds === 0) return "-";
        return secondsToTime(maxSeconds);
    };

    // 시간 범위 병합을 이용한 합계 계산 함수 (밀리초 반환)
    const calculateTotalWithMergeMs = (tradeArray: any[]): number => {
        if (tradeArray.length === 0) return 0;
        
        const timeRanges: TimeRange[] = [];
        
        tradeArray.forEach(trade => {
            const entryTimeStr = String(trade["진입 시간"]);
            const exitTimeStr = String(trade["청산 시간"]);
            
            if (entryTimeStr && exitTimeStr) {
                const entryTime = parseDate(entryTimeStr);
                const exitTime = parseDate(exitTimeStr);
                
                if (entryTime && exitTime) {
                    timeRanges.push({
                        start: entryTime.getTime(),
                        end: exitTime.getTime()
                    });
                }
            }
        });
        
        if (timeRanges.length === 0) return 0;
        
        // 시간 범위 병합하여 중복 제거
        const mergedRanges = mergeTimeRanges(timeRanges);
        
        // 총 시간 계산 (밀리초)
        return calculateTotalTimeFromRanges(mergedRanges);
    };

    // 시간 범위 병합을 이용한 합계 계산 함수 (문자열 반환)
    const calculateTotalWithMerge = (tradeArray: any[]): string => {
        const totalMilliseconds = calculateTotalWithMergeMs(tradeArray);
        
        if (totalMilliseconds === 0) return "-";
        
        // 초로 변환
        const totalSeconds = Math.floor(totalMilliseconds / 1000);
        return secondsToTime(totalSeconds);
    };

    // 시장 참여 비율 계산 함수
    const calculateMarketParticipationRate = (tradeArray: any[]): string => {
        if (tradeArray.length === 0 || backtestingPeriodMs === 0) return "-";
        
        const totalHoldingMs = calculateTotalWithMergeMs(tradeArray);
        
        if (totalHoldingMs === 0) return "-";
        
        // 비율 계산 (퍼센트)
        const rate = (totalHoldingMs / backtestingPeriodMs) * 100;
        
        // 소수점 2자리까지 표시
        return rate.toFixed(2) + "%";
    };

    return {
        avgHoldingTime: calculateAverage(allValidTrades),
        profitAvgHoldingTime: calculateAverage(profitTrades),
        lossAvgHoldingTime: calculateAverage(lossTrades),
        maxProfitHoldingTime: calculateMaximum(profitTrades),
        maxLossHoldingTime: calculateMaximum(lossTrades),
        totalHoldingTime: calculateTotalWithMerge(allValidTrades),
        profitTotalHoldingTime: calculateTotalWithMerge(profitTrades),
        lossTotalHoldingTime: calculateTotalWithMerge(lossTrades),
        marketParticipationRate: calculateMarketParticipationRate(allValidTrades),
        profitMarketParticipationRate: calculateMarketParticipationRate(profitTrades),
        lossMarketParticipationRate: calculateMarketParticipationRate(lossTrades)
    };
}; 