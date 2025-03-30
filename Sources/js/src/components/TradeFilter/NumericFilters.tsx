import React from "react";
import {useTradeFilter, toggleOption} from "../TradeFilter";
import {TradeFilter} from "../TradeFilter/TradeFilterContext";

interface NumericFilterDefinition {
    label: string;
    minKey: keyof TradeFilter;
    maxKey: keyof TradeFilter;
}

const numericFilters: NumericFilterDefinition[] = [
    {label: "레버리지", minKey: "leverageMin", maxKey: "leverageMax"},
    {label: "진입 가격", minKey: "entryPriceMin", maxKey: "entryPriceMax"},
    {label: "진입 수량", minKey: "entryQuantityMin", maxKey: "entryQuantityMax"},
    {label: "청산 가격", minKey: "exitPriceMin", maxKey: "exitPriceMax"},
    {label: "청산 수량", minKey: "exitQuantityMin", maxKey: "exitQuantityMax"},
    {label: "강제 청산 가격", minKey: "forcedLiquidationPriceMin", maxKey: "forcedLiquidationPriceMax"},
    {label: "진입 수수료", minKey: "entryFeeMin", maxKey: "entryFeeMax"},
    {label: "청산 수수료", minKey: "exitFeeMin", maxKey: "exitFeeMax"},
    {label: "강제 청산 수수료", minKey: "forcedLiquidationFeeMin", maxKey: "forcedLiquidationFeeMax"},
    {label: "손익", minKey: "profitLossMin", maxKey: "profitLossMax"},
    {label: "순손익", minKey: "netProfitLossMin", maxKey: "netProfitLossMax"},
    {label: "개별 손익률", minKey: "individualProfitRateMin", maxKey: "individualProfitRateMax"},
    {label: "전체 손익률", minKey: "overallProfitRateMin", maxKey: "overallProfitRateMax"},
    {label: "현재 자금", minKey: "currentCapitalMin", maxKey: "currentCapitalMax"},
    {label: "최고 자금", minKey: "highestCapitalMin", maxKey: "highestCapitalMax"},
    {label: "드로우다운", minKey: "drawdownMin", maxKey: "drawdownMax"},
    {label: "최고 드로우다운", minKey: "maxDrawdownMin", maxKey: "maxDrawdownMax"},
    {label: "누적 손익", minKey: "accumulatedProfitLossMin", maxKey: "accumulatedProfitLossMax"},
    {label: "누적 손익률", minKey: "accumulatedProfitRateMin", maxKey: "accumulatedProfitRateMax"},
    {label: "보유 심볼 수", minKey: "heldSymbolsCountMin", maxKey: "heldSymbolsCountMax"},
];

const NumericFilters: React.FC = () => {
    const {filter, setFilter} = useTradeFilter();

    const handleMinChange = (key: keyof TradeFilter) => (e: React.ChangeEvent<HTMLInputElement>) => {
        const value = e.target.value ? e.target.value : "";
        toggleOption(key, value, true, setFilter);
    };

    const handleMaxChange = (key: keyof TradeFilter) => (e: React.ChangeEvent<HTMLInputElement>) => {
        const value = e.target.value ? e.target.value : "";
        toggleOption(key, value, true, setFilter);
    };

    return (
        <div className="space-y-4">
            {numericFilters.map(({label, minKey, maxKey}) => (
                <div key={label}>
                    <h3 className="text-sm font-bold text-white">{label}</h3>
                    <div className="flex space-x-2">
                        <input
                            type="number"
                            placeholder="이상"
                            value={typeof filter[minKey] === "number" ? filter[minKey] : ""}
                            onChange={handleMinChange(minKey)}
                            className="w-1/2 p-1 rounded"
                        />
                        <input
                            type="number"
                            placeholder="이하"
                            value={typeof filter[maxKey] === "number" ? filter[maxKey] : ""}
                            onChange={handleMaxChange(maxKey)}
                            className="w-1/2 p-1 rounded"
                        />
                    </div>
                </div>
            ))}
        </div>
    );
};

export default NumericFilters;
