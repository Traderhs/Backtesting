import React from 'react';
import {AnimatePresence, motion} from 'framer-motion';
import './LoadingSpinner.css';

// 로딩 스피너 컴포넌트
const LoadingSpinner: React.FC = () => {
    return (
        <AnimatePresence>
            <motion.div
                className="loading-screen"
                initial={{opacity: 0}}
                animate={{opacity: 1}}
                exit={{opacity: 0}}
                transition={{
                    duration: 0.3,
                    ease: [0.4, 0.0, 0.2, 1]
                }}
                style={{
                    position: 'absolute'
                }}
            >
                {/* 반투명 배경 오버레이 */}
                <div className="loading-overlay"/>

                {/* 로딩 스피너 */}
                <motion.div
                    initial={{scale: 0.8, opacity: 0}}
                    animate={{scale: 1, opacity: 1}}
                    transition={{
                        type: "spring",
                        stiffness: 200,
                        damping: 20,
                        duration: 0.8
                    }}
                    className="loading-spinner-frame"
                >
                    <div className="loading-spinner-ring"/>
                </motion.div>
            </motion.div>
        </AnimatePresence>
    );
};

export default LoadingSpinner;
