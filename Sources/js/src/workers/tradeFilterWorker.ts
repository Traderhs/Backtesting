// Trade Filter Worker - 백그라운드에서 필터링 처리
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

// 메시지 타입
interface WorkerMessage {
    type: string;
    trades: TradeItem[];
    filter: TradeFilter;
    allTrades: TradeItem[];
    taskId?: string;
    chunkIndex?: number;
    totalChunks?: number;
}

// 초고속 날짜 파싱 함수
const getValidDate = (dateString: string | undefined): Date | undefined => {
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
        if (year >= 1900 && year <= 2100 && month >= 0 && month <= 11 &&
            day >= 1 && day <= 31 && hour >= 0 && hour <= 23 &&
            minute >= 0 && minute <= 59 && second >= 0 && second <= 59) {
            return new Date(year, month, day, hour, minute, second);
        }
    }

    // 백업: 네이티브 파싱 (성능 저하 최소화)
    const date = new Date(str);
    return isNaN(date.getTime()) ? undefined : date;
};

// 최적화된 자금 재계산 함수
const recalculateBalance = (filtered: TradeItem[], allTrades: TradeItem[]): TradeItem[] => {
    if (filtered.length <= 1) return filtered; // 헤더만 있는 경우

    const initialBalance = allTrades.length > 0 ? Number(allTrades[0]["현재 자금"]) : 0;
    let currentBalance = initialBalance;
    let maxBalance = initialBalance;
    let maxDrawdown = 0;

    // 거래 번호 재매기기 - 최적화된 버전
    const tradeNumberMap = new Map<number, number>();
    let newTradeNumber = 1;
    let lastOriginalTradeNumber: number | null = null;

    // 첫 번째 패스: 거래 번호 재매핑만 수행
    for (let i = 1; i < filtered.length; i++) {
        const originalTradeNumber = Number(filtered[i]["거래 번호"]);

        if (originalTradeNumber !== lastOriginalTradeNumber) {
            if (!tradeNumberMap.has(originalTradeNumber)) {
                tradeNumberMap.set(originalTradeNumber, newTradeNumber);
                newTradeNumber++;
            }
            lastOriginalTradeNumber = originalTradeNumber;
        }
    }

    // 파산 시점 찾기 - 더 효율적인 방식
    let bankruptIndex = -1;
    let tempBalance = initialBalance;

    for (let i = 1; i < filtered.length; i++) {
        const profit = Number(filtered[i]["순손익"]);
        tempBalance += profit;

        if (tempBalance < 0) {
            bankruptIndex = i;
            break;
        }
    }

    const endIndex = bankruptIndex > 0 ? bankruptIndex : filtered.length;

    // 결과 배열 미리 할당
    const result = new Array(endIndex);
    result[0] = filtered[0]; // 헤더 복사

    // 진입 시점 자금 매핑 - O(n²) → O(n) 최적화
    const entryCapitalCache = new Map<string, number>();

    for (let i = 1; i < endIndex; i++) {
        const trade = filtered[i];
        const originalTradeNumber = Number(trade["거래 번호"]);
        const mappedNumber = tradeNumberMap.get(originalTradeNumber) || originalTradeNumber;

        // 현재 거래의 손익 처리
        const profit = Number(trade["순손익"]);
        currentBalance += profit;
        maxBalance = Math.max(maxBalance, currentBalance);
        const drawdown = maxBalance > 0 ? (1 - currentBalance / maxBalance) * 100 : 0;
        maxDrawdown = Math.max(maxDrawdown, drawdown);

        // 전체 순손익률 계산을 위한 진입 시점 자금 찾기
        let entryCapital = initialBalance;
        const currentEntryTimeStr = String(trade["진입 시간"]);
        const cacheKey = `${mappedNumber}_${currentEntryTimeStr}`;

        if (entryCapitalCache.has(cacheKey)) {
            entryCapital = entryCapitalCache.get(cacheKey)!;
        } else {
            // 이전 거래들 중에서 진입 시점 자금 찾기
            const currentEntryTime = new Date(currentEntryTimeStr);
            let mostRecentExitTime = new Date(0);
            let bestTradeNumber = -1;

            for (let j = 1; j < i; j++) {
                const prevTrade = result[j] || filtered[j];
                const prevExitTimeStr = String(prevTrade["청산 시간"]);

                if (prevExitTimeStr === '-') continue;

                const prevExitTime = new Date(prevExitTimeStr);
                const prevTradeNumber = Number(prevTrade["거래 번호"]);

                if (prevExitTime <= currentEntryTime && prevTradeNumber < mappedNumber) {
                    // 더 최근의 청산 시간이거나, 같은 청산 시간이지만 더 큰 거래 번호인 경우
                    if (prevExitTime > mostRecentExitTime ||
                        (prevExitTime.getTime() === mostRecentExitTime.getTime() && prevTradeNumber > bestTradeNumber)) {
                        mostRecentExitTime = prevExitTime;
                        bestTradeNumber = prevTradeNumber;
                        entryCapital = Number(prevTrade["현재 자금"]);
                    }
                }
            }

            entryCapitalCache.set(cacheKey, entryCapital);
        }

        const overallProfitRate = entryCapital !== 0 ? (profit / entryCapital) * 100 : 0;

        // 객체 스프레드 연산자 대신 직접 할당으로 성능 향상
        result[i] = {
            ...trade,
            "거래 번호": mappedNumber,
            "전체 순손익률": overallProfitRate,
            "현재 자금": currentBalance,
            "최고 자금": maxBalance,
            "드로우다운": drawdown,
            "최고 드로우다운": maxDrawdown,
            "누적 손익": currentBalance - initialBalance,
            "누적 손익률": initialBalance !== 0 ? ((currentBalance - initialBalance) / initialBalance) * 100 : 0,
            "보유 심볼 수": "-"
        };
    }

    return result;
};


