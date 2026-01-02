import React, {useCallback, useMemo, useState} from "react";
import AdvancedExitTimeFilterModal, {AdvancedExitTimeFilterValues} from "./AdvancedExitTimeFilterModal";
import {useTradeFilter} from "./index.ts";
import {Button} from "@/Components/UI/Button.tsx";
import {motion} from "framer-motion";

// Sidebar와 동일한 애니메이션 변형 정의
const itemVariants = {
    hidden: {opacity: 1, x: 0},
    visible: {
        opacity: 1,
        x: 0
    },
    hover: {
        scale: 1.03,
        borderColor: 'rgba(255, 215, 0, 0.7)',
        boxShadow: '0 0 5px rgba(255, 215, 0, 0.5)',
        transition: {duration: 0.2}
    },
    tap: (custom: { isActive: boolean }) => ({
        scale: 0.98,
        backgroundColor: 'rgba(52, 46, 14, 1)',
        boxShadow: custom.isActive
            ? 'inset 0 0 0 1000px rgba(255, 215, 0, 0.2), 0 0 5px rgba(255, 215, 0, 0.3)'
            : 'inset 0 0 0 1000px rgba(255, 215, 0, 0.15), 0 0 5px rgba(255, 215, 0, 0.3)',
        transition: {duration: 0.1}
    })
};

const AdvancedExitTimeFilter: React.FC = () => {
    const [isModalOpen, setModalOpen] = useState(false);
    const [values, setValues] = useState<AdvancedExitTimeFilterValues>({});
    const [buttonPressed, setButtonPressed] = useState(false);
    const {allTrades, setOpenCalendar} = useTradeFilter();

    // 실제 거래(거래 번호 0 제외)가 존재하는지 여부 확인
    const hasValidTrades = useMemo(() => allTrades.some(trade => Number(trade["거래 번호"]) !== 0), [allTrades]);

    const handleMouseDown = () => {
        setButtonPressed(true);
    };

    const handleMouseUp = useCallback(() => {
        if (buttonPressed) {
            if (isModalOpen) {
                setModalOpen(false);
            } else {
                setOpenCalendar(null);
                const closeChartCalendarEvent = new CustomEvent('closeChartCalendar');
                window.dispatchEvent(closeChartCalendarEvent);
                requestAnimationFrame(() => {
                    setModalOpen(true);
                });
            }
        }
        setButtonPressed(false);
    }, [buttonPressed, isModalOpen, setOpenCalendar]);

    const handleMouseLeave = () => {
        setButtonPressed(false);
    };

    const handleClose = useCallback(() => {
        setModalOpen(false);
    }, []);

    // 실제 거래가 없으면 필터 UI를 렌더링하지 않음 (모든 훅 호출 이후)
    if (!hasValidTrades) {
        return null;
    }

    return (
        <div style={{marginTop: '14px'}}>
            <motion.div
                variants={itemVariants}
                initial="hidden"
                animate="visible"
                whileHover="hover"
                whileTap="tap"
                custom={{isActive: isModalOpen}}
                className={`sidebar-button-container ${isModalOpen ? "active-sidebar-button" : ""}`}
                style={{
                    width: '100%',
                    height: '44px',
                    borderRadius: '8px',
                    overflow: 'visible',
                    display: 'flex'
                }}
            >
                <Button
                    variant="ghost"
                    className="time-filter-button advanced-button"
                    onMouseDown={handleMouseDown}
                    onMouseUp={handleMouseUp}
                    onMouseLeave={handleMouseLeave}
                    style={{
                        width: '100%',
                        height: '44px',
                        borderRadius: '8px',
                        border: '1px solid rgba(255, 215, 0, 0.4)',
                        fontFamily: "'Inter', 'Pretendard', sans-serif",
                        fontSize: '15px',
                        fontWeight: isModalOpen ? 600 : 400,
                        color: 'white',
                        cursor: 'pointer',
                        padding: '8px 12px',
                        outline: 'none',
                        backgroundColor: isModalOpen ? 'rgba(255, 215, 0, 0.4)' : 'rgba(17, 17, 17, 1)',
                        borderColor: isModalOpen ? 'rgba(255, 215, 0, 0.8)' : 'rgba(255, 215, 0, 0.4)',
                        display: 'flex',
                        alignItems: 'center',
                        justifyContent: 'center',
                        margin: 0,
                        flex: 1
                    }}
                >
                    고급 필터
                </Button>
            </motion.div>
            <AdvancedExitTimeFilterModal
                isOpen={isModalOpen}
                onClose={handleClose}
                values={values}
                setValues={setValues}
                tradeData={allTrades}
            />
        </div>
    );
};

export default AdvancedExitTimeFilter;
