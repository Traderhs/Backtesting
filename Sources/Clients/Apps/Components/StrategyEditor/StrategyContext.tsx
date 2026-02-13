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

export interface StrategyConfig {
    // 전략 섹션에서 관리되는 폴더 목록
    strategyHeaderDirs?: string[];
    strategySourceDirs?: string[];
    indicatorHeaderDirs?: string[];
    indicatorSourceDirs?: string[];

    // 선택된 전략 정보
    name: string;
    dllPath?: string | null;
    strategyHeaderPath?: string | null;
    strategySourcePath?: string | null;
}

interface StrategyContextType {
    // 에디터 설정
    configLoaded: boolean;
    setConfigLoaded: React.Dispatch<React.SetStateAction<boolean>>;

    // 백테스팅 실행 상태
    isBacktestingRunning: boolean;
    setIsBacktestingRunning: React.Dispatch<React.SetStateAction<boolean>>;

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

    // 펀딩 비율 설정
    fundingRatesDirectory: string;
    setFundingRatesDirectory: React.Dispatch<React.SetStateAction<string>>;

    // 엔진 설정
    engineConfig: EngineConfig;
    setEngineConfig: React.Dispatch<React.SetStateAction<EngineConfig>>;

    // 전략 설정
    strategyConfig: StrategyConfig | null;
    setStrategyConfig: React.Dispatch<React.SetStateAction<StrategyConfig | null>>;
}

const StrategyContext = createContext<StrategyContextType | undefined>(undefined);

export function StrategyProvider({children}: { children: ReactNode }) {
    // 에디터 설정
    const [configLoaded, setConfigLoaded] = useState(false);
    const [isBacktestingRunning, setIsBacktestingRunning] = useState(false);
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

    // 펀딩 비율 설정
    const [fundingRatesDirectory, setFundingRatesDirectory] = useState<string>('');

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

    // 전략 설정
    const [strategyConfig, setStrategyConfig] = useState<StrategyConfig | null>({
        strategyHeaderDirs: undefined,
        strategySourceDirs: undefined,
        indicatorHeaderDirs: undefined,
        indicatorSourceDirs: undefined,

        name: '',
        dllPath: null,
        strategyHeaderPath: null,
        strategySourcePath: null
    });

    // 호출자 파일명을 간단히 추출
    const getCallerFileInfo = (): string | null => {
        try {
            const err = new Error();
            const stack = err.stack || '';
            const lines = stack.split('\n').map(l => l.trim());

            for (let i = 1; i < lines.length; i++) {
                const line = lines[i];
                // 내부 함수/유틸 호출 라인은 건너뜀
                if (!line || line.includes('getCallerFileInfo') || line.includes('addLog') || line.includes('StrategyContext')) {
                    continue;
                }

                // 파일:라인:컬럼 형식 캡처
                const m = line.match(/\(?([^\s)]+):(\d+):(\d+)\)?$/);
                if (!m) {
                    continue;
                }

                let fp = m[1];
                const ln = m[2];

                // 쿼리스트링 제거
                fp = fp.split('?')[0];

                // 번들 해시 같은 접미사 제거 (예: main-abc123.js -> main.js)
                fp = fp.replace(/-([A-Za-z0-9]+)(?=\.(js|ts|tsx|jsx|mjs)$)/, '');

                const parts = fp.split(/[\\/]/);
                const filename = parts[parts.length - 1];

                return `${filename}:${ln}`;
            }
        } catch {
            // 무시
        }
        return null;
    };

    const addLog = (level: string, message: any, timestamp?: string | null, fileInfo?: string | null) => {
        // message를 안전하게 문자열로 정규화
        if (message === undefined || message === null) {
            message = '';
        }

        if (typeof message !== 'string') {
            try {
                message = String(message);
            } catch (e) {
                message = '';
            }
        }

        // 일반 로그는 앞에 구분자 프리픽스가 없으면 추가
        if (!message.startsWith(' |')) {
            message = ' | ' + message;
        }

        // 호출자 정보가 없는 경우 스택에서 추출
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

        // 일반 로그는 그대로 추가
        setLogs(prev => [...prev, {level, message, timestamp: finalTimestamp, fileInfo: fileInfo || null}]);
    };

    const clearLogs = () => {
        setLogs([]);
    };

    const value = {
        configLoaded,
        setConfigLoaded,

        isBacktestingRunning,
        setIsBacktestingRunning,

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

        fundingRatesDirectory,
        setFundingRatesDirectory,

        engineConfig,
        setEngineConfig,

        strategyConfig,
        setStrategyConfig
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
