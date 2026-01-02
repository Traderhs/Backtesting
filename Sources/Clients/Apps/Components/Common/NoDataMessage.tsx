'use client'
import React from 'react';
import {motion} from 'framer-motion';

interface NoDataMessageProps {
    message?: string;
    fontSize?: string;
    justifyContent?: 'flex-start' | 'center' | 'flex-end';
    alignItems?: 'flex-start' | 'center' | 'flex-end';
    customStyle?: React.CSSProperties;
}

const NoDataMessage: React.FC<NoDataMessageProps> = ({
                                                         message = "데이터가 존재하지 않습니다.",
                                                         fontSize = '1.5rem',
                                                         justifyContent = 'center',
                                                         alignItems = 'center',
                                                         customStyle = {}
                                                     }) => (
    <motion.div
        initial={{opacity: 0}}
        animate={{opacity: 1}}
        style={{
            width: '100%',
            height: '100%',
            display: 'flex',
            flexDirection: 'column',
            justifyContent: justifyContent,
            alignItems: alignItems,
            background: '#111111',
            position: 'relative',
            ...customStyle
        }}
    >
        <motion.div
            initial={{y: -20, opacity: 0}}
            animate={{y: 0, opacity: 1}}
            transition={{
                type: "spring",
                stiffness: 300,
                damping: 25,
                delay: 0.2
            }}
            style={{
                color: 'white',
                fontSize: fontSize,
                fontWeight: 500,
                fontFamily: "'Inter', 'Pretendard', sans-serif",
                textShadow: '0 0 15px rgba(255, 215, 0, 0.7)',
                pointerEvents: 'none',
                userSelect: 'none',
                WebkitUserSelect: 'none',
                MozUserSelect: 'none',
                msUserSelect: 'none'
            }}
        >
            {message}
        </motion.div>
    </motion.div>
);

export default NoDataMessage;
