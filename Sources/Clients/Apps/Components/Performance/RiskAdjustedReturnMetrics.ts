import {TradeItem} from "@/Components/TradeFilter/TradeFilterContext";
import {safeNumber} from './Utils.ts';
import {RiskRewardMetricsResult} from "./RiskRewardMetrics";
import {AverageProfitLossMetricsResult} from "./AverageProfitLossMetrics";

export interface RiskAdjustedReturnMetricsResult {
    calmarRatio: string;
    sharpeRatio: string;
    sortinoRatio: string;
    riskFreeRate: string;
    isLoading: boolean;
    cacheKey?: string;
}

// 안전한 숫자 포맷팅 함수 (toFixed 버그 회피)
// 동작 요약:
// - 무한대/NaN/비유한 값 처리
// - 기본적으로 요청된 decimals 자리까지 반올림하여 표시하되, trailing zeros는 제거
// - 만약 abs(value)를 요청된 자리로 반올림하면 0이 되는 경우(예: 0.0045, decimals=2)
//   최초로 0이 아닌 소수 자리부터 시작해 그 자리 포함 2자리까지 표시하고, trailing zeros 제거
const safeFormatNumber = (value: number, decimals: number): string => {
    // 무한대와 NaN 처리
    if (value === Infinity) return "∞";
    if (value === -Infinity) return "-∞";
    if (Number.isNaN(value)) return "-";
    if (!Number.isFinite(value)) return "-";

    const sign = value < 0 ? '-' : '';
    const absValue = Math.abs(value);

    // 요청된 소수 자리로 반올림했을 때 0이 아닌지 확인
    const factor = Math.pow(10, decimals);
    const roundedToRequested = Math.round(absValue * factor) / factor;

    // 정확히 0이면 (값이 0이거나 너무 작아서 반올림 시 0) 처리
    if (roundedToRequested === 0) {
        // 값 자체가 0이면 표준 0 출력
        if (absValue === 0) {
            return decimals > 0 ? '0.' + '0'.repeat(decimals) : '0';
        }

        // 작은 값인 경우: 최초 비0 소수 자리부터 2자리까지 보여주고 trailing zeros 제거
        const MAX_CHECK_DECIMALS = 15; // double 정밀도 고려
        const fixed = absValue.toFixed(MAX_CHECK_DECIMALS); // 충분히 긴 고정 소수 문자열
        const parts = fixed.split('.');
        if (parts.length === 1) {
            return sign + parts[0];
        }

        const frac = parts[1];
        // 최초 비0 자리 찾기
        let firstNonZero = -1;
        for (let i = 0; i < frac.length; i++) {
            if (frac[i] !== '0') {
                firstNonZero = i;
                break;
            }
        }

        if (firstNonZero === -1) {
            // 모든 소수 자리가 0이면 0으로 처리
            return decimals > 0 ? '0.' + '0'.repeat(decimals) : '0';
        }

        const neededDecimals = Math.min(MAX_CHECK_DECIMALS, firstNonZero + 2); // 최초 비0 자리 포함 2자리
        let result = absValue.toFixed(neededDecimals);

        // 소수점 우측의 불필요한 0 제거
        if (result.indexOf('.') >= 0) {
            result = result.replace(/(?:\.0+|(?<=\.[0-9]*[1-9])0+)$/, match => {
                // 단순 제거 패턴보다 안전하게 처리: 아래 fallback로 처리
                return match;
            });
            // 일반적인 방법으로 trailing zeros 제거
            result = result.replace(/(\.\d*?)0+$/, '$1');
            result = result.replace(/\.$/, '');
        }

        return sign + result;
    }

    // 요청된 자리에서 비0이면, 해당 자리까지 반올림하여 표시하되 trailing zeros 제거
    let formatted = roundedToRequested.toFixed(decimals);
    if (formatted.indexOf('.') >= 0) {
        // 소수점 뒤의 불필요한 0 제거
        formatted = formatted.replace(/(\.\d*?)0+$/, '$1');
        formatted = formatted.replace(/\.$/, '');
    }

    return sign + formatted;
};

// 캐시 및 기본값
const DEFAULT_RISK_FREE_RATE = 0.03; // 3%
let cachedRiskFreeRate: number | null = null;
let metricsCache: RiskAdjustedReturnMetricsResult | null = null;
let isFetchingRiskFreeRate = false;

// 미국 10년 국채 수익률을 가져오는 함수
const fetchTreasuryYield = async (): Promise<number> => {
    try {
        const response = await fetch('https://api.fiscaldata.treasury.gov/services/api/fiscal_service/v2/accounting/od/avg_interest_rates?sort=-record_date&filter=security_desc:eq:Treasury Bonds&page[size]=1');
        const data = await response.json();

        return data.data && data.data.length > 0
            ? parseFloat(data.data[0].avg_interest_rate_amt) / 100
            : DEFAULT_RISK_FREE_RATE;
    } catch (error) {
        return DEFAULT_RISK_FREE_RATE;
    }
};

