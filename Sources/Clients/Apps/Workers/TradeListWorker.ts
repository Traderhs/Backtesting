// TradeList 워커 - 텍스트 측정과 포맷팅 작업 처리

interface WorkerMessage {
    type: 'INIT' | 'MEASURE_TEXT' | 'FORMAT_CELLS' | 'CALCULATE_VISIBLE_RANGE';
    payload?: any;
    requestId?: number;
}

interface TextMeasureRequest {
    texts: Array<{
        text: string;
        font: string;
        key: string;
    }>;
}

interface FormatCellsRequest {
    trades: any[];
    headers: string[];
    config: any;
    startIndex: number;
    endIndex: number;
}

interface VisibleRangeRequest {
    scrollTop: number;
    containerHeight: number;
    rowHeight: number;
    totalRows: number;
    buffer: number;
}

// 캔버스 컨텍스트를 재사용하여 성능 최적화
let canvasContext: OffscreenCanvasRenderingContext2D | null = null;

function getCanvasContext(): OffscreenCanvasRenderingContext2D {
    if (!canvasContext) {
        const canvas = new OffscreenCanvas(1, 1);
        canvasContext = canvas.getContext('2d')!;
    }
    return canvasContext!;
}

// 텍스트 측정 캐시
const textMeasureCache = new Map<string, number>();

function measureText(text: string, font: string): number {
    const cacheKey = `${text}|${font}`;

    if (textMeasureCache.has(cacheKey)) {
        return textMeasureCache.get(cacheKey)!;
    }

    const context = getCanvasContext();
    context.font = font;
    const width = Math.ceil(context.measureText(text).width);

    textMeasureCache.set(cacheKey, width);
    return width;
}

// 포맷팅 함수들
const DOLLAR_FIELDS = new Set([
    "진입 수수료", "청산 수수료", "강제 청산 수수료", "펀딩비 수령", "펀딩비 지불",
    "펀딩비", "손익", "순손익", "현재 자금", "최고 자금", "누적 손익"
]);

const PERCENT_FIELDS = new Set([
    "개별 순손익률", "전체 순손익률", "드로우다운", "최고 드로우다운", "누적 손익률"
]);

const PRICE_FIELDS = new Set(["진입 가격", "청산 가격", "강제 청산 가격"]);
const QUANTITY_FIELDS = new Set(["진입 수량", "청산 수량"]);

const precisionCache = new Map<string, { pricePrecision: number, qtyPrecision: number }>();

function getSymbolPrecision(config: any, symbol: string): { pricePrecision: number, qtyPrecision: number } {
    if (precisionCache.has(symbol)) {
        return precisionCache.get(symbol)!;
    }

    const defaultPrecision = {pricePrecision: 2, qtyPrecision: 3};

    if (!config?.["심볼"]) {
        precisionCache.set(symbol, defaultPrecision);
        return defaultPrecision;
    }

    const symbolInfo = config["심볼"].find((s: any) => s["심볼 이름"] === symbol);

    if (!symbolInfo?.["거래소 정보"]) {
        precisionCache.set(symbol, defaultPrecision);
        return defaultPrecision;
    }

    const exchangeInfo = symbolInfo["거래소 정보"];
    const pricePrecision = exchangeInfo["소수점 정밀도"] || 2;
    const qtyStep = exchangeInfo["수량 최소 단위"] || 0.001;
    const qtyPrecision = qtyStep.toString().includes('.') ?
        qtyStep.toString().split('.')[1].length : 0;

    const result = {pricePrecision, qtyPrecision};
    precisionCache.set(symbol, result);
    return result;
}

function formatWithCommas(value: string | number, precision: number = 0): string {
    if (value === undefined || value === null || String(value) === "-") return "-";

    const num = typeof value === "number" ? value : parseFloat(String(value));
    if (isNaN(num)) return String(value);

    const fixedValue = precision > 0 ? num.toFixed(precision) : Math.round(num).toString();
    const parts = fixedValue.split('.');
    const integerPart = parts[0];
    const decimalPart = parts[1];
    const formattedInteger = integerPart.replace(/\B(?=(\d{3})+(?!\d))/g, ',');

    return decimalPart ? `${formattedInteger}.${decimalPart}` : formattedInteger;
}

