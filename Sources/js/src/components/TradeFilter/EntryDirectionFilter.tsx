import React from "react";
import { useTradeFilter, toggleOption } from "../TradeFilter";

const EntryDirectionFilter: React.FC = () => {
    const { filter, setFilter } = useTradeFilter();

    return (
        <div>
            <h3 className="text-sm font-bold text-white">진입 방향</h3>
            {["매수", "매도"].map(direction => (
                <div key={direction}>
                    <input
                        type="checkbox"
                        checked={filter.entryDirections.includes(direction)}
                        onChange={e => toggleOption("entryDirections", direction, e.target.checked, setFilter)}
                    />
                    <label className="ml-2 text-white">{direction}</label>
                </div>
            ))}
        </div>
    );
};

export default EntryDirectionFilter;
