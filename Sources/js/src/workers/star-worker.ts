interface Star {
    relX: number;
    relY: number;
    radius: number;
    opacity: number;
    opacityDirection: number;
    opacitySpeed: number;
}

let canvas: OffscreenCanvas | null = null;
let ctx: OffscreenCanvasRenderingContext2D | null = null;
let stars: Star[] = [];
let animationFrameId: number | null = null;
let lastFrameTime = 0;
let FPS_LIMIT = 30; // 프레임 레이트 제한 (const에서 let으로 변경)
let FRAME_TIME = 1000 / FPS_LIMIT; // 프레임당 시간 (ms)
let canvasWidth = 0;
let canvasHeight = 0;
let isPageVisible = true; // 페이지 가시성 상태 추적

// FPS 제한 변경 함수
function updateFpsLimit(newLimit: number) {
    FPS_LIMIT = newLimit;
    FRAME_TIME = 1000 / FPS_LIMIT;
}

function initializeStars() {
    if (!canvasWidth || !canvasHeight) {
        return;
    }

    // 별이 아직 생성되지 않았을 때만 생성
    if (stars.length === 0) {
        stars = Array.from({length: 400}).map(() => ({
            relX: Math.random(),
            relY: Math.random(),
            radius: Math.random() * 1.5 + 0.5,
            opacity: Math.random() * 0.7 + 0.1,
            opacityDirection: Math.random() > 0.5 ? 1 : -1,
            opacitySpeed: Math.random() * 0.01 + 0.01
        }));
    }
}

function animateStars(timestamp: number) {
    if (!canvas || !ctx || !canvasWidth || !canvasHeight) {
        // 필수 요소가 준비되지 않으면 다음 프레임 요청만 하고 종료
        animationFrameId = requestAnimationFrame(animateStars);
        return;
    }

    // 페이지가 보이지 않는 상태면 애니메이션 업데이트 감소
    if (!isPageVisible) {
        // 페이지가 보이지 않을 때는 초당 5프레임으로 제한
        const reducedFPS = 5;
        const reducedFrameTime = 1000 / reducedFPS;

        const elapsed = timestamp - lastFrameTime;
        if (elapsed < reducedFrameTime) {
            animationFrameId = requestAnimationFrame(animateStars);
            return;
        }

        lastFrameTime = timestamp - (elapsed % reducedFrameTime);
    } else {
        // 프레임 제한 로직 (페이지가 보일 때)
        const elapsed = timestamp - lastFrameTime;
        if (elapsed < FRAME_TIME) {
            animationFrameId = requestAnimationFrame(animateStars);
            return;
        }

        lastFrameTime = timestamp - (elapsed % FRAME_TIME);
    }

    // 캔버스 초기화
    ctx.clearRect(0, 0, canvasWidth, canvasHeight);

    // ctx가 null이 아님을 다시 한번 확인 (TypeScript를 위해)
    if (!ctx) {
        console.error("Worker context became null unexpectedly");

        // 애니메이션 루프는 계속 시도
        animationFrameId = requestAnimationFrame(animateStars);
        return;
    }

    // 별 그리기
    stars.forEach(star => {
        // 투명도 업데이트
        star.opacity += star.opacitySpeed * star.opacityDirection;

        if (star.opacity > 0.9 || star.opacity < 0.1) {
            star.opacityDirection *= -1;
            star.opacity = Math.max(0.1, Math.min(0.9, star.opacity));
        }

        // 현재 위치 계산
        const currentX = star.relX * canvasWidth;
        const currentY = star.relY * canvasHeight;

        // 발광 효과 및 그리기 (Optional Chaining 사용)
        ctx?.save(); // 상태 저장
        ctx?.beginPath();
        ctx?.arc(currentX, currentY, star.radius, 0, Math.PI * 2);

        if (ctx) { // fillStyle, shadowColor, shadowBlur는 null이면 설정 불가
            ctx.fillStyle = `rgba(255, 215, 0, ${star.opacity})`;
            ctx.shadowColor = `rgba(255, 215, 0, ${star.opacity * 0.7})`;
            ctx.shadowBlur = star.radius * 4;
        }

        ctx?.fill();

        if (ctx) { // 그림자 리셋
            ctx.shadowBlur = 0;
            ctx.shadowColor = 'transparent';
        }

        ctx?.restore(); // 상태 복원
    });

    // 페이지가 보이지 않으면 비트맵 전송을 건너뛰기
    if (!isPageVisible) {
        // 다음 프레임 요청만 하고 이미지 전송은 건너뜀
        animationFrameId = requestAnimationFrame(animateStars);
        return;
    }

    // 렌더링된 비트맵을 메인 스레드로 전송
    try {
        const bitmap = canvas.transferToImageBitmap();

        // postMessage 호출 시 옵션 객체 사용
        self.postMessage({type: 'render', bitmap}, {transfer: [bitmap]});
    } catch (error) {
        console.error("Failed to transfer bitmap:", error);

        // 전송 실패 시 루프가 멈추지 않도록 다음 프레임 요청
        animationFrameId = requestAnimationFrame(animateStars);
        return; // 아래의 다음 프레임 요청 중복 방지
    }

    // 다음 프레임 요청
    animationFrameId = requestAnimationFrame(animateStars);
}

