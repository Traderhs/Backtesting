import {useState} from "react"
import {Card, CardHeader, CardTitle, CardContent} from "@/components/ui/card.tsx"
import {Button} from "@/components/ui/button.tsx"

interface StrategyCardProps {
    name: string
    indicators: {
        name: string
        timeframe: string
    }[]
}

export default function StrategyCard({name, indicators}: StrategyCardProps) {
    const [expanded, setExpanded] = useState(false)
    const [source, setSource] = useState<string | null | undefined>(undefined)

    const handleToggle = async () => {
        setExpanded(prev => !prev)
        if (source !== undefined) return
        try {
            const res = await fetch(`/Backboard/sources/${name}.cpp`)
            const text = await res.text()

            // HTML fallback 방지
            const isHtml = text.trimStart().startsWith("<!DOCTYPE html") || text.trimStart().startsWith("<html")
            if (!res.ok || isHtml) {
                setSource(null)
                return
            }

            setSource(text)
        } catch {
            setSource(null)
        }
    }

    return (
        <Card>
            <CardHeader>
                <CardTitle className="text-xl">🧠 {name}</CardTitle>
            </CardHeader>
            <CardContent className="space-y-2 text-sm">
                <div>
                    <span className="font-medium">지표</span>
                    <div className="mt-1 ml-2 flex flex-col gap-1">
                        {indicators.map((i, idx) => (
                            <span key={idx}>{i.name} {i.timeframe.replace(/[()]/g, '')}</span>
                        ))}
                    </div>
                </div>
                <Button variant="outline" size="sm" onClick={handleToggle}>
                    {expanded ? "코드 숨기기" : "코드 보기"}
                </Button>
                {expanded && (
                    <div className="mt-2 p-3 bg-black text-white text-xs rounded whitespace-pre-wrap">
                        {source === undefined && "로딩 중..."}
                        {source === null && "소스 코드 없음"}
                        {source && source}
                    </div>
                )}
            </CardContent>
        </Card>
    )
}
