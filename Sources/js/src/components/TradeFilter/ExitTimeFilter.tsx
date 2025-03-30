import React, {useState} from "react";
import {useTradeFilter} from "../TradeFilter";
import DateTimeModal from "./DateTimeModal";
import {Button} from "../ui/button.tsx";

const ExitTimeFilter: React.FC = () => {
    const {filter, allTrades} = useTradeFilter();
    const [isExitTimeMinOpen, setExitTimeMinOpen] = useState(false);
    const [isExitTimeMaxOpen, setExitTimeMaxOpen] = useState(false);

    const formatDate = (dateStr: string | undefined) => {
        if (!dateStr) return "";
        const date = new Date(dateStr);
        return date.toISOString().replace("T", " ").split(".")[0];
    };

    return (
        <div>
            <h3 className="text-sm font-bold text-white">청산 시간</h3>
            <div className="flex space-x-2">
                <Button onClick={() => setExitTimeMinOpen(true)}>
                    {formatDate(filter.exitTimeMin) || "시작"}
                </Button>
                <Button onClick={() => setExitTimeMaxOpen(true)}>
                    {formatDate(filter.exitTimeMax) || "끝"}
                </Button>
            </div>

            <DateTimeModal type="exitTimeMin" isOpen={isExitTimeMinOpen} onClose={() => setExitTimeMinOpen(false)}
                           tradeData={allTrades}/>
            <DateTimeModal type="exitTimeMax" isOpen={isExitTimeMaxOpen} onClose={() => setExitTimeMaxOpen(false)}
                           tradeData={allTrades}/>
        </div>
    );
};

export default ExitTimeFilter;