self.onmessage = (event) => {
    const {type, width, height} = event.data;

    if (type === 'init') {
        if (!width || !height) {
            console.error("Worker init message missing width or height");

            return;
        }

        // 워커 자체적으로 OffscreenCanvas 생성
        canvas = new OffscreenCanvas(width, height);

        const context = canvas.getContext('2d');
        if (!context) {
            console.error("Failed to get 2D context from OffscreenCanvas in worker");
            self.postMessage({type: 'error', message: 'Failed to get worker context'});
            canvas = null; // 생성 실패 시 null 처리
            return;
        }

        ctx = context as OffscreenCanvasRenderingContext2D;

        canvasWidth = width;
        canvasHeight = height;

        initializeStars();

        // 애니메이션 루프 시작
        if (!animationFrameId) {
            lastFrameTime = performance.now();
            animationFrameId = requestAnimationFrame(animateStars);
        }
    } else if (type === 'resize') {
        if (!width || !height) {
            console.error("Worker resize message missing width or height");
            return;
        }

        // 내부 OffscreenCanvas 크기 조절
        if (canvas) {
            canvas.width = width;
            canvas.height = height;

            canvasWidth = width;
            canvasHeight = height;
            // 별 위치는 상대적이므로 다시 계산할 필요 없음
        }
    } else if (type === 'resumeAnimation') {
        // 페이지가 다시 표시될 때 애니메이션 재개
        isPageVisible = true;

        // 애니메이션 프레임이 없으면 다시 시작
        if (!animationFrameId) {
            lastFrameTime = performance.now();
            animationFrameId = requestAnimationFrame(animateStars);
        }
    } else if (type === 'visibilityChange') {
        // 페이지 가시성 상태 업데이트
        isPageVisible = event.data.isVisible;

        // 애니메이션 중인지 여부 추가 확인
        const isTabAnimating = event.data.isAnimating === true;

        // 탭 애니메이션 중이면 별 애니메이션 프레임 레이트 낮추기
        if (isTabAnimating) {
            updateFpsLimit(15); // 탭 전환 애니메이션 중 FPS 제한
        } else {
            updateFpsLimit(30); // 일반 모드
        }

        // 페이지가 다시 표시될 때 상태 최신화
        if (isPageVisible && !animationFrameId) {
            lastFrameTime = performance.now();
            animationFrameId = requestAnimationFrame(animateStars);
        }
    } else if (type === 'pause') {
        isPageVisible = false;
    } else if (type === 'animatingStateChange') {
        // 애니메이션 상태 변경 시 FPS 조정
        const isTabAnimating = event.data.isAnimating === true;

        if (isTabAnimating) {
            updateFpsLimit(15); // 탭 전환 중 FPS 제한
        } else {
            updateFpsLimit(30); // 일반 모드로 복귀
        }
    }
};

// 워커 종료 시 애니메이션 프레임 정리
self.onclose = () => {
    if (animationFrameId) {
        cancelAnimationFrame(animationFrameId);
    }
};
