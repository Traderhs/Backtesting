import React from "react";
import { useTradeFilter, toggleOption } from "../TradeFilter";

const EntryNameFilter: React.FC = () => {
    const { filter, setFilter, options } = useTradeFilter();

    return (
        <div>
            <h3 className="text-sm font-bold text-white">진입 이름</h3>
            {options.entryNames.map(option => (
                <div key={option.name}>
                    <input
                        type="checkbox"
                        checked={filter.entryNames.includes(option.name)}
                        onChange={e => toggleOption("entryNames", option.name, e.target.checked, setFilter)}
                    />
                    <label className="ml-2 text-white">{option.name}</label>
                </div>
            ))}
        </div>
    );
};

export default EntryNameFilter;
