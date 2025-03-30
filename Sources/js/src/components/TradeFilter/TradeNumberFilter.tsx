import React from "react";
import {useTradeFilter, toggleOption} from "../TradeFilter";

const TradeNumberFilter: React.FC = () => {
    const {filter, setFilter} = useTradeFilter();

    const handleMinChange = (e: React.ChangeEvent<HTMLInputElement>) => {
        const value = e.target.value ? e.target.value : ""; // 문자열로 변환
        toggleOption("tradeNumberMin", value, true, setFilter); // toggleOption 사용
    };

    const handleMaxChange = (e: React.ChangeEvent<HTMLInputElement>) => {
        const value = e.target.value ? e.target.value : ""; // 문자열로 변환
        toggleOption("tradeNumberMax", value, true, setFilter); // toggleOption 사용
    };

    return (
        <div>
            <h3 className="text-sm font-bold text-white">거래 번호</h3>
            <div className="flex space-x-2">
                <input
                    type="number"
                    placeholder="이상"
                    value={filter.tradeNumberMin !== undefined ? filter.tradeNumberMin : ""}
                    onChange={handleMinChange}
                    className="w-1/2 p-1 rounded"
                />
                <input
                    type="number"
                    placeholder="이하"
                    value={filter.tradeNumberMax !== undefined ? filter.tradeNumberMax : ""}
                    onChange={handleMaxChange}
                    className="w-1/2 p-1 rounded"
                />
            </div>
        </div>
    );
};

export default TradeNumberFilter;
