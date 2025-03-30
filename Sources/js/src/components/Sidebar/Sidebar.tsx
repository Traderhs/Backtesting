import {useEffect, useState} from "react"
import 'react-datepicker/dist/react-datepicker.css'
import {Button} from "../ui/button.tsx"
import {
    RecalculateBalanceCheckbox,
    TradeNumberFilter,
    StrategyFilter,
    SymbolFilter,
    EntryNameFilter,
    ExitNameFilter,
    EntryDirectionFilter,
    EntryTimeFilter,
    ExitTimeFilter,
    AdvancedEntryTimeFilter,
    AdvancedExitTimeFilter,
    HoldingTimeFilter,
    NumericFilters
} from "../TradeFilter";

interface SidebarProps {
    onSelectTab: (tab: string) => void
    isOpen: boolean
    onToggle: () => void
    activeTab: string
}

export default function Sidebar({
                                    onSelectTab,
                                    isOpen,
                                    onToggle,
                                    activeTab,
                                }: SidebarProps) {
    // 차트 확장 상태
    const [configExpanded, setConfigExpanded] = useState(
        activeTab === "Config" || activeTab.startsWith("Config")
    )

    const [chartFiles, setChartFiles] = useState<string[]>([])
    const [chartError, setChartError] = useState<string | null>(null)
    const handleConfigClick = () => setConfigExpanded(prev => !prev)
    const handleChartClick = () => {
        if (chartError) {
            onSelectTab('Chart')
            return
        }
        setChartExpanded(prev => !prev)
    }

    // 필터 확장 상태
    const [filterExpanded, setFilterExpanded] = useState(false)

    // 백테스팅 설정 확장 상태
    const [chartExpanded, setChartExpanded] = useState(
        activeTab === "Chart" || activeTab.startsWith("Chart:")
    )

    useEffect(() => {
        const fetchChartFiles = async () => {
            const res = await fetch("/chart-files")
            if (!res.ok) {
                setChartError("차트 폴더가 존재하지 않습니다.")
                return
            }

            const files: string[] = await res.json()
            if (!files || files.length === 0) {
                setChartError("차트 파일이 존재하지 않습니다.")
                return
            }
            setChartFiles(files.map(f => f.replace(".html", "")))
        }
        void fetchChartFiles()
    }, [])

    if (!isOpen) {
        return (
            <div className="absolute top-4 left-4 z-50">
                <Button variant="outline" size="sm" onClick={onToggle}>
                    ▶︎
                </Button>
            </div>
        )
    }

    return (
        <div
            className="h-screen bg-gray-900 p-4 flex flex-col border-r transition-all duration-300 w-64 overflow-y-auto">
            <div className="flex items-center justify-between mb-4">
                <h2 className="text-lg font-bold text-white">📊 대시보드</h2>
                <Button variant="outline" size="sm" onClick={onToggle}>
                    ◀︎
                </Button>
            </div>

            <Button variant="ghost" className="w-full justify-start" onClick={() => onSelectTab("Overview")}>
                🧠 <span className="ml-2">전체 요약</span>
            </Button>
            <Button variant="ghost" className="w-full justify-start" onClick={() => onSelectTab("Performance")}>
                📈 <span className="ml-2">성과 지표</span>
            </Button>

            {/* 분석 그래프 */}
            <Button variant="ghost" className="w-full justify-start" onClick={() => onSelectTab("Plot")}>
                📉 <span className="ml-2">분석 그래프</span>
            </Button>

            {/* 거래 차트 */}
            <Button variant="ghost" className="w-full justify-start" onClick={handleChartClick}>
                <span className="ml-2">거래 차트</span>
                {!chartError && (
                    <span className="ml-auto">{chartExpanded ? "▼" : "▶"}</span>
                )}
            </Button>

            {chartExpanded && chartFiles.length > 0 && (
                <div className="ml-6 flex flex-col">
                    {chartFiles.map((file) => (
                        <Button
                            key={file}
                            variant="ghost"
                            className={`w-full justify-start text-sm ${activeTab === `Chart:${file}` ? "text-primary font-bold" : ""}`}
                            onClick={() => onSelectTab(`Chart:${file}`)}
                        >
                            📄 {file}
                        </Button>
                    ))}
                </div>
            )}

            {/* 거래 내역 */}
            <Button variant="ghost" className="w-full justify-start" onClick={() => onSelectTab("TradeList")}>
                📋 <span className="ml-2">거래 내역</span>
            </Button>

            {/* 거래 필터 */}
            <Button
                // 기존 activeTab 비교는 그대로 둬도 되지만, onClick에서 탭 전환은 제거합니다.
                variant={activeTab === "TradeFilter" ? "default" : "ghost"}
                className="w-full justify-start"
                onClick={() => setFilterExpanded(prev => !prev)}
            >
                <div className="flex items-center justify-between w-full">
                    <span>🌐 거래 필터</span>
                    <span>{filterExpanded ? "▼" : "▶"}</span>
                </div>
            </Button>

            {filterExpanded && (
                <div className="ml-6 mt-2 flex flex-col space-y-2">
                    {/* 자금 재계산 체크박스 */}
                    <RecalculateBalanceCheckbox/>

                    {/* 거래 번호 필터 */}
                    <TradeNumberFilter/>

                    {/* 전략 필터 */}
                    <StrategyFilter/>

                    {/* 심볼 필터 */}
                    <SymbolFilter/>

                    {/* 진입 이름 */}
                    <EntryNameFilter/>

                    {/* 청산 이름 */}
                    <ExitNameFilter/>

                    {/* 진입 방향 */}
                    <EntryDirectionFilter/>

                    {/* 진입 시간 필터 */}
                    <EntryTimeFilter/>

                    {/* 진입 시간 고급 필터 */}
                    <AdvancedEntryTimeFilter/>

                    {/* 청산 시간 필터 */}
                    <ExitTimeFilter/>

                    {/* 청산 시간 고급 필터 */}
                    <AdvancedExitTimeFilter/>

                    {/* 보유 시간 필터 */}
                    <HoldingTimeFilter/>

                    {/* 그 외 숫자 이상 이하 필터들 */}
                    <NumericFilters/>
                </div>
            )}

            {/* 백테스팅 설정 */}
            <Button variant="ghost" className="w-full justify-start" onClick={handleConfigClick}>
                <span className="ml-2">백테스팅 설정</span>
                <span className="ml-auto">{configExpanded ? "▼" : "▶"}</span>
            </Button>
            {configExpanded && (
                <div className="ml-6 flex flex-col">
                    <Button variant="ghost" className="w-full justify-start"
                            onClick={() => onSelectTab("ConfigSymbols")}>
                        🪙 <span className="ml-2">심볼 데이터</span>
                    </Button>
                    <Button variant="ghost" className="w-full justify-start"
                            onClick={() => onSelectTab("ConfigStrategies")}>
                        🧠 <span className="ml-2">전략 및 지표</span>
                    </Button>
                    <Button variant="ghost" className="w-full justify-start"
                            onClick={() => onSelectTab("ConfigSettings")}>
                        ⚙️ <span className="ml-2">엔진 설정</span>
                    </Button>
                </div>
            )}

            {/* 백테스팅 로그 */}
            <Button variant="ghost" className="w-full justify-start" onClick={() => onSelectTab("Log")}>
                <span className="ml-2">백테스팅 로그</span>
            </Button>
        </div>
    )
}