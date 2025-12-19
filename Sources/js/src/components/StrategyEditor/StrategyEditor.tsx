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

    // 기본 4개 바 데이터 설정 (트레이딩, 돋보기, 참조, 마크 가격)
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
    const {ws} = useWebSocket();
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

                const m = line.match(/([^\s)]+?\.(tsx|js))(?:\?[^:\s]*)?(?::\d+:\d+)?$/i);
                if (m) {
                    let fp = m[1].split('?')[0];
                    // 번들러 해시 제거 (예: name-BXO9bpex.js -> name.js)
                    fp = fp.replace(/-([A-Za-z0-9]+)(?=\.js$)/, '');

                    // .js 파일은 원본 표시를 위해 .tsx로 변경
                    if (fp.endsWith('.js')) {
                        fp = fp.replace(/\.js$/, '.tsx');
                    }

                    const parts = fp.split(/[\\/]/);
                    return parts[parts.length - 1];
                }
            }
        } catch (e) {
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

        setLogs(prev => [...prev, {level, message, timestamp: finalTimestamp, fileInfo}]);
    };

    // 컴포넌트 마운트 시 editor.json 로드
    useEffect(() => {
        if (!ws || ws.readyState !== WebSocket.OPEN || configLoaded) {
            return;
        }

        ws.send(JSON.stringify({action: 'loadEditorConfig'}));
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

    // 바 데이터 설정 해시 계산 (심볼 리스트 + 바 데이터 설정)
    const calculateConfigHash = () => {
        const config = {
            symbols: [...symbolConfigs],  // 심볼 추가 순서가 의미 있으므로 순서를 보존하여 비교
            barData: barDataConfigs.map(c => ({
                timeframe: timeframeToString(c.timeframe),
                klinesDirectory: c.klinesDirectory,
                barDataType: c.barDataType
            }))
        };

        return JSON.stringify(config);
    };

    // 심볼 추가
    const handleAddSymbol = (symbol: string) => {
        if (!symbol.trim()) {
            return;
        }

        if (!symbolConfigs.includes(symbol.trim())) {
            setSymbolConfigs([...symbolConfigs, symbol.trim()]);
        }
    };

    // 심볼 삭제
    const handleRemoveSymbol = (symbolIndex: number) => {
        setSymbolConfigs(symbolConfigs.filter((_, i) => i !== symbolIndex));
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
                addLog('ERROR', `${config.barDataType} 바 데이터 폴더 경로를 입력해주세요.`);
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
                        {symbolConfigs.map((symbol, symbolIndex) => (
                            <div
                                key={symbolIndex}
                                className="bg-blue-600 text-white px-3 py-1 rounded-full text-sm flex items-center gap-2"
                            >
                                {symbol}
                                <button
                                    onClick={() => handleRemoveSymbol(symbolIndex)}
                                    className="hover:text-red-300 text-base leading-none"
                                >
                                    ×
                                </button>
                            </div>
                        ))}
                    </div>
                    <div className="flex gap-2">
                        <Input
                            type="text"
                            placeholder="심볼 이름 입력 후 Enter 키 또는 추가 버튼 클릭"
                            onKeyDown={(e: React.KeyboardEvent<HTMLInputElement>) => {
                                if (e.key === 'Enter') {
                                    handleAddSymbol(e.currentTarget.value);
                                    e.currentTarget.value = '';
                                }
                            }}
                            className="flex-1 bg-[#252525] border-gray-600"
                        />
                        <Button
                            onClick={(e) => {
                                const input = e.currentTarget.previousElementSibling as HTMLInputElement;
                                handleAddSymbol(input.value);
                                input.value = '';
                            }}
                            className="bg-blue-600 hover:bg-blue-700 text-white px-4"
                        >
                            추가
                        </Button>
                    </div>
                </div>

                {/* 바 데이터 설정 */}
                <div className="bg-[#1a1a1a] rounded-lg border border-gray-700 p-4 mb-4">
                    <h2 className="text-lg font-semibold text-white mb-4">바 데이터 설정</h2>

                    <div className="grid grid-cols-2 gap-4">
                        {barDataConfigs.map((config, index) => {
                            const isDisabled = config.barDataType === BarDataType.MAGNIFIER && !useBarMagnifier;
                            const barDataTypeLabel = {
                                [BarDataType.TRADING]: '트레이딩 바 데이터',
                                [BarDataType.MAGNIFIER]: '돋보기 바 데이터',
                                [BarDataType.REFERENCE]: '참조 바 데이터',
                                [BarDataType.MARK_PRICE]: '마크 가격 바 데이터'
                            }[config.barDataType];

                            return (
                                <div
                                    key={index}
                                    className={`bg-[#252525] rounded-lg p-4 border border-gray-600 ${
                                        isDisabled ? 'opacity-50' : ''
                                    }`}
                                >
                                    <div className="flex items-center justify-between mb-3">
                                        <h3 className="text-sm font-semibold text-white">{barDataTypeLabel}</h3>
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
                                                        // 허용되는 키: 숫자, Backspace, Delete, Arrow, Tab
                                                        const allowed = /^(?:[0-9]|Backspace|Delete|ArrowLeft|ArrowRight|Tab)$/;
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
