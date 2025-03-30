import React, { useState } from "react";
import { Button } from "../ui/button.tsx";
import AdvancedEntryTimeFilterModal, { AdvancedEntryTimeFilterValues } from "./AdvancedEntryTimeFilterModal";
import { useTradeFilter } from "../TradeFilter";

const AdvancedEntryTimeFilter: React.FC = () => {
    const [isModalOpen, setModalOpen] = useState(false);
    const [values, setValues] = useState<AdvancedEntryTimeFilterValues>({});
    const { allTrades } = useTradeFilter();

    return (
        <div>
            <Button onClick={() => setModalOpen(true)}>고급 필터</Button>
            <AdvancedEntryTimeFilterModal
                isOpen={isModalOpen}
                onClose={() => setModalOpen(false)}
                values={values}
                setValues={setValues}
                tradeData={allTrades}
            />
        </div>
    );
};

export default AdvancedEntryTimeFilter;
