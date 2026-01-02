import React, {createContext, ReactNode, useContext, useState} from 'react';
import {BarDataConfig, BarDataType, TimeframeUnit} from '@/Types/BarData';

// 타입 정의
export interface LogEntry {
    level: string;
    message: string;
    timestamp: string | null;
    fileInfo: string | null;
}

export interface ExchangeConfig {
    apiKeyEnvVar: string;
    apiSecretEnvVar: string;

    exchangeInfoPath: string;
    leverageBracketPath: string;
}

export interface EngineConfig {
    // 프로젝트 폴더
    projectDirectory: string;

    useBacktestPeriodStart: boolean;  // true면 처음부터, false면 backtestPeriodStart 사용
    useBacktestPeriodEnd: boolean;    // true면 끝까지, false면 backtestPeriodEnd 사용
    backtestPeriodStart: string;
    backtestPeriodEnd: string;
    backtestPeriodFormat: string;

    // 바 돋보기 기능
    useBarMagnifier: boolean;

    // 초기 자금
    initialBalance: number | undefined;

    // 수수료
    takerFeePercentage: number | undefined;
    makerFeePercentage: number | undefined;

    // 슬리피지
    slippageModel: 'PercentageSlippage' | 'MarketImpactSlippage';
    slippageTakerPercentage: number | undefined;  // PercentageSlippage용
    slippageMakerPercentage: number | undefined;  // PercentageSlippage용
    slippageStressMultiplier: number | undefined; // MarketImpactSlippage용

    // 수량 검사
    checkMarketMaxQty: boolean;
    checkMarketMinQty: boolean;
    checkLimitMaxQty: boolean;
    checkLimitMinQty: boolean;
    checkMinNotionalValue: boolean;

    // 바 데이터 중복 검사
    checkSameBarDataWithTarget: boolean;
    checkSameBarDataTrading: boolean;
    checkSameBarDataMagnifier: boolean;
    checkSameBarDataReference: boolean;
    checkSameBarDataMarkPrice: boolean;
}

interface StrategyContextType {
    // 에디터 설정
    configLoaded: boolean;
    setConfigLoaded: React.Dispatch<React.SetStateAction<boolean>>;

    logs: LogEntry[];
    addLog: (level: string, message: string, timestamp?: string | null, fileInfo?: string | null) => void;
    clearLogs: () => void;

    // 거래소 설정
    exchangeConfig: ExchangeConfig;
    setExchangeConfig: React.Dispatch<React.SetStateAction<ExchangeConfig>>;

    lastDataUpdates: string;
    setLastDataUpdates: React.Dispatch<React.SetStateAction<string>>;

    // 심볼 설정
    symbolConfigs: string[];
    setSymbolConfigs: React.Dispatch<React.SetStateAction<string[]>>;

    selectedPair: string;
    setSelectedPair: React.Dispatch<React.SetStateAction<string>>;

    customPairs: string[];
    setCustomPairs: React.Dispatch<React.SetStateAction<string[]>>;

    // 바 데이터 설정
    barDataConfigs: BarDataConfig[];
    setBarDataConfigs: React.Dispatch<React.SetStateAction<BarDataConfig[]>>;

    // 엔진 설정
    engineConfig: EngineConfig;
    setEngineConfig: React.Dispatch<React.SetStateAction<EngineConfig>>;
}

const StrategyContext = createContext<StrategyContextType | undefined>(undefined);

