import React, {useEffect, useRef, useState} from 'react';
import {useStrategy} from './StrategyContext';
import {useDropdownAutoScroll} from './useDropdownAutoScroll';
import './StrategyEditor.css';

// 공통 파일 다이얼로그 유틸
// 선택된 디렉토리 경로를 POSIX 스타일로 반환, 실패 시 null
async function selectDirectory(): Promise<string | null> {
    try {
        if (!window.electronAPI) {
            console.error('Electron API가 사용 불가능합니다.');
            return null;
        }

        const result = await window.electronAPI.selectPath('directory');
        if (result && !result.canceled && result.filePaths && result.filePaths.length > 0) {
            return (result.filePaths[0] || '').replace(/\\/g, '/');
        }
    } catch (err) {
        console.error('폴더 선택 오류:', err);
    }

    return null;
}

const toPosix = (p: string) => (p || '').replace(/\\/g, '/');

const joinPosix = (basePath: string, childPath: string) => {
    const base = toPosix(basePath).replace(/\/+$/, '');
    const child = toPosix(childPath).replace(/^\/+/, '');

    if (!base) {
        return child;
    }

    return `${base}/${child}`;
};

const isDefaultDir = (path: string, defaultPath: string) => {
    if (!path) {
        return false;
    }

    return toPosix(path) === toPosix(defaultPath);
};

interface StrategyInfo {
    name: string;
    strategyHeaderPath?: string;
    strategySourcePath: string;
}

/**
 * 전략 설정 섹션
 * 전략 폴더 관리 및 전략 선택 기능 제공
 */
