import React, {useState, useEffect} from 'react';
import {useTradeFilter} from '@/components/TradeFilter';
import {calculateBalanceMetrics} from './BalanceMetrics.ts';
import {calculateFeeMetrics} from './FeeMetrics.ts';
import {calculateFundingMetrics} from './FundingMetrics.ts';
import {calculateLeverageMetrics} from './LeverageMetrics.ts';
import {calculateTotalProfitLossMetrics} from './TotalProfitLossMetrics.ts';
import {calculateAverageProfitLossMetrics} from './AverageProfitLossMetrics.ts';
import {calculateMaxProfitLossMetrics} from './MaxProfitLossMetrics.ts';
import {calculateMaxProfitLossRateMetrics} from './MaxProfitLossRateMetrics.ts';
import {calculateAGRMetrics} from './AGRMetrics';
import {calculateRiskRewardMetrics} from './RiskRewardMetrics.ts';
import {calculateRiskAdjustedReturnMetrics} from './RiskAdjustedReturnMetrics.ts';
import {calculateTradeCountMetrics} from './TradeCountMetrics.ts';
import {formatDateTime, parseDate, formatDuration, formatDollar, formatPercent} from './Utils.ts';
import {calculateStreaks} from './Streaks';
import {calculateDrawdownMetrics} from './DrawdownMetrics';
import {calculateHoldingTimeMetrics} from './HoldingTimeMetrics';
import {calculateSymbolCountMetrics} from './SymbolCountMetrics';

export type PerformanceMetrics = {
    tradingPeriod: string;
    startDate: Date | null;
    endDate: Date | null;
    balanceMetrics: ReturnType<typeof calculateBalanceMetrics>;
    feeMetrics: ReturnType<typeof calculateFeeMetrics>;
    fundingMetrics: ReturnType<typeof calculateFundingMetrics>;
    leverageMetrics: ReturnType<typeof calculateLeverageMetrics>;
    totalProfitLossMetrics: ReturnType<typeof calculateTotalProfitLossMetrics>;
    avgProfitLossMetrics: ReturnType<typeof calculateAverageProfitLossMetrics>;
    maxProfitLossMetrics: ReturnType<typeof calculateMaxProfitLossMetrics>;
    maxProfitLossRateMetrics: ReturnType<typeof calculateMaxProfitLossRateMetrics>;
    agrMetrics: ReturnType<typeof calculateAGRMetrics>;
    riskRewardMetrics: ReturnType<typeof calculateRiskRewardMetrics>;
    riskAdjustedReturnMetrics: ReturnType<typeof calculateRiskAdjustedReturnMetrics>;
    tradeCountMetrics: ReturnType<typeof calculateTradeCountMetrics>;
    streaks: {
        maxWinStreak: number;
        maxLoseStreak: number;
    };
    drawdownMetrics: ReturnType<typeof calculateDrawdownMetrics>;
    holdingTimeMetrics: ReturnType<typeof calculateHoldingTimeMetrics>;
    symbolCountMetrics: ReturnType<typeof calculateSymbolCountMetrics>;
};

// 컴포넌트 Props 타입 정의
interface ReportProps {
    onReady?: () => void; // 데이터 준비 완료 시 호출될 콜백
    config?: any; // config 데이터
}

export const calculatePerformanceMetrics = (trades: any[], config?: any): PerformanceMetrics | null => {
    if (!trades || trades.length <= 1) {
        return null;
    }

    // 거래 번호 0번을 제외한 실제 거래
    const actualTrades = trades.length > 1 ? trades.slice(1) : [];

    if (actualTrades.length === 0) {
        return null;
    }

    const firstTrade = actualTrades[0];
    const lastTrade = actualTrades[actualTrades.length - 1];
    const startDate = firstTrade ? parseDate(String(firstTrade["진입 시간"])) : null;
    const endDate = lastTrade ? parseDate(String(lastTrade["청산 시간"])) : null;

    const startDateString = formatDateTime(startDate);
    const endDateString = formatDateTime(endDate);
    const durationString = formatDuration(startDate, endDate);
    const tradingPeriod = `${startDateString} - ${endDateString} (${durationString})`;

    // 분리된 메트릭 계산 함수 사용
    // BalanceMetrics는 초기 자금(trades[0]) 계산을 위해 trades 전체를 사용
    const balanceMetrics = calculateBalanceMetrics(trades);

    // 다른 메트릭 함수들은 실제 거래 내역(actualTrades)을 사용
    const feeMetrics = calculateFeeMetrics(actualTrades);
    const fundingMetrics = calculateFundingMetrics(actualTrades);
    const leverageMetrics = calculateLeverageMetrics(actualTrades);
    const totalProfitLossMetrics = calculateTotalProfitLossMetrics(actualTrades, balanceMetrics.initialBalance);
    const avgProfitLossMetrics = calculateAverageProfitLossMetrics(actualTrades);
    const maxProfitLossMetrics = calculateMaxProfitLossMetrics(actualTrades);
    const maxProfitLossRateMetrics = calculateMaxProfitLossRateMetrics(actualTrades);
    const agrMetrics = calculateAGRMetrics(balanceMetrics.initialBalance, balanceMetrics.endingBalance, startDate, endDate);
    const riskRewardMetrics = calculateRiskRewardMetrics(actualTrades, avgProfitLossMetrics);
    const riskAdjustedReturnMetrics = calculateRiskAdjustedReturnMetrics(
        actualTrades,
        agrMetrics.cagr,
        riskRewardMetrics,
        avgProfitLossMetrics
    );
    const tradeCountMetrics = calculateTradeCountMetrics(
        actualTrades,
        startDate,
        endDate
    );
    const {maxWinStreak, maxLoseStreak} = calculateStreaks(actualTrades);
    const drawdownMetrics = calculateDrawdownMetrics(actualTrades, startDate, endDate);
    const holdingTimeMetrics = calculateHoldingTimeMetrics(actualTrades, config);
    const symbolCountMetrics = calculateSymbolCountMetrics(actualTrades);

    return {
        tradingPeriod,
        startDate,
        endDate,
        balanceMetrics,
        feeMetrics,
        fundingMetrics,
        leverageMetrics,
        totalProfitLossMetrics,
        avgProfitLossMetrics,
        maxProfitLossMetrics,
        maxProfitLossRateMetrics,
        agrMetrics,
        riskRewardMetrics,
        riskAdjustedReturnMetrics,
        tradeCountMetrics,
        streaks: {maxWinStreak, maxLoseStreak},
        drawdownMetrics,
        holdingTimeMetrics,
        symbolCountMetrics
    };
};

