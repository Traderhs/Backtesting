import React, {createContext, useContext, useEffect, useState} from 'react';

interface WebSocketProviderProps {
    children: React.ReactNode;
}

interface WebSocketContextValue {
    ws: WebSocket | null;
    serverError: boolean;
    projectDirectoryRequested: boolean;  // 서버가 프로젝트 폴더 입력을 요청한 상태인지 여부
    clearProjectDirectoryRequest: () => void;  // 요청 플래그를 수동으로 해제
}

const WebSocketContext = createContext<WebSocketContextValue>({
    ws: null,
    serverError: false,
    projectDirectoryRequested: false,
    clearProjectDirectoryRequest: () => {
    }
});

export const useWebSocket = () => useContext(WebSocketContext);

export const WebSocketProvider: React.FC<WebSocketProviderProps> = ({children}) => {
    const [ws, setWs] = useState<WebSocket | null>(null);
    const [serverError, setServerError] = useState<boolean>(false);
    const [projectDirectoryRequested, setProjectDirectoryRequested] = useState<boolean>(false);

    useEffect(() => {
        const wsProtocol = window.location.protocol === "https:" ? "wss" : "ws";
        // 개발 환경에서는 Vite가 프론트를 서빙하므로 백엔드는 별도 포트에서 동작
        // Vite용 환경변수 VITE_BACKBOARD_PORT가 설정되어 있으면 사용하고, 없으면 기본 포트 7777을 사용
        const isDev = window?.electronAPI?.isDev;
        const devPort = isDev ? ((import.meta as any).env.VITE_BACKBOARD_PORT || '7777') : null;
        const host = devPort ? `${window.location.hostname}:${devPort}` : window.location.host;
        const socketUrl = `${wsProtocol}://${host}`;

        try {
            const socket = new WebSocket(socketUrl);
            setWs(socket);

            socket.onopen = () => {
                setServerError(false);
            };

            socket.onclose = () => {
                setServerError(true);
            };

            socket.onerror = (error) => {
                console.error("WebSocket 에러 발생:", error);
                setServerError(true);
            };

            // 전역 메시지 수신 (전략 에디터가 아직 마운트 되기 전에 도착할 수 있는 요청을 보관)
            const handleMessage = async (event: MessageEvent) => {
                try {
                    const data = JSON.parse(event.data);
                    if (data && data.action === 'requestProjectDirectory') {
                        // Electron 환경에서는 자동으로 프로젝트 디렉토리 전송
                        if ((window as any).electronAPI?.isElectron) {
                            try {
                                const projectDir = await (window as any).electronAPI.getProjectDirectory();
                                if (socket && socket.readyState === WebSocket.OPEN) {
                                    socket.send(JSON.stringify({
                                        action: 'setProjectDirectory',
                                        directory: projectDir
                                    }));
                                }
                            } catch (e) {
                                // Electron API 실패 시 수동 선택으로 폴더 요청
                                setProjectDirectoryRequested(true);
                            }
                        } else {
                            // 서버가 프로젝트 폴더를 요청했음을 플래그로 표시
                            setProjectDirectoryRequested(true);
                        }
                    }
                } catch (e) {
                    // 무시
                }
            };

            socket.addEventListener('message', handleMessage);

            return () => {
                try {
                    socket.removeEventListener('message', handleMessage);
                } catch (e) {
                    // 무시
                }
                socket.close();
            };
        } catch (e) {
            console.error('[WebSocket] 소켓 생성 실패:', e);
            setServerError(true);
        }
    }, []);

    const clearProjectDirectoryRequest = () => setProjectDirectoryRequested(false);

    return (
        <WebSocketContext.Provider value={{ws, serverError, projectDirectoryRequested, clearProjectDirectoryRequest}}>
            {children}
        </WebSocketContext.Provider>
    );
};
