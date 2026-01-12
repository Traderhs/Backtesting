import {useState} from 'react';
import {VscRefresh} from 'react-icons/vsc';
import './PathResetButton.css';

interface PathResetButtonProps {
    onClick: () => void;
}

export default function PathResetButton({onClick}: PathResetButtonProps) {
    const [isRotating, setIsRotating] = useState(false);

    const handleClick = () => {
        if (isRotating) return;

        setIsRotating(true);
        onClick();

        setTimeout(() => {
            setIsRotating(false);
        }, 600);
    };

    return (
        <div className="strategy-editor-path-reset-button-container">
            <button
                onClick={handleClick}
                className={`strategy-editor-path-reset-button ${isRotating ? 'rotating' : ''}`}
                aria-label="경로 초기화"
                title="기본 경로로 초기화"
            >
                <VscRefresh size={15} style={{transform: 'rotate(90deg)', strokeWidth: '0.25'}}/>
            </button>
        </div>
    );
}
