// Trade Filter Manager - 다중 워커 관리 및 병렬 처리
interface TradeItem {
    [key: string]: any;
}

interface TradeFilter {
    recalculateBalance: boolean;
    tradeNumberMin?: number;
    tradeNumberMax?: number;
    strategies: string[];
    symbols: string[];
    entryNames: string[];
    exitNames: string[];
    entryDirections: string[];
    entryTimeMin?: string;
    entryTimeMax?: string;
    exitTimeMin?: string;
    exitTimeMax?: string;
    entryYears: number[];
    entryMonths: number[];
    entryDays: number[];
    entryDayOfWeeks: number[];
    entryHours: number[];
    entryMinutes: number[];
    entrySeconds: number[];
    exitYears: number[];
    exitMonths: number[];
    exitDays: number[];
    exitDayOfWeeks: number[];
    exitHours: number[];
    exitMinutes: number[];
    exitSeconds: number[];
    holdingTimeMin?: number;
    holdingTimeMax?: number;
    leverageMin?: number;
    leverageMax?: number;
    entryPriceMin?: number;
    entryPriceMax?: number;
    entryQuantityMin?: number;
    entryQuantityMax?: number;
    exitPriceMin?: number;
    exitPriceMax?: number;
    exitQuantityMin?: number;
    exitQuantityMax?: number;
    forcedLiquidationPriceMin?: number;
    forcedLiquidationPriceMax?: number;
    fundingCountMin?: number;
    fundingCountMax?: number;
    fundingFeeMin?: number;
    fundingFeeMax?: number;
    entryFeeMin?: number;
    entryFeeMax?: number;
    exitFeeMin?: number;
    exitFeeMax?: number;
    forcedLiquidationFeeMin?: number;
    forcedLiquidationFeeMax?: number;
    profitLossMin?: number;
    profitLossMax?: number;
    netProfitLossMin?: number;
    netProfitLossMax?: number;
    individualProfitRateMin?: number;
    individualProfitRateMax?: number;
    overallProfitRateMin?: number;
    overallProfitRateMax?: number;
    currentCapitalMin?: number;
    currentCapitalMax?: number;
    highestCapitalMin?: number;
    highestCapitalMax?: number;
    drawdownMin?: number;
    drawdownMax?: number;
    maxDrawdownMin?: number;
    maxDrawdownMax?: number;
    accumulatedProfitLossMin?: number;
    accumulatedProfitLossMax?: number;
    accumulatedProfitRateMin?: number;
    accumulatedProfitRateMax?: number;
    heldSymbolsCountMin?: number;
    heldSymbolsCountMax?: number;
}

interface WorkerTask {
    id: string;
    chunk: TradeItem[];
    filter: TradeFilter;
    allTrades: TradeItem[];
    chunkIndex: number;
    totalChunks: number;
}

interface WorkerResult {
    id: string;
    trades: TradeItem[];
    hasBankruptcy?: boolean; // 파산 여부 추가
    chunkIndex: number;
    totalChunks: number;
    processingTime: number;
    error?: string;
}

export class TradeFilterManager {
    private workers: Worker[] = [];
    private busyWorkers: Set<number> = new Set();
    private pendingTasks: WorkerTask[] = [];
    private results: Map<string, WorkerResult[]> = new Map();
    private callbacks: Map<string, (result: { trades: TradeItem[], hasBankruptcy: boolean }) => void> = new Map();
    private progressCallbacks: Map<string, (progress: number) => void> = new Map();

    // 자금 재계산 관련 추가 속성들
    private needsBalanceRecalculation: Map<string, boolean> = new Map();
    private originalTradesMap: Map<string, TradeItem[]> = new Map();

    // 최신 작업 관리를 위한 속성들
    private latestTaskId: string | null = null;
    private activeTasks: Set<string> = new Set();

    // 워커 수 동적 결정 (CPU 코어 수 기반)
    private readonly maxWorkers: number;
    private readonly chunkSize: number;