// 일별 포트폴리오 가치 계산 함수
const buildDailyPortfolioValues = (actualTrades: TradeItem[]): { dates: Date[], values: number[] } => {
    if (actualTrades.length === 0) return {dates: [], values: []};

    // 초기 자본금은 거래번호 0번의 '현재 자금'
    const firstTrade = actualTrades[0];
    const initialCapital = safeNumber(firstTrade["현재 자금"] || 10000);

    // 날짜별 자본금 변화 추적
    const dateValueMap = new Map<string, number>();

    // 첫 번째 거래일 이전의 초기 자본금 설정
    const firstTradeDate = new Date(String(firstTrade["진입 시간"] || 0));
    const startDate = new Date(firstTradeDate.getTime() - 24 * 60 * 60 * 1000); // 하루 전
    const startDateKey = startDate.toISOString().split('T')[0];
    dateValueMap.set(startDateKey, initialCapital);

    // 각 거래의 청산 시점에서 자본금 업데이트
    actualTrades.forEach(trade => {
        const exitTime = String(trade["청산 시간"] || 0);
        if (!exitTime || exitTime === "0") return;

        const exitDate = new Date(exitTime);
        const exitDateKey = exitDate.toISOString().split('T')[0];
        const currentCapital = safeNumber(trade["현재 자금"] || 0);

        // 해당 날짜의 최종 자본금으로 업데이트
        dateValueMap.set(exitDateKey, currentCapital);
    });

    // 날짜 정렬 및 누락된 날짜 보간
    const sortedDates = Array.from(dateValueMap.keys()).sort();
    const allDates: Date[] = [];
    const allValues: number[] = [];

    if (sortedDates.length === 0) return {dates: [], values: []};

    // 시작일부터 마지막일까지 모든 날짜 생성
    const startDateObj = new Date(sortedDates[0]);
    const endDateObj = new Date(sortedDates[sortedDates.length - 1]);

    let currentDate = new Date(startDateObj);
    let lastKnownValue = dateValueMap.get(sortedDates[0]) || initialCapital;

    while (currentDate <= endDateObj) {
        const dateKey = currentDate.toISOString().split('T')[0];
        const value = dateValueMap.get(dateKey);

        if (value !== undefined) {
            lastKnownValue = value;
        }

        allDates.push(new Date(currentDate));
        allValues.push(lastKnownValue);

        currentDate.setDate(currentDate.getDate() + 1);
    }

    return {dates: allDates, values: allValues};
};

// 일별 수익률 계산 함수
const calculateDailyReturns = (values: number[]): number[] => {
    const returns: number[] = [];

    for (let i = 1; i < values.length; i++) {
        const previousValue = values[i - 1];
        const currentValue = values[i];

        if (previousValue > 0) {
            const dailyReturn = (currentValue - previousValue) / previousValue;
            returns.push(dailyReturn);
        } else {
            returns.push(0); // 이전 값이 0 이하면 0% 수익률
        }
    }

    return returns;
};

// 표준편차 계산 함수
const calculateStandardDeviation = (values: number[]): number => {
    const n = values.length;
    if (n < 2) return 0;

    const mean = values.reduce((a, b) => a + b, 0) / n;
    const variance = values.reduce((a, b) => a + (b - mean) ** 2, 0) / (n - 1);
    return Math.sqrt(variance);
};

// 하방 표준편차 계산 함수 (무위험 수익률보다 낮은 수익률만 사용)
const calculateDownsideDeviation = (values: number[], targetReturn: number): number => {
    const negativeReturns = values.filter(r => r < targetReturn)
        .map(r => r - targetReturn);

    const n = negativeReturns.length;
    if (n === 0) return 0;

    const sumOfSquaredDifferences = negativeReturns.reduce((acc, r) => acc + r ** 2, 0);
    const downsideVariance = sumOfSquaredDifferences / n;
    return Math.sqrt(downsideVariance);
};

