import { useEffect, useState } from "react"
import { useGlobalFilter } from "@/hooks/useGlobalFilter"
import { Card } from "@/components/ui/card"
import { Label } from "@/components/ui/label"
import { Checkbox } from "@/components/ui/checkbox"

type Item = { name: string }

export default function GlobalFilter() {
    const { filter, setFilter } = useGlobalFilter()
    const [symbols, setSymbols] = useState<Item[]>([])
    const [strategies, setStrategies] = useState<Item[]>([])

    useEffect(() => {
        const loadConfig = async () => {
            try {
                const res = await fetch("/Backboard/config.json")
                const json = await res.json()

                const symbolItems = (json["심볼"] ?? []).map((s: Record<string, unknown>) => ({
                    name: String(s["심볼명"]),
                }))
                const strategyItems = (json["전략"] ?? []).map((s: Record<string, unknown>) => ({
                    name: String(s["전략명"]),
                }))

                setSymbols(symbolItems)
                setStrategies(strategyItems)

                // ✅ 필터 초기화 (전부 체크된 상태)
                setFilter({
                    symbols: symbolItems.map((s: Item) => s.name),
                    strategies: strategyItems.map((s: Item) => s.name),
                })
            } catch (err) {
                console.error("Config.json 로딩 실패:", err)
            }
        }

        void loadConfig()
    // eslint-disable-next-line react-hooks/exhaustive-deps
    }, [])

    const toggle = (
        key: "symbols" | "strategies",
        value: string,
        checked: boolean
    ) => {
        const list = filter[key] || []
        const updated = checked
            ? [...list, value]
            : list.filter((v) => v !== value)
        setFilter({ ...filter, [key]: updated })
    }

    return (
        <Card className="p-4 space-y-6 rounded-2xl shadow-md">
            <h2 className="text-xl font-bold">🌐 전역 필터</h2>

            <Section title="📈 전략" items={strategies} type="strategies" />
            <Section title="💱 심볼" items={symbols} type="symbols" />
        </Card>
    )

    function Section({
                         title,
                         items,
                         type,
                     }: {
        title: string
        items: Item[]
        type: "symbols" | "strategies"
    }) {
        return (
            <div>
                <Label className="font-semibold mb-2 block">{title}</Label>
                <div className="space-y-2">
                    {items.map(({ name }) => (
                        <div key={name} className="flex items-center gap-2">
                            <Checkbox
                                id={`${type}-${name}`}
                                checked={filter[type]?.includes(name)}
                                onCheckedChange={(checked) =>
                                    toggle(type, name, Boolean(checked))
                                }
                            />
                            <Label htmlFor={`${type}-${name}`}>{name}</Label>
                        </div>
                    ))}
                </div>
            </div>
        )
    }
}
