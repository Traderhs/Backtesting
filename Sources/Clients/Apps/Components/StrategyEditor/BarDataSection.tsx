import React, {useRef, useState} from 'react';
import {FolderOpen} from 'lucide-react';
import {BarDataConfig, BarDataType, TimeframeUnit} from '@/Types/BarData.ts';
import {useStrategy} from './StrategyContext';
import PathResetButton from './PathResetButton';
import './StrategyEditor.css';

// 타임프레임 단위 옵션
const TIMEFRAME_UNIT_OPTIONS = [
    {value: TimeframeUnit.SECOND, label: '초'},
    {value: TimeframeUnit.MINUTE, label: '분'},
    {value: TimeframeUnit.HOUR, label: '시간'},
    {value: TimeframeUnit.DAY, label: '일'},
    {value: TimeframeUnit.WEEK, label: '주'},
    {value: TimeframeUnit.MONTH, label: '개월'},
    {value: TimeframeUnit.YEAR, label: '년'}
];

// 기본 경로 상수
const DEFAULT_CONTINUOUS_KLINES_PATH = 'Data/Continuous Klines';
const DEFAULT_MARK_PRICE_KLINES_PATH = 'Data/Mark Price Klines';
const DEFAULT_FUNDING_RATES_PATH = 'Data/Funding Rates';

/**
 * 바 데이터 설정 섹션 컴포넌트
 * 트레이딩/돋보기/참조/마크 가격 바 데이터 설정 관리
 */
