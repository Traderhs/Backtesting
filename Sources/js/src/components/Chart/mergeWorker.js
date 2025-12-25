// mergeWorker.js - Web Worker for data merging operations
// 메인 스레드를 차단하지 않고 데이터 병합을 처리하는 Worker

/**
 * 데이터 병합 로직
 * 데이터를 단순하게 앞 또는 뒤에 이어붙입니다.
 */
function mergeIndicatorData(cache, newData, direction = 'right') {
    // 신규 데이터가 없으면 cache 그대로 반환
    if (newData.length === 0) {
        return cache;
    }

    // 기존 데이터가 없으면 신규 데이터 그대로 반환
    if (cache.length === 0) {
        return newData;
    }

    // 성능 최적화: Array.concat 사용 - 대용량 배열에서 스프레드 연산자보다 효율적
    return direction === 'left'
        ? newData.concat(cache) // 과거 데이터 요청: 새 데이터 + 기존 데이터
        : cache.concat(newData); // 최신 데이터 요청: 기존 데이터 + 새 데이터
}

// 메시지 이벤트 리스너 등록
self.addEventListener('message', function (e) {
    const {operation, cache, newData, direction, indicatorName} = e.data;

    // 현재는 병합 작업만 지원
    if (operation === 'merge') {
        try {
            // 데이터 병합 처리
            const result = mergeIndicatorData(cache, newData, direction);

            // 결과를 메인 스레드로 반환
            self.postMessage({
                success: true,
                result,
                indicatorName,
                operation
            });
        } catch (error) {
            // 오류 발생 시 오류 메시지 반환
            self.postMessage({
                success: false,
                error: error.message,
                indicatorName,
                operation
            });
        }
    }
});
