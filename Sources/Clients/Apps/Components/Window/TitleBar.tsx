import React, {useEffect, useMemo, useRef, useState} from 'react';
import './TitleBar.css';
import {useResults} from '@/Contexts/ResultsContext';
import {useDropdownAutoScroll} from '../StrategyEditor/useDropdownAutoScroll';

// Results 선택 컴포넌트
function ResultsSelector() {
    const {results, selectedResult, selectResult, refreshResults} = useResults();
    const [isLoading, setIsLoading] = useState(false);

    const formatResultName = (name: string) => {
        if (!name) {
            return '';
        }

        // yyyymmdd_hhmmss -> yyyy-mm-dd hh:mm:ss
        const m8 = name.match(/^(\d{4})(\d{2})(\d{2})_(\d{2})(\d{2})(\d{2})$/);
        if (m8) {
            return `${m8[1]}-${m8[2]}-${m8[3]} ${m8[4]}:${m8[5]}:${m8[6]}`;
        }

        return name;
    };

    const options = useMemo(() => {
        if (!results || results.length === 0) {
            return [{name: '', label: '백테스팅 결과 없음'}];
        }

        return results.map(r => ({name: r.name, label: formatResultName(r.name)}));
    }, [results]);


    const handleRefresh = async () => {
        setIsLoading(true);

        try {
            await refreshResults();
        } finally {
            setIsLoading(false);
        }
    };

    // 드롭다운 상호작용 상태/레퍼런스
    const ddRef = useRef<HTMLDivElement | null>(null);
    const [isOpen, setIsOpen] = useState(false);
    const [localSelecting, setLocalSelecting] = useState(false);

    // 드롭다운이 열릴 때 선택된 항목으로 자동 스크롤
    useDropdownAutoScroll(ddRef, isOpen, '.titlebar-dropdown-options', '.titlebar-dropdown-option.selected');

    useEffect(() => {
        const titlebarRoot = document.querySelector('.titlebar-root');

        if (!isOpen) {
            // 닫힐 때는 반드시 드래그 복원
            titlebarRoot?.classList.remove('titlebar-disable-drag');
            return;
        }

        // 드롭다운을 열면 자동으로 결과 목록을 새로고침
        // 실패해도 UI가 멈추지 않도록 방어적으로 호출
        handleRefresh().catch(() => {
        });

        // 타이틀바의 drag 영역 때문에 document 이벤트가 도달하지 않는 문제를
        // 해결하기 위해 드롭다운이 열리는 동안에만 타이틀바의 drag를 비활성화.
        titlebarRoot?.classList.add('titlebar-disable-drag');

        const onDocDown = (ev: MouseEvent) => {
            if (!ddRef.current) {
                return;
            }

            if (!ddRef.current.contains(ev.target as Node)) {
                setIsOpen(false);
            }
        };

        const onKey = (ev: KeyboardEvent) => {
            if (ev.key === 'Escape') {
                setIsOpen(false);
            }
        };

        document.addEventListener('mousedown', onDocDown);
        document.addEventListener('keydown', onKey);

        return () => {
            document.removeEventListener('mousedown', onDocDown);
            document.removeEventListener('keydown', onKey);

            titlebarRoot?.classList.remove('titlebar-disable-drag');
        };
    }, [isOpen]);


    const onToggle = () => {
        // 선택 처리 중일 때만 토글 차단
        if (localSelecting) {
            return;
        }

        setIsOpen(s => !s);
    };

    const onKeyDownToggle = (e: React.KeyboardEvent) => {
        if (e.key === 'Enter' || e.key === ' ') {
            e.preventDefault();

            onToggle();
        } else if (e.key === 'Escape') {
            setIsOpen(false);
        }
    };

    const onSelectOption = async (name: string) => {
        // 새로고침 중이면 선택 무시
        if (isLoading) {
            return;
        }

        // 빈값은 null 전달
        const val = name || null;
        setLocalSelecting(true);
        setIsOpen(false);

        try {
            await selectResult(val);
        } catch (err) {
            console.error('selectResult failed', err);
        } finally {
            setLocalSelecting(false);
        }
    };

    return (
        <div style={{display: 'flex', alignItems: 'center', gap: 6}}>
            <div
                className="titlebar-dropdown-field"
                style={{
                    minWidth: 180,
                }}
                ref={ddRef}
            >
                <div
                    className={`titlebar-dropdown-select ${localSelecting ? 'loading' : ''}`}
                    role="button"
                    tabIndex={0}
                    aria-haspopup="menu"
                    aria-expanded={isOpen}
                    aria-busy={localSelecting || isLoading}
                    onClick={onToggle}
                    onKeyDown={onKeyDownToggle}
                >
                    {localSelecting ? (
                        <div className="dropdown-loading-wrapper" aria-hidden={false}>
                            <span className="chart-loading-indicator" style={{width: 14, height: 14, borderWidth: 2}}
                                  aria-hidden="true"/>
                            <span className="sr-only">로딩 중</span>
                        </div>
                    ) : (
                        options.find(o => o.name === selectedResult)?.label || '백테스팅 결과 선택'
                    )}
                </div>

                {isOpen && (
                    <div className="titlebar-dropdown-options" role="menu">
                        {(!options || options.length === 0 || (options.length === 1 && options[0].name === '')) ? (
                            <div className="strategy-editor-dropdown-option">백테스팅 결과 없음</div>
                        ) : (
                            options.map(o => (
                                <div
                                    key={o.name}
                                    role="menuitem"
                                    className={`titlebar-dropdown-option ${selectedResult === o.name ? 'selected' : ''}`}
                                    onClick={() => onSelectOption(o.name)}
                                >
                                    <span className="titlebar-dropdown-option-label">{o.label}</span>
                                </div>
                            ))
                        )}
                    </div>
                )}
            </div>
        </div>
    );
}