function formatCellValue(value: string | number | undefined, key: string, config: any, symbol: string): string {
    if (value === undefined || value === null) return "";
    if (key === 'originalIdxForSort') return "";

    if (key === "거래 번호") {
        return `#${value}`;
    }

    if (key === "보유 심볼 수") {
        return String(value) === "-" ? "-" : `${value}개`;
    }

    if (key === "펀딩 수령 횟수" || key === "펀딩 지불 횟수" || key === "펀딩 횟수") {
        return String(value) === "-" ? "-" : `${value}회`;
    }

    if (PRICE_FIELDS.has(key)) {
        if (!value || String(value) === "-") return "-";
        const num = typeof value === "number" ? value : parseFloat(String(value));
        if (isNaN(num)) return String(value);
        const precision = getSymbolPrecision(config, symbol);
        return formatWithCommas(num, precision.pricePrecision);
    }

    if (QUANTITY_FIELDS.has(key)) {
        if (!value || String(value) === "-") return "-";
        const num = typeof value === "number" ? value : parseFloat(String(value));
        if (isNaN(num)) return String(value);
        const precision = getSymbolPrecision(config, symbol);
        return formatWithCommas(num, precision.qtyPrecision);
    }

    const num = typeof value === "number" ? value : parseFloat(String(value));
    if (isNaN(num)) return String(value);

    if (DOLLAR_FIELDS.has(key)) {
        return `$${formatWithCommas(num, 2)}`;
    }

    if (PERCENT_FIELDS.has(key)) {
        return `${formatWithCommas(num, 2)}%`;
    }

    return formatWithCommas(num, 2);
}

// 가시 범위 계산
function calculateVisibleRange(request: VisibleRangeRequest) {
    const {scrollTop, containerHeight, rowHeight, totalRows, buffer} = request;

    const startIndex = Math.max(0, Math.floor(scrollTop / rowHeight) - buffer);
    const endIndex = Math.min(totalRows - 1,
        Math.ceil((scrollTop + containerHeight) / rowHeight) + buffer);

    return {start: startIndex, end: endIndex};
}

// 메시지 핸들러
self.onmessage = function (e: MessageEvent<WorkerMessage>) {
    const {type, payload, requestId} = e.data;

    try {
        switch (type) {
            case 'INIT': {
                self.postMessage({
                    type: 'WORKER_READY',
                    requestId
                });
                break;
            }

            case 'MEASURE_TEXT': {
                const {texts} = payload as TextMeasureRequest;
                const results = texts.map(({text, font, key}) => ({
                    key,
                    width: measureText(text, font)
                }));

                self.postMessage({
                    type: 'MEASURE_TEXT_RESULT',
                    payload: results,
                    requestId
                });
                break;
            }

            case 'FORMAT_CELLS': {
                const {trades, headers, config, startIndex, endIndex} = payload as FormatCellsRequest;
                const formattedCells = [];

                for (let i = startIndex; i <= endIndex && i < trades.length; i++) {
                    const trade = trades[i];
                    const symbol = String(trade["심볼 이름"] || "");
                    const rowData: any = {index: i};

                    for (const header of headers) {
                        rowData[header] = formatCellValue(trade[header], header, config, symbol);
                    }

                    formattedCells.push(rowData);
                }

                self.postMessage({
                    type: 'FORMAT_CELLS_RESULT',
                    payload: formattedCells,
                    requestId
                });
                break;
            }

            case 'CALCULATE_VISIBLE_RANGE': {
                const result = calculateVisibleRange(payload as VisibleRangeRequest);

                self.postMessage({
                    type: 'CALCULATE_VISIBLE_RANGE_RESULT',
                    payload: result,
                    requestId
                });
                break;
            }
        }
    } catch (error) {
        self.postMessage({
            type: 'ERROR',
            payload: {error: error instanceof Error ? error.message : 'Unknown error'},
            requestId
        });
    }
};
