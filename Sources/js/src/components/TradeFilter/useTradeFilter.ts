import { useContext } from "react";
import { TradeFilterContext, TradeFilterContextType } from "./TradeFilterContext";

export const useTradeFilter = (): TradeFilterContextType => {
    const context = useContext(TradeFilterContext);
    if (!context) {
        throw new Error("useTradeFilter must be used within a TradeFilterProvider");
    }
    return context;
};
