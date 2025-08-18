// workerUtil.ts - Web Worker 유틸리티

// worker 인스턴스를 singleton으로 관리
let mergeWorker: Worker | null = null;

// worker가 처리 중인 작업과 콜백을 매핑
interface PendingTask {
    resolve: (data: any) => void;
    reject: (error: any) => void;
}

// 대기 중인 작업을 추적하는 맵
const pendingTasks: Map<string, PendingTask> = new Map();

// 병합 작업 임계값 - 이 크기 이상일 때 Worker 사용
const WORKER_THRESHOLD = 10000;

// Worker 코드를 문자열로 정의
const workerScript = `
// 데이터 병합 로직
function mergeIndicatorData(cache, newData) {
    // 신규 데이터가 없으면 cache 그대로 반환
    if (newData.length === 0) {
        return cache;
    }

    // 기존 데이터가 없으면 신규 데이터 그대로 반환
    if (cache.length === 0) {
        return newData;
    }
    
    // 성능 최적화: Array.concat 사용
    return cache.concat(newData); // 항상 최신 데이터 병합
}

// 메시지 이벤트 리스너 등록
self.addEventListener('message', function(e) {
    const { operation, cache, newData, indicatorName } = e.data;
    
    // 현재는 병합 작업만 지원
    if (operation === 'merge') {
        try {
            // 데이터 병합 처리
            const result = mergeIndicatorData(cache, newData);
            
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
`;

/**
 * Worker 인스턴스를 생성하거나 기존 인스턴스를 반환
 */
export function getMergeWorker(): Worker {
    if (!mergeWorker) {
        try {
            // Blob URL로 Worker 생성 (경로 문제 해결)
            const blob = new Blob([workerScript], { type: 'application/javascript' });
            const blobURL = URL.createObjectURL(blob);
            mergeWorker = new Worker(blobURL);
            
            // Worker로부터 메시지 수신 처리
            mergeWorker.onmessage = (e) => {
                const { success, result, error, indicatorName, operation } = e.data;
                
                // 작업 ID (indicatorName + operation)
                const taskId = `${indicatorName}-${operation}`;
                
                // 해당 작업의 콜백 조회
                const task = pendingTasks.get(taskId);
                if (task) {
                    if (success) {
                        task.resolve(result);
                    } else {
                        task.reject(new Error(error));
                    }
                    
                    // 처리 완료된 작업 제거
                    pendingTasks.delete(taskId);
                }
            };
            
            // 오류 처리
            mergeWorker.onerror = (error) => {
                console.error('Worker 오류:', error);
                
                // 모든 대기 중인 작업에 오류 전달
                for (const [taskId, task] of pendingTasks.entries()) {
                    task.reject(error);
                    pendingTasks.delete(taskId);
                }
            };
        } catch (err) {
            console.error('Worker 생성 실패:', err);
            throw err;
        }
    }
    
    return mergeWorker;
}

/**
 * Worker를 사용해 데이터를 병합
 * @param cache 기존 캐시 데이터
 * @param newData 새로운 데이터
 * @param indicatorName 지표 이름 (작업 식별용)
 * @returns Promise<merged data>
 */
export function mergeDataWithWorker<T>(
    cache: T[],
    newData: T[],
    indicatorName: string
): Promise<T[]> {
    // 빠른 경로: 데이터가 작은 경우 Worker 사용 안 함 (오버헤드 방지)
    if (cache.length === 0 || newData.length === 0 || cache.length + newData.length < WORKER_THRESHOLD) {
        // 원래 mergeIndicatorData 로직 인라인으로 구현
        if (newData.length === 0) return Promise.resolve(cache.slice());
        if (cache.length === 0) return Promise.resolve(newData.slice());
        
        // 성능 최적화: Array.concat 사용
        return Promise.resolve(cache.concat(newData));
    }
    
    return new Promise((resolve, reject) => {
        try {
            const worker = getMergeWorker();
            const taskId = `${indicatorName}-merge`;
            
            // 작업 등록
            pendingTasks.set(taskId, { resolve, reject });
            
            // Worker에 메시지 전송
            worker.postMessage({
                operation: 'merge',
                cache,
                newData,
                indicatorName
            });
        } catch (err) {
            reject(err);
        }
    });
}

// Worker 정리 함수
export function terminateWorker(): void {
    if (mergeWorker) {
        mergeWorker.terminate();
        mergeWorker = null;
    }
    
    pendingTasks.clear();
} 