// 필터 적용 메인 함수 - 모든 필터 AND 적용 후 자금 재계산
const applyFiltersInOrder = (trades: TradeItem[], filter: TradeFilter, allTrades: TradeItem[]): {
    result: TradeItem[],
    hasBankruptcy: boolean
} => {
    // 모든 필터를 AND로 한 번에 적용
    const filteredTrades = applyAllFiltersAtOnce(trades, filter);

    // 자금 재계산이 켜져있고 필터링된 거래가 있으면 자금 재계산 수행
    if (filter.recalculateBalance && filteredTrades.length > 1) {
        const recalculated = recalculateBalance(filteredTrades, allTrades);
        // 파산 여부 확인: recalculated가 filteredTrades보다 짧으면 파산 발생
        const hasBankruptcy = recalculated.length < filteredTrades.length;
        return {result: recalculated, hasBankruptcy};
    }

    return {result: filteredTrades, hasBankruptcy: false};
};

// 최적화된 모든 필터를 한 번에 적용하는 함수
const applyAllFiltersAtOnce = (trades: TradeItem[], filter: TradeFilter): TradeItem[] => {
    // 입력 검증
    if (!trades || !Array.isArray(trades) || trades.length === 0) {
        return [];
    }

    if (!filter) {
        return trades;
    }

    // 빠른 경로: 진짜 활성 필터가 있는지 확인 (빈 배열도 활성 필터)
    const hasAnyFilter = (
        // 거래 번호 필터
        filter.tradeNumberMin !== undefined || filter.tradeNumberMax !== undefined ||

        // 문자열 필터들 (배열이 존재하면 활성 - 빈 배열도 포함)
        filter.strategies !== undefined ||
        filter.symbols !== undefined ||
        filter.entryNames !== undefined ||
        filter.exitNames !== undefined ||
        filter.entryDirections !== undefined ||

        // 시간 필터들 (배열이 존재하면 활성 - 빈 배열도 포함)
        filter.entryTimeMin !== undefined || filter.entryTimeMax !== undefined ||
        filter.exitTimeMin !== undefined || filter.exitTimeMax !== undefined ||
        filter.entryYears !== undefined || filter.entryMonths !== undefined ||
        filter.entryDays !== undefined || filter.entryDayOfWeeks !== undefined ||
        filter.entryHours !== undefined || filter.entryMinutes !== undefined ||
        filter.entrySeconds !== undefined ||
        filter.exitYears !== undefined || filter.exitMonths !== undefined ||
        filter.exitDays !== undefined || filter.exitDayOfWeeks !== undefined ||
        filter.exitHours !== undefined || filter.exitMinutes !== undefined ||
        filter.exitSeconds !== undefined ||

        // 보유 시간 필터
        filter.holdingTimeMin !== undefined || filter.holdingTimeMax !== undefined ||

        // 숫자 범위 필터들
        filter.leverageMin !== undefined || filter.leverageMax !== undefined ||
        filter.entryPriceMin !== undefined || filter.entryPriceMax !== undefined ||
        filter.entryQuantityMin !== undefined || filter.entryQuantityMax !== undefined ||
        filter.exitPriceMin !== undefined || filter.exitPriceMax !== undefined ||
        filter.exitQuantityMin !== undefined || filter.exitQuantityMax !== undefined ||
        filter.forcedLiquidationPriceMin !== undefined || filter.forcedLiquidationPriceMax !== undefined ||
        filter.fundingReceiveCountMin !== undefined || filter.fundingReceiveCountMax !== undefined ||
        filter.fundingReceiveFeeMin !== undefined || filter.fundingReceiveFeeMax !== undefined ||
        filter.fundingPayCountMin !== undefined || filter.fundingPayCountMax !== undefined ||
        filter.fundingPayFeeMin !== undefined || filter.fundingPayFeeMax !== undefined ||
        filter.fundingCountMin !== undefined || filter.fundingCountMax !== undefined ||
        filter.fundingFeeMin !== undefined || filter.fundingFeeMax !== undefined ||
        filter.entryFeeMin !== undefined || filter.entryFeeMax !== undefined ||
        filter.exitFeeMin !== undefined || filter.exitFeeMax !== undefined ||
        filter.forcedLiquidationFeeMin !== undefined || filter.forcedLiquidationFeeMax !== undefined ||
        filter.profitLossMin !== undefined || filter.profitLossMax !== undefined ||
        filter.netProfitLossMin !== undefined || filter.netProfitLossMax !== undefined ||
        filter.individualProfitRateMin !== undefined || filter.individualProfitRateMax !== undefined ||
        filter.overallProfitRateMin !== undefined || filter.overallProfitRateMax !== undefined ||
        filter.currentCapitalMin !== undefined || filter.currentCapitalMax !== undefined ||
        filter.highestCapitalMin !== undefined || filter.highestCapitalMax !== undefined ||
        filter.drawdownMin !== undefined || filter.drawdownMax !== undefined ||
        filter.maxDrawdownMin !== undefined || filter.maxDrawdownMax !== undefined ||
        filter.accumulatedProfitLossMin !== undefined || filter.accumulatedProfitLossMax !== undefined ||
        filter.accumulatedProfitRateMin !== undefined || filter.accumulatedProfitRateMax !== undefined ||
        filter.heldSymbolsCountMin !== undefined || filter.heldSymbolsCountMax !== undefined
    );

    // 필터가 하나도 없으면 원본 데이터 그대로 반환
    if (!hasAnyFilter) {
        return trades;
    }

    // 필터링에 사용할 Set들을 미리 생성 (빈 배열도 올바르게 처리)
    const strategySet = (filter.strategies && filter.strategies.length > 0) ? new Set(filter.strategies) :
        (filter.strategies && filter.strategies.length === 0) ? new Set() : null;
    const symbolSet = (filter.symbols && filter.symbols.length > 0) ? new Set(filter.symbols) :
        (filter.symbols && filter.symbols.length === 0) ? new Set() : null;
    const entryNameSet = (filter.entryNames && filter.entryNames.length > 0) ? new Set(filter.entryNames) :
        (filter.entryNames && filter.entryNames.length === 0) ? new Set() : null;
    const exitNameSet = (filter.exitNames && filter.exitNames.length > 0) ? new Set(filter.exitNames) :
        (filter.exitNames && filter.exitNames.length === 0) ? new Set() : null;
    const entryDirectionSet = (filter.entryDirections && filter.entryDirections.length > 0) ? new Set(filter.entryDirections) :
        (filter.entryDirections && filter.entryDirections.length === 0) ? new Set() : null;

    // 시간 필터용 Set들 (빈 배열도 올바르게 처리)
    const entryYearSet = (filter.entryYears && filter.entryYears.length > 0) ? new Set(filter.entryYears) :
        (filter.entryYears && filter.entryYears.length === 0) ? new Set() : null;
    const entryMonthSet = (filter.entryMonths && filter.entryMonths.length > 0) ? new Set(filter.entryMonths) :
        (filter.entryMonths && filter.entryMonths.length === 0) ? new Set() : null;
    const entryDaySet = (filter.entryDays && filter.entryDays.length > 0) ? new Set(filter.entryDays) :
        (filter.entryDays && filter.entryDays.length === 0) ? new Set() : null;
    const entryDayOfWeekSet = (filter.entryDayOfWeeks && filter.entryDayOfWeeks.length > 0) ? new Set(filter.entryDayOfWeeks) :
        (filter.entryDayOfWeeks && filter.entryDayOfWeeks.length === 0) ? new Set() : null;
    const entryHourSet = (filter.entryHours && filter.entryHours.length > 0) ? new Set(filter.entryHours) :
        (filter.entryHours && filter.entryHours.length === 0) ? new Set() : null;
    const entryMinuteSet = (filter.entryMinutes && filter.entryMinutes.length > 0) ? new Set(filter.entryMinutes) :
        (filter.entryMinutes && filter.entryMinutes.length === 0) ? new Set() : null;
    const entrySecondSet = (filter.entrySeconds && filter.entrySeconds.length > 0) ? new Set(filter.entrySeconds) :
        (filter.entrySeconds && filter.entrySeconds.length === 0) ? new Set() : null;

    const exitYearSet = (filter.exitYears && filter.exitYears.length > 0) ? new Set(filter.exitYears) :
        (filter.exitYears && filter.exitYears.length === 0) ? new Set() : null;
    const exitMonthSet = (filter.exitMonths && filter.exitMonths.length > 0) ? new Set(filter.exitMonths) :
        (filter.exitMonths && filter.exitMonths.length === 0) ? new Set() : null;
    const exitDaySet = (filter.exitDays && filter.exitDays.length > 0) ? new Set(filter.exitDays) :
        (filter.exitDays && filter.exitDays.length === 0) ? new Set() : null;
    const exitDayOfWeekSet = (filter.exitDayOfWeeks && filter.exitDayOfWeeks.length > 0) ? new Set(filter.exitDayOfWeeks) :
        (filter.exitDayOfWeeks && filter.exitDayOfWeeks.length === 0) ? new Set() : null;
    const exitHourSet = (filter.exitHours && filter.exitHours.length > 0) ? new Set(filter.exitHours) :
        (filter.exitHours && filter.exitHours.length === 0) ? new Set() : null;
    const exitMinuteSet = (filter.exitMinutes && filter.exitMinutes.length > 0) ? new Set(filter.exitMinutes) :
        (filter.exitMinutes && filter.exitMinutes.length === 0) ? new Set() : null;
    const exitSecondSet = (filter.exitSeconds && filter.exitSeconds.length > 0) ? new Set(filter.exitSeconds) :
        (filter.exitSeconds && filter.exitSeconds.length === 0) ? new Set() : null;

    // 필터가 하나도 없으면 조기 리턴
    if (!hasAnyFilter) {
        return trades;
    }

    // 기본 시간 범위 필터를 위한 날짜 사전 파싱 (필요한 경우에만)
    const entryTimeMinDate = filter.entryTimeMin ? getValidDate(filter.entryTimeMin) : undefined;
    const entryTimeMaxDate = filter.entryTimeMax ? getValidDate(filter.entryTimeMax) : undefined;
    const exitTimeMinDate = filter.exitTimeMin ? getValidDate(filter.exitTimeMin) : undefined;
    const exitTimeMaxDate = filter.exitTimeMax ? getValidDate(filter.exitTimeMax) : undefined;

    // 플래그를 사용하여 각 필터 그룹이 활성화되었는지 확인
    const hasTradeNumberFilter = filter.tradeNumberMin !== undefined || filter.tradeNumberMax !== undefined;
    const hasHoldingTimeFilter = filter.holdingTimeMin !== undefined || filter.holdingTimeMax !== undefined;
    const hasEntryTimeFilter = entryTimeMinDate !== undefined || entryTimeMaxDate !== undefined ||
        entryYearSet !== null || entryMonthSet !== null || entryDaySet !== null ||
        entryDayOfWeekSet !== null || entryHourSet !== null || entryMinuteSet !== null ||
        entrySecondSet !== null;
    const hasExitTimeFilter = exitTimeMinDate !== undefined || exitTimeMaxDate !== undefined ||
        exitYearSet !== null || exitMonthSet !== null || exitDaySet !== null ||
        exitDayOfWeekSet !== null || exitHourSet !== null || exitMinuteSet !== null ||
        exitSecondSet !== null;

    // 고급 시간 필터 활성화 여부
    const hasAdvancedEntryFilter = entryYearSet !== null || entryMonthSet !== null || entryDaySet !== null ||
        entryDayOfWeekSet !== null || entryHourSet !== null || entryMinuteSet !== null ||
        entrySecondSet !== null;
    const hasAdvancedExitFilter = exitYearSet !== null || exitMonthSet !== null || exitDaySet !== null ||
        exitDayOfWeekSet !== null || exitHourSet !== null || exitMinuteSet !== null ||
        exitSecondSet !== null;

    // 결과 배열 - 추정 크기로 미리 할당하여 리사이징 최소화
    const estimatedSize = Math.min(trades.length, Math.max(1000, trades.length / 4));
    const result: TradeItem[] = new Array(estimatedSize);
    result[0] = trades[0]; // 헤더 행 포함
    let resultIndex = 1;

    // 숫자 필터들을 배열로 미리 정의하여 반복문 최적화
    const numericFilters = [];
    if (filter.leverageMin !== undefined || filter.leverageMax !== undefined)
        numericFilters.push({field: "레버리지", min: filter.leverageMin, max: filter.leverageMax});
    if (filter.entryPriceMin !== undefined || filter.entryPriceMax !== undefined)
        numericFilters.push({field: "진입 가격", min: filter.entryPriceMin, max: filter.entryPriceMax});
    if (filter.entryQuantityMin !== undefined || filter.entryQuantityMax !== undefined)
        numericFilters.push({field: "진입 수량", min: filter.entryQuantityMin, max: filter.entryQuantityMax});
    if (filter.exitPriceMin !== undefined || filter.exitPriceMax !== undefined)
        numericFilters.push({field: "청산 가격", min: filter.exitPriceMin, max: filter.exitPriceMax});
    if (filter.exitQuantityMin !== undefined || filter.exitQuantityMax !== undefined)
        numericFilters.push({field: "청산 수량", min: filter.exitQuantityMin, max: filter.exitQuantityMax});
    if (filter.forcedLiquidationPriceMin !== undefined || filter.forcedLiquidationPriceMax !== undefined)
        numericFilters.push({
            field: "강제 청산 가격",
            min: filter.forcedLiquidationPriceMin,
            max: filter.forcedLiquidationPriceMax
        });
    if (filter.fundingReceiveCountMin !== undefined || filter.fundingReceiveCountMax !== undefined)
        numericFilters.push({
            field: "펀딩 수령 횟수",
            min: filter.fundingReceiveCountMin,
            max: filter.fundingReceiveCountMax
        });
    if (filter.fundingReceiveFeeMin !== undefined || filter.fundingReceiveFeeMax !== undefined)
        numericFilters.push({field: "펀딩비 수령", min: filter.fundingReceiveFeeMin, max: filter.fundingReceiveFeeMax});
    if (filter.fundingPayCountMin !== undefined || filter.fundingPayCountMax !== undefined)
        numericFilters.push({field: "펀딩 지불 횟수", min: filter.fundingPayCountMin, max: filter.fundingPayCountMax});
    if (filter.fundingPayFeeMin !== undefined || filter.fundingPayFeeMax !== undefined)
        numericFilters.push({field: "펀딩비 지불", min: filter.fundingPayFeeMin, max: filter.fundingPayFeeMax});
    if (filter.fundingCountMin !== undefined || filter.fundingCountMax !== undefined)
        numericFilters.push({field: "펀딩 횟수", min: filter.fundingCountMin, max: filter.fundingCountMax});
    if (filter.fundingFeeMin !== undefined || filter.fundingFeeMax !== undefined)
        numericFilters.push({field: "펀딩비", min: filter.fundingFeeMin, max: filter.fundingFeeMax});
    if (filter.entryFeeMin !== undefined || filter.entryFeeMax !== undefined)
        numericFilters.push({field: "진입 수수료", min: filter.entryFeeMin, max: filter.entryFeeMax});
    if (filter.exitFeeMin !== undefined || filter.exitFeeMax !== undefined)
        numericFilters.push({field: "청산 수수료", min: filter.exitFeeMin, max: filter.exitFeeMax});
    if (filter.forcedLiquidationFeeMin !== undefined || filter.forcedLiquidationFeeMax !== undefined)
        numericFilters.push({
            field: "강제 청산 수수료",
            min: filter.forcedLiquidationFeeMin,
            max: filter.forcedLiquidationFeeMax
        });
    if (filter.profitLossMin !== undefined || filter.profitLossMax !== undefined)
        numericFilters.push({field: "손익", min: filter.profitLossMin, max: filter.profitLossMax});
    if (filter.netProfitLossMin !== undefined || filter.netProfitLossMax !== undefined)
        numericFilters.push({field: "순손익", min: filter.netProfitLossMin, max: filter.netProfitLossMax});
    if (filter.individualProfitRateMin !== undefined || filter.individualProfitRateMax !== undefined)
        numericFilters.push({
            field: "개별 순손익률",
            min: filter.individualProfitRateMin,
            max: filter.individualProfitRateMax
        });
    if (filter.overallProfitRateMin !== undefined || filter.overallProfitRateMax !== undefined)
        numericFilters.push({field: "전체 순손익률", min: filter.overallProfitRateMin, max: filter.overallProfitRateMax});
    if (filter.currentCapitalMin !== undefined || filter.currentCapitalMax !== undefined)
        numericFilters.push({field: "현재 자금", min: filter.currentCapitalMin, max: filter.currentCapitalMax});
    if (filter.highestCapitalMin !== undefined || filter.highestCapitalMax !== undefined)
        numericFilters.push({field: "최고 자금", min: filter.highestCapitalMin, max: filter.highestCapitalMax});
    if (filter.drawdownMin !== undefined || filter.drawdownMax !== undefined)
        numericFilters.push({field: "드로우다운", min: filter.drawdownMin, max: filter.drawdownMax});
    if (filter.maxDrawdownMin !== undefined || filter.maxDrawdownMax !== undefined)
        numericFilters.push({field: "최고 드로우다운", min: filter.maxDrawdownMin, max: filter.maxDrawdownMax});
    if (filter.accumulatedProfitLossMin !== undefined || filter.accumulatedProfitLossMax !== undefined)
        numericFilters.push({
            field: "누적 손익",
            min: filter.accumulatedProfitLossMin,
            max: filter.accumulatedProfitLossMax
        });
    if (filter.accumulatedProfitRateMin !== undefined || filter.accumulatedProfitRateMax !== undefined)
        numericFilters.push({
            field: "누적 손익률",
            min: filter.accumulatedProfitRateMin,
            max: filter.accumulatedProfitRateMax
        });
    if (filter.heldSymbolsCountMin !== undefined || filter.heldSymbolsCountMax !== undefined)
        numericFilters.push({field: "보유 심볼 수", min: filter.heldSymbolsCountMin, max: filter.heldSymbolsCountMax});

    // 메인 필터링 루프
    for (let i = 1; i < trades.length; i++) {
        const trade = trades[i];

        // 빠른 실패: 가장 선택적인 필터부터 확인

        // 1. 거래 번호 필터 (숫자 비교, 가장 빠름)
        if (hasTradeNumberFilter) {
            const tradeNumber = Number(trade["거래 번호"]);
            if ((filter.tradeNumberMin !== undefined && tradeNumber < filter.tradeNumberMin) ||
                (filter.tradeNumberMax !== undefined && tradeNumber > filter.tradeNumberMax)) {
                continue;
            }
        }

        // 2. 문자열 필터들 (Set 검색, 빠름)
        if (strategySet && !strategySet.has(String(trade["전략 이름"]))) {
            continue;
        }
        if (symbolSet && !symbolSet.has(String(trade["심볼 이름"]))) {
            continue;
        }
        if (entryNameSet && !entryNameSet.has(String(trade["진입 이름"]))) {
            continue;
        }
        if (exitNameSet && !exitNameSet.has(String(trade["청산 이름"]))) {
            continue;
        }
        if (entryDirectionSet && !entryDirectionSet.has(String(trade["진입 방향"]))) {
            continue;
        }

        // 3. 시간 필터들 (날짜 파싱 필요, 보통 속도)
        let entryTime: Date | undefined;
        let exitTime: Date | undefined;

        if (hasEntryTimeFilter || hasExitTimeFilter || hasHoldingTimeFilter) {
            const entryTimeStr = String(trade["진입 시간"]);
            const exitTimeStr = String(trade["청산 시간"]);

            if (entryTimeStr === '-' || exitTimeStr === '-') {
                if (hasEntryTimeFilter || hasExitTimeFilter || hasHoldingTimeFilter) continue;
            } else {
                entryTime = getValidDate(entryTimeStr);
                exitTime = getValidDate(exitTimeStr);

                if (!entryTime || !exitTime) continue;
            }
        }

        // 진입 시간 필터
        if (hasEntryTimeFilter && entryTime) {
            // 기본 시간 범위 필터
            if ((entryTimeMinDate !== undefined && entryTime < entryTimeMinDate) ||
                (entryTimeMaxDate !== undefined && entryTime > entryTimeMaxDate)) {
                continue;
            }

            // 고급 시간 필터 (활성화된 경우에만)
            if (hasAdvancedEntryFilter) {
                const entryYear = entryTime.getFullYear();
                const entryMonth = entryTime.getMonth() + 1;
                const entryDay = entryTime.getDate();
                const entryDayOfWeek = entryTime.getDay();
                const entryHour = entryTime.getHours();
                const entryMinute = entryTime.getMinutes();
                const entrySecond = entryTime.getSeconds();

                if ((entryYearSet && !entryYearSet.has(entryYear)) ||
                    (entryMonthSet && !entryMonthSet.has(entryMonth)) ||
                    (entryDaySet && !entryDaySet.has(entryDay)) ||
                    (entryDayOfWeekSet && !entryDayOfWeekSet.has(entryDayOfWeek)) ||
                    (entryHourSet && !entryHourSet.has(entryHour)) ||
                    (entryMinuteSet && !entryMinuteSet.has(entryMinute)) ||
                    (entrySecondSet && !entrySecondSet.has(entrySecond))) {
                    continue;
                }
            }
        }

        // 청산 시간 필터
        if (hasExitTimeFilter && exitTime) {
            // 기본 시간 범위 필터
            if ((exitTimeMinDate !== undefined && exitTime < exitTimeMinDate) ||
                (exitTimeMaxDate !== undefined && exitTime > exitTimeMaxDate)) {
                continue;
            }

            // 고급 시간 필터 (활성화된 경우에만)
            if (hasAdvancedExitFilter) {
                const exitYear = exitTime.getFullYear();
                const exitMonth = exitTime.getMonth() + 1;
                const exitDay = exitTime.getDate();
                const exitDayOfWeek = exitTime.getDay();
                const exitHour = exitTime.getHours();
                const exitMinute = exitTime.getMinutes();
                const exitSecond = exitTime.getSeconds();

                if ((exitYearSet && !exitYearSet.has(exitYear)) ||
                    (exitMonthSet && !exitMonthSet.has(exitMonth)) ||
                    (exitDaySet && !exitDaySet.has(exitDay)) ||
                    (exitDayOfWeekSet && !exitDayOfWeekSet.has(exitDayOfWeek)) ||
                    (exitHourSet && !exitHourSet.has(exitHour)) ||
                    (exitMinuteSet && !exitMinuteSet.has(exitMinute)) ||
                    (exitSecondSet && !exitSecondSet.has(exitSecond))) {
                    continue;
                }
            }
        }

        // 보유 시간 필터
        if (hasHoldingTimeFilter && entryTime && exitTime) {
            const holdingTimeMs = exitTime.getTime() - entryTime.getTime();
            const holdingTimeSec = Math.floor(holdingTimeMs / 1000);
            if ((filter.holdingTimeMin !== undefined && holdingTimeSec < filter.holdingTimeMin) ||
                (filter.holdingTimeMax !== undefined && holdingTimeSec > filter.holdingTimeMax)) {
                continue;
            }
        }

        // 4. 숫자 범위 필터들 (Number 변환 필요, 가장 느림이므로 마지막에)
        let passedNumericFilters = true;
        for (const {field, min, max} of numericFilters) {
            const value = Number(trade[field]);
            if ((min !== undefined && value < min) || (max !== undefined && value > max)) {
                passedNumericFilters = false;
                break;
            }
        }

        if (!passedNumericFilters) continue;

        // 모든 필터를 통과한 경우 결과에 추가
        if (resultIndex >= result.length) {
            // 배열 크기가 부족하면 확장
            result.length = result.length * 2;
        }
        result[resultIndex++] = trade;
    }

    // 사용하지 않은 공간 제거
    result.length = resultIndex;

    return result;
};

