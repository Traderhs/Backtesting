// Advanced Trade Filter Worker - 고급 최적화 기능이 적용된 워커
interface TradeItem {
    [key: string]: any;
}

interface TradeFilter {
    recalculateBalance: boolean;
    tradeNumberMin?: number;
    tradeNumberMax?: number;
    strategies: string[];
    symbols: string[];
    entryNames: string[];
    exitNames: string[];
    entryDirections: string[];
    entryTimeMin?: string;
    entryTimeMax?: string;
    exitTimeMin?: string;
    exitTimeMax?: string;
    entryYears: number[];
    entryMonths: number[];
    entryDays: number[];
    entryDayOfWeeks: number[];
    entryHours: number[];
    entryMinutes: number[];
    entrySeconds: number[];
    exitYears: number[];
    exitMonths: number[];
    exitDays: number[];
    exitDayOfWeeks: number[];
    exitHours: number[];
    exitMinutes: number[];
    exitSeconds: number[];
    holdingTimeMin?: number;
    holdingTimeMax?: number;
    leverageMin?: number;
    leverageMax?: number;
    entryPriceMin?: number;
    entryPriceMax?: number;
    entryQuantityMin?: number;
    entryQuantityMax?: number;
    exitPriceMin?: number;
    exitPriceMax?: number;
    exitQuantityMin?: number;
    exitQuantityMax?: number;
    forcedLiquidationPriceMin?: number;
    forcedLiquidationPriceMax?: number;
    fundingReceiveCountMin?: number;
    fundingReceiveCountMax?: number;
    fundingReceiveFeeMin?: number;
    fundingReceiveFeeMax?: number;
    fundingPayCountMin?: number;
    fundingPayCountMax?: number;
    fundingPayFeeMin?: number;
    fundingPayFeeMax?: number;
    fundingCountMin?: number;
    fundingCountMax?: number;
    fundingFeeMin?: number;
    fundingFeeMax?: number;
    entryFeeMin?: number;
    entryFeeMax?: number;
    exitFeeMin?: number;
    exitFeeMax?: number;
    forcedLiquidationFeeMin?: number;
    forcedLiquidationFeeMax?: number;
    profitLossMin?: number;
    profitLossMax?: number;
    netProfitLossMin?: number;
    netProfitLossMax?: number;
    individualProfitRateMin?: number;
    individualProfitRateMax?: number;
    overallProfitRateMin?: number;
    overallProfitRateMax?: number;
    currentCapitalMin?: number;
    currentCapitalMax?: number;
    highestCapitalMin?: number;
    highestCapitalMax?: number;
    drawdownMin?: number;
    drawdownMax?: number;
    maxDrawdownMin?: number;
    maxDrawdownMax?: number;
    accumulatedProfitLossMin?: number;
    accumulatedProfitLossMax?: number;
    accumulatedProfitRateMin?: number;
    accumulatedProfitRateMax?: number;
    heldSymbolsCountMin?: number;
    heldSymbolsCountMax?: number;
}

// 스마트 청크 분할 전략
interface ChunkStrategy {
    byStrategy?: boolean;      // 전략별로 분할
    bySymbol?: boolean;        // 심볼별로 분할
    byTimeRange?: boolean;     // 시간 범위별로 분할
    maxChunkSize?: number;     // 최대 청크 크기
}

interface AdvancedWorkerMessage {
    type: string;
    trades: TradeItem[];
    filter: TradeFilter;
    chunkStrategy?: ChunkStrategy;
    taskId?: string;
    chunkIndex?: number;
    totalChunks?: number;
}

// 초고속 날짜 파싱 함수
const fastDateParse = (dateString: string | undefined): Date | undefined => {
    if (!dateString) return undefined;
    
    const str = dateString.trim();
    if (!str) return undefined;
    
    // 빠른 ISO 날짜 파싱
    if (str.length >= 19 && str[4] === '-' && str[7] === '-') {
        // "2024-01-01T12:00:00" 또는 "2024-01-01 12:00:00" 형태
        const year = parseInt(str.substr(0, 4), 10);
        const month = parseInt(str.substr(5, 2), 10) - 1; // 0-based
        const day = parseInt(str.substr(8, 2), 10);
        const hour = parseInt(str.substr(11, 2), 10);
        const minute = parseInt(str.substr(14, 2), 10);
        const second = parseInt(str.substr(17, 2), 10);
        
        // 기본적인 유효성만 검증 (성능 최우선)
        if (year >= 2020 && year <= 2030 && month >= 0 && month <= 11 && 
            day >= 1 && day <= 31 && hour >= 0 && hour <= 23 && 
            minute >= 0 && minute <= 59 && second >= 0 && second <= 59) {
            return new Date(year, month, day, hour, minute, second);
        }
    }
    
    // 백업: 네이티브 파싱 (성능 저하 최소화)
    const date = new Date(str);
    return isNaN(date.getTime()) ? undefined : date;
};

// 스마트 필터링 - 조건에 따라 최적 경로 선택
const smartFilter = (trades: TradeItem[], filter: TradeFilter): TradeItem[] => {
    if (trades.length <= 1) return trades;

    // 필터 복잡도 분석
    const complexity = analyzeFilterComplexity(filter);
    
    if (complexity.isSimple) {
        return fastSimpleFilter(trades, filter);
    } else if (complexity.hasTimeFilters) {
        return timeOptimizedFilter(trades, filter);
    } else {
        return generalOptimizedFilter(trades, filter);
    }
};

