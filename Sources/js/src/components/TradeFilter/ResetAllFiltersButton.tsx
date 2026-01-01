import React, {useContext, useEffect, useMemo, useState} from "react";
import {TradeFilterContext} from "./TradeFilterContext";
import './ResetAllFiltersButton.css';
import {resetAllFilters} from "./FilterResetEvent";
import {Button} from "../ui/button.tsx";
import {motion, Variants} from "framer-motion";

// Sidebar와 동일한 애니메이션 변형 정의
const itemVariants: Variants = {
    hidden: {opacity: 0, x: 0},

    visible: {
        opacity: 1,
        x: 0,
        transition: {
            duration: 0.25,
            ease: 'easeInOut'
        }
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

const ResetAllFiltersButton: React.FC = () => {
    const context = useContext(TradeFilterContext);
    const [isActive, setIsActive] = useState(false);

    if (!context) {
        throw new Error("ResetAllFiltersButton must be used within a TradeFilterProvider");
    }

    // custom 값을 메모이제이션하여 불필요한 리렌더링 방지
    const customValue = useMemo(() => ({isActive}), [isActive]);

    // 활성화 타이머 관리
    useEffect(() => {
        let timer: NodeJS.Timeout;
        if (isActive) {
            timer = setTimeout(() => {
                setIsActive(false);
            }, 500); // 0.5초 후에 active 상태 해제
        }
        return () => {
            if (timer) clearTimeout(timer);
        };
    }, [isActive]);

    // 필터 초기화 핸들러
    const handleResetFilter = () => {
        // 버튼 활성화 상태 설정
        setIsActive(true);

        // 모든 개별 필터들의 reset 이벤트 발생
        // 각 필터 컴포넌트가 자신의 초기화 로직을 실행
        resetAllFilters();
    };

    return (
        <div style={{paddingBottom: '6px'}}>
            <motion.div
                variants={itemVariants}
                initial="hidden"
                animate="visible"
                whileHover="hover"
                whileTap="tap"
                custom={customValue}
                className={`sidebar-button-container${isActive ? " reset-all-filters-button-active-container" : ""}`}
                style={{
                    width: '100%',
                    borderRadius: '8px',
                    overflow: 'visible',
                    willChange: 'transform, opacity, box-shadow',
                    transform: 'translateZ(0)'
                }}
                layoutId="reset-filter-button"
            >
                <Button
                    variant="ghost"
                    onClick={handleResetFilter}
                    className={`reset-all-filters-button ${isActive ? 'active' : ''}`}
                >
                    필터 초기화
                </Button>
            </motion.div>
        </div>
    );
};

export default ResetAllFiltersButton;
