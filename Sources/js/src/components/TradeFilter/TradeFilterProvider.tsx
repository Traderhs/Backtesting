import React, {useEffect, useState} from "react";
import {
    TradeFilterContext,
    TradeFilter,
    TradeItem,
    TradeFilterContextType,
} from "./TradeFilterContext";
import {
    getYearOptions,
    getMonthOptions,
    getDayOptions,
    getDayOfWeekOptions,
    getHourOptions,
    getMinuteSecondOptions,
} from "./TimeFilterOptions";
import {parseHoldingTime} from "./ParseHoldingTime";
import ServerAlert from "../ServerAlert";

export const TradeFilterProvider = ({children}: { children: React.ReactNode }) => {
    const [filter, setFilter] = useState<TradeFilter>({
        recalculateBalance: undefined,

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

    const [tradeListError, setTradeListError] = useState(false)
    const [allTrades, setAllTrades] = useState<TradeItem[]>([]);
    const [filteredTrades, setFilteredTrades] = useState<TradeItem[]>([]);
    const [loading, setLoading] = useState(true);
    const [symbolOptions, setSymbolOptions] = useState<{ name: string }[]>([]);
    const [strategyOptions, setStrategyOptions] = useState<{ name: string }[]>([]);
    const [entryOptions, setEntryOptions] = useState<{ name: string }[]>([]);
    const [exitOptions, setExitOptions] = useState<{ name: string }[]>([]);

    // 앱 시작 시 config.json 불러오기 (전략, 심볼)
    useEffect(() => {
        async function loadConfig() {
            try {
                const res = await fetch("/Backboard/config.json");
                const config = await res.json();
                const symbols = (config["심볼"] ?? []).map(
                    (s: Record<string, unknown>) => ({name: String(s["심볼명"])})
                );
                const strategies = (config["전략"] ?? []).map(
                    (s: Record<string, unknown>) => ({name: String(s["전략명"])})
                );
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

                setEntryOptions(uniqueEntryNames);
                setExitOptions(uniqueExitNames);

                setFilter((prev) => ({
                    ...prev,
                    entryNames: uniqueEntryNames.map((option) => option.name),
                    exitNames: uniqueExitNames.map((option) => option.name),
                }));
            } catch (error) {
                console.error("trade_list.json 로딩 실패:", error);
                setAllTrades([]);
            } finally {
                setLoading(false);
            }
        }

        void fetchTrades();
    }, []);

    // allTrades가 로드되면 고급 필터의 기본값(모든 옵션 체크)을 설정
    useEffect(() => {
        if (allTrades.length > 0) {
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
        }
    }, [allTrades]);

    // 필터 적용 및 필터된 거래 내역에 누적 계산 값 적용 (0행은 항상 포함)
    useEffect(() => {
        const filtered = allTrades.filter((trade, index) => {
            if (index === 0) return true; // 첫 번째 행은 항상 포함

            // 기본 필터 조건
            const tradeNumber = Number(trade["거래 번호"]);
            const tradeNumberMatch =
                (filter.tradeNumberMin === undefined || tradeNumber >= filter.tradeNumberMin) &&
                (filter.tradeNumberMax === undefined || tradeNumber <= filter.tradeNumberMax);

            const strategyMatch = filter.strategies.includes(String(trade["전략 이름"]));
            const symbolMatch = filter.symbols.includes(String(trade["심볼 이름"]));
            const entryMatch = filter.entryNames.includes(String(trade["진입 이름"]));
            const exitMatch = filter.exitNames.includes(String(trade["청산 이름"]));
            const directionMatch = filter.entryDirections.includes(String(trade["진입 방향"]));

            const entryTimeStr = String(trade["진입 시간"]).replace(" ", "T") + "Z";
            const exitTimeStr = String(trade["청산 시간"]).replace(" ", "T") + "Z";
            const entryTime = Date.parse(entryTimeStr);
            const exitTime = Date.parse(exitTimeStr);

            const entryTimeMatch =
                (filter.entryTimeMin === undefined || entryTime >= Date.parse(filter.entryTimeMin)) &&
                (filter.entryTimeMax === undefined || entryTime <= Date.parse(filter.entryTimeMax));
            const exitTimeMatch =
                (filter.exitTimeMin === undefined || exitTime >= Date.parse(filter.exitTimeMin)) &&
                (filter.exitTimeMax === undefined || exitTime <= Date.parse(filter.exitTimeMax));

            // 고급 필터 조건 - 시간 관련
            const entryDate = new Date(entryTimeStr);
            const exitDate = new Date(exitTimeStr);

            // 보유 시간 필터
            const holdingTimeStr = String(trade["보유 시간"]);
            const holdingTime = parseHoldingTime(holdingTimeStr); // 초 단위 변환
            const holdingTimeMatch =
                (filter.holdingTimeMin === undefined || (holdingTime !== null && holdingTime >= filter.holdingTimeMin)) &&
                (filter.holdingTimeMax === undefined || (holdingTime !== null && holdingTime <= filter.holdingTimeMax));

            const leverage = Number(trade["레버리지"]);
            const leverageMatch =
                (filter.leverageMin === undefined || leverage >= filter.leverageMin) &&
                (filter.leverageMax === undefined || leverage <= filter.leverageMax);

            const entryPrice = Number(trade["진입 가격"]);
            const entryPriceMatch =
                (filter.entryPriceMin === undefined || entryPrice >= filter.entryPriceMin) &&
                (filter.entryPriceMax === undefined || entryPrice <= filter.entryPriceMax);

            const entryQuantity = Number(trade["진입 수량"]);
            const entryQuantityMatch =
                (filter.entryQuantityMin === undefined || entryQuantity >= filter.entryQuantityMin) &&
                (filter.entryQuantityMax === undefined || entryQuantity <= filter.entryQuantityMax);

            const exitPrice = Number(trade["청산 가격"]);
            const exitPriceMatch =
                (filter.exitPriceMin === undefined || exitPrice >= filter.exitPriceMin) &&
                (filter.exitPriceMax === undefined || exitPrice <= filter.exitPriceMax);

            const exitQuantity = Number(trade["청산 수량"]);
            const exitQuantityMatch =
                (filter.exitQuantityMin === undefined || exitQuantity >= filter.exitQuantityMin) &&
                (filter.exitQuantityMax === undefined || exitQuantity <= filter.exitQuantityMax);

            const forcedLiquidationPrice = Number(trade["강제 청산 가격"]);
            const forcedLiquidationPriceMatch =
                (filter.forcedLiquidationPriceMin === undefined || forcedLiquidationPrice >= filter.forcedLiquidationPriceMin) &&
                (filter.forcedLiquidationPriceMax === undefined || forcedLiquidationPrice <= filter.forcedLiquidationPriceMax);

            const entryFee = Number(trade["진입 수수료"]);
            const entryFeeMatch =
                (filter.entryFeeMin === undefined || entryFee >= filter.entryFeeMin) &&
                (filter.entryFeeMax === undefined || entryFee <= filter.entryFeeMax);

            const exitFee = Number(trade["청산 수수료"]);
            const exitFeeMatch =
                (filter.exitFeeMin === undefined || exitFee >= filter.exitFeeMin) &&
                (filter.exitFeeMax === undefined || exitFee <= filter.exitFeeMax);

            const forcedLiquidationFee = Number(trade["강제 청산 수수료"]);
            const forcedLiquidationFeeMatch =
                (filter.forcedLiquidationFeeMin === undefined || forcedLiquidationFee >= filter.forcedLiquidationFeeMin) &&
                (filter.forcedLiquidationFeeMax === undefined || forcedLiquidationFee <= filter.forcedLiquidationFeeMax);

            const profitLoss = Number(trade["손익"]);
            const profitLossMatch =
                (filter.profitLossMin === undefined || profitLoss >= filter.profitLossMin) &&
                (filter.profitLossMax === undefined || profitLoss <= filter.profitLossMax);

            const netProfitLoss = Number(trade["순손익"]);
            const netProfitLossMatch =
                (filter.netProfitLossMin === undefined || netProfitLoss >= filter.netProfitLossMin) &&
                (filter.netProfitLossMax === undefined || netProfitLoss <= filter.netProfitLossMax);

            const individualProfitRate = Number(trade["개별 손익률"]);
            const individualProfitRateMatch =
                (filter.individualProfitRateMin === undefined || individualProfitRate >= filter.individualProfitRateMin) &&
                (filter.individualProfitRateMax === undefined || individualProfitRate <= filter.individualProfitRateMax);

            const overallProfitRate = Number(trade["전체 손익률"]);
            const overallProfitRateMatch =
                (filter.overallProfitRateMin === undefined || overallProfitRate >= filter.overallProfitRateMin) &&
                (filter.overallProfitRateMax === undefined || overallProfitRate <= filter.overallProfitRateMax);

            const currentCapital = Number(trade["현재 자금"]);
            const currentCapitalMatch =
                (filter.currentCapitalMin === undefined || currentCapital >= filter.currentCapitalMin) &&
                (filter.currentCapitalMax === undefined || currentCapital <= filter.currentCapitalMax);

            const highestCapital = Number(trade["최고 자금"]);
            const highestCapitalMatch =
                (filter.highestCapitalMin === undefined || highestCapital >= filter.highestCapitalMin) &&
                (filter.highestCapitalMax === undefined || highestCapital <= filter.highestCapitalMax);

            const drawdown = Number(trade["드로우다운"]);
            const drawdownMatch =
                (filter.drawdownMin === undefined || drawdown >= filter.drawdownMin) &&
                (filter.drawdownMax === undefined || drawdown <= filter.drawdownMax);

            const maxDrawdown = Number(trade["최고 드로우다운"]);
            const maxDrawdownMatch =
                (filter.maxDrawdownMin === undefined || maxDrawdown >= filter.maxDrawdownMin) &&
                (filter.maxDrawdownMax === undefined || maxDrawdown <= filter.maxDrawdownMax);

            const accumulatedProfitLoss = Number(trade["누적 손익"]);
            const accumulatedProfitLossMatch =
                (filter.accumulatedProfitLossMin === undefined || accumulatedProfitLoss >= filter.accumulatedProfitLossMin) &&
                (filter.accumulatedProfitLossMax === undefined || accumulatedProfitLoss <= filter.accumulatedProfitLossMax);

            const accumulatedProfitRate = Number(trade["누적 손익률"]);
            const accumulatedProfitRateMatch =
                (filter.accumulatedProfitRateMin === undefined || accumulatedProfitRate >= filter.accumulatedProfitRateMin) &&
                (filter.accumulatedProfitRateMax === undefined || accumulatedProfitRate <= filter.accumulatedProfitRateMax);

            const heldSymbolsCount = Number(trade["보유 심볼 수"]);
            const heldSymbolsCountMatch =
                (filter.heldSymbolsCountMin === undefined || heldSymbolsCount >= filter.heldSymbolsCountMin) &&
                (filter.heldSymbolsCountMax === undefined || heldSymbolsCount <= filter.heldSymbolsCountMax);

            return (
                tradeNumberMatch &&
                strategyMatch &&
                symbolMatch &&
                entryMatch &&
                exitMatch &&
                directionMatch &&
                entryTimeMatch &&
                exitTimeMatch &&
                filter.entryYears.includes(entryDate.getUTCFullYear()) &&
                filter.entryMonths.includes(entryDate.getUTCMonth() + 1) &&
                filter.entryDays.includes(entryDate.getUTCDate()) &&
                filter.entryDayOfWeeks.includes(entryDate.getUTCDay()) &&
                filter.entryHours.includes(entryDate.getUTCHours()) &&
                filter.entryMinutes.includes(entryDate.getUTCMinutes()) &&
                filter.entrySeconds.includes(entryDate.getUTCSeconds()) &&
                filter.exitYears.includes(exitDate.getUTCFullYear()) &&
                filter.exitMonths.includes(exitDate.getUTCMonth() + 1) &&
                filter.exitDays.includes(exitDate.getUTCDate()) &&
                filter.exitDayOfWeeks.includes(exitDate.getUTCDay()) &&
                filter.exitHours.includes(exitDate.getUTCHours()) &&
                filter.exitMinutes.includes(exitDate.getUTCMinutes()) &&
                filter.exitSeconds.includes(exitDate.getUTCSeconds()) &&
                holdingTimeMatch &&
                leverageMatch &&
                entryPriceMatch &&
                entryQuantityMatch &&
                exitPriceMatch &&
                exitQuantityMatch &&
                forcedLiquidationPriceMatch &&
                entryFeeMatch &&
                exitFeeMatch &&
                forcedLiquidationFeeMatch &&
                profitLossMatch &&
                netProfitLossMatch &&
                individualProfitRateMatch &&
                overallProfitRateMatch &&
                currentCapitalMatch &&
                highestCapitalMatch &&
                drawdownMatch &&
                maxDrawdownMatch &&
                accumulatedProfitLossMatch &&
                accumulatedProfitRateMatch &&
                heldSymbolsCountMatch
            );
        });

        // allTrades에 오류가 없을 때만 진행
        if (allTrades.length > 0) {
            // recalculateBalance 값은 기본적으로 true (활성화)
            const shouldRecalculate = filter.recalculateBalance !== false;

            // 필터 조건이 적용되지 않아 전체 거래 데이터가 그대로 포함된 경우,
            // 즉, filtered가 allTrades와 동일하면 원본 데이터를 보여줌.
            if (!shouldRecalculate || filtered.length === allTrades.length) {
                setFilteredTrades(filtered);
                return;
            }

            // 자금 재계산 로직 (필터가 적용된 경우에만)
            const initialBalance = allTrades.length > 0 ? Number(allTrades[0]["현재 자금"]) : 0;
            let currentBalance = initialBalance;
            let maxBalance = initialBalance;
            let maxDrawdown = 0;

            const updatedTrades = filtered.map((trade, index) => {
                if (index === 0) {
                    // 첫 번째 행은 아무 처리 없이 그대로 반환
                    return trade;
                }

                const profit = Number(trade["순손익"]);
                currentBalance += profit;
                maxBalance = Math.max(maxBalance, currentBalance);
                const drawdown = maxBalance ? (1 - currentBalance / maxBalance) * 100 : 0;
                maxDrawdown = Math.max(maxDrawdown, drawdown);
                return {
                    ...trade,
                    "전체 손익률": "-", // 재계산 시 전체 손익률은 별도로 계산할 수 있지만, 여기서는 '-'로 표시
                    "현재 자금": currentBalance,
                    "최고 자금": maxBalance,
                    "드로우다운": drawdown,
                    "최고 드로우다운": maxDrawdown,
                    "누적 손익": currentBalance - initialBalance,
                    "누적 손익률": initialBalance ? ((currentBalance - initialBalance) / initialBalance) * 100 : 0,
                    "보유 심볼 수": "-", // 보유 심볼 수 역시 '-' 처리 (추가 계산이 필요하면 별도 로직 적용)
                };
            });

            setFilteredTrades(updatedTrades);
        }
    }, [filter, allTrades]);

    const contextValue: TradeFilterContextType = {
        filter,
        setFilter,
        allTrades,
        filteredTrades,
        loading,
        options: {
            symbols: symbolOptions,
            strategies: strategyOptions,
            entryNames: entryOptions,
            exitNames: exitOptions,
        },
    };

    // 에러 경고 컴포넌트를 변수로 분리
    const tradeListAlert = tradeListError ? (
        <ServerAlert
            serverError={tradeListError}
            message={"오류가 발생했습니다.\n거래 내역 첫 번째 행의 거래 번호가 0이 아닙니다."}
        />
    ) : null;

    // Provider 컴포넌트를 변수로 분리
    const tradeFilterProvider = (
        <TradeFilterContext.Provider value={contextValue}>
            {children}
        </TradeFilterContext.Provider>
    );

    return (
        <>
            {tradeListAlert}
            {tradeFilterProvider}
        </>
    );
};