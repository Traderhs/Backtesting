import {useState, useEffect, useMemo, memo, useRef} from "react";
import SymbolCard from "./SymbolCard"
import StrategyIndicatorCard from "./StrategyIndicatorCard"
import EngineCard from "./EngineCard"
import LoadingSpinner from "../Common/LoadingSpinner";

export interface BarDataRaw {
    period: {
        start: string
        end: string
    }
    timeframe: string
    count: number
    missing: {
        count: number
        times: string[]
    }
    path: string
}

export interface StrategyRaw {
    name: string
    sourcePath?: string
    headerPath?: string
    className?: string
    indicators: {
        name: string
        timeframe: string
        sourcePath?: string
        headerPath?: string
        className?: string
    }[]
}

export interface ExchangeInfoRaw {
    dataPath: string
    priceStep: number
    pricePrecision: number
    qtyStep: number
    qtyPrecision: number
    maxOrderQty: number
    minOrderQty: number
    maxMarketOrderQty: number
    minMarketOrderQty: number
    minNotional: number
    liquidationFee: number
}

export interface LeverageBracketRaw {
    bracketNum: number
    minNotional: number
    maxNotional: number
    maxLeverage: number
    maintMarginRatio: number
    maintAmount: number
}

export interface FundingRatesRaw {
    dataPath: string
    period: {
        start: string
        end: string
    }
    totalCount: number
    positiveCount: number
    negativeCount: number
    averageFundingRate: number
    maxFundingRate: number
    minFundingRate: number
}

export interface SymbolRaw {
    symbolName: string
    tradingBarData: Record<string, unknown>
    magnifierBarData?: Record<string, unknown>
    referenceBarData: Record<string, unknown>[]
    markPriceBarData: Record<string, unknown>
    exchangeInfo?: Record<string, unknown>
    leverageBrackets?: Record<string, unknown>[]
    fundingRates?: Record<string, unknown>
}

export interface ConfigJson {
    symbols: SymbolRaw[]
    strategies: StrategyRaw[]
    settings: Record<string, unknown>
}

interface ConfigProps {
    config: any;
}

// 성능 최적화를 위해 PageTitle 컴포넌트를 메모이제이션
const PageTitle = memo(() => (
    <div style={{
        position: 'relative',
        marginBottom: '25px',
        zIndex: 100
    }}>
        <h2
            style={{
                color: 'white',
                fontSize: '2.5rem',
                fontWeight: 700,
                textAlign: 'left',
                marginLeft: '51px',
                marginTop: '26px',
                paddingBottom: '8px',
                display: 'inline-block',
                position: 'relative',
            }}
        >
            백테스팅 설정
            <span
                style={{
                    position: 'absolute',
                    bottom: 0,
                    left: 0,
                    right: 0,
                    height: '2px',
                    background: 'rgba(255, 215, 0, 0.4)',
                    width: '100%',
                }}
            />
        </h2>
    </div>
));