const Report: React.FC<ReportProps> = ({onReady, config}) => {
    const {filteredTrades} = useTradeFilter();
    const [metrics, setMetrics] = useState<PerformanceMetrics | null>(null);
    const [hoveredGroup, setHoveredGroup] = useState<number | null>(null);

    // 데이터 계산 함수
    const computeMetrics = () => {
        try {
            if (!filteredTrades || filteredTrades.length <= 1) {
                setMetrics(null);
                return;
            }

            const calculatedMetrics = calculatePerformanceMetrics(filteredTrades, config);
            setMetrics(calculatedMetrics);
        } catch (error) {
            console.error("성과 지표 계산 중 오류 발생:", error);
            setMetrics(null);
        }
    };

    useEffect(() => {
        try {
            // filteredTrades가 유효할 때 계산 수행 
            computeMetrics();
        } finally {
            // 계산 여부와 무관하게 무조건 준비 완료 상태 알림
            if (onReady) {
                onReady();
            }
        }
    }, [filteredTrades, onReady]);

    const placeholder = "-";

    const reportData = {
        '거래 기간': metrics?.tradingPeriod || placeholder,
        '자금 정보': placeholder,
        '최고 자금': placeholder,
        '최저 자금': placeholder,
        '펀딩 횟수': metrics ? `${metrics.fundingMetrics.totalFundingCount.toLocaleString('en-US')}회` : placeholder,
        '펀딩비 합계': metrics ? formatDollar(metrics.fundingMetrics.totalFundingFeeSum || 0) : placeholder,
        '평균 펀딩비': metrics ? formatDollar(metrics.fundingMetrics.avgTotalFundingFee || 0) : placeholder,
        '수수료 합계': placeholder,
        '레버리지': placeholder,
        "순손익 합계": placeholder,
        '순수익 합계': placeholder,
        '순손실 합계': placeholder,
        '평균 순손익': placeholder,
        '평균 순수익': placeholder,
        '평균 순손실': placeholder,
        '최대 순수익': placeholder,
        '최대 순손실': placeholder,
        '최대 개별 순수익률': placeholder,
        '최대 전체 순수익률': placeholder,
        '최대 개별 순손실률': placeholder,
        '최대 전체 순손실률': placeholder,
        'MDD': placeholder,
        '승률': metrics ? formatPercent(metrics.riskRewardMetrics.winRate) : placeholder,
        '손익비': metrics ? (() => {
            const val = metrics.riskRewardMetrics.profitFactor;
            return val === "∞" ? "∞" : (() => {
                const numVal = Number(val);
                return isNaN(numVal) ? '0.00' : numVal.toLocaleString('en-US', {
                    minimumFractionDigits: 2,
                    maximumFractionDigits: 2
                });
            })();
        })() : placeholder,
        '기대값': metrics ? (() => {
            const winRate = parseFloat(metrics.riskRewardMetrics.winRate.replace('%', '')) / 100;
            const lossRate = 1 - winRate;
            const avgWin = Number(metrics.avgProfitLossMetrics.avgProfit);
            const avgLoss = Math.abs(Number(metrics.avgProfitLossMetrics.avgLoss));

            let winPart = 0;
            let lossPart = 0;

            if (!isNaN(avgWin)) {
                winPart = winRate * avgWin;
            }

            if (!isNaN(avgLoss)) {
                lossPart = lossRate * avgLoss;
            }

            const expectancy = winPart - lossPart;
            return formatDollar(expectancy);
        })() : placeholder,
        '성장률': placeholder,
        '수익 효율성 지수': placeholder,
        '진입 횟수': placeholder,
        '평균 진입 횟수': placeholder,
        '강제 청산 횟수': metrics ? (() => {
            const val = Number(metrics.tradeCountMetrics.liquidationCount);
            return isNaN(val) ? '0회' : val.toLocaleString('en-US') + '회';
        })() : placeholder,
        '최대 연속 거래': placeholder,
        '드로우다운 평균 깊이': placeholder,
        '드로우다운 하락 기간': placeholder,
        '드로우다운 회복 기간': placeholder,
        '시장 참여 비율': placeholder,
        '보유 시간 합계': metrics ? metrics.holdingTimeMetrics.totalHoldingTime : placeholder,
        '평균 보유 시간': placeholder,
        '최대 보유 시간': placeholder,
        '보유 심볼 수': placeholder,
    };

    const metricKeys = Object.keys(reportData) as (keyof typeof reportData)[];

    // borderBottom이 적용될 항목들
    const borderBottomMetrics = ['자금 정보', '최고 자금', '펀딩 횟수', '펀딩비 합계', '순손익 합계', '순수익 합계', '평균 순손익', '평균 순수익', '최대 순수익', '최대 개별 순수익률', '최대 개별 순손실률', 'MDD', '승률', '손익비', '진입 횟수', '평균 진입 횟수', '강제 청산 횟수', '드로우다운 평균 깊이', '드로우다운 하락 기간', '시장 참여 비율', '보유 시간 합계', '평균 보유 시간'];

    // 하드코딩된 그룹 정의
    const metricGroups = {
        // 그룹 0: 자금 관련
        0: ['자금 정보', '최고 자금', '최저 자금'],
        // 그룹 1: 펀딩 관련
        1: ['펀딩 횟수', '펀딩비 합계', '평균 펀딩비'],
        // 그룹 2: 수수료
        2: ['수수료 합계'],
        // 그룹 3: 레버리지
        3: ['레버리지'],
        // 그룹 4: 순손익
        4: ['순손익 합계', '순수익 합계', '순손실 합계'],
        // 그룹 5: 평균 손익
        5: ['평균 순손익', '평균 순수익', '평균 순손실'],
        // 그룹 6: 최대 손익
        6: ['최대 순수익', '최대 순손실'],
        // 그룹 7: 개별 수익률
        7: ['최대 개별 순수익률', '최대 전체 순수익률'],
        // 그룹 8: 개별 손실률
        8: ['최대 개별 순손실률', '최대 전체 순손실률'],
        // 그룹 9: MDD 승률 손익비 기대값
        9: ['MDD', '승률', '손익비', '기대값'],
        // 그룹 10: 성장률
        10: ['성장률'],
        // 그룹 11: 수익 효율성
        11: ['수익 효율성 지수'],
        // 그룹 12: 진입 횟수
        12: ['진입 횟수', '평균 진입 횟수', '강제 청산 횟수', '최대 연속 거래'],
        // 그룹 13: 드로우다운
        13: ['드로우다운 평균 깊이', '드로우다운 하락 기간', '드로우다운 회복 기간'],
        // 그룹 14: 시장 참여 보유 시간
        14: ['시장 참여 비율', '보유 시간 합계', '평균 보유 시간', '최대 보유 시간'],
        // 그룹 15: 보유 심볼
        15: ['보유 심볼 수']
    };

    // 그룹 정의: 하드코딩된 그룹에서 항목 찾기
    const getMetricGroup = (metric: string) => {
        for (const [groupId, metrics] of Object.entries(metricGroups)) {
            if (metrics.includes(metric)) {
                return parseInt(groupId);
            }
        }
        return -1;
    };

    const handleMouseEnter = (metric: string) => {
        const group = getMetricGroup(metric);
        if (group !== -1) {
            setHoveredGroup(group);
        }
    };

    const handleMouseLeave = () => {
        setHoveredGroup(null);
    };

    const isInHoveredGroup = (metric: string) => {
        if (hoveredGroup === null) return false;
        const group = getMetricGroup(metric);
        return group === hoveredGroup;
    };

    return (
        <div className="overflow-visible">
            {metrics && (
                <table className="report-table w-full border-collapse text-white">
                    <thead>
                    <tr className="bg-[#b39700]">
                        <th className="p-2 text-center main-header-cell" colSpan={2}>전략 성과</th>
                    </tr>
                    </thead>
                    <tbody>
                    {metricKeys.map((metric, index) => (
                        <tr key={metric}
                            className={`${index % 2 === 0 ? 'bg-[#4d4000]' : 'bg-[#665400]'} ${isInHoveredGroup(metric) ? 'group-hovered' : ''} ${borderBottomMetrics.includes(metric) ? 'border-bottom-dashed' : ''}`}
                            onMouseEnter={() => handleMouseEnter(metric)}
                            onMouseLeave={handleMouseLeave}>
                            <td className="p-2 text-center">{metric}</td>
                            {metric === '자금 정보' ? (
                                <td className="p-0 text-center align-middle overflow-hidden">
                                    <div className="flex justify-around items-center h-full">
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span className="text-xs text-[#fff2b3] mb-1 block truncate header-text">초기 자금</span>
                                            <span
                                                className="truncate">{metrics && formatDollar(metrics.balanceMetrics.initialBalance)}</span>
                                        </div>
                                        <div
                                            className="flex-1 text-center px-1 py-2 overflow-hidden">
                                            <span className="text-xs text-[#fff2b3] mb-1 block truncate header-text">종료 자금</span>
                                            <span
                                                className={`truncate ${
                                                    metrics && metrics.balanceMetrics.endingBalance > metrics.balanceMetrics.initialBalance
                                                        ? 'positive'
                                                        : metrics && metrics.balanceMetrics.endingBalance < metrics.balanceMetrics.initialBalance
                                                            ? 'negative'
                                                            : '' // 같을 경우 기본 색상 유지 
                                                }`}>
                                                {metrics && formatDollar(metrics.balanceMetrics.endingBalance)}
                                            </span>
                                        </div>
                                    </div>
                                </td>
                            ) : metric === '성장률' ? (
                                <td className="p-0 text-center align-middle overflow-hidden">
                                    <div className="flex justify-around items-center h-full">
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">CDGR</span>
                                            <span
                                                className={`truncate ${
                                                    metrics && parseFloat(String(metrics.agrMetrics.cdgr).replace('%', '')) > 0 ? 'positive' :
                                                        metrics && parseFloat(String(metrics.agrMetrics.cdgr).replace('%', '')) < 0 ? 'negative' : ''
                                                }`}>
                                                {metrics && formatPercent(metrics.agrMetrics.cdgr)}
                                            </span>
                                        </div>
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">CWGR</span>
                                            <span
                                                className={`truncate ${
                                                    metrics && parseFloat(String(metrics.agrMetrics.cwgr).replace('%', '')) > 0 ? 'positive' :
                                                        metrics && parseFloat(String(metrics.agrMetrics.cwgr).replace('%', '')) < 0 ? 'negative' : ''
                                                }`}>
                                                {metrics && formatPercent(metrics.agrMetrics.cwgr)}
                                            </span>
                                        </div>
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">CMGR</span>
                                            <span
                                                className={`truncate ${
                                                    metrics && parseFloat(String(metrics.agrMetrics.cmgr).replace('%', '')) > 0 ? 'positive' :
                                                        metrics && parseFloat(String(metrics.agrMetrics.cmgr).replace('%', '')) < 0 ? 'negative' : ''
                                                }`}>
                                                {metrics && formatPercent(metrics.agrMetrics.cmgr)}
                                            </span>
                                        </div>
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">CQGR</span>
                                            <span
                                                className={`truncate ${
                                                    metrics && parseFloat(String(metrics.agrMetrics.cqgr).replace('%', '')) > 0 ? 'positive' :
                                                        metrics && parseFloat(String(metrics.agrMetrics.cqgr).replace('%', '')) < 0 ? 'negative' : ''
                                                }`}>
                                                {metrics && formatPercent(metrics.agrMetrics.cqgr)}
                                            </span>
                                        </div>
                                        <div className="flex-1 text-center px-1 py-2 overflow-hidden">
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">CAGR</span>
                                            <span
                                                className={`truncate ${
                                                    metrics && parseFloat(String(metrics.agrMetrics.cagr).replace('%', '')) > 0 ? 'positive' :
                                                        metrics && parseFloat(String(metrics.agrMetrics.cagr).replace('%', '')) < 0 ? 'negative' : ''
                                                }`}>
                                                {metrics && formatPercent(metrics.agrMetrics.cagr)}
                                            </span>
                                        </div>
                                    </div>
                                </td>
                            ) : metric === 'MDD' ? (
                                <td className="p-0 text-center align-middle overflow-hidden">
                                    <div className="flex justify-around items-center h-full">
                                        <div
                                            className="flex-grow text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">MDD</span>
                                            <span
                                                className="truncate"
                                                style={{color: metrics && parseFloat(metrics.riskRewardMetrics.mdd) === 0 ? '#008000' : '#a01722'}}>
                                                {metrics && formatPercent(metrics.riskRewardMetrics.mdd)}
                                            </span>
                                        </div>
                                    </div>
                                </td>
                            ) : metric === '수익 효율성 지수' ? (
                                <td className="p-0 text-center align-middle overflow-hidden">
                                    <div className="flex justify-around items-center h-full">
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span className="text-xs text-[#fff2b3] mb-1 block truncate header-text">칼마 지수</span>
                                            <span
                                                className={`truncate ${
                                                    metrics && metrics.riskAdjustedReturnMetrics.calmarRatio === "∞" ? '' :
                                                        metrics && Number(metrics.riskAdjustedReturnMetrics.calmarRatio) > 0 ? 'positive' :
                                                            metrics && Number(metrics.riskAdjustedReturnMetrics.calmarRatio) < 0 ? 'negative' : ''
                                                }`}
                                                style={{
                                                    color: metrics && metrics.riskAdjustedReturnMetrics.calmarRatio === "∞" ? '#ffffff' : undefined
                                                }}>
                                                {metrics && metrics.riskAdjustedReturnMetrics.calmarRatio}
                                            </span>
                                        </div>
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span className="text-xs text-[#fff2b3] mb-1 block truncate header-text">샤프 지수</span>
                                            <span
                                                className={`truncate ${
                                                    metrics && metrics.riskAdjustedReturnMetrics.sharpeRatio === "∞" ? '' :
                                                        metrics && Number(metrics.riskAdjustedReturnMetrics.sharpeRatio) > 0 ? 'positive' :
                                                            metrics && Number(metrics.riskAdjustedReturnMetrics.sharpeRatio) < 0 ? 'negative' : ''
                                                }`}
                                                style={{
                                                    color: metrics && metrics.riskAdjustedReturnMetrics.sharpeRatio === "∞" ? '#ffffff' : undefined
                                                }}>
                                                {metrics && metrics.riskAdjustedReturnMetrics.sharpeRatio}
                                            </span>
                                        </div>
                                        <div className="flex-1 text-center px-1 py-2 overflow-hidden">
                                            <span className="text-xs text-[#fff2b3] mb-1 block truncate header-text">소르티노 지수</span>
                                            <span
                                                className={`truncate ${
                                                    metrics && metrics.riskAdjustedReturnMetrics.sortinoRatio === "∞" ? '' :
                                                        metrics && Number(metrics.riskAdjustedReturnMetrics.sortinoRatio) > 0 ? 'positive' :
                                                            metrics && Number(metrics.riskAdjustedReturnMetrics.sortinoRatio) < 0 ? 'negative' : ''
                                                }`}
                                                style={{
                                                    color: metrics && metrics.riskAdjustedReturnMetrics.sortinoRatio === "∞" ? '#ffffff' : undefined
                                                }}>
                                                {metrics && metrics.riskAdjustedReturnMetrics.sortinoRatio}
                                            </span>
                                        </div>
                                    </div>
                                </td>
                            ) : metric === '진입 횟수' ? (
                                <td className="p-0 text-center align-middle overflow-hidden">
                                    <div className="flex justify-around items-center h-full">
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">전체</span>
                                            <span
                                                className="truncate">{metrics && (() => {
                                                const val = Number(metrics.tradeCountMetrics.totalTradeCount);
                                                return isNaN(val) ? '0회' : val.toLocaleString('en-US') + '회';
                                            })()}</span>
                                        </div>
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">수익</span>
                                            <span
                                                className={`truncate ${metrics && parseInt(metrics.tradeCountMetrics.profitTradeCount) > 0 ? 'positive' : ''}`}>
                                                {metrics ? (() => {
                                                    const val = Number(metrics.tradeCountMetrics.profitTradeCount);
                                                    return isNaN(val) ? '0회' : val.toLocaleString('en-US') + '회';
                                                })() : placeholder}
                                            </span>
                                        </div>
                                        <div className="flex-1 text-center px-1 py-2 overflow-hidden">
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">손실</span>
                                            <span
                                                className={`truncate ${metrics && parseInt(metrics.tradeCountMetrics.lossTradeCount) > 0 ? 'negative' : ''}`}>
                                                {metrics ? (() => {
                                                    const val = Number(metrics.tradeCountMetrics.lossTradeCount);
                                                    return isNaN(val) ? '0회' : val.toLocaleString('en-US') + '회';
                                                })() : placeholder}
                                            </span>
                                        </div>
                                    </div>
                                </td>
                            ) : metric === '평균 진입 횟수' ? (
                                <td className="p-0 text-center align-middle overflow-hidden">
                                    <div className="flex justify-around items-center h-full">
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">일간</span>
                                            <span
                                                className="truncate">{metrics && (() => {
                                                const val = metrics.tradeCountMetrics.dailyTradeCount;
                                                return val === "-" ? "-" : (() => {
                                                    const numVal = Number(val);
                                                    return isNaN(numVal) ? '0.00회' : numVal.toLocaleString('en-US', {
                                                        minimumFractionDigits: 2,
                                                        maximumFractionDigits: 2
                                                    }) + '회';
                                                })();
                                            })()}</span>
                                        </div>
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">주간</span>
                                            <span
                                                className="truncate">{metrics && (() => {
                                                const val = metrics.tradeCountMetrics.weeklyTradeCount;
                                                return val === "-" ? "-" : (() => {
                                                    const numVal = Number(val);
                                                    return isNaN(numVal) ? '0.00회' : numVal.toLocaleString('en-US', {
                                                        minimumFractionDigits: 2,
                                                        maximumFractionDigits: 2
                                                    }) + '회';
                                                })();
                                            })()}</span>
                                        </div>
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">월간</span>
                                            <span
                                                className="truncate">{metrics && (() => {
                                                const val = metrics.tradeCountMetrics.monthlyTradeCount;
                                                return val === "-" ? "-" : (() => {
                                                    const numVal = Number(val);
                                                    return isNaN(numVal) ? '0.00회' : numVal.toLocaleString('en-US', {
                                                        minimumFractionDigits: 2,
                                                        maximumFractionDigits: 2
                                                    }) + '회';
                                                })();
                                            })()}</span>
                                        </div>
                                        <div className="flex-1 text-center px-1 py-2 overflow-hidden">
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">연간</span>
                                            <span
                                                className="truncate">{metrics && (() => {
                                                const val = metrics.tradeCountMetrics.yearlyTradeCount;
                                                return val === "-" ? "-" : (() => {
                                                    const numVal = Number(val);
                                                    return isNaN(numVal) ? '0.00회' : numVal.toLocaleString('en-US', {
                                                        minimumFractionDigits: 2,
                                                        maximumFractionDigits: 2
                                                    }) + '회';
                                                })();
                                            })()}</span>
                                        </div>
                                    </div>
                                </td>
                            ) : metric === '최대 연속 거래' ? (
                                <td className="p-0 text-center align-middle overflow-hidden">
                                    <div className="flex justify-around items-center h-full">
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">연승</span>
                                            <span
                                                className={`truncate ${(() => {
                                                    const val = Number(metrics.streaks.maxWinStreak);
                                                    return val === 0 ? '' : 'positive';
                                                })()}`}
                                                style={{
                                                    color: (() => {
                                                        const val = Number(metrics.streaks.maxWinStreak);
                                                        return val === 0 ? '#ffffff' : undefined;
                                                    })()
                                                }}>
                                                {metrics && (() => {
                                                    const val = Number(metrics.streaks.maxWinStreak);
                                                    return isNaN(val) ? '0' : val.toLocaleString('en-US');
                                                })()}
                                                <span className="font-semibold">연승</span>
                                            </span>
                                        </div>
                                        <div className="flex-1 text-center px-1 py-2 overflow-hidden">
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">연패</span>
                                            <span
                                                className={`truncate ${(() => {
                                                    const val = Number(metrics.streaks.maxLoseStreak);
                                                    return val === 0 ? '' : 'negative';
                                                })()}`}
                                                style={{
                                                    color: (() => {
                                                        const val = Number(metrics.streaks.maxLoseStreak);
                                                        return val === 0 ? '#ffffff' : undefined;
                                                    })()
                                                }}>
                                                {metrics && (() => {
                                                    const val = Number(metrics.streaks.maxLoseStreak);
                                                    return isNaN(val) ? '0' : val.toLocaleString('en-US');
                                                })()}
                                                <span className="font-semibold">연패</span>
                                            </span>
                                        </div>
                                    </div>
                                </td>
                            ) : metric === '펀딩 횟수' ? (
                                <td className="p-0 text-center align-middle overflow-hidden">
                                    <div className="flex justify-around items-center h-full">
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">전체</span>
                                            <span className="truncate">
                                                {metrics ? `${metrics.fundingMetrics.totalFundingCount.toLocaleString('en-US')}회` : '-'}
                                            </span>
                                        </div>
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">수령</span>
                                            <span
                                                className={`truncate ${
                                                    metrics && metrics.fundingMetrics.receivedFundingCount > 0 ? 'positive' : ''
                                                }`}
                                            >
                                                {metrics ? `${metrics.fundingMetrics.receivedFundingCount.toLocaleString('en-US')}회` : '-'}
                                            </span>
                                        </div>
                                        <div className="flex-1 text-center px-1 py-2 overflow-hidden">
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">지불</span>
                                            <span
                                                className={`truncate ${
                                                    metrics && metrics.fundingMetrics.paidFundingCount > 0 ? 'negative' : ''
                                                }`}
                                            >
                                                {metrics ? `${metrics.fundingMetrics.paidFundingCount.toLocaleString('en-US')}회` : '-'}
                                            </span>
                                        </div>
                                    </div>
                                </td>
                            ) : metric === '펀딩비 합계' ? (
                                <td className="p-0 text-center align-middle overflow-hidden">
                                    <div className="flex justify-around items-center h-full">
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">전체</span>
                                            <span
                                                className={`truncate ${
                                                    metrics && metrics.fundingMetrics.totalFundingFeeSum !== undefined && metrics.fundingMetrics.totalFundingFeeSum !== 0
                                                        ? (metrics.fundingMetrics.totalFundingFeeSum > 0 ? 'positive' : 'negative')
                                                        : ''
                                                }`}
                                            >
                                                {metrics ? formatDollar(metrics.fundingMetrics.totalFundingFeeSum || 0) : '-'}
                                            </span>
                                        </div>
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">수령</span>
                                            <span
                                                className={`truncate ${
                                                    metrics && metrics.fundingMetrics.receivedFundingFeeSum !== undefined && metrics.fundingMetrics.receivedFundingFeeSum > 0 ? 'positive' : ''
                                                }`}
                                            >
                                                {metrics ? formatDollar(metrics.fundingMetrics.receivedFundingFeeSum || 0) : '-'}
                                            </span>
                                        </div>
                                        <div className="flex-1 text-center px-1 py-2 overflow-hidden">
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">지불</span>
                                            <span
                                                className={`truncate ${
                                                    metrics && metrics.fundingMetrics.paidFundingFeeSum !== undefined && metrics.fundingMetrics.paidFundingFeeSum < 0 ? 'negative' : ''
                                                }`}
                                            >
                                                {metrics ? formatDollar(metrics.fundingMetrics.paidFundingFeeSum || 0) : '-'}
                                            </span>
                                        </div>
                                    </div>
                                </td>
                            ) : metric === '평균 펀딩비' ? (
                                <td className="p-0 text-center align-middle overflow-hidden">
                                    <div className="flex justify-around items-center h-full">
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">전체</span>
                                            <span
                                                className={`truncate ${
                                                    metrics && metrics.fundingMetrics.avgTotalFundingFee !== undefined && metrics.fundingMetrics.avgTotalFundingFee !== 0
                                                        ? (metrics.fundingMetrics.avgTotalFundingFee > 0 ? 'positive' : 'negative')
                                                        : ''
                                                }`}
                                            >
                                                {metrics ? formatDollar(metrics.fundingMetrics.avgTotalFundingFee || 0) : '-'}
                                            </span>
                                        </div>
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">수령</span>
                                            <span
                                                className={`truncate ${
                                                    metrics && metrics.fundingMetrics.avgReceivedFundingFee !== undefined && metrics.fundingMetrics.avgReceivedFundingFee > 0 ? 'positive' : ''
                                                }`}
                                            >
                                                {metrics ? formatDollar(metrics.fundingMetrics.avgReceivedFundingFee || 0) : '-'}
                                            </span>
                                        </div>
                                        <div className="flex-1 text-center px-1 py-2 overflow-hidden">
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">지불</span>
                                            <span
                                                className={`truncate ${
                                                    metrics && metrics.fundingMetrics.avgPaidFundingFee !== undefined && metrics.fundingMetrics.avgPaidFundingFee < 0 ? 'negative' : ''
                                                }`}
                                            >
                                                {metrics ? formatDollar(metrics.fundingMetrics.avgPaidFundingFee || 0) : '-'}
                                            </span>
                                        </div>
                                    </div>
                                </td>
                            ) : metric === '수수료 합계' ? (
                                <td className="p-0 text-center align-middle overflow-hidden">
                                    <div className="flex justify-around items-center h-full">
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">전체</span>
                                            <span
                                                className="truncate">{metrics && formatDollar(metrics.feeMetrics.totalFee)}</span>
                                        </div>
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">진입</span>
                                            <span
                                                className="truncate">{metrics && formatDollar(metrics.feeMetrics.totalEntryFee)}</span>
                                        </div>
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">청산</span>
                                            <span
                                                className="truncate">{metrics && formatDollar(metrics.feeMetrics.totalExitFee)}</span>
                                        </div>
                                        <div className="flex-1 text-center px-1 py-2 overflow-hidden">
                                            <span className="text-xs text-[#fff2b3] mb-1 block truncate header-text">강제 청산</span>
                                            <span
                                                className="truncate">{metrics && formatDollar(metrics.feeMetrics.totalLiquidationFee)}</span>
                                        </div>
                                    </div>
                                </td>
                            ) : metric === '최고 자금' ? (
                                <td className="p-0 text-center align-middle overflow-hidden">
                                    <div className="flex justify-around items-center h-full">
                                        <div
                                            className="flex-grow text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span className="text-xs text-[#fff2b3] mb-1 block truncate header-text">최고 자금</span>
                                            <span
                                                className="truncate"
                                                style={{color: '#008000'}}>{metrics && formatDollar(metrics.balanceMetrics.highestBalanceValue)}</span>
                                        </div>
                                        <div
                                            className="flex-grow text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">증가율</span>
                                            <span
                                                className="truncate"
                                                style={{color: '#008000'}}>{metrics && formatPercent(metrics.balanceMetrics.highestBalancePercentage)}</span>
                                        </div>
                                        <div className="flex-grow text-center px-1 py-2 overflow-hidden">
                                            <span className="text-xs text-[#fff2b3] mb-1 block truncate header-text">달성 시간</span>
                                            <span
                                                className="truncate">{metrics && metrics.balanceMetrics.highestBalanceTime}</span>
                                        </div>
                                    </div>
                                </td>
                            ) : metric === '최저 자금' ? (
                                <td className="p-0 text-center align-middle overflow-hidden">
                                    <div className="flex justify-around items-center h-full">
                                        <div
                                            className="flex-grow text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span className="text-xs text-[#fff2b3] mb-1 block truncate header-text">최저 자금</span>
                                            <span
                                                className="truncate"
                                                style={{color: metrics && metrics.balanceMetrics.lowestBalanceValue === metrics.balanceMetrics.initialBalance ? '#008000' : '#a01722'}}>{metrics && formatDollar(metrics.balanceMetrics.lowestBalanceValue)}</span>
                                        </div>
                                        <div
                                            className="flex-grow text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">감소율</span>
                                            <span
                                                className="truncate"
                                                style={{color: metrics && metrics.balanceMetrics.lowestBalanceValue === metrics.balanceMetrics.initialBalance ? '#008000' : '#a01722'}}>{metrics && formatPercent(metrics.balanceMetrics.lowestBalancePercentage)}</span>
                                        </div>
                                        <div className="flex-grow text-center px-1 py-2 overflow-hidden">
                                            <span className="text-xs text-[#fff2b3] mb-1 block truncate header-text">달성 시간</span>
                                            <span
                                                className="truncate">{metrics && metrics.balanceMetrics.lowestBalanceTime}</span>
                                        </div>
                                    </div>
                                </td>
                            ) : metric === '레버리지' ? (
                                <td className="p-0 text-center align-middle overflow-hidden">
                                    <div className="flex justify-around items-center h-full">
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">평균</span>
                                            <span
                                                className="truncate">{metrics && (() => {
                                                const val = Number(metrics.leverageMetrics.average);
                                                return isNaN(val) ? '0' : val.toLocaleString('en-US') + 'x';
                                            })()}</span>
                                        </div>
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">최대</span>
                                            <span className="truncate">{metrics && (() => {
                                                const val = Number(metrics.leverageMetrics.max);
                                                return isNaN(val) ? '0' : val.toLocaleString('en-US') + 'x';
                                            })()}</span>
                                        </div>
                                        <div className="flex-1 text-center px-1 py-2 overflow-hidden">
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">최소</span>
                                            <span className="truncate">{metrics && (() => {
                                                const val = Number(metrics.leverageMetrics.min);
                                                return isNaN(val) ? '0' : val.toLocaleString('en-US') + 'x';
                                            })()}</span>
                                        </div>
                                    </div>
                                </td>
                            ) : metric === "순손익 합계" ? (
                                <td className="p-0 text-center align-middle overflow-hidden">
                                    <div className="flex justify-around items-center h-full">
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span className="text-xs text-[#fff2b3] mb-1 block truncate header-text">순손익 합계</span>
                                            <span
                                                className={`truncate ${
                                                    metrics && metrics.totalProfitLossMetrics.totalProfitLoss > 0 ? 'positive' :
                                                        metrics && metrics.totalProfitLossMetrics.totalProfitLoss < 0 ? 'negative' : ''
                                                }`}>
                                                {metrics && formatDollar(metrics.totalProfitLossMetrics.totalProfitLoss)}
                                            </span>
                                        </div>
                                        <div className="flex-1 text-center px-1 py-2 overflow-hidden">
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">순손익률</span>
                                            <span
                                                className={`truncate ${
                                                    metrics && metrics.totalProfitLossMetrics.totalProfitLoss > 0 ? 'positive' :
                                                        metrics && metrics.totalProfitLossMetrics.totalProfitLoss < 0 ? 'negative' : ''
                                                }`}>
                                                {metrics && formatPercent(metrics.totalProfitLossMetrics.totalProfitLossPercent)}
                                            </span>
                                        </div>
                                    </div>
                                </td>
                            ) : metric === '순수익 합계' ? (
                                <td className="p-0 text-center align-middle overflow-hidden">
                                    <div className="flex justify-around items-center h-full">
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span className="text-xs text-[#fff2b3] mb-1 block truncate header-text">순수익 합계</span>
                                            <span
                                                className={`truncate ${
                                                    metrics && Number(metrics.totalProfitLossMetrics.totalProfit) > 0 ? 'positive' : ''
                                                }`}>
                                                {metrics && formatDollar(metrics.totalProfitLossMetrics.totalProfit)}
                                            </span>
                                        </div>
                                        <div className="flex-1 text-center px-1 py-2 overflow-hidden">
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">순수익률</span>
                                            <span
                                                className={`truncate ${
                                                    metrics && Number(metrics.totalProfitLossMetrics.totalProfit) > 0 ? 'positive' : ''
                                                }`}>
                                                {metrics && formatPercent(metrics.totalProfitLossMetrics.totalProfitPercent)}
                                            </span>
                                        </div>
                                    </div>
                                </td>
                            ) : metric === '순손실 합계' ? (
                                <td className="p-0 text-center align-middle overflow-hidden">
                                    <div className="flex justify-around items-center h-full">
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span className="text-xs text-[#fff2b3] mb-1 block truncate header-text">순손실 합계</span>
                                            <span
                                                className={`truncate ${
                                                    metrics && Number(metrics.totalProfitLossMetrics.totalLoss) < 0 ? 'negative' : ''
                                                }`}>
                                                {metrics && formatDollar(metrics.totalProfitLossMetrics.totalLoss)}
                                            </span>
                                        </div>
                                        <div className="flex-1 text-center px-1 py-2 overflow-hidden">
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">순손실률</span>
                                            <span
                                                className={`truncate ${
                                                    metrics && Number(metrics.totalProfitLossMetrics.totalLoss) < 0 ? 'negative' : ''
                                                }`}>
                                                {metrics && formatPercent(metrics.totalProfitLossMetrics.totalLossPercent)}
                                            </span>
                                        </div>
                                    </div>
                                </td>
                            ) : metric === '평균 순손익' ? (
                                <td className="p-0 text-center align-middle overflow-hidden">
                                    <div className="flex justify-around items-center h-full">
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span className="text-xs text-[#fff2b3] mb-1 block truncate header-text">평균 순손익</span>
                                            <span
                                                className={`truncate ${
                                                    metrics && Number(metrics.avgProfitLossMetrics.avgPnL) > 0 ? 'positive' :
                                                        metrics && Number(metrics.avgProfitLossMetrics.avgPnL) < 0 ? 'negative' : ''
                                                }`}>
                                                {metrics && formatDollar(metrics.avgProfitLossMetrics.avgPnL)}
                                            </span>
                                        </div>
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">평균 개별 순손익률</span>
                                            <span
                                                className={`truncate ${
                                                    metrics && parseFloat(String(metrics.avgProfitLossMetrics.avgIndividualPnLRate).replace('%', '')) > 0 ? 'positive' :
                                                        metrics && parseFloat(String(metrics.avgProfitLossMetrics.avgIndividualPnLRate).replace('%', '')) < 0 ? 'negative' : ''
                                                }`}>
                                                {metrics && formatPercent(metrics.avgProfitLossMetrics.avgIndividualPnLRate)}
                                            </span>
                                        </div>
                                        <div className="flex-1 text-center px-1 py-2 overflow-hidden">
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">평균 전체 순손익률</span>
                                            <span
                                                className={`truncate ${
                                                    metrics && parseFloat(String(metrics.avgProfitLossMetrics.avgTotalPnLRate).replace('%', '')) > 0 ? 'positive' :
                                                        metrics && parseFloat(String(metrics.avgProfitLossMetrics.avgTotalPnLRate).replace('%', '')) < 0 ? 'negative' : ''
                                                }`}>
                                                {metrics && formatPercent(metrics.avgProfitLossMetrics.avgTotalPnLRate)}
                                            </span>
                                        </div>
                                    </div>
                                </td>
                            ) : metric === '평균 순수익' ? (
                                <td className="p-0 text-center align-middle overflow-hidden">
                                    <div className="flex justify-around items-center h-full">
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span className="text-xs text-[#fff2b3] mb-1 block truncate header-text">평균 순수익</span>
                                            <span
                                                className={`truncate ${
                                                    metrics && Number(metrics.avgProfitLossMetrics.avgProfit) > 0 ? 'positive' : ''
                                                }`}>
                                                {metrics && formatDollar(metrics.avgProfitLossMetrics.avgProfit)}
                                            </span>
                                        </div>
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text"> 평균 개별 순수익률</span>
                                            <span
                                                className={`truncate ${
                                                    metrics && parseFloat(String(metrics.avgProfitLossMetrics.avgProfitIndividualRate).replace('%', '')) > 0 ? 'positive' : ''
                                                }`}>
                                                {metrics && formatPercent(metrics.avgProfitLossMetrics.avgProfitIndividualRate)}
                                            </span>
                                        </div>
                                        <div className="flex-1 text-center px-1 py-2 overflow-hidden">
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">평균 전체 순수익률</span>
                                            <span
                                                className={`truncate ${
                                                    metrics && parseFloat(String(metrics.avgProfitLossMetrics.avgProfitTotalRate).replace('%', '')) > 0 ? 'positive' : ''
                                                }`}>
                                                {metrics && formatPercent(metrics.avgProfitLossMetrics.avgProfitTotalRate)}
                                            </span>
                                        </div>
                                    </div>
                                </td>
                            ) : metric === '평균 순손실' ? (
                                <td className="p-0 text-center align-middle overflow-hidden">
                                    <div className="flex justify-around items-center h-full">
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span className="text-xs text-[#fff2b3] mb-1 block truncate header-text">평균 순손실</span>
                                            <span
                                                className={`truncate ${
                                                    metrics && Number(metrics.avgProfitLossMetrics.avgLoss) < 0 ? 'negative' : ''
                                                }`}>
                                                {metrics && formatDollar(metrics.avgProfitLossMetrics.avgLoss)}
                                            </span>
                                        </div>
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">평균 개별 순손실률</span>
                                            <span
                                                className={`truncate ${
                                                    metrics && parseFloat(String(metrics.avgProfitLossMetrics.avgLossIndividualRate).replace('%', '')) < 0 ? 'negative' : ''
                                                }`}>
                                                {metrics && formatPercent(metrics.avgProfitLossMetrics.avgLossIndividualRate)}
                                            </span>
                                        </div>
                                        <div className="flex-1 text-center px-1 py-2 overflow-hidden">
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">평균 전체 순손실률</span>
                                            <span
                                                className={`truncate ${
                                                    metrics && parseFloat(String(metrics.avgProfitLossMetrics.avgLossTotalRate).replace('%', '')) < 0 ? 'negative' : ''
                                                }`}>
                                                {metrics && formatPercent(metrics.avgProfitLossMetrics.avgLossTotalRate)}
                                            </span>
                                        </div>
                                    </div>
                                </td>
                            ) : metric === '최대 순수익' ? (
                                <td className="p-0 text-center align-middle overflow-hidden">
                                    <div className="flex justify-around items-center h-full">
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span className="text-xs text-[#fff2b3] mb-1 block truncate header-text">최대 순수익</span>
                                            <span
                                                className={`truncate ${
                                                    metrics && metrics.maxProfitLossMetrics.maxProfit !== '-' && Number(metrics.maxProfitLossMetrics.maxProfit) > 0 ? 'positive' : ''
                                                }`}>
                                                {metrics && metrics.maxProfitLossMetrics.maxProfit === '-' ? '-' : formatDollar(metrics.maxProfitLossMetrics.maxProfit)}
                                            </span>
                                        </div>
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span className="text-xs text-[#fff2b3] mb-1 block truncate header-text">개별 순수익률</span>
                                            <span
                                                className={`truncate ${
                                                    metrics && metrics.maxProfitLossMetrics.maxProfitIndividualRate !== '-' && parseFloat(String(metrics.maxProfitLossMetrics.maxProfitIndividualRate).replace('%', '')) > 0 ? 'positive' : ''
                                                }`}>
                                                {metrics && metrics.maxProfitLossMetrics.maxProfitIndividualRate === '-' ? '-' : formatPercent(metrics.maxProfitLossMetrics.maxProfitIndividualRate)}
                                            </span>
                                        </div>
                                        <div className="flex-1 text-center px-1 py-2 overflow-hidden">
                                            <span className="text-xs text-[#fff2b3] mb-1 block truncate header-text">전체 순수익률</span>
                                            <span
                                                className={`truncate ${
                                                    metrics && metrics.maxProfitLossMetrics.maxProfitTotalRate !== '-' && parseFloat(String(metrics.maxProfitLossMetrics.maxProfitTotalRate).replace('%', '')) > 0 ? 'positive' : ''
                                                }`}>
                                                {metrics && metrics.maxProfitLossMetrics.maxProfitTotalRate === '-' ? '-' : formatPercent(metrics.maxProfitLossMetrics.maxProfitTotalRate)}
                                            </span>
                                        </div>
                                    </div>
                                </td>
                            ) : metric === '최대 순손실' ? (
                                <td className="p-0 text-center align-middle overflow-hidden">
                                    <div className="flex justify-around items-center h-full">
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span className="text-xs text-[#fff2b3] mb-1 block truncate header-text">최대 순손실</span>
                                            <span
                                                className={`truncate ${
                                                    metrics && Number(metrics.maxProfitLossMetrics.maxLoss) < 0 ? 'negative' : ''
                                                }`}>
                                                {metrics && formatDollar(metrics.maxProfitLossMetrics.maxLoss)}
                                            </span>
                                        </div>
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span className="text-xs text-[#fff2b3] mb-1 block truncate header-text">개별 순손실률</span>
                                            <span
                                                className={`truncate ${
                                                    metrics && parseFloat(String(metrics.maxProfitLossMetrics.maxLossIndividualRate).replace('%', '')) < 0 ? 'negative' : ''
                                                }`}>
                                                {metrics && formatPercent(metrics.maxProfitLossMetrics.maxLossIndividualRate)}
                                            </span>
                                        </div>
                                        <div className="flex-1 text-center px-1 py-2 overflow-hidden">
                                            <span className="text-xs text-[#fff2b3] mb-1 block truncate header-text">전체 순손실률</span>
                                            <span
                                                className={`truncate ${
                                                    metrics && parseFloat(String(metrics.maxProfitLossMetrics.maxLossTotalRate).replace('%', '')) < 0 ? 'negative' : ''
                                                }`}>
                                                {metrics && formatPercent(metrics.maxProfitLossMetrics.maxLossTotalRate)}
                                            </span>
                                        </div>
                                    </div>
                                </td>
                            ) : metric === '최대 개별 순수익률' ? (
                                <td className="p-0 text-center align-middle overflow-hidden">
                                    <div className="flex justify-around items-center h-full">
                                        <div
                                            className="flex-grow text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">최대 개별 순수익률</span>
                                            <span
                                                className={'truncate ' + (metrics ? (() => {
                                                    const val = metrics.maxProfitLossRateMetrics.maxIndividualProfitRate.value;
                                                    const pnlStr = metrics.maxProfitLossRateMetrics.maxIndividualProfitRate.pnl;
                                                    if (val === '-' || pnlStr === '-' || pnlStr === undefined) return '';
                                                    const pnlNum = parseFloat(String(pnlStr).replace(/,/g, ''));
                                                    return isNaN(pnlNum) ? '' : (pnlNum > 0 ? 'positive' : (pnlNum < 0 ? 'negative' : ''));
                                                })() : '')}>
                                                {metrics && metrics.maxProfitLossRateMetrics.maxIndividualProfitRate.value === '-' ? '-' : formatPercent(metrics.maxProfitLossRateMetrics.maxIndividualProfitRate.value)}
                                            </span>
                                        </div>
                                        <div
                                            className="flex-grow text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">순수익</span>
                                            <span
                                                className={'truncate ' + (metrics ? (() => {
                                                    const pnlStr = metrics.maxProfitLossRateMetrics.maxIndividualProfitRate.pnl;
                                                    if (pnlStr === '-' || pnlStr === undefined) return '';
                                                    const pnlNum = parseFloat(String(pnlStr).replace(/,/g, ''));
                                                    return isNaN(pnlNum) ? '' : (pnlNum > 0 ? 'positive' : (pnlNum < 0 ? 'negative' : ''));
                                                })() : '')}>
                                                {metrics && metrics.maxProfitLossRateMetrics.maxIndividualProfitRate.pnl === '-' ? '-' : formatDollar(metrics.maxProfitLossRateMetrics.maxIndividualProfitRate.pnl)}
                                            </span>
                                        </div>
                                        <div className="flex-grow text-center px-1 py-2 overflow-hidden">
                                            <span className="text-xs text-[#fff2b3] mb-1 block truncate header-text">청산 시간</span>
                                            <span
                                                className="truncate">{metrics && metrics.maxProfitLossRateMetrics.maxIndividualProfitRate.time}</span>
                                        </div>
                                    </div>
                                </td>
                            ) : metric === '최대 전체 순수익률' ? (
                                <td className="p-0 text-center align-middle overflow-hidden">
                                    <div className="flex justify-around items-center h-full">
                                        <div
                                            className="flex-grow text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">최대 전체 순수익률</span>
                                            <span
                                                className={'truncate ' + (metrics ? (() => {
                                                    const val = metrics.maxProfitLossRateMetrics.maxTotalProfitRate.value;
                                                    const pnlStr = metrics.maxProfitLossRateMetrics.maxTotalProfitRate.pnl;
                                                    if (val === '-' || pnlStr === '-' || pnlStr === undefined) return '';
                                                    const pnlNum = parseFloat(String(pnlStr).replace(/,/g, ''));
                                                    return isNaN(pnlNum) ? '' : (pnlNum > 0 ? 'positive' : (pnlNum < 0 ? 'negative' : ''));
                                                })() : '')}>
                                                {metrics && metrics.maxProfitLossRateMetrics.maxTotalProfitRate.value === '-' ? '-' : formatPercent(metrics.maxProfitLossRateMetrics.maxTotalProfitRate.value)}
                                            </span>
                                        </div>
                                        <div
                                            className="flex-grow text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">순수익</span>
                                            <span
                                                className={'truncate ' + (metrics ? (() => {
                                                    const pnlStr = metrics.maxProfitLossRateMetrics.maxTotalProfitRate.pnl;
                                                    if (pnlStr === '-' || pnlStr === undefined) return '';
                                                    const pnlNum = parseFloat(String(pnlStr).replace(/,/g, ''));
                                                    return isNaN(pnlNum) ? '' : (pnlNum > 0 ? 'positive' : (pnlNum < 0 ? 'negative' : ''));
                                                })() : '')}>
                                                {metrics && metrics.maxProfitLossRateMetrics.maxTotalProfitRate.pnl === '-' ? '-' : formatDollar(metrics.maxProfitLossRateMetrics.maxTotalProfitRate.pnl)}
                                            </span>
                                        </div>
                                        <div className="flex-grow text-center px-1 py-2 overflow-hidden">
                                            <span className="text-xs text-[#fff2b3] mb-1 block truncate header-text">청산 시간</span>
                                            <span
                                                className="truncate">{metrics && metrics.maxProfitLossRateMetrics.maxTotalProfitRate.time}</span>
                                        </div>
                                    </div>
                                </td>
                            ) : metric === '최대 개별 순손실률' ? (
                                <td className="p-0 text-center align-middle overflow-hidden">
                                    <div className="flex justify-around items-center h-full">
                                        <div
                                            className="flex-grow text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">최대 개별 순손실률</span>
                                            <span
                                                className={`truncate ${
                                                    metrics && (() => {
                                                        const pnlStr = metrics.maxProfitLossRateMetrics.maxIndividualLossRate.pnl;
                                                        const rateVal = parseFloat(String(metrics.maxProfitLossRateMetrics.maxIndividualLossRate.value).replace('%', ''));
                                                        if (pnlStr === '-' && isNaN(rateVal)) return '';
                                                        const pnlVal = pnlStr === '-' ? 0 : Number(pnlStr.replace(/,/g, ''));
                                                        return (pnlVal === 0 && rateVal === 0) ? '' : (pnlVal < 0 || rateVal < 0) ? 'negative' : '';
                                                    })()
                                                }`}
                                                style={{
                                                    color: metrics && (() => {
                                                        const pnlStr = metrics.maxProfitLossRateMetrics.maxIndividualLossRate.pnl;
                                                        const rateVal = parseFloat(String(metrics.maxProfitLossRateMetrics.maxIndividualLossRate.value).replace('%', ''));
                                                        if (pnlStr === '-' && isNaN(rateVal)) return undefined;
                                                        const pnlVal = pnlStr === '-' ? 0 : Number(pnlStr.replace(/,/g, ''));
                                                        return (pnlVal === 0 && rateVal === 0) ? '#ffffff' : undefined;
                                                    })()
                                                }}>
                                                {metrics && formatPercent(metrics.maxProfitLossRateMetrics.maxIndividualLossRate.value)}
                                            </span>
                                        </div>
                                        <div
                                            className="flex-grow text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">순손실</span>
                                            <span
                                                className={`truncate ${
                                                    metrics && (() => {
                                                        const pnlStr = metrics.maxProfitLossRateMetrics.maxIndividualLossRate.pnl;
                                                        if (pnlStr === '-') return '';
                                                        const pnlVal = Number(pnlStr.replace(/,/g, ''));
                                                        return pnlVal === 0 ? '' : pnlVal < 0 ? 'negative' : '';
                                                    })()
                                                }`}
                                                style={{
                                                    color: metrics && (() => {
                                                        const pnlStr = metrics.maxProfitLossRateMetrics.maxIndividualLossRate.pnl;
                                                        if (pnlStr === '-') return undefined;
                                                        const pnlVal = Number(pnlStr.replace(/,/g, ''));
                                                        return pnlVal === 0 ? '#ffffff' : undefined;
                                                    })()
                                                }}>
                                                {metrics && formatDollar(metrics.maxProfitLossRateMetrics.maxIndividualLossRate.pnl)}
                                            </span>
                                        </div>
                                        <div className="flex-grow text-center px-1 py-2 overflow-hidden">
                                            <span className="text-xs text-[#fff2b3] mb-1 block truncate header-text">청산 시간</span>
                                            <span
                                                className="truncate">{metrics && metrics.maxProfitLossRateMetrics.maxIndividualLossRate.time}</span>
                                        </div>
                                    </div>
                                </td>
                            ) : metric === '최대 전체 순손실률' ? (
                                <td className="p-0 text-center align-middle overflow-hidden">
                                    <div className="flex justify-around items-center h-full">
                                        <div
                                            className="flex-grow text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">최대 전체 순손실률</span>
                                            <span
                                                className={`truncate ${
                                                    metrics && (() => {
                                                        const pnlStr = metrics.maxProfitLossRateMetrics.maxTotalLossRate.pnl;
                                                        const rateVal = parseFloat(String(metrics.maxProfitLossRateMetrics.maxTotalLossRate.value).replace('%', ''));
                                                        if (pnlStr === '-' && isNaN(rateVal)) return '';
                                                        const pnlVal = pnlStr === '-' ? 0 : Number(pnlStr.replace(/,/g, ''));
                                                        return (pnlVal === 0 && rateVal === 0) ? '' : (pnlVal < 0 || rateVal < 0) ? 'negative' : '';
                                                    })()
                                                }`}
                                                style={{
                                                    color: metrics && (() => {
                                                        const pnlStr = metrics.maxProfitLossRateMetrics.maxTotalLossRate.pnl;
                                                        const rateVal = parseFloat(String(metrics.maxProfitLossRateMetrics.maxTotalLossRate.value).replace('%', ''));
                                                        if (pnlStr === '-' && isNaN(rateVal)) return undefined;
                                                        const pnlVal = pnlStr === '-' ? 0 : Number(pnlStr.replace(/,/g, ''));
                                                        return (pnlVal === 0 && rateVal === 0) ? '#ffffff' : undefined;
                                                    })()
                                                }}>
                                                {metrics && formatPercent(metrics.maxProfitLossRateMetrics.maxTotalLossRate.value)}
                                            </span>
                                        </div>
                                        <div
                                            className="flex-grow text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">순손실</span>
                                            <span
                                                className={`truncate ${
                                                    metrics && (() => {
                                                        const pnlStr = metrics.maxProfitLossRateMetrics.maxTotalLossRate.pnl;
                                                        if (pnlStr === '-') return '';
                                                        const pnlVal = Number(pnlStr.replace(/,/g, ''));
                                                        return pnlVal === 0 ? '' : pnlVal < 0 ? 'negative' : '';
                                                    })()
                                                }`}
                                                style={{
                                                    color: metrics && (() => {
                                                        const pnlStr = metrics.maxProfitLossRateMetrics.maxTotalLossRate.pnl;
                                                        if (pnlStr === '-') return undefined;
                                                        const pnlVal = Number(pnlStr.replace(/,/g, ''));
                                                        return pnlVal === 0 ? '#ffffff' : undefined;
                                                    })()
                                                }}>
                                                {metrics && formatDollar(metrics.maxProfitLossRateMetrics.maxTotalLossRate.pnl)}
                                            </span>
                                        </div>
                                        <div className="flex-grow text-center px-1 py-2 overflow-hidden">
                                            <span className="text-xs text-[#fff2b3] mb-1 block truncate header-text">청산 시간</span>
                                            <span
                                                className="truncate">{metrics && metrics.maxProfitLossRateMetrics.maxTotalLossRate.time}</span>
                                        </div>
                                    </div>
                                </td>
                            ) : metric === '드로우다운 평균 깊이' ? (
                                <td className="p-2 text-center align-middle overflow-hidden">
                                    <span
                                        className={`truncate ${metrics && metrics.drawdownMetrics.avgDepth !== '-' ? 'negative' : ''}`}>{metrics && metrics.drawdownMetrics.avgDepth}</span>
                                </td>
                            ) : metric === '드로우다운 하락 기간' ? (
                                <td className="p-0 text-center align-middle overflow-hidden">
                                    <div className="flex justify-around items-center h-full">
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">합계</span>
                                            <span
                                                className={`truncate ${metrics && metrics.drawdownMetrics.totalDownsideDuration !== placeholder ? 'negative' : ''}`}>
                                                {metrics ? metrics.drawdownMetrics.totalDownsideDuration : placeholder}
                                            </span>
                                        </div>
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">평균</span>
                                            <span
                                                className={`truncate ${metrics && metrics.drawdownMetrics.avgDownsideDuration !== placeholder ? 'negative' : ''}`}>
                                                {metrics ? metrics.drawdownMetrics.avgDownsideDuration : placeholder}
                                            </span>
                                        </div>
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">최대</span>
                                            <span
                                                className={`truncate ${metrics && metrics.drawdownMetrics.maxDownsideDuration !== placeholder ? 'negative' : ''}`}>
                                                {metrics ? metrics.drawdownMetrics.maxDownsideDuration : placeholder}
                                            </span>
                                        </div>
                                        <div className="flex-1 text-center px-1 py-2 overflow-hidden">
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">비율</span>
                                            <span
                                                className={`truncate ${metrics && metrics.drawdownMetrics.downsideDurationRatio !== placeholder ? 'negative' : ''}`}>
                                                {metrics ? metrics.drawdownMetrics.downsideDurationRatio : placeholder}
                                            </span>
                                        </div>
                                    </div>
                                </td>
                            ) : metric === '드로우다운 회복 기간' ? (
                                <td className="p-0 text-center align-middle overflow-hidden">
                                    <div className="flex justify-around items-center h-full">
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">합계</span>
                                            <span
                                                className={`truncate ${metrics && metrics.drawdownMetrics.totalRecoveryDuration !== placeholder ? 'positive' : ''}`}>
                                                {metrics ? metrics.drawdownMetrics.totalRecoveryDuration : placeholder}
                                            </span>
                                        </div>
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">평균</span>
                                            <span
                                                className={`truncate ${metrics && metrics.drawdownMetrics.avgRecoveryDuration !== placeholder ? 'positive' : ''}`}>
                                                {metrics ? metrics.drawdownMetrics.avgRecoveryDuration : placeholder}
                                            </span>
                                        </div>
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">최대</span>
                                            <span
                                                className={`truncate ${metrics && metrics.drawdownMetrics.maxRecoveryDuration !== placeholder ? 'positive' : ''}`}>
                                                {metrics ? metrics.drawdownMetrics.maxRecoveryDuration : placeholder}
                                            </span>
                                        </div>
                                        <div className="flex-1 text-center px-1 py-2 overflow-hidden">
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">비율</span>
                                            <span
                                                className={`truncate ${metrics && metrics.drawdownMetrics.recoveryDurationRatio !== placeholder ? 'positive' : ''}`}>
                                                {metrics ? metrics.drawdownMetrics.recoveryDurationRatio : placeholder}
                                            </span>
                                        </div>
                                    </div>
                                </td>
                            ) : metric === '평균 보유 시간' ? (
                                <td className="p-0 text-center align-middle overflow-hidden">
                                    <div className="flex justify-around items-center h-full">
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">전체</span>
                                            <span
                                                className="truncate">{metrics && metrics.holdingTimeMetrics.avgHoldingTime}</span>
                                        </div>
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">수익</span>
                                            <span
                                                className={`truncate ${metrics && metrics.holdingTimeMetrics.profitAvgHoldingTime !== placeholder ? 'positive' : ''}`}>
                                                {metrics ? metrics.holdingTimeMetrics.profitAvgHoldingTime : placeholder}
                                            </span>
                                        </div>
                                        <div className="flex-1 text-center px-1 py-2 overflow-hidden">
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">손실</span>
                                            <span
                                                className={`truncate ${metrics && metrics.holdingTimeMetrics.lossAvgHoldingTime !== placeholder ? 'negative' : ''}`}>
                                                {metrics ? metrics.holdingTimeMetrics.lossAvgHoldingTime : placeholder}
                                            </span>
                                        </div>
                                    </div>
                                </td>
                            ) : metric === '최대 보유 시간' ? (
                                <td className="p-0 text-center align-middle overflow-hidden">
                                    <div className="flex justify-around items-center h-full">
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">수익</span>
                                            <span
                                                className={`truncate ${metrics && metrics.holdingTimeMetrics.maxProfitHoldingTime !== placeholder ? 'positive' : ''}`}>
                                                {metrics ? metrics.holdingTimeMetrics.maxProfitHoldingTime : placeholder}
                                            </span>
                                        </div>
                                        <div className="flex-1 text-center px-1 py-2 overflow-hidden">
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">손실</span>
                                            <span
                                                className={`truncate ${metrics && metrics.holdingTimeMetrics.maxLossHoldingTime !== placeholder ? 'negative' : ''}`}>
                                                {metrics ? metrics.holdingTimeMetrics.maxLossHoldingTime : placeholder}
                                            </span>
                                        </div>
                                    </div>
                                </td>
                            ) : metric === '보유 심볼 수' ? (
                                <td className="p-0 text-center align-middle overflow-hidden">
                                    <div className="flex justify-around items-center h-full">
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">평균</span>
                                            <span className="truncate">{metrics && (() => {
                                                const val = metrics.symbolCountMetrics.avgSymbolCount;
                                                return val === "-" ? "-" : (() => {
                                                    const numVal = Number(val);
                                                    return isNaN(numVal) ? '0' : numVal.toLocaleString('en-US') + '개';
                                                })();
                                            })()}</span>
                                        </div>
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">최대</span>
                                            <span className="truncate">{metrics && (() => {
                                                const val = metrics.symbolCountMetrics.maxSymbolCount;
                                                return val === "-" ? "-" : (() => {
                                                    const numVal = Number(val);
                                                    return isNaN(numVal) ? '0' : numVal.toLocaleString('en-US') + '개';
                                                })();
                                            })()}</span>
                                        </div>
                                        <div className="flex-1 text-center px-1 py-2 overflow-hidden">
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">최소</span>
                                            <span className="truncate">{metrics && (() => {
                                                const val = metrics.symbolCountMetrics.minSymbolCount;
                                                return val === "-" ? "-" : (() => {
                                                    const numVal = Number(val);
                                                    return isNaN(numVal) ? '0' : numVal.toLocaleString('en-US') + '개';
                                                })();
                                            })()}</span>
                                        </div>
                                    </div>
                                </td>
                            ) : metric === '시장 참여 비율' ? (
                                <td className="p-0 text-center align-middle overflow-hidden">
                                    <div className="flex justify-around items-center h-full">
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">전체</span>
                                            <span
                                                className="truncate">{metrics && metrics.holdingTimeMetrics.marketParticipationRate}</span>
                                        </div>
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">수익</span>
                                            <span
                                                className={`truncate ${metrics && metrics.holdingTimeMetrics.profitMarketParticipationRate !== placeholder ? 'positive' : ''}`}>
                                                {metrics ? metrics.holdingTimeMetrics.profitMarketParticipationRate : placeholder}
                                            </span>
                                        </div>
                                        <div className="flex-1 text-center px-1 py-2 overflow-hidden">
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">손실</span>
                                            <span
                                                className={`truncate ${metrics && metrics.holdingTimeMetrics.lossMarketParticipationRate !== placeholder ? 'negative' : ''}`}>
                                                {metrics ? metrics.holdingTimeMetrics.lossMarketParticipationRate : placeholder}
                                            </span>
                                        </div>
                                    </div>
                                </td>
                            ) : metric === '보유 시간 합계' ? (
                                <td className="p-0 text-center align-middle overflow-hidden">
                                    <div className="flex justify-around items-center h-full">
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">전체</span>
                                            <span
                                                className="truncate">{metrics && metrics.holdingTimeMetrics.totalHoldingTime}</span>
                                        </div>
                                        <div
                                            className="flex-1 text-center px-1 py-2 border-r last:border-r-0 overflow-hidden"
                                            style={{borderRightColor: 'rgba(255, 215, 0, 0.4)'}}>
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">수익</span>
                                            <span
                                                className={`truncate ${metrics && metrics.holdingTimeMetrics.profitTotalHoldingTime !== placeholder ? 'positive' : ''}`}>
                                                {metrics ? metrics.holdingTimeMetrics.profitTotalHoldingTime : placeholder}
                                            </span>
                                        </div>
                                        <div className="flex-1 text-center px-1 py-2 overflow-hidden">
                                            <span
                                                className="text-xs text-[#fff2b3] mb-1 block truncate header-text">손실</span>
                                            <span
                                                className={`truncate ${metrics && metrics.holdingTimeMetrics.lossTotalHoldingTime !== placeholder ? 'negative' : ''}`}>
                                                {metrics ? metrics.holdingTimeMetrics.lossTotalHoldingTime : placeholder}
                                            </span>
                                        </div>
                                    </div>
                                </td>
                            ) : metric === '기대값' ? (
                                <td className="p-2 text-center align-middle overflow-hidden">
                                    <span
                                        className={`truncate ${
                                            (() => {
                                                const val = reportData[metric];
                                                if (val === "-") return '';
                                                const numVal = parseFloat(val.replace(/[$,]/g, ''));
                                                if (isNaN(numVal)) return '';
                                                if (numVal === 0) return '';
                                                if (numVal > 0) return 'positive';
                                                return 'negative';
                                            })()
                                        }`}
                                        style={{
                                            color: (() => {
                                                const val = reportData[metric];
                                                if (val === "-") return '#ffffff';
                                                return undefined;
                                            })()
                                        }}
                                    >
                                        {reportData[metric as keyof typeof reportData]}
                                    </span>
                                </td>
                            ) : (
                                <td className="p-2 text-center align-middle overflow-hidden">
                                    <span className="truncate">{reportData[metric as keyof typeof reportData]}</span>
                                </td>
                            )}
                        </tr>
                    ))}
                    </tbody>
                </table>
            )}
        </div>
    );
};

export default Report;