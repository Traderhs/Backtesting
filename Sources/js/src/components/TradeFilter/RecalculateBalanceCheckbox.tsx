import React, {useContext} from "react";
import {TradeFilterContext} from "./TradeFilterContext";

const RecalculateBalanceCheckbox: React.FC = () => {
    const context = useContext(TradeFilterContext);

    if (!context) {
        throw new Error("RecalculateBalanceCheckbox must be used within a TradeFilterProvider");
    }

    const {setFilter, filter} = context;

    const handleChange = (event: React.ChangeEvent<HTMLInputElement>) => {
        setFilter(prevFilter => ({...prevFilter, recalculateBalance: event.target.checked}));
    };

    return (
        <div className="flex items-center gap-2">
            <input
                type="checkbox"
                checked={filter.recalculateBalance ?? true}
                onChange={handleChange}
            />
            <label>자금 재계산</label>
        </div>
    );
};

export default RecalculateBalanceCheckbox;