export default function BarDataSection() {
    const {
        barDataConfigs,
        setBarDataConfigs,
        fundingRatesDirectory,
        setFundingRatesDirectory,
        engineConfig,
        setEngineConfig
    } = useStrategy();

    const {projectDirectory, useBarMagnifier} = engineConfig;

    // 타임프레임 단위 드롭다운 열린 인덱스
    const [openUnitDropdownIndex, setOpenUnitDropdownIndex] = useState<number | null>(null);
    const unitDropdownRefs = useRef<(HTMLDivElement | null)[]>([]);

    const toPosix = (p: string) => (p || '').replace(/\\/g, '/');

    const joinPosix = (basePath: string, childPath: string) => {
        const base = toPosix(basePath).replace(/\/+$/, '');
        const child = toPosix(childPath).replace(/^\/+/, '');

        if (!base) {
            return child;
        }

        return `${base}/${child}`;
    };

    // 참조 바 데이터 추가
    const handleAddReferenceBar = () => {
        const safeProjectDir = projectDirectory ? toPosix(projectDirectory) : '';
        const defaultKlinesDir = safeProjectDir ? joinPosix(safeProjectDir, DEFAULT_CONTINUOUS_KLINES_PATH) : '';

        const newConfig: BarDataConfig = {
            timeframe: {value: null, unit: TimeframeUnit.NULL},
            klinesDirectory: defaultKlinesDir,
            barDataType: BarDataType.REFERENCE
        };

        const markPriceIndex = barDataConfigs.findIndex(c => c.barDataType === BarDataType.MARK_PRICE);
        const newConfigs = [...barDataConfigs];
        const insertIndex = markPriceIndex === -1 ? newConfigs.length : markPriceIndex;

        newConfigs.splice(insertIndex, 0, newConfig);
        setBarDataConfigs(newConfigs);
    };

    // 참조 바 데이터 삭제
    const handleRemoveReferenceBar = (index: number) => {
        if (barDataConfigs[index] && barDataConfigs[index].barDataType === BarDataType.REFERENCE) {
            const newConfigs = barDataConfigs.filter((_, i) => i !== index);
            setBarDataConfigs(newConfigs);
        }
    };

    // 바 데이터 설정 업데이트
    const updateBarDataConfig = (index: number, updates: Partial<BarDataConfig>) => {
        const newConfigs = [...barDataConfigs];
        newConfigs[index] = {...newConfigs[index], ...updates};
        setBarDataConfigs(newConfigs);
    };

    // 폴더 선택 핸들러
    const handleSelectDirectory = async (index: number) => {
        try {
            if (!window.electronAPI) {
                console.error('Electron API가 사용 불가능합니다.');
                return;
            }

            const result = await window.electronAPI.selectPath('directory');

            if (result && !result.canceled && result.filePaths.length > 0) {
                updateBarDataConfig(index, {klinesDirectory: toPosix(result.filePaths[0])});
            }
        } catch (error) {
            console.error('폴더 선택 오류:', error);
        }
    };

    // 경로 초기화 핸들러
    const handleResetDirectory = (index: number) => {
        const baseDir = toPosix(projectDirectory || '').replace(/\/+$/, '');
        if (!baseDir) {
            return;
        }

        const config = barDataConfigs[index];
        const defaultPath = config.barDataType === BarDataType.MARK_PRICE
            ? DEFAULT_MARK_PRICE_KLINES_PATH
            : DEFAULT_CONTINUOUS_KLINES_PATH;

        updateBarDataConfig(index, {klinesDirectory: joinPosix(baseDir, defaultPath)});
    };

    // 펀딩 비율 폴더 선택 핸들러
    const handleSelectFundingDirectory = async () => {
        try {
            if (!window.electronAPI) {
                console.error('Electron API가 사용 불가능합니다.');
                return;
            }

            const result = await window.electronAPI.selectPath('directory');
            if (result && !result.canceled && result.filePaths.length > 0) {
                setFundingRatesDirectory(toPosix(result.filePaths[0]));
            }
        } catch (error) {
            console.error('펀딩 폴더 선택 오류:', error);
        }
    };

    const handleResetFundingDirectory = () => {
        const baseDir = toPosix(projectDirectory || '').replace(/\/+$/, '');
        if (!baseDir) {
            return;
        }

        setFundingRatesDirectory(joinPosix(baseDir, DEFAULT_FUNDING_RATES_PATH));
    };

    // 드롭다운 외부 클릭 감지
    React.useEffect(() => {
        const handleClickOutside = (event: MouseEvent) => {
            if (openUnitDropdownIndex !== null) {
                const ref = unitDropdownRefs.current[openUnitDropdownIndex];
                if (ref && !ref.contains(event.target as Node)) {
                    setOpenUnitDropdownIndex(null);
                }
            }
        };

        document.addEventListener('mousedown', handleClickOutside);
        return () => document.removeEventListener('mousedown', handleClickOutside);
    }, [openUnitDropdownIndex]);

    // 바 데이터 타입 라벨
    const getBarDataTypeLabel = (type: BarDataType) => {
        switch (type) {
            case BarDataType.TRADING:
                return '트레이딩 바 데이터';
            case BarDataType.MAGNIFIER:
                return '돋보기 바 데이터';
            case BarDataType.REFERENCE:
                return '참조 바 데이터';
            case BarDataType.MARK_PRICE:
                return '마크 가격 바 데이터';
            default:
                return '바 데이터';
        }
    };

    // 타임프레임 단위 라벨
    const getUnitLabel = (unit: TimeframeUnit) => {
        const option = TIMEFRAME_UNIT_OPTIONS.find(o => o.value === unit);
        return option ? option.label : '단위';
    };

    return (
        <div
            className="strategy-editor-section-container h-full flex flex-col"
            style={{marginTop: '40px', marginRight: '10px'}}
        >
            {/* 헤더: 텍스트와 밑줄/버튼을 분리 렌더링 (클래스 외형 유지, 밑줄 간격 보존) */}
            <div
                style={{
                    position: 'relative',
                    width: '100%',
                    marginBottom: '-24px'
                }}
            >
                <h2
                    className="strategy-editor-section-header"
                >
                    바 데이터 설정
                </h2>

                {/* 버튼은 헤더 영역에 겹치도록 절대정렬하여 레이아웃에 영향 주지 않음 */}
                <button
                    onClick={handleAddReferenceBar}
                    className="strategy-editor-button"
                    style={{position: 'absolute', left: '90%', top: '-50%', transform: 'translateY(-50%)'}}
                    title={"참조 바 데이터 추가"}
                >
                    참조 바 데이터 추가
                </button>
            </div>

            {/* 바 데이터 카드 그리드 (2열) */}
            <div className="strategy-editor-bardata-grid">
                {barDataConfigs.map((config, index) => {
                    const isDisabled = config.barDataType === BarDataType.MAGNIFIER && !useBarMagnifier;
                    const canDelete = config.barDataType === BarDataType.REFERENCE;

                    return (
                        <div
                            key={index}
                            className={`strategy-editor-bardata-card ${isDisabled ? 'disabled' : ''}`}
                        >
                            {/* 카드 헤더 */}
                            <div className="strategy-editor-bardata-card-header">
                                <span className="strategy-editor-bardata-card-title">
                                    {getBarDataTypeLabel(config.barDataType)}
                                </span>

                                <div style={{display: 'flex', alignItems: 'center', gap: '8px'}}>
                                    {config.barDataType === BarDataType.MAGNIFIER && (
                                        <label className="checkbox-container">
                                            <input
                                                type="checkbox"
                                                checked={useBarMagnifier}
                                                onChange={(e) => {
                                                    setEngineConfig(prev => ({
                                                        ...prev,
                                                        useBarMagnifier: e.target.checked
                                                    }));
                                                }}
                                                className="custom-checkbox"
                                            />

                                            <span className="checkbox-label">바 돋보기 기능</span>
                                        </label>
                                    )}
                                    {canDelete && (
                                        <button
                                            onClick={() => handleRemoveReferenceBar(index)}
                                            className="strategy-editor-bardata-remove"
                                            title="참조 바 데이터 삭제"
                                        >
                                            ×
                                        </button>
                                    )}
                                </div>
                            </div>

                            {/* 카드 바디 */}
                            <div className="strategy-editor-bardata-card-body">
                                <div className="strategy-editor-bardata-row-horizontal">
                                    {/* 타임프레임 */}
                                    <div className="strategy-editor-bardata-field">
                                        <span className="strategy-editor-bardata-label-top">타임프레임</span>

                                        <div className="strategy-editor-bardata-timeframe-inputs">
                                            {/* 숫자 입력 */}
                                            <input
                                                type="text"
                                                inputMode="numeric"
                                                value={config.timeframe.value ?? ''}
                                                onKeyDown={(e: React.KeyboardEvent<HTMLInputElement>) => {
                                                    if (e.ctrlKey || e.metaKey) return;
                                                    const allowed = /^(?:[0-9]|Backspace|Delete|ArrowLeft|ArrowRight|Tab|Home|End)$/;
                                                    if (!allowed.test(e.key)) e.preventDefault();
                                                }}
                                                onChange={(e: React.ChangeEvent<HTMLInputElement>) => {
                                                    const raw = e.target.value || '';
                                                    const digits = raw.replace(/\D/g, '');
                                                    const noLeading = digits.replace(/^0+/, '');
                                                    const finalValue = noLeading === '' ? null : parseInt(noLeading, 10);
                                                    updateBarDataConfig(index, {
                                                        timeframe: {...config.timeframe, value: finalValue}
                                                    });
                                                }}
                                                disabled={isDisabled}
                                                className="strategy-editor-input strategy-editor-bardata-timeframe-value"
                                                placeholder="값"
                                            />

                                            {/* 단위 드롭다운 */}
                                            <div
                                                className="strategy-editor-dropdown-field strategy-editor-bardata-timeframe-unit"
                                                ref={el => {
                                                    unitDropdownRefs.current[index] = el;
                                                }}
                                            >
                                                <div
                                                    className={`strategy-editor-dropdown-select ${isDisabled ? 'disabled' : ''}`}
                                                    onClick={() => {
                                                        if (!isDisabled) {
                                                            setOpenUnitDropdownIndex(prev => prev === index ? null : index);
                                                        }
                                                    }}
                                                >
                                                    {getUnitLabel(config.timeframe.unit)}
                                                </div>
                                                {openUnitDropdownIndex === index && !isDisabled && (
                                                    <div className="strategy-editor-dropdown-options">
                                                        {TIMEFRAME_UNIT_OPTIONS.map(option => (
                                                            <div
                                                                key={option.value}
                                                                className={`strategy-editor-dropdown-option ${config.timeframe.unit === option.value ? 'selected' : ''}`}
                                                                onClick={() => {
                                                                    updateBarDataConfig(index, {
                                                                        timeframe: {
                                                                            ...config.timeframe,
                                                                            unit: option.value
                                                                        }
                                                                    });
                                                                    setOpenUnitDropdownIndex(null);
                                                                }}
                                                            >
                                                                {option.label}
                                                            </div>
                                                        ))}
                                                    </div>
                                                )}
                                            </div>
                                        </div>
                                    </div>

                                    {/* 폴더 경로 */}
                                    <div className="strategy-editor-bardata-field strategy-editor-bardata-path-field">
                                        <span className="strategy-editor-bardata-label-top">폴더 경로</span>

                                        <div className="strategy-editor-file-selector">
                                            <input
                                                type="text"
                                                value={config.klinesDirectory}
                                                readOnly
                                                tabIndex={-1}
                                                onFocus={(e) => e.currentTarget.blur()}
                                                onMouseDown={(e) => e.preventDefault()}
                                                disabled={isDisabled}
                                                placeholder="폴더 경로"
                                                className="strategy-editor-input strategy-editor-input-with-icon"
                                                title={config.klinesDirectory}
                                            />
                                            <div className="strategy-editor-file-selector-buttons">
                                                <PathResetButton onClick={() => handleResetDirectory(index)}/>
                                                <button
                                                    onClick={() => handleSelectDirectory(index)}
                                                    className="strategy-editor-file-selector-button"
                                                    title={`${getBarDataTypeLabel(config.barDataType)} 폴더 선택`}
                                                    disabled={isDisabled}
                                                >
                                                    <FolderOpen size={20}/>
                                                </button>
                                            </div>
                                        </div>
                                    </div>
                                </div>
                            </div>
                        </div>
                    );
                })}

                {/* 펀딩 비율 카드 */}
                <div className="strategy-editor-bardata-card">
                    <div className="strategy-editor-bardata-card-header">
                        <span className="strategy-editor-bardata-card-title">펀딩 비율</span>
                    </div>

                    <div className="strategy-editor-bardata-card-body">
                        <div className="strategy-editor-bardata-row-horizontal">
                            <div className="strategy-editor-bardata-field strategy-editor-bardata-path-field">
                                <span className="strategy-editor-bardata-label-top">폴더 경로</span>

                                <div className="strategy-editor-file-selector">
                                    <input
                                        type="text"
                                        value={fundingRatesDirectory}
                                        readOnly
                                        tabIndex={-1}
                                        onFocus={(e) => e.currentTarget.blur()}
                                        onMouseDown={(e) => e.preventDefault()}
                                        placeholder="폴더 경로"
                                        title={fundingRatesDirectory}
                                        className="strategy-editor-input strategy-editor-input-with-icon cursor-text"
                                    />

                                    <div className="strategy-editor-file-selector-buttons">
                                        <PathResetButton onClick={handleResetFundingDirectory}/>
                                        <button
                                            onClick={handleSelectFundingDirectory}
                                            className="strategy-editor-file-selector-button"
                                            title="펀딩 비율 폴더 선택"
                                        >
                                            <FolderOpen size={20}/>
                                        </button>
                                    </div>
                                </div>
                            </div>
                        </div>
                    </div>
                </div>
            </div>
        </div>
    );
}
