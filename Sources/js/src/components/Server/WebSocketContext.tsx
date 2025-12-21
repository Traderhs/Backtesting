import React, {createContext, useContext, useState, useEffect} from 'react';

interface WebSocketProviderProps {
    children: React.ReactNode;
}

interface WebSocketContextValue {
    ws: WebSocket | null;
    serverError: boolean;
}

const WebSocketContext = createContext<WebSocketContextValue>({
    ws: null,
    serverError: false,
});

export const useWebSocket = () => useContext(WebSocketContext);

export const WebSocketProvider: React.FC<WebSocketProviderProps> = ({children}) => {
    const [ws, setWs] = useState<WebSocket | null>(null);
    const [serverError, setServerError] = useState<boolean>(false);

    useEffect(() => {
        const wsProtocol = window.location.protocol === "https:" ? "wss" : "ws";
        // 개발 환경에서는 Vite가 프론트엔드를 서빙하므로 백엔드는 별도 포트에서 동작
        // Vite용 환경변수 VITE_BACKBOARD_PORT가 설정되어 있으면 사용하고, 없으면 기본 포트 7777을 사용
        const isDev = !!(import.meta && (import.meta as any).env && (import.meta as any).env.DEV);
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

            return () => {
                socket.close();
            };
        } catch (e) {
            console.error('[WebSocket] 소켓 생성 실패:', e);
            setServerError(true);
        }
    }, []);

    return (
        <WebSocketContext.Provider value={{ws, serverError}}>
            {children}
        </WebSocketContext.Provider>
    );
};
