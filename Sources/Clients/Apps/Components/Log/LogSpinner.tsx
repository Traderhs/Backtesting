import React, {useEffect} from 'react';

// CSS 키프레임 애니메이션 정의
const spinnerAnimation = `
  @keyframes spin {
    0% { transform: rotate(0deg) translateZ(0); }
    100% { transform: rotate(360deg) translateZ(0); }
  }
`;

// Log 탭 전용 로딩 스피너 컴포넌트 - CSS 애니메이션으로 최적화
const LogSpinner: React.FC = () => {
    useEffect(() => {
        // CSS 애니메이션 스타일 주입 (한 번만)
        const styleId = 'log-spinner-animation';
        if (!document.getElementById(styleId)) {
            const style = document.createElement('style');
            style.id = styleId;
            style.textContent = spinnerAnimation;
            document.head.appendChild(style);
        }

        // 컴포넌트 마운트 시 스크롤 방지
        document.body.style.overflow = 'hidden';

        return () => {
            document.body.style.overflow = '';
        };
    }, []);

    return (
        <div
            style={{
                width: '100%',
                height: '100%',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                flexDirection: 'column',
                gap: '20px',
                position: 'fixed',
                top: 0,
                left: 0,
                right: 0,
                bottom: 0,
                zIndex: 9999,
                background: 'rgba(17, 17, 17, 0.9)',
                pointerEvents: 'none'
            }}
        >
            {/* 반투명 배경 오버레이 */}
            <div
                style={{
                    position: 'absolute',
                    top: 0,
                    left: 0,
                    right: 0,
                    bottom: 0,
                    backgroundColor: 'rgba(17, 17, 17, 0.85)',
                    backdropFilter: 'blur(4px)',
                    WebkitBackdropFilter: 'blur(4px)',
                    pointerEvents: 'none'
                }}
            />

            {/* Log 전용 로딩 스피너 - CSS 애니메이션으로 최적화 */}
            <div
                style={{
                    position: 'relative',
                    zIndex: 1010,
                    width: '42px',
                    height: '42px',
                    borderRadius: '50%',
                    border: '3px solid rgba(20, 20, 20, 0.15)',
                    borderTopColor: '#FFD700',
                    margin: '0 auto',
                    boxShadow: '0 0 20px rgba(255, 215, 0, 0.15)',
                    transform: 'translateZ(0)',
                    willChange: 'transform',
                    backfaceVisibility: 'hidden',
                    WebkitBackfaceVisibility: 'hidden',
                    animation: 'spin 1s cubic-bezier(0.4, 0.1, 0.3, 1) infinite'
                }}
            />
        </div>
    );
};

export default LogSpinner;
