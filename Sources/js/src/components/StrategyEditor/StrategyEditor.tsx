import {useRef, useState} from 'react';
import {Button} from '../ui/button';
import {useWebSocket} from '../Server/WebSocketContext';
import {BarDataConfig, BarDataType, timeframeToString, TimeframeUnit} from '@/types/barData.ts';
import LogPanel from './LogPanel';
import SymbolSection from './SymbolSection';
import BarDataSection from './BarDataSection';
import ConfigSection from './ConfigSection';
import '../Common/LoadingSpinner.css';

/**
 * 전략 에디터 컴포넌트
 * 백테스팅 전략을 편집하고 실행할 수 있는 UI 제공
 */
export default function StrategyEditor() {
    // addLog 함수 참조
    const addLogRef = useRef<((level: string, message: string, timestamp?: string | null, fileInfo?: string | null) => void) | null>(null);

    // 공통 심볼 리스트
    const [symbolConfigs, setSymbolConfigs] = useState<string[]>([]);

    // 페어 설정
    const [selectedPair, setSelectedPair] = useState<string>('USDT');
    const [customPairs, setCustomPairs] = useState<string[]>([]);

    // 기본 바 데이터 설정 (트레이딩, 돋보기, 참조 1개, 마크 가격)
    const [barDataConfigs, setBarDataConfigs] = useState<BarDataConfig[]>([
        {
            timeframe: {value: null, unit: TimeframeUnit.NULL},
            klinesDirectory: '',
            barDataType: BarDataType.TRADING
        },
        {
            timeframe: {value: null, unit: TimeframeUnit.NULL},
            klinesDirectory: '',
            barDataType: BarDataType.MAGNIFIER
        },
        {
            timeframe: {value: null, unit: TimeframeUnit.NULL},
            klinesDirectory: '',
            barDataType: BarDataType.REFERENCE
        },
        {
            timeframe: {value: null, unit: TimeframeUnit.NULL},
            klinesDirectory: '',
            barDataType: BarDataType.MARK_PRICE
        }
    ]);

    const [useBarMagnifier, setUseBarMagnifier] = useState(true);
    const [configLoaded, setConfigLoaded] = useState(false);
    const {ws} = useWebSocket();
    const [showProjectDialog, setShowProjectDialog] = useState(false);
    const [projectDirectoryInput, setProjectDirectoryInput] = useState('');
    const [projectDirectory, setProjectDirectory] = useState('');

    // 설정의 이전 해시값 저장 (변경 감지용)
    const previousConfigHash = useRef<string>('');

    // 로그 패널 상태
    const [isLogPanelOpen, setIsLogPanelOpen] = useState(false);
    const [logPanelHeight, setLogPanelHeight] = useState(400);

    // addLog 함수 래퍼
    const addLog = (level: string, message: string, timestamp?: string | null, fileInfo?: string | null) => {
        if (addLogRef.current) {
            addLogRef.current(level, message, timestamp, fileInfo);
        }
    };

    // 바 데이터 설정 해시 계산 (바 돋보기 상태 + 심볼 리스트 + 바 데이터 설정)
    const calculateConfigHash = () => {
        const config = {
            useBarMagnifier: useBarMagnifier,
            symbols: [...symbolConfigs],  // 심볼 추가 순서가 의미 있으므로 순서를 보존하여 비교
            barData: barDataConfigs.map(c => ({
                timeframe: timeframeToString(c.timeframe),
                klinesDirectory: c.klinesDirectory,
                barDataType: c.barDataType
            }))
        };

        return JSON.stringify(config);
    };


    // 백테스팅 실행
    const handleRunSingleBacktesting = () => {
        if (!ws || ws.readyState !== WebSocket.OPEN) {
            addLog('ERROR', 'WebSocket 연결이 없습니다.');
            return;
        }

        // 백테스팅 실행 시 로그 패널이 닫혀있으면 자동으로 열기
        if (!isLogPanelOpen) {
            setIsLogPanelOpen(true);
        }

        const configsToValidate = barDataConfigs.filter(config =>
            config.barDataType !== BarDataType.MAGNIFIER || useBarMagnifier
        );

        if (symbolConfigs.length === 0) {
            addLog('ERROR', '백테스팅을 실행하려면 최소 1개의 심볼이 필요합니다.');
            return;
        }

        for (const config of configsToValidate) {
            if (!config.klinesDirectory.trim()) {
                addLog('ERROR', `${config.barDataType} 바 데이터 폴더를 입력해주세요.`);
                return;
            }
        }

        // 타임프레임 유효성 검사: value 또는 unit이 비어있으면 로그 후 중단
        for (const config of configsToValidate) {
            const tf = config.timeframe;

            if (!tf || tf.value === null) {
                const msg = `${config.barDataType} 바 데이터의 타임프레임 값이 비어있습니다.`;
                addLog('ERROR', msg);

                return;
            }

            if (!tf || tf.unit === TimeframeUnit.NULL) {
                const msg = `${config.barDataType} 바 데이터의 타임프레임 단위가 비어있습니다.`;
                addLog('ERROR', msg);

                return;
            }
        }

        const configsToSend = barDataConfigs.filter(config =>
            config.barDataType !== BarDataType.MAGNIFIER || useBarMagnifier
        );

        // 현재 바 데이터 설정의 해시값 계산
        const currentHash = calculateConfigHash();

        // 해시가 없거나 (최초 실행) 해시가 변경된 경우에만 clear and add
        const needsClearAndAdd = previousConfigHash.current === '' ||
            currentHash !== previousConfigHash.current;

        ws.send(JSON.stringify({
            action: 'runSingleBacktesting',
            useBarMagnifier: useBarMagnifier,
            clearAndAddBarData: needsClearAndAdd,
            symbolConfigs: symbolConfigs,
            barDataConfigs: configsToSend.map(config => ({
                timeframe: timeframeToString(config.timeframe),
                klinesDirectory: (config.klinesDirectory || '').replace(/\\/g, '/'),
                barDataType: config.barDataType
            }))
        }));

        // 현재 해시값 저장
        previousConfigHash.current = currentHash;
    };

    return (
        <div className="flex flex-col h-full w-full">
            {/* 프로젝트 폴더 다이얼로그 & Config 관리 */}
            <ConfigSection
                projectDirectory={projectDirectory}
                setProjectDirectory={setProjectDirectory}
                projectDirectoryInput={projectDirectoryInput}
                setProjectDirectoryInput={setProjectDirectoryInput}
                showProjectDialog={showProjectDialog}
                setShowProjectDialog={setShowProjectDialog}
                useBarMagnifier={useBarMagnifier}
                setUseBarMagnifier={setUseBarMagnifier}
                symbolConfigs={symbolConfigs}
                setSymbolConfigs={setSymbolConfigs}
                selectedPair={selectedPair}
                setSelectedPair={setSelectedPair}
                customPairs={customPairs}
                setCustomPairs={setCustomPairs}
                barDataConfigs={barDataConfigs}
                setBarDataConfigs={setBarDataConfigs}
                isLogPanelOpen={isLogPanelOpen}
                setIsLogPanelOpen={setIsLogPanelOpen}
                logPanelHeight={logPanelHeight}
                setLogPanelHeight={setLogPanelHeight}
                configLoaded={configLoaded}
                setConfigLoaded={setConfigLoaded}
                addLog={addLog}
            />

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

                {/* 심볼 섹션 */}
                <SymbolSection
                    symbolConfigs={symbolConfigs}
                    setSymbolConfigs={setSymbolConfigs}
                    addLog={addLog}
                    selectedPair={selectedPair}
                    setSelectedPair={setSelectedPair}
                    customPairs={customPairs}
                    setCustomPairs={setCustomPairs}
                />

                {/* 바 데이터 설정 */}
                <BarDataSection
                    barDataConfigs={barDataConfigs}
                    setBarDataConfigs={setBarDataConfigs}
                    useBarMagnifier={useBarMagnifier}
                    setUseBarMagnifier={setUseBarMagnifier}
                    projectDirectory={projectDirectory}
                    configLoaded={configLoaded}
                />
            </div>

            {/* 로그 패널 */}
            <LogPanel
                isLogPanelOpen={isLogPanelOpen}
                setIsLogPanelOpen={setIsLogPanelOpen}
                logPanelHeight={logPanelHeight}
                setLogPanelHeight={setLogPanelHeight}
                onAddLog={(addLogFunc) => {
                    addLogRef.current = addLogFunc;
                }}
            />
        </div>
    );
}
