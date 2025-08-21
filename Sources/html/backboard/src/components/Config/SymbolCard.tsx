import { Card, CardHeader, CardTitle, CardContent } from "@/components/ui/card.tsx"
import { useState } from "react"

interface BarData {
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

interface SymbolCardProps {
    symbol: string
    trading: BarData
    zoomed?: BarData[]
    reference: BarData[]
    mark: BarData
}

export default function SymbolCard({ symbol, trading, zoomed, reference, mark }: SymbolCardProps) {
    const [expanded, setExpanded] = useState<Record<string, boolean>>({})

    const renderBar = (title: string, data?: BarData | BarData[]) => {
        if (!data || (Array.isArray(data) && data.length === 0)) return null
        const bars = Array.isArray(data) ? data : [data]

        return (
            <div className="flex flex-col border rounded p-3 space-y-1 bg-muted">
                <div className="font-semibold mb-1">{title}</div>
                {bars.map((bar, idx) => {
                    const key = `${title}-${idx}`
                    const isExpanded = expanded[key] || false

                    return (
                        <div key={idx} className="text-xs space-y-0.5">
                            <div><span className="font-medium">기간</span> — {bar.period.start} - {bar.period.end}</div>
                            <div><span className="font-medium">타임프레임</span> — {bar.timeframe}</div>
                            <div><span className="font-medium">바 개수</span> — {bar.count}</div>
                            <div>
                                <span className="font-medium">누락</span> — {bar.missing.count}개
                                {bar.missing.count > 0 && (
                                    <button
                                        onClick={() => setExpanded(prev => ({ ...prev, [key]: !isExpanded }))}
                                        className="ml-2 text-blue-600 underline"
                                    >
                                        {isExpanded ? "숨기기" : "펼치기"}
                                    </button>
                                )}
                            </div>
                            {isExpanded && bar.missing.times.length > 0 && (
                                <ul className="ml-4 list-disc">
                                    {bar.missing.times.map((t, i) => <li key={i}>{t}</li>)}
                                </ul>
                            )}
                            <div className="break-all"><span className="font-medium">경로</span> — {bar.path}</div>
                        </div>
                    )
                })}
            </div>
        )
    }

    const hasZoomed = zoomed && zoomed.length > 0
    const columnCount = hasZoomed ? 4 : 3

    return (
        <Card>
            <CardHeader>
                <CardTitle className="text-xl">{symbol}</CardTitle>
            </CardHeader>
            <CardContent>
                <div className={`grid grid-cols-1 lg:grid-cols-${columnCount} gap-4`}>
                    {renderBar("트레이딩 바 데이터", trading)}
                    {hasZoomed && renderBar("돋보기 바 데이터", zoomed)}
                    {renderBar("참조 바 데이터", reference)}
                    {renderBar("마크 가격 바 데이터", mark)}
                </div>
            </CardContent>
        </Card>
    )
}
