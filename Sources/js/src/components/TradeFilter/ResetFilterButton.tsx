import React, {useState} from 'react';
import {VscRefresh} from 'react-icons/vsc';
import './ResetFilterButton.css';

interface ResetFilterButtonProps {
    onClick: () => void;
}

const ResetFilterButton: React.FC<ResetFilterButtonProps> = ({onClick}) => {
    const [isRotating, setIsRotating] = useState(false);

    const handleClick = () => {
        // 이미 회전 중이면 무시
        if (isRotating) return;

        // 회전 애니메이션 시작
        setIsRotating(true);

        // 원래 클릭 핸들러 호출
        onClick();

        // 애니메이션이 끝난 후 상태 초기화
        setTimeout(() => {
            setIsRotating(false);
        }, 600); // 애니메이션 시간과 일치
    };

    return (
        <div className="reset-filter-button-container sidebar-button-container">
            <button
                onClick={handleClick}
                className={`reset-filter-button ${isRotating ? 'rotating' : ''}`}
                aria-label="필터 초기화"
                disabled={isRotating}
            >
                <VscRefresh className="reset-icon" size={15} style={{transform: 'rotate(90deg)', strokeWidth: '0.25'}}/>
            </button>
        </div>
    );
};

export default ResetFilterButton;
