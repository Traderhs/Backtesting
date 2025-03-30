import React from "react";
import { useTradeFilter, toggleOption } from "../TradeFilter";;

const StrategyFilter: React.FC = () => {
    const { filter, setFilter, options } = useTradeFilter();

    return (
        <div>
            <h3 className="text-sm font-bold text-white">전략</h3>
            {options.strategies.map(option => (
                <div key={option.name}>
                    <input
                        type="checkbox"
                        checked={filter.strategies.includes(option.name)}
                        onChange={e => toggleOption("strategies", option.name, e.target.checked, setFilter)}
                    />
                    <label className="ml-2 text-white">{option.name}</label>
                </div>
            ))}
        </div>
    );
};

export default StrategyFilter;
