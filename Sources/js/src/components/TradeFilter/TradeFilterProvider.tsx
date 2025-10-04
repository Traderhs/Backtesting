import React, {useCallback, useEffect, useMemo, useRef, useState} from "react";
import {TradeFilter, TradeFilterContext, TradeItem} from "./TradeFilterContext";
import {
    getDayOfWeekOptions,
    getDayOptions,
    getHourOptions,
    getMinuteSecondOptions,
    getMonthOptions,
    getYearOptions,
} from "./TimeFilterOptions.js";

import ServerAlert from "../Server/ServerAlert.tsx";
import {filterTradesAsync} from "@/workers/tradeFilterUtils.ts";

// 필터링 로직을 청크 단위로 처리하는 함수
export const TradeFilterProvider = ({children}: { children: React.ReactNode }) => {
    const [filter, setFilter] = useState<TradeFilter>({
        recalculateBalance: true,

        tradeNumberMin: undefined,
        tradeNumberMax: undefined,
        strategies: [],
        symbols: [],
        entryNames: [],
        exitNames: [],
        entryDirections: ["매수", "매도"],
        entryTimeMin: undefined,
        entryTimeMax: undefined,
        exitTimeMin: undefined,
        exitTimeMax: undefined,
        // 초기에는 빈 배열로 설정하여 불필요한 계산 방지
        entryYears: [],
        entryMonths: [],
        entryDays: [],
        entryDayOfWeeks: [],
        entryHours: [],
        entryMinutes: [],
        entrySeconds: [],
        exitYears: [],
        exitMonths: [],
        exitDays: [],
        exitDayOfWeeks: [],
        exitHours: [],
        exitMinutes: [],
        exitSeconds: [],
        holdingTimeMin: undefined,
        holdingTimeMax: undefined,
        leverageMin: undefined,
        leverageMax: undefined,
        entryPriceMin: undefined,
        entryPriceMax: undefined,
        entryQuantityMin: undefined,
        entryQuantityMax: undefined,
        exitPriceMin: undefined,
        exitPriceMax: undefined,
        exitQuantityMin: undefined,
        exitQuantityMax: undefined,
        forcedLiquidationPriceMin: undefined,
        forcedLiquidationPriceMax: undefined,
        fundingReceiveCountMin: undefined,
        fundingReceiveCountMax: undefined,
        fundingReceiveFeeMin: undefined,
        fundingReceiveFeeMax: undefined,
        fundingPayCountMin: undefined,
        fundingPayCountMax: undefined,
        fundingPayFeeMin: undefined,
        fundingPayFeeMax: undefined,
        fundingCountMin: undefined,
        fundingCountMax: undefined,
        fundingFeeMin: undefined,
        fundingFeeMax: undefined,
        entryFeeMin: undefined,
        entryFeeMax: undefined,
        exitFeeMin: undefined,
        exitFeeMax: undefined,
        forcedLiquidationFeeMin: undefined,
        forcedLiquidationFeeMax: undefined,
        profitLossMin: undefined,
        profitLossMax: undefined,
        netProfitLossMin: undefined,
        netProfitLossMax: undefined,
        individualProfitRateMin: undefined,
        individualProfitRateMax: undefined,
        overallProfitRateMin: undefined,
        overallProfitRateMax: undefined,
        currentCapitalMin: undefined,
        currentCapitalMax: undefined,
        highestCapitalMin: undefined,
        highestCapitalMax: undefined,
        drawdownMin: undefined,
        drawdownMax: undefined,
        maxDrawdownMin: undefined,
        maxDrawdownMax: undefined,
        accumulatedProfitLossMin: undefined,
        accumulatedProfitLossMax: undefined,
        accumulatedProfitRateMin: undefined,
        accumulatedProfitRateMax: undefined,
        heldSymbolsCountMin: undefined,
        heldSymbolsCountMax: undefined,
    });

    // 필터 패널 확장 상태 추가
    const [filterExpanded, setFilterExpanded] = useState<boolean>(false);

    // 달력 상태 관리 추가
    const [openCalendar, setOpenCalendar] = useState<string | null>(null);

    const [tradeListError, setTradeListError] = useState(false)
    const [allTrades, setAllTrades] = useState<TradeItem[]>([]);
    const [filteredTrades, setFilteredTrades] = useState<TradeItem[]>([]);
    const [loading, setLoading] = useState(true);
    const [symbolOptions, setSymbolOptions] = useState<{ name: string }[]>([]);
    const [strategyOptions, setStrategyOptions] = useState<{ name: string }[]>([]);
    const [entryOptions, setEntryOptions] = useState<{ name: string }[]>([]);
    const [exitOptions, setExitOptions] = useState<{ name: string }[]>([]);

    // 필터링 작업 진행 상태 추가
    const [isFiltering, setIsFiltering] = useState(false);
    const filteringTimeoutRef = useRef<NodeJS.Timeout | null>(null);

    // 고급 시간 필터 옵션 계산 완료 상태
    const [timeOptionsCalculated, setTimeOptionsCalculated] = useState(false);

    // 필터링 진행률 상태
    const [filteringProgress, setFilteringProgress] = useState<number>(0);

    // 자금 재계산 시 파산 발생 여부
    const [hasBankruptcy, setHasBankruptcy] = useState<boolean>(false);

    // 앱 시작 시 config.json 불러오기 (전략, 심볼)
    useEffect(() => {
        async function loadConfig() {
            try {
                const res = await fetch("/Backboard/config.json");
                const config = await res.json();
                const symbols = (config["심볼"] ?? []).map(
                    (s: Record<string, unknown>) => ({name: String(s["심볼 이름"])})
                );

                // 전략은 이제 객체로 존재
                const strategy = config["전략"] as Record<string, unknown>;
                const strategies = [{name: String(strategy["전략 이름"])}];

                setSymbolOptions(symbols);
                setStrategyOptions(strategies);
                setFilter((prev) => ({
                    ...prev,
                    symbols: symbols.map((s: { name: string }) => s.name),
                    strategies: strategies.map((s: { name: string }) => s.name),
                }));
            } catch (error) {
                console.error("config.json 로딩 실패:", error);
            }
        }

        void loadConfig();
    }, []);

    // trade_list.json 로드 및 "진입 이름", "청산 이름" 고유값 추출
    useEffect(() => {
        async function fetchTrades() {
            try {
                const res = await fetch("/Backboard/trade_list.json");
                const data: TradeItem[] = await res.json();

                // 0행 오류 확인
                if (data.length > 0) {
                    if (Number(data[0]["거래 번호"]) !== 0) {
                        setTradeListError(true);
                        setAllTrades([]);
                        return;
                    }

                    // 0행 수정: 지정된 필드("거래 번호", "진입 방향", "현재 자금", "최고 자금", "드로우다운", "최고 드로우다운", "누적 손익", "누적 손익률", "보유 심볼 수")를 제외한 나머지 필드는 '-'로 변경
                    const keepFields = [
                        "거래 번호",
                        "진입 방향",
                        "현재 자금",
                        "최고 자금",
                        "드로우다운",
                        "최고 드로우다운",
                        "누적 손익",
                        "누적 손익률",
                        "보유 심볼 수",
                    ];
                    const modifiedRow0: TradeItem = {...data[0]};
                    Object.keys(modifiedRow0).forEach((key) => {
                        if (!keepFields.includes(key)) {
                            modifiedRow0[key] = "-";
                        }
                    });
                    data[0] = modifiedRow0;
                }

                setAllTrades(data);

                const uniqueEntryNames = Array.from(
                    new Set(
                        data
                            .map((trade) => trade["진입 이름"])
                            .filter((name) => String(name).trim() !== "-")
                    )
                ).map((name) => ({name: String(name)}));

                const uniqueExitNames = Array.from(
                    new Set(
                        data
                            .map((trade) => trade["청산 이름"])
                            .filter((name) => String(name).trim() !== "-")
                    )
                ).map((name) => ({name: String(name)}));

                // 이름 기준으로 정렬
                uniqueEntryNames.sort((a, b) => a.name.localeCompare(b.name));
                uniqueExitNames.sort((a, b) => a.name.localeCompare(b.name));

                setEntryOptions(uniqueEntryNames);
                setExitOptions(uniqueExitNames);

                // 시간 범위 계산 (GMT 기준)
                const validTrades = data.filter(trade => {
                    const tradeNumber = Number(trade["거래 번호"]);
                    return tradeNumber !== 0;
                });

                let minEntryTime = undefined;
                let maxEntryTime = undefined;
                let minExitTime = undefined;
                let maxExitTime = undefined;

                if (validTrades.length > 0) {
                    // 진입 시간 범위 - 전체 데이터에서 실제 최소/최대값 찾기
                    const entryTimes = validTrades.map(trade => String(trade["진입 시간"])).filter(time => time && time !== "-");
                    if (entryTimes.length > 0) {
                        // 전체 데이터를 순회하면서 실제 최소/최대값 찾기
                        let minEntryTimeStr = entryTimes[0];
                        let maxEntryTimeStr = entryTimes[0];
                        for (const time of entryTimes) {
                            if (time < minEntryTimeStr) minEntryTimeStr = time;
                            if (time > maxEntryTimeStr) maxEntryTimeStr = time;
                        }

                        // GMT 기준으로 파싱
                        const startTime = minEntryTimeStr.includes('T')
                            ? minEntryTimeStr
                            : new Date(minEntryTimeStr + 'Z').toISOString();
                        const endTime = maxEntryTimeStr.includes('T')
                            ? maxEntryTimeStr
                            : new Date(maxEntryTimeStr + 'Z').toISOString();

                        minEntryTime = startTime;
                        maxEntryTime = endTime;
                    }

                    // 청산 시간 범위 - 전체 데이터에서 실제 최소/최대값 찾기
                    const exitTimes = validTrades.map(trade => String(trade["청산 시간"])).filter(time => time && time !== "-");
                    if (exitTimes.length > 0) {
                        // 전체 데이터를 순회하면서 실제 최소/최대값 찾기
                        let minExitTimeStr = exitTimes[0];
                        let maxExitTimeStr = exitTimes[0];
                        for (const time of exitTimes) {
                            if (time < minExitTimeStr) minExitTimeStr = time;
                            if (time > maxExitTimeStr) maxExitTimeStr = time;
                        }

                        // GMT 기준으로 파싱
                        const startTime = minExitTimeStr.includes('T')
                            ? minExitTimeStr
                            : new Date(minExitTimeStr + 'Z').toISOString();
                        const endTime = maxExitTimeStr.includes('T')
                            ? maxExitTimeStr
                            : new Date(maxExitTimeStr + 'Z').toISOString();

                        minExitTime = startTime;
                        maxExitTime = endTime;
                    }
                }

                // 고급 필터 초기값도 즉시 계산
                const computedEntryYears = getYearOptions(data, ["진입 시간", "청산 시간"]);
                const computedEntryMonths = getMonthOptions();
                const computedEntryDays = getDayOptions();
                const computedEntryDayOfWeeks = getDayOfWeekOptions();
                const computedEntryHours = getHourOptions();
                const computedEntryMinutes = getMinuteSecondOptions();
                const computedEntrySeconds = getMinuteSecondOptions();

                setFilter((prev) => ({
                    ...prev,
                    entryNames: uniqueEntryNames.map((option) => option.name),
                    exitNames: uniqueExitNames.map((option) => option.name),
                    // 시간 범위 초기값 (GMT)
                    entryTimeMin: minEntryTime,
                    entryTimeMax: maxEntryTime,
                    exitTimeMin: minExitTime,
                    exitTimeMax: maxExitTime,
                    // 고급 필터 초기값도 즉시 설정
                    entryYears: computedEntryYears,
                    entryMonths: computedEntryMonths,
                    entryDays: computedEntryDays,
                    entryDayOfWeeks: computedEntryDayOfWeeks,
                    entryHours: computedEntryHours,
                    entryMinutes: computedEntryMinutes,
                    entrySeconds: computedEntrySeconds,
                    exitYears: computedEntryYears,
                    exitMonths: computedEntryMonths,
                    exitDays: computedEntryDays,
                    exitDayOfWeeks: computedEntryDayOfWeeks,
                    exitHours: computedEntryHours,
                    exitMinutes: computedEntryMinutes,
                    exitSeconds: computedEntrySeconds,
                }));

                // 초기값 계산 완료로 설정
                setTimeOptionsCalculated(true);
            } catch (error) {
                console.error("trade_list.json 로딩 실패:", error);
                setAllTrades([]);
            } finally {
                setLoading(false);
            }
        }

        void fetchTrades();
    }, []);

    // 고급 필터가 처음 열릴 때만 시간 옵션 계산
    useEffect(() => {
        if (filterExpanded && !timeOptionsCalculated && allTrades.length > 0) {
            // 비동기로 처리하여 UI 블로킹 방지
            setTimeout(() => {
                const computedEntryYears = getYearOptions(allTrades, ["진입 시간", "청산 시간"]);
                const computedEntryMonths = getMonthOptions();
                const computedEntryDays = getDayOptions();
                const computedEntryDayOfWeeks = getDayOfWeekOptions();
                const computedEntryHours = getHourOptions();
                const computedEntryMinutes = getMinuteSecondOptions();
                const computedEntrySeconds = getMinuteSecondOptions();

                // 청산시간도 동일한 옵션 사용
                setFilter((prev) => ({
                    ...prev,
                    entryYears: computedEntryYears,
                    entryMonths: computedEntryMonths,
                    entryDays: computedEntryDays,
                    entryDayOfWeeks: computedEntryDayOfWeeks,
                    entryHours: computedEntryHours,
                    entryMinutes: computedEntryMinutes,
                    entrySeconds: computedEntrySeconds,
                    exitYears: computedEntryYears,
                    exitMonths: computedEntryMonths,
                    exitDays: computedEntryDays,
                    exitDayOfWeeks: computedEntryDayOfWeeks,
                    exitHours: computedEntryHours,
                    exitMinutes: computedEntryMinutes,
                    exitSeconds: computedEntrySeconds,
                }));

                setTimeOptionsCalculated(true);
            }, 0);
        }
    }, [filterExpanded, timeOptionsCalculated, allTrades]);

    // 즉시 필터링 함수 (디바운스 없음)
    const immediateFiltering = useCallback(async (currentFilter: TradeFilter, trades: TradeItem[]) => {
        // 필터링 중 상태 설정
        setIsFiltering(true);
        setFilteringProgress(0);

        // allTrades가 비어있으면 처리하지 않음
        if (!trades.length) {
            setFilteredTrades([]);
            setIsFiltering(false);
            return;
        }

        try {
            // 워커의 TradeFilter 형식으로 변환 (recalculateBalance를 boolean으로 확실히 설정)
            const workerFilter = {
                ...currentFilter,
                recalculateBalance: currentFilter.recalculateBalance ?? true,
                // string | number 타입들을 number로 변환
                tradeNumberMin: currentFilter.tradeNumberMin !== undefined ? Number(currentFilter.tradeNumberMin) : undefined,
                tradeNumberMax: currentFilter.tradeNumberMax !== undefined ? Number(currentFilter.tradeNumberMax) : undefined,
                leverageMin: currentFilter.leverageMin !== undefined ? Number(currentFilter.leverageMin) : undefined,
                leverageMax: currentFilter.leverageMax !== undefined ? Number(currentFilter.leverageMax) : undefined,
                entryPriceMin: currentFilter.entryPriceMin !== undefined ? Number(currentFilter.entryPriceMin) : undefined,
                entryPriceMax: currentFilter.entryPriceMax !== undefined ? Number(currentFilter.entryPriceMax) : undefined,
                entryQuantityMin: currentFilter.entryQuantityMin !== undefined ? Number(currentFilter.entryQuantityMin) : undefined,
                entryQuantityMax: currentFilter.entryQuantityMax !== undefined ? Number(currentFilter.entryQuantityMax) : undefined,
                exitPriceMin: currentFilter.exitPriceMin !== undefined ? Number(currentFilter.exitPriceMin) : undefined,
                exitPriceMax: currentFilter.exitPriceMax !== undefined ? Number(currentFilter.exitPriceMax) : undefined,
                exitQuantityMin: currentFilter.exitQuantityMin !== undefined ? Number(currentFilter.exitQuantityMin) : undefined,
                exitQuantityMax: currentFilter.exitQuantityMax !== undefined ? Number(currentFilter.exitQuantityMax) : undefined,
                forcedLiquidationPriceMin: currentFilter.forcedLiquidationPriceMin !== undefined ? Number(currentFilter.forcedLiquidationPriceMin) : undefined,
                forcedLiquidationPriceMax: currentFilter.forcedLiquidationPriceMax !== undefined ? Number(currentFilter.forcedLiquidationPriceMax) : undefined,
                fundingCountMin: currentFilter.fundingCountMin !== undefined ? Number(currentFilter.fundingCountMin) : undefined,
                fundingCountMax: currentFilter.fundingCountMax !== undefined ? Number(currentFilter.fundingCountMax) : undefined,
                fundingFeeMin: currentFilter.fundingFeeMin !== undefined ? Number(currentFilter.fundingFeeMin) : undefined,
                fundingFeeMax: currentFilter.fundingFeeMax !== undefined ? Number(currentFilter.fundingFeeMax) : undefined,
                entryFeeMin: currentFilter.entryFeeMin !== undefined ? Number(currentFilter.entryFeeMin) : undefined,
                entryFeeMax: currentFilter.entryFeeMax !== undefined ? Number(currentFilter.entryFeeMax) : undefined,
                exitFeeMin: currentFilter.exitFeeMin !== undefined ? Number(currentFilter.exitFeeMin) : undefined,
                exitFeeMax: currentFilter.exitFeeMax !== undefined ? Number(currentFilter.exitFeeMax) : undefined,
                forcedLiquidationFeeMin: currentFilter.forcedLiquidationFeeMin !== undefined ? Number(currentFilter.forcedLiquidationFeeMin) : undefined,
                forcedLiquidationFeeMax: currentFilter.forcedLiquidationFeeMax !== undefined ? Number(currentFilter.forcedLiquidationFeeMax) : undefined,
                fundingReceiveCountMin: currentFilter.fundingReceiveCountMin !== undefined ? Number(currentFilter.fundingReceiveCountMin) : undefined,
                fundingReceiveCountMax: currentFilter.fundingReceiveCountMax !== undefined ? Number(currentFilter.fundingReceiveCountMax) : undefined,
                fundingReceiveFeeMin: currentFilter.fundingReceiveFeeMin !== undefined ? Number(currentFilter.fundingReceiveFeeMin) : undefined,
                fundingReceiveFeeMax: currentFilter.fundingReceiveFeeMax !== undefined ? Number(currentFilter.fundingReceiveFeeMax) : undefined,
                fundingPayCountMin: currentFilter.fundingPayCountMin !== undefined ? Number(currentFilter.fundingPayCountMin) : undefined,
                fundingPayCountMax: currentFilter.fundingPayCountMax !== undefined ? Number(currentFilter.fundingPayCountMax) : undefined,
                fundingPayFeeMin: currentFilter.fundingPayFeeMin !== undefined ? Number(currentFilter.fundingPayFeeMin) : undefined,
                fundingPayFeeMax: currentFilter.fundingPayFeeMax !== undefined ? Number(currentFilter.fundingPayFeeMax) : undefined,
                profitLossMin: currentFilter.profitLossMin !== undefined ? Number(currentFilter.profitLossMin) : undefined,
                profitLossMax: currentFilter.profitLossMax !== undefined ? Number(currentFilter.profitLossMax) : undefined,
                netProfitLossMin: currentFilter.netProfitLossMin !== undefined ? Number(currentFilter.netProfitLossMin) : undefined,
                netProfitLossMax: currentFilter.netProfitLossMax !== undefined ? Number(currentFilter.netProfitLossMax) : undefined,
                individualProfitRateMin: currentFilter.individualProfitRateMin !== undefined ? Number(currentFilter.individualProfitRateMin) : undefined,
                individualProfitRateMax: currentFilter.individualProfitRateMax !== undefined ? Number(currentFilter.individualProfitRateMax) : undefined,
                overallProfitRateMin: currentFilter.overallProfitRateMin !== undefined ? Number(currentFilter.overallProfitRateMin) : undefined,
                overallProfitRateMax: currentFilter.overallProfitRateMax !== undefined ? Number(currentFilter.overallProfitRateMax) : undefined,
                currentCapitalMin: currentFilter.currentCapitalMin !== undefined ? Number(currentFilter.currentCapitalMin) : undefined,
                currentCapitalMax: currentFilter.currentCapitalMax !== undefined ? Number(currentFilter.currentCapitalMax) : undefined,
                highestCapitalMin: currentFilter.highestCapitalMin !== undefined ? Number(currentFilter.highestCapitalMin) : undefined,
                highestCapitalMax: currentFilter.highestCapitalMax !== undefined ? Number(currentFilter.highestCapitalMax) : undefined,
                drawdownMin: currentFilter.drawdownMin !== undefined ? Number(currentFilter.drawdownMin) : undefined,
                drawdownMax: currentFilter.drawdownMax !== undefined ? Number(currentFilter.drawdownMax) : undefined,
                maxDrawdownMin: currentFilter.maxDrawdownMin !== undefined ? Number(currentFilter.maxDrawdownMin) : undefined,
                maxDrawdownMax: currentFilter.maxDrawdownMax !== undefined ? Number(currentFilter.maxDrawdownMax) : undefined,
                accumulatedProfitLossMin: currentFilter.accumulatedProfitLossMin !== undefined ? Number(currentFilter.accumulatedProfitLossMin) : undefined,
                accumulatedProfitLossMax: currentFilter.accumulatedProfitLossMax !== undefined ? Number(currentFilter.accumulatedProfitLossMax) : undefined,
                accumulatedProfitRateMin: currentFilter.accumulatedProfitRateMin !== undefined ? Number(currentFilter.accumulatedProfitRateMin) : undefined,
                accumulatedProfitRateMax: currentFilter.accumulatedProfitRateMax !== undefined ? Number(currentFilter.accumulatedProfitRateMax) : undefined,
                heldSymbolsCountMin: currentFilter.heldSymbolsCountMin !== undefined ? Number(currentFilter.heldSymbolsCountMin) : undefined,
                heldSymbolsCountMax: currentFilter.heldSymbolsCountMax !== undefined ? Number(currentFilter.heldSymbolsCountMax) : undefined,
            };

            // 멀티 워커로 필터링 실행
            const result = await filterTradesAsync(
                trades as any[],
                workerFilter
            );

            setFilteredTrades(result.trades as TradeItem[]);
            setHasBankruptcy(result.hasBankruptcy); // 파산 여부 저장
            setIsFiltering(false);
            setFilteringProgress(100);
        } catch (error) {
            console.error('멀티 워커 필터링 실패:', error);
            setFilteredTrades([]);
            setIsFiltering(false);
            setFilteringProgress(0);
        }
    }, []);

    // 필터링 로직을 최적화된 useEffect로 처리
    useEffect(() => {
        // 이전 타이머 정리
        if (filteringTimeoutRef.current) {
            clearTimeout(filteringTimeoutRef.current);
        }

        // 즉시 필터링 실행
        immediateFiltering(filter, allTrades).then();

        return () => {
            if (filteringTimeoutRef.current) {
                clearTimeout(filteringTimeoutRef.current);
            }
        };
    }, [filter, allTrades]);

    // contextValue도 useMemo로 최적화하여 불필요한 재생성 방지
    const contextValue = useMemo(() => ({
        filter,
        setFilter,
        allTrades,
        filteredTrades,
        loading,
        isFiltering, // 필터링 진행 상태 추가
        filteringProgress, // 멀티 워커 진행률 추가
        hasBankruptcy, // 자금 재계산 시 파산 발생 여부 추가
        options: {
            symbols: symbolOptions,
            strategies: strategyOptions,
            entryNames: entryOptions,
            exitNames: exitOptions,
        },
        // 필터 패널 확장 상태 관련 값 추가
        filterExpanded,
        setFilterExpanded,
        // 달력 상태 관리 추가
        openCalendar,
        setOpenCalendar
    }), [
        filter,
        allTrades,
        filteredTrades,
        loading,
        isFiltering,
        filteringProgress,
        hasBankruptcy,
        symbolOptions,
        strategyOptions,
        entryOptions,
        exitOptions,
        filterExpanded,
        openCalendar
    ]);

    // 에러 경고 컴포넌트를 변수로 분리
    const tradeListAlert = tradeListError ? (
        <ServerAlert
            serverError={true}
            message="거래 내역 데이터 오류: 0번 거래(초기 자금 정보)가 올바르게 구성되지 않았습니다."
        />
    ) : null;

    // Provider 렌더링
    return (
        <>
            {tradeListAlert}
            <TradeFilterContext.Provider value={contextValue}>
                {children}
            </TradeFilterContext.Provider>
        </>
    );
};