export function StrategyProvider({children}: { children: ReactNode }) {
    // 에디터 설정
    const [configLoaded, setConfigLoaded] = useState(false);
    const [logs, setLogs] = useState<LogEntry[]>([]);

    // 거래소 설정
    const [exchangeConfig, setExchangeConfig] = useState<ExchangeConfig>({
        apiKeyEnvVar: '',
        apiSecretEnvVar: '',
        exchangeInfoPath: '',
        leverageBracketPath: ''
    });

    const [lastDataUpdates, setLastDataUpdates] = useState<string>('');

    // 심볼 설정
    const [symbolConfigs, setSymbolConfigs] = useState<string[]>([]);
    const [selectedPair, setSelectedPair] = useState<string>('USDT');
    const [customPairs, setCustomPairs] = useState<string[]>([]);

    // 바 데이터 설정
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

    // 엔진 설정
    const [engineConfig, setEngineConfig] = useState<EngineConfig>({
        projectDirectory: '',

        useBacktestPeriodStart: true,
        useBacktestPeriodEnd: true,
        backtestPeriodStart: '',
        backtestPeriodEnd: '',
        backtestPeriodFormat: '%Y-%m-%d %H:%M:%S',

        useBarMagnifier: true,

        initialBalance: undefined,

        takerFeePercentage: undefined,
        makerFeePercentage: undefined,

        slippageModel: 'MarketImpactSlippage',
        slippageTakerPercentage: undefined,
        slippageMakerPercentage: undefined,
        slippageStressMultiplier: undefined,

        checkMarketMaxQty: true,
        checkMarketMinQty: true,
        checkLimitMaxQty: true,
        checkLimitMinQty: true,
        checkMinNotionalValue: true,

        checkSameBarDataWithTarget: true,
        checkSameBarDataTrading: true,
        checkSameBarDataMagnifier: true,
        checkSameBarDataReference: true,
        checkSameBarDataMarkPrice: true
    });

    // 호출자 파일명을 간단히 추출
    const getCallerFileInfo = (): string | null => {
        try {
            const err = new Error();
            const stack = err.stack || '';
            const lines = stack.split('\n').map(l => l.trim());

            for (let i = 1; i < lines.length; i++) {
                const line = lines[i];
                if (!line || line.includes('getCallerFileInfo') || line.includes('addLog')) {
                    continue;
                }

                const m = line.match(/([^\s)]+?\.(tsx|ts|js|jsx|mjs))(?:\?[^:\s]*)?(?::\d+:\d+)?$/i);
                if (!m) {
                    continue;
                }

                let fp = m[1].split('?')[0];
                const parts = fp.split(/[\\/]/);
                let filename = parts[parts.length - 1];

                const extMatch = filename.match(/\.(tsx|ts|js|jsx|mjs)$/i);
                if (!extMatch) {
                    continue;
                }

                const ext = extMatch[0];
                let base = filename.slice(0, -ext.length);
                base = base.replace(/-[A-Za-z0-9]+$/, '');

                let finalExt = ext;
                if (finalExt === '.js') {
                    finalExt = '.tsx';
                }

                return base + finalExt;
            }
        } catch {
            // 무시
        }
        return null;
    };

    const addLog = (level: string, message: string, timestamp?: string | null, fileInfo?: string | null) => {
        if (level !== 'SEPARATOR' && !message.startsWith(' |')) {
            message = ' | ' + message;
        }

        if (!fileInfo) {
            const caller = getCallerFileInfo();
            if (caller) {
                fileInfo = caller;
            }
        }

        const finalTimestamp = timestamp || (() => {
            const d = new Date();
            const pad = (n: number) => String(n).padStart(2, '0');
            return `${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())} ${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())}`;
        })();

        if (level === 'SEPARATOR') {
            setLogs(prev => [...prev, {level, message, timestamp: finalTimestamp, fileInfo: fileInfo || null}]);
            return;
        }

        const isCppLog = !!(fileInfo && /\.cpp\b/i.test(fileInfo));

        if (isCppLog) {
            setLogs(prev => [...prev, {level, message, timestamp: finalTimestamp, fileInfo: fileInfo || null}]);
        } else {
            setLogs(prev => [
                ...prev,
                {level, message, timestamp: finalTimestamp, fileInfo: fileInfo || null},
                {level: 'SEPARATOR', message: '', timestamp: null, fileInfo: null}
            ]);
        }
    };

    const clearLogs = () => {
        setLogs([]);
    };

    const value = {
        configLoaded,
        setConfigLoaded,

        logs,
        addLog,
        clearLogs,

        exchangeConfig,
        setExchangeConfig,

        lastDataUpdates,
        setLastDataUpdates,

        symbolConfigs,
        setSymbolConfigs,

        selectedPair,
        setSelectedPair,

        customPairs,
        setCustomPairs,

        barDataConfigs,
        setBarDataConfigs,

        engineConfig,
        setEngineConfig
    };

    return (
        <StrategyContext.Provider value={value}>
            {children}
        </StrategyContext.Provider>
    );
}

export function useStrategy() {
    const context = useContext(StrategyContext);
    if (context === undefined) {
        throw new Error('useStrategy must be used within a StrategyProvider');
    }
    return context;
}
