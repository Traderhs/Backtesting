import {useState} from 'react';
import {motion} from 'framer-motion';
import Report from './Report';
import Streaks from './Streaks';
import './Performance.css';
import React from 'react';
import LoadingSpinner from '@/components/Common/LoadingSpinner';
import NoDataMessage from '@/components/Common/NoDataMessage';
import {useTradeFilter} from '@/components/TradeFilter';

/**
 * 페이지 제목 컴포넌트
 */
const PageTitle = React.memo(() => (
    <div style={{
        position: 'relative',
        marginBottom: '25px',
        zIndex: 100
    }}>
        <motion.h2
            initial={{opacity: 0, x: -20}}
            animate={{opacity: 1, x: 0}}
            transition={{delay: 0.1, duration: 0.5}}
            style={{
                color: 'white',
                fontSize: '2.5rem',
                fontWeight: 700,
                textAlign: 'left',
                marginLeft: '39px',
                marginTop: '14px',
                marginBottom: '-7px',
                paddingBottom: '8px',
                display: 'inline-block',
                position: 'relative',
            }}
        >
            성과 지표
            <motion.span
                initial={{width: 0}}
                animate={{width: '100%'}}
                transition={{delay: 0.3, duration: 0.5}}
                style={{
                    position: 'absolute',
                    bottom: 0,
                    left: 0,
                    right: 0,
                    height: '2px',
                    background: 'rgba(255, 215, 0, 0.4)',
                }}
            />
        </motion.h2>
    </div>
));

interface PerformanceProps {
    config: any;
}

/**
 * 성과 지표 컴포넌트
 * Report와 Streaks 컴포넌트 로딩 완료 여부를 추적하여 메인 로딩 스피너를 제어합니다.
 */
const Performance: React.FC<PerformanceProps> = ({config}) => {
    const [isReportReady, setIsReportReady] = useState(false);
    const [isStreaksReady, setIsStreaksReady] = useState(false);
    const {filteredTrades} = useTradeFilter();

    const handleReportReady = () => {
        setIsReportReady(true);
    };

    const handleStreaksReady = () => {
        setIsStreaksReady(true);
    };

    // 모든 컴포넌트가 준비되었는지 여부
    const isLoading = !isReportReady || !isStreaksReady;

    // 거래 데이터가 없는 경우
    if (!filteredTrades || filteredTrades.length === 1) {
        return <NoDataMessage message="거래 내역이 존재하지 않습니다."/>;
    }

    return (
        <motion.div
            initial={{opacity: 0}}
            animate={{opacity: 1}}
            transition={{duration: 0.5}}
            style={{
                width: '100%',
                height: '100%',
                padding: '12px',
                overflow: 'auto',
                display: 'flex',
                flexDirection: 'column',
                position: 'relative'
            }}
            className="performance-container"
        >
            {/* 로딩 중이면 스피너 오버레이 표시 */}
            {isLoading && (
                <div style={{
                    position: 'absolute',
                    top: 0,
                    left: 0,
                    right: 0,
                    bottom: 0,
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    backgroundColor: 'rgba(0, 0, 0, 0.7)',
                    zIndex: 1000
                }}>
                    <LoadingSpinner/>
                </div>
            )}

            {/* 제목 */}
            <PageTitle/>

            {/* 컨텐츠 래퍼 - 항상 렌더링 */}
            <motion.div
                initial={{opacity: 0, y: 20}}
                animate={{opacity: isLoading ? 0 : 1, y: isLoading ? 20 : 0}}
                transition={{delay: 0.3, duration: 0.5}}
                className="flex flex-row space-x-6"
                style={{
                    flex: 1,
                    width: '100%',
                    position: 'relative',
                    marginBottom: '20px',
                    zIndex: 1
                }}
            >
                {/* Report 컴포넌트를 왼쪽 절반에 배치 (왼쪽 여백 추가) */}
                <motion.div
                    className="w-1/2 pl-6"
                    initial={{opacity: 0, x: -20}}
                    animate={{opacity: isLoading ? 0 : 1, x: isLoading ? -20 : 0}}
                    transition={{delay: 0.4, duration: 0.5}}
                    style={{background: 'transparent'}}
                >
                    <Report key="report" onReady={handleReportReady} config={config}/>
                </motion.div>

                {/* Streaks 컴포넌트를 오른쪽 절반에 배치 (오른쪽 여백 추가) */}
                <motion.div
                    className="w-1/2 pr-6"
                    initial={{opacity: 0, x: 20}}
                    animate={{opacity: isLoading ? 0 : 1, x: isLoading ? 20 : 0}}
                    transition={{delay: 0.5, duration: 0.5}}
                    style={{
                        background: 'transparent'
                    }}
                >
                    <Streaks key="streaks" onReady={handleStreaksReady}/>
                </motion.div>
            </motion.div>

            {/* 애니메이션 스타일 */}
            <div dangerouslySetInnerHTML={{
                __html: `
        <style>
          @keyframes spin {
            0% { transform: rotate(0deg); }
            100% { transform: rotate(360deg); }
          }
          
          @keyframes float {
            0% { transform: translateY(0px); }
            50% { transform: translateY(-10px); }
            100% { transform: translateY(0px); }
          }
          
          @keyframes pulse {
            0% { box-shadow: 0 0 0 0 rgba(255, 215, 0, 0.4); }
            70% { box-shadow: 0 0 0 10px rgba(255, 215, 0, 0); }
            100% { box-shadow: 0 0 0 0 rgba(255, 215, 0, 0); }
          }
        </style>
      `
            }}/>
        </motion.div>
    );
};

export default React.memo(Performance);
