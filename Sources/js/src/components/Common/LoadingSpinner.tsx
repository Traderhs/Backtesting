import React, {useEffect} from 'react';
import {AnimatePresence, motion} from 'framer-motion';
import './LoadingSpinner.css';

// 로딩 스피너 컴포넌트
const LoadingSpinner: React.FC = () => {
    // 컴포넌트 마운트 시 스크롤 방지
    useEffect(() => {
        document.body.style.overflow = 'hidden';

        return () => {
            document.body.style.overflow = '';
        };
    }, []);

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
                    width: '100%',
                    height: '100%',
                    display: 'flex',
                    flexDirection: 'column',
                    justifyContent: 'center',
                    alignItems: 'center',
                    backgroundColor: 'rgba(17, 17, 17, 0.9)',
                    position: 'fixed',
                    top: 0,
                    left: 0,
                    right: 0,
                    bottom: 0,
                    zIndex: 1000,
                    pointerEvents: 'all'
                }}
            >
                {/* 반투명 배경 오버레이 */}
                <div
                    className="loading-overlay"
                    style={{
                        position: 'absolute',
                        top: 0,
                        left: 0,
                        right: 0,
                        bottom: 0,
                        backgroundColor: 'rgba(17, 17, 17, 0.85)',
                        backdropFilter: 'blur(4px)',
                        WebkitBackdropFilter: 'blur(4px)'
                    }}
                />

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
                    className="chart-loading-indicator"
                    style={{
                        position: 'relative',
                        zIndex: 1010
                    }}
                />
            </motion.div>
        </AnimatePresence>
    );
};

export default LoadingSpinner;
