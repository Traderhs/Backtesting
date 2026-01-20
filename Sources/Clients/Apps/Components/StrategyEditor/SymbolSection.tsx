import React, {useEffect, useLayoutEffect, useRef, useState} from 'react';
import {createPortal} from 'react-dom';
import {useWebSocket} from '../Server/WebSocketContext';
import {useStrategy} from './StrategyContext';
import './StrategyEditor.css';

// StrategyEditor 전역에서 사용 가능한 드롭다운 자동 스크롤 훅
import {useDropdownAutoScroll} from './useDropdownAutoScroll';

// 기본 페어 목록 (우선순위대로)
const DEFAULT_PAIRS = [
    '없음',  // 페어 없음
    'USDT',  // 최우선
    'BTC', 'ETH', 'BNB', 'SOL',  // 코인계열에서 많이 쓰는 페어
    'USD', 'KRW', 'JPY', 'EUR', 'GBP'  // 일반적으로 많이 쓰는 페어
];

export default function SymbolSection() {
    const {
        symbolConfigs,
        setSymbolConfigs,
        selectedPair,
        setSelectedPair,
        customPairs,
        setCustomPairs
    } = useStrategy();

    const [symbolLogos, setSymbolLogos] = useState<Record<string, { url: string | null; loading: boolean }>>({});
    const [symbolInput, setSymbolInput] = useState<string>('');
    const [knownSymbols, setKnownSymbols] = useState<string[]>([]);
    const [suggestionsVisible, setSuggestionsVisible] = useState<boolean>(false);
    const [selectedSuggestionIndex, setSelectedSuggestionIndex] = useState<number>(-1);
    const [justAutocompleted, setJustAutocompleted] = useState<boolean>(false);
    const [autocompletedBase, setAutocompletedBase] = useState<string | null>(null);
    const [caretPos, setCaretPos] = useState<number | null>(null);
    const suggestionsContainerRef = useRef<HTMLDivElement | null>(null);

    // 페어 관련 상태
    const [isAddingCustomPair, setIsAddingCustomPair] = useState(false);
    const [customPairInput, setCustomPairInput] = useState('');
    const customPairInputRef = useRef<HTMLInputElement>(null);
    const [isPairDropdownOpen, setIsPairDropdownOpen] = useState(false);
    const pairDropdownRef = useRef<HTMLDivElement>(null);

    // 드래그 앤 드롭 관련 상태
    const [draggedIndex, setDraggedIndex] = useState<number | null>(null);
    const [dropIndex, setDropIndex] = useState<number | null>(null); // 삽입될 인덱스 (0 ~ length)
    const scrollContainerRef = useRef<HTMLDivElement>(null);
    const [dropIndicatorTop, setDropIndicatorTop] = useState<number | null>(null);
    const [dragPreviewPos, setDragPreviewPos] = useState<{ x: number; y: number } | null>(null);
    const [dragScrollVersion, setDragScrollVersion] = useState<number>(0);

    // Pointer 기반 드래그용 ref
    const activePointerIdRef = useRef<number | null>(null);
    const draggedIndexRef = useRef<number | null>(null);
    const dropIndexRef = useRef<number | null>(null);
    const lastPointerPosRef = useRef<{ x: number; y: number } | null>(null);
    const rafPreviewIdRef = useRef<number | null>(null);
    const pendingWheelDeltaRef = useRef<number>(0);
    const wheelRafIdRef = useRef<number | null>(null);
    const lastWheelTimeRef = useRef<number>(0);

    useEffect(() => {
        draggedIndexRef.current = draggedIndex;
    }, [draggedIndex]);

    useEffect(() => {
        dropIndexRef.current = dropIndex;
    }, [dropIndex]);

    // 드래그 중에는 body 커서를 move로 강제
    useEffect(() => {
        if (draggedIndex === null) {
            document.body.classList.remove('symbol-dragging');
            return;
        }

        document.body.classList.add('symbol-dragging');

        return () => {
            document.body.classList.remove('symbol-dragging');
        };
    }, [draggedIndex]);

    // 가이드선은 레이아웃을 밀지 않도록 absolute overlay로 계산
    useLayoutEffect(() => {
        if (draggedIndex === null || dropIndex === null) {
            setDropIndicatorTop(null);
            return;
        }

        // 제자리 이동(바로 위/아래)은 가이드 표시하지 않음
        if (dropIndex === draggedIndex || dropIndex === draggedIndex + 1) {
            setDropIndicatorTop(null);
            return;
        }

        const container = scrollContainerRef.current;
        if (!container) {
            setDropIndicatorTop(null);
            return;
        }

        const items = Array.from(container.querySelectorAll('.strategy-editor-symbol-item')) as HTMLElement[];
        if (items.length === 0) {
            setDropIndicatorTop(null);
            return;
        }

        const containerRect = container.getBoundingClientRect();

        // 심볼 테두리에서 살짝 띄워서 가이드선이 "붙어" 보이지 않게 함
        const indicatorGapPx = 3;

        let top: number;
        if (dropIndex <= 0) {
            const firstRect = items[0].getBoundingClientRect();
            top = (firstRect.top - containerRect.top) + container.scrollTop - indicatorGapPx * 2;
        } else if (dropIndex >= items.length) {
            const lastRect = items[items.length - 1].getBoundingClientRect();
            top = (lastRect.bottom - containerRect.top) + container.scrollTop + indicatorGapPx;
        } else {
            const prevRect = items[dropIndex - 1].getBoundingClientRect();
            top = (prevRect.bottom - containerRect.top) + container.scrollTop + indicatorGapPx;
        }

        setDropIndicatorTop(top);
    }, [draggedIndex, dropIndex, symbolConfigs.length, dragScrollVersion]);

    // Pointer 기반 드래그 중 위치 계산 및 드롭 처리
    useEffect(() => {
        if (draggedIndex === null) {
            return;
        }

        const schedulePreviewPosUpdate = (x: number, y: number) => {
            lastPointerPosRef.current = {x, y};

            if (rafPreviewIdRef.current !== null) {
                return;
            }

            rafPreviewIdRef.current = window.requestAnimationFrame(() => {
                rafPreviewIdRef.current = null;
                const p = lastPointerPosRef.current;

                if (!p) {
                    return;
                }

                setDragPreviewPos({x: p.x, y: p.y});
            });
        };

        const computeAutoScrollDelta = (container_rect: DOMRect, client_y: number) => {
            // 휠로 스크롤 중에는 엣지 오토스크롤을 잠깐 끄고, 충돌로 인한 이질감 방지
            if (performance.now() - lastWheelTimeRef.current < 200) {
                return 0;
            }

            const scrollThreshold = 90;
            const maxScrollSpeed = 12;

            const distTop = client_y - container_rect.top;
            const distBottom = container_rect.bottom - client_y;

            if (distTop < scrollThreshold) {
                const t = Math.max(0, Math.min(1, (scrollThreshold - distTop) / scrollThreshold));
                return -Math.max(1, Math.round(maxScrollSpeed * t * t));
            }

            if (distBottom < scrollThreshold) {
                const t = Math.max(0, Math.min(1, (scrollThreshold - distBottom) / scrollThreshold));
                return Math.max(1, Math.round(maxScrollSpeed * t * t));
            }

            return 0;
        };

        const updateDropIndexByPoint = (client_x: number, client_y: number) => {
            const container = scrollContainerRef.current;
            if (!container) {
                dropIndexRef.current = null;
                setDropIndex(null);
                return;
            }

            const containerRect = container.getBoundingClientRect();
            const isInContainer = !(client_y < containerRect.top || client_y > containerRect.bottom ||
                client_x < containerRect.left || client_x > containerRect.right);

            if (!isInContainer) {
                dropIndexRef.current = null;
                setDropIndex(null);
                return;
            }

            // 자동 스크롤
            const delta = computeAutoScrollDelta(containerRect, client_y);
            if (delta !== 0) {
                const prevScrollTop = container.scrollTop;
                container.scrollTop += delta;

                if (container.scrollTop !== prevScrollTop) {
                    setDragScrollVersion(v => v + 1);
                }
            }

            const items = Array.from(container.querySelectorAll('.strategy-editor-symbol-item')) as HTMLElement[];
            if (items.length === 0) {
                dropIndexRef.current = 0;
                setDropIndex(0);
                return;
            }

            let foundIndex = items.length;
            for (let i = 0; i < items.length; i++) {
                const childRect = items[i].getBoundingClientRect();
                const midY = childRect.top + (childRect.height / 2);

                if (client_y < midY) {
                    foundIndex = i;
                    break;
                }
            }

            if (dropIndexRef.current !== foundIndex) {
                dropIndexRef.current = foundIndex;
                setDropIndex(foundIndex);
            }
        };

        const finalizeDrop = () => {
            const from = draggedIndexRef.current;
            const to = dropIndexRef.current;

            document.body.classList.remove('symbol-dragging');
            activePointerIdRef.current = null;

            if (from === null || to === null) {
                setDraggedIndex(null);
                setDropIndex(null);
                return;
            }

            if (to === from || to === from + 1) {
                setDraggedIndex(null);
                setDropIndex(null);
                return;
            }

            setSymbolConfigs(prev => {
                if (from < 0 || from >= prev.length) {
                    return prev;
                }

                const next = [...prev];
                const [moved] = next.splice(from, 1);

                let insertAt = to;
                if (from < to) {
                    insertAt -= 1;
                }

                insertAt = Math.max(0, Math.min(insertAt, next.length));
                next.splice(insertAt, 0, moved);
                return next;
            });

            setDraggedIndex(null);
            setDropIndex(null);
        };

        const onPointerMove = (e: PointerEvent) => {
            const activeId = activePointerIdRef.current;
            if (activeId !== null && e.pointerId !== activeId) {
                return;
            }

            schedulePreviewPosUpdate(e.clientX, e.clientY);

            updateDropIndexByPoint(e.clientX, e.clientY);
        };

        const onPointerUpOrCancel = (e: PointerEvent) => {
            const activeId = activePointerIdRef.current;
            if (activeId !== null && e.pointerId !== activeId) {
                return;
            }

            finalizeDrop();
        };

        window.addEventListener('pointermove', onPointerMove, {capture: true});
        window.addEventListener('pointerup', onPointerUpOrCancel, {capture: true});
        window.addEventListener('pointercancel', onPointerUpOrCancel, {capture: true});

        const onWheel = (e: WheelEvent) => {
            const container = scrollContainerRef.current;
            if (!container) {
                return;
            }

            // 드래그 중에는 wheel이 기본 스크롤로만 동작하도록 고정
            e.preventDefault();
            e.stopPropagation();

            lastWheelTimeRef.current = performance.now();
            pendingWheelDeltaRef.current += e.deltaY;

            const stepWheelScroll = () => {
                wheelRafIdRef.current = null;
                const c = scrollContainerRef.current;
                if (!c) {
                    pendingWheelDeltaRef.current = 0;
                    return;
                }

                const pending = pendingWheelDeltaRef.current;
                if (Math.abs(pending) < 0.5) {
                    pendingWheelDeltaRef.current = 0;
                    return;
                }

                // 한 프레임에 일부만 적용해서 부드럽게 보간
                const apply = pending * 0.28;
                pendingWheelDeltaRef.current = pending - apply;

                const prevScrollTop = c.scrollTop;
                c.scrollTop += apply;
                if (c.scrollTop !== prevScrollTop) {
                    setDragScrollVersion(v => v + 1);
                }

                const p = lastPointerPosRef.current;
                if (p) {
                    updateDropIndexByPoint(p.x, p.y);
                    schedulePreviewPosUpdate(p.x, p.y);
                }

                wheelRafIdRef.current = window.requestAnimationFrame(stepWheelScroll);
            };

            if (wheelRafIdRef.current === null) {
                wheelRafIdRef.current = window.requestAnimationFrame(stepWheelScroll);
            }
        };

        const onContainerScroll = () => {
            setDragScrollVersion(v => v + 1);

            const p = lastPointerPosRef.current;
            if (p) {
                updateDropIndexByPoint(p.x, p.y);
            }
        };

        const container = scrollContainerRef.current;
        if (container) {
            container.addEventListener('scroll', onContainerScroll, {passive: true});
        }

        document.addEventListener('wheel', onWheel, {capture: true, passive: false});

        return () => {
            window.removeEventListener('pointermove', onPointerMove, {capture: true} as any);
            window.removeEventListener('pointerup', onPointerUpOrCancel, {capture: true} as any);
            window.removeEventListener('pointercancel', onPointerUpOrCancel, {capture: true} as any);

            document.removeEventListener('wheel', onWheel, {capture: true} as any);

            if (container) {
                container.removeEventListener('scroll', onContainerScroll as any);
            }

            if (rafPreviewIdRef.current !== null) {
                window.cancelAnimationFrame(rafPreviewIdRef.current);
                rafPreviewIdRef.current = null;
            }

            if (wheelRafIdRef.current !== null) {
                window.cancelAnimationFrame(wheelRafIdRef.current);
                wheelRafIdRef.current = null;
            }

            pendingWheelDeltaRef.current = 0;

            setDragPreviewPos(null);
        };
    }, [draggedIndex, setSymbolConfigs]);

    // 백엔드로부터 사용 가능한 베이스 심볼 목록을 가져옴
    const {ws} = useWebSocket();
    useEffect(() => {
        if (!ws) {
            return;
        }

        (async () => {
            try {
                const res = await fetch('/api/symbols?include=popular,web');
                if (!res.ok) {
                    return;
                }

                const data = await res.json();

                if (Array.isArray(data.symbols)) {
                    // 서버에서 이미 베이스 심볼만 반환하므로 그대로 사용
                    setKnownSymbols(data.symbols.map((s: string) => String(s).toUpperCase()).sort());
                }
            } catch (e) {
                // 무시
            }
        })();
    }, [ws]);

    // 페어 추가 핸들러
    const handleAddCustomPair = () => {
        const pair = customPairInput.trim().toUpperCase();
        if (!pair) {
            return;
        }

        // 기본 페어이거나 이미 추가된 페어인 경우 선택하고 모달 닫기
        if (DEFAULT_PAIRS.includes(pair) || customPairs.includes(pair)) {
            // 기본 페어인 경우에는 경고 없이 모달 닫기
            setSelectedPair(pair);

            setIsAddingCustomPair(false);
            setCustomPairInput('');
            return;
        }

        setCustomPairs(prev => [...prev, pair]);
        setSelectedPair(pair);

        setIsAddingCustomPair(false);
        setCustomPairInput('');
    };

    // 페어 삭제 핸들러
    const handleRemoveCustomPair = (pair: string) => {
        setCustomPairs(prev => prev.filter(p => p !== pair));

        if (selectedPair === pair) {
            setSelectedPair('USDT');
        }
    };

    // 페어 입력창이 열릴 때 포커스
    useEffect(() => {
        if (isAddingCustomPair && customPairInputRef.current) {
            customPairInputRef.current.focus();
        }
    }, [isAddingCustomPair]);

    const isCompletedSymbol = (input: string) => {
        const s = (input || '').toUpperCase().trim();
        if (!s) {
            return false;
        }

        // 페어가 "없음"이 아닌 경우, 선택된 페어로 끝나는지 확인
        if (selectedPair !== '없음' && !s.endsWith(selectedPair)) {
            return false;
        }

        if (symbolConfigs.includes(s)) {
            return false;
        }

        if (typeof caretPos !== 'number') {
            return true;
        }

        return caretPos === s.length;
    };

    const getSuggestions = (input: string) => {
        const q = (input || '').toUpperCase();
        if (!q) {
            return [];
        }

        const pos = typeof caretPos === 'number' && caretPos >= 0 ? Math.min(caretPos, q.length) : q.length;
        const prefix = q.slice(0, pos).trim();
        if (!prefix) {
            return [];
        }

        // 항상 베이스 심볼만 추천
        let matches = knownSymbols.filter(s => s.startsWith(prefix));

        matches.sort((a, b) => {
            const sa = a.length - prefix.length;
            const sb = b.length - prefix.length;

            if (sa !== sb) {
                return sa - sb;
            }

            return a.localeCompare(b);
        });

        return matches.slice(0, 50);
    };

    const fetchLogoForSymbol = async (symbol: string) => {
        const info = symbolLogos[symbol];
        if (info && info.loading) {
            return;
        }

        setSymbolLogos(prev => ({...prev, [symbol]: {url: null, loading: true}}));

        try {
            const res = await fetch(`/api/get-logo?symbol=${encodeURIComponent(symbol)}`);
            const data = await res.json();
            const url = data && data.logoUrl ? data.logoUrl : null;

            setSymbolLogos(prev => ({...prev, [symbol]: {url, loading: false}}));
        } catch (e) {
            setSymbolLogos(prev => ({...prev, [symbol]: {url: null, loading: false}}));
        }
    };

    const handleAddSymbol = (symbol: string) => {
        const sRaw = symbol.trim();
        if (!sRaw) {
            return;
        }

        const s = sRaw.toUpperCase();

        if (!symbolConfigs.includes(s)) {
            setSymbolConfigs(prev => [...prev, s]);

            // 로고 로딩 상태 등록 및 즉시 페치 시작
            setSymbolLogos(prev => ({...prev, [s]: {url: null, loading: true}}));
            fetchLogoForSymbol(s).then();
        }
    };

    const handleRemoveSymbol = (symbolIndex: number) => {
        const removedSymbol = symbolConfigs[symbolIndex];
        setSymbolConfigs(prev => prev.filter((_, i) => i !== symbolIndex));

        setSymbolLogos(prev => {
            const copy = {...prev};

            if (removedSymbol && copy[removedSymbol]) {
                delete copy[removedSymbol];
            }

            return copy;
        });
    };

    // 심볼 목록 초기화 핸들러
    const handleResetSymbols = async () => {
        // Electron 환경이면 네이티브 다이얼로그 사용, 아니면 브라우저 confirm으로 폴백
        let confirmed: boolean;

        try {
            if (window && (window as any).electronAPI && (window as any).electronAPI.showConfirm) {
                confirmed = await (window as any).electronAPI.showConfirm({
                    title: 'BackBoard',
                    message: '심볼 목록을 초기화하시겠습니까?'
                });
            } else {
                confirmed = confirm('심볼 목록을 초기화하시겠습니까?');
            }
        } catch (e) {
            // 실패 시 안전하게 폴백
            confirmed = confirm('심볼 목록을 초기화하시겠습니까?');
        }

        if (!confirmed) {
            return;
        }

        // 심볼 목록 및 로고 상태 초기화
        setSymbolConfigs([]);
        setSymbolLogos({});

        // 입력/추천 상태 초기화
        setSymbolInput('');
        setSuggestionsVisible(false);
        setSelectedSuggestionIndex(-1);
        setJustAutocompleted(false);
        setAutocompletedBase(null);
    };

    // 컴포넌트가 마운트되거나 symbolConfigs가 바뀔 때, 아직 로고가 없는 심볼에 대해 로고를 가져옴
    useEffect(() => {
        for (const s of symbolConfigs) {
            const info = symbolLogos[s];

            if (!info || (info.url === null && !info.loading)) {
                fetchLogoForSymbol(s).then();
            }
        }
    }, [symbolConfigs]);

    // 새 심볼이 추가되었을 때 리스트가 자동으로 맨 아래로 스크롤되도록 함
    const prevSymbolCountRef = useRef<number>(symbolConfigs.length);

    // 초기 로드 시(서버/컨텍스트로부터 심볼이 채워지는 경우) 자동 스크롤을 억제하기 위한 플래그
    const initialLoadRef = useRef<boolean>(true);

    useEffect(() => {
        const prev = prevSymbolCountRef.current;
        const cur = symbolConfigs.length;

        // 첫 렌더링 또는 초기 데이터 로드 시에는 자동으로 맨 아래로 스크롤하지 않음
        if (initialLoadRef.current) {
            initialLoadRef.current = false;
            prevSymbolCountRef.current = cur;
            return;
        }

        // 갯수가 늘어난 경우에만 스크롤 (중복으로 추가되지 않았을 때)
        if (cur > prev) {
            const container = scrollContainerRef.current;
            if (container) {
                // 초기 로드에서 다수 항목으로 한꺼번에 로드된 경우(이전 갯수 0이고 아직 사용자가 스크롤하지 않은 경우),
                // 자동으로 맨 아래로 스크롤하지 않음. 이는 초기 로드에서 상단에 머무르도록 보장.
                const isLikelyInitialPopulation = (prev === 0 && container.scrollTop === 0);
                if (!isLikelyInitialPopulation) {
                    // 즉시 맨 아래로 이동
                    container.scrollTop = container.scrollHeight;
                }
            }
        }

        prevSymbolCountRef.current = cur;
    }, [symbolConfigs.length]);

    // 선택된 추천 항목이 바뀔 때 스크롤하여 뷰에 들어오도록 함
    useEffect(() => {
        if (selectedSuggestionIndex < 0) {
            return;
        }

        const container = suggestionsContainerRef.current;
        if (!container) {
            return;
        }

        const el = container.querySelector(`[data-index='${selectedSuggestionIndex}']`) as HTMLElement | null;
        if (!el) {
            return;
        }

        const elTop = el.offsetTop;
        const elBottom = elTop + el.offsetHeight;
        const viewTop = container.scrollTop;
        const viewBottom = viewTop + container.clientHeight;

        if (elTop < viewTop) {
            container.scrollTop = elTop;
        } else if (elBottom > viewBottom) {
            container.scrollTop = elBottom - container.clientHeight;
        }
    }, [selectedSuggestionIndex]);

    // 드롭다운 외부 클릭 감지
    useEffect(() => {
        const handleClickOutside = (event: MouseEvent) => {
            if (pairDropdownRef.current && !pairDropdownRef.current.contains(event.target as Node)) {
                setIsPairDropdownOpen(false);
            }
        };

        document.addEventListener('mousedown', handleClickOutside);
        return () => {
            document.removeEventListener('mousedown', handleClickOutside);
        };
    }, []);

    // 페어 선택 핸들러
    const handlePairSelect = (pair: string) => {
        setSelectedPair(pair);
        setIsPairDropdownOpen(false);
    };

    // 드롭다운이 열릴 때 선택된 항목으로 자동 스크롤 처리 (전역 훅 사용)
    useDropdownAutoScroll(pairDropdownRef, isPairDropdownOpen);

    const handlePointerDownOnItem = (e: React.PointerEvent<HTMLDivElement>, index: number) => {
        // 좌클릭만 드래그 시작
        if (e.button !== 0) {
            return;
        }

        e.preventDefault();

        lastPointerPosRef.current = {x: e.clientX, y: e.clientY};
        setDragPreviewPos({x: e.clientX, y: e.clientY});

        activePointerIdRef.current = e.pointerId;
        draggedIndexRef.current = index;
        dropIndexRef.current = null;
        try {
            e.currentTarget.setPointerCapture(e.pointerId);
        } catch (err) {
            // 무시
        }

        document.body.classList.add('symbol-dragging');
        setDraggedIndex(index);
    };

    return (
        <div
            className="strategy-editor-section-container h-full flex flex-col"
            style={{marginRight: '10px'}}
        >
            <h2 className="strategy-editor-section-header">심볼 설정</h2>

            <div style={{padding: '0 0.5rem', display: 'flex', flexDirection: 'column', flex: 1, marginTop: '8px'}}>
                {/* 심볼 입력란, 페어 선택, 버튼들 */}
                <div style={{display: 'flex', gap: '12px'}}>
                    {/* 심볼 입력 필드 */}
                    <div style={{position: 'relative', flex: 1}}>
                        <input
                            id="strategy-symbol-input"
                            type="text"
                            autoComplete="off"
                            placeholder="심볼 이름"
                            value={symbolInput}
                            onChange={(e: React.ChangeEvent<HTMLInputElement>) => {
                                const el = document.getElementById('strategy-symbol-input') as HTMLInputElement | null;
                                const selStart = el ? el.selectionStart : null;

                                const v = e.currentTarget.value.toUpperCase();
                                setSymbolInput(v);

                                if (selStart !== null) {
                                    setCaretPos(Math.min(selStart, v.length));
                                } else {
                                    setCaretPos(v.length);
                                }

                                setJustAutocompleted(false);
                                setAutocompletedBase(null);
                                setSuggestionsVisible(true);
                                setSelectedSuggestionIndex(0);

                                if (el && selStart !== null) {
                                    requestAnimationFrame(() => {
                                        try {
                                            const clampedStart = Math.min(selStart, el.value.length);
                                            el.setSelectionRange(clampedStart, clampedStart);
                                        } catch (err) {
                                            // 무시
                                        }
                                    });
                                }
                            }}
                            onKeyDown={(e: React.KeyboardEvent<HTMLInputElement>) => {
                                if (e.key === 'Enter') {
                                    e.preventDefault();

                                    // 바로 전 Enter로 자동완성된 상태라면 두번째 Enter는 바로 추가로 동작하도록 처리
                                    if (justAutocompleted && autocompletedBase) {
                                        const finalSymbol = selectedPair === '없음'
                                            ? autocompletedBase
                                            : autocompletedBase + selectedPair;

                                        handleAddSymbol(finalSymbol);
                                        setSymbolInput('');
                                        setSuggestionsVisible(false);
                                        setSelectedSuggestionIndex(-1);
                                        setJustAutocompleted(false);
                                        setAutocompletedBase(null);
                                        return;
                                    }

                                    const suggs = getSuggestions(symbolInput);

                                    if (suggs.length > 0 && selectedSuggestionIndex >= 0) {
                                        const chosen = suggs[selectedSuggestionIndex];

                                        if (chosen) {
                                            if (symbolInput === chosen) {
                                                const finalSymbol = selectedPair === '없음'
                                                    ? chosen
                                                    : chosen + selectedPair;

                                                handleAddSymbol(finalSymbol);
                                                setSymbolInput('');
                                                setSuggestionsVisible(false);
                                                setSelectedSuggestionIndex(-1);
                                                setJustAutocompleted(false);
                                                setAutocompletedBase(null);
                                            } else {
                                                setSymbolInput(chosen);
                                                setCaretPos(chosen.length);
                                                setJustAutocompleted(true);
                                                setAutocompletedBase(chosen);
                                                setSuggestionsVisible(false);

                                                const newIndex = suggs.indexOf(chosen);
                                                setSelectedSuggestionIndex(newIndex >= 0 ? newIndex : 0);

                                                requestAnimationFrame(() => {
                                                    const inputEl = document.getElementById('strategy-symbol-input') as HTMLInputElement | null;
                                                    if (inputEl) {
                                                        try {
                                                            inputEl.focus();
                                                            inputEl.setSelectionRange(chosen.length, chosen.length);
                                                        } catch (e) {
                                                            // 무시
                                                        }
                                                    }
                                                });
                                            }
                                        }
                                        return;
                                    }

                                    const normalized = (symbolInput || '').toUpperCase().trim();
                                    if (!normalized) {
                                        return;
                                    }

                                    const fullSymbol = selectedPair === '없음'
                                        ? normalized
                                        : (normalized.endsWith(selectedPair) ? normalized : normalized + selectedPair);

                                    handleAddSymbol(fullSymbol);
                                    setSymbolInput('');
                                    setSuggestionsVisible(false);
                                    setSelectedSuggestionIndex(-1);
                                    setJustAutocompleted(false);
                                    setAutocompletedBase(null);
                                } else if (e.key === 'Tab') {
                                    e.preventDefault();

                                    const suggs = getSuggestions(symbolInput);
                                    if (suggs.length > 0) {
                                        const idx = selectedSuggestionIndex >= 0 ? selectedSuggestionIndex : 0;
                                        const chosen = suggs[idx];
                                        if (chosen) {
                                            setSymbolInput(chosen);
                                            setCaretPos(chosen.length);
                                            setJustAutocompleted(true);
                                            setAutocompletedBase(chosen);
                                            setSuggestionsVisible(false);
                                            setSelectedSuggestionIndex(idx);
                                        }
                                    }
                                } else if (e.key === 'ArrowDown') {
                                    e.preventDefault();
                                    const suggs = getSuggestions(symbolInput);

                                    if (suggs.length === 0) {
                                        return;
                                    }

                                    setSelectedSuggestionIndex(i => Math.min(i + 1, suggs.length - 1));
                                } else if (e.key === 'ArrowUp') {
                                    e.preventDefault();
                                    const suggs = getSuggestions(symbolInput);

                                    if (suggs.length === 0) {
                                        return;
                                    }

                                    setSelectedSuggestionIndex(i => Math.max(i - 1, 0));
                                } else if (e.key === 'Escape') {
                                    setSuggestionsVisible(false);
                                }
                            }}
                            onBlur={() => {
                                setTimeout(() => {
                                    setSuggestionsVisible(false);
                                }, 150);
                            }}
                            onSelect={(e: React.SyntheticEvent<HTMLInputElement>) => {
                                const el = e.target as HTMLInputElement;
                                setCaretPos(el.selectionStart ?? null);
                            }}
                            onKeyUp={(e: React.KeyboardEvent<HTMLInputElement>) => {
                                const el = e.currentTarget as HTMLInputElement;
                                setCaretPos(el.selectionStart ?? null);
                            }}
                            onClick={(e: React.MouseEvent<HTMLInputElement>) => {
                                const el = e.currentTarget as HTMLInputElement;
                                const pos = el.selectionStart ?? el.value.length;

                                setCaretPos(pos);

                                const prefix = (el.value || '').slice(0, pos).toUpperCase().trim();
                                const hasSuggestion = prefix.length > 0 && knownSymbols.some(s => s.startsWith(prefix));

                                if (hasSuggestion) {
                                    setSuggestionsVisible(true);
                                    setSelectedSuggestionIndex(0);
                                }
                            }}
                            className="strategy-editor-input"
                            style={{width: '100%'}}
                        />

                        {/* 추천 목록 */}
                        {suggestionsVisible && symbolInput && !isCompletedSymbol(symbolInput) && getSuggestions(symbolInput).length > 0 && (
                            <div className="strategy-editor-dropdown-options" ref={suggestionsContainerRef}>
                                {getSuggestions(symbolInput).map((sugg, idx) => (
                                    <div
                                        key={sugg}
                                        data-index={idx}
                                        onMouseDown={(ev) => {
                                            ev.preventDefault();

                                            setSymbolInput(sugg);
                                            setCaretPos(sugg.length);
                                            setJustAutocompleted(true);
                                            setAutocompletedBase(sugg);
                                            setSuggestionsVisible(false);
                                            setSelectedSuggestionIndex(idx);

                                            requestAnimationFrame(() => {
                                                const inputEl = document.getElementById('strategy-symbol-input') as HTMLInputElement | null;

                                                if (inputEl) {
                                                    try {
                                                        inputEl.focus();
                                                        inputEl.setSelectionRange(sugg.length, sugg.length);
                                                    } catch (e) {
                                                        // 무시
                                                    }
                                                }
                                            });
                                        }}
                                        className={`strategy-editor-dropdown-option ${selectedSuggestionIndex === idx ? 'selected' : ''}`}
                                    >
                                        {sugg}
                                    </div>
                                ))}
                            </div>
                        )}
                    </div>

                    {/* 페어 선택 드롭다운 */}
                    <div className="strategy-editor-dropdown-field" ref={pairDropdownRef} style={{width: '120px'}}>
                        <div
                            className="strategy-editor-dropdown-select"
                            onClick={() => setIsPairDropdownOpen(!isPairDropdownOpen)}
                        >
                            {selectedPair}
                        </div>

                        {isPairDropdownOpen && (
                            <div className="strategy-editor-dropdown-options">
                                {/* 기본 페어들 */}
                                {DEFAULT_PAIRS.map(pair => (
                                    <div
                                        key={pair}
                                        className={`strategy-editor-dropdown-option ${selectedPair === pair ? 'selected' : ''}`}
                                        onClick={() => handlePairSelect(pair)}
                                    >
                                        {pair}
                                    </div>
                                ))}

                                {/* 페어들 */}
                                {customPairs.length > 0 && (
                                    <>
                                        {customPairs.map(pair => (
                                            <div
                                                key={pair}
                                                className={`strategy-editor-dropdown-option ${selectedPair === pair ? 'selected' : ''}`}
                                                style={{
                                                    display: 'flex',
                                                    alignItems: 'center',
                                                    justifyContent: 'space-between'
                                                }}
                                            >
                                                <span onClick={() => handlePairSelect(pair)} style={{flex: 1}}>
                                                    {pair}
                                                </span>
                                                <button
                                                    onClick={(e) => {
                                                        e.stopPropagation();
                                                        handleRemoveCustomPair(pair);
                                                    }}
                                                    onMouseDown={(e) => e.stopPropagation()}
                                                    onPointerDown={(e) => e.stopPropagation()}
                                                    className="strategy-editor-dropdown-option-remove"
                                                    title="페어 삭제"
                                                >
                                                    ×
                                                </button>
                                            </div>
                                        ))}
                                    </>
                                )}

                                {/* 페어 추가 */}
                                <div
                                    className="strategy-editor-dropdown-option"
                                    style={{
                                        borderTop: '1px solid rgb(255 215 0 / 20%)',
                                        marginTop: '4px'
                                    }}
                                    onClick={() => {
                                        setIsAddingCustomPair(true);
                                        setIsPairDropdownOpen(false);
                                    }}
                                >
                                    + 페어 추가
                                </div>
                            </div>
                        )}
                    </div>

                    {/* 버튼 그룹 */}
                    <button
                        onClick={handleResetSymbols}
                        className="strategy-editor-button"
                        title="심볼 목록 초기화"
                    >
                        초기화
                    </button>
                    <button
                        onClick={() => {
                            const suggs = getSuggestions(symbolInput);

                            if (selectedSuggestionIndex >= 0 && suggs.length > 0) {
                                const chosen = suggs[selectedSuggestionIndex];

                                if (autocompletedBase && symbolInput === autocompletedBase) {
                                    const finalSymbol = selectedPair === '없음' ? autocompletedBase : autocompletedBase + selectedPair;

                                    handleAddSymbol(finalSymbol);
                                    setAutocompletedBase(null);
                                } else if (chosen && (justAutocompleted || symbolInput === chosen)) {
                                    const finalSymbol = selectedPair === '없음' ? chosen : chosen + selectedPair;

                                    handleAddSymbol(finalSymbol);
                                } else {
                                    const val = (symbolInput || '').toUpperCase().trim();
                                    if (val) {
                                        const finalSymbol = selectedPair === '없음'
                                            ? val
                                            : (val.endsWith(selectedPair) ? val : val + selectedPair);
                                        handleAddSymbol(finalSymbol);
                                    }
                                }
                            } else {
                                const val = (symbolInput || '').toUpperCase().trim();
                                if (val) {
                                    const finalSymbol = selectedPair === '없음'
                                        ? val
                                        : (val.endsWith(selectedPair) ? val : val + selectedPair);
                                    handleAddSymbol(finalSymbol);
                                }
                            }

                            setSymbolInput('');
                            setSuggestionsVisible(false);
                            setSelectedSuggestionIndex(-1);
                            setJustAutocompleted(false);
                        }}
                        className="strategy-editor-button active"
                        title="심볼 추가"
                    >
                        심볼 추가
                    </button>
                </div>

                {/* 추가된 심볼 리스트 */}
                <div style={{position: 'relative', flex: 1, minHeight: 0, marginTop: '16px'}}>
                    <div
                        className="strategy-editor-symbol-list-container"
                        ref={scrollContainerRef}
                        style={{height: '100%'}}
                    >
                        {draggedIndex !== null && dragPreviewPos !== null && symbolConfigs[draggedIndex] && createPortal(
                            <div
                                className="strategy-editor-symbol-drag-preview"
                                style={{
                                    left: `${dragPreviewPos.x + 12}px`,
                                    top: `${dragPreviewPos.y + 12}px`,
                                }}
                            >
                                <div className="strategy-editor-symbol-number">
                                    {draggedIndex + 1}
                                </div>

                                {symbolLogos[symbolConfigs[draggedIndex]] && symbolLogos[symbolConfigs[draggedIndex]].loading ? (
                                    <div className="chart-loading-indicator" style={{
                                        width: '26px',
                                        height: '26px',
                                        border: '2px solid rgba(20,20,20,0.15) !important',
                                        borderTopColor: '#FFD700',
                                        boxShadow: 'none'
                                    }}/>
                                ) : symbolLogos[symbolConfigs[draggedIndex]] && symbolLogos[symbolConfigs[draggedIndex]].url ? (
                                    <img
                                        src={symbolLogos[symbolConfigs[draggedIndex]].url as string}
                                        alt={`${symbolConfigs[draggedIndex]} logo`}
                                        className="strategy-editor-symbol-logo"
                                    />
                                ) : (
                                    <div className="strategy-editor-symbol-logo-placeholder"/>
                                )}

                                <div className="strategy-editor-symbol-name">
                                    {symbolConfigs[draggedIndex]}
                                </div>
                            </div>,
                            document.body
                        )}

                        {dropIndicatorTop !== null && (
                            <div
                                className="strategy-editor-drop-indicator"
                                style={{top: `${dropIndicatorTop}px`}}
                            />
                        )}
                        <div
                            className="strategy-editor-symbol-list"
                            // 리스트 전체에 min-height가 설정되어 있어 빈 공간 드래그 가능
                        >
                            {symbolConfigs.length === 0 ? (
                                <div style={{
                                    position: 'absolute',
                                    left: 0,
                                    right: 0,
                                    top: 0,
                                    bottom: 0,
                                    display: 'flex',
                                    alignItems: 'center',
                                    justifyContent: 'center',
                                    textAlign: 'center',
                                    color: 'rgb(255 255 255 / 50%)',
                                    fontSize: '16px',
                                    padding: '0 12px'
                                }}>
                                    추가된 심볼이 없습니다.
                                </div>
                            ) : (
                                symbolConfigs.map((symbol, symbolIndex) => {
                                    const info = symbolLogos[symbol];

                                    return (
                                        <React.Fragment key={symbolIndex}>
                                            <div
                                                className={`strategy-editor-symbol-item ${
                                                    draggedIndex === symbolIndex ? 'dragging' : ''
                                                }`}
                                                onPointerDown={(e) => handlePointerDownOnItem(e, symbolIndex)}
                                                style={{cursor: draggedIndex === symbolIndex ? 'grabbing' : 'grab'}}
                                            >
                                                {/* 순서 번호 */}
                                                <div className="strategy-editor-symbol-number">
                                                    {symbolIndex + 1}
                                                </div>

                                                {/* 심볼 로고 */}
                                                {info && info.loading ? (
                                                    <div className="chart-loading-indicator" style={{
                                                        width: '26px',
                                                        height: '26px',
                                                        border: '2px solid rgba(20,20,20,0.15) !important',
                                                        borderTopColor: '#FFD700',
                                                        boxShadow: 'none'
                                                    }}/>
                                                ) : info && info.url ? (
                                                    <img src={info.url} alt={`${symbol} logo`}
                                                         className="strategy-editor-symbol-logo"/>
                                                ) : (
                                                    <div className="strategy-editor-symbol-logo-placeholder"/>
                                                )}

                                                {/* 심볼 이름 */}
                                                <div className="strategy-editor-symbol-name">
                                                    {symbol}
                                                </div>

                                                {/* 삭제 버튼 */}
                                                <button
                                                    onClick={(e) => {
                                                        e.stopPropagation(); // 드래그 이벤트 전파 방지? 클릭이므로 무관하지만 안전하게
                                                        handleRemoveSymbol(symbolIndex);
                                                    }}
                                                    onPointerDown={(e) => e.stopPropagation()} // 드래그 시작 방지
                                                    onMouseDown={(e) => e.stopPropagation()} // 드래그 시작 방지
                                                    className="strategy-editor-symbol-remove"
                                                    title="심볼 삭제"
                                                >
                                                    ×
                                                </button>
                                            </div>
                                        </React.Fragment>
                                    );
                                })
                            )}
                        </div>
                    </div>
                </div>
            </div>

            {/* 페어 추가 모달 */}
            {isAddingCustomPair && (
                <div className="strategy-editor-modal-overlay" onClick={() => {
                    setIsAddingCustomPair(false);
                    setCustomPairInput('');
                }}>
                    <div className="strategy-editor-modal-container" onClick={(e) => e.stopPropagation()}>
                        <h3 className="strategy-editor-modal-header">페어 추가</h3>
                        <div style={{display: 'flex', flexDirection: 'column', gap: '12px'}}>
                            <input
                                ref={customPairInputRef}
                                type="text"
                                placeholder="페어 이름"
                                value={customPairInput}
                                onChange={(e) => setCustomPairInput(e.target.value.toUpperCase())}
                                onKeyDown={(e) => {
                                    if (e.key === 'Enter') {
                                        handleAddCustomPair();
                                    } else if (e.key === 'Escape') {
                                        setIsAddingCustomPair(false);
                                        setCustomPairInput('');
                                    }
                                }}
                                className="strategy-editor-input"
                            />
                            <div style={{display: 'flex', gap: '8px', justifyContent: 'flex-end'}}>
                                <button
                                    onClick={() => {
                                        setIsAddingCustomPair(false);
                                        setCustomPairInput('');
                                    }}
                                    className="strategy-editor-button"
                                >
                                    취소
                                </button>
                                <button
                                    onClick={handleAddCustomPair}
                                    className="strategy-editor-button active"
                                >
                                    추가
                                </button>
                            </div>
                        </div>
                    </div>
                </div>
            )}
        </div>
    );
}
