import {useStrategy} from './StrategyContext';
import {FolderOpen} from 'lucide-react';
import PathResetButton from './PathResetButton';
import './StrategyEditor.css';

// 기본 경로 상수
const DEFAULT_EXCHANGE_INFO_PATH = 'Data/exchange_info.json';
const DEFAULT_LEVERAGE_BRACKET_PATH = 'Data/leverage_bracket.json';

export default function ExchangeSection() {
    const {exchangeConfig, setExchangeConfig, lastDataUpdates, engineConfig} = useStrategy();

    const updateConfig = (key: keyof typeof exchangeConfig, value: string) => {
        setExchangeConfig(prev => ({...prev, [key]: value}));
    };

    const toPosix = (p: string) => (p || '').replace(/\\/g, '/');

    const joinPosix = (basePath: string, childPath: string) => {
        const base = toPosix(basePath).replace(/\/+$/, '');
        const child = toPosix(childPath).replace(/^\/+/, '');

        if (!base) {
            return child;
        }

        return `${base}/${child}`;
    };

    const inferProjectDirFromPath = (p: string): string => {
        const normalized = toPosix(p);
        const m = normalized.match(/^(.*)\/Data\/(exchange_info\.json|leverage_bracket\.json)$/);
        return m ? m[1] : '';
    };

    const handleSelectExchangeInfoPath = async () => {
        try {
            if (!window.electronAPI) {
                console.error('Electron API가 사용 불가능합니다.');
                return;
            }

            const result = await window.electronAPI.selectPath('file');

            if (result && !result.canceled && result.filePaths.length > 0) {
                updateConfig('exchangeInfoPath', toPosix(result.filePaths[0]));
            }
        } catch (error) {
            console.error('파일 선택 오류:', error);
        }
    };

    const handleSelectLeverageBracketPath = async () => {
        try {
            if (!window.electronAPI) {
                console.error('Electron API가 사용 불가능합니다.');
                return;
            }

            const result = await window.electronAPI.selectPath('file');

            if (result && !result.canceled && result.filePaths.length > 0) {
                updateConfig('leverageBracketPath', toPosix(result.filePaths[0]));
            }
        } catch (error) {
            console.error('파일 선택 오류:', error);
        }
    };

    const handleResetExchangeInfoPath = () => {
        const base_dir = toPosix(engineConfig?.projectDirectory || '').replace(/\/+$/, '') || inferProjectDirFromPath(exchangeConfig.exchangeInfoPath);
        if (!base_dir) {
            return;
        }

        updateConfig('exchangeInfoPath', joinPosix(base_dir, DEFAULT_EXCHANGE_INFO_PATH));
    };

    const handleResetLeverageBracketPath = () => {
        const base_dir = toPosix(engineConfig?.projectDirectory || '').replace(/\/+$/, '') || inferProjectDirFromPath(exchangeConfig.leverageBracketPath);
        if (!base_dir) {
            return;
        }

        updateConfig('leverageBracketPath', joinPosix(base_dir, DEFAULT_LEVERAGE_BRACKET_PATH));
    };

    return (
        <div className="strategy-editor-section-container h-full flex flex-col">
            <h2 className="strategy-editor-section-header">거래소 설정</h2>

            <div className="flex flex-row flex-1 pt-2 pl-2 pr-6">
                {/* 좌측: 라벨 영역 */}
                <div className="flex flex-col gap-4 min-w-[180px] items-end">
                    <div className="strategy-editor-row-item strategy-editor-label">Binance API Key 환경 변수</div>
                    <div className="strategy-editor-row-item strategy-editor-label">Binance API Serect 환경 변수</div>
                    <div className="strategy-editor-row-item strategy-editor-label">거래소 정보 파일 경로</div>
                    <div className="strategy-editor-row-item strategy-editor-label">레버리지 구간 파일 경로</div>
                    <div className="strategy-editor-row-item strategy-editor-label">마지막 데이터 업데이트</div>
                </div>

                {/* 중앙: 긴 금색 구분선 */}
                <div className="strategy-editor-vertical-divider"></div>

                {/* 우측: 입력 영역 */}
                <div className="flex flex-col gap-4 flex-1">
                    <input type="text" value={exchangeConfig.apiKeyEnvVar}
                           onChange={(e) => updateConfig('apiKeyEnvVar', e.currentTarget.value)}
                           placeholder="환경 변수 이름"
                           className="strategy-editor-input strategy-editor-row-item"/>

                    <input type="text" value={exchangeConfig.apiSecretEnvVar}
                           onChange={(e) => updateConfig('apiSecretEnvVar', e.currentTarget.value)}
                           placeholder="환경 변수 이름"
                           className="strategy-editor-input strategy-editor-row-item"/>

                    <div className="strategy-editor-file-selector strategy-editor-row-item">
                        <input type="text" value={exchangeConfig.exchangeInfoPath}
                               readOnly
                               placeholder="거래소 정보 파일 경로"
                               className="strategy-editor-input strategy-editor-input-with-icon cursor-text"/>

                        <div className="strategy-editor-file-selector-buttons">
                            <PathResetButton onClick={handleResetExchangeInfoPath}/>
                            <button
                                onClick={handleSelectExchangeInfoPath}
                                className="strategy-editor-file-selector-button"
                                title="파일 선택"
                            >
                                <FolderOpen size={20}/>
                            </button>
                        </div>
                    </div>

                    <div className="strategy-editor-file-selector strategy-editor-row-item">
                        <input type="text" value={exchangeConfig.leverageBracketPath}
                               readOnly
                               placeholder="레버리지 구간 파일 경로"
                               className="strategy-editor-input strategy-editor-input-with-icon cursor-text"/>

                        <div className="strategy-editor-file-selector-buttons">
                            <PathResetButton onClick={handleResetLeverageBracketPath}/>
                            <button
                                onClick={handleSelectLeverageBracketPath}
                                className="strategy-editor-file-selector-button"
                                title="파일 선택"
                            >
                                <FolderOpen size={20}/>
                            </button>
                        </div>
                    </div>

                    <div className="strategy-editor-info-display strategy-editor-row-item">
                        {lastDataUpdates || "업데이트 정보 없음"}
                    </div>
                </div>
            </div>
        </div>
    );
}
