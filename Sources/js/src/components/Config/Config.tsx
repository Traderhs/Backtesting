import { useState, useEffect } from "react"
import SymbolCard from "./SymbolCard"
import StrategyCard from "./StrategyCard"

interface BarDataRaw {
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

interface StrategyRaw {
    name: string
    indicators: {
        name: string
        timeframe: string
    }[]
}

interface SymbolRaw {
    symbolName: string
    tradingBarData: Record<string, unknown>
    zoomedBarData?: Record<string, unknown>[]
    referenceBarData: Record<string, unknown>[]
    markPriceBarData: Record<string, unknown>
}

interface ConfigJson {
    symbols: SymbolRaw[]
    strategies: StrategyRaw[]
    settings: Record<string, unknown>
}

interface ConfigProps {
    tab: "ConfigSymbols" | "ConfigStrategies" | "ConfigSettings"
}

export default function Config({ tab }: ConfigProps) {
    const [config, setConfig] = useState<ConfigJson | null>(null)

    useEffect(() => {
        fetch("/Backboard/config.json")
            .then((res) => res.json())
            .then((data) => {
                const symbols = (data["심볼"] as Record<string, unknown>[]).map((s): SymbolRaw => {
                    const symbol = s as Record<string, unknown>
                    return {
                        symbolName: symbol["심볼명"] as string,
                        tradingBarData: symbol["트레이딩 바 데이터"] as Record<string, unknown>,
                        zoomedBarData: symbol["돋보기 바 데이터"] as Record<string, unknown>[] | undefined,
                        referenceBarData: symbol["참조 바 데이터"] as Record<string, unknown>[],
                        markPriceBarData: symbol["마크 가격 바 데이터"] as Record<string, unknown>
                    }
                })

                const strategies = (data["전략"] as Record<string, unknown>[]).map((s): StrategyRaw => {
                    const strategy = s as Record<string, unknown>
                    const filteredIndicators = (strategy["지표"] as Record<string, string>[])
                        .filter(i =>
                            !["open", "high", "low", "close", "volume"].includes(
                                i["지표명"].toLowerCase()
                            )
                        )
                        .map(i => ({
                            name: i["지표명"],
                            timeframe: i["타임프레임"]
                        }))
                    return {
                        name: strategy["전략명"] as string,
                        indicators: filteredIndicators
                    }
                })

                const settings = data["설정"] as Record<string, unknown>

                setConfig({ symbols, strategies, settings })
            })
    }, [])

    // 바 데이터 변환 함수
    const convertBar = (bar: Record<string, unknown>): BarDataRaw => ({
        period: {
            start: (bar["기간"] as Record<string, string>)["시작"],
            end: (bar["기간"] as Record<string, string>)["끝"]
        },
        timeframe: bar["타임프레임"] as string,
        count: bar["바 개수"] as number,
        missing: {
            count: (bar["누락된 바"] as Record<string, unknown>)["개수"] as number,
            times: (bar["누락된 바"] as Record<string, unknown>)["시간"] as string[]
        },
        path: bar["데이터 경로"] as string
    })

    // 설정 렌더링 함수
    const renderSetting = (key: string, value: unknown) => {
        if (typeof value === "object" && value !== null && !Array.isArray(value)) {
            return (
                <div className="mb-4" key={key}>
                    <h3 className="font-semibold text-base mb-1">{key}</h3>
                    <ul className="list-disc list-inside pl-4 text-sm text-muted-foreground">
                        {Object.entries(value as Record<string, unknown>).map(([k, v]) => (
                            <li key={k}>
                                <span className="font-medium text-foreground">{k}</span>: {String(v)}
                            </li>
                        ))}
                    </ul>
                </div>
            )
        }
        return (
            <div className="text-sm text-muted-foreground" key={key}>
                <span className="font-medium text-foreground">{key}:</span> {String(value)}
            </div>
        )
    }

    // 탭에 따라 다른 콘텐츠 렌더링
    let content
    if (tab === "ConfigSymbols") {
        // 심볼
        content = config?.symbols.map((s, idx) => (
            <SymbolCard
                key={idx}
                symbol={s.symbolName}
                trading={convertBar(s.tradingBarData)}
                zoomed={s.zoomedBarData?.map(convertBar)}
                reference={s.referenceBarData?.map(convertBar)}
                mark={convertBar(s.markPriceBarData)}
            />
        ))
    } else if (tab === "ConfigStrategies") {
        // 전략
        content = (
            <>
                {config?.strategies.map((s, idx) => (
                    <StrategyCard key={idx} name={s.name} indicators={s.indicators} />
                ))}
                {config?.strategies.every(s => s.indicators.length === 0) && (
                    <p className="text-muted-foreground italic">추가된 지표가 없습니다.</p>
                )}
            </>
        )
    } else if (tab === "ConfigSettings") {
        // 설정
        content = (
            <div className="space-y-3">
                {config &&
                    Object.entries(config.settings).map(([key, value]) =>
                        renderSetting(key, value)
                    )}
            </div>
        )
    } else {
        // 혹시 모를 예외 처리
        content = <p className="text-sm">선택된 탭에 해당하는 콘텐츠가 없습니다.</p>
    }

    return (
        <div className="flex flex-col h-full overflow-hidden">
            <div className="flex-none mb-4">
                <h1 className="text-2xl font-bold">⚙️ 백테스팅 설정</h1>
            </div>
            <div className="flex-1 overflow-auto">
                {content}
            </div>
        </div>
    )
}
