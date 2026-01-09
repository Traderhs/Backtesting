import {useRef, useState} from 'react';
import {Button} from '@/Components/UI/Button.tsx';
import {useWebSocket} from '../Server/WebSocketContext';
import {BarDataType, timeframeToString, TimeframeUnit} from '@/Types/BarData.ts';
import EditorSection from './EditorSection';
import SymbolSection from './SymbolSection';
import BarDataSection from './BarDataSection';
import ConfigSection from './ConfigSection';
import ExchangeSection from './ExchangeSection';
import StrategySection from './StrategySection';
import {StrategyProvider, useStrategy} from './StrategyContext';
import '../Common/LoadingSpinner.css';

function StrategyEditorContent() {
    const {
        addLog,
        clearLogs,
        symbolConfigs,
        barDataConfigs,
        exchangeConfig,
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
            addLog('ERROR', 'API 키의 환경 변수 이름을 입력해 주세요.');
            return;
        }

        if (!exchangeConfig.apiSecretEnvVar.trim()) {
            addLog('ERROR', 'API 시크릿의 환경 변수 이름을 입력해 주세요.');
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

        // 엔진 설정 검사
        // 백테스팅 기간 체크 박스 검증: 체크 안되어있는데 칸이 비워져 있는 경우
        if (!engineConfig.useBacktestPeriodStart && !engineConfig.backtestPeriodStart.trim()) {
            addLog('ERROR', '백테스팅 기간의 시작을 입력하거나 [처음부터] 체크 박스를 선택해 주세요.');
            return;
        }

        if (!engineConfig.useBacktestPeriodEnd && !engineConfig.backtestPeriodEnd.trim()) {
            addLog('ERROR', '백테스팅 기간의 종료를 입력하거나 [끝까지] 체크 박스를 선택해 주세요.');
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
            symbolConfigs: symbolConfigs,
            barDataConfigs: configsToSend.map(config => ({
                timeframe: timeframeToString(config.timeframe),
                klinesDirectory: (config.klinesDirectory || '').replace(/\\/g, '/'),
                barDataType: config.barDataType
            })),
            useBarMagnifier: engineConfig.useBarMagnifier,
            clearAndAddBarData: needsClearAndAdd,
            strategyConfig: strategyConfig,
        }));

        // 현재 해시값 저장
        previousConfigHash.current = currentHash;
    };

    return (
        <div className="flex flex-col h-full w-full">
            {/* 메인 콘텐츠 영역 */}
            <div className="flex-1 overflow-y-auto p-6 pb-2"
                 style={{height: isLogPanelOpen ? `calc(100% - ${logPanelHeight}px)` : '100%'}}>
                <div className="flex items-center justify-between mb-4">
                    <h1 className="text-2xl font-bold text-white">전략 에디터</h1>
                    <div className="flex items-center gap-2">
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
                    </div>
                </div>

                {/* 거래소 설정 */}
                <ExchangeSection/>

                {/* 심볼 섹션 */}
                <SymbolSection/>

                {/* 바 데이터 설정 */}
                <BarDataSection/>

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
export default function StrategyEditor() {
    return (
        <StrategyProvider>
            <StrategyEditorContent/>
        </StrategyProvider>
    );
}
