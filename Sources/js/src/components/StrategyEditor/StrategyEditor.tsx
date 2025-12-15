import { useState, useEffect, useRef } from 'react';
import { Button } from '../ui/button';
import { useWebSocket } from '../Server/WebSocketContext';

/**
 * 전략 에디터 컴포넌트
 * 백테스팅 전략을 편집하고 실행할 수 있는 UI 제공
 */
export default function StrategyEditor() {
    const [logs, setLogs] = useState<Array<{ level: string; message: string; timestamp: string | null; fileInfo: string | null }>>([]);
    const logContainerRef = useRef<HTMLDivElement>(null);
    const { ws } = useWebSocket();

    // 로그 추가 함수
    const addLog = (level: string, message: string, timestamp: string | null = null, fileInfo: string | null = null) => {
        // C++에서 timestamp가 제공되지 않으면 현재 시간 사용
        const finalTimestamp = timestamp || (() => {
            const d = new Date();
            const pad = (n: number) => String(n).padStart(2, '0');
            return `${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())} ${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())}`;
        })();
        setLogs(prev => [...prev, { level, message, timestamp: finalTimestamp, fileInfo }]);
    };

    // WebSocket 메시지 수신
    useEffect(() => {
        if (!ws) return;

        const handleMessage = (event: MessageEvent) => {
            try {
                const data = JSON.parse(event.data);
                if (data.action === 'backtestLog') {
                    addLog(data.level, data.message, data.timestamp, data.fileInfo);
                }
            } catch (err) {
                console.error('로그 메시지 파싱 오류:', err);
            }
        };

        ws.addEventListener('message', handleMessage);
        return () => ws.removeEventListener('message', handleMessage);
    }, [ws]);

    // 로그가 추가될 때마다 스크롤을 맨 아래로
    useEffect(() => {
        if (logContainerRef.current) {
            logContainerRef.current.scrollTop = logContainerRef.current.scrollHeight;
        }
    }, [logs]);

    // 백테스트 실행 핸들러
    const handleRunBacktest = () => {
        if (!ws || ws.readyState !== WebSocket.OPEN) {
            addLog('ERROR_L', 'WebSocket 연결이 없습니다.');
            return;
        }

        ws.send(JSON.stringify({ action: 'runBacktest' }));
    };

    // 로그 레벨에 따른 색상 매핑 (Log.tsx와 동일한 로직)
    const getLogColor = (level: string) => {
        switch (level) {
            case 'DEBUG':
                return 'rgb(156, 220, 254)'; // Light Blue
            case 'WARN':
                return 'rgb(229, 192, 123)'; // Yellow/Orange
            case 'ERROR':
                return 'rgb(224, 108, 117)'; // Red
            case 'BALANCE':
                return 'rgb(128, 128, 128)'; // Gray
            case 'INFO':
            default:
                return '#ffffff'; // White
        }
    };

    return (
        <div className="flex flex-col h-full w-full p-6 gap-4">
            {/* 상단 제어 영역 */}
            <div className="flex-shrink-0">
                <div className="flex items-center justify-between mb-4">
                    <h1 className="text-2xl font-bold text-white">전략 에디터</h1>
                    <Button
                        onClick={handleRunBacktest}
                        className="bg-blue-600 hover:bg-blue-700 text-white font-semibold px-6 py-2 rounded-lg transition-colors"
                    >
                        백테스트 실행
                    </Button>
                </div>
            </div>

            {/* 로그 영역 */}
            <div className="flex-1 flex flex-col min-h-0 bg-[#1a1a1a] rounded-lg border border-gray-700">
                <div className="px-4 py-2 border-b border-gray-700">
                    <h2 className="text-sm font-semibold text-gray-300">실행 로그</h2>
                </div>
                <div 
                    ref={logContainerRef}
                    className="flex-1 overflow-y-auto p-4 font-mono text-sm"
                    style={{ fontFamily: "'Inter', 'Pretendard', monospace" }}
                >
                    {logs.length === 0 ? (
                        <div className="text-gray-500 text-center mt-8">
                            백테스트를 실행하면 로그가 여기에 표시됩니다.
                        </div>
                    ) : (
                        logs.map((log, index) => (
                            log.level === 'SEPARATOR' ? (
                                <div key={index} className="my-2">
                                    {
                                        (() => {
                                            // Log.tsx와 동일한 계산 방식 복제
                                            // Align with container text start (Tailwind p-4 = 16px)
                                            const paddingLeft = 8;
                                            const paddingRight = 10;
                                            const charWidth = 8.4;
                                            const minWidth = 1500; // px

                                            const containerWidth = logContainerRef.current ? logContainerRef.current.clientWidth : 0;
                                            const desiredWidth = Math.max(minWidth, Math.max(containerWidth - paddingLeft - paddingRight, 0));
                                            const textCharCount = Math.ceil(desiredWidth / charWidth);
                                            const separatorLine = '─'.repeat(textCharCount);

                                            const wrapperWidth = desiredWidth + paddingLeft + paddingRight;

                                            return (
                                                <div
                                                    style={{
                                                        whiteSpace: 'pre',
                                                        fontFamily: "'Inter', 'Pretendard', monospace",
                                                        fontSize: '14px',
                                                        lineHeight: '1.4',
                                                        color: 'rgba(255, 215, 0, 0.4)',
                                                        padding: `0px ${paddingRight}px 0px ${paddingLeft}px`,
                                                        boxSizing: 'border-box',
                                                        display: 'flex',
                                                        alignItems: 'center',
                                                        width: `${wrapperWidth}px`,
                                                        minWidth: `${wrapperWidth}px`,
                                                        overflow: 'hidden',
                                                        fontSmooth: 'always',
                                                        WebkitFontFeatureSettings: '"liga" 1, "kern" 1, "calt" 1',
                                                        fontFeatureSettings: '"liga" 1, "kern" 1, "calt" 1',
                                                        textSizeAdjust: '100%',
                                                        WebkitTextSizeAdjust: '100%'
                                                    }}
                                                >
                                                    {separatorLine}
                                                </div>
                                            );
                                        })()
                                    }
                                </div>
                            ) : (
                                <div key={index} className={`mb-1`}>
                                    {log.timestamp && (
                                        <span style={{ color: 'rgb(106, 153, 85)' }}>[{log.timestamp}]</span>
                                    )}
                                    {' '}
                                    <span className="font-semibold" style={{ color: getLogColor(log.level) }}>[{log.level}]</span>
                                    {' '}
                                    {log.fileInfo && (
                                        <>
                                            <span style={{ color: 'rgb(86, 156, 214)' }}>[{log.fileInfo}]</span>
                                            <span style={{ color: getLogColor(log.level) }}> |</span>
                                        </>
                                    )}
                                    {' '}
                                    <span style={{ color: getLogColor(log.level) }}>{log.message}</span>
                                </div>
                            )
                        ))
                    )}
                </div>
            </div>
        </div>
    );
}
