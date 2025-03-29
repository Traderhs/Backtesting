import React, { createContext, useEffect, useState } from "react"

export type GlobalFilter = {
    strategies: string[]
    symbols: string[]
}

export type Trade = {
    date: string
    symbol: string
    strategy: string
    // 다른 필드도 필요 시 추가 가능
}

const GlobalFilterContext = createContext<{
    filter: GlobalFilter
    setFilter: (filter: GlobalFilter) => void
    allTrades: Trade[]
    filteredTrades: Trade[]
    isFilterActive: boolean
    loading: boolean
}>({
    filter: { strategies: [], symbols: [] },
    setFilter: () => {},
    allTrades: [],
    filteredTrades: [],
    isFilterActive: false,
    loading: true,
})

export const GlobalFilterProvider = ({ children }: { children: React.ReactNode }) => {
    const [filter, setFilter] = useState<GlobalFilter>({ strategies: [], symbols: [] })
    const [allTrades, setAllTrades] = useState<Trade[]>([])
    const [filteredTrades, setFilteredTrades] = useState<Trade[]>([])
    const [loading, setLoading] = useState(true)

    useEffect(() => {
        const fetchTrades = async () => {
            try {
                const res = await fetch("/Backboard/trade_list.json")
                const data = await res.json()
                setAllTrades(data)
            } catch (err) {
                console.error("거래 내역 로딩 실패:", err)
                setAllTrades([])
            } finally {
                setLoading(false)
            }
        }

        void fetchTrades()
    }, [])

    useEffect(() => {
        const filtered = allTrades.filter((trade) => {
            const strategyMatch =
                filter.strategies.length === 0 || filter.strategies.includes(trade.strategy)
            const symbolMatch =
                filter.symbols.length === 0 || filter.symbols.includes(trade.symbol)
            return strategyMatch && symbolMatch
        })

        setFilteredTrades(filtered)
    }, [filter, allTrades])

    const isFilterActive = filter.strategies.length > 0 || filter.symbols.length > 0

    return (
        <GlobalFilterContext.Provider
            value={{ filter, setFilter, allTrades, filteredTrades, isFilterActive, loading }}
        >
            {children}
        </GlobalFilterContext.Provider>
    )
}

export { GlobalFilterContext }
