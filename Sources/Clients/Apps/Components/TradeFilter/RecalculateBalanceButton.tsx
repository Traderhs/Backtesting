import React, {useContext, useEffect, useRef} from "react";
import {TradeFilterContext} from "./TradeFilterContext";
import "./RecalculateBalanceButton.css";
import {Button} from "@/Components/UI/Button.tsx";
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

const RecalculateBalanceButton: React.FC = () => {
    const context = useContext(TradeFilterContext);
    const buttonRef = useRef<HTMLButtonElement>(null);
    const containerRef = useRef<HTMLDivElement>(null);
    const previousStateRef = useRef<boolean | undefined>(undefined);

    if (!context) {
        throw new Error("RecalculateBalanceButton must be used within a TradeFilterProvider");
    }

    const {setFilter, filter} = context;

    // 상태 변경 시 스타일 확실히 적용하기
    useEffect(() => {
        if (buttonRef.current && previousStateRef.current !== filter.recalculateBalance) {
            // 활성화 -> 비활성화 전환 시 shadow를 명시적으로 제거
            if (!filter.recalculateBalance) {
                // 트랜지션이 끝난 후에도 shadow가 남아있지 않도록 확실히 처리
                setTimeout(() => {
                    if (buttonRef.current) {
                        buttonRef.current.style.boxShadow = 'none';
                    }
                    if (containerRef.current) {
                        containerRef.current.style.boxShadow = 'none';
                    }
                }, 50); // 짧은 timeout으로 렌더링 주기 이후에 적용
            }

            previousStateRef.current = filter.recalculateBalance;
        }
    }, [filter.recalculateBalance]);

    const handleClick = () => {
        const newRecalculateBalance = !filter.recalculateBalance;

        // 비활성화 -> 활성화 될 때 `보유 심볼 수` 필터를 초기화
        if (newRecalculateBalance) {
            setFilter(prevFilter => ({
                ...prevFilter,
                recalculateBalance: newRecalculateBalance,
                heldSymbolsCountMin: undefined,
                heldSymbolsCountMax: undefined
            }));
        } else {
            setFilter(prevFilter => ({
                ...prevFilter,
                recalculateBalance: newRecalculateBalance
            }));
        }
    };

    return (
        <div style={{paddingTop: '22px', paddingBottom: '6px'}}>
            <motion.div
                ref={containerRef}
                variants={itemVariants}
                initial="hidden"
                animate="visible"
                whileHover="hover"
                whileTap="tap"
                custom={{isActive: filter.recalculateBalance}}
                className={`sidebar-button-container ${filter.recalculateBalance ? "active-sidebar-button" : ""}`}
                style={{
                    width: '100%',
                    height: '44px',
                    borderRadius: '8px',
                    overflow: 'visible',
                    display: 'flex'
                }}
            >
                <Button
                    ref={buttonRef}
                    variant="ghost"
                    onClick={handleClick}
                    className={`recalculate-button ${filter.recalculateBalance ? 'recalculate-button-active' : ''}`}
                    style={{
                        flex: 1,
                        margin: 0
                    }}
                >
                    자금 재계산
                </Button>
            </motion.div>
        </div>
    );
};

export default RecalculateBalanceButton;
