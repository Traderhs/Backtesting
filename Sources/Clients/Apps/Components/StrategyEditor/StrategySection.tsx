import {useEffect, useState} from 'react';
import {Button} from '@/Components/UI/Button.tsx';
import {Input} from '@/Components/UI/Input.tsx';
import {useStrategy} from './StrategyContext';

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

    const project_dir = toPosix(engineConfig?.projectDirectory || '').replace(/\/+$/, '');
    const default_strategy_header_dir = joinPosix(project_dir, 'Includes/Strategies');
    const default_strategy_source_dir = joinPosix(project_dir, 'Sources/Cores/Strategies');
    const default_indicator_header_dir = joinPosix(project_dir, 'Includes/Indicators');
    const default_indicator_source_dir = joinPosix(project_dir, 'Sources/Cores/Indicators');

    // 기본 폴더가 설정에 포함되어 있는지 확인하고 없으면 추가
    useEffect(() => {
        if (!project_dir || !strategyConfig) {
            return;
        }

        let changed = false;

        // 객체 복사
        const newConfig = {...strategyConfig};

        const ensureDefault = (dirs: string[] | undefined, defaultPath: string): string[] => {
            const currentDirs = dirs || [];

            // 이미 존재하는지 확인
            const exists = currentDirs.some(dir => isDefaultDir(dir, defaultPath));
            if (!exists) {
                changed = true;

                return [defaultPath, ...currentDirs];
            }

            return currentDirs;
        };

        newConfig.strategyHeaderDirs = ensureDefault(newConfig.strategyHeaderDirs, default_strategy_header_dir);
        newConfig.strategySourceDirs = ensureDefault(newConfig.strategySourceDirs, default_strategy_source_dir);
        newConfig.indicatorHeaderDirs = ensureDefault(newConfig.indicatorHeaderDirs, default_indicator_header_dir);
        newConfig.indicatorSourceDirs = ensureDefault(newConfig.indicatorSourceDirs, default_indicator_source_dir);

        if (changed) {
            setStrategyConfig(newConfig);
        }
    }, [strategyConfig, setStrategyConfig, project_dir, default_strategy_header_dir, default_strategy_source_dir, default_indicator_header_dir, default_indicator_source_dir]);

    // 전략 헤더 폴더 추가
    const handleAddHeaderFolder = () => {
        const current = strategyConfig || {name: '', strategyHeaderDirs: [], strategySourceDirs: []};
        const newFolders = [...(current.strategyHeaderDirs || []), ''];
        setStrategyConfig(prev => ({...(prev || current), strategyHeaderDirs: newFolders}));
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
    const handleAddSourceFolder = () => {
        const current = strategyConfig || {name: '', strategyHeaderDirs: [], strategySourceDirs: []};
        const newFolders = [...(current.strategySourceDirs || []), ''];
        setStrategyConfig(prev => ({...(prev || current), strategySourceDirs: newFolders}));
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
    const handleAddIndicatorHeaderFolder = () => {
        const current = strategyConfig || {
            name: '',
            strategyHeaderDirs: [],
            strategySourceDirs: [],
            indicatorHeaderDirs: [],
            indicatorSourceDirs: []
        };
        const newFolders = [...(current.indicatorHeaderDirs || []), ''];
        setStrategyConfig(prev => ({...(prev || current), indicatorHeaderDirs: newFolders}));
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
    const handleAddIndicatorSourceFolder = () => {
        const current = strategyConfig || {
            name: '',
            strategyHeaderDirs: [],
            strategySourceDirs: [],
            indicatorHeaderDirs: [],
            indicatorSourceDirs: []
        };
        const newFolders = [...(current.indicatorSourceDirs || []), ''];
        setStrategyConfig(prev => ({...(prev || current), indicatorSourceDirs: newFolders}));
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
        }
    };

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
        <div className="mb-6 p-4 bg-[#071029] border border-gray-700 rounded-lg">
            <h2 className="text-lg font-semibold text-white mb-4">전략 설정</h2>

            <div className="space-y-4">
                {/* 전략 헤더 폴더 관리 */}
                <div>
                    <div className="flex items-center justify-between mb-2">
                        <label className="text-xs text-gray-300">전략 헤더 폴더 (기본: Includes/Strategies)</label>
                        <Button
                            onClick={handleAddHeaderFolder}
                            className="bg-blue-600 hover:bg-blue-700 text-white text-xs px-3 py-1 rounded"
                        >
                            헤더 폴더 추가
                        </Button>
                    </div>

                    {(!strategyConfig || (strategyConfig.strategyHeaderDirs || []).length === 0) && (
                        <p className="text-xs text-gray-500 italic">추가 전략 헤더 폴더가 없습니다. 버튼을 클릭하여 추가하세요.</p>
                    )}

                    <div className="space-y-2">
                        {(strategyConfig?.strategyHeaderDirs || []).map((folder, index) => (
                            <div key={index} className="flex gap-2">
                                <Input
                                    type="text"
                                    value={folder}
                                    onChange={(e) => handleHeaderFolderChange(index, e.target.value)}
                                    placeholder="예: D:/Programming/Backtesting/Includes/Strategies"
                                    className="bg-[#050a12] border-gray-600 flex-1 text-sm"
                                    readOnly={isDefaultDir(folder, default_strategy_header_dir)}
                                />
                                {!isDefaultDir(folder, default_strategy_header_dir) && (
                                    <Button
                                        onClick={() => handleRemoveHeaderFolder(index)}
                                        className="bg-red-600 hover:bg-red-700 text-white px-3 py-1 rounded text-sm"
                                    >
                                        제거
                                    </Button>
                                )}
                            </div>
                        ))}
                    </div>
                </div>

                {/* 전략 소스 폴더 관리 */}
                <div>
                    <div className="flex items-center justify-between mb-2">
                        <label className="text-xs text-gray-300">전략 소스 폴더 (기본: Sources/Cores/Strategies)</label>
                        <Button
                            onClick={handleAddSourceFolder}
                            className="bg-blue-600 hover:bg-blue-700 text-white text-xs px-3 py-1 rounded"
                        >
                            소스 폴더 추가
                        </Button>
                    </div>

                    {(!strategyConfig || (strategyConfig.strategySourceDirs || []).length === 0) && (
                        <p className="text-xs text-gray-500 italic">추가 전략 소스 폴더가 없습니다. 버튼을 클릭하여 추가하세요.</p>
                    )}

                    <div className="space-y-2">
                        {(strategyConfig?.strategySourceDirs || []).map((folder, index) => (
                            <div key={index} className="flex gap-2">
                                <Input
                                    type="text"
                                    value={folder}
                                    onChange={(e) => handleSourceFolderChange(index, e.target.value)}
                                    placeholder="예: D:/Programming/Backtesting/Sources/Cores/Strategies"
                                    className="bg-[#050a12] border-gray-600 flex-1 text-sm"
                                    readOnly={isDefaultDir(folder, default_strategy_source_dir)}
                                />
                                {!isDefaultDir(folder, default_strategy_source_dir) && (
                                    <Button
                                        onClick={() => handleRemoveSourceFolder(index)}
                                        className="bg-red-600 hover:bg-red-700 text-white px-3 py-1 rounded text-sm"
                                    >
                                        제거
                                    </Button>
                                )}
                            </div>
                        ))}
                    </div>
                </div>

                {/* 지표 헤더 폴더 관리 */}
                <div>
                    <div className="flex items-center justify-between mb-2">
                        <label className="text-xs text-gray-300">지표 헤더 폴더 (기본: Includes/Indicators)</label>
                        <Button
                            onClick={handleAddIndicatorHeaderFolder}
                            className="bg-blue-600 hover:bg-blue-700 text-white text-xs px-3 py-1 rounded"
                        >
                            헤더 폴더 추가
                        </Button>
                    </div>

                    {(!strategyConfig || (strategyConfig.indicatorHeaderDirs || []).length === 0) && (
                        <p className="text-xs text-gray-500 italic">추가 지표 헤더 폴더가 없습니다. 버튼을 클릭하여 추가하세요.</p>
                    )}

                    <div className="space-y-2">
                        {(strategyConfig?.indicatorHeaderDirs || []).map((folder, index) => (
                            <div key={index} className="flex gap-2">
                                <Input
                                    type="text"
                                    value={folder}
                                    onChange={(e) => handleIndicatorHeaderFolderChange(index, e.target.value)}
                                    placeholder="예: D:/Programming/Backtesting/Includes/Indicators"
                                    className="bg-[#050a12] border-gray-600 flex-1 text-sm"
                                    readOnly={isDefaultDir(folder, default_indicator_header_dir)}
                                />
                                {!isDefaultDir(folder, default_indicator_header_dir) && (
                                    <Button
                                        onClick={() => handleRemoveIndicatorHeaderFolder(index)}
                                        className="bg-red-600 hover:bg-red-700 text-white px-3 py-1 rounded text-sm"
                                    >
                                        제거
                                    </Button>
                                )}
                            </div>
                        ))}
                    </div>
                </div>

                {/* 지표 소스 폴더 관리 */}
                <div>
                    <div className="flex items-center justify-between mb-2">
                        <label className="text-xs text-gray-300">지표 소스 폴더 (기본: Sources/Cores/Indicators)</label>
                        <Button
                            onClick={handleAddIndicatorSourceFolder}
                            className="bg-blue-600 hover:bg-blue-700 text-white text-xs px-3 py-1 rounded"
                        >
                            소스 폴더 추가
                        </Button>
                    </div>

                    {(!strategyConfig || (strategyConfig.indicatorSourceDirs || []).length === 0) && (
                        <p className="text-xs text-gray-500 italic">추가 지표 소스 폴더가 없습니다. 버튼을 클릭하여 추가하세요.</p>
                    )}

                    <div className="space-y-2">
                        {(strategyConfig?.indicatorSourceDirs || []).map((folder, index) => (
                            <div key={index} className="flex gap-2">
                                <Input
                                    type="text"
                                    value={folder}
                                    onChange={(e) => handleIndicatorSourceFolderChange(index, e.target.value)}
                                    placeholder="예: D:/Programming/Backtesting/Sources/Cores/Indicators"
                                    className="bg-[#050a12] border-gray-600 flex-1 text-sm"
                                    readOnly={isDefaultDir(folder, default_indicator_source_dir)}
                                />
                                {!isDefaultDir(folder, default_indicator_source_dir) && (
                                    <Button
                                        onClick={() => handleRemoveIndicatorSourceFolder(index)}
                                        className="bg-red-600 hover:bg-red-700 text-white px-3 py-1 rounded text-sm"
                                    >
                                        제거
                                    </Button>
                                )}
                            </div>
                        ))}
                    </div>
                </div>

                {/* 선택된 전략 표시 */}
                <div>
                    <label className="text-xs text-gray-300 block mb-2">선택된 전략</label>
                    <div className="bg-[#050a12] border border-gray-600 rounded p-3">
                        {strategyConfig ? (
                            <>
                                <div className="text-sm text-white font-semibold mb-1">
                                    {strategyConfig.name}
                                </div>
                                {strategyConfig.strategySourcePath && (
                                    <div className="text-xs text-gray-400">
                                        {strategyConfig.strategySourcePath}
                                    </div>
                                )}
                            </>
                        ) : (
                            <div className="text-sm text-gray-500">전략이 선택되지 않았습니다</div>
                        )}
                    </div>
                </div>

                {/* 전략 목록 */}
                <div>
                    <div className="flex items-center justify-between mb-2">
                        <label className="text-xs text-gray-300">사용 가능한 전략</label>
                        <Button
                            onClick={handleRefreshStrategies}
                            disabled={isLoading}
                            className="bg-gray-600 hover:bg-gray-500 text-white text-xs px-3 py-1 rounded"
                        >
                            {isLoading ? '로딩 중...' : '새로고침'}
                        </Button>
                    </div>

                    <div className="bg-[#050a12] border border-gray-600 rounded max-h-40 overflow-y-auto">
                        {availableStrategies.length === 0 ? (
                            <div className="p-3 text-xs text-gray-500 text-center">
                                사용 가능한 전략이 없습니다. 새로고침 버튼을 클릭하세요.
                            </div>
                        ) : (
                            <div className="divide-y divide-gray-700">
                                {availableStrategies.map((strategy, index) => (
                                    <div
                                        key={index}
                                        onClick={() => handleSelectStrategy(strategy)}
                                        className={`p-2 cursor-pointer hover:bg-gray-700 transition-colors ${
                                            strategyConfig?.name === strategy.name ? 'bg-blue-900' : ''
                                        }`}
                                    >
                                        <div className="text-sm text-white">{strategy.name}</div>
                                        <div className="text-xs text-gray-400 truncate">
                                            소스: {strategy.strategySourcePath}
                                        </div>
                                        {strategy.strategyHeaderPath && (
                                            <div className="text-xs text-gray-500 truncate">
                                                헤더: {strategy.strategyHeaderPath}
                                            </div>
                                        )}
                                    </div>
                                ))}
                            </div>
                        )}
                    </div>
                </div>

                <div className="text-xs text-gray-400">
                    <p>※ 백테스팅 실행 시 선택된 전략을 자동으로 빌드하고 로드합니다.</p>
                </div>
            </div>
        </div>
    );
}
