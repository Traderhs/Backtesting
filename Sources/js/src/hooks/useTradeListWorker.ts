import { useEffect, useRef, useCallback, useState } from 'react';

interface WorkerHook {
    measureText: (texts: Array<{ text: string; font: string; key: string }>) => Promise<Array<{ key: string; width: number }>>;
    formatCells: (trades: any[], headers: string[], config: any, startIndex: number, endIndex: number) => Promise<any[]>;
    calculateVisibleRange: (scrollTop: number, containerHeight: number, rowHeight: number, totalRows: number, buffer: number) => Promise<{ start: number; end: number }>;
    isReady: boolean;
}

export function useTradeListWorker(): WorkerHook {
    const workerRef = useRef<Worker | null>(null);
    const [isReady, setIsReady] = useState(false);
    const requestIdRef = useRef(0);
    const pendingRequestsRef = useRef(new Map<number, { resolve: (value: any) => void; reject: (error: any) => void }>());

    useEffect(() => {
        // 워커 초기화
        workerRef.current = new Worker(new URL('../workers/tradeListWorker.ts', import.meta.url), {
            type: 'module'
        });

        workerRef.current.onmessage = (e) => {
            const { type, payload, requestId } = e.data;
            
            if (type === 'WORKER_READY') {
                setIsReady(true);
                return;
            }

            const pendingRequest = pendingRequestsRef.current.get(requestId);
            if (!pendingRequest) return;

            pendingRequestsRef.current.delete(requestId);

            if (type === 'ERROR') {
                pendingRequest.reject(new Error(payload.error));
            } else {
                pendingRequest.resolve(payload);
            }
        };

        workerRef.current.onerror = (error) => {
            console.error('TradeList Worker error:', error);
        };

        // 워커 준비 완료 확인
        workerRef.current.postMessage({ type: 'INIT' });

        return () => {
            if (workerRef.current) {
                workerRef.current.terminate();
                workerRef.current = null;
            }
            pendingRequestsRef.current.clear();
        };
    }, []);

    const sendRequest = useCallback((type: string, payload: any): Promise<any> => {
        return new Promise((resolve, reject) => {
            if (!workerRef.current || !isReady) {
                reject(new Error('Worker not ready'));
                return;
            }

            const requestId = ++requestIdRef.current;
            pendingRequestsRef.current.set(requestId, { resolve, reject });

            workerRef.current.postMessage({
                type,
                payload,
                requestId
            });

            // 타임아웃 설정 (5초)
            setTimeout(() => {
                if (pendingRequestsRef.current.has(requestId)) {
                    pendingRequestsRef.current.delete(requestId);
                    reject(new Error('Worker request timeout'));
                }
            }, 5000);
        });
    }, [isReady]);

    const measureText = useCallback((texts: Array<{ text: string; font: string; key: string }>) => {
        return sendRequest('MEASURE_TEXT', { texts });
    }, [sendRequest]);

    const formatCells = useCallback((trades: any[], headers: string[], config: any, startIndex: number, endIndex: number) => {
        return sendRequest('FORMAT_CELLS', { trades, headers, config, startIndex, endIndex });
    }, [sendRequest]);

    const calculateVisibleRange = useCallback((scrollTop: number, containerHeight: number, rowHeight: number, totalRows: number, buffer: number) => {
        return sendRequest('CALCULATE_VISIBLE_RANGE', { scrollTop, containerHeight, rowHeight, totalRows, buffer });
    }, [sendRequest]);

    return {
        measureText,
        formatCells,
        calculateVisibleRange,
        isReady
    };
}
