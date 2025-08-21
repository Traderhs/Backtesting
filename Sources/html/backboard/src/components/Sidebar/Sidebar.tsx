import {useState, useEffect} from "react"
import {Button} from "@/components/ui/button.tsx"

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
    const [configExpanded, setConfigExpanded] = useState(
        activeTab === "Config" || activeTab.startsWith("Config")
    )

    const [chartExpanded, setChartExpanded] = useState(
        activeTab === "Chart" || activeTab.startsWith("Chart:")
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
        <div className="h-screen bg-gray-900 p-4 flex flex-col border-r transition-all duration-300 w-64">
            <div className="flex items-center justify-between mb-4">
                <h2 className="text-lg font-bold text-white">📊 대시보드</h2>
                <Button variant="outline" size="sm" onClick={onToggle}>
                    ◀︎
                </Button>
            </div>

            {/* 기본 메뉴 */}
            <Button variant="ghost" className="w-full justify-start" onClick={() => onSelectTab("Overview")}>🧠 <span
                className="ml-2">전체 요약</span></Button>
            <Button variant="ghost" className="w-full justify-start" onClick={() => onSelectTab("Performance")}>📈 <span
                className="ml-2">성과 지표</span></Button>
            <Button variant="ghost" className="w-full justify-start" onClick={() => onSelectTab("Plot")}>📉 <span
                className="ml-2">분석 그래프</span></Button>

            {/* 💹 거래 차트 접기 */}
            <Button variant="ghost" className="w-full justify-start" onClick={handleChartClick}>
                💹 <span className="ml-2">거래 차트</span>
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
                            className={`w-full justify-start text-sm ${
                                activeTab === `Chart:${file}` ? "text-primary font-bold" : ""
                            }`}
                            onClick={() => onSelectTab(`Chart:${file}`)}
                        >
                            📄 {file}
                        </Button>
                    ))}
                </div>
            )}

            <Button variant="ghost" className="w-full justify-start" onClick={() => onSelectTab("TradeList")}>
                📋 <span className="ml-2">거래 내역</span>
            </Button>

            <Button
                variant={activeTab === "GlobalFilter" ? "default" : "ghost"}
                className="w-full justify-start"
                onClick={() => onSelectTab("GlobalFilter")}
            >
                🌐 거래 필터
            </Button>

            {/* ⚙️ 백테스팅 설정 접기 */}
            <Button variant="ghost" className="w-full justify-start" onClick={handleConfigClick}>
                ⚙️ <span className="ml-2">백테스팅 설정</span>
                <span className="ml-auto">{configExpanded ? "▼" : "▶"}</span>
            </Button>
            {configExpanded && (
                <div className="ml-6 flex flex-col">
                    <Button variant="ghost" className="w-full justify-start"
                            onClick={() => onSelectTab("ConfigSymbols")}>🪙 <span className="ml-2">심볼 데이터</span></Button>
                    <Button variant="ghost" className="w-full justify-start"
                            onClick={() => onSelectTab("ConfigStrategies")}>🧠 <span
                        className="ml-2">전략 및 지표</span></Button>
                    <Button variant="ghost" className="w-full justify-start"
                            onClick={() => onSelectTab("ConfigSettings")}>⚙️ <span
                        className="ml-2">엔진 설정</span></Button>
                </div>
            )}

            <Button variant="ghost" className="w-full justify-start" onClick={() => onSelectTab("Log")}>
                🪵 <span className="ml-2">백테스팅 로그</span>
            </Button>
        </div>
    )
}