    constructor() {
        // CPU 코어 수 기반으로 워커 수 결정
        this.maxWorkers = Math.max(navigator.hardwareConcurrency || 4, 2);
        this.chunkSize = Math.max(500, Math.min(2000, Math.floor(20000 / this.maxWorkers)));

        this.initializeWorkers();
    }

    public filterTrades(
        trades: TradeItem[],
        filter: TradeFilter,
        onProgress?: (progress: number) => void
    ): Promise<{ trades: TradeItem[], hasBankruptcy: boolean }> {
        return new Promise((resolve, reject) => {
            try {
                // 입력 검증
                if (!trades || !Array.isArray(trades) || trades.length === 0) {
                    resolve({trades: [], hasBankruptcy: false});
                    return;
                }

                // 아무 필터도 적용되어 있지 않으면 원본 데이터 그대로 반환
                if (!this.hasActiveFilters(filter)) {
                    resolve({trades, hasBankruptcy: false});
                    return;
                }

                // 이전 작업들 모두 취소
                this.cancelAllActiveTasks();

                // 작업 ID 생성
                const taskId = `task_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;

                // 최신 작업으로 설정
                this.latestTaskId = taskId;
                this.activeTasks.add(taskId);

                // 콜백 등록
                this.callbacks.set(taskId, resolve);
                if (onProgress) {
                    this.progressCallbacks.set(taskId, onProgress);
                }

                // 자금 재계산 정보 저장
                this.needsBalanceRecalculation.set(taskId, filter.recalculateBalance);
                this.originalTradesMap.set(taskId, trades);

                // 데이터가 작은 경우 단일 워커로 처리
                if (trades.length < this.chunkSize * 2) {
                    // 단일 워커도 청크 모드와 동일하게 필터링만 수행하고 자금 재계산은 메인 스레드에서
                    const singleWorkerFilter = {...filter, recalculateBalance: false};
                    const workerIndex = this.getAvailableWorker();
                    if (workerIndex !== -1) {
                        this.busyWorkers.add(workerIndex);
                        this.workers[workerIndex].postMessage({
                            type: 'filter',
                            trades,
                            filter: singleWorkerFilter,
                            allTrades: trades,
                            taskId,
                            chunkIndex: 0,
                            totalChunks: 1
                        });
                    } else {
                        // 사용 가능한 워커가 없으면 대기열에 추가
                        this.pendingTasks.push({
                            id: taskId,
                            chunk: trades,
                            filter: singleWorkerFilter,
                            allTrades: trades,
                            chunkIndex: 0,
                            totalChunks: 1
                        });
                    }
                    return;
                }

                // 대량 데이터의 경우 청크로 분할
                const chunks = this.createChunks(trades);

                // 각 청크를 작업으로 생성
                for (let i = 0; i < chunks.length; i++) {
                    // 청크 처리 시에는 자금 재계산을 끄고 필터링만 수행
                    const chunkFilter = {...filter, recalculateBalance: false};

                    const task: WorkerTask = {
                        id: taskId,
                        chunk: chunks[i],
                        filter: chunkFilter, // 자금 재계산 끈 필터 사용
                        allTrades: trades,
                        chunkIndex: i,
                        totalChunks: chunks.length
                    };

                    const workerIndex = this.getAvailableWorker();
                    if (workerIndex !== -1) {
                        this.busyWorkers.add(workerIndex);
                        this.workers[workerIndex].postMessage({
                            type: 'filter',
                            trades: task.chunk,
                            filter: task.filter,
                            allTrades: task.allTrades,
                            taskId: task.id,
                            chunkIndex: task.chunkIndex,
                            totalChunks: task.totalChunks
                        });
                    } else {
                        // 사용 가능한 워커가 없으면 대기열에 추가
                        this.pendingTasks.push(task);
                    }
                }

                // 진행률 초기화
                if (onProgress) {
                    onProgress(0);
                }

            } catch (error) {
                console.error('필터링 작업 시작 실패:', error);
                reject(error);
            }
        });
    }

    public destroy(): void {
        // 모든 워커 종료
        this.workers.forEach((worker, index) => {
            try {
                worker.terminate();
            } catch (error) {
                console.error(`워커 ${index} 종료 실패:`, error);
            }
        });

        // 상태 초기화
        this.workers = [];
        this.busyWorkers.clear();
        this.pendingTasks = [];
        this.results.clear();
        this.callbacks.clear();
        this.progressCallbacks.clear();
        this.needsBalanceRecalculation.clear();
        this.originalTradesMap.clear();
        this.latestTaskId = null;
        this.activeTasks.clear();
    }

    private initializeWorkers(): void {
        for (let i = 0; i < this.maxWorkers; i++) {
            try {
                const worker = new Worker(new URL('./TradeFilterWorker.ts', import.meta.url), {
                    type: 'module'
                });

                worker.onmessage = (event) => this.handleWorkerMessage(i, event);
                worker.onerror = (error) => this.handleWorkerError(i, error);

                this.workers.push(worker);
            } catch (error) {
                console.error(`워커 ${i} 초기화 실패:`, error);
            }
        }
    }

    private handleWorkerMessage(workerIndex: number, event: MessageEvent): void {
        const {type, trades, id, chunkIndex, totalChunks, processingTime, error} = event.data;

        if (type === 'filterResult') {
            this.busyWorkers.delete(workerIndex);

            if (error) {
                console.error(`워커 ${workerIndex} 에러:`, error);
                this.handleTaskError(id, error);
            } else {
                this.handleTaskResult(id, {
                    id,
                    trades,
                    chunkIndex,
                    totalChunks,
                    processingTime: processingTime || 0,
                });
            }

            // 대기 중인 작업이 있으면 실행
            this.processNextTask();
        }
    }

    private handleWorkerError(workerIndex: number, error: ErrorEvent): void {
        console.error(`워커 ${workerIndex} 오류:`, error);
        this.busyWorkers.delete(workerIndex);

        // 워커 재시작 시도
        try {
            this.workers[workerIndex].terminate();
            const newWorker = new Worker(new URL('./TradeFilterWorker.ts', import.meta.url), {
                type: 'module'
            });

            newWorker.onmessage = (event) => this.handleWorkerMessage(workerIndex, event);
            newWorker.onerror = (error) => this.handleWorkerError(workerIndex, error);

            this.workers[workerIndex] = newWorker;
        } catch (restartError) {
            console.error(`워커 ${workerIndex} 재시작 실패:`, restartError);
        }
    }

    private handleTaskResult(taskId: string, result: WorkerResult): void {
        // 최신 작업이 아니면 무시
        if (taskId !== this.latestTaskId || !this.activeTasks.has(taskId)) {
            return;
        }

        if (!this.results.has(taskId)) {
            this.results.set(taskId, []);
        }

        const taskResults = this.results.get(taskId)!;
        taskResults.push(result);

        // 진행률 업데이트
        const progressCallback = this.progressCallbacks.get(taskId);
        if (progressCallback) {
            const progress = (result.chunkIndex + 1) / result.totalChunks * 100;
            progressCallback(Math.min(progress, 100));
        }

        // 모든 청크가 완료되었는지 확인
        const expectedChunks = this.getExpectedChunks(taskId);
        if (taskResults.length === expectedChunks) {
            this.combineResults(taskId).then();
        }
    }

    private handleTaskError(taskId: string, error: string): void {
        console.error(`작업 ${taskId} 실패:`, error);
        const callback = this.callbacks.get(taskId);
        if (callback) {
            // 에러 발생 시 빈 배열 반환
            callback({trades: [], hasBankruptcy: false});
            this.cleanupTask(taskId);
        }
    }

    private getExpectedChunks(taskId: string): number {
        // 첫 번째 결과에서 총 청크 수 확인
        const taskResults = this.results.get(taskId);
        return taskResults && taskResults.length > 0 ? taskResults[0].totalChunks : 1;
    }

    private async combineResults(taskId: string): Promise<void> {
        // 최신 작업이 아니면 무시
        if (taskId !== this.latestTaskId || !this.activeTasks.has(taskId)) {
            this.cleanupTask(taskId);
            return;
        }

        const taskResults = this.results.get(taskId);
        const callback = this.callbacks.get(taskId);

        if (!taskResults || !callback) {
            console.error(`작업 ${taskId}의 결과 또는 콜백을 찾을 수 없음`);
            return;
        }

        try {
            // 청크 인덱스 순으로 정렬
            taskResults.sort((a, b) => a.chunkIndex - b.chunkIndex);

            // 결과 합치기
            const combinedTrades: TradeItem[] = [];
            let totalProcessingTime = 0;

            for (const result of taskResults) {
                if (result.chunkIndex === 0) {
                    // 첫 번째 청크는 헤더 포함
                    combinedTrades.push(...result.trades);
                } else {
                    // 나머지 청크는 헤더 제외
                    combinedTrades.push(...result.trades.slice(1));
                }
                totalProcessingTime += result.processingTime;
            }

            // 자금 재계산이 필요한 경우 메인 스레드에서 수행 (더 빠름)
            const needsRecalculation = this.needsBalanceRecalculation.get(taskId);
            let hasBankruptcy = false;

            // 워커에서 hasBankruptcy 정보 수집
            for (const result of taskResults) {
                if (result.hasBankruptcy) {
                    hasBankruptcy = true;
                    break;
                }
            }

            if (needsRecalculation && combinedTrades.length > 1) {
                const recalculationResult = this.recalculateBalanceInMainThread(
                    combinedTrades,
                    this.originalTradesMap.get(taskId) || combinedTrades
                );

                callback({trades: recalculationResult.result, hasBankruptcy: recalculationResult.hasBankruptcy});
            } else {
                callback({trades: combinedTrades, hasBankruptcy});
            }

            this.cleanupTask(taskId);
        } catch (error) {
            console.error(`작업 ${taskId} 결과 합치기 실패:`, error);
            callback({trades: [], hasBankruptcy: false});
            this.cleanupTask(taskId);
        }
    }

    private cleanupTask(taskId: string): void {
        this.results.delete(taskId);
        this.callbacks.delete(taskId);
        this.progressCallbacks.delete(taskId);
        this.needsBalanceRecalculation.delete(taskId);
        this.originalTradesMap.delete(taskId);
        this.activeTasks.delete(taskId);
    }

    private cancelAllActiveTasks(): void {
        // 진행 중인 모든 작업 취소
        for (const taskId of this.activeTasks) {
            const callback = this.callbacks.get(taskId);
            if (callback) {
                // 취소된 작업은 빈 배열로 처리하지 않고 아예 무시
                this.cleanupTask(taskId);
            }
        }

        // 대기 중인 작업들도 제거
        this.pendingTasks = [];
    }

    private createChunks(trades: TradeItem[]): TradeItem[][] {
        if (trades.length <= 1) {
            return [trades]; // 헤더만 있는 경우
        }

        const header = trades[0];
        const dataRows = trades.slice(1);
        const chunks: TradeItem[][] = [];

        for (let i = 0; i < dataRows.length; i += this.chunkSize) {
            const chunkData = dataRows.slice(i, i + this.chunkSize);
            // 각 청크에 헤더 추가
            chunks.push([header, ...chunkData]);
        }

        return chunks.length > 0 ? chunks : [trades];
    }

    private getAvailableWorker(): number {
        for (let i = 0; i < this.workers.length; i++) {
            if (!this.busyWorkers.has(i)) {
                return i;
            }
        }
        return -1;
    }

    private processNextTask(): void {
        if (this.pendingTasks.length === 0) {
            return;
        }

        const workerIndex = this.getAvailableWorker();
        if (workerIndex === -1) {
            return; // 사용 가능한 워커가 없음
        }

        const task = this.pendingTasks.shift()!;
        this.busyWorkers.add(workerIndex);

        try {
            this.workers[workerIndex].postMessage({
                type: 'filter',
                trades: task.chunk,
                filter: task.filter,
                allTrades: task.allTrades,
                taskId: task.id,
                chunkIndex: task.chunkIndex,
                totalChunks: task.totalChunks
            });
        } catch (error) {
            console.error(`워커 ${workerIndex}에 작업 전송 실패:`, error);
            this.busyWorkers.delete(workerIndex);
            this.handleTaskError(task.id, `워커 통신 실패: ${error}`);
        }
    }

    // 메인 스레드에서 자금 재계산 수행 (워커보다 빠름)
    private recalculateBalanceInMainThread(filtered: TradeItem[], allTrades: TradeItem[]): {
        result: TradeItem[],
        hasBankruptcy: boolean
    } {
        if (filtered.length <= 1) return {result: filtered, hasBankruptcy: false}; // 헤더만 있는 경우

        // 필터된 거래가 원본 거래와 동일한지 확인
        if (filtered.length === allTrades.length) {
            let isIdentical = true;
            for (let i = 0; i < filtered.length; i++) {
                if (filtered[i]["거래 번호"] !== allTrades[i]["거래 번호"]) {
                    isIdentical = false;
                    break;
                }
            }
            if (isIdentical) {
                return {result: allTrades, hasBankruptcy: false}; // 원본 거래 반환
            }
        }

        const initialBalance = allTrades.length > 0 ? Number(allTrades[0]["현재 자금"]) : 0;
        let currentBalance = initialBalance;
        let maxBalance = initialBalance;
        let maxDrawdown = 0;

        // 거래 번호 재매기기 - 최적화된 버전
        const tradeNumberMap = new Map<number, number>();
        let newTradeNumber = 1;
        let lastOriginalTradeNumber: number | null = null;

        // 첫 번째 패스: 거래 번호 재매핑만 수행
        for (let i = 1; i < filtered.length; i++) {
            const originalTradeNumber = Number(filtered[i]["거래 번호"]);

            if (originalTradeNumber !== lastOriginalTradeNumber) {
                if (!tradeNumberMap.has(originalTradeNumber)) {
                    tradeNumberMap.set(originalTradeNumber, newTradeNumber);
                    newTradeNumber++;
                }
                lastOriginalTradeNumber = originalTradeNumber;
            }
        }

        // 파산 시점 찾기
        let bankruptIndex = -1;
        let tempBalance = initialBalance;

        for (let i = 1; i < filtered.length; i++) {
            const profit = Number(filtered[i]["순손익"]);
            tempBalance += profit;

            if (tempBalance < 0) {
                bankruptIndex = i;
                break;
            }
        }

        const endIndex = bankruptIndex > 0 ? bankruptIndex : filtered.length;
        const hasBankruptcy = bankruptIndex > 0; // 파산 발생 여부

        // 결과 배열 미리 할당
        const result = new Array(endIndex);
        result[0] = filtered[0]; // 헤더 복사

        // 빠른 자금 재계산
        for (let i = 1; i < endIndex; i++) {
            const trade = {...filtered[i]}; // 얕은 복사
            const originalTradeNumber = Number(trade["거래 번호"]);
            const mappedNumber = tradeNumberMap.get(originalTradeNumber) || originalTradeNumber;

            // 현재 거래의 손익 처리
            const profit = Number(trade["순손익"]);
            currentBalance += profit;
            maxBalance = Math.max(maxBalance, currentBalance);
            const drawdown = maxBalance > 0 ? (1 - currentBalance / maxBalance) * 100 : 0;
            maxDrawdown = Math.max(maxDrawdown, drawdown);

            // 자금 관련 필드 업데이트
            trade["거래 번호"] = mappedNumber;
            trade["현재 자금"] = currentBalance;
            trade["최고 자금"] = maxBalance;
            trade["드로우다운"] = drawdown;
            trade["최고 드로우다운"] = maxDrawdown;
            trade["누적 손익"] = currentBalance - initialBalance;
            trade["누적 손익률"] = (currentBalance - initialBalance) / initialBalance * 100;
            trade["보유 심볼 수"] = "-";

            result[i] = trade;
        }

        return {result, hasBankruptcy};
    }

    // 필터가 실제로 적용되어 있는지 확인하는 유틸리티
    private hasActiveFilters(filter: TradeFilter): boolean {
        return (
            // 거래 번호 필터
            filter.tradeNumberMin !== undefined || filter.tradeNumberMax !== undefined ||

            // 문자열 필터들
            (filter.strategies && filter.strategies.length > 0) ||
            (filter.symbols && filter.symbols.length > 0) ||
            (filter.entryNames && filter.entryNames.length > 0) ||
            (filter.exitNames && filter.exitNames.length > 0) ||
            (filter.entryDirections && filter.entryDirections.length > 0 &&
                !(filter.entryDirections.length === 2 && filter.entryDirections.includes("매수") && filter.entryDirections.includes("매도"))) ||

            // 시간 필터들
            filter.entryTimeMin !== undefined || filter.entryTimeMax !== undefined ||
            filter.exitTimeMin !== undefined || filter.exitTimeMax !== undefined ||
            (filter.entryYears && filter.entryYears.length > 0) ||
            (filter.entryMonths && filter.entryMonths.length > 0) ||
            (filter.entryDays && filter.entryDays.length > 0) ||
            (filter.entryDayOfWeeks && filter.entryDayOfWeeks.length > 0) ||
            (filter.entryHours && filter.entryHours.length > 0) ||
            (filter.entryMinutes && filter.entryMinutes.length > 0) ||
            (filter.entrySeconds && filter.entrySeconds.length > 0) ||
            (filter.exitYears && filter.exitYears.length > 0) ||
            (filter.exitMonths && filter.exitMonths.length > 0) ||
            (filter.exitDays && filter.exitDays.length > 0) ||
            (filter.exitDayOfWeeks && filter.exitDayOfWeeks.length > 0) ||
            (filter.exitHours && filter.exitHours.length > 0) ||
            (filter.exitMinutes && filter.exitMinutes.length > 0) ||
            (filter.exitSeconds && filter.exitSeconds.length > 0) ||

            // 보유 시간 필터
            filter.holdingTimeMin !== undefined || filter.holdingTimeMax !== undefined ||

            // 숫자 범위 필터들
            filter.leverageMin !== undefined || filter.leverageMax !== undefined ||
            filter.entryPriceMin !== undefined || filter.entryPriceMax !== undefined ||
            filter.entryQuantityMin !== undefined || filter.entryQuantityMax !== undefined ||
            filter.exitPriceMin !== undefined || filter.exitPriceMax !== undefined ||
            filter.exitQuantityMin !== undefined || filter.exitQuantityMax !== undefined ||
            filter.forcedLiquidationPriceMin !== undefined || filter.forcedLiquidationPriceMax !== undefined ||
            filter.entryFeeMin !== undefined || filter.entryFeeMax !== undefined ||
            filter.exitFeeMin !== undefined || filter.exitFeeMax !== undefined ||
            filter.forcedLiquidationFeeMin !== undefined || filter.forcedLiquidationFeeMax !== undefined ||
            filter.fundingCountMin !== undefined || filter.fundingCountMax !== undefined ||
            filter.fundingFeeMin !== undefined || filter.fundingFeeMax !== undefined ||
            filter.profitLossMin !== undefined || filter.profitLossMax !== undefined ||
            filter.netProfitLossMin !== undefined || filter.netProfitLossMax !== undefined ||
            filter.individualProfitRateMin !== undefined || filter.individualProfitRateMax !== undefined ||
            filter.overallProfitRateMin !== undefined || filter.overallProfitRateMax !== undefined ||
            filter.currentCapitalMin !== undefined || filter.currentCapitalMax !== undefined ||
            filter.highestCapitalMin !== undefined || filter.highestCapitalMax !== undefined ||
            filter.drawdownMin !== undefined || filter.drawdownMax !== undefined ||
            filter.maxDrawdownMin !== undefined || filter.maxDrawdownMax !== undefined ||
            filter.accumulatedProfitLossMin !== undefined || filter.accumulatedProfitLossMax !== undefined ||
            filter.accumulatedProfitRateMin !== undefined || filter.accumulatedProfitRateMax !== undefined ||
            filter.heldSymbolsCountMin !== undefined || filter.heldSymbolsCountMax !== undefined
        );
    }
}

// 싱글톤 인스턴스
export const tradeFilterManager = new TradeFilterManager();