// 최적화된 워커 메시지 핸들러
self.addEventListener('message', (event) => {
    const {type, trades, filter, allTrades, taskId, chunkIndex, totalChunks}: WorkerMessage = event.data;

    if (type === 'filter') {
        try {
            // 입력 검증 - 더 안전한 방식
            if (!trades || !Array.isArray(trades)) {
                self.postMessage({
                    type: 'filterResult',
                    trades: [],
                    id: taskId,
                    chunkIndex: chunkIndex || 0,
                    totalChunks: totalChunks || 1,
                    processingTime: 0
                });
                return;
            }

            if (trades.length === 0) {
                self.postMessage({
                    type: 'filterResult',
                    trades: [],
                    id: taskId,
                    chunkIndex: chunkIndex || 0,
                    totalChunks: totalChunks || 1
                });
                return;
            }

            // 필터 객체 검증
            if (!filter) {
                self.postMessage({
                    type: 'filterResult',
                    trades: trades,
                    id: taskId,
                    chunkIndex: chunkIndex || 0,
                    totalChunks: totalChunks || 1,
                    processingTime: 0
                });
                return;
            }

            // allTrades 검증
            if (!allTrades || !Array.isArray(allTrades)) {
                self.postMessage({
                    type: 'filterResult',
                    trades: trades,
                    id: taskId,
                    chunkIndex: chunkIndex || 0,
                    totalChunks: totalChunks || 1,
                    processingTime: 0
                });
                return;
            }

            // 필터링 실행 (성능 측정 포함)
            const startTime = performance.now();
            const {result, hasBankruptcy} = applyFiltersInOrder(trades, filter, allTrades);
            const endTime = performance.now();

            self.postMessage({
                type: 'filterResult',
                trades: result,
                hasBankruptcy: hasBankruptcy, // 파산 여부 추가
                id: taskId,
                chunkIndex: chunkIndex || 0,
                totalChunks: totalChunks || 1,
                processingTime: Math.round(endTime - startTime)
            });
        } catch (error) {
            console.error('필터링 중 오류:', error);
            self.postMessage({
                type: 'filterResult',
                trades: [],
                id: taskId,
                chunkIndex: chunkIndex || 0,
                totalChunks: totalChunks || 1,
                error: error instanceof Error ? error.message : String(error),
                stack: error instanceof Error ? error.stack : undefined
            });
        }
    }
});

export {};