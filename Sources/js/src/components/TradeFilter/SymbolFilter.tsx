import React from "react";
import { useTradeFilter, toggleOption } from "../TradeFilter";

const SymbolFilter: React.FC = () => {
    const { filter, setFilter, options } = useTradeFilter();

    return (
        <div>
            <h3 className="text-sm font-bold text-white">심볼</h3>
            {options.symbols.map(option => (
                <div key={option.name}>
                    <input
                        type="checkbox"
                        checked={filter.symbols.includes(option.name)}
                        onChange={e => toggleOption("symbols", option.name, e.target.checked, setFilter)}
                    />
                    <label className="ml-2 text-white">{option.name}</label>
                </div>
            ))}
        </div>
    );
};

export default SymbolFilter;