// 상단 커스텀 타이틀 바
export default function TitleBar() {
    const [isMax, setIsMax] = useState(false);

    useEffect(() => {
        // 초기 상태 확인
        (async () => {
            try {
                if ((window as any).electronAPI?.isMaximized) {
                    const r = await (window as any).electronAPI.isMaximized();
                    setIsMax(Boolean(r));
                }
            } catch (e) {
                // 무시
            }
        })();

        // 이벤트 리스너
        const onMax = () => setIsMax(true);
        const onUnmax = () => setIsMax(false);

        (window as any).electronAPI?.onWindowMaximized?.(onMax);
        (window as any).electronAPI?.onWindowUnmaximized?.(onUnmax);

        return () => {
            // 프리로드에서 off가 제공되지 않으므로 기본 해제는 생략
        };
    }, []);

    const handleMin = () => {
        (window as any).electronAPI?.minimize?.();
    };

    const handleMax = () => {
        (window as any).electronAPI?.toggleMaximize?.();
    };

    const handleClose = () => {
        (window as any).electronAPI?.close?.();
    };

    return (
        <div className="titlebar-root">
            <div className="titlebar-left">
                <div className="titlebar-icon"/>
            </div>

            <div className="titlebar-controls">
                <div id="titlebar-actions-portal" className="flex items-center"/>

                <div style={{display: 'flex', alignItems: 'center'}}>
                    <ResultsSelector/>
                </div>

                <button className="tb-btn tb-min" onClick={handleMin} title="최소화" aria-label="최소화">
                    <svg
                        xmlns="http://www.w3.org/2000/svg"
                        width="16"
                        height="16"
                        viewBox="0 0 16 16"
                        fill="none"
                    >
                        <path d="M4 8h8" stroke="currentColor" strokeWidth="1.4" strokeLinecap="round"/>
                    </svg>
                </button>

                <button className="tb-btn tb-max" onClick={handleMax} title={isMax ? '복원' : '최대화'}
                        aria-label={isMax ? '복원' : '최대화'}>
                    {isMax ? (
                        // 복원 아이콘
                        <svg
                            xmlns="http://www.w3.org/2000/svg"
                            viewBox="0 0 12 12"
                            fill="none"
                            stroke="currentColor"
                            strokeWidth={1}
                            strokeLinecap="round"
                            strokeLinejoin="round"
                        >
                            <path d="M3 1H10.2Q11 1 11 1.8V9"/>
                            <rect x={1} y={3} width={8} height={8} rx={0.8} ry={0.8}/>
                        </svg>
                    ) : (
                        // 최대화 아이콘
                        <svg
                            xmlns="http://www.w3.org/2000/svg"
                            viewBox="0 0 12 12"
                            fill="none"
                            stroke="currentColor"
                            strokeWidth={1}
                            strokeLinecap="round"
                            strokeLinejoin="round"
                            style={{shapeRendering: 'geometricPrecision'}}
                        >
                            <rect x={1.2} y={1.2} width={8.8} height={8.8} rx={1.0} ry={1.0}/>
                        </svg>
                    )}
                </button>

                <button className="tb-btn tb-close" onClick={handleClose} title="닫기" aria-label="닫기">
                    <svg
                        xmlns="http://www.w3.org/2000/svg"
                        width="16"
                        height="16"
                        viewBox="0 0 16 16"
                        fill="none"
                    >
                        <path d="M4 4L12 12M12 4L4 12" stroke="currentColor" strokeWidth="1.4" strokeLinecap="round"
                              strokeLinejoin="round"/>
                    </svg>
                </button>
            </div>
        </div>
    );
}
