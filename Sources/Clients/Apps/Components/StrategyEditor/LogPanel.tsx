import React, {useCallback, useEffect, useRef, useState} from 'react';
import {useWebSocket} from '../Server/WebSocketContext';

export type LogEntry = {
    level: string;
    message: string;
    timestamp: string | null;
    fileInfo: string | null;
};

interface Props {
    isLogPanelOpen: boolean;
    setIsLogPanelOpen: (v: boolean) => void;
    logPanelHeight: number;
    setLogPanelHeight: (h: number) => void;
    onAddLog?: (addLogFunc: (level: string, message: string, timestamp?: string | null, fileInfo?: string | null) => void) => void;
}

// 호출자 파일명을 간단히 추출 (라인 번호 제거, 파일명만 반환)
const getCallerFileInfo = (): string | null => {
    try {
        const err = new Error();
        const stack = err.stack || '';
        const lines = stack.split('\n').map(l => l.trim());

        for (let i = 1; i < lines.length; i++) {
            const line = lines[i];
            if (!line || line.includes('getCallerFileInfo') || line.includes('addLog')) {
                continue;
            }

            const m = line.match(/([^\s)]+?\.(tsx|ts|js|jsx|mjs))(?:\?[^:\s]*)?(?::\d+:\d+)?$/i);
            if (!m) {
                continue;
            }

            let fp = m[1].split('?')[0];

            // 경로 분리
            const parts = fp.split(/[\\/]/);
            let filename = parts[parts.length - 1];

            // 파일명 / 확장자 분리
            const extMatch = filename.match(/\.(tsx|ts|js|jsx|mjs)$/i);
            if (!extMatch) {
                continue;
            }

            const ext = extMatch[0];
            let base = filename.slice(0, -ext.length);

            // 끝에 붙은 Vite / 번들 해시 무조건 제거 (ex: -Z0b, -BXO9bpex)
            base = base.replace(/-[A-Za-z0-9]+$/, '');

            // .js → .tsx (원본 표시용)
            let finalExt = ext;
            if (finalExt === '.js') {
                finalExt = '.tsx';
            }

            return base + finalExt;
        }
    } catch {
        // 무시
    }

    return null;
};

