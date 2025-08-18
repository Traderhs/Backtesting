import {TradeItem} from "@/components/TradeFilter/TradeFilterContext";
import {formatDuration, formatPercent} from './Utils.ts';

export interface DrawdownMetricsResult {
    avgDepth: string;                 // 드로우다운 평균 깊이
    totalDownsideDuration: string;    // 드로우다운 합계 하락 기간
    avgDownsideDuration: string;      // 드로우다운 평균 하락 기간
    maxDownsideDuration: string;      // 드로우다운 최대 하락 기간
    downsideDurationRatio: string;    // 드로우다운 하락 기간 비율
    totalRecoveryDuration: string;    // 드로우다운 합계 회복 기간
    avgRecoveryDuration: string;      // 드로우다운 평균 회복 기간
    maxRecoveryDuration: string;      // 드로우다운 최대 회복 기간
    recoveryDurationRatio: string;    // 드로우다운 회복 기간 비율
}

/**
 * 드로우다운 관련 통계를 계산하는 함수
 * @param actualTrades 실제 거래 내역 (0번 거래 제외)
 * @param startDate 거래 시작 날짜
 * @param endDate 거래 종료 날짜
 */
export const calculateDrawdownMetrics = (
    actualTrades: TradeItem[],
    startDate: Date | null,
    endDate: Date | null
): DrawdownMetricsResult => {
    // 거래 번호별로 마지막 항목의 드로우다운 값만 사용
    const tradeNumberToDrawdown: Record<number, {
        trade: TradeItem,
        drawdown: number
    }> = {};

    // 각 거래 번호별로 마지막 항목 찾기
    actualTrades.forEach(trade => {
        const tradeNumber = Number(trade["거래 번호"]);
        
        // 0번 거래(초기값)는 건너뜀
        if (tradeNumber === 0) return;

        const drawdown = Number(trade["드로우다운"] || 0);

        // 해당 거래 번호가 없거나, 현재 항목의 청산 시간이 더 최근이면 업데이트
        if (!tradeNumberToDrawdown[tradeNumber] ||
            new Date(String(trade["청산 시간"])) >
            new Date(String(tradeNumberToDrawdown[tradeNumber].trade["청산 시간"]))) {
            tradeNumberToDrawdown[tradeNumber] = {trade, drawdown};
        }
    });

    // 드로우다운 계산을 위한 변수들
    const depthList: number[] = [];
    const downsideList: number[] = [];
    const recoveryList: number[] = [];

    let ddDownsideStartTime: Date | null = null;
    let ddDownsideEndTime: Date | null = null;
    let currentDepth = 0;
    let wasInDrawdown = false;

    // 청산 시간 순으로 정렬된 거래 생성
    const sortedTrades = Object.values(tradeNumberToDrawdown)
        .map(item => item.trade)
        .sort((a, b) => {
            const timeA = new Date(String(a["청산 시간"])).getTime();
            const timeB = new Date(String(b["청산 시간"])).getTime();
            return timeA - timeB;
        });

    // 정렬된 거래가 없으면 빈 결과 반환
    if (sortedTrades.length === 0) {
        return {
            avgDepth: '-',
            totalDownsideDuration: '-',
            avgDownsideDuration: '-',
            maxDownsideDuration: '-',
            downsideDurationRatio: '-',
            totalRecoveryDuration: '-',
            avgRecoveryDuration: '-',
            maxRecoveryDuration: '-',
            recoveryDurationRatio: '-'
        };
    }

    // 드로우다운 통계 계산
    for (let i = 0; i < sortedTrades.length; i++) {
        const trade = sortedTrades[i];
        const dd = Number(trade["드로우다운"] || 0);

        // 드로우다운이 0보다 큰 경우 (드로우다운 상태)
        if (dd > 0) {
            wasInDrawdown = true;
            
            // 드로우다운이 시작되지 않았으면 시작
            if (ddDownsideStartTime === null) {
                ddDownsideStartTime = new Date(String(trade["청산 시간"]));
            }

            // 현재 깊이보다 더 깊은 드로우다운이면 업데이트
            if (dd > currentDepth) {
                currentDepth = dd;
                ddDownsideEndTime = new Date(String(trade["청산 시간"]));
            }
        } 
        // 드로우다운이 0이고 이전에 드로우다운 상태였으면
        else if (dd === 0 && wasInDrawdown) {
            // 드로우다운 정보가 있는 경우에만 처리
            if (ddDownsideStartTime !== null && ddDownsideEndTime !== null && currentDepth > 0) {
                depthList.push(currentDepth);

                // 하락 기간 계산 (초 단위)
                const downsideDuration =
                    (ddDownsideEndTime.getTime() - ddDownsideStartTime.getTime()) / 1000;
                downsideList.push(downsideDuration);

                // 회복 기간 계산 (초 단위)
                const recoveryDuration =
                    (new Date(String(trade["청산 시간"])).getTime() - ddDownsideStartTime.getTime()) / 1000;
                recoveryList.push(recoveryDuration);

                // 변수 초기화
                ddDownsideStartTime = null;
                ddDownsideEndTime = null;
                currentDepth = 0;
                wasInDrawdown = false;
            }
        }
    }

    // 마지막 거래가 드로우다운 상태인 경우 처리
    if (wasInDrawdown && ddDownsideStartTime !== null && ddDownsideEndTime !== null && currentDepth > 0) {
        depthList.push(currentDepth);
        
        // 하락 기간 계산
        const downsideDuration = 
            (ddDownsideEndTime.getTime() - ddDownsideStartTime.getTime()) / 1000;
        downsideList.push(downsideDuration);
        
        // 아직 회복 중이므로 recoveryList에 추가하지 않음
    }

    // 통계 계산
    const avgDepth = depthList.length > 0
        ? depthList.reduce((sum, val) => sum + val, 0) / depthList.length
        : '-';

    const avgDownsideDuration = downsideList.length > 0
        ? downsideList.reduce((sum, val) => sum + val, 0) / downsideList.length
        : '-';

    const maxDownsideDuration = downsideList.length > 0
        ? Math.max(...downsideList)
        : '-';

    const totalDownsideDuration = downsideList.reduce((sum, val) => sum + val, 0);

    const avgRecoveryDuration = recoveryList.length > 0
        ? recoveryList.reduce((sum, val) => sum + val, 0) / recoveryList.length
        : '-';

    const maxRecoveryDuration = recoveryList.length > 0
        ? Math.max(...recoveryList)
        : '-';

    const totalRecoveryDuration = recoveryList.reduce((sum, val) => sum + val, 0);

    // 전체 거래 기간 계산 (초 단위)
    let totalTradingDuration = 0;
    if (startDate && endDate) {
        totalTradingDuration = (endDate.getTime() - startDate.getTime()) / 1000;
    }

    // 비율 계산
    const downsideDurationRaw = totalTradingDuration > 0 ? (totalDownsideDuration / totalTradingDuration) * 100 : 0;
    const recoveryDurationRaw = totalTradingDuration > 0 ? (totalRecoveryDuration / totalTradingDuration) * 100 : 0;

    const downsideDurationRatio = totalTradingDuration > 0 && downsideDurationRaw > 0
        ? formatPercent(downsideDurationRaw)
        : '-';

    const recoveryDurationRatio = totalTradingDuration > 0 && recoveryDurationRaw > 0
        ? formatPercent(recoveryDurationRaw)
        : '-';

    return {
        avgDepth: formatPercent(avgDepth),
        totalDownsideDuration: totalDownsideDuration !== 0 ? formatDuration(new Date(0), new Date(totalDownsideDuration * 1000)) : '-',
        avgDownsideDuration: avgDownsideDuration !== '-' ? formatDuration(new Date(0), new Date(avgDownsideDuration * 1000)) : '-',
        maxDownsideDuration: maxDownsideDuration !== '-' ? formatDuration(new Date(0), new Date(maxDownsideDuration * 1000)) : '-',
        downsideDurationRatio: downsideDurationRatio,
        totalRecoveryDuration: totalRecoveryDuration !== 0 ? formatDuration(new Date(0), new Date(totalRecoveryDuration * 1000)) : '-',
        avgRecoveryDuration: avgRecoveryDuration !== '-' ? formatDuration(new Date(0), new Date(avgRecoveryDuration * 1000)) : '-',
        maxRecoveryDuration: maxRecoveryDuration !== '-' ? formatDuration(new Date(0), new Date(maxRecoveryDuration * 1000)) : '-',
        recoveryDurationRatio: recoveryDurationRatio
    };
}; 