// 필터 복잡도 분석
const analyzeFilterComplexity = (filter: TradeFilter) => {
    const hasTimeFilters = (
        filter.entryTimeMin !== undefined || filter.entryTimeMax !== undefined ||
        filter.exitTimeMin !== undefined || filter.exitTimeMax !== undefined ||
        filter.entryYears.length > 0 || filter.entryMonths.length > 0 ||
        filter.exitYears.length > 0 || filter.exitMonths.length > 0 ||
        filter.holdingTimeMin !== undefined || filter.holdingTimeMax !== undefined
    );

    const hasComplexFilters = (
        filter.strategies.length > 5 || filter.symbols.length > 10 ||
        hasTimeFilters ||
        Object.keys(filter).filter(key => 
            key.includes('Min') || key.includes('Max')
        ).length > 10
    );

    const isSimple = !hasComplexFilters && !hasTimeFilters;

    return { isSimple, hasTimeFilters, hasComplexFilters };
};

// 간단한 필터를 위한 고속 처리
const fastSimpleFilter = (trades: TradeItem[], filter: TradeFilter): TradeItem[] => {
    const result = [trades[0]]; // 헤더
    
    const strategySet = filter.strategies.length > 0 ? new Set(filter.strategies) : null;
    const symbolSet = filter.symbols.length > 0 ? new Set(filter.symbols) : null;
    
    for (let i = 1; i < trades.length; i++) {
        const trade = trades[i];
        
        // 빠른 문자열 필터
        if (strategySet && !strategySet.has(String(trade["전략 이름"]))) continue;
        if (symbolSet && !symbolSet.has(String(trade["심볼 이름"]))) continue;
        
        // 간단한 숫자 필터
        if (filter.tradeNumberMin !== undefined && Number(trade["거래 번호"]) < filter.tradeNumberMin) continue;
        if (filter.tradeNumberMax !== undefined && Number(trade["거래 번호"]) > filter.tradeNumberMax) continue;
        
        result.push(trade);
    }
    
    return result;
};

// 시간 필터 최적화 버전
const timeOptimizedFilter = (trades: TradeItem[], filter: TradeFilter): TradeItem[] => {
    const result = [trades[0]]; // 헤더
    
    // 시간 필터 사전 준비
    const entryTimeMin = filter.entryTimeMin ? fastDateParse(filter.entryTimeMin) : undefined;
    const entryTimeMax = filter.entryTimeMax ? fastDateParse(filter.entryTimeMax) : undefined;
    const exitTimeMin = filter.exitTimeMin ? fastDateParse(filter.exitTimeMin) : undefined;
    const exitTimeMax = filter.exitTimeMax ? fastDateParse(filter.exitTimeMax) : undefined;
    
    for (let i = 1; i < trades.length; i++) {
        const trade = trades[i];
        
        // 시간 필터 확인
        if (entryTimeMin || entryTimeMax) {
            const entryTime = fastDateParse(String(trade["진입 시간"]));
            if (!entryTime) continue;
            if (entryTimeMin && entryTime < entryTimeMin) continue;
            if (entryTimeMax && entryTime > entryTimeMax) continue;
        }
        
        if (exitTimeMin || exitTimeMax) {
            const exitTime = fastDateParse(String(trade["청산 시간"]));
            if (!exitTime) continue;
            if (exitTimeMin && exitTime < exitTimeMin) continue;
            if (exitTimeMax && exitTime > exitTimeMax) continue;
        }
        
        result.push(trade);
    }
    
    return result;
};

// 일반 최적화 필터
const generalOptimizedFilter = (trades: TradeItem[], filter: TradeFilter): TradeItem[] => {
    // 기존 최적화된 로직 사용 (기존 tradeFilterWorker.ts의 applyAllFiltersAtOnce 로직)
    return applyOptimizedFilter(trades, filter);
};

// 기존 최적화 로직 (간소화 버전)
const applyOptimizedFilter = (trades: TradeItem[], filter: TradeFilter): TradeItem[] => {
    const result = [trades[0]]; // 헤더
    
    // 필터 준비
    const strategySet = filter.strategies.length > 0 ? new Set(filter.strategies) : null;
    const symbolSet = filter.symbols.length > 0 ? new Set(filter.symbols) : null;
    
    for (let i = 1; i < trades.length; i++) {
        const trade = trades[i];
        
        // 기본 필터들
        if (strategySet && !strategySet.has(String(trade["전략 이름"]))) continue;
        if (symbolSet && !symbolSet.has(String(trade["심볼 이름"]))) continue;
        
        // 거래 번호 필터
        if (filter.tradeNumberMin !== undefined && Number(trade["거래 번호"]) < filter.tradeNumberMin) continue;
        if (filter.tradeNumberMax !== undefined && Number(trade["거래 번호"]) > filter.tradeNumberMax) continue;
        
        result.push(trade);
    }
    
    return result;
};

// 워커 메시지 핸들러
self.addEventListener('message', (event) => {
    const { type, trades, filter, taskId, chunkIndex, totalChunks }: AdvancedWorkerMessage = event.data;

    if (type === 'filter') {
        try {
            const startTime = performance.now();
            
            // 스마트 필터링 실행
            const result = smartFilter(trades, filter);
            
            const endTime = performance.now();
            const processingTime = Math.round(endTime - startTime);
            
            self.postMessage({
                type: 'filterResult',
                trades: result,
                id: taskId,
                chunkIndex: chunkIndex || 0,
                totalChunks: totalChunks || 1,
                processingTime
            });
        } catch (error) {
            console.error('고급 필터링 중 오류:', error);
            self.postMessage({
                type: 'filterResult',
                trades: [],
                id: taskId,
                chunkIndex: chunkIndex || 0,
                totalChunks: totalChunks || 1,
                error: error instanceof Error ? error.message : String(error)
            });
        }
    }
});

export {};
