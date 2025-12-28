import React, {useEffect, useRef, useState} from 'react';
import {Input} from '@/components/ui/input';
import {Button} from '../ui/button';
import {Select, SelectContent, SelectItem, SelectTrigger, SelectValue} from '@/components/ui/select';
import {useWebSocket} from '../Server/WebSocketContext';

interface Props {
    symbolConfigs: string[];
    setSymbolConfigs: React.Dispatch<React.SetStateAction<string[]>>;
    addLog: (level: string, message: string, timestamp?: string | null, fileInfo?: string | null) => void;
    selectedPair: string;
    setSelectedPair: React.Dispatch<React.SetStateAction<string>>;
    customPairs: string[];
    setCustomPairs: React.Dispatch<React.SetStateAction<string[]>>;
}

// 기본 페어 목록 (우선순위대로)
const DEFAULT_PAIRS = [
    '없음',  // 페어 없음
    'USDT',  // 최우선
    'BTC', 'ETH', 'BNB', 'SOL',  // 코인계열에서 많이 쓰는 페어
    'USD', 'KRW', 'JPY', 'EUR', 'GBP'  // 일반적으로 많이 쓰는 페어
];

export default function SymbolSection({
                                          symbolConfigs,
                                          setSymbolConfigs,
                                          addLog,
                                          selectedPair,
                                          setSelectedPair,
                                          customPairs,
                                          setCustomPairs
                                      }: Props) {
    const [symbolLogos, setSymbolLogos] = useState<Record<string, { url: string | null; loading: boolean }>>({});
    const [symbolInput, setSymbolInput] = useState<string>('');
    const [knownSymbols, setKnownSymbols] = useState<string[]>([]);
    const [suggestionsVisible, setSuggestionsVisible] = useState<boolean>(false);
    const [selectedSuggestionIndex, setSelectedSuggestionIndex] = useState<number>(-1);
    const [justAutocompleted, setJustAutocompleted] = useState<boolean>(false);
    const [autocompletedBase, setAutocompletedBase] = useState<string | null>(null); // Tab/Click으로 자동완성된 베이스 심볼
    const [caretPos, setCaretPos] = useState<number | null>(null);
    const suggestionsContainerRef = useRef<HTMLDivElement | null>(null);

    // 사용자가 화살표로 추천을 직접 이동했는지 추적 (기본값으로 자동 선택된 경우와 구분)
    const [suggestionNavigated, setSuggestionNavigated] = useState<boolean>(false);

    // 페어 관련 상태
    const [isAddingCustomPair, setIsAddingCustomPair] = useState(false);
    const [customPairInput, setCustomPairInput] = useState('');
    const customPairInputRef = useRef<HTMLInputElement>(null);

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

    // 커스텀 페어 추가 핸들러
    const handleAddCustomPair = () => {
        const pair = customPairInput.trim().toUpperCase();
        if (!pair) {
            return;
        }

        if (DEFAULT_PAIRS.includes(pair)) {
            addLog('WARN', `${pair}는 이미 기본 페어에 포함되어 있습니다.`);
            return;
        }

        if (customPairs.includes(pair)) {
            addLog('WARN', `${pair}는 이미 추가되어 있습니다.`);
            return;
        }

        setCustomPairs(prev => [...prev, pair]);
        setSelectedPair(pair);

        setIsAddingCustomPair(false);
        setCustomPairInput('');
    };

    // 커스텀 페어 삭제 핸들러
    const handleRemoveCustomPair = (pair: string) => {
        setCustomPairs(prev => prev.filter(p => p !== pair));

        if (selectedPair === pair) {
            setSelectedPair('USDT');
        }
    };

    // 커스텀 페어 입력창이 열릴 때 포커스
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

    // 추가한 심볼들 초기화 핸들러
    const handleResetSymbols = () => {
        if (!confirm('추가한 심볼을 초기화하시겠습니까?')) {
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
        setSuggestionNavigated(false);

        // 로그 남기기
        addLog('INFO', '심볼 목록을 초기화했습니다.');
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

    return (
        <div className="bg-[#1a1a1a] rounded-lg border border-gray-700 p-4 mb-4">
            <h2 className="text-lg font-semibold text-white mb-3">심볼 설정</h2>
            <div className="flex flex-wrap gap-2 mb-3 min-h-[32px]">
                {symbolConfigs.map((symbol, symbolIndex) => {
                    const info = symbolLogos[symbol];

                    return (
                        <div key={symbolIndex}
                             className="bg-blue-600 text-white px-3 py-1 rounded-full text-sm flex items-center gap-2">
                            {info && info.loading ? (
                                <div className="chart-loading-indicator" style={{
                                    width: '18px',
                                    height: '18px',
                                    border: '2px solid rgba(20,20,20,0.15) !important',
                                    borderTopColor: '#FFD700',
                                    boxShadow: 'none'
                                }}/>
                            ) : info && info.url ? (
                                <img src={info.url} alt={`${symbol} logo`} className="w-5 h-5 rounded-full"/>
                            ) : (
                                <div className="w-5 h-5 bg-white/20 rounded-full"/>
                            )}

                            <span className="truncate max-w-[8rem]">{symbol}</span>

                            <button onClick={() => handleRemoveSymbol(symbolIndex)}
                                    className="hover:text-red-300 text-base leading-none" title="심볼 삭제">×
                            </button>
                        </div>
                    );
                })}
            </div>

            <div className="flex gap-2">
                <div className="relative flex-1">
                    <Input
                        id="strategy-symbol-input"
                        type="text"
                        autoComplete="off"
                        placeholder="심볼 이름 입력 후 Enter 키 또는 추가 클릭"
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
                            setSuggestionNavigated(false); // 입력 변경시 네비게이션 초기화

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

                                const suggs = getSuggestions(symbolInput);

                                // 추천 목록이 있을 때
                                if (suggs.length > 0 && selectedSuggestionIndex >= 0) {
                                    const chosen = suggs[selectedSuggestionIndex];

                                    if (chosen) {
                                        if (symbolInput === chosen && justAutocompleted) {
                                            // 두 번째 Enter: 페어 결합하여 실제 추가
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
                                            // 첫 번째 Enter: 베이스 심볼만 자동완성
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

                                // 추천 목록이 없거나 선택되지 않았을 때: 입력값 직접 추가
                                const normalized = (symbolInput || '').toUpperCase().trim();
                                if (!normalized) {
                                    return;
                                }

                                // 일반 입력으로 추가할 때는 autocompletedBase에 의존하지 않음
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

                                // Tab 누르면 추천을 베이스 심볼로 자동완성만 하고 포커스 유지
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
                                        setSuggestionNavigated(true);
                                    }
                                }
                            } else if (e.key === 'ArrowDown') {
                                e.preventDefault();
                                const suggs = getSuggestions(symbolInput);

                                if (suggs.length === 0) {
                                    return;
                                }

                                setSelectedSuggestionIndex(i => Math.min(i + 1, suggs.length - 1));
                                setSuggestionNavigated(true); // 키보드로 이동했음을 표시
                            } else if (e.key === 'ArrowUp') {
                                e.preventDefault();
                                const suggs = getSuggestions(symbolInput);

                                if (suggs.length === 0) {
                                    return;
                                }

                                setSelectedSuggestionIndex(i => Math.max(i - 1, 0));
                                setSuggestionNavigated(true); // 키보드로 이동했음을 표시
                            } else if (e.key === 'Escape') {
                                setSuggestionsVisible(false);
                            }
                        }}
                        onBlur={() => {
                            setTimeout(() => {
                                setSuggestionsVisible(false);
                                setSuggestionNavigated(false);
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
                        className="w-full bg-[#252525] border-gray-600"
                    />

                    {suggestionsVisible && symbolInput && !isCompletedSymbol(symbolInput) && getSuggestions(symbolInput).length > 0 && (
                        <div className="absolute left-0 right-0 mt-1 border border-gray-700 rounded shadow z-30"
                             style={{backgroundColor: 'rgba(11,18,32,1)', backdropFilter: 'none'}}>
                            <div className="max-h-44 overflow-y-auto"
                                 style={{scrollbarWidth: 'thin' as any, backgroundColor: 'rgba(11,18,32,1)'}}
                                 ref={suggestionsContainerRef}>
                                {getSuggestions(symbolInput).map((sugg, idx) => (
                                    <div key={sugg} data-index={idx} onMouseDown={(ev) => {
                                        ev.preventDefault();

                                        // 추천 선택 시 베이스 심볼만 자동완성 (페어 결합 없이)
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
                                         className={`px-3 py-2 text-sm cursor-pointer hover:bg-gray-800 ${selectedSuggestionIndex === idx ? 'bg-gray-800' : ''}`}>
                                        {sugg}
                                    </div>
                                ))}
                            </div>
                        </div>
                    )}
                </div>

                {/* 페어 선택 드롭다운 */}
                <Select value={selectedPair} onValueChange={(value) => {
                    if (value === '__add_custom__') {
                        setIsAddingCustomPair(true);
                    } else {
                        setSelectedPair(value);
                    }
                }}>
                    <SelectTrigger className="w-32 bg-[#252525] border-gray-600 text-white">
                        <SelectValue/>
                    </SelectTrigger>
                    <SelectContent className="bg-[#1a1a1a] border-gray-700">
                        {/* 기본 페어들 */}
                        {DEFAULT_PAIRS.map(pair => (
                            <SelectItem key={pair} value={pair} className="text-white hover:bg-gray-800">
                                {pair}
                            </SelectItem>
                        ))}

                        {/* 커스텀 페어들 */}
                        {customPairs.length > 0 && (
                            <>
                                <div className="px-2 py-1 text-xs text-gray-500 border-t border-gray-700 mt-1">
                                    커스텀 페어
                                </div>
                                {customPairs.map(pair => (
                                    <div key={pair}
                                         className="flex items-center justify-between px-2 py-1.5 hover:bg-gray-800">
                                        <SelectItem value={pair} className="text-white flex-1 border-0 p-0">
                                            {pair}
                                        </SelectItem>
                                        <button
                                            onClick={(e) => {
                                                e.stopPropagation();
                                                handleRemoveCustomPair(pair);
                                            }}
                                            className="ml-2 text-red-400 hover:text-red-300 text-sm"
                                            title="커스텀 페어 삭제"
                                        >
                                            ×
                                        </button>
                                    </div>
                                ))}
                            </>
                        )}

                        {/* 커스텀 페어 추가 */}
                        <div className="border-t border-gray-700 mt-1">
                            <SelectItem value="__add_custom__" className="text-blue-400 hover:bg-gray-800">
                                + 커스텀 페어 추가
                            </SelectItem>
                        </div>
                    </SelectContent>
                </Select>

                {/* 커스텀 페어 추가 모달 */}
                {isAddingCustomPair && (
                    <div className="fixed inset-0 bg-black/50 flex items-center justify-center z-50" onClick={() => {
                        setIsAddingCustomPair(false);
                        setCustomPairInput('');
                    }}>
                        <div className="bg-[#1a1a1a] border border-gray-700 rounded-lg p-4 min-w-[300px]"
                             onClick={(e) => e.stopPropagation()}>
                            <h3 className="text-white font-semibold mb-3">커스텀 페어 추가</h3>
                            <div className="flex gap-2">
                                <Input
                                    ref={customPairInputRef}
                                    type="text"
                                    placeholder="예: SOL, DOGE"
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
                                    className="flex-1 bg-[#252525] border-gray-600 text-white"
                                />
                                <Button onClick={handleAddCustomPair} className="bg-blue-600 hover:bg-blue-700">
                                    추가
                                </Button>
                                <Button onClick={() => {
                                    setIsAddingCustomPair(false);
                                    setCustomPairInput('');
                                }} className="bg-gray-600 hover:bg-gray-700">
                                    취소
                                </Button>
                            </div>
                        </div>
                    </div>
                )}

                <Button onClick={handleResetSymbols}
                        className="bg-red-600 hover:bg-red-700 text-white px-4">초기화</Button>
                <Button onClick={() => {
                    const suggs = getSuggestions(symbolInput);

                    if (selectedSuggestionIndex >= 0 && suggs.length > 0) {
                        const chosen = suggs[selectedSuggestionIndex];

                        // 자동완성 상태이거나 사용자가 화살표로 추천을 직접 선택했을 때
                        // 추천이 자동완성된 상태(또는 키보드로 직접 이동한 경우)라면
                        if (autocompletedBase && symbolInput === autocompletedBase) {
                            const finalSymbol = selectedPair === '없음' ? autocompletedBase : autocompletedBase + selectedPair;

                            handleAddSymbol(finalSymbol);
                            setAutocompletedBase(null);
                        } else if (chosen && (justAutocompleted || suggestionNavigated)) {
                            // 선택된 추천을 즉시 추가 (선택이 확정된 상태)
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

                    // 상태 초기화
                    setSymbolInput('');
                    setSuggestionsVisible(false);
                    setSelectedSuggestionIndex(-1);
                    setJustAutocompleted(false);
                    setSuggestionNavigated(false);
                }} className="bg-blue-600 hover:bg-blue-700 text-white px-4">추가</Button>
            </div>
        </div>
    );
}