// 주요 Config 컴포넌트를 메모이제이션하여 불필요한 리렌더링 방지
const Config = memo(({config: rawConfig}: ConfigProps) => {
    const [parsedConfig, setParsedConfig] = useState<ConfigJson | null>(null);
    const [isLoading, setIsLoading] = useState(true);
    const [showContent, setShowContent] = useState(false);

    // 이 컴포넌트가 현재 화면에 보이는지 여부를 추적
    const isVisible = useRef(true);

    // 가시성 변경 감지를 위한 IntersectionObserver 설정
    useEffect(() => {
        // 현재 컴포넌트의 부모 요소 찾기
        const configElement = document.querySelector('.tab-content[data-tab="Config"]');
        if (!configElement) return;

        const observer = new IntersectionObserver(
            (entries) => {
                const [entry] = entries;
                isVisible.current = entry.isIntersecting;
            },
            {threshold: 0.1} // 10% 이상 보일 때 감지
        );

        observer.observe(configElement);

        return () => {
            observer.disconnect();
        };
    }, []);

    useEffect(() => {
        if (!rawConfig) {
            setParsedConfig(null);
            setIsLoading(true);
            setShowContent(false);
            return;
        }

        try {
            setIsLoading(true);
            setShowContent(false);

            // 성능 개선: 데이터 파싱 작업을 requestAnimationFrame 내에서 실행
            const parseData = () => {
                const symbols = (rawConfig["심볼"] as Record<string, unknown>[]).map((s): SymbolRaw => {
                    const symbol = s as Record<string, unknown>;
                    const magnifierData = symbol["돋보기 바 데이터"] as Record<string, unknown> | unknown[] | undefined;

                    // Check if magnifierData is an empty object or empty array
                    const isEmpty =
                        magnifierData &&
                        (Array.isArray(magnifierData) ? magnifierData.length === 0 :
                            Object.keys(magnifierData as object).length === 0);

                    return {
                        symbolName: symbol["심볼 이름"] as string,
                        exchangeInfo: symbol["거래소 정보"] as Record<string, unknown>,
                        leverageBrackets: symbol["레버리지 구간"] as Record<string, unknown>[],
                        fundingRates: symbol["펀딩 비율"] as Record<string, unknown>,
                        tradingBarData: symbol["트레이딩 바 데이터"] as Record<string, unknown>,
                        magnifierBarData: isEmpty ? undefined : magnifierData as Record<string, unknown>,
                        referenceBarData: symbol["참조 바 데이터"] as Record<string, unknown>[],
                        markPriceBarData: symbol["마크 가격 바 데이터"] as Record<string, unknown>,
                    };
                });

                // 전략 객체에서 정보 추출
                const strategy = rawConfig["전략"] as Record<string, string>;
                // 지표 배열에서 정보 추출
                const indicatorsRaw = rawConfig["지표"] as Record<string, unknown>[] | undefined;
                const indicatorsArray = Array.isArray(indicatorsRaw) ? indicatorsRaw : [];

                const filteredIndicators = indicatorsArray.map((i) => ({
                    name: i["지표 이름"] as string,
                    className: i["지표 클래스 이름"] as string | undefined,
                    timeframe: i["타임프레임"] as string,
                    sourcePath: i["소스 파일 경로"] as string | undefined,
                    headerPath: i["헤더 파일 경로"] as string | undefined
                }));

                const strategies = [{
                    name: strategy["전략 이름"],
                    className: strategy["전략 클래스 이름"],
                    sourcePath: strategy["소스 파일 경로"],
                    headerPath: strategy["헤더 파일 경로"],
                    indicators: filteredIndicators,
                }];

                const settings = rawConfig["엔진 설정"] as Record<string, unknown>;

                setParsedConfig({symbols, strategies, settings});

                // 데이터 로딩 완료 처리 - 최소 1초 로딩
                setTimeout(() => {
                    setIsLoading(false);
                    setShowContent(true);
                }, 1000);
            };

            // 비동기적으로 파싱 수행하여 렌더링 블록 방지
            setTimeout(parseData, 0);
        } catch (error) {
            console.error("Config 데이터 파싱 오류:", error);
            setParsedConfig(null);
            setIsLoading(false);
            setShowContent(false);
        }

        return () => {
            // 정리 작업
        };
    }, [rawConfig]);

    // 변환 함수들을 useMemo로 최적화
    const convertBar = useMemo(() => (bar: Record<string, unknown>): BarDataRaw => ({
        period: {
            start: (bar["데이터 기간"] as Record<string, string>)["시작"],
            end: (bar["데이터 기간"] as Record<string, string>)["종료"]
        },
        timeframe: bar["타임프레임"] as string,
        count: bar["바 개수"] as number,
        missing: {
            count: (bar["누락된 바"] as Record<string, unknown>)["개수"] as number,
            times: (bar["누락된 바"] as Record<string, unknown>)["시간"] as string[]
        },
        path: bar["데이터 경로"] as string
    }), []);

    // 다른 변환 함수들도 useMemo로 최적화
    const convertExchangeInfo = useMemo(() => (info: Record<string, unknown>): ExchangeInfoRaw => ({
        dataPath: info["데이터 경로"] as string,
        priceStep: info["가격 최소 단위"] as number,
        pricePrecision: info["가격 소수점 정밀도"] as number,
        qtyStep: info["수량 최소 단위"] as number,
        qtyPrecision: info["수량 소수점 정밀도"] as number,
        maxOrderQty: info["지정가 최대 수량"] as number,
        minOrderQty: info["지정가 최소 수량"] as number,
        maxMarketOrderQty: info["시장가 최대 수량"] as number,
        minMarketOrderQty: info["시장가 최소 수량"] as number,
        minNotional: info["최소 명목 가치"] as number,
        liquidationFee: info["강제 청산 수수료율"] as number,
    }), []);

    const convertLeverageBracket = useMemo(() => (bracket: Record<string, unknown>): LeverageBracketRaw => ({
        bracketNum: bracket["구간 번호"] as number,
        minNotional: bracket["최소 명목 가치"] as number,
        maxNotional: bracket["최대 명목 가치"] as number,
        maxLeverage: bracket["최대 레버리지"] as number,
        maintMarginRatio: bracket["유지 마진율"] as number,
        maintAmount: bracket["유지 금액"] as number
    }), []);

    const convertFundingRates = useMemo(() => (fundingRates: Record<string, unknown>): FundingRatesRaw => ({
        dataPath: fundingRates["데이터 경로"] as string,
        period: {
            start: (fundingRates["데이터 기간"] as Record<string, string>)["시작"],
            end: (fundingRates["데이터 기간"] as Record<string, string>)["종료"]
        },
        totalCount: fundingRates["합계 펀딩 횟수"] as number,
        positiveCount: fundingRates["양수 펀딩 횟수"] as number,
        negativeCount: fundingRates["음수 펀딩 횟수"] as number,
        averageFundingRate: fundingRates["평균 펀딩 비율"] as number,
        maxFundingRate: fundingRates["최고 펀딩 비율"] as number,
        minFundingRate: fundingRates["최저 펀딩 비율"] as number
    }), []);

    // 렌더링 최적화를 위한 MutationObserver 설정
    useEffect(() => {
        // 부모 탭 상태 변경 감지
        const configTab = document.querySelector('.tab-content[data-tab="Config"]');
        if (!configTab) return;

        const observer = new MutationObserver((mutations) => {
            for (const mutation of mutations) {
                if (mutation.attributeName === 'style' || mutation.attributeName === 'class') {
                    const style = (mutation.target as HTMLElement).style;
                    const isNowVisible = style.visibility !== 'hidden' && style.display !== 'none';

                    if (isVisible.current !== isNowVisible) {
                        isVisible.current = isNowVisible;
                    }
                }
            }
        });

        observer.observe(configTab, {
            attributes: true,
            attributeFilter: ['style', 'class']
        });

        return () => {
            observer.disconnect();
        };
    }, []);

    if (!parsedConfig) {
        return isLoading ? <LoadingSpinner/> : null;
    }

    if (!showContent) {
        return <LoadingSpinner/>;
    }

    return (
        <div
            className="flex flex-col h-full overflow-y-auto gpu-accelerated-heavy"
            style={{overflowY: 'auto'}} /* 명시적으로 스크롤 설정 */
        >
            <PageTitle/>

            <div
                className="px-8 pb-8 space-y-6 gpu-accelerated-heavy"
            >
                {/* 심볼 정보 */}
                <section
                    className="gpu-accelerated-heavy"
                >
                    <div className="space-y-4">
                        <div
                            key="symbol-card"
                            className="gpu-accelerated-heavy"
                        >
                            <SymbolCard
                                symbols={parsedConfig.symbols.map(s => ({
                                    symbol: s.symbolName,
                                    exchangeInfo: s.exchangeInfo ? convertExchangeInfo(s.exchangeInfo) : undefined,
                                    leverageBrackets: s.leverageBrackets ? {
                                        dataPath: (s.leverageBrackets as any)["데이터 경로"],
                                        brackets: ((s.leverageBrackets as any)["구간"] as Record<string, unknown>[]).map(convertLeverageBracket)
                                    } : undefined,
                                    fundingRates: s.fundingRates ? convertFundingRates(s.fundingRates) : undefined,
                                    trading: convertBar(s.tradingBarData),
                                    magnifier: s.magnifierBarData ? convertBar(s.magnifierBarData) : undefined,
                                    reference: s.referenceBarData?.map(convertBar),
                                    mark: convertBar(s.markPriceBarData),
                                }))}
                                initialSymbol={parsedConfig.symbols.length > 0 ? parsedConfig.symbols[0].symbolName : undefined}
                            />
                        </div>
                    </div>
                </section>

                {/* 전략 및 지표 정보 - 전체 너비 사용 */}
                <section
                    className="gpu-accelerated-heavy"
                >
                    <div className="space-y-4">
                        {!isLoading && parsedConfig?.strategies.map((s, idx) => (
                            <StrategyIndicatorCard
                                key={idx}
                                name={s.name}
                                className={s.className}
                                sourcePath={s.sourcePath}
                                headerPath={s.headerPath}
                                indicators={s.indicators}
                            />
                        ))}
                        {parsedConfig.strategies.length === 0 && (
                            <p className="text-muted-foreground italic">
                                추가된 전략이 없습니다.
                            </p>
                        )}
                    </div>
                </section>

                {/* 엔진 설정 - 맨 아래로 이동 */}
                <section
                    className="gpu-accelerated-heavy"
                >
                    <div
                        key="engine-card"
                        className="gpu-accelerated-heavy"
                    >
                        <EngineCard settings={parsedConfig.settings}/>
                    </div>
                </section>
            </div>
        </div>
    )
});

export default Config;