// 메트릭 계산 및 캐시 업데이트 함수
const calculateAndUpdateMetrics = (
    actualTrades: TradeItem[],
    cagrStr: string,
    riskRewardMetrics: RiskRewardMetricsResult,
    cacheKey: string
): RiskAdjustedReturnMetricsResult => {
    const riskFreeRate = cachedRiskFreeRate || DEFAULT_RISK_FREE_RATE;

    // CAGR과 MDD 파싱
    const cagr = parseFloat(cagrStr.replace('%', '')) / 100;
    const annualMdd = parseFloat(riskRewardMetrics.mdd.replace('%', '')) / 100;

    // 일별 포트폴리오 가치 재구성
    const {dates, values} = buildDailyPortfolioValues(actualTrades);

    if (dates.length < 2) {
        return {
            calmarRatio: "-",
            sharpeRatio: "-",
            sortinoRatio: "-",
            riskFreeRate: safeFormatNumber(riskFreeRate * 100, 2) + '%',
            isLoading: false,
            cacheKey
        };
    }

    // 일별 수익률 계산
    const dailyReturns = calculateDailyReturns(values);

    if (dailyReturns.length === 0) {
        return {
            calmarRatio: "-",
            sharpeRatio: "-",
            sortinoRatio: "-",
            riskFreeRate: safeFormatNumber(riskFreeRate * 100, 2) + '%',
            isLoading: false,
            cacheKey
        };
    }

    // 연간 무위험 수익률을 일별로 변환
    const dailyRiskFreeRate = Math.pow(1 + riskFreeRate, 1 / 365.25) - 1;

    // 일별 평균 수익률 계산
    const dailyAvgReturn = dailyReturns.reduce((sum, ret) => sum + ret, 0) / dailyReturns.length;

    // 일별 표준편차 계산
    const dailyStdDev = calculateStandardDeviation(dailyReturns);

    // 일별 하방 표준편차 계산
    const dailyDownsideDev = calculateDownsideDeviation(dailyReturns, dailyRiskFreeRate);

    // 칼마 비율 계산 (CAGR / MDD)
    let calmarRatioStr: string;
    if (cagr === 0) {
        calmarRatioStr = "-";
    } else if (annualMdd <= 0) {
        calmarRatioStr = "∞";
    } else {
        const rawCalmar = cagr / annualMdd;
        calmarRatioStr = safeFormatNumber(rawCalmar, 2);
    }

    // 일별 샤프 비율 계산
    const dailySharpeRatio = dailyStdDev > 0
        ? (dailyAvgReturn - dailyRiskFreeRate) / dailyStdDev
        : (dailyAvgReturn > dailyRiskFreeRate ? Infinity : 0);

    // 일별 소르티노 비율 계산
    const dailySortinoRatio = dailyDownsideDev > 0
        ? (dailyAvgReturn - dailyRiskFreeRate) / dailyDownsideDev
        : (dailyAvgReturn > dailyRiskFreeRate ? Infinity : 0);

    // 연간 비율로 변환 (√365.25로 곱하여 연율화)
    const annualSharpeRatio = dailySharpeRatio * Math.sqrt(365.25);
    const annualSortinoRatio = dailySortinoRatio * Math.sqrt(365.25);

    // 결과 객체 생성
    const result: RiskAdjustedReturnMetricsResult = {
        calmarRatio: calmarRatioStr,
        sharpeRatio: actualTrades.length <= 2
            ? "-"
            : safeFormatNumber(annualSharpeRatio, 2),
        sortinoRatio: actualTrades.length <= 2
            ? "-"
            : safeFormatNumber(annualSortinoRatio, 2),
        riskFreeRate: safeFormatNumber(riskFreeRate * 100, 2) + '%',
        isLoading: false,
        cacheKey
    };

    // 캐시에 저장
    metricsCache = result;

    return result;
};

// 무위험 수익률 로드 함수
const loadRiskFreeRate = async (): Promise<number | null> => {
    if (isFetchingRiskFreeRate || cachedRiskFreeRate !== null) return cachedRiskFreeRate;

    isFetchingRiskFreeRate = true;
    try {
        cachedRiskFreeRate = await fetchTreasuryYield();
        return cachedRiskFreeRate;
    } catch (error) {
        cachedRiskFreeRate = DEFAULT_RISK_FREE_RATE;
        return cachedRiskFreeRate;
    } finally {
        isFetchingRiskFreeRate = false;
    }
};

// 메인 계산 함수
export const calculateRiskAdjustedReturnMetrics = (
    actualTrades: TradeItem[],
    cagrStr: string,
    riskRewardMetrics: RiskRewardMetricsResult,
    avgMetrics: AverageProfitLossMetricsResult
): RiskAdjustedReturnMetricsResult => {
    // 트레이드 데이터가 없으면 기본값 반환
    if (actualTrades.length === 0) {
        return {
            calmarRatio: "-",
            sharpeRatio: "-",
            sortinoRatio: "-",
            riskFreeRate: "-",
            isLoading: false
        };
    }

    // 캐시 키 생성
    const cacheKey = `${actualTrades.length}-${cagrStr}-${riskRewardMetrics.mdd}-${avgMetrics.avgPnL}`;
    if (metricsCache?.cacheKey === cacheKey && !metricsCache.isLoading) {
        return metricsCache;
    }

    // 무위험 수익률이 없으면 로딩 상태로 반환하고 비동기 로드 시작
    if (cachedRiskFreeRate === null) {
        const loadingResult: RiskAdjustedReturnMetricsResult = {
            calmarRatio: "-",
            sharpeRatio: "-",
            sortinoRatio: "-",
            riskFreeRate: "로딩 중...",
            isLoading: true,
            cacheKey
        };

        metricsCache = loadingResult;

        loadRiskFreeRate().then().catch(error => {
            console.error('[RiskAdjusted] Failed to load initial risk-free rate:', error);
        });

        return loadingResult;
    }

    // 무위험 수익률이 있으면 메트릭 계산
    return calculateAndUpdateMetrics(actualTrades, cagrStr, riskRewardMetrics, cacheKey);
};

// 모듈 최하단에서 초기 로드 시작
loadRiskFreeRate().then().catch(error => {
    console.error('[RiskAdjusted] Failed to load initial risk-free rate:', error);
});
