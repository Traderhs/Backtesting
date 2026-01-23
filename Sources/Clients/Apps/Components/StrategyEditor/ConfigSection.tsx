import React from 'react';
import {Input} from '@/Components/UI/Input';
import NumericInput from '@/Components/StrategyEditor/NumericInput';
import {useStrategy} from './StrategyContext';
import {Calendar as CalendarIcon} from 'lucide-react';
import StrategyEditorCalendar from '@/Components/StrategyEditor/StrategyEditorCalendar';
import {BarDataType, timeframeToString, TimeframeUnit} from '@/Types/BarData';

export default function ConfigSection() {
    const {engineConfig, setEngineConfig, barDataConfigs} = useStrategy();
    const [isSlippageDropdownOpen, setIsSlippageDropdownOpen] = React.useState(false);
    const slippageDropdownRef = React.useRef<HTMLDivElement>(null);

    // 달력 상태
    const [showStartCalendar, setShowStartCalendar] = React.useState(false);
    const [showEndCalendar, setShowEndCalendar] = React.useState(false);
    const [startCalendarLastSelectedDate, setStartCalendarLastSelectedDate] = React.useState<Date | null>(null);
    const [startCalendarLastSelectedTime, setStartCalendarLastSelectedTime] = React.useState<string>('');
    const [endCalendarLastSelectedDate, setEndCalendarLastSelectedDate] = React.useState<Date | null>(null);
    const [endCalendarLastSelectedTime, setEndCalendarLastSelectedTime] = React.useState<string>('');

    // 그리드/행 컨테이너 참조 및 divider 포지션 상태
    const gridRef = React.useRef<HTMLDivElement | null>(null);
    const rowContainerRef = React.useRef<HTMLDivElement | null>(null);
    const [dividerLeft, setDividerLeft] = React.useState<number | null>(null);

    // 숫자 입력용 로컬 문자열 상태 (typing 중 손실 없이 텍스트로 관리)
    const [initialBalanceInput, setInitialBalanceInput] = React.useState<string>(engineConfig.initialBalance !== undefined ? String(engineConfig.initialBalance) : '');
    const [takerFeeInput, setTakerFeeInput] = React.useState<string>(engineConfig.takerFeePercentage !== undefined ? String(engineConfig.takerFeePercentage) : '');
    const [makerFeeInput, setMakerFeeInput] = React.useState<string>(engineConfig.makerFeePercentage !== undefined ? String(engineConfig.makerFeePercentage) : '');
    const [slippageTakerInput, setSlippageTakerInput] = React.useState<string>(engineConfig.slippageTakerPercentage !== undefined ? String(engineConfig.slippageTakerPercentage) : '');
    const [slippageMakerInput, setSlippageMakerInput] = React.useState<string>(engineConfig.slippageMakerPercentage !== undefined ? String(engineConfig.slippageMakerPercentage) : '');
    const [slippageStressInput, setSlippageStressInput] = React.useState<string>(engineConfig.slippageStressMultiplier !== undefined ? String(engineConfig.slippageStressMultiplier) : '');

    // engineConfig가 외부에서 변경될 경우 로컬 상태 동기화
    React.useEffect(() => {
        setInitialBalanceInput(engineConfig.initialBalance !== undefined ? String(engineConfig.initialBalance) : '');
        setTakerFeeInput(engineConfig.takerFeePercentage !== undefined ? String(engineConfig.takerFeePercentage) : '');
        setMakerFeeInput(engineConfig.makerFeePercentage !== undefined ? String(engineConfig.makerFeePercentage) : '');
        setSlippageTakerInput(engineConfig.slippageTakerPercentage !== undefined ? String(engineConfig.slippageTakerPercentage) : '');
        setSlippageMakerInput(engineConfig.slippageMakerPercentage !== undefined ? String(engineConfig.slippageMakerPercentage) : '');
        setSlippageStressInput(engineConfig.slippageStressMultiplier !== undefined ? String(engineConfig.slippageStressMultiplier) : '');
    }, [engineConfig.initialBalance, engineConfig.takerFeePercentage, engineConfig.makerFeePercentage, engineConfig.slippageTakerPercentage, engineConfig.slippageMakerPercentage, engineConfig.slippageStressMultiplier]);

    const handleConfigChange = (field: keyof typeof engineConfig, value: any) => {
        setEngineConfig(prev => ({...prev, [field]: value}));
    };

    const parseDateTimeString = (datetimeStr: string) => {
        if (!datetimeStr) {
            return null;
        }

        const matched = datetimeStr.match(/^(\d{4})-(\d{2})-(\d{2})\s+(\d{2}):(\d{2}):(\d{2})$/);
        if (!matched) {
            return null;
        }

        const [, Y, M, D, hh, mm, ss] = matched;
        return {
            date: new Date(Number(Y), Number(M) - 1, Number(D), Number(hh), Number(mm), Number(ss)),
            timeStr: `${hh}:${mm}:${ss}`
        };
    };

    const formatDate = (date: Date) => {
        const y = date.getFullYear();
        const m = String(date.getMonth() + 1).padStart(2, '0');
        const day = String(date.getDate()).padStart(2, '0');
        return `${y}-${m}-${day}`;
    };

    const formatDateTime = (date: Date, timeStr: string) => {
        return `${formatDate(date)} ${timeStr}`;
    };

    const getStartCalendarInitial = () => {
        if (engineConfig.backtestPeriodStart && engineConfig.backtestPeriodStart.trim()) {
            const parsed = parseDateTimeString(engineConfig.backtestPeriodStart);
            if (parsed) {
                return parsed;
            }
        }

        // 기본: Unix epoch start (1970-01-01 00:00:00)
        return {date: new Date(0), timeStr: '00:00:00'};
    };

    const getEndCalendarInitial = () => {
        if (engineConfig.backtestPeriodEnd && engineConfig.backtestPeriodEnd.trim()) {
            const parsed = parseDateTimeString(engineConfig.backtestPeriodEnd);
            if (parsed) {
                return parsed;
            }
        }

        const now = new Date();
        const timeStr = `${String(now.getHours()).padStart(2, '0')}:${String(now.getMinutes()).padStart(2, '0')}:${String(now.getSeconds()).padStart(2, '0')}`;
        return {date: now, timeStr};
    };

    const handleStartDateTimeSelected = (date: Date, time: string) => {
        const formatted = formatDateTime(date, time);
        handleConfigChange('backtestPeriodStart', formatted);
        handleConfigChange('useBacktestPeriodStart', false);
        setStartCalendarLastSelectedDate(date);
        setStartCalendarLastSelectedTime(time);
        setShowStartCalendar(false);
    };

    const handleEndDateTimeSelected = (date: Date, time: string) => {
        const formatted = formatDateTime(date, time);
        handleConfigChange('backtestPeriodEnd', formatted);
        handleConfigChange('useBacktestPeriodEnd', false);
        setEndCalendarLastSelectedDate(date);
        setEndCalendarLastSelectedTime(time);
        setShowEndCalendar(false);
    };

    // 드롭다운 외부 클릭 감지
    React.useEffect(() => {
        const handleClickOutside = (event: MouseEvent) => {
            if (slippageDropdownRef.current && !slippageDropdownRef.current.contains(event.target as Node)) {
                setIsSlippageDropdownOpen(false);
            }
        };

        document.addEventListener('mousedown', handleClickOutside);
        return () => document.removeEventListener('mousedown', handleClickOutside);
    }, []);

    // 그리드 가운데 divider 위치 계산
    React.useLayoutEffect(() => {
        if (!rowContainerRef.current) {
            return;
        }

        const computeFor = (gridEl: HTMLDivElement | null, setLeft: (v: number | null) => void) => {
            if (!gridEl) {
                setLeft(null);
                return;
            }

            const parentRect = rowContainerRef.current!.getBoundingClientRect();
            const children = Array.from(gridEl.children).filter(c => {
                const cl = (c as HTMLElement).classList;

                return !cl.contains('strategy-editor-grid-divider') && !cl.contains('strategy-editor-grid-divider-abs');
            }) as HTMLElement[];

            // 라벨 열(컬럼 1)에 해당하는 요소들의 우측값 중 최댓값을 취하여 구분선 위치를 결정
            const labelEls = children.filter((c) => {
                try {
                    const cs = window.getComputedStyle(c as Element);

                    return cs.gridColumnStart === '1' || (cs.gridColumn && cs.gridColumn.startsWith('1'));
                } catch (e) {
                    return false;
                }
            }) as HTMLElement[];

            const LEFT_GAP = 20; // 라벨 끝 -> 구분선 왼쪽
            const RIGHT_GAP = 22; // 구분선 오른쪽 -> 값 시작
            const DIV_WIDTH = 1;

            // 우선 라벨 끝을 기준으로 구분선 위치를 결정
            if (labelEls.length > 0) {
                const maxRight = Math.max(...labelEls.map(l => l.getBoundingClientRect().right));
                const left = Math.round(maxRight - parentRect.left + LEFT_GAP);
                setLeft(left);
                return;
            }

            // 폴백: 첫번째 값 셀의 left를 이용하여 구분선 위치를 추론
            const firstValueEl = children[1] as HTMLElement | undefined;
            if (firstValueEl) {
                const valueRect = firstValueEl.getBoundingClientRect();
                // valueRect.left == 화면상의 값 시작 위치
                // 구분선 왼쪽은 valueRect.left - (DIV_WIDTH + RIGHT_GAP)
                const left = Math.round(valueRect.left - parentRect.left - (DIV_WIDTH + RIGHT_GAP));
                setLeft(left);
            } else {
                setLeft(null);
            }
        };

        const computePos = () => {
            computeFor(gridRef.current, setDividerLeft);
        };

        // 초기 계산
        computePos();

        // ResizeObserver로 반응형 처리
        let ro: ResizeObserver | null = null;
        try {
            ro = new ResizeObserver(() => computePos());
            if (gridRef.current) {
                ro.observe(gridRef.current);
            }

            ro.observe(rowContainerRef.current);
        } catch (e) {
            // 브라우저가 ResizeObserver 미지원시 window resize 대체
            window.addEventListener('resize', computePos);
        }

        return () => {
            if (ro) {
                ro.disconnect();
            } else {
                window.removeEventListener('resize', computePos);
            }
        };
    }, [engineConfig]);

    return (
        <div
            className="strategy-editor-section-container flex flex-col"
        >
            <h2 className="strategy-editor-section-header">엔진 설정</h2>

            <div ref={rowContainerRef} className="flex flex-col flex-1 pt-2 pl-6 pr-6 "
                 style={{position: 'relative'}}>
                {/* ===== 그리드 ===== */}
                <div ref={gridRef} className="grid strategy-editor-grid">
                    {/* 좌측: 라벨 영역 */}
                    <div className="strategy-editor-row-item"
                         style={{alignItems: 'center', justifyContent: 'flex-end', textAlign: 'right'}}>
                        <div className="strategy-editor-label">프로젝트 폴더</div>
                    </div>

                    <div>
                        <input
                            type="text"
                            value={engineConfig.projectDirectory}
                            readOnly
                            tabIndex={-1}
                            onFocus={(e) => e.currentTarget.blur()}
                            onMouseDown={(e) => e.preventDefault()}
                            placeholder="프로젝트 폴더"
                            className="strategy-editor-input"
                            title={engineConfig.projectDirectory}
                        />
                    </div>

                    <div className="strategy-editor-row-item"
                         style={{alignItems: 'initial', justifyContent: 'flex-end', textAlign: 'right'}}>
                        <div className="strategy-editor-label">백테스팅 기간</div>
                    </div>

                    <div>
                        <div style={{
                            flex: 1,
                            display: 'flex',
                            flexDirection: 'column',
                            gap: '12px'
                        }}>
                            <div style={{display: 'flex', gap: '12px'}}>
                                <div className="strategy-editor-period-column">
                                    <label
                                        className="checkbox-container"
                                        style={{display: 'flex', alignItems: 'center'}}
                                    >
                                        <input
                                            type="checkbox"
                                            checked={engineConfig.useBacktestPeriodStart}
                                            onChange={(e) => {
                                                handleConfigChange('useBacktestPeriodStart', e.target.checked);
                                            }}
                                            className="custom-checkbox"
                                        />
                                        <span className="checkbox-label" style={{fontSize: '12px'}}>처음부터</span>
                                    </label>

                                    <div
                                        className={`strategy-editor-period-field ${engineConfig.useBacktestPeriodStart ? 'disabled' : ''}`}>
                                        <div className="strategy-editor-file-selector">
                                            <Input
                                                type="text"
                                                value={engineConfig.backtestPeriodStart}
                                                readOnly
                                                tabIndex={-1}
                                                onFocus={(e) => e.currentTarget.blur()}
                                                onMouseDown={(e) => e.preventDefault()}
                                                disabled={engineConfig.useBacktestPeriodStart}
                                                onChange={(e) => {
                                                    const value = e.target.value;
                                                    handleConfigChange('backtestPeriodStart', value);
                                                    handleConfigChange('useBacktestPeriodStart', !value.trim());
                                                }}
                                                placeholder="백테스팅 시작 시간"
                                                className="strategy-editor-input w-full"
                                                style={{fontSize: '13px'}}
                                            />

                                            <div className="strategy-editor-file-selector-buttons">
                                                <button
                                                    onMouseDown={(e) => {
                                                        e.preventDefault();
                                                        e.stopPropagation();
                                                    }}
                                                    onClick={() => {
                                                        if (showEndCalendar) {
                                                            setShowEndCalendar(false);
                                                            setShowStartCalendar(true);
                                                        } else {
                                                            setShowStartCalendar(prev => !prev);
                                                        }
                                                    }}
                                                    className="strategy-editor-file-selector-button"
                                                    title="백테스팅 시작 시간 입력"
                                                    aria-label="백테스팅 시작 시간 입력"
                                                    disabled={engineConfig.useBacktestPeriodStart}
                                                >
                                                    <CalendarIcon size={18}/>
                                                </button>
                                            </div>
                                        </div>
                                    </div>
                                </div>

                                <div className="strategy-editor-period-column">
                                    <label className="checkbox-container"
                                           style={{display: 'flex', alignItems: 'center'}}
                                    >
                                        <input
                                            type="checkbox"
                                            checked={engineConfig.useBacktestPeriodEnd}
                                            onChange={(e) => {
                                                handleConfigChange('useBacktestPeriodEnd', e.target.checked);
                                            }}
                                            className="custom-checkbox"
                                        />
                                        <span className="checkbox-label" style={{fontSize: '12px'}}>끝까지</span>
                                    </label>

                                    <div
                                        className={`strategy-editor-period-field ${engineConfig.useBacktestPeriodEnd ? 'disabled' : ''}`}>
                                        <div className="strategy-editor-file-selector">
                                            <Input
                                                type="text"
                                                value={engineConfig.backtestPeriodEnd}
                                                readOnly
                                                tabIndex={-1}
                                                onFocus={(e) => e.currentTarget.blur()}
                                                onMouseDown={(e) => e.preventDefault()}
                                                disabled={engineConfig.useBacktestPeriodEnd}
                                                onChange={(e) => {
                                                    const value = e.target.value;
                                                    handleConfigChange('backtestPeriodEnd', value);
                                                    handleConfigChange('useBacktestPeriodEnd', !value.trim());
                                                }}
                                                placeholder="백테스팅 종료 시간"
                                                className="strategy-editor-input w-full"
                                                style={{fontSize: '13px'}}
                                            />

                                            <div className="strategy-editor-file-selector-buttons">
                                                <button
                                                    onMouseDown={(e) => {
                                                        e.preventDefault();
                                                        e.stopPropagation();
                                                    }}
                                                    onClick={() => {
                                                        if (showStartCalendar) {
                                                            setShowStartCalendar(false);
                                                            setShowEndCalendar(true);
                                                        } else {
                                                            setShowEndCalendar(prev => !prev);
                                                        }
                                                    }}
                                                    className="strategy-editor-file-selector-button"
                                                    title="백테스팅 종료 시간 입력"
                                                    aria-label="백테스팅 종료 시간 입력"
                                                    disabled={engineConfig.useBacktestPeriodEnd}
                                                >
                                                    <CalendarIcon size={18}/>
                                                </button>
                                            </div>
                                        </div>
                                    </div>
                                </div>
                            </div>
                        </div>
                    </div>

                    <div
                        className="strategy-editor-row-item"
                        style={{alignItems: 'center', justifyContent: 'flex-end', textAlign: 'right', height: '100%'}}
                    >
                        <div className="strategy-editor-label">바 돋보기 기능</div>
                    </div>

                    <div className="strategy-editor-toggle-container">
                        <label className="strategy-editor-toggle-switch">
                            <input
                                type="checkbox"
                                checked={engineConfig.useBarMagnifier}
                                onChange={(e) => handleConfigChange('useBarMagnifier', e.target.checked)}
                            />
                            <span className="strategy-editor-toggle-slider"></span>
                        </label>
                    </div>

                    <div
                        className="strategy-editor-row-item"
                        style={{alignItems: 'center', justifyContent: 'flex-end', textAlign: 'right'}}
                    >
                        <div className="strategy-editor-label">초기 자금</div>
                    </div>

                    <div style={{width: '50%'}}>
                        <NumericInput
                            id="initial-balance"
                            value={initialBalanceInput}
                            onChange={(v) => setInitialBalanceInput(v)}
                            onBlur={() => {
                                const v = initialBalanceInput.trim();
                                handleConfigChange('initialBalance', parseFloat(v) || undefined);
                            }}
                            placeholder="초기 자금"
                            unit="$"
                            allowedChars="0123456789."
                            className="strategy-editor-input w-full"
                        />
                    </div>

                    <div
                        className="strategy-editor-row-item"
                        style={{alignItems: 'initial', justifyContent: 'flex-end', textAlign: 'right'}}
                    >
                        <div className="strategy-editor-label">수수료율</div>
                    </div>

                    <div>
                        <div className="flex items-center gap-3">
                            <div
                                className="strategy-editor-percentage-column"
                                style={{gap: '6px'}}
                            >
                                <span style={{color: 'rgb(255 255 255 / 70%)', fontSize: '13px'}}>테이커</span>

                                <NumericInput
                                    id="taker-fee"
                                    value={takerFeeInput}
                                    onChange={(v) => setTakerFeeInput(v)}
                                    onBlur={() => {
                                        const v = takerFeeInput.trim();
                                        handleConfigChange('takerFeePercentage', parseFloat(v) || undefined);
                                    }}
                                    placeholder="테이커 수수료율"
                                    unit="%"
                                    allowedChars="0123456789."
                                    className="strategy-editor-input w-full"
                                />
                            </div>

                            <div
                                className="strategy-editor-percentage-column"
                                style={{gap: '6px'}}
                            >
                                <span style={{color: 'rgb(255 255 255 / 70%)', fontSize: '13px'}}>메이커</span>

                                <NumericInput
                                    id="maker-fee"
                                    value={makerFeeInput}
                                    onChange={(v) => setMakerFeeInput(v)}
                                    onBlur={() => {
                                        const v = makerFeeInput.trim();
                                        handleConfigChange('makerFeePercentage', parseFloat(v) || undefined);
                                    }}
                                    placeholder="메이커 수수료율"
                                    unit="%"
                                    allowedChars="0123456789."
                                    className="strategy-editor-input w-full"
                                />
                            </div>
                        </div>
                    </div>

                    <div
                        className="strategy-editor-row-item"
                        style={{alignItems: 'initial', justifyContent: 'flex-end', textAlign: 'right'}}
                    >
                        <div className="strategy-editor-label">슬리피지 모델</div>
                    </div>

                    <div style={{display: 'flex', flexDirection: 'column', gap: '12px'}}>
                        <div style={{width: '30%'}}>
                            <div
                                className="strategy-editor-dropdown-field"
                                ref={slippageDropdownRef}
                                style={{width: '100%'}}
                            >
                                <div
                                    className="strategy-editor-dropdown-select"
                                    onClick={() => setIsSlippageDropdownOpen(!isSlippageDropdownOpen)}
                                >
                                    {engineConfig.slippageModel === 'PercentageSlippage' ? '퍼센트 슬리피지' : '시장 충격 슬리피지'}
                                </div>

                                {isSlippageDropdownOpen && (
                                    <div className="strategy-editor-dropdown-options">
                                        <div
                                            className={`strategy-editor-dropdown-option ${engineConfig.slippageModel === 'PercentageSlippage' ? 'selected' : ''}`}
                                            onClick={() => {
                                                handleConfigChange('slippageModel', 'PercentageSlippage');
                                                setIsSlippageDropdownOpen(false);
                                            }}>
                                            퍼센트 슬리피지
                                        </div>

                                        <div
                                            className={`strategy-editor-dropdown-option ${engineConfig.slippageModel === 'MarketImpactSlippage' ? 'selected' : ''}`}
                                            onClick={() => {
                                                handleConfigChange('slippageModel', 'MarketImpactSlippage');
                                                setIsSlippageDropdownOpen(false);
                                            }}>
                                            시장 충격 슬리피지
                                        </div>
                                    </div>
                                )}
                            </div>
                        </div>

                        {/* 입력 필드 통합 */}
                        <div>
                            {engineConfig.slippageModel === 'PercentageSlippage' ? (
                                <div className="flex items-start gap-3">
                                    <div
                                        className="strategy-editor-percentage-column"
                                        style={{gap: '6px'}}
                                    >
                                        <span style={{color: 'rgb(255 255 255 / 70%)', fontSize: '13px'}}>테이커</span>

                                        <NumericInput
                                            id="slippage-taker"
                                            value={slippageTakerInput}
                                            onChange={(v) => setSlippageTakerInput(v)}
                                            onBlur={() => {
                                                const v = slippageTakerInput.trim();
                                                handleConfigChange('slippageTakerPercentage', parseFloat(v) || undefined);
                                            }}
                                            placeholder="테이커 슬리피지율"
                                            unit="%"
                                            allowedChars="0123456789."
                                            className="strategy-editor-input w-full"
                                        />
                                    </div>

                                    <div
                                        className="strategy-editor-percentage-column"
                                        style={{gap: '6px'}}
                                    >
                                        <span style={{color: 'rgb(255 255 255 / 70%)', fontSize: '13px'}}>메이커</span>

                                        <NumericInput
                                            id="slippage-maker"
                                            value={slippageMakerInput}
                                            onChange={(v) => setSlippageMakerInput(v)}
                                            onBlur={() => {
                                                const v = slippageMakerInput.trim();
                                                handleConfigChange('slippageMakerPercentage', parseFloat(v) || undefined);
                                            }}
                                            placeholder="메이커 슬리피지율"
                                            unit="%"
                                            allowedChars="0123456789."
                                            className="strategy-editor-input w-full"
                                        />
                                    </div>
                                </div>
                            ) : (
                                <div style={{width: '30%'}}>
                                    <div style={{display: 'flex', flexDirection: 'column', gap: '6px'}}>
                                        <span style={{color: 'rgb(255 255 255 / 70%)', fontSize: '13px'}}>스트레스 계수</span>

                                        <NumericInput
                                            id="slippage-stress"
                                            value={slippageStressInput}
                                            onChange={(v) => setSlippageStressInput(v)}
                                            onBlur={() => {
                                                const v = slippageStressInput.trim();
                                                handleConfigChange('slippageStressMultiplier', parseFloat(v) || undefined);
                                            }}
                                            placeholder="스트레스 계수"
                                            allowedChars="0123456789."
                                            className="strategy-editor-input w-full"
                                        />
                                    </div>
                                </div>
                            )}
                        </div>
                    </div>

                    {/* 수량 검사 옵션 */}
                    <div
                        className="strategy-editor-row-item"
                        style={{
                            alignItems: 'initial',
                            justifyContent: 'flex-end',
                            textAlign: 'right',
                            height: '100%'
                        }}
                    >
                        <div className="strategy-editor-label" style={{textAlign: 'right'}}>수량 검사 옵션</div>
                    </div>

                    <div
                        className="checkbox-group"
                        style={{display: 'flex', flexDirection: 'column', gap: '12px'}}
                    >
                        <div style={{display: 'flex', minHeight: '24px'}}>
                            <label
                                className="checkbox-container"
                                style={{display: 'flex', alignItems: 'center', width: 'auto', flex: '0 0 auto'}}
                            >
                                <input
                                    type="checkbox"
                                    checked={engineConfig.checkMarketMaxQty}
                                    onChange={(e) => handleConfigChange('checkMarketMaxQty', e.target.checked)}
                                    className="custom-checkbox"
                                />

                                <span className="checkbox-label">시장가 최대 수량 검사</span>
                            </label>

                            <label
                                className="checkbox-container"
                                style={{display: 'flex', alignItems: 'center', width: 'auto', flex: '0 0 auto'}}
                            >
                                <input
                                    type="checkbox"
                                    checked={engineConfig.checkMarketMinQty}
                                    onChange={(e) => handleConfigChange('checkMarketMinQty', e.target.checked)}
                                    className="custom-checkbox"
                                />

                                <span className="checkbox-label">시장가 최소 수량 검사</span>
                            </label>
                        </div>

                        <div style={{display: 'flex', minHeight: '24px'}}>
                            <label
                                className="checkbox-container"
                                style={{display: 'flex', alignItems: 'center', width: 'auto', flex: '0 0 auto'}}
                            >
                                <input
                                    type="checkbox"
                                    checked={engineConfig.checkLimitMaxQty}
                                    onChange={(e) => handleConfigChange('checkLimitMaxQty', e.target.checked)}
                                    className="custom-checkbox"
                                />

                                <span className="checkbox-label">지정가 최대 수량 검사</span>
                            </label>

                            <label
                                className="checkbox-container"
                                style={{display: 'flex', alignItems: 'center', width: 'auto', flex: '0 0 auto'}}
                            >
                                <input
                                    type="checkbox"
                                    checked={engineConfig.checkLimitMinQty}
                                    onChange={(e) => handleConfigChange('checkLimitMinQty', e.target.checked)}
                                    className="custom-checkbox"
                                />

                                <span className="checkbox-label">지정가 최소 수량 검사</span>
                            </label>
                        </div>

                        <div style={{display: 'flex', minHeight: '24px'}}>
                            <label
                                className="checkbox-container"
                                style={{display: 'flex', alignItems: 'center', width: 'auto', flex: '0 0 auto'}}
                            >
                                <input
                                    type="checkbox"
                                    checked={engineConfig.checkMinNotionalValue}
                                    onChange={(e) => handleConfigChange('checkMinNotionalValue', e.target.checked)}
                                    className="custom-checkbox"
                                />

                                <span className="checkbox-label">최소 명목 가치 검사</span>
                            </label>
                        </div>
                    </div>

                    {/* 바 데이터 중복 검사 */}
                    <div
                        className="strategy-editor-row-item"
                        style={{
                            alignItems: 'initial',
                            justifyContent: 'flex-end',
                            textAlign: 'right',
                            height: '100%'
                        }}
                    >
                        <div className="strategy-editor-label" style={{textAlign: 'right'}}>바 데이터 중복 검사</div>
                    </div>

                    <div
                        className="checkbox-group"
                        style={{display: 'flex', flexDirection: 'column', gap: '12px'}}
                    >
                        <div style={{display: 'flex', minHeight: '24px'}}>
                            <label
                                className="checkbox-container"
                                style={{display: 'flex', alignItems: 'center', width: 'auto', flex: '0 0 auto'}}
                            >

                                <input
                                    type="checkbox"
                                    checked={engineConfig.checkSameBarDataWithTarget}
                                    onChange={(e) => handleConfigChange('checkSameBarDataWithTarget', e.target.checked)}
                                    className="custom-checkbox"
                                />

                                <span className="checkbox-label">마크 가격 바 데이터와 목표 바 데이터 중복 검사</span>
                            </label>
                        </div>

                        <div style={{display: 'flex', minHeight: '24px'}}>
                            <label
                                className="checkbox-container"
                                style={{display: 'flex', alignItems: 'center', width: 'auto', flex: '0 0 auto'}}
                            >
                                <input
                                    type="checkbox"
                                    checked={engineConfig.checkSameBarDataTrading}
                                    onChange={(e) => handleConfigChange('checkSameBarDataTrading', e.target.checked)}
                                    className="custom-checkbox"
                                />

                                <span className="checkbox-label">심볼 간 트레이딩 바 데이터 중복 검사</span>
                            </label>

                            <label
                                className="checkbox-container"
                                style={{display: 'flex', alignItems: 'center', width: 'auto', flex: '0 0 auto'}}
                            >
                                <input
                                    type="checkbox"
                                    checked={engineConfig.checkSameBarDataMagnifier}
                                    onChange={(e) => handleConfigChange('checkSameBarDataMagnifier', e.target.checked)}
                                    className="custom-checkbox"
                                />

                                <span className="checkbox-label">심볼 간 돋보기 바 데이터 중복 검사</span>
                            </label>
                        </div>

                        <div style={{display: 'flex', minHeight: '24px'}}>
                            <label
                                className="checkbox-container"
                                style={{display: 'flex', alignItems: 'center', width: 'auto', flex: '0 0 auto'}}
                            >
                                <input
                                    type="checkbox"
                                    checked={engineConfig.checkSameBarDataReference}
                                    onChange={(e) => handleConfigChange('checkSameBarDataReference', e.target.checked)}
                                    className="custom-checkbox"
                                />

                                <span className="checkbox-label">심볼 간 참조 바 데이터 중복 검사</span>
                            </label>

                            <label
                                className="checkbox-container"
                                style={{display: 'flex', alignItems: 'center', width: 'auto', flex: '0 0 auto'}}>

                                <input
                                    type="checkbox"
                                    checked={engineConfig.checkSameBarDataMarkPrice}
                                    onChange={(e) => handleConfigChange('checkSameBarDataMarkPrice', e.target.checked)}
                                    className="custom-checkbox"
                                />

                                <span className="checkbox-label">심볼 간 마크 가격 바 데이터 중복 검사</span>
                            </label>
                        </div>
                    </div>
                </div>

                <div className="strategy-editor-grid-divider-abs" aria-hidden="true"
                     style={{left: dividerLeft !== null ? dividerLeft + 'px' : undefined}}></div>
            </div>

            {/* 캘린더 모달 (시작/종료) */}
            {showStartCalendar && (
                <StrategyEditorCalendar
                    onClose={() => setShowStartCalendar(false)}
                    lastSelectedDate={(startCalendarLastSelectedDate) || getStartCalendarInitial().date}
                    lastSelectedTime={(startCalendarLastSelectedTime) || getStartCalendarInitial().timeStr}
                    onDateTimeSelected={handleStartDateTimeSelected}
                    mode={'start'}
                    timeframe={(function () {
                        // 우선 트레이딩 바데이터의 타임프레임을 사용
                        const trading = barDataConfigs.find(c => c.barDataType === '트레이딩' || c.barDataType === undefined);
                        if (trading && trading.timeframe) {
                            // 값/단위 중 하나라도 비어있으면 미설정으로 간주
                            const tfString = timeframeToString(trading.timeframe);
                            if (tfString && tfString.trim()) return tfString;
                        }

                        // 트레이딩 바 데이터의 타임프레임이 설정되지 않은 경우 1d 사용
                        return '1d';
                    })()}
                />
            )}

            {showEndCalendar && (
                <StrategyEditorCalendar
                    onClose={() => setShowEndCalendar(false)}
                    lastSelectedDate={(endCalendarLastSelectedDate) || getEndCalendarInitial().date}
                    lastSelectedTime={(endCalendarLastSelectedTime) || getEndCalendarInitial().timeStr}
                    onDateTimeSelected={handleEndDateTimeSelected}
                    mode={'end'}
                    timeframe={(function () {
                        const tradingBarData = barDataConfigs.find(config => config.barDataType === BarDataType.TRADING || config.barDataType === undefined);
                        if (tradingBarData && tradingBarData.timeframe && tradingBarData.timeframe.value !== null && tradingBarData.timeframe.unit !== TimeframeUnit.NULL) {
                            return timeframeToString(tradingBarData.timeframe);
                        }

                        return '1d';
                    })()}
                />
            )}
        </div>
    );
}
