import {useCallback, useEffect, useRef} from "react";

// 디바운스 함수 정의 (메인 스레드에서 사용)
function debounce<T extends (...args: any[]) => any>(func: T, wait: number): (...args: Parameters<T>) => void {
    let timeout: ReturnType<typeof setTimeout> | null = null;
    return (...args: Parameters<T>): void => {
        const later = () => {
            timeout = null;
            func(...args);
        };
        if (timeout !== null) {
            clearTimeout(timeout);
        }
        timeout = setTimeout(later, wait);
    };
}

export const StarField = () => {
    const canvasRef = useRef<HTMLCanvasElement>(null);
    const workerRef = useRef<Worker | null>(null);
    const isUnmounting = useRef(false); // 언마운트 상태 추적
    const isPageVisible = useRef(true); // 페이지 가시성 상태 추적
    // 메인 스레드의 캔버스 컨텍스트 저장
    const mainCtxRef = useRef<CanvasRenderingContext2D | null>(null);

    // useCallback으로 resizeCanvas 함수를 메모이제이션
    const resizeCanvas = useCallback(() => {
        const canvas = canvasRef.current;
        if (!canvas || !mainCtxRef.current) return; // 컨텍스트도 확인

        const parent = canvas.parentElement;
        if (parent) {
            const width = parent.clientWidth;
            const height = parent.clientHeight;
            // 메인 캔버스 크기 조절
            canvas.width = width;
            canvas.height = height;

            // 워커에게 리사이즈 메시지 전송 (크기 정보만)
            workerRef.current?.postMessage({type: 'resize', width, height});
        }
    }, []);

    // 디바운스된 resize 핸들러 생성
    const debouncedResizeHandler = useRef(debounce(resizeCanvas, 250)).current;

    // 페이지 가시성 변경 핸들러
    const handleVisibilityChange = useCallback(() => {
        const visible = document.visibilityState === 'visible';
        isPageVisible.current = visible;

        // 워커에게 가시성 변경 메시지 전송
        if (workerRef.current) {
            workerRef.current.postMessage({
                type: 'visibilityChange',
                isVisible: visible
            });

            // 페이지가 다시 보이게 되면 워커에게 애니메이션 재개 알림
            if (visible) {
                workerRef.current.postMessage({type: 'resumeAnimation'});
            }
        }
    }, []);

    useEffect(() => {
        isUnmounting.current = false;
        const canvas = canvasRef.current;
        if (!canvas) return;

        // 페이지 가시성 이벤트 리스너 등록
        document.addEventListener('visibilitychange', handleVisibilityChange);

        // 메인 캔버스의 2D 컨텍스트 가져오기 (최초 1회)
        const ctx = canvas.getContext('2d', {alpha: true, willReadFrequently: false});
        if (!ctx) {
            console.error("Failed to get 2D context from main canvas.");
            return;
        }
        mainCtxRef.current = ctx; // 컨텍스트 저장

        // Web Worker 생성
        const worker = new Worker(new URL('../../Workers/StarWorker.ts', import.meta.url), {type: 'module'});
        workerRef.current = worker;

        // 워커 초기화: 크기 정보만 전송
        try {
            const parent = canvas.parentElement;
            const initialWidth = parent?.clientWidth ?? window.innerWidth;
            const initialHeight = parent?.clientHeight ?? window.innerHeight;
            // 메인 캔버스 초기 크기 설정
            canvas.width = initialWidth;
            canvas.height = initialHeight;

            // 제어권 이전(transferControlToOffscreen) 대신 크기 정보 전송
            worker.postMessage({type: 'init', width: initialWidth, height: initialHeight});
        } catch (error) {
            console.error("Failed to initialize worker:", error);
            worker.terminate();
            workerRef.current = null;
            return;
        }

        // 메시지 핸들러 설정: 워커로부터 렌더링된 비트맵 수신
        worker.onmessage = (event) => {
            const {type, bitmap, message} = event.data;

            if (type === 'render') {
                // 비트맵 렌더링 - 컨텍스트 확인 후 그리기
                const mainCtx = mainCtxRef.current;
                if (!mainCtx || !canvas || isUnmounting.current) {
                    // 언마운트 또는 컨텍스트 없는 경우 비트맵 닫기
                    bitmap.close();
                    return;
                }

                // 현재 페이지가 보이는 상태일 때만 그리기 작업 수행
                if (isPageVisible.current) {
                    // requestAnimationFrame을 사용하여 브라우저 렌더링 주기에 맞춰 그리기
                    requestAnimationFrame(() => {
                        // 메인 스레드 캔버스에 비트맵 그리기
                        mainCtx.clearRect(0, 0, canvas.width, canvas.height);
                        mainCtx.drawImage(bitmap, 0, 0);

                        // 비트맵을 그린 후에 닫아야 함
                        bitmap.close();
                    });
                } else {
                    // 페이지가 보이지 않는 경우 비트맵만 닫음
                    bitmap.close();
                }
            } else if (type === 'error') {
                console.error("Error message from worker:", message);
            }
        };

        // 초기 캔버스 크기 설정
        resizeCanvas();

        // 창 크기 변경 시 디바운스된 핸들러 사용
        window.addEventListener('resize', debouncedResizeHandler);

        // 컴포넌트 언마운트 시 워커 종료 및 리스너 제거
        return () => {
            isUnmounting.current = true;
            window.removeEventListener('resize', debouncedResizeHandler);
            document.removeEventListener('visibilitychange', handleVisibilityChange);

            // 워커 종료
            if (workerRef.current) {
                workerRef.current.terminate();
                workerRef.current = null;
            }
            mainCtxRef.current = null; // 컨텍스트 참조 정리
        };
    }, [debouncedResizeHandler, handleVisibilityChange]);

    return (
        <canvas
            ref={canvasRef}
            className="w-full h-full"
            style={{
                background: 'transparent',
                display: 'block'
            }}
        />
    );
};