export default function StrategySection() {
    const {engineConfig, strategyConfig, setStrategyConfig, addLog} = useStrategy();
    const [availableStrategies, setAvailableStrategies] = useState<StrategyInfo[]>([]);
    const [isLoading, setIsLoading] = useState(false);

    // 스크롤 컨테이너 ref
    const strategyHeaderScrollRef = useRef<HTMLDivElement>(null);
    const strategySourceScrollRef = useRef<HTMLDivElement>(null);
    const indicatorHeaderScrollRef = useRef<HTMLDivElement>(null);
    const indicatorSourceScrollRef = useRef<HTMLDivElement>(null);
    const isFetchingRef = useRef(false);

    // 스크롤 헬퍼: 레이아웃 업데이트가 끝난 다음 프레임에 끝까지 스크롤
    const scrollToBottom = (ref: React.RefObject<HTMLDivElement | null>) => {
        const el = ref.current;
        if (!el) return;

        // 두 프레임 보장: 렌더/레이아웃이 완료되도록 함
        requestAnimationFrame(() => {
            requestAnimationFrame(() => {
                el.scrollTop = el.scrollHeight;
            });
        });

        // 추가 안전망: 한 번 더 작은 지연으로 보정
        setTimeout(() => {
            if (el.scrollTop + el.clientHeight < el.scrollHeight) {
                el.scrollTop = el.scrollHeight;
            }
        }, 50);
    };

    // 경로 정규화
    const normalizePath = (p: string) => (p || '').replace(/\\/g, '/').replace(/\/+/g, '/').replace(/\/+$/, '');

    // 특정 경로가 있는 항목으로 스크롤
    const scrollToPath = (ref: React.RefObject<HTMLDivElement | null>, path: string) => {
        const el = ref.current;
        if (!el) {
            return;
        }

        const candidates = Array.from(el.querySelectorAll<HTMLElement>('[data-path]'));
        const normalized = normalizePath(path);
        const target = candidates.find(c => normalizePath(c.getAttribute('data-path') || '') === normalized) as HTMLElement | undefined;

        if (!target) {
            // 폴백
            scrollToBottom(ref);
            return;
        }

        requestAnimationFrame(() => {
            requestAnimationFrame(() => {
                el.scrollTop = target.offsetTop;
            });
        });

        setTimeout(() => {
            if (el.scrollTop + el.clientHeight < target.offsetTop + target.clientHeight) {
                el.scrollTop = target.offsetTop;
            }
        }, 50);
    };

    // 전략 드롭다운 관련
    const strategiesDropdownRef = useRef<HTMLDivElement>(null);
    const [isStrategiesDropdownOpen, setIsStrategiesDropdownOpen] = useState(false);

    const project_dir = toPosix(engineConfig?.projectDirectory || '').replace(/\/+$/, '');
    const default_strategy_header_dir = joinPosix(project_dir, 'Includes/Strategies');
    const default_strategy_source_dir = joinPosix(project_dir, 'Sources/Cores/Strategies');
    const default_indicator_header_dir = joinPosix(project_dir, 'Includes/Indicators');
    const default_indicator_source_dir = joinPosix(project_dir, 'Sources/Cores/Indicators');


    // 전략 헤더 폴더 추가
    const handleAddHeaderFolder = async () => {
        const selected = await selectDirectory();
        if (!selected) {
            return;
        }

        const normalized = normalizePath(selected);
        const current = strategyConfig || {name: '', strategyHeaderDirs: [], strategySourceDirs: []};
        const existsIndex = (current.strategyHeaderDirs || []).findIndex(d => normalizePath(d) === normalized);
        if (existsIndex !== -1) {
            // 중복은 추가하지 않고 해당 항목으로 스크롤
            scrollToPath(strategyHeaderScrollRef, normalized);
            return;
        }

        const newFolders = [...(current.strategyHeaderDirs || []), selected];
        setStrategyConfig(prev => ({...(prev || current), strategyHeaderDirs: newFolders}));

        // 스크롤을 하단으로 이동
        scrollToBottom(strategyHeaderScrollRef);
    };

    // 전략 헤더 폴더 변경
    const handleHeaderFolderChange = (index: number, value: string) => {
        const current = strategyConfig || {name: '', strategyHeaderDirs: [], strategySourceDirs: []};
        const newFolders = [...(current.strategyHeaderDirs || [])];
        newFolders[index] = value;
        setStrategyConfig(prev => ({...(prev || current), strategyHeaderDirs: newFolders}));
    };

    // 전략 헤더 폴더 제거
    const handleRemoveHeaderFolder = (index: number) => {
        const current = strategyConfig || {name: '', strategyHeaderDirs: [], strategySourceDirs: []};
        const newFolders = (current.strategyHeaderDirs || []).filter((_, i) => i !== index);
        setStrategyConfig(prev => ({...(prev || current), strategyHeaderDirs: newFolders}));
    };

    // 전략 소스 폴더 추가
    const handleAddSourceFolder = async () => {
        const selected = await selectDirectory();
        if (!selected) {
            return;
        }

        const normalized = normalizePath(selected);
        const current = strategyConfig || {name: '', strategyHeaderDirs: [], strategySourceDirs: []};
        const existsIndex = (current.strategySourceDirs || []).findIndex(d => normalizePath(d) === normalized);
        if (existsIndex !== -1) {
            // 중복은 추가하지 않고 해당 항목으로 스크롤
            scrollToPath(strategySourceScrollRef, normalized);
            return;
        }

        const newFolders = [...(current.strategySourceDirs || []), selected];
        setStrategyConfig(prev => ({...(prev || current), strategySourceDirs: newFolders}));

        // 스크롤을 하단으로 이동
        scrollToBottom(strategySourceScrollRef);
    };

    // 전략 소스 폴더 변경
    const handleSourceFolderChange = (index: number, value: string) => {
        const current = strategyConfig || {name: '', strategyHeaderDirs: [], strategySourceDirs: []};
        const newFolders = [...(current.strategySourceDirs || [])];
        newFolders[index] = value;
        setStrategyConfig(prev => ({...(prev || current), strategySourceDirs: newFolders}));
    };

    // 전략 소스 폴더 제거
    const handleRemoveSourceFolder = (index: number) => {
        const current = strategyConfig || {name: '', strategyHeaderDirs: [], strategySourceDirs: []};
        const newFolders = (current.strategySourceDirs || []).filter((_, i) => i !== index);
        setStrategyConfig(prev => ({...(prev || current), strategySourceDirs: newFolders}));
    };

    // 지표 헤더 폴더 추가
    const handleAddIndicatorHeaderFolder = async () => {
        const selected = await selectDirectory();
        if (!selected) {
            return;
        }

        const normalized = normalizePath(selected);
        const current = strategyConfig || {
            name: '',
            strategyHeaderDirs: [],
            strategySourceDirs: [],
            indicatorHeaderDirs: [],
            indicatorSourceDirs: []
        };
        const existsIndex = (current.indicatorHeaderDirs || []).findIndex(d => normalizePath(d) === normalized);
        if (existsIndex !== -1) {
            // 중복은 추가하지 않고 해당 항목으로 스크롤
            scrollToPath(indicatorHeaderScrollRef, normalized);
            return;
        }

        const newFolders = [...(current.indicatorHeaderDirs || []), selected];
        setStrategyConfig(prev => ({...(prev || current), indicatorHeaderDirs: newFolders}));

        // 스크롤을 하단으로 이동
        scrollToBottom(indicatorHeaderScrollRef);
    };

    // 지표 헤더 폴더 변경
    const handleIndicatorHeaderFolderChange = (index: number, value: string) => {
        const current = strategyConfig || {
            name: '',
            strategyHeaderDirs: [],
            strategySourceDirs: [],
            indicatorHeaderDirs: [],
            indicatorSourceDirs: []
        };
        const newFolders = [...(current.indicatorHeaderDirs || [])];
        newFolders[index] = value;
        setStrategyConfig(prev => ({...(prev || current), indicatorHeaderDirs: newFolders}));
    };

    // 지표 헤더 폴더 제거
    const handleRemoveIndicatorHeaderFolder = (index: number) => {
        const current = strategyConfig || {
            name: '',
            strategyHeaderDirs: [],
            strategySourceDirs: [],
            indicatorHeaderDirs: [],
            indicatorSourceDirs: []
        };
        const newFolders = (current.indicatorHeaderDirs || []).filter((_, i) => i !== index);
        setStrategyConfig(prev => ({...(prev || current), indicatorHeaderDirs: newFolders}));
    };

    // 지표 소스 폴더 추가
    const handleAddIndicatorSourceFolder = async () => {
        const selected = await selectDirectory();
        if (!selected) {
            return;
        }

        const normalized = normalizePath(selected);
        const current = strategyConfig || {
            name: '',
            strategyHeaderDirs: [],
            strategySourceDirs: [],
            indicatorHeaderDirs: [],
            indicatorSourceDirs: []
        };
        const existsIndex = (current.indicatorSourceDirs || []).findIndex(d => normalizePath(d) === normalized);
        if (existsIndex !== -1) {
            // 중복은 추가하지 않고 해당 항목으로 스크롤
            scrollToPath(indicatorSourceScrollRef, normalized);
            return;
        }

        const newFolders = [...(current.indicatorSourceDirs || []), selected];
        setStrategyConfig(prev => ({...(prev || current), indicatorSourceDirs: newFolders}));

        // 스크롤을 하단으로 이동
        scrollToBottom(indicatorSourceScrollRef);
    };

    // 지표 소스 폴더 변경
    const handleIndicatorSourceFolderChange = (index: number, value: string) => {
        const current = strategyConfig || {
            name: '',
            strategyHeaderDirs: [],
            strategySourceDirs: [],
            indicatorHeaderDirs: [],
            indicatorSourceDirs: []
        };
        const newFolders = [...(current.indicatorSourceDirs || [])];
        newFolders[index] = value;
        setStrategyConfig(prev => ({...(prev || current), indicatorSourceDirs: newFolders}));
    };

    // 지표 소스 폴더 제거
    const handleRemoveIndicatorSourceFolder = (index: number) => {
        const current = strategyConfig || {
            name: '',
            strategyHeaderDirs: [],
            strategySourceDirs: [],
            indicatorHeaderDirs: [],
            indicatorSourceDirs: []
        };
        const newFolders = (current.indicatorSourceDirs || []).filter((_, i) => i !== index);
        setStrategyConfig(prev => ({...(prev || current), indicatorSourceDirs: newFolders}));
    };

    // 전략 목록 새로고침
    const handleRefreshStrategies = async () => {
        if (isFetchingRef.current) {
            return;
        }

        isFetchingRef.current = true;
        setIsLoading(true);

        try {
            const response = await fetch('/api/strategies');
            const result = await response.json();

            if (result.strategies) {
                setAvailableStrategies(result.strategies);
            } else {
                addLog('ERROR', `전략 목록 조회 실패: ${result.error || '알 수 없는 오류'}`);
                setAvailableStrategies([]);
            }
        } catch (err: any) {
            addLog('ERROR', `전략 목록 조회 실패: ${err.message}`);
            setAvailableStrategies([]);
        } finally {
            setIsLoading(false);
            isFetchingRef.current = false;
        }
    };

    // 드롭다운 자동 스크롤 연결
    useDropdownAutoScroll(strategiesDropdownRef, isStrategiesDropdownOpen);

    // 드롭다운 열 때 자동 새로고침
    useEffect(() => {
        if (!isStrategiesDropdownOpen) return;
        handleRefreshStrategies().then();
    }, [isStrategiesDropdownOpen]);

    // 드롭다운 외부 클릭 시 닫기
    useEffect(() => {
        const handleClickOutside = (event: MouseEvent) => {
            if (strategiesDropdownRef.current && !strategiesDropdownRef.current.contains(event.target as Node)) {
                setIsStrategiesDropdownOpen(false);
            }
        };

        document.addEventListener('mousedown', handleClickOutside);
        return () => document.removeEventListener('mousedown', handleClickOutside);
    }, []);

    // 전략 선택
    const handleSelectStrategy = (strategy: StrategyInfo) => {
        if (!project_dir) {
            addLog('ERROR', '프로젝트 폴더가 설정되지 않아 DLL 경로를 만들 수 없습니다.');
            return;
        }

        // DLL 경로를 프로젝트 구조에 맞게 생성: {프로젝트 폴더}/Builds/Strategies/{전략명}/{전략명}.dll
        const dllPath = joinPosix(project_dir, `Builds/Strategies/${strategy.name}/${strategy.name}.dll`);

        setStrategyConfig(prev => ({
            ...(prev || {name: ''}),
            name: strategy.name,
            dllPath: dllPath,
            strategySourcePath: strategy.strategySourcePath,
            strategyHeaderPath: strategy.strategyHeaderPath || null,
        }));
    };

    // 컴포넌트 마운트 시 전략 목록 로드
    useEffect(() => {
        handleRefreshStrategies().then();
    }, [strategyConfig?.strategyHeaderDirs, strategyConfig?.strategySourceDirs, strategyConfig?.indicatorHeaderDirs, strategyConfig?.indicatorSourceDirs]);

    return (
        <div
            className="strategy-editor-section-container"
            style={{height: '825px', marginRight: '10px'}}
        >
            <h2 className="strategy-editor-section-header">전략 설정</h2>

            <div style={{display: 'flex', flexDirection: 'column', gap: '32px'}}>
                {/* 전략 선택 */}
                <div style={{display: 'flex', flexDirection: 'column', alignItems: 'flex-start'}}>
                    <label className="strategy-editor-label" style={{fontSize: '16px'}}>전략</label>

                    <div style={{height: '8px'}}/>

                    <div
                        className="strategy-editor-dropdown-field" ref={strategiesDropdownRef}
                        style={{width: '320px'}}
                    >
                        <div
                            className={`strategy-editor-dropdown-select ${isStrategiesDropdownOpen ? 'open' : ''}`}
                            onClick={() => setIsStrategiesDropdownOpen(!isStrategiesDropdownOpen)}
                        >
                            {strategyConfig?.name || '전략'}
                        </div>

                        {isStrategiesDropdownOpen && (
                            <div className="strategy-editor-dropdown-options">
                                {isLoading ? (
                                    <div className="strategy-editor-dropdown-option">로딩 중...</div>
                                ) : availableStrategies.length === 0 ? (
                                    <div className="strategy-editor-dropdown-option">전략이 없습니다.</div>
                                ) : (
                                    availableStrategies.map((strategy) => (
                                        <div
                                            key={strategy.name}
                                            className={`strategy-editor-dropdown-option ${strategyConfig?.name === strategy.name ? 'selected' : ''}`}
                                            onClick={() => {
                                                handleSelectStrategy(strategy);
                                                setIsStrategiesDropdownOpen(false);
                                            }}
                                        >
                                            {strategy.name}
                                        </div>
                                    ))
                                )}
                            </div>
                        )}
                    </div>
                </div>

                {/* 2x2 그리드 레이아웃 */}
                <div style={{
                    display: 'grid',
                    gridTemplateColumns: 'repeat(2, 1fr)',
                    rowGap: '32px',
                    columnGap: '32px'
                }}>
                    {/* 전략 헤더 폴더 */}
                    <div>
                        <div
                            style={{
                                display: 'flex',
                                justifyContent: 'space-between',
                                alignItems: 'center',
                                marginBottom: '16px'
                            }}>

                            <label className="strategy-editor-label strategy-section-label">전략 헤더 폴더</label>
                            <button
                                onClick={handleAddHeaderFolder}
                                className="strategy-editor-button"
                            >
                                폴더 추가
                            </button>
                        </div>

                        {/* 스크롤 가능한 폴더 리스트 컨테이너 */}
                        <div
                            ref={strategyHeaderScrollRef}
                            style={{
                                height: '232px',
                                overflowY: 'auto',
                                border: '1px solid rgb(255 215 0 / 30%)',
                                borderRadius: '6px',
                                backgroundColor: 'rgb(17 17 17 / 80%)',
                                padding: '16px'
                            }}
                        >
                            {(!strategyConfig || (strategyConfig.strategyHeaderDirs || []).length === 0) && (
                                <p style={{
                                    fontSize: '12px',
                                    color: 'rgb(255 255 255 / 40%)',
                                    fontStyle: 'italic',
                                    textAlign: 'center',
                                    padding: '16px'
                                }}>추가된 전략 헤더 폴더가 없습니다.</p>
                            )}

                            <div style={{display: 'flex', flexDirection: 'column', gap: '8px'}}>
                                {(strategyConfig?.strategyHeaderDirs || []).map((folder, index) => (
                                    <div
                                        key={index}
                                        data-path={normalizePath(folder)}
                                        style={{display: 'flex', gap: '8px', alignItems: 'center'}}>

                                        <div
                                            className="strategy-editor-file-selector"
                                            style={{flex: 1}}
                                        >
                                            <input
                                                type="text"
                                                value={folder}
                                                title={folder}
                                                onChange={(e) => handleHeaderFolderChange(index, e.target.value)}
                                                className="strategy-editor-input strategy-editor-input-with-icon"
                                                readOnly
                                                tabIndex={-1}
                                                onFocus={(e) => e.currentTarget.blur()}
                                                onMouseDown={(e) => e.preventDefault()}
                                                style={{width: '100%'}}
                                            />

                                            <div className="strategy-editor-file-selector-buttons">
                                                {!isDefaultDir(folder, default_strategy_header_dir) && (
                                                    <button
                                                        onClick={(e) => {
                                                            e.stopPropagation();
                                                            handleRemoveHeaderFolder(index);
                                                        }}
                                                        onPointerDown={(e) => e.stopPropagation()}
                                                        onMouseDown={(e) => e.stopPropagation()}
                                                        className="strategy-editor-symbol-remove"
                                                        title="폴더 제거"
                                                    >
                                                        ×
                                                    </button>
                                                )}
                                            </div>
                                        </div>
                                    </div>
                                ))}
                            </div>
                        </div>
                    </div>

                    {/* 전략 소스 폴더 */}
                    <div>
                        <div
                            style={{
                                display: 'flex',
                                justifyContent: 'space-between',
                                alignItems: 'center',
                                marginBottom: '16px'
                            }}
                        >
                            <label className="strategy-editor-label strategy-section-label">전략 소스 폴더</label>
                            <button
                                onClick={handleAddSourceFolder}
                                className="strategy-editor-button"
                            >
                                폴더 추가
                            </button>
                        </div>

                        {/* 스크롤 가능한 폴더 리스트 컨테이너 */}
                        <div
                            ref={strategySourceScrollRef}
                            style={{
                                height: '232px',
                                overflowY: 'auto',
                                border: '1px solid rgb(255 215 0 / 30%)',
                                borderRadius: '6px',
                                backgroundColor: 'rgb(17 17 17 / 80%)',
                                padding: '16px'
                            }}
                        >
                            {(!strategyConfig || (strategyConfig.strategySourceDirs || []).length === 0) && (
                                <p style={{
                                    fontSize: '12px',
                                    color: 'rgb(255 255 255 / 40%)',
                                    fontStyle: 'italic',
                                    textAlign: 'center',
                                    padding: '16px'
                                }}>추가된 전략 소스 폴더가 없습니다.</p>
                            )}

                            <div style={{display: 'flex', flexDirection: 'column', gap: '8px'}}>
                                {(strategyConfig?.strategySourceDirs || []).map((folder, index) => (
                                    <div
                                        key={index}
                                        data-path={normalizePath(folder)}
                                        style={{display: 'flex', gap: '8px', alignItems: 'center'}}
                                    >
                                        <div
                                            className="strategy-editor-file-selector"
                                            style={{flex: 1}}
                                        >
                                            <input
                                                type="text"
                                                value={folder}
                                                title={folder}
                                                onChange={(e) => handleSourceFolderChange(index, e.target.value)}
                                                className="strategy-editor-input strategy-editor-input-with-icon"
                                                readOnly
                                                tabIndex={-1}
                                                onFocus={(e) => e.currentTarget.blur()}
                                                onMouseDown={(e) => e.preventDefault()}
                                                style={{width: '100%'}}
                                            />
                                            <div className="strategy-editor-file-selector-buttons">
                                                {!isDefaultDir(folder, default_strategy_source_dir) && (
                                                    <button
                                                        onClick={(e) => {
                                                            e.stopPropagation();
                                                            handleRemoveSourceFolder(index);
                                                        }}
                                                        onPointerDown={(e) => e.stopPropagation()}
                                                        onMouseDown={(e) => e.stopPropagation()}
                                                        className="strategy-editor-symbol-remove"
                                                        title="폴더 제거"
                                                    >
                                                        ×
                                                    </button>
                                                )}
                                            </div>
                                        </div>
                                    </div>
                                ))}
                            </div>
                        </div>
                    </div>

                    {/* 지표 헤더 폴더 */}
                    <div>
                        <div
                            style={{
                                display: 'flex',
                                justifyContent: 'space-between',
                                alignItems: 'center',
                                marginBottom: '16px'
                            }}
                        >
                            <label className="strategy-editor-label strategy-section-label">지표 헤더 폴더</label>
                            <button
                                onClick={handleAddIndicatorHeaderFolder}
                                className="strategy-editor-button"
                            >
                                폴더 추가
                            </button>
                        </div>

                        {/* 스크롤 가능한 폴더 리스트 컨테이너 */}
                        <div
                            ref={indicatorHeaderScrollRef}
                            style={{
                                height: '232px',
                                overflowY: 'auto',
                                border: '1px solid rgb(255 215 0 / 30%)',
                                borderRadius: '6px',
                                backgroundColor: 'rgb(17 17 17 / 80%)',
                                padding: '16px'
                            }}
                        >
                            {(!strategyConfig || (strategyConfig.indicatorHeaderDirs || []).length === 0) && (
                                <p style={{
                                    fontSize: '12px',
                                    color: 'rgb(255 255 255 / 40%)',
                                    fontStyle: 'italic',
                                    textAlign: 'center',
                                    padding: '16px'
                                }}>추가된 지표 헤더 폴더가 없습니다.</p>
                            )}

                            <div style={{display: 'flex', flexDirection: 'column', gap: '8px'}}>
                                {(strategyConfig?.indicatorHeaderDirs || []).map((folder, index) => (
                                    <div
                                        key={index}
                                        data-path={normalizePath(folder)}
                                        style={{display: 'flex', gap: '8px', alignItems: 'center'}}
                                    >
                                        <div
                                            className="strategy-editor-file-selector"
                                            style={{flex: 1}}
                                        >
                                            <input
                                                type="text"
                                                value={folder}
                                                title={folder}
                                                onChange={(e) => handleIndicatorHeaderFolderChange(index, e.target.value)}
                                                className="strategy-editor-input strategy-editor-input-with-icon"
                                                readOnly
                                                tabIndex={-1}
                                                onFocus={(e) => e.currentTarget.blur()}
                                                onMouseDown={(e) => e.preventDefault()}
                                                style={{width: '100%'}}
                                            />

                                            <div className="strategy-editor-file-selector-buttons">
                                                {!isDefaultDir(folder, default_indicator_header_dir) && (
                                                    <button
                                                        onClick={(e) => {
                                                            e.stopPropagation();
                                                            handleRemoveIndicatorHeaderFolder(index);
                                                        }}
                                                        onPointerDown={(e) => e.stopPropagation()}
                                                        onMouseDown={(e) => e.stopPropagation()}
                                                        className="strategy-editor-symbol-remove"
                                                        title="폴더 제거"
                                                    >
                                                        ×
                                                    </button>
                                                )}
                                            </div>
                                        </div>
                                    </div>
                                ))}
                            </div>
                        </div>
                    </div>

                    {/* 지표 소스 폴더 */}
                    <div>
                        <div
                            style={{
                                display: 'flex',
                                justifyContent: 'space-between',
                                alignItems: 'center',
                                marginBottom: '16px'
                            }}
                        >
                            <label className="strategy-editor-label strategy-section-label">지표 소스 폴더</label>
                            <button
                                onClick={handleAddIndicatorSourceFolder}
                                className="strategy-editor-button"
                            >
                                폴더 추가
                            </button>
                        </div>

                        {/* 스크롤 가능한 폴더 리스트 컨테이너 */}
                        <div
                            ref={indicatorSourceScrollRef}
                            style={{
                                height: '232px',
                                overflowY: 'auto',
                                border: '1px solid rgb(255 215 0 / 30%)',
                                borderRadius: '6px',
                                backgroundColor: 'rgb(17 17 17 / 80%)',
                                padding: '16px'
                            }}
                        >
                            {(!strategyConfig || (strategyConfig.indicatorSourceDirs || []).length === 0) && (
                                <p style={{
                                    fontSize: '12px',
                                    color: 'rgb(255 255 255 / 40%)',
                                    fontStyle: 'italic',
                                    textAlign: 'center',
                                    padding: '16px'
                                }}>추가된 지표 소스 폴더가 없습니다.</p>
                            )}

                            <div style={{display: 'flex', flexDirection: 'column', gap: '8px'}}>
                                {(strategyConfig?.indicatorSourceDirs || []).map((folder, index) => (
                                    <div
                                        key={index}
                                        data-path={normalizePath(folder)}
                                        style={{display: 'flex', gap: '8px', alignItems: 'center'}}
                                    >
                                        <div
                                            className="strategy-editor-file-selector"
                                            style={{flex: 1}}
                                        >
                                            <input
                                                type="text"
                                                value={folder}
                                                title={folder}
                                                onChange={(e) => handleIndicatorSourceFolderChange(index, e.target.value)}
                                                className="strategy-editor-input strategy-editor-input-with-icon"
                                                readOnly
                                                tabIndex={-1}
                                                onFocus={(e) => e.currentTarget.blur()}
                                                onMouseDown={(e) => e.preventDefault()}
                                                style={{width: '100%'}}
                                            />

                                            <div className="strategy-editor-file-selector-buttons">
                                                {!isDefaultDir(folder, default_indicator_source_dir) && (
                                                    <button
                                                        onClick={(e) => {
                                                            e.stopPropagation();
                                                            handleRemoveIndicatorSourceFolder(index);
                                                        }}
                                                        onPointerDown={(e) => e.stopPropagation()}
                                                        onMouseDown={(e) => e.stopPropagation()}
                                                        className="strategy-editor-symbol-remove"
                                                        title="폴더 제거"
                                                    >
                                                        ×
                                                    </button>
                                                )}
                                            </div>
                                        </div>
                                    </div>
                                ))}
                            </div>
                        </div>
                    </div>
                </div>
            </div>
        </div>
    );
}
