import {useState, useEffect} from "react"
import ServerAlert from "@/components/ServerAlert"
import {TradeFilterProvider} from "@/components/TradeFilter/TradeFilterProvider.tsx"
import Sidebar from "@/components/Sidebar/Sidebar"
import Chart from "@/components/Chart/Chart"
import TradeList from "@/components/TradeList/TradeList"
import Config from "@/components/Config/Config"
import Log from "@/components/Log/Log"

function App() {
    const [serverError, setServerError] = useState(false)
    const [tab, setTab] = useState("Overview")
    const [isSidebarOpen, setSidebarOpen] = useState(true)

    useEffect(() => {
        // WebSocket 연결 생성
        const wsProtocol = window.location.protocol === "https:" ? "wss" : "ws";
        const ws = new WebSocket(`${wsProtocol}://${window.location.host}`);

        ws.onopen = () => {
            console.log("WebSocket 연결 성공");
            // 연결이 정상적으로 이루어지면 서버 에러 상태 초기화
            setServerError(false);
        };

        ws.onclose = () => {
            console.log("WebSocket 연결 종료");
            // 창을 닫은 경우엔 새 연결이 이루어지지 않으므로 서버 오류 알림 표시
            // (단, 새로고침 시에는 즉시 재연결되므로 onclose 이벤트가 발생해도 별도 처리가 필요없음)
            setServerError(true);
        };

        ws.onerror = (error) => {
            console.error("WebSocket 에러 발생:", error);
            setServerError(true);
        };

        // 컴포넌트 unmount 시 WebSocket 연결 정리
        return () => {
            ws.close();
        };
    }, []);

    return (
        <TradeFilterProvider>
            <div className="flex h-screen overflow-hidden">
                {/* 서버 오류 발생 시 경고 팝업 표시 */}
                {serverError && <ServerAlert serverError={serverError}/>}

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

                    {(tab === "ConfigSymbols" || tab === "ConfigStrategies" || tab === "ConfigSettings") && (
                        <Config tab={tab}/>
                    )}

                    {tab === "Log" && <Log/>}
                </main>
            </div>
        </TradeFilterProvider>
    )
}

export default App
