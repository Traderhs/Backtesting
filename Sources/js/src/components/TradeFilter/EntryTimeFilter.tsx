import React, { useState } from "react";
import { useTradeFilter } from "../TradeFilter";
import DateTimeModal from "./DateTimeModal";
import { Button } from "../ui/button.tsx";

const EntryTimeFilter: React.FC = () => {
    const { filter, allTrades } = useTradeFilter(); // ✅ 원본 데이터 (allTrades) 사용하기
    const [isEntryTimeMinOpen, setEntryTimeMinOpen] = useState(false);
    const [isEntryTimeMaxOpen, setEntryTimeMaxOpen] = useState(false);

    const formatDate = (dateStr: string | undefined) => {
        if (!dateStr) return "";
        const date = new Date(dateStr);
        return date.toISOString().replace("T", " ").split(".")[0];
    };

    return (
        <div>
            <h3 className="text-sm font-bold text-white">진입 시간</h3>
            <div className="flex space-x-2">
                <Button onClick={() => setEntryTimeMinOpen(true)}>
                    {formatDate(filter.entryTimeMin) || "시작"}
                </Button>
                <Button onClick={() => setEntryTimeMaxOpen(true)}>
                    {formatDate(filter.entryTimeMax) || "끝"}
                </Button>
            </div>

            {/* ✅ 원본 데이터 (allTrades)를 넘기도록 수정 */}
            <DateTimeModal type="entryTimeMin" isOpen={isEntryTimeMinOpen} onClose={() => setEntryTimeMinOpen(false)} tradeData={allTrades} />
            <DateTimeModal type="entryTimeMax" isOpen={isEntryTimeMaxOpen} onClose={() => setEntryTimeMaxOpen(false)} tradeData={allTrades} />
        </div>
    );
};

export default EntryTimeFilter;
