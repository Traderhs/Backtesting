import React, {createContext, useCallback, useContext, useEffect, useState} from 'react';
import {useWebSocket} from '@/Components/Server/WebSocketContext';

// 결과 선택 기능: projectDir/Results 아래 폴더 목록을 가져오고 앱 전체에서 선택 상태를 공유
// - selectedResult: 선택된 Results 폴더 이름
// - results: 서버에서 읽어온 폴더 목록
// - refreshResults(): Results 목록을 최신화
// - getResultUrl(path): 선택된 결과를 기준으로 파일을 읽어올 수 있는 URL 반환

interface ResultEntry {
    name: string;
    mtime?: number;
}

interface ResultsContextType {
    selectedResult: string | null;
    setSelectedResult: (r: string | null) => void;
    selectResult: (r: string | null, opts?: { autoSwitchOverview?: boolean }) => Promise<void>;
    isSelectingResult: boolean;
    results: ResultEntry[];
    refreshResults: () => Promise<ResultEntry[]>;
    getResultUrl: (relPath: string) => string | null;
    selectedResultVersion: number;
}

const ResultsContext = createContext<ResultsContextType | null>(null);

export const ResultsProvider: React.FC<React.PropsWithChildren<{}>> = ({children}) => {
    const [results, setResults] = useState<ResultEntry[]>([]);
    const [selectedResult, setSelectedResult] = useState<string | null>(null);
    const [isSelectingResult, setIsSelectingResult] = useState(false);
    const [selectedResultVersion, setSelectedResultVersion] = useState<number>(0); // 동일 이름 결과의 재실행(리프레시) 트리거용
    const {ws} = useWebSocket();

    const refreshResults = useCallback(async (): Promise<ResultEntry[]> => {
        try {
            const res = await fetch('/api/results');
            if (!res.ok) {
                setResults([]);
                return [];
            }

            const json = await res.json();

            const list: ResultEntry[] = Array.isArray(json.results) ? json.results : [];
            setResults(list);

            return list;
        } catch (err) {
            setResults([]);
            return [];
        }
    }, []);

    // 초기 마운트 시 한 번만 자동 선택 수행
    useEffect(() => {
        let cancelled = false;
        (async () => {
            const list = await refreshResults();
            if (cancelled) {
                return;
            }

            if (!list || list.length === 0) {
                return;
            }

            // 사용자가 이미 선택한 값이 없을 때만 자동 선택
            setSelectedResult(prev => {
                if (prev) {
                    return prev;
                }

                return list.slice().sort((a, b) => b.name.localeCompare(a.name))[0].name;
            });
        })();
        return () => {
            cancelled = true;
        };
    }, [refreshResults]);


    // 취소·타임아웃·동시성 제어를 통해 선택이 고착되는 것을 방지
    const selectResult = useCallback(async (name: string | null, opts?: { autoSwitchOverview?: boolean }) => {
        // 빠른 경합을 방지하기 위한 고유 id
        const id = (selectResult as any)._seq = ((selectResult as any)._seq || 0) + 1;
        (selectResult as any)._current = id;

        const startTime = Date.now();

        // 즉시 동일한 값이면 재요청이 아닌 경우에만 무시
        if (name === selectedResult) {
            return;
        }

        // 이전에 남아있는 프리페치가 있으면 취소
        try {
            const prev = (selectResult as any)._abort;
            if (prev && typeof prev.abort === 'function') {
                prev.abort();
            }
        } catch (e) {
            // ignore
        }

        setIsSelectingResult(true);

        // 새 AbortController를 등록해 동시 요청을 취소 가능하게 함
        const controller = new AbortController();
        (selectResult as any)._abort = controller;

        // 항상 타임아웃으로 정리되어 settle 됨
        const safeFetch = async (url: string, ms = 2000) => {
            const ac = controller;
            const timer = setTimeout(() => ac.abort(), ms);
            try {
                return await fetch(url, {method: 'GET', signal: ac.signal});
            } finally {
                clearTimeout(timer);
            }
        };

        // finally에서 읽을 변수들을 함수 스코프에 선언
        let autoSwitchOverview = false;

        try {
            // 즉시 선택값을 반영해 UI 반응성을 유지
            setSelectedResult(name);

            autoSwitchOverview = Boolean(opts?.autoSwitchOverview);

            if (!name) {
                // 해제 요청은 빠르게 처리
                await new Promise(r => setTimeout(r, 50));
                return;
            }

            // 전체 프리페치에 대한 최대 대기시간을 둬서 영원히 대기하지 않게 함
            const overallTimeoutMs = 3000;

            // 워커 우선 시도(메인 스레드에서의 JSON/text 파싱을 차단하지 않음)
            const resources = [
                {url: `/api/config?result=${encodeURIComponent(name)}`, responseType: 'json' as const},
                {url: `/api/trade-list?result=${encodeURIComponent(name)}`, responseType: 'json' as const},
                {url: `/api/results/${encodeURIComponent(name)}/backtesting.log`, responseType: 'text' as const}
            ];

            let prefetchPromise: Promise<any>;
            try {
                // 워커 클라이언트 사용
                const rw = await import('@/Utils/ResultsWorkerClient.ts');

                if (rw && typeof rw.prefetchWithAbort === 'function') {
                    // 워커 요청(취소 가능)
                    const {id, promise, abort} = rw.prefetchWithAbort(resources, {timeoutMs: 2000});

                    // 컨트롤러와 연동된 abort 객체로 대체하여 selectResult의 기존 취소 로직과 호환시킴
                    const compositeAbort = () => {
                        try {
                            abort();
                        } catch (e) {
                            // 무시
                        }
                        try {
                            controller.abort();
                        } catch (e) {
                            // 무시
                        }
                    };

                    (selectResult as any)._abort = {abort: compositeAbort, id};

                    prefetchPromise = promise.catch(() => null);
                } else {
                    // 워커 클라이언트가 없는 경우 폴백
                    prefetchPromise = Promise.allSettled([
                        safeFetch(`/api/config?result=${encodeURIComponent(name)}`, 2000).catch(() => null),
                        safeFetch(`/api/trade-list?result=${encodeURIComponent(name)}`, 2000).catch(() => null),
                        safeFetch(`/api/results/${encodeURIComponent(name)}/backtesting.log`, 2000).catch(() => null)
                    ]);
                }
            } catch (e) {
                // 워커 로드/실행 실패 시 기존 방식으로 폴백
                prefetchPromise = Promise.allSettled([
                    safeFetch(`/api/config?result=${encodeURIComponent(name)}`, 2000).catch(() => null),
                    safeFetch(`/api/trade-list?result=${encodeURIComponent(name)}`, 2000).catch(() => null),
                    safeFetch(`/api/results/${encodeURIComponent(name)}/backtesting.log`, 2000).catch(() => null)
                ]);
            }

            // overall timeout 보장
            await Promise.race([
                prefetchPromise,
                new Promise(resolve => setTimeout(resolve, overallTimeoutMs))
            ]);

            // 결과 목록이 동기적으로 바뀔 수 있으므로 한 번 갱신을 시도하되
            // 절대 선택값을 지우지 않음
            void refreshResults().catch(() => null);

            // 너무 빠르게 깜빡이지 않도록 최소 표시 시간 보장
            await new Promise(r => setTimeout(r, 120));
        } catch (err) {
            // 무시
        } finally {
            // 결과 전환 시 전체 스피너는 최소 3000ms는 돔
            try {
                const MIN_DISPLAY_MS = 3000;
                const elapsed = Date.now() - (startTime || 0);
                const remaining = Math.max(0, MIN_DISPLAY_MS - elapsed);

                if (remaining > 0) {
                    // 대기하되, 대기 중에 다른 select가 시작되면 조기 종료
                    await new Promise<void>(resolve => {
                        const start = Date.now();
                        const check = () => {
                            if ((selectResult as any)._current !== id) {
                                resolve();
                                return;
                            }

                            if (Date.now() - start >= remaining) {
                                resolve();
                                return;
                            }

                            setTimeout(check, 50);
                        };

                        check();
                    });
                }
            } catch (e) {
                // 무시
            }

            // 현재 실행 중인 select가 맞을 때만 플래그를 해제
            if ((selectResult as any)._current === id) {
                setIsSelectingResult(false);

                // 성공적으로 선택이 완료된 경우에만 Overview 탭 요청을 전송
                try {
                    if (name && autoSwitchOverview) {
                        requestAnimationFrame(() => {
                            try {
                                window.dispatchEvent(new CustomEvent('backboard.selectTab', {detail: {tab: 'Overview'}}));
                            } catch (e) {
                                // 무시
                            }
                        });
                    }
                } catch (e) {
                    // 무시
                }

                // 선택 완료는 Chart 등에서 리셋이 필요할 수 있으므로 버전 증가로 하위 컴포넌트 리마운트 트리거
                try {
                    setSelectedResultVersion(v => v + 1);
                } catch (e) {
                    // 무시
                }

                // 정리
                try {
                    delete (selectResult as any)._abort;
                } catch (e) {
                    // 무시
                }
            }
        }
    }, [selectedResult, refreshResults]);

    const getResultUrl = useCallback((relPath: string) => {
        if (!selectedResult) {
            return null;
        }

        // relPath는 상대 경로(예: config.json, Indicators/sma/...)로 전달됨
        return `/api/results/${encodeURIComponent(selectedResult)}/${relPath.replace(/^\/+/, '')}`;
    }, [selectedResult]);

    // 서버가 '백테스팅 성공'을 요청자에게 통지하면 자동 갱신 + 선택을 수행
    useEffect(() => {
        if (!ws) return;

        const rawHandler = async (ev: MessageEvent) => {
            try {
                const msg = JSON.parse(ev.data as any);
                if (!msg || msg.action !== 'backtestingSuccess') {
                    return;
                }

                const serverResultName = typeof msg.resultName === 'string' ? msg.resultName : null;
                const list = await refreshResults();

                const pick = serverResultName || (list && list.length ? list.slice().sort((a, b) => b.name.localeCompare(a.name))[0].name : null);
                if (!pick) {
                    return;
                }

                // 서버 이벤트에 의해 호출되는 경우에만 Overview 자동 전환을 허용
                if (pick === selectedResult) {
                    // 목록 갱신 + 버전 증가
                    void refreshResults().catch(() => null);
                    setSelectedResultVersion(v => v + 1);

                    // 서버 알림으로 온 경우에는 Overview 자동 전환을 허용
                    requestAnimationFrame(() => {
                        try {
                            window.dispatchEvent(new CustomEvent('backboard.selectTab', {detail: {tab: 'Overview'}}));
                        } catch (e) {
                            // 무시
                        }
                    });
                } else {
                    await selectResult(pick, {autoSwitchOverview: true});
                }
            } catch (err) {
                // 무시
            }
        };

        // EventListener 타입과의 호환성 때문에 래퍼를 사용
        const wrapper = (ev: Event) => {
            void rawHandler(ev as MessageEvent);
        };

        ws.addEventListener('message', wrapper);

        return () => ws.removeEventListener('message', wrapper);
    }, [ws, refreshResults, selectResult]);

    return (
        <ResultsContext.Provider value={{
            selectedResult,
            setSelectedResult,
            selectResult,
            isSelectingResult,
            results,
            refreshResults,
            getResultUrl,
            selectedResultVersion
        }}>
            {children}
        </ResultsContext.Provider>
    );
};

export function useResults() {
    const ctx = useContext(ResultsContext);
    if (!ctx) {
        throw new Error('useResults must be used within ResultsProvider');
    }

    return ctx;
}
