import React from "react";
import { useTradeFilter, toggleOption } from "../TradeFilter";

const ExitNameFilter: React.FC = () => {
    const { filter, setFilter, options } = useTradeFilter();

    return (
        <div>
            <h3 className="text-sm font-bold text-white">청산 이름</h3>
            {options.exitNames.map(option => (
                <div key={option.name}>
                    <input
                        type="checkbox"
                        checked={filter.exitNames.includes(option.name)}
                        onChange={e => toggleOption("exitNames", option.name, e.target.checked, setFilter)}
                    />
                    <label className="ml-2 text-white">{option.name}</label>
                </div>
            ))}
        </div>
    );
};

export default ExitNameFilter;
