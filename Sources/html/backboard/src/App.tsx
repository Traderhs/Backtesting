import {useState, useEffect} from "react"
import {GlobalFilterProvider} from "@/lib/GlobalFilterContext";
import GlobalFilter from "@/components/Sidebar/GlobalFilter"
import Sidebar from "@/components/Sidebar/Sidebar"
import Chart from "@/components/Chart/Chart"
import TradeList from "@/components/TradeList/TradeList"
import Config from "@/components/Config/Config"
import Log from "@/components/Log/Log"

function App() {
    useEffect(() => {
        const interval = setInterval(() => {
            fetch("/ping").catch(() => {
            })
        }, 1000)
        return () => clearInterval(interval)
    }, [])

    const [tab, setTab] = useState("Overview")
    const [isSidebarOpen, setSidebarOpen] = useState(true)

    return (
        <GlobalFilterProvider>
            <div className="flex h-screen overflow-hidden">
                <Sidebar
                    onSelectTab={setTab}
                    isOpen={isSidebarOpen}
                    onToggle={() => setSidebarOpen(prev => !prev)}
                    activeTab={tab}
                />

                <main className="flex-1 h-full overflow-hidden">
                    {tab === "Overview" && (
                        <h1 className="text-2xl font-bold">📊 대시보드 메인 콘텐츠</h1>
                    )}

                    {/* 차트 폴더나 파일 미존재 시 오류 진입 */}
                    {tab === "Chart" && <Chart/>}
                    {/* 특정 심볼의 차트 세부탭 */}
                    {tab.startsWith("Chart:") && (
                        <Chart filename={tab.split("Chart:")[1]}/>
                    )}

                    {tab === "TradeList" && <TradeList/>}

                    {tab === "GlobalFilter" && <GlobalFilter/>}

                    {(tab === "ConfigSymbols" || tab === "ConfigStrategies" || tab === "ConfigSettings") && (
                        <Config tab={tab}/>
                    )}

                    {tab === "Log" && <Log/>}
                </main>
            </div>
        </GlobalFilterProvider>
    )
}

export default App
