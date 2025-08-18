import React, { createContext, useContext, useState, useEffect } from 'react';

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

export const WebSocketProvider: React.FC<WebSocketProviderProps> = ({ children }) => {
    const [ws, setWs] = useState<WebSocket | null>(null);
    const [serverError, setServerError] = useState<boolean>(false);

    useEffect(() => {
        const wsProtocol = window.location.protocol === "https:" ? "wss" : "ws";
        const socket = new WebSocket(`${wsProtocol}://${window.location.host}`);
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
    }, []);

    return (
        <WebSocketContext.Provider value={{ ws, serverError }}>
            {children}
        </WebSocketContext.Provider>
    );
};
