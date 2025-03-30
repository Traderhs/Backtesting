import React, { useState } from "react";
import { Button } from "../ui/button.tsx";
import AdvancedExitTimeFilterModal, { AdvancedExitTimeFilterValues } from "./AdvancedExitTimeFilterModal";
import { useTradeFilter } from "../TradeFilter";

const AdvancedExitTimeFilter: React.FC = () => {
    const [isModalOpen, setModalOpen] = useState(false);
    const [values, setValues] = useState<AdvancedExitTimeFilterValues>({});
    const { allTrades } = useTradeFilter();

    return (
        <div>
            <Button onClick={() => setModalOpen(true)}>고급 필터</Button>
            <AdvancedExitTimeFilterModal
                isOpen={isModalOpen}
                onClose={() => setModalOpen(false)}
                values={values}
                setValues={setValues}
                tradeData={allTrades}
            />
        </div>
    );
};

export default AdvancedExitTimeFilter;
