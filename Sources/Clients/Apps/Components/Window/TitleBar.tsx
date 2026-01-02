import {useEffect, useState} from 'react';
import './TitleBar.css';

// 상단 커스텀 타이틀 바 (높이 40px)
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
