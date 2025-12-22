import React, {useState, useEffect, useRef} from 'react';
import {Button} from '../ui/button';
import {useWebSocket} from '../Server/WebSocketContext';
import {BarDataConfig, BarDataType, TimeframeUnit, timeframeToString, parseTimeframeString} from '@/types/barData.ts';
import {
    Select,
    SelectContent,
    SelectItem,
    SelectTrigger,
    SelectValue,
} from '@/components/ui/select';
import {Input} from '@/components/ui/input';
import '../Common/LoadingSpinner.css';

/**
 * 전략 에디터 컴포넌트
 * 백테스팅 전략을 편집하고 실행할 수 있는 UI 제공
 */
export default function StrategyEditor() {
    const [logs, setLogs] = useState<Array<{
        level: string;
        message: string;
        timestamp: string | null;
        fileInfo: string | null
    }>>([]);

    // 공통 심볼 리스트 (모든 바 데이터에 공통 적용)
    const [symbolConfigs, setSymbolConfigs] = useState<string[]>([]);

    // 심볼별 로고 상태: { url: string | null, loading: boolean }
    const [symbolLogos, setSymbolLogos] = useState<Record<string, { url: string | null; loading: boolean }>>({});

    // 심볼 입력 및 추천 관련 상태
    const [symbolInput, setSymbolInput] = useState<string>('');
    const [knownSymbols, setKnownSymbols] = useState<string[]>([]);
    const [suggestionsVisible, setSuggestionsVisible] = useState<boolean>(false);
    const [selectedSuggestionIndex, setSelectedSuggestionIndex] = useState<number>(-1);

    // 자동완성 직후 플래그: 자동완성 직후에는 드롭다운을 닫고 입력 변경 시에만 다시 열리게 함
    const [justAutocompleted, setJustAutocompleted] = useState<boolean>(false);

    // 캐럿 위치 (커서 위치) 추적: 자동완성 후 중간 편집 시 추천을 커서 기준으로 계산하기 위함
    const [caretPos, setCaretPos] = useState<number | null>(null);

    // 추천 목록 스크롤 컨테이너 레퍼런스
    const suggestionsContainerRef = useRef<HTMLDivElement | null>(null);

    // 기본 바 데이터 설정 (트레이딩, 돋보기, 참조 1개, 마크 가격)
    const [barDataConfigs, setBarDataConfigs] = useState<BarDataConfig[]>([
        {
            timeframe: {value: null, unit: TimeframeUnit.NULL},
            klinesDirectory: '',
            barDataType: BarDataType.TRADING
        },
        {
            timeframe: {value: null, unit: TimeframeUnit.NULL},
            klinesDirectory: '',
            barDataType: BarDataType.MAGNIFIER
        },
        {
            timeframe: {value: null, unit: TimeframeUnit.NULL},
            klinesDirectory: '',
            barDataType: BarDataType.REFERENCE
        },
        {
            timeframe: {value: null, unit: TimeframeUnit.NULL},
            klinesDirectory: '',
            barDataType: BarDataType.MARK_PRICE
        }
    ]);

    const [useBarMagnifier, setUseBarMagnifier] = useState(true);
    const [configLoaded, setConfigLoaded] = useState(false);
    const logContainerRef = useRef<HTMLDivElement>(null);
    const {ws, projectDirectoryRequested, clearProjectDirectoryRequest} = useWebSocket();
    const [showProjectDialog, setShowProjectDialog] = useState(false);
    const [projectDirectoryInput, setProjectDirectoryInput] = useState('');
    const [projectDirectory, setProjectDirectory] = useState('');

    // 설정의 이전 해시값 저장 (변경 감지용)
    const previousConfigHash = useRef<string>('');

    // 로그 패널 상태
    const [isLogPanelOpen, setIsLogPanelOpen] = useState(false);
    const [logPanelHeight, setLogPanelHeight] = useState(400);
    const [isResizing, setIsResizing] = useState(false);

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


    // 로그 추가 함수
    const addLog = (level: string, message: string, timestamp: string | null = null, fileInfo: string | null = null) => {
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
    };

    // 컴포넌트 마운트 시 editor.json 로드 (ws가 CONNECTING 상태일 때 open 이벤트도 처리)
    useEffect(() => {
        if (!ws) return;

        // 백엔드로부터 사용 가능한 심볼 목록을 가져옴 (/api/symbols 사용)
        (async () => {
            try {
                const res = await fetch('/api/symbols?include=popular,web');
                if (!res.ok) {
                    return;
                }

                const data = await res.json();

                if (Array.isArray(data.symbols)) {
                    setKnownSymbols(data.symbols.map((s: string) => String(s).toUpperCase()));
                }
            } catch (e) {
                // 무시
            }
        })();

        const trySendLoad = () => {
            if (ws.readyState === WebSocket.OPEN && !configLoaded) {
                try {
                    ws.send(JSON.stringify({action: 'loadEditorConfig'}));
                } catch (e) {
                    console.error('loadEditorConfig 전송 실패:', e);
                }
            }
        };

        // 즉시 시도
        trySendLoad();

        // 아직 OPEN이 아니면 open 이벤트에서 재시도
        const onOpen = () => {
            trySendLoad();
        };

        ws.addEventListener('open', onOpen);

        return () => {
            try {
                ws.removeEventListener('open', onOpen);
            } catch (e) {
                // 무시
            }
        };
    }, [ws, configLoaded]);

    // 로그 패널 상태 변경 시 자동 저장 (디바운스)
    useEffect(() => {
        if (!configLoaded) {
            return;
        }

        const timer = setTimeout(() => {
            saveConfig();
        }, 300);

        return () => clearTimeout(timer);
    }, [isLogPanelOpen, logPanelHeight]);

    // WebSocket 메시지 수신
    useEffect(() => {
        if (!ws) return;

        const handleMessage = (event: MessageEvent) => {
            try {
                const data = JSON.parse(event.data);

                if (data.action === 'backtestingLog') {
                    addLog(data.level, data.message, data.timestamp, data.fileInfo);
                } else if (data.action === 'requestProjectDirectory') {
                    // 서버가 프로젝트 폴더를 요청하면 팝업을 띄우고 경로를 전송
                    setShowProjectDialog(true);
                } else if (data.action === 'projectDirectoryInvalid') {
                    // 서버에서 이미 경고 로그를 broadcastLog로 전송하므로 여기서는 로그 불필요
                    // 다시 팝업을 띄워 재입력 유도
                    setShowProjectDialog(true);
                } else if (data.action === 'editorConfigLoaded') {
                    const config = data.config;

                    if (config && typeof config === 'object' && (config['에디터 설정'] || config['엔진 설정'])) {
                        // 에디터 설정
                        const editor = config['에디터 설정'] || {};

                        if (typeof editor['로그 패널 열림'] === 'boolean') {
                            setIsLogPanelOpen(editor['로그 패널 열림']);
                        } else if (typeof editor['로그 패널 열림'] === 'string') {
                            setIsLogPanelOpen(editor['로그 패널 열림'] === "열림");
                        }

                        if (typeof editor['로그 패널 높이'] === 'number') {
                            setLogPanelHeight(editor['로그 패널 높이']);
                        }

                        // 엔진 설정
                        const engine = config['엔진 설정'] || {};

                        if (typeof engine['프로젝트 폴더'] === 'string') {
                            setProjectDirectory(engine['프로젝트 폴더']);
                        }

                        if (typeof engine['바 돋보기 기능'] === 'boolean') {
                            setUseBarMagnifier(engine['바 돋보기 기능']);
                        } else if (typeof engine['바 돋보기 기능'] === 'string') {
                            setUseBarMagnifier(engine['바 돋보기 기능'] === "활성화");
                        }

                        // 심볼 설정
                        if (Array.isArray(config['심볼 설정'])) {
                            setSymbolConfigs(config['심볼 설정']);
                        }

                        // 바 데이터 설정
                        if (Array.isArray(config['바 데이터 설정'])) {
                            const loadedConfigs = config['바 데이터 설정'].map((barDataConfig: any) => ({
                                timeframe: parseTimeframeString(barDataConfig.timeframe),
                                klinesDirectory: barDataConfig.klinesDirectory,
                                barDataType: barDataConfig.barDataType
                            }));

                            setBarDataConfigs(loadedConfigs);
                        }
                    }

                    setConfigLoaded(true);
                }
            } catch (err) {
                console.error('메시지 파싱 오류:', err);
            }
        };

        ws.addEventListener('message', handleMessage);

        return () => ws.removeEventListener('message', handleMessage);
    }, [ws]);

    // 서버가 프로젝트 폴더 입력을 요청한 경우(서버가 먼저 보낼 수 있으므로 플래그로 처리)
    useEffect(() => {
        if (projectDirectoryRequested && !showProjectDialog) {
            setShowProjectDialog(true);
        }
    }, [projectDirectoryRequested, showProjectDialog]);

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

    // 로그가 추가될 때마다 스크롤을 맨 아래로
    useEffect(() => {
        if (logContainerRef.current) {
            logContainerRef.current.scrollTop = logContainerRef.current.scrollHeight;
        }
    }, [logs]);

    // editor.json 저장 함수
    const saveConfig = () => {
        if (!ws || ws.readyState !== WebSocket.OPEN) {
            return;
        }

        ws.send(JSON.stringify({
            action: 'saveEditorConfig',
            config: {
                '에디터 설정': {
                    '로그 패널 열림': isLogPanelOpen,
                    '로그 패널 높이': logPanelHeight,
                },
                '엔진 설정': {
                    '프로젝트 폴더': projectDirectory,
                    '바 돋보기 기능': useBarMagnifier,
                },
                '심볼 설정': symbolConfigs,
                '바 데이터 설정': barDataConfigs.map(config => ({
                    timeframe: timeframeToString(config.timeframe),
                    klinesDirectory: config.klinesDirectory,
                    barDataType: config.barDataType
                })),
            }
        }));
    };

    // useBarMagnifier 변경 시 자동 저장 (초기 로드 제외)
    useEffect(() => {
        if (!configLoaded) {
            return;
        }

        const timer = setTimeout(() => {
            saveConfig();
        }, 300);
        return () => clearTimeout(timer);
    }, [useBarMagnifier]);

    // 바 데이터 설정 해시 계산 (바 돋보기 상태 + 심볼 리스트 + 바 데이터 설정)
    const calculateConfigHash = () => {
        const config = {
            useBarMagnifier: useBarMagnifier,
            symbols: [...symbolConfigs],  // 심볼 추가 순서가 의미 있으므로 순서를 보존하여 비교
            barData: barDataConfigs.map(c => ({
                timeframe: timeframeToString(c.timeframe),
                klinesDirectory: c.klinesDirectory,
                barDataType: c.barDataType
            }))
        };

        return JSON.stringify(config);
    };

    // 입력값이 완결된 USDT 심볼인지 확인리 (캐럿이 끝에 있어야 하는 조건)
    const isCompletedUSDT = (input: string) => {
        const s = (input || '').toUpperCase().trim();
        if (!s.endsWith('USDT')) {
            return false;
        }

        if (symbolConfigs.includes(s)) {
            return false;
        }

        // caretPos가 null이면 사용자가 끝에 있다고 가정 (fallback)
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

        // 커서 위치 기준으로 prefix를 사용 (중간 편집 시 그 위치까지의 텍스트를 기준으로 추천)
        const pos = typeof caretPos === 'number' && caretPos >= 0 ? Math.min(caretPos, q.length) : q.length;
        const prefix = q.slice(0, pos).trim();
        if (!prefix) {
            return [];
        }

        // 알려진 심볼 중에서 prefix로 시작하는 것(중복 등록 방지)
        let matches = knownSymbols.filter(s => s.startsWith(prefix) && !symbolConfigs.includes(s));

        // 정렬: 접미사(길이)가 적은 것 우선, 동일 길이에서는 알파벳 순
        matches.sort((a, b) => {
            const sa = a.length - prefix.length;
            const sb = b.length - prefix.length;
            if (sa !== sb) return sa - sb;
            return a.localeCompare(b);
        });

        // 항상 우선으로 보여줄 USDT 후보를 가장 위에 둔다(중복 제거)
        const suggestions: string[] = [];
        if (!prefix.endsWith('USDT')) {
            const cand = (prefix + 'USDT').toUpperCase();

            if (!symbolConfigs.includes(cand)) {
                suggestions.push(cand);
            }
        } else {
            const cand = prefix.toUpperCase();

            if (!symbolConfigs.includes(cand)) {
                suggestions.push(cand);
            }
        }

        for (const m of matches) {
            if (!suggestions.includes(m)) {
                suggestions.push(m);
            }

            if (suggestions.length >= 50) {
                break;
            }
        }

        return suggestions;
    };

    // 심볼 추가
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

    // 심볼 삭제
    const handleRemoveSymbol = (symbolIndex: number) => {
        const removedSymbol = symbolConfigs[symbolIndex];
        setSymbolConfigs(prev => prev.filter((_, i) => i !== symbolIndex));

        // 로고 캐시 정리
        setSymbolLogos(prev => {
            const copy = {...prev};
            if (removedSymbol && copy[removedSymbol]) {
                delete copy[removedSymbol];
            }
            return copy;
        });
    };

    // symbolConfigs 변경 시 자동 저장 (초기 로드 제외)
    useEffect(() => {
        if (!configLoaded) {
            return;
        }

        const timer = setTimeout(() => {
            saveConfig();
        }, 300);

        return () => clearTimeout(timer);
    }, [symbolConfigs]);

    // 특정 심볼의 로고를 백엔드에서 가져오는 헬퍼
    const fetchLogoForSymbol = async (symbol: string) => {
        // 이미 로딩 중이면 중복 요청 방지
        const info = symbolLogos[symbol];
        if (info && info.loading) return;

        setSymbolLogos(prev => ({...prev, [symbol]: {url: null, loading: true}}));

        try {
            const res = await fetch(`/api/get-logo?symbol=${encodeURIComponent(symbol)}`);
            const data = await res.json();
            const url = data && data.logoUrl ? data.logoUrl : null;

            setSymbolLogos(prev => ({...prev, [symbol]: {url, loading: false}}));
        } catch (e) {
            // 실패 시에도 로딩 상태 종료. url null이면 fallback UI가 표시됨
            setSymbolLogos(prev => ({...prev, [symbol]: {url: null, loading: false}}));
        }
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
    // 참조 바 데이터 추가
    const handleAddReferenceBar = () => {
        // 프로젝트 디렉터리가 설정되어 있으면 기본 klines 폴더로 추론
        const defaultKlinesDir = projectDirectory ? `${projectDirectory}/Data/Continuous Klines` : '';

        const newConfig: BarDataConfig = {
            timeframe: {value: null, unit: TimeframeUnit.NULL},
            klinesDirectory: defaultKlinesDir,
            barDataType: BarDataType.REFERENCE
        };

        // MARK_PRICE 바로 앞에 삽입 (없으면 끝에 추가)
        const markPriceIndex = barDataConfigs.findIndex(c => c.barDataType === BarDataType.MARK_PRICE);
        const newConfigs = [...barDataConfigs];
        const insertIndex = markPriceIndex === -1 ? newConfigs.length : markPriceIndex;
        newConfigs.splice(insertIndex, 0, newConfig);
        setBarDataConfigs(newConfigs);

        // 설정 저장
        setTimeout(() => {
            saveConfig();
        }, 300);
    };

    // 참조 바 데이터 삭제 (모든 참조 바 삭제 가능)
    const handleRemoveReferenceBar = (index: number) => {
        // 삭제 대상이 참조 바인지 확인
        if (barDataConfigs[index] && barDataConfigs[index].barDataType === BarDataType.REFERENCE) {
            const newConfigs = barDataConfigs.filter((_, i) => i !== index);
            setBarDataConfigs(newConfigs);

            // 설정 저장
            setTimeout(() => {
                saveConfig();
            }, 300);
        }
    };

    // 바 데이터 설정 업데이트
    const updateBarDataConfig = (index: number, updates: Partial<BarDataConfig>) => {
        const newConfigs = [...barDataConfigs];
        newConfigs[index] = {...newConfigs[index], ...updates};
        setBarDataConfigs(newConfigs);

        // 바 데이터 설정 변경 시 설정 저장
        if (updates.klinesDirectory !== undefined || updates.timeframe !== undefined) {
            setTimeout(() => {
                if (ws && ws.readyState === WebSocket.OPEN) {
                    ws.send(JSON.stringify({
                        action: 'saveEditorConfig',
                        config: {
                            '에디터 설정': {
                                '로그 패널 열림': isLogPanelOpen ? "열림" : "닫힘",
                                '로그 패널 높이': logPanelHeight,
                            },
                            '엔진 설정': {
                                '프로젝트 폴더': projectDirectory,
                                '바 돋보기 기능': useBarMagnifier ? "활성화" : "비활성화",
                            },
                            '심볼 설정': symbolConfigs,
                            '바 데이터 설정': newConfigs.map(config => ({
                                timeframe: timeframeToString(config.timeframe),
                                klinesDirectory: config.klinesDirectory,
                                barDataType: config.barDataType
                            })),
                        }
                    }));
                }
            }, 500);
        }
    };

    // 백테스팅 실행
    const handleRunSingleBacktesting = () => {
        if (!ws || ws.readyState !== WebSocket.OPEN) {
            addLog('ERROR', 'WebSocket 연결이 없습니다.');
            return;
        }

        // 백테스팅 실행 시 로그 패널이 닫혀있으면 자동으로 열기
        if (!isLogPanelOpen) {
            setIsLogPanelOpen(true);
        }

        const configsToValidate = barDataConfigs.filter(config =>
            config.barDataType !== BarDataType.MAGNIFIER || useBarMagnifier
        );

        if (symbolConfigs.length === 0) {
            addLog('ERROR', '백테스팅을 실행하려면 최소 1개의 심볼이 필요합니다.');
            return;
        }

        for (const config of configsToValidate) {
            if (!config.klinesDirectory.trim()) {
                addLog('ERROR', `${config.barDataType} 바 데이터 폴더를 입력해주세요.`);
                return;
            }
        }

        // 타임프레임 유효성 검사: value 또는 unit이 비어있으면 로그 후 중단
        for (const config of configsToValidate) {
            const tf = config.timeframe;

            if (!tf || tf.value === null) {
                const msg = `${config.barDataType} 바 데이터의 타임프레임 값이 비어있습니다.`;
                addLog('ERROR', msg);

                return;
            }

            if (!tf || tf.unit === TimeframeUnit.NULL) {
                const msg = `${config.barDataType} 바 데이터의 타임프레임 단위가 비어있습니다.`;
                addLog('ERROR', msg);

                return;
            }
        }

        const configsToSend = barDataConfigs.filter(config =>
            config.barDataType !== BarDataType.MAGNIFIER || useBarMagnifier
        );

        // 현재 바 데이터 설정의 해시값 계산
        const currentHash = calculateConfigHash();

        // 해시가 없거나 (최초 실행) 해시가 변경된 경우에만 clear and add
        const needsClearAndAdd = previousConfigHash.current === '' ||
            currentHash !== previousConfigHash.current;

        ws.send(JSON.stringify({
            action: 'runSingleBacktesting',
            useBarMagnifier: useBarMagnifier,
            clearAndAddBarData: needsClearAndAdd,
            symbolConfigs: symbolConfigs,
            barDataConfigs: configsToSend.map(config => ({
                timeframe: timeframeToString(config.timeframe),
                klinesDirectory: config.klinesDirectory,
                barDataType: config.barDataType
            }))
        }));

        // 현재 해시값 저장
        previousConfigHash.current = currentHash;
    };

    const handleProvideProjectDirectory = () => {
        if (!ws || ws.readyState !== WebSocket.OPEN) {
            return;
        }

        if (!projectDirectoryInput || projectDirectoryInput.trim() === '') {
            addLog('ERROR', '프로젝트 폴더 경로를 입력해주세요.');
            return;
        }

        ws.send(JSON.stringify({action: 'provideProjectDirectory', projectDirectory: projectDirectoryInput}));

        setProjectDirectory(projectDirectoryInput);
        setShowProjectDialog(false);

        // 서버 요청 플래그 해제
        try {
            clearProjectDirectoryRequest();
        } catch (e) {
            // 무시
        }
    };

    // 로그 색상
    const getLogColor = (level: string) => {
        switch (level) {
            case 'DEBUG':
                return 'rgb(156, 220, 254)';
            case 'WARN':
                return 'rgb(229, 192, 123)';
            case 'ERROR':
                return 'rgb(224, 108, 117)';
            case 'BALANCE':
                return 'rgb(128, 128, 128)';
            case 'INFO':
            default:
                return '#ffffff';
        }
    };

    // 리사이즈 핸들러
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
    }, [isResizing]);

    return (
        <div className="flex flex-col h-full w-full">
            {showProjectDialog && (
                <div className="fixed inset-0 z-50 flex items-center justify-center">
                    <div className="absolute inset-0 bg-black/60"/>
                    <div className="relative bg-[#0f1724] rounded-lg p-6 w-[520px] border border-gray-600 z-10">
                        <h3 className="text-lg font-semibold text-white mb-3">프로젝트 폴더 입력</h3>
                        <p className="text-sm text-gray-300 mb-3">프로젝트 폴더를 입력하세요. 해당 폴더에 Backboard.exe가 존재해야 합니다.</p>
                        <Input type="text" value={projectDirectoryInput}
                               onChange={(e) => setProjectDirectoryInput(e.currentTarget.value)}
                               placeholder="프로젝트 폴더 경로"
                               className="mb-4 bg-[#0b1220] border-gray-600"/>
                        <div className="flex justify-end gap-2">
                            <Button onClick={handleProvideProjectDirectory} className="bg-blue-600">입력</Button>
                        </div>
                    </div>
                </div>
            )}

            {/* 메인 콘텐츠 영역 */}
            <div className="flex-1 overflow-y-auto p-6 pb-2"
                 style={{height: isLogPanelOpen ? `calc(100% - ${logPanelHeight}px)` : '100%'}}>
                <div className="flex items-center justify-between mb-4">
                    <h1 className="text-2xl font-bold text-white">전략 에디터</h1>
                    <div className="flex items-center gap-2">
                        <Button
                            onClick={() => setIsLogPanelOpen(!isLogPanelOpen)}
                            className="bg-gray-700 hover:bg-gray-600 text-white font-semibold px-4 py-2 rounded-lg transition-colors"
                        >
                            {isLogPanelOpen ? '로그 숨기기' : '로그 표시'}
                        </Button>
                        <Button
                            onClick={handleRunSingleBacktesting}
                            className="bg-blue-600 hover:bg-blue-700 text-white font-semibold px-6 py-2 rounded-lg transition-colors"
                        >
                            백테스팅 실행
                        </Button>
                    </div>
                </div>

                {/* 공통 심볼 리스트 */}
                <div className="bg-[#1a1a1a] rounded-lg border border-gray-700 p-4 mb-4">
                    <h2 className="text-lg font-semibold text-white mb-3">심볼 설정</h2>
                    <div className="flex flex-wrap gap-2 mb-3 min-h-[32px]">
                        {symbolConfigs.map((symbol, symbolIndex) => {
                            const info = symbolLogos[symbol];
                            return (
                                <div
                                    key={symbolIndex}
                                    className="bg-blue-600 text-white px-3 py-1 rounded-full text-sm flex items-center gap-2"
                                >
                                    {/* 로고 또는 로딩 스피너 */}
                                    {info && info.loading ? (
                                        <div
                                            className="chart-loading-indicator"
                                            style={{
                                                width: '18px',
                                                height: '18px',
                                                border: '2px solid rgba(20,20,20,0.15) !important',
                                                borderTopColor: '#FFD700',
                                                boxShadow: 'none'
                                            }}
                                        />
                                    ) : info && info.url ? (
                                        <img src={info.url} alt={`${symbol} logo`} className="w-5 h-5 rounded-full"/>
                                    ) : (
                                        <div className="w-5 h-5 bg-white/20 rounded-full"/>
                                    )}

                                    <span className="truncate max-w-[8rem]">{symbol}</span>

                                    <button
                                        onClick={() => handleRemoveSymbol(symbolIndex)}
                                        className="hover:text-red-300 text-base leading-none"
                                        title="심볼 삭제"
                                    >
                                        ×
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
                                placeholder="심볼 이름 입력 후 Enter 키 또는 추천 클릭"
                                value={symbolInput}
                                onChange={(e: React.ChangeEvent<HTMLInputElement>) => {
                                    // 입력 필드의 현재 캐럿 위치를 저장
                                    const el = document.getElementById('strategy-symbol-input') as HTMLInputElement | null;
                                    const selStart = el ? el.selectionStart : null;

                                    const v = e.currentTarget.value.toUpperCase();
                                    setSymbolInput(v);

                                    // caretPos 업데이트(비동기적으로 최신 값 반영)
                                    if (selStart !== null) {
                                        setCaretPos(Math.min(selStart, v.length));
                                    } else {
                                        setCaretPos(v.length);
                                    }

                                    // 사용자가 입력을 변경하면 자동완성 플래그 해제 및 추천 다시 노출
                                    setJustAutocompleted(false);
                                    setSuggestionsVisible(true);
                                    setSelectedSuggestionIndex(-1);

                                    // 캐럿 위치 복원 (비동기)
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

                                        // 입력값이 이미 완결된 USDT 심볼(그리고 caret이 끝에 있을 때)이면 드롭다운 없이 즉시 추가
                                        const normalized = (symbolInput || '').toUpperCase().trim();
                                        const caretAtEnd = typeof caretPos !== 'number' ? true : caretPos === normalized.length;

                                        if (normalized.endsWith('USDT') && caretAtEnd && !symbolConfigs.includes(normalized)) {
                                            handleAddSymbol(normalized);

                                            setSymbolInput('');
                                            setSuggestionsVisible(false);
                                            setSelectedSuggestionIndex(-1);
                                            setJustAutocompleted(false);
                                            return;
                                        }

                                        const suggs = getSuggestions(symbolInput);

                                        if (selectedSuggestionIndex >= 0) {
                                            const chosen = suggs[selectedSuggestionIndex];

                                            if (chosen) {
                                                if (symbolInput === chosen) {
                                                    // 입력값이 이미 추천과 동일한 경우
                                                    // 첫 Enter는 자동완성 플래그를 설정하고 드롭다운만 닫음
                                                    if (justAutocompleted) {
                                                        // 이미 자동완성된 상태에서 Enter를 다시 누르면 실제 추가
                                                        handleAddSymbol(chosen);
                                                        setSymbolInput('');
                                                        setSuggestionsVisible(false);
                                                        setSelectedSuggestionIndex(-1);
                                                        setJustAutocompleted(false);
                                                    } else {
                                                        setJustAutocompleted(true);
                                                        setSuggestionsVisible(false);
                                                        setSelectedSuggestionIndex(suggs.indexOf(chosen));
                                                    }
                                                } else {
                                                    // 자동완성만 수행: 드롭다운 닫고 플래그 설정
                                                    setSymbolInput(chosen);
                                                    setCaretPos(chosen.length);
                                                    setJustAutocompleted(true);
                                                    setSuggestionsVisible(false);

                                                    const newIndex = suggs.indexOf(chosen);
                                                    setSelectedSuggestionIndex(newIndex >= 0 ? newIndex : 0);

                                                    // 입력으로 포커스 이동하여 커서를 끝으로 이동
                                                    requestAnimationFrame(() => {
                                                        const inputEl = document.getElementById('strategy-symbol-input') as HTMLInputElement | null;
                                                        if (inputEl) {
                                                            try {
                                                                inputEl.focus();
                                                                inputEl.setSelectionRange(chosen.length, chosen.length);
                                                            } catch (e) {
                                                            }
                                                        }
                                                    });
                                                }
                                            }
                                        } else {
                                            // 선택된 추천 인덱스가 없는 경우에도 입력값이 추천 목록에 정확히 일치하면
                                            // 첫 Enter는 자동완성 플래그를 설정하여 드롭다운을 닫고, 두 번째 Enter에서 실제 추가하도록 함.
                                            const allSuggs = getSuggestions(symbolInput);
                                            const exactMatchIndex = allSuggs.indexOf(symbolInput);

                                            if (exactMatchIndex >= 0) {
                                                // 이미 자동완성 플래그가 설정되어 있으면 실제 추가
                                                if (justAutocompleted) {
                                                    handleAddSymbol(symbolInput);
                                                    setSymbolInput('');
                                                    setSuggestionsVisible(false);
                                                    setSelectedSuggestionIndex(-1);
                                                    setJustAutocompleted(false);
                                                } else {
                                                    // 첫 Enter: 자동완성 플래그만 설정하고 드롭다운 닫기
                                                    setJustAutocompleted(true);
                                                    setSuggestionsVisible(false);
                                                    setSelectedSuggestionIndex(exactMatchIndex);
                                                }
                                            } else {
                                                // 추천에 없으면 즉시 추가
                                                handleAddSymbol(symbolInput);
                                                setSymbolInput('');
                                                setSuggestionsVisible(false);
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
                                    // blur 시 약간의 딜레이를 두어 클릭 이벤트가 처리될 수 있게 함
                                    setTimeout(() => setSuggestionsVisible(false), 150);
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
                                    // 클릭으로 캐럿 이동 시 추천이 존재하면 드롭다운 열기
                                    const el = e.currentTarget as HTMLInputElement;
                                    const pos = el.selectionStart ?? el.value.length;
                                    setCaretPos(pos);

                                    // 직접 prefix 기반으로 추천 존재 여부 판단하여 드롭다운 표시
                                    const prefix = (el.value || '').slice(0, pos).toUpperCase().trim();
                                    const hasSuggestion = prefix.length > 0 && (knownSymbols.some(s => s.startsWith(prefix) && !symbolConfigs.includes(s)) || (!prefix.endsWith('USDT') && !symbolConfigs.includes((prefix + 'USDT').toUpperCase())));

                                    if (hasSuggestion) {
                                        setSuggestionsVisible(true);
                                    }
                                }}
                                className="w-full bg-[#252525] border-gray-600"
                            />

                            {/* 추천 드롭다운 */}
                            {suggestionsVisible && symbolInput && !isCompletedUSDT(symbolInput) && getSuggestions(symbolInput).length > 0 && (
                                <div className="absolute left-0 right-0 mt-1 border border-gray-700 rounded shadow z-30"
                                     style={{backgroundColor: 'rgba(11,18,32,1)', backdropFilter: 'none'}}>
                                    <div className="max-h-44 overflow-y-auto"
                                         style={{scrollbarWidth: 'thin' as any, backgroundColor: 'rgba(11,18,32,1)'}}
                                         ref={suggestionsContainerRef}>
                                        {getSuggestions(symbolInput).map((sugg, idx) => (
                                            <div
                                                key={sugg}
                                                data-index={idx}
                                                onMouseDown={(ev) => {
                                                    // mouseDown에서 preventDefault로 blur에서 닫히는 걸 방지
                                                    ev.preventDefault();

                                                    // 클릭 시 자동완성만 수행(즉시 추가하지 않음)
                                                    setSymbolInput(sugg);
                                                    setCaretPos(sugg.length);
                                                    setJustAutocompleted(true);
                                                    setSuggestionsVisible(false);
                                                    setSelectedSuggestionIndex(idx);

                                                    // 입력으로 포커스 이동 및 캐럿을 끝으로 이동
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
                                                className={`px-3 py-2 text-sm cursor-pointer hover:bg-gray-800 ${selectedSuggestionIndex === idx ? 'bg-gray-800' : ''}`}
                                            >
                                                {sugg}
                                            </div>
                                        ))}
                                    </div>
                                </div>
                            )}
                        </div>
                        <Button
                            onClick={() => {
                                if (selectedSuggestionIndex >= 0) {
                                    const chosen = getSuggestions(symbolInput)[selectedSuggestionIndex];
                                    if (chosen) {
                                        handleAddSymbol(chosen);
                                    }
                                } else {
                                    const val = (symbolInput || '').toUpperCase().trim();
                                    if (val) {
                                        handleAddSymbol(val);
                                    }
                                }

                                setSymbolInput('');
                                setSuggestionsVisible(false);
                            }}
                            className="bg-blue-600 hover:bg-blue-700 text-white px-4"
                        >
                            추가
                        </Button>
                    </div>
                </div>

                {/* 바 데이터 설정 */}
                <div className="bg-[#1a1a1a] rounded-lg border border-gray-700 p-4 mb-4">
                    <div className="flex items-center justify-between mb-4">
                        <h2 className="text-lg font-semibold text-white">바 데이터 설정</h2>
                        <Button
                            onClick={handleAddReferenceBar}
                            className="bg-green-600 hover:bg-green-700 text-white px-3 py-1.5 text-sm rounded-lg flex items-center gap-1"
                        >
                            <span className="text-lg leading-none">+</span>
                            <span>참조 바 추가</span>
                        </Button>
                    </div>

                    <div className="grid grid-cols-2 gap-4">
                        {barDataConfigs.map((config, index) => {
                            const isDisabled = config.barDataType === BarDataType.MAGNIFIER && !useBarMagnifier;
                            const canDelete = config.barDataType === BarDataType.REFERENCE;

                            const barDataTypeLabel = {
                                [BarDataType.TRADING]: '트레이딩 바 데이터',
                                [BarDataType.MAGNIFIER]: '돋보기 바 데이터',
                                [BarDataType.REFERENCE]: '참조 바 데이터',
                                [BarDataType.MARK_PRICE]: '마크 가격 바 데이터'
                            }[config.barDataType];

                            return (
                                <div
                                    key={index}
                                    className={`bg-[#252525] rounded-lg p-4 border border-gray-600 relative ${
                                        isDisabled ? 'opacity-50' : ''
                                    }`}
                                >
                                    <div className="flex items-center justify-between mb-3">
                                        <h3 className="text-sm font-semibold text-white">{barDataTypeLabel}</h3>
                                        <div className="flex items-center gap-2">
                                            {config.barDataType === BarDataType.MAGNIFIER && (
                                                <label className="flex items-center gap-2 text-xs text-gray-300">
                                                    <input
                                                        type="checkbox"
                                                        checked={useBarMagnifier}
                                                        onChange={(e) => {
                                                            setUseBarMagnifier(e.target.checked);
                                                        }}
                                                        className="w-3.5 h-3.5"
                                                    />
                                                    바 돋보기 기능
                                                </label>
                                            )}
                                            {canDelete && (
                                                <button
                                                    onClick={() => handleRemoveReferenceBar(index)}
                                                    className="text-red-500 hover:text-red-400 text-xl leading-none px-1"
                                                    title="참조 바 삭제"
                                                >
                                                    ×
                                                </button>
                                            )}
                                        </div>
                                    </div>

                                    <div className="space-y-3">
                                        {/* 타임프레임 */}
                                        <div>
                                            <label className="block text-xs text-gray-400 mb-1">타임프레임</label>
                                            <div className="grid grid-cols-2 gap-2">
                                                <Input
                                                    type="text"
                                                    inputMode="numeric"
                                                    pattern="[0-9]*"
                                                    value={config.timeframe.value ?? ''}
                                                    onKeyDown={(e: React.KeyboardEvent<HTMLInputElement>) => {
                                                        // Ctrl/Cmd 조합키는 브라우저 기본 동작(undo/redo/copy/paste 등)을 허용
                                                        if (e.ctrlKey || e.metaKey) {
                                                            return;
                                                        }

                                                        // 허용되는 키: 숫자, Backspace, Delete, Arrow, Tab
                                                        const allowed = /^(?:[0-9]|Backspace|Delete|ArrowLeft|ArrowRight|Tab|Home|End)$/;
                                                        if (!allowed.test(e.key)) {
                                                            e.preventDefault();
                                                        }
                                                    }}
                                                    onChange={(e: React.ChangeEvent<HTMLInputElement>) => {
                                                        // 숫자 외 문자 제거
                                                        const raw = e.target.value || '';
                                                        const digits = raw.replace(/\D/g, '');
                                                        // 선행 0 제거
                                                        const noLeading = digits.replace(/^0+/, '');

                                                        const finalValue = noLeading === '' ? null : parseInt(noLeading, 10);

                                                        updateBarDataConfig(index, {
                                                            timeframe: {
                                                                ...config.timeframe,
                                                                value: finalValue
                                                            }
                                                        });
                                                    }}
                                                    disabled={isDisabled}
                                                    className="bg-[#1a1a1a] border-gray-600 text-sm"
                                                />
                                                <Select
                                                    value={config.timeframe.unit}
                                                    onValueChange={(value: TimeframeUnit) => updateBarDataConfig(index, {
                                                        timeframe: {...config.timeframe, unit: value as TimeframeUnit}
                                                    })}
                                                    disabled={isDisabled}
                                                >
                                                    <SelectTrigger
                                                        className="w-full bg-[#1a1a1a] border-gray-600 text-sm">
                                                        <SelectValue/>
                                                    </SelectTrigger>
                                                    <SelectContent>
                                                        <SelectItem value={TimeframeUnit.MILLISECOND}>밀리초</SelectItem>
                                                        <SelectItem value={TimeframeUnit.SECOND}>초</SelectItem>
                                                        <SelectItem value={TimeframeUnit.MINUTE}>분</SelectItem>
                                                        <SelectItem value={TimeframeUnit.HOUR}>시간</SelectItem>
                                                        <SelectItem value={TimeframeUnit.DAY}>일</SelectItem>
                                                        <SelectItem value={TimeframeUnit.WEEK}>주</SelectItem>
                                                        <SelectItem value={TimeframeUnit.MONTH}>개월</SelectItem>
                                                    </SelectContent>
                                                </Select>
                                            </div>
                                        </div>

                                        {/* 폴더 경로 */}
                                        <div>
                                            <label className="block text-xs text-gray-400 mb-1">폴더 경로</label>
                                            <Input
                                                type="text"
                                                value={config.klinesDirectory}
                                                onChange={(e: React.ChangeEvent<HTMLInputElement>) => updateBarDataConfig(index, {klinesDirectory: e.target.value})}
                                                placeholder="바 데이터 폴더 경로 입력"
                                                disabled={isDisabled}
                                                className="w-full bg-[#1a1a1a] border-gray-600 text-xs"
                                            />
                                        </div>
                                    </div>
                                </div>
                            );
                        })}
                    </div>
                </div>
            </div>

            {/* 로그 패널 (하단) */}
            {isLogPanelOpen && (
                <>
                    {/* 리사이즈 바 */}
                    <div
                        className="h-1 bg-gray-700 hover:bg-blue-500 cursor-ns-resize transition-colors"
                        onMouseDown={handleResizeMouseDown}
                        style={{userSelect: 'none'}}
                    />

                    {/* 로그 패널 */}
                    <div
                        className="bg-[#1a1a1a] border-t border-gray-700 flex flex-col"
                        style={{height: `${logPanelHeight}px`}}
                    >
                        <div className="px-4 py-2 border-b border-gray-700 flex items-center justify-between">
                            <h2 className="text-sm font-semibold text-gray-300">실행 로그</h2>
                            <button
                                onClick={() => setIsLogPanelOpen(false)}
                                className="text-gray-400 hover:text-white text-lg leading-none px-2"
                            >
                                ×
                            </button>
                        </div>
                        <div
                            ref={logContainerRef}
                            className="overflow-y-auto p-4 font-mono text-sm flex-1"
                            style={{fontFamily: "'Inter', 'Pretendard', monospace"}}
                        >
                            {logs.length === 0 ? (
                                <div className="text-gray-500 text-center mt-8">
                                    백테스팅을 실행하면 로그가 여기에 표시됩니다.
                                </div>
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
                                            }}>
                                                {'─'.repeat(150)}
                                            </div>
                                        </div>
                                    ) : (
                                        <div key={index} className="mb-1">
                                            {log.timestamp && (
                                                <span style={{color: 'rgb(106, 153, 85)'}}>[{log.timestamp}]</span>
                                            )}
                                            {' '}
                                            <span className="font-semibold"
                                                  style={{color: getLogColor(log.level)}}>[{log.level}]</span>
                                            {' '}
                                            {log.fileInfo && (
                                                <>
                                                    <span style={{color: 'rgb(86, 156, 214)'}}>[{log.fileInfo}]</span>
                                                </>
                                            )}
                                            {' '}
                                            <span style={{color: getLogColor(log.level)}}>{log.message}</span>
                                        </div>
                                    )
                                ))
                            )}
                        </div>
                    </div>
                </>
            )}
        </div>
    );
}
