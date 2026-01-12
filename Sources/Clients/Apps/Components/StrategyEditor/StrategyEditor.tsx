import {useEffect, useRef, useState} from 'react';
import {createPortal} from 'react-dom';
import {motion} from 'framer-motion';
import {Button} from '@/Components/UI/Button.tsx';
import {useWebSocket} from '../Server/WebSocketContext';
import {BarDataType, timeframeToString, TimeframeUnit} from '@/Types/BarData.ts';
import EditorSection from './EditorSection';
import SymbolSection from './SymbolSection';
import BarDataSection from './BarDataSection';
import FundingRateSection from './FundingRateSection';
import ConfigSection from './ConfigSection';
import ExchangeSection from './ExchangeSection';
import StrategySection from './StrategySection';
import {StrategyProvider, useStrategy} from './StrategyContext';
import '../Common/LoadingSpinner.css';
import './StrategyEditor.css';

function StrategyEditorContent({isActive}: { isActive: boolean }) {
    const [titleBarPortal, setTitleBarPortal] = useState<HTMLElement | null>(null);

    useEffect(() => {
        setTitleBarPortal(document.getElementById('titlebar-actions-portal'));
    }, []);

    const {
        addLog,
        clearLogs,
        exchangeConfig,
        symbolConfigs,
        barDataConfigs,
        fundingRatesDirectory,
        engineConfig,
        strategyConfig
    } = useStrategy();

    const {ws} = useWebSocket();

    // 설정의 이전 해시값 저장 (변경 감지용)
    const previousConfigHash = useRef<string>('');

    const [isLogPanelOpen, setIsLogPanelOpen] = useState(false);
    const [logPanelHeight, setLogPanelHeight] = useState(400);

    // 바 데이터 설정 해시 계산 (심볼 리스트 + 바 데이터 설정 + 바 돋보기 상태)
    const calculateConfigHash = () => {
        const config = {
            symbols: [...symbolConfigs],  // 심볼 추가 순서가 의미 있으므로 순서를 보존하여 비교
            barData: barDataConfigs.map(c => ({
                timeframe: timeframeToString(c.timeframe),
                klinesDirectory: c.klinesDirectory,
                barDataType: c.barDataType
            })),
            useBarMagnifier: engineConfig.useBarMagnifier
        };

        return JSON.stringify(config);
    };

    // 백테스팅 실행
    const handleRunSingleBacktesting = async () => {
        // 백테스팅 시작 시간
        const backtestingStartTime = Date.now();

        // 로그 초기화
        clearLogs();

        if (!ws || ws.readyState !== WebSocket.OPEN) {
            addLog('ERROR', 'WebSocket 연결이 없습니다.');
            return;
        }

        // 백테스팅 실행 시 로그 패널이 닫혀있으면 자동으로 열기
        if (!isLogPanelOpen) {
            setIsLogPanelOpen(true);
        }

        if (!exchangeConfig.apiKeyEnvVar.trim()) {
            addLog('ERROR', 'Binance API Key의 환경 변수 이름을 입력해 주세요.');
            return;
        }

        if (!exchangeConfig.apiSecretEnvVar.trim()) {
            addLog('ERROR', 'Binance API Secret의 환경 변수 이름을 입력해 주세요.');
            return;
        }

        if (!exchangeConfig.exchangeInfoPath.trim()) {
            addLog('ERROR', '거래소 정보 파일 경로를 입력해 주세요.');
            return;
        }

        if (!exchangeConfig.leverageBracketPath.trim()) {
            addLog('ERROR', '레버리지 구간 파일 경로를 입력해 주세요.');
            return;
        }

        if (symbolConfigs.length === 0) {
            addLog('ERROR', '백테스팅을 실행하려면 최소 1개의 심볼이 필요합니다. 심볼을 추가해 주세요.');
            return;
        }

        const configsToValidate = barDataConfigs.filter(config =>
            config.barDataType !== BarDataType.MAGNIFIER || engineConfig.useBarMagnifier
        );

        for (const config of configsToValidate) {
            if (!config.klinesDirectory.trim()) {
                addLog('ERROR', `${config.barDataType} 바 데이터의 폴더를 입력해 주세요.`);
                return;
            }
        }

        // 타임프레임 유효성 검사
        for (const config of configsToValidate) {
            const tf = config.timeframe;
            if (!tf || tf.value === null) {
                addLog('ERROR', `${config.barDataType} 바 데이터의 타임프레임 값을 입력해 주세요.`);
                return;
            }

            if (!tf || tf.unit === TimeframeUnit.NULL) {
                addLog('ERROR', `${config.barDataType} 바 데이터의 타임프레임 단위를 입력해 주세요.`);
                return;
            }
        }

        if (!fundingRatesDirectory.trim()) {
            addLog('ERROR', '펀딩 비율 폴더를 입력해 주세요.');
            return;
        }

        // 엔진 설정 검사
        // 백테스팅 기간 체크 박스 검증: 체크 안되어있는데 칸이 비워져 있는 경우
        if (!engineConfig.useBacktestPeriodStart && !engineConfig.backtestPeriodStart.trim()) {
            addLog('ERROR', '백테스팅 시작 시간을 입력하거나 [처음부터] 체크 박스를 선택해 주세요.');
            return;
        }

        if (!engineConfig.useBacktestPeriodEnd && !engineConfig.backtestPeriodEnd.trim()) {
            addLog('ERROR', '백테스팅 종료 시간을 입력하거나 [끝까지] 체크 박스를 선택해 주세요.');
            return;
        }

        if (!engineConfig.initialBalance) {
            addLog('ERROR', '초기 자금을 입력해 주세요.');
            return;
        }

        if (!engineConfig.takerFeePercentage) {
            addLog('ERROR', '테이커 수수료율을 입력해 주세요.');
            return;
        }

        if (!engineConfig.makerFeePercentage) {
            addLog('ERROR', '메이커 수수료율을 입력해 주세요.');
            return;
        }

        if (engineConfig.slippageModel == 'PercentageSlippage') {
            if (!engineConfig.slippageTakerPercentage) {
                addLog('ERROR', '테이커 슬리피지율을 입력해 주세요.');
                return;
            }

            if (!engineConfig.slippageMakerPercentage) {
                addLog('ERROR', '메이커 슬리피지율을 입력해 주세요.');
                return;
            }
        } else if (engineConfig.slippageModel == 'MarketImpactSlippage') {
            if (!engineConfig.slippageStressMultiplier) {
                addLog('ERROR', '슬리피지 스트레스 계수를 입력해 주세요.');
                return;
            }
        }

        // 전략 빌드 먼저 수행
        if (strategyConfig && strategyConfig.strategySourcePath) {
            addLog('INFO', `[${strategyConfig.name}] 전략의 빌드를 시작합니다.`);

            // strategyHeaderPath 검증
            if (!strategyConfig.strategyHeaderPath) {
                addLog('ERROR', '전략 헤더 파일 경로가 존재하지 않습니다.');
                return;
            }

            try {
                const buildResponse = await fetch('/api/strategy/build', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify({
                        strategySourcePath: strategyConfig.strategySourcePath,
                        strategyHeaderPath: strategyConfig.strategyHeaderPath,
                        indicatorHeaderDirs: strategyConfig.indicatorHeaderDirs,
                        indicatorSourceDirs: strategyConfig.indicatorSourceDirs
                    })
                });

                const buildResult = await buildResponse.json();

                if (buildResult.success) {
                    addLog("SEPARATOR", "");
                } else {
                    // 오류 로그는 서버에서 기록
                    return;
                }
            } catch (err: any) {
                addLog('ERROR', `빌드 요청 실패: ${err.message}`);
                return;
            }
        } else {
            addLog('ERROR', '전략을 추가해 주세요.');
            return;
        }

        // =============================================================================================================
        const configsToSend = barDataConfigs.filter(config =>
            config.barDataType !== BarDataType.MAGNIFIER || engineConfig.useBarMagnifier
        );

        // 현재 바 데이터 설정의 해시값 계산
        const currentHash = calculateConfigHash();

        // 해시가 없거나 (최초 실행) 해시가 변경된 경우에만 clear and add
        const needsClearAndAdd = previousConfigHash.current === '' ||
            currentHash !== previousConfigHash.current;

        ws.send(JSON.stringify({
            action: 'runSingleBacktesting',
            backtestingStartTime: backtestingStartTime,
            symbolConfigs: symbolConfigs,
            barDataConfigs: configsToSend.map(config => ({
                timeframe: timeframeToString(config.timeframe),
                klinesDirectory: (config.klinesDirectory || '').replace(/\\/g, '/'),
                barDataType: config.barDataType
            })),
            fundingRatesDirectory: (fundingRatesDirectory || '').replace(/\\/g, '/'),
            useBarMagnifier: engineConfig.useBarMagnifier,
            clearAndAddBarData: needsClearAndAdd,
            strategyConfig: strategyConfig
        }));

        // 현재 해시값 저장
        previousConfigHash.current = currentHash;
    };

    return (
        <div className="flex flex-col h-full w-full">
            {/* 메인 콘텐츠 영역 */}
            <div className="h-full w-full flex flex-col p-4 overflow-y-auto"
                 style={{
                     height: isLogPanelOpen ? `calc(100% - ${logPanelHeight}px)` : '100%'
                 }}>
                {/* 제목 영역 */}
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
                            marginLeft: '35px',
                            marginTop: '10px',
                            paddingBottom: '8px',
                            display: 'inline-block',
                            position: 'relative',
                        }}
                    >
                        전략 에디터
                        {/* 밑줄 */}
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

                {isActive && titleBarPortal && createPortal(
                    <div className="flex items-center gap-2" style={{WebkitAppRegion: 'no-drag'} as any}>
                        <Button
                            onClick={() => setIsLogPanelOpen(!isLogPanelOpen)}
                            className="bg-gray-700 hover:bg-gray-600 text-white font-semibold px-4 py-2 rounded-lg transition-colors"
                        >
                            {isLogPanelOpen ? '로그 숨기기' : '로그 표시'}
                        </Button>
                        <Button
                            onClick={handleRunSingleBacktesting}
                            className="bg-blue-600 hover:bg-blue-700 text-white font-semibold px-6 py-2 rounded-lg transition-colors"
                        >
                            백테스팅 실행
                        </Button>
                    </div>,
                    titleBarPortal
                )}

                <div className="grid grid-cols-1 lg:grid-cols-2 gap-4">
                    {/* 거래소 설정 */}
                    <div className="h-full">
                        <ExchangeSection/>
                    </div>

                    {/* 심볼 섹션 */}
                    <div className="h-full">
                        <SymbolSection/>
                    </div>
                </div>

                {/* 바 데이터 설정 */}
                <BarDataSection/>

                {/* 펀딩 비율 설정 */}
                <FundingRateSection/>

                {/* 엔진 설정 */}
                <ConfigSection/>

                {/* 전략 설정 */}
                <StrategySection/>
            </div>

            {/* 에디터 설정 */}
            <EditorSection
                isLogPanelOpen={isLogPanelOpen}
                setIsLogPanelOpen={setIsLogPanelOpen}
                logPanelHeight={logPanelHeight}
                setLogPanelHeight={setLogPanelHeight}
            />
        </div>
    );
}

/**
 * 전략 에디터 컴포넌트
 * 백테스팅 전략을 편집하고 실행할 수 있는 UI 제공
 */
export default function StrategyEditor({isActive}: { isActive?: boolean }) {
    return (
        <StrategyProvider>
            <StrategyEditorContent isActive={isActive ?? true}/>
        </StrategyProvider>
    );
}