export default function LogPanel({
                                     isLogPanelOpen,
                                     setIsLogPanelOpen,
                                     logPanelHeight,
                                     setLogPanelHeight,
                                     onAddLog
                                 }: Props) {
    const [isResizing, setIsResizing] = useState(false);
    const logContainerRef = useRef<HTMLDivElement | null>(null);
    const [logs, setLogs] = useState<LogEntry[]>([]);
    const {ws} = useWebSocket();

    // 로그 추가 함수
    const addLog = useCallback((level: string, message: string, timestamp: string | null = null, fileInfo: string | null = null) => {
        // 레벨이 'SEPARATOR'가 아닌 경우 메시지 앞에 ' | ' 접두어가 항상 붙도록 보장
        if (level !== 'SEPARATOR' && !message.startsWith(' |')) {
            message = ' | ' + message;
        }

        // 제공된 fileInfo가 없으면 호출자 정보로 채움
        if (!fileInfo) {
            const caller = getCallerFileInfo();

            if (caller) {
                fileInfo = caller;
            }
        }

        const finalTimestamp = timestamp || (() => {
            const d = new Date();
            const pad = (n: number) => String(n).padStart(2, '0');

            return `${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())} ${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())}`;
        })();

        // 'SEPARATOR'는 그대로 추가
        if (level === 'SEPARATOR') {
            setLogs(prev => [...prev, {level, message, timestamp: finalTimestamp, fileInfo}]);
            return;
        }

        // cpp(.cpp)에서 발생한 로그는 이미 서버 쪽에서 구분선을 넣을 수 있으므로 추가 구분선을 붙이지 않음
        const isCppLog = !!(fileInfo && /\.cpp\b/i.test(fileInfo));

        if (isCppLog) {
            setLogs(prev => [...prev, {level, message, timestamp: finalTimestamp, fileInfo}]);
        } else {
            // 일반 로그 뒤에는 항상 구분선 추가
            setLogs(prev => [
                ...prev,
                {level, message, timestamp: finalTimestamp, fileInfo},
                {level: 'SEPARATOR', message: '', timestamp: null, fileInfo: null}
            ]);
        }
    }, []);

    // addLog 함수를 부모에게 전달
    useEffect(() => {
        if (onAddLog) {
            onAddLog(addLog);
        }
    }, [addLog, onAddLog]);

    // WebSocket 메시지 수신 (backtestingLog 처리)
    useEffect(() => {
        if (!ws) return;

        const handleMessage = (event: MessageEvent) => {
            try {
                const data = JSON.parse(event.data);

                if (data.action === 'backtestingLog') {
                    addLog(data.level, data.message, data.timestamp, data.fileInfo);
                }
            } catch (err) {
                console.error('메시지 파싱 오류:', err);
            }
        };

        ws.addEventListener('message', handleMessage);

        return () => ws.removeEventListener('message', handleMessage);
    }, [ws, addLog]);

    const getLogColor = (level: string) => {
        switch (level) {
            case 'INFO': {
                return 'rgb(255, 255, 255)';
            }

            case 'BALANCE':
            case 'DEBUG': {
                return 'rgb(128, 128, 128)';
            }

            case 'WARN': {
                return 'rgb(229, 192, 123)';
            }

            case 'ERROR': {
                return 'rgb(224, 108, 117)';
            }

            default: {
                return '#ffffff';
            }
        }
    };

    const handleResizeMouseDown = (e: React.MouseEvent) => {
        e.preventDefault();

        setIsResizing(true);
    };

    useEffect(() => {
        if (!isResizing) {
            return;
        }

        let rafId: number | null = null;
        let lastY = 0;

        const handleMouseMove = (e: MouseEvent) => {
            lastY = e.clientY;

            if (rafId === null) {
                rafId = requestAnimationFrame(() => {
                    const newHeight = window.innerHeight - lastY;
                    if (newHeight >= 200 && newHeight <= window.innerHeight * 0.8) {
                        setLogPanelHeight(newHeight);
                    }

                    rafId = null;
                });
            }
        };

        const handleMouseUp = () => {
            setIsResizing(false);

            if (rafId !== null) {
                cancelAnimationFrame(rafId);
            }
        };

        document.addEventListener('mousemove', handleMouseMove);
        document.addEventListener('mouseup', handleMouseUp);

        return () => {
            document.removeEventListener('mousemove', handleMouseMove);
            document.removeEventListener('mouseup', handleMouseUp);

            if (rafId !== null) {
                cancelAnimationFrame(rafId);
            }
        };
    }, [isResizing, setLogPanelHeight]);

    // 로그가 추가될 때마다 스크롤을 맨 아래로
    useEffect(() => {
        if (logContainerRef.current) {
            logContainerRef.current.scrollTop = logContainerRef.current.scrollHeight;
        }
    }, [logs]);

    if (!isLogPanelOpen) {
        return null;
    }

    return (
        <>
            {/* 리사이즈 바 */}
            <div
                className="h-1 bg-gray-700 hover:bg-blue-500 cursor-ns-resize transition-colors"
                onMouseDown={handleResizeMouseDown}
                style={{userSelect: 'none'}}
            />

            {/* 로그 패널 */}
            <div className="bg-[#1a1a1a] border-t border-gray-700 flex flex-col"
                 style={{height: `${logPanelHeight}px`}}>
                <div className="px-4 py-2 border-b border-gray-700 flex items-center justify-between">
                    <h2 className="text-sm font-semibold text-gray-300">실행 로그</h2>
                    <button
                        onClick={() => setIsLogPanelOpen(false)}
                        className="text-gray-400 hover:text-white text-lg leading-none px-2"
                    >
                        ×
                    </button>
                </div>
                <div ref={logContainerRef} className="overflow-y-auto p-4 font-mono text-sm flex-1"
                     style={{fontFamily: "'Inter', 'Pretendard', monospace"}}>
                    {logs.length === 0 ? (
                        <div className="text-gray-500 text-center mt-8">백테스팅을 실행하면 로그가 여기에 표시됩니다.</div>
                    ) : (
                        logs.map((log, index) => (
                            log.level === 'SEPARATOR' ? (
                                <div key={index} className="my-2">
                                    <div style={{
                                        whiteSpace: 'pre',
                                        fontFamily: "'Inter', 'Pretendard', monospace",
                                        fontSize: '14px',
                                        lineHeight: '1.4',
                                        color: 'rgba(255, 215, 0, 0.4)',
                                    }}>{'─'.repeat(150)}</div>
                                </div>
                            ) : (
                                <div key={index} className="mb-1">
                                    {log.timestamp &&
                                        <span style={{color: 'rgb(106, 153, 85)'}}>[{log.timestamp}]</span>}
                                    {' '}
                                    <span className="font-semibold"
                                          style={{color: getLogColor(log.level)}}>[{log.level}]</span>
                                    {' '}
                                    {log.fileInfo && <><span
                                        style={{color: 'rgb(86, 156, 214)'}}>[{log.fileInfo}]</span></>}
                                    {' '}
                                    <span style={{color: getLogColor(log.level)}}>{log.message}</span>
                                </div>
                            )
                        ))
                    )}
                </div>
            </div>
        </>
    );
}
