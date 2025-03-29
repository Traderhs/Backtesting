import { useState, useEffect } from "react"

interface TradeItem {
    "거래 번호": number
    [key: string]: string | number
}

const hoverFields = [
    "진입 수수료",
    "청산 수수료",
    "강제 청산 수수료",
    "손익",
    "순손익",
    "개별 손익률",
    "전체 손익률",
    "현재 자금",
    "최고 자금",
    "드로우다운",
    "최고 드로우다운",
    "누적 손익",
    "누적 손익률",
]

const dollarFields = [
    "진입 수수료",
    "청산 수수료",
    "강제 청산 수수료",
    "손익",
    "순손익",
    "현재 자금",
    "최고 자금",
    "누적 손익",
]

const percentFields = [
    "개별 손익률",
    "전체 손익률",
    "드로우다운",
    "최고 드로우다운",
    "누적 손익률",
]

const baseColumns = new Set([
    "거래 번호",
    "심볼 이름",
    "진입 이름",
    "청산 이름",
    "진입 방향",
    "보유 시간",
    "진입 가격",
    "진입 수량",
    "청산 가격",
    "청산 수량",
    "순손익",
    "개별 손익률",
    "전체 손익률",
    "현재 자금",
    "최고 자금",
    "드로우다운",
    "최고 드로우다운",
])

function formatWithTooltip(value: string | number, key: string) {
    const num = typeof value === "number" ? value : parseFloat(String(value))
    if (isNaN(num)) return String(value)

    const short = num.toFixed(2)
    const roundedFull = parseFloat(Number(num.toPrecision(15)).toFixed(10)).toString()

    let display = short
    if (dollarFields.includes(key)) {
        display = num < 0 ? `-$${Math.abs(Number(short)).toFixed(2)}` : `$${short}`
    } else if (percentFields.includes(key)) {
        display = `${short}%`
    }

    let tooltip = roundedFull
    if (dollarFields.includes(key)) {
        tooltip = num < 0 ? `-$${Math.abs(Number(roundedFull))}` : `$${roundedFull}`
    } else if (percentFields.includes(key)) {
        tooltip = `${roundedFull}%`
    }

    return display !== tooltip ? <span title={tooltip}>{display}</span> : display
}

export default function TradeList() {
    const [trades, setTrades] = useState<TradeItem[]>([])
    const [selectedColumns, setSelectedColumns] = useState<Set<string>>(new Set(baseColumns))
    const [prevSelectedColumns, setPrevSelectedColumns] = useState<Set<string> | null>(null)

    useEffect(() => {
        fetch("/Backboard/trade_list.json")
            .then((res) => res.json())
            .then((data) => {
                const sorted = data.sort(
                    (a: TradeItem, b: TradeItem) => a["거래 번호"] - b["거래 번호"]
                )
                setTrades(sorted)
            })
    }, [])

    const allHeaders = trades.length > 0 ? Object.keys(trades[0]) : Array.from(baseColumns)
    const displayedHeaders = allHeaders.filter((h) => selectedColumns.has(h))

    const handleToggleColumn = (column: string) => {
        setSelectedColumns((prev) => {
            const newSet = new Set(prev)
            if (newSet.has(column)) {
                newSet.delete(column)
            } else {
                newSet.add(column)
            }
            return newSet
        })
    }

    const handleToggleAll = () => {
        if (selectedColumns.size !== allHeaders.length) {
            setPrevSelectedColumns(new Set(selectedColumns))
            setSelectedColumns(new Set(allHeaders))
        } else {
            if (prevSelectedColumns) {
                setSelectedColumns(new Set(prevSelectedColumns))
                setPrevSelectedColumns(null)
            } else {
                setSelectedColumns(new Set(baseColumns))
            }
        }
    }

    return (
        <div className="flex flex-col h-full w-full overflow-hidden">
            {/* 헤더 */}
            <div className="flex-none mb-4">
                <h1 className="text-2xl font-bold">
                    📋 거래 내역 ({trades.length}건)
                </h1>
            </div>

            {/* 본문 */}
            <div className="flex-1 flex flex-col min-h-0">
                {/* 체크박스 영역 - 고정 */}
                <div className="flex flex-wrap items-center gap-3 mb-4 shrink-0">
                    <button
                        className="text-sm text-blue-600 underline"
                        onClick={handleToggleAll}
                    >
                        {selectedColumns.size !== allHeaders.length ? "전체 선택" : "전체 선택 해제"}
                    </button>
                    {allHeaders.map((header) => (
                        <label key={header} className="text-sm flex items-center space-x-1">
                            <input
                                type="checkbox"
                                checked={selectedColumns.has(header)}
                                onChange={() => handleToggleColumn(header)}
                            />
                            <span>{header}</span>
                        </label>
                    ))}
                </div>

                {/* 테이블 영역 - 스크롤 가능 */}
                <div className="flex-1 min-h-0 overflow-auto">
                    {trades.length === 0 ? (
                        <p className="text-muted-foreground italic text-sm">
                            거래 내역이 없습니다.
                        </p>
                    ) : (
                        <table className="table-auto border-collapse w-full shadow">
                            <thead className="sticky top-0 z-10 bg-gray-200 shadow-md">
                            <tr>
                                {displayedHeaders.map((header) => (
                                    <th
                                        key={header}
                                        className="border px-1 py-0.5 bg-gray-300 text-center whitespace-nowrap"
                                        title={header}
                                    >
                                        {header}
                                    </th>
                                ))}
                            </tr>
                            </thead>
                            <tbody>
                            {trades.map((row, idx) => (
                                <tr key={idx} className="border-t">
                                    {displayedHeaders.map((key) => (
                                        <td
                                            key={key}
                                            className="border px-1 py-0.5 whitespace-nowrap text-right"
                                        >
                                            {hoverFields.includes(key)
                                                ? formatWithTooltip(row[key], key)
                                                : row[key]}
                                        </td>
                                    ))}
                                </tr>
                            ))}
                            </tbody>
                        </table>
                    )}
                </div>
            </div>
        </div>
    )
}

