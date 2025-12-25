import React, {useCallback, useEffect, useRef, useState} from 'react';
import {IChartApi, ISeriesApi, SeriesType, Time} from 'lightweight-charts';
import {CandleData} from './CandleStickRenderer';
import {useTradeFilter} from '@/components/TradeFilter';

// TradeList 에서 가져온 포맷팅 정보
const dollarFields = [
    "진입 수수료", "청산 수수료", "강제 청산 수수료", "손익",
    "순손익", "현재 자금", "최고 자금", "누적 손익",
    "펀딩비 수령", "펀딩비 지불", "펀딩비",
];
const percentFields = [
    "개별 순손익률", "전체 순손익률", "드로우다운", "최고 드로우다운", "누적 손익률",
];
const countFields = [
    "펀딩 수령 횟수", "펀딩 지불 횟수", "펀딩 횟수",
];

// 숫자 포맷팅 헬퍼 함수 수정
function formatValue(value: string | number, key: string, pricePrecision: number = 2, config?: any, symbol?: string): string {
    const num = typeof value === 'number' ? value : parseFloat(String(value).replace(/,/g, ''));

    // NaN인 경우 원본 문자열 반환
    if (isNaN(num)) return String(value);

    // 이름이나 날짜 필드는 원본 값 반환 (천 단위 쉼표 제외)
    const excludeFields = ["전략 이름", "진입 이름", "청산 이름", "진입 시간", "청산 시간", "보유 시간"];
    if (excludeFields.includes(key)) {
        return String(value);
    }

    // 가격 필드들에 precision 적용
    const priceFields = ["진입 가격", "청산 가격", "강제 청산 가격"];
    if (priceFields.includes(key)) {
        return num.toLocaleString('en-US', {
            minimumFractionDigits: pricePrecision,
            maximumFractionDigits: pricePrecision
        });
    }

    // 수량 필드들에 심볼별 precision 적용
    const quantityFields = ["진입 수량", "청산 수량"];
    if (quantityFields.includes(key)) {
        const qtyPrecision = getSymbolQtyPrecision(config, symbol || "");
        return num.toLocaleString('en-US', {
            minimumFractionDigits: qtyPrecision,
            maximumFractionDigits: qtyPrecision
        });
    }

    // 특정 필드에 대한 포맷팅
    if (dollarFields.includes(key)) {
        return num < 0 ? `-$${Math.abs(num).toLocaleString('en-US', {
            minimumFractionDigits: 2,
            maximumFractionDigits: 2
        })}` : `$${num.toLocaleString('en-US', {minimumFractionDigits: 2, maximumFractionDigits: 2})}`;
    } else if (percentFields.includes(key)) {
        return `${num.toLocaleString('en-US', {minimumFractionDigits: 2, maximumFractionDigits: 2})}%`;
    } else if (countFields.includes(key)) {
        return `${Math.round(num).toLocaleString('en-US')}회`;
    } else if (key === "레버리지") {
        return `${Math.round(num).toLocaleString('en-US')}x`;
    } else if (key === "보유 심볼 수") {
        // '-'가 아닐 때만 '개'를 붙임
        return String(value) === '-' ? String(value) : `${Math.round(num).toLocaleString('en-US')}개`;
    }

    // 나머지 숫자 필드들에는 천 단위 쉼표 추가
    // 소수점이 있는 경우와 없는 경우 구분
    if (num % 1 === 0) {
        // 정수인 경우
        return Math.round(num).toLocaleString('en-US');
    } else {
        // 소수점이 있는 경우
        return num.toLocaleString('en-US', {minimumFractionDigits: 2, maximumFractionDigits: 2});
    }
}

// 마우스 포인트와 선분 사이의 최단 거리 계산 함수
function distToSegmentSquared(p: { x: number; y: number }, v: { x: number; y: number }, w: {
    x: number;
    y: number
}): number {
    const l2 = (v.x - w.x) ** 2 + (v.y - w.y) ** 2;
    if (l2 === 0) return (p.x - v.x) ** 2 + (p.y - v.y) ** 2;
    let t = ((p.x - v.x) * (w.x - v.x) + (p.y - v.y) * (w.y - v.y)) / l2;
    t = Math.max(0, Math.min(1, t));
    const projection = {x: v.x + t * (w.x - v.x), y: v.y + t * (w.y - v.y)};
    return (p.x - projection.x) ** 2 + (p.y - projection.y) ** 2;
}

interface TradeMarkersProps {
    symbol: string; // 현재 보고 있는 심볼 추가
    chart: IChartApi | null;
    mainSeries: ISeriesApi<SeriesType> | undefined;
    candleStickData: CandleData[]; // 현재 로드된 전체 캔들 데이터
    containerRef: React.RefObject<HTMLDivElement | null>; // 차트 컨테이너 ref
    pricePrecision: number;
    config?: any;
    candleInterval: number;
}

// 심볼별 수량 precision 정보를 가져오는 함수
function getSymbolQtyPrecision(config: any, symbol: string): number {
    if (!config || !config["심볼"]) {
        return 3; // 기본값
    }

    const symbolInfo = config["심볼"].find((s: any) => s["심볼 이름"] === symbol);
    if (!symbolInfo || !symbolInfo["거래소 정보"]) {
        return 0; // 기본값
    }

    return symbolInfo["거래소 정보"]["수량 소수점 정밀도"] || 0;
}

// 마커 아이템 인터페이스 추가
interface MarkerItem {
    trade: any;
    candle: CandleData;
}

// 시간 파싱 함수
function parseTimeStringToUTCSec(timeString: string): number | null {
    if (timeString === '-') return null;
    try {
        // 한국 시간(KST) 형식인 경우 UTC로 변환 (KST는 UTC+9)
        // "YYYY-MM-DD HH:MM:SS" 형식 가정
        const parts = timeString.split(/[\s-:]/);
        if (parts.length >= 6) {
            const year = parseInt(parts[0]);
            const month = parseInt(parts[1]) - 1; // 월은 0-11
            const day = parseInt(parts[2]);
            const hour = parseInt(parts[3]);
            const minute = parseInt(parts[4]);
            const second = parseInt(parts[5]);

            // Date 객체를 UTC 기준으로 생성
            const utcDate = Date.UTC(year, month, day, hour, minute, second);
            return utcDate / 1000; // 밀리초를 초로 변환
        }

        // 일반적인 파싱 (fallback)
        const date = new Date(timeString);
        return date.getTime() / 1000;
    } catch (e) {
        console.error("시간 문자열 파싱 오류:", timeString, e);
        return null;
    }
}

// 캔들 데이터 내에서 가장 가까운 시간 찾기 함수 개선
function findNearestCandleTime(candleData: CandleData[], targetTime: number): CandleData | null {
    if (!candleData || candleData.length === 0) return null;

    // 이진 탐색으로 타겟 시간보다 작거나 같은 가장 가까운 캔들 찾기
    let left = 0;
    let right = candleData.length - 1;
    let candidateIdx = -1; // 타겟 시간보다 큰 첫 번째 캔들 인덱스

    while (left <= right) {
        const mid = Math.floor((left + right) / 2);
        const candleTime = Number(candleData[mid].time);

        if (candleTime === targetTime) {
            // 정확히 일치하는 시간을 찾으면 바로 반환
            return candleData[mid];
        } else if (candleTime > targetTime) {
            // 타겟 시간보다 큰 캔들 찾음
            candidateIdx = mid;
            right = mid - 1;
        } else {
            left = mid + 1;
        }
    }

    // 타겟 시간보다 작거나 같은 캔들 중 가장 큰 시간을 가진 캔들 찾기
    if (candidateIdx > 0) {
        // 찾은 캔들의 바로 이전 캔들 반환 (타겟 시간보다 작거나 같은 캔들 중 가장 근접한 것)
        return candleData[candidateIdx - 1];
    } else if (candidateIdx === 0) {
        // 모든 캔들이 타겟 시간보다 크다면, 첫 번째 캔들 반환
        return candleData[0];
    } else {
        // 모든 캔들이 타겟 시간보다 작다면, 마지막 캔들 반환
        return candleData[candleData.length - 1];
    }
}

// 렌더링 최적화를 위한 스로틀링 유틸리티 함수 추가
function throttle<T extends (...args: any[]) => any>(
    func: T,
    limit: number
): (...args: Parameters<T>) => void {
    let inThrottle = false;
    let lastFunc: ReturnType<typeof setTimeout> | null = null;
    let lastRan = 0;

    return function (this: any, ...args: Parameters<T>) {
        const context = this;

        if (!inThrottle) {
            func.apply(context, args);
            lastRan = Date.now();
            inThrottle = true;

            setTimeout(() => {
                inThrottle = false;
            }, limit);
        } else {
            if (lastFunc) clearTimeout(lastFunc);
            lastFunc = setTimeout(() => {
                if ((Date.now() - lastRan) >= limit) {
                    func.apply(context, args);
                    lastRan = Date.now();
                }
            }, limit - (Date.now() - lastRan));
        }
    };
}

const TradeMarkers: React.FC<TradeMarkersProps> = ({
                                                       symbol,
                                                       chart,
                                                       mainSeries,
                                                       candleStickData,
                                                       containerRef,
                                                       pricePrecision,
                                                       config,
                                                       candleInterval
                                                   }) => {
    const overlayCanvasRef = useRef<HTMLCanvasElement>(null);
    const frameIdRef = useRef<number | null>(null);
    const renderedTradeNumbers = useRef<Set<number>>(new Set());
    const {filteredTrades, loading} = useTradeFilter();
    const tradeLinesRef = useRef<Array<{ trade: any; x1: number; y1: number; x2: number; y2: number }>>([]); // 그려진 라인 정보 저장

    // 마커 렌더링 최적화를 위한 추가 ref
    const renderingTimeoutRef = useRef<ReturnType<typeof setTimeout> | null>(null);
    const isRenderingScheduledRef = useRef<boolean>(false);
    const visibleRangeRef = useRef<{ from: number, to: number } | null>(null);
    const canvasNeedsUpdateRef = useRef<boolean>(true);

    // 호버 상태 추가
    const [hoveredTrade, setHoveredTrade] = useState<{ trade: any; x: number; y: number } | null>(null);

    // 툴팁 참조 추가
    const tooltipRef = useRef<HTMLDivElement>(null);

    // 툴팁 최종 위치 상태 추가
    const [tooltipPosition, setTooltipPosition] = useState<{ top: number; left: number } | null>(null);

    // 미리 가공된 데이터를 캐싱하기 위한 ref 타입 정의 수정
    const processedDataRef = useRef<{
        candles: Map<number, CandleData>;
        candleArray: CandleData[];
        entryMarkers: Array<{ time: number, trade: any }>;
        exitMarkers: Array<{ time: number, trade: any }>;
        filteredTrades: any[]; // 필터링된 거래 목록 추가
        initialBalance: number | null; // 초기 자금 추가
    }>({
        candles: new Map(),
        candleArray: [],
        entryMarkers: [],
        exitMarkers: [],
        filteredTrades: [], // 초기값 추가
        initialBalance: null, // 초기 자금 초기값 추가
    });

    // 컨테이너 크기를 state로 관리해서 불필요한 리사이즈 방지
    const [canvasSize, setCanvasSize] = useState({width: 0, height: 0});

    // 데이터 전처리 useEffect 수정
    useEffect(() => {
        if (!chart || !mainSeries || loading || !symbol || filteredTrades.length === 0 || candleStickData.length === 0) {
            // 데이터가 준비되지 않았거나 로딩 중일 때 캐시 초기화
            processedDataRef.current = {
                candles: new Map(),
                candleArray: [],
                entryMarkers: [],
                exitMarkers: [],
                filteredTrades: [], // 초기화
                initialBalance: null, // 초기화
            };
            return;
        }

        // 캔들 데이터 맵 구성
        const candleMap = new Map<number, CandleData>();
        for (const candle of candleStickData) {
            candleMap.set(Number(candle.time), candle);
        }

        // 진입/청산 마커 데이터 미리 구성
        const entryMarkers: Array<{ time: number, trade: any }> = [];
        const exitMarkers: Array<{ time: number, trade: any }> = [];

        // 현재 symbol과 일치하는 거래만 필터링 (여기서 한 번만!)
        const tradesForCurrentSymbol = filteredTrades.filter(
            (trade: any) => trade["심볼 이름"] === symbol
        );

        // 초기 자금 계산
        let initialBalance: number | null = null;
        const initialTrade = filteredTrades.find(trade => trade["거래 번호"] === 0);
        if (initialTrade && initialTrade["현재 자금"] !== undefined) {
            initialBalance = Number(String(initialTrade["현재 자금"]).replace(/,/g, ''));
        } else if (tradesForCurrentSymbol.length > 0 && tradesForCurrentSymbol[0]["현재 자금"] !== undefined) {
            // 거래 0번이 없으면 첫 번째 거래의 현재 자금을 초기 자금으로 사용 (안전장치)
            initialBalance = Number(String(tradesForCurrentSymbol[0]["현재 자금"]).replace(/,/g, ''));
        }

        // 필터링된 거래 목록에서 마커 정보 추출
        for (let i = 0; i < tradesForCurrentSymbol.length; i++) {
            const trade = tradesForCurrentSymbol[i];
            const entryTimeStr = String(trade["진입 시간"]);
            const exitTimeStr = String(trade["청산 시간"]);
            const entryTimestamp = parseTimeStringToUTCSec(entryTimeStr);
            const exitTimestamp = parseTimeStringToUTCSec(exitTimeStr);
            if (entryTimestamp) entryMarkers.push({time: entryTimestamp, trade});
            if (exitTimestamp) exitMarkers.push({time: exitTimestamp, trade});
        }

        // 처리된 데이터 저장 (필터링된 거래 목록 포함)
        processedDataRef.current = {
            candles: candleMap,
            candleArray: [...candleStickData],
            entryMarkers,
            exitMarkers,
            filteredTrades: tradesForCurrentSymbol, // 저장!
            initialBalance,
        };

    }, [chart, mainSeries, candleStickData, filteredTrades, symbol, loading]);

    // 컨테이너 크기 변경 감지 및 캔버스 크기 조정 최적화
    useEffect(() => {
        if (!containerRef.current || !overlayCanvasRef.current) return;

        const updateCanvasSize = () => {
            if (!containerRef.current) return;
            const rect = containerRef.current.getBoundingClientRect();

            // 실제로 크기가 변경된 경우에만 캔버스 크기 조정 및 리렌더링
            if (rect.width !== canvasSize.width || rect.height !== canvasSize.height) {
                setCanvasSize({width: rect.width, height: rect.height});

                if (overlayCanvasRef.current) {
                    overlayCanvasRef.current.width = rect.width;
                    overlayCanvasRef.current.height = rect.height;

                    // 호버 상태 초기화
                    setHoveredTrade(null);

                    // 캔버스 크기 변경 시 즉시 리렌더링 필요성 표시
                    canvasNeedsUpdateRef.current = true;
                }
            }
        };

        // 초기 실행
        updateCanvasSize();

        // ResizeObserver 사용하여 크기 변경 감지
        const resizeObserver = new ResizeObserver(throttle(updateCanvasSize, 200)); // 리사이즈 스로틀링 추가
        resizeObserver.observe(containerRef.current);

        return () => {
            resizeObserver.disconnect();
        };
    }, [containerRef, canvasSize]);

    // findNearestCandleTime 함수 대체 - Lightweight Charts API 활용
    const findNearestCandleUsingLightweightAPI = (targetTime: number): CandleData | null => {
        if (!chart || !mainSeries || candleStickData.length === 0) return null;

        // 시간 스케일에서 좌표 변환 기능 사용
        const timeScale = chart.timeScale();
        const point = timeScale.timeToCoordinate(targetTime as Time);

        // 좌표가 유효하지 않으면 null 반환
        if (point === null) {
            // fallback: 이진 탐색으로 가장 가까운 시간 찾기
            return findNearestCandleTime(candleStickData, targetTime);
        }

        // 좌표에서 시간으로 다시 변환 (가장 가까운 시간 포인트 가져옴)
        const nearestTime = timeScale.coordinateToTime(point);
        if (nearestTime === null) {
            // fallback: 이진 탐색으로 가장 가까운 시간 찾기
            return findNearestCandleTime(candleStickData, targetTime);
        }

        // 타겟 시간보다 큰 가장 가까운 캔들의 인덱스를 찾기
        let leftIdx = 0;
        let rightIdx = candleStickData.length - 1;
        let candidateIdx = -1; // 타겟 시간보다 큰 첫 번째 캔들 인덱스

        while (leftIdx <= rightIdx) {
            const midIdx = Math.floor((leftIdx + rightIdx) / 2);
            const candleTime = Number(candleStickData[midIdx].time);

            if (candleTime === targetTime) {
                // 정확히 일치하는 시간을 찾으면 바로 반환
                return candleStickData[midIdx];
            } else if (candleTime > targetTime) {
                // 타겟 시간보다 큰 캔들 찾음
                candidateIdx = midIdx;
                rightIdx = midIdx - 1;
            } else {
                leftIdx = midIdx + 1;
            }
        }

        // 타겟 시간보다 작거나 같은 캔들 중 가장 큰 시간을 가진 캔들 찾기
        if (candidateIdx > 0) {
            // 찾은 캔들의 바로 이전 캔들 반환 (타겟 시간보다 작거나 같은 캔들 중 가장 근접한 것)
            return candleStickData[candidateIdx - 1];
        } else if (candidateIdx === 0) {
            // 모든 캔들이 타겟 시간보다 크다면, 첫 번째 캔들 반환
            return candleStickData[0];
        } else {
            // 모든 캔들이 타겟 시간보다 작다면, 마지막 캔들 반환
            return candleStickData[candleStickData.length - 1];
        }
    };

    // 삼각형 마커 그리기 헬퍼 함수 (BaseChart.html SVG 모양 기준)
    const drawTriangleMarker = (
        ctx: CanvasRenderingContext2D,
        x: number,
        y: number,
        color: string,
        isEntry: boolean // true: 진입 (오른쪽 뾰족), false: 청산 (왼쪽 뾰족)
    ) => {
        const triangleWidth = 9;
        const triangleHeight = 11;
        const halfHeight = triangleHeight / 2; // 5.5

        ctx.fillStyle = color;
        ctx.beginPath();

        if (isEntry) {
            // 오른쪽을 가리키는 삼각형 (HTML의 position: 'left')
            ctx.moveTo(x - triangleWidth, y - halfHeight);
            ctx.lineTo(x - triangleWidth, y + halfHeight);
            ctx.lineTo(x, y);
        } else {
            // 왼쪽을 가리키는 삼각형 (HTML의 position: 'right')
            ctx.moveTo(x + triangleWidth, y - halfHeight);
            ctx.lineTo(x + triangleWidth, y + halfHeight);
            ctx.lineTo(x, y);
        }

        ctx.closePath();
        ctx.fill();

        // 검은색 테두리 추가
        ctx.strokeStyle = 'black';
        ctx.lineWidth = 1;
        ctx.stroke();
    };

    // 캐시 최적화를 위한 변수들 추가
    const cachedMarkerPositionsRef = useRef<{
        entryMarkers: Map<string, { x: number, y: number, tradeNumber: number, entryDirection: string }>;
        exitMarkers: Map<string, { x: number, y: number, entryDirection: string }>;
        tradeLines: Map<number, { x1: number, y1: number, x2: number, y2: number, netProfitLoss: number }>;
    }>({
        entryMarkers: new Map(),
        exitMarkers: new Map(),
        tradeLines: new Map()
    });

    // 캐시 초기화 함수
    const clearMarkerCache = () => {
        cachedMarkerPositionsRef.current.entryMarkers.clear();
        cachedMarkerPositionsRef.current.exitMarkers.clear();
        cachedMarkerPositionsRef.current.tradeLines.clear();
    };

    // 마커 그리기 함수를 최적화
    const drawMarkers = useCallback(() => {
        // 렌더링 상태 업데이트
        isRenderingScheduledRef.current = false;

        // 컨테이너가 실제로 보이는지 확인
        if (!containerRef.current || containerRef.current.offsetParent === null) {
            // 안 보이면 캔버스 클리어하고 종료
            const canvas = overlayCanvasRef.current;
            const ctx = canvas?.getContext('2d');
            if (ctx && canvas) {
                ctx.clearRect(0, 0, canvas.width, canvas.height);
            }
            return;
        }

        if (!chart || !mainSeries || !overlayCanvasRef.current || loading) {
            return;
        }

        const canvas = overlayCanvasRef.current;
        const ctx = canvas.getContext('2d', {alpha: true});
        if (!ctx) return;

        // 시간 범위 확인
        const timeScale = chart.timeScale();
        const visibleRange = timeScale.getVisibleRange();
        if (!visibleRange) {
            return;
        }

        const visibleStartTime = Number(visibleRange.from);
        const visibleEndTime = Number(visibleRange.to);

        // 차트가 움직이는 동안은 항상 업데이트하도록 rangeChanged 항상 true로 설정
        const rangeChanged = true;

        // 업데이트 완료 처리
        canvasNeedsUpdateRef.current = false;
        visibleRangeRef.current = {from: visibleStartTime, to: visibleEndTime};

        // 캔버스 크기 설정 (DPI 설정은 유지)
        if (canvas.width !== canvas.clientWidth || canvas.height !== canvas.clientHeight) {
            const containerRect = containerRef.current.getBoundingClientRect();
            canvas.width = containerRect.width;
            canvas.height = containerRect.height;
        }

        // 캔버스 비우기
        ctx.clearRect(0, 0, canvas.width, canvas.height);

        // 라인 정보 초기화
        tradeLinesRef.current = [];

        // === 클리핑 영역 설정 ===
        ctx.save();

        // 클리핑 영역 계산
        let clipX = 0;
        let clipY = 0;
        let clipWidth = canvas.width;
        let clipHeight = canvas.height;

        try {
            // 프라이스 스케일 너비 가져오기
            const priceScaleWidth = chart.priceScale('right').width() || 0;

            // 오직 0번 페인(메인 차트)의 높이만 사용
            let paneHeight = 0;
            if (chart.panes().length > 0) {
                paneHeight = chart.panes()[0].getHeight();
            }

            // 클리핑 영역 계산 (무조건 0번 페인 높이 기준)
            clipWidth = canvas.width - priceScaleWidth;
            clipHeight = paneHeight;

            // 페인 높이가 유효한 경우에만 클리핑 적용
            if (clipWidth > 0 && paneHeight > 0) {
                ctx.beginPath();
                ctx.rect(0, 0, clipWidth, paneHeight);
                ctx.clip();
            }
        } catch (e) {
            console.error("클리핑 영역 설정 오류:", e);
        }

        const {entryMarkers, exitMarkers, filteredTrades: tradesForLines} = processedDataRef.current;

        // 마커 캐시 초기화 - 로직이 변경되었으니 항상 초기화
        clearMarkerCache();

        // === 마커 개수 확인 및 최적화 플래그 설정 ===
        let visibleMarkerCount = 0;
        for (const markerInfo of entryMarkers) {
            if (markerInfo.time >= visibleStartTime && markerInfo.time <= visibleEndTime) {
                visibleMarkerCount++;
            }
        }
        for (const markerInfo of exitMarkers) {
            if (markerInfo.time >= visibleStartTime && markerInfo.time <= visibleEndTime) {
                visibleMarkerCount++;
            }
        }
        const drawOnlyTriangles = visibleMarkerCount > 100; // 임계값 낮춤 (200 -> 100)
        // === 끝: 마커 개수 확인 ===

        renderedTradeNumbers.current.clear();
        const groupedEntryMarkers = new Map<string, MarkerItem[]>();
        const groupedExitMarkers = new Map<string, MarkerItem[]>();

        // 좌표가 클립 영역 내에 있는지 확인하는 헬퍼 함수
        const isInClipArea = (x: number, y: number): boolean => {
            return x >= clipX && x <= clipWidth && y >= clipY && y <= clipHeight;
        };

        // === 삼각형 마커를 먼저 그리기 위한 캐시된 마커 위치 저장용 배열 ===
        const triangleMarkersToRender: Array<{ x: number, y: number, color: string, isEntry: boolean }> = [];

        // 진입 삼각형 마커 처리 최적화
        const cachedEntryMarkers = cachedMarkerPositionsRef.current.entryMarkers;
        for (const markerInfo of entryMarkers) {
            const {time, trade} = markerInfo;
            if (time < visibleStartTime || time > (visibleEndTime + candleInterval - 1)) {
                continue;
            }

            const tradeNumber = Number(trade["거래 번호"]);
            const tradeKey = `e_${tradeNumber}`;

            // 캐시된 마커 정보가 있는지 확인
            let markerPosition = cachedEntryMarkers.get(tradeKey);

            if (!markerPosition || rangeChanged) {
                const nearestCandle = findNearestCandleUsingLightweightAPI(time);
                if (!nearestCandle) continue;

                const entryDirection = String(trade["진입 방향"]);
                const entryPrice = parseFloat(String(trade["진입 가격"]));

                const x = timeScale.timeToCoordinate(nearestCandle.time as Time);
                const y = mainSeries.priceToCoordinate(entryPrice);

                if (x === null || y === null) continue;

                // 새 위치 정보 캐시
                markerPosition = {x, y, tradeNumber, entryDirection};
                cachedEntryMarkers.set(tradeKey, markerPosition);

                // 텍스트 마커 그룹화 위한 정보 저장
                const key = `${nearestCandle.time}_${entryDirection}`;
                if (!groupedEntryMarkers.has(key)) groupedEntryMarkers.set(key, []);
                groupedEntryMarkers.get(key)?.push({trade, candle: nearestCandle});
            }

            // 마커가 클립 영역 내에 있고 아직 그려지지 않았으면 렌더링 목록에 추가
            if (isInClipArea(markerPosition.x, markerPosition.y) && !renderedTradeNumbers.current.has(tradeNumber)) {
                const color = markerPosition.entryDirection === "매수" ? "#388e3c" : "#d32f2f";
                triangleMarkersToRender.push({x: markerPosition.x, y: markerPosition.y, color, isEntry: true});
                renderedTradeNumbers.current.add(tradeNumber);
            }
        }

        // drawMarkers 내부 청산 마커 루프
        const cachedExitMarkers = cachedMarkerPositionsRef.current.exitMarkers;
        for (const markerInfo of exitMarkers) {
            const {time, trade} = markerInfo;
            const tradeNumber = Number(trade["거래 번호"]);
            if (time < visibleStartTime || time > (visibleEndTime + candleInterval - 1)) {
                continue;
            }

            const tradeKey = `x_${tradeNumber}`;
            let markerPosition = cachedExitMarkers.get(tradeKey);
            if (!markerPosition || rangeChanged) {
                const nearestCandle = findNearestCandleUsingLightweightAPI(time);
                if (!nearestCandle) {
                    continue;
                }

                const entryDirection = String(trade["진입 방향"]);
                const exitPrice = parseFloat(String(trade["청산 가격"]));
                const x = timeScale.timeToCoordinate(nearestCandle.time as Time);
                const y = mainSeries.priceToCoordinate(exitPrice);
                if (x === null || y === null) {
                    continue;
                }

                markerPosition = {x, y, entryDirection};
                cachedExitMarkers.set(tradeKey, markerPosition);
                const key = `${nearestCandle.time}_${entryDirection}`;
                if (!groupedExitMarkers.has(key)) {
                    groupedExitMarkers.set(key, []);
                }

                groupedExitMarkers.get(key)?.push({trade, candle: nearestCandle});
            }
            if (isInClipArea(markerPosition.x, markerPosition.y)) {
                const color = markerPosition.entryDirection === "매수" ? "#d32f2f" : "#388e3c";
                triangleMarkersToRender.push({x: markerPosition.x, y: markerPosition.y, color, isEntry: false});
            }
        }

        // === 텍스트 및 연결선 조건부 렌더링 ===
        if (!drawOnlyTriangles) {
            // === 모든 텍스트를 하나로 통합해서 처리 (위/아래 스택 2개만 사용) ===
            const allTexts: Array<{
                x: number;
                y: number;
                text: string;
                tradeNumber: number;
                candleTime: Time;
                stackType: 'top' | 'bottom';
            }> = [];

            // 진입 텍스트 수집
            for (const [_, markersGroup] of groupedEntryMarkers.entries()) {
                markersGroup.forEach((item: MarkerItem) => {
                    const {trade, candle} = item;
                    const tradeNumber = Number(trade["거래 번호"]);
                    const direction = String(trade["진입 방향"]);
                    const entryName = String(trade["진입 이름"]);
                    const basePrice = direction === '매수' ? candle.low : candle.high;
                    const y = mainSeries.priceToCoordinate(basePrice);
                    const x = timeScale.timeToCoordinate(candle.time as Time);
                    if (x !== null && y !== null) {
                        // 매수 진입(아래), 매도 진입(위)
                        allTexts.push({
                            x, y, text: entryName, tradeNumber, candleTime: candle.time,
                            stackType: direction === '매수' ? 'bottom' : 'top'
                        });
                    }
                });
            }
            // 청산 텍스트 수집
            for (const [_, markersGroup] of groupedExitMarkers.entries()) {
                markersGroup.forEach((item: MarkerItem) => {
                    const {trade, candle} = item;
                    const tradeNumber = Number(trade["거래 번호"]);
                    const direction = String(trade["진입 방향"]);
                    const exitName = String(trade["청산 이름"]);
                    const basePrice = direction === '매수' ? candle.high : candle.low;
                    const y = mainSeries.priceToCoordinate(basePrice);
                    const x = timeScale.timeToCoordinate(candle.time as Time);
                    if (x !== null && y !== null) {
                        // 매수 청산(위), 매도 청산(아래)
                        allTexts.push({
                            x, y, text: exitName, tradeNumber, candleTime: candle.time,
                            stackType: direction === '매수' ? 'top' : 'bottom'
                        });
                    }
                });
            }
            // 거래 번호 순으로 정렬
            allTexts.sort((a, b) => a.tradeNumber - b.tradeNumber);
            // 캔들별 위/아래 스택 관리
            const candleStacks = new Map<string, { top: number; bottom: number }>();
            // 각 텍스트의 최종 위치 계산 및 그리기
            allTexts.forEach(textInfo => {
                const candleKey = `${textInfo.candleTime}`;
                if (!candleStacks.has(candleKey)) {
                    candleStacks.set(candleKey, {top: 0, bottom: 0});
                }
                const stack = candleStacks.get(candleKey)!;
                let finalY: number;
                if (textInfo.stackType === 'top') {
                    finalY = textInfo.y - 15 - (stack.top * 20);
                    stack.top++;
                } else {
                    finalY = textInfo.y + 15 + (stack.bottom * 20);
                    stack.bottom++;
                }
                // 텍스트 그리기
                if (isInClipArea(textInfo.x, finalY)) {
                    ctx.font = '12px "Inter", "Pretendard", sans-serif';
                    ctx.textAlign = 'center';
                    ctx.textBaseline = 'middle';
                    ctx.fillStyle = '#000000';
                    ctx.fillText(textInfo.text, textInfo.x - 1, finalY - 1);
                    ctx.fillText(textInfo.text, textInfo.x + 1, finalY - 1);
                    ctx.fillText(textInfo.text, textInfo.x - 1, finalY + 1);
                    ctx.fillText(textInfo.text, textInfo.x + 1, finalY + 1);
                    ctx.fillStyle = '#ffffff';
                    ctx.fillText(textInfo.text, textInfo.x, finalY);
                }
            });

            // --- 연결선 그리기 (라인 좌표 저장 및 성능 최적화) ---
            ctx.save();
            ctx.setLineDash([4, 2]);
            ctx.lineWidth = 2;

            // 캐시된 라인 정보
            const cachedTradeLines = cachedMarkerPositionsRef.current.tradeLines;

            // 배치 그리기 위한 라인 정렬
            const profitLines: { x1: number, y1: number, x2: number, y2: number }[] = [];
            const lossLines: { x1: number, y1: number, x2: number, y2: number }[] = [];

            for (let i = 0; i < tradesForLines.length; i++) {
                const trade = tradesForLines[i];
                const tradeNumber = Number(trade["거래 번호"]);

                // 캐시 확인
                let lineInfo = cachedTradeLines.get(tradeNumber);

                if (!lineInfo || rangeChanged) {
                    const entryTimeStr = String(trade["진입 시간"]);
                    const exitTimeStr = String(trade["청산 시간"]);
                    const entryPrice = parseFloat(String(trade["진입 가격"]));
                    const exitPrice = parseFloat(String(trade["청산 가격"]));
                    const netProfitLoss = parseFloat(String(trade["순손익"]).replace(/,/g, ''));
                    const entryTimestamp = parseTimeStringToUTCSec(entryTimeStr);
                    const exitTimestamp = parseTimeStringToUTCSec(exitTimeStr);

                    if (!entryTimestamp || !exitTimestamp) continue;

                    // 시간 필터링: 시간 범위를 벗어나면 건너뛰기
                    if ((entryTimestamp < visibleStartTime && exitTimestamp < visibleStartTime) ||
                        (entryTimestamp > visibleEndTime && exitTimestamp > visibleEndTime)) {
                        continue;
                    }

                    const entryCandle = findNearestCandleUsingLightweightAPI(entryTimestamp);
                    const exitCandle = findNearestCandleUsingLightweightAPI(exitTimestamp);

                    if (!entryCandle || !exitCandle) continue;

                    const entryX = timeScale.timeToCoordinate(entryCandle.time as Time);
                    const entryY = mainSeries.priceToCoordinate(entryPrice);
                    const exitX = timeScale.timeToCoordinate(exitCandle.time as Time);
                    const exitY = mainSeries.priceToCoordinate(exitPrice);

                    if (entryX === null || entryY === null || exitX === null || exitY === null) continue;

                    // 계산된 정보 캐시
                    lineInfo = {x1: entryX, y1: entryY, x2: exitX, y2: exitY, netProfitLoss};
                    cachedTradeLines.set(tradeNumber, lineInfo);
                }

                // 클립 영역 체크: 양쪽 끝점이 모두 클립 영역 밖이면 그리지 않음
                const {x1, y1, x2, y2, netProfitLoss} = lineInfo;

                if (isInClipArea(x1, y1) || isInClipArea(x2, y2) ||
                    (x1 < clipX && x2 > clipWidth) || (x1 > clipWidth && x2 < clipX) ||
                    (y1 < clipY && y2 > clipHeight) || (y1 > clipHeight && y2 < clipY)) {

                    // 라인 정보 저장 (호버 효과를 위해)
                    tradeLinesRef.current.push({trade, x1, y1, x2, y2});

                    // 배치 그리기를 위해 분류
                    if (netProfitLoss > 0) {
                        profitLines.push({x1, y1, x2, y2});
                    } else {
                        lossLines.push({x1, y1, x2, y2});
                    }
                }
            }

            // 수익 라인 한번에 그리기
            if (profitLines.length > 0) {
                ctx.strokeStyle = "#388e3c";
                ctx.beginPath();
                for (const line of profitLines) {
                    ctx.moveTo(line.x1, line.y1);
                    ctx.lineTo(line.x2, line.y2);
                }
                ctx.stroke();
            }

            // 손실 라인 한번에 그리기
            if (lossLines.length > 0) {
                ctx.strokeStyle = "#d32f2f";
                ctx.beginPath();
                for (const line of lossLines) {
                    ctx.moveTo(line.x1, line.y1);
                    ctx.lineTo(line.x2, line.y2);
                }
                ctx.stroke();
            }

            ctx.restore();
        }
        // === 끝: 텍스트 및 연결선 조건부 렌더링 ===

        // === 삼각형 마커를 마지막에 그리기 (점선 위에 그려지도록) ===
        triangleMarkersToRender.forEach(marker => {
            drawTriangleMarker(ctx, marker.x, marker.y, marker.color, marker.isEntry);
        });

        // 클리핑 컨텍스트 복원
        ctx.restore();
    }, [chart, mainSeries, findNearestCandleUsingLightweightAPI, containerRef]);

    // 차트 이벤트 구독 및 애니메이션 루프 최적화
    useEffect(() => {
        if (!chart) return;

        // 초기 렌더링
        canvasNeedsUpdateRef.current = true;

        // 애니메이션 루프 복원 - 항상 렌더링하도록 수정
        const renderLoop = () => {
            // 항상 그리도록 변경 (조건부 렌더링 제거)
            drawMarkers();
            frameIdRef.current = requestAnimationFrame(renderLoop);
        };

        // 첫 렌더링 시작
        frameIdRef.current = requestAnimationFrame(renderLoop);

        // 차트 상호작용 이벤트 핸들러 (축 크기 변경, 차트 확대/축소 등에 대응)
        const handleTimeScaleChange = () => {
            // 차트 범위가 변경되었을 때 즉시 렌더링 필요성 표시
            canvasNeedsUpdateRef.current = true;
            // 여기서 강제 업데이트는 필요 없음 - 애니메이션 루프가 항상 실행됨
        };

        // 이벤트 구독 - 우선순위 높게 설정
        chart.timeScale().subscribeVisibleLogicalRangeChange(handleTimeScaleChange);

        // 윈도우 리사이즈 이벤트 리스너 - 스로틀링 시간 줄임
        const handleResize = () => {
            canvasNeedsUpdateRef.current = true;
        };
        window.addEventListener('resize', throttle(handleResize, 100)); // 200ms → 100ms

        return () => {
            // 이벤트 구독 해제
            if (chart) {
                chart.timeScale().unsubscribeVisibleLogicalRangeChange(handleTimeScaleChange);
            }

            window.removeEventListener('resize', handleResize);

            // 애니메이션 프레임 정리
            if (frameIdRef.current !== null) {
                cancelAnimationFrame(frameIdRef.current);
                frameIdRef.current = null;
            }

            if (renderingTimeoutRef.current) {
                clearTimeout(renderingTimeoutRef.current);
                renderingTimeoutRef.current = null;
            }

            const canvas = overlayCanvasRef.current;
            const ctx = canvas?.getContext('2d');
            if (ctx && canvas) {
                ctx.clearRect(0, 0, canvas.width, canvas.height);
            }
        };
    }, [symbol, chart, mainSeries, filteredTrades, loading, candleStickData, drawMarkers]);

    // 마우스 이동 이벤트 핸들러 최적화
    const handleMouseMove = useCallback(throttle((event: MouseEvent) => {
        if (!overlayCanvasRef.current || tradeLinesRef.current.length === 0 || !containerRef.current) {
            if (hoveredTrade) setHoveredTrade(null);
            return;
        }

        const canvas = overlayCanvasRef.current;
        const rect = canvas.getBoundingClientRect(); // 캔버스 기준 좌표
        const mouseX = event.clientX - rect.left;
        const mouseY = event.clientY - rect.top;
        const point = {x: mouseX, y: mouseY};

        let closestLine = null;
        let minDistSq = 10 * 10;

        // 성능 최적화: 최대 검사할 라인 수 제한 (화면에 보이는 선이 너무 많을 경우 처리 시간이 길어짐)
        const maxLinesToCheck = Math.min(tradeLinesRef.current.length, 100);

        for (let i = 0; i < maxLinesToCheck; i++) {
            const line = tradeLinesRef.current[i];
            const v = {x: line.x1, y: line.y1};
            const w = {x: line.x2, y: line.y2};
            const distSq = distToSegmentSquared(point, v, w);

            if (distSq < minDistSq) {
                minDistSq = distSq;
                closestLine = line;
            }
        }

        if (closestLine) {
            // 마우스 위치(캔버스 기준)를 함께 저장
            setHoveredTrade({trade: closestLine.trade, x: mouseX, y: mouseY});
        } else {
            if (hoveredTrade) {
                setHoveredTrade(null);
            }
        }
    }, 50), [hoveredTrade, containerRef]); // 스로틀링 간격 50ms로 변경

    // 마우스 리브 이벤트 핸들러
    const handleMouseLeave = useCallback(() => {
        setHoveredTrade(null);
    }, []);

    // 이벤트 리스너 등록 및 해제
    useEffect(() => {
        const container = containerRef.current;
        if (container) {
            container.addEventListener('mousemove', handleMouseMove);
            container.addEventListener('mouseleave', handleMouseLeave);
        }

        return () => {
            if (container) {
                container.removeEventListener('mousemove', handleMouseMove);
                container.removeEventListener('mouseleave', handleMouseLeave);
            }
        };
    }, [containerRef, handleMouseMove, handleMouseLeave]); // 핸들러 함수 자체를 의존성으로

    // === 툴팁 위치 계산 로직 추가 ===
    useEffect(() => {
        if (hoveredTrade && tooltipRef.current && containerRef.current && chart) {
            const tooltipElement = tooltipRef.current;
            const containerElement = containerRef.current;

            const tooltipHeight = tooltipElement.offsetHeight;
            const tooltipWidth = tooltipElement.offsetWidth;
            const containerRect = containerElement.getBoundingClientRect();
            const containerHeight = containerRect.height;
            const containerWidth = containerRect.width;

            const {x: mouseX, y: mouseY} = hoveredTrade; // 캔버스 기준 마우스 좌표

            const horizontalOffset = 50; // 좌우 간격
            const verticalOffset = 25; // 수직 간격

            let top = mouseY + verticalOffset;
            let left = mouseX + horizontalOffset; // 기본: 마우스 오른쪽

            // 2. 오른쪽 경계 체크: 오른쪽 Y축(priceScale)의 왼쪽을 경계로 설정
            const priceScaleRightWidth = chart.priceScale('right').width() ?? 0;
            const effectiveRightBoundary = containerWidth - priceScaleRightWidth;

            // 오른쪽으로 넘어가면 마우스 왼쪽으로 툴팁 위치 변경
            if (left + tooltipWidth > effectiveRightBoundary) {
                left = mouseX - horizontalOffset - tooltipWidth;
            }

            // 왼쪽 클리핑 확인 (왼쪽으로 옮겼을 때 너무 좌측으로 가지 않도록)
            // EquityCurve.tsx의 priceAxisLabel 처럼 왼쪽 price scale 너비를 고려할 수도 있지만,
            // 여기서는 일단 화면 왼쪽 가장자리를 기준으로 최소 여백(5px)만 확보
            if (left < 5) {
                left = 5;
                // 만약 왼쪽으로 붙였음에도 오른쪽 경계를 넘는다면 (툴팁이 매우 넓고 오른쪽 Y축 공간이 좁을 때)
                // 이 경우 툴팁이 잘릴 수 있음. 추가적인 복잡한 로직이 필요할 수 있으나 일단 현재 상태 유지.
            }

            // 아래쪽 클리핑 확인
            if (top + tooltipHeight > containerHeight) {
                top = mouseY - verticalOffset - tooltipHeight;
                if (top < 0) {
                    top = 5;
                }
            }

            setTooltipPosition({top, left});
        } else {
            setTooltipPosition(null);
        }
    }, [hoveredTrade, containerRef, chart]); // chart 의존성 추가

    // 컴포넌트 정의 시 초기화 코드 추가
    useEffect(() => {
        renderedTradeNumbers.current.clear();

        // 컴포넌트 언마운트 시 정리
        return () => {
            renderedTradeNumbers.current.clear();
        };
    }, []);

    // 차트가 초기화될 때 마커를 확실히 그리기 위한 useEffect
    useEffect(() => {
        if (!chart || !mainSeries) return;

        // 약간의 지연 후 마커 그리기 (차트 초기화 완료 후)
        const timer = setTimeout(() => {
            canvasNeedsUpdateRef.current = true;
            drawMarkers();
        }, 300);

        return () => {
            clearTimeout(timer);
        };
    }, [chart, mainSeries, drawMarkers]);

    return (
        <>
            <canvas
                ref={overlayCanvasRef}
                style={{
                    position: 'absolute',
                    left: 0,
                    top: 0,
                    width: '100%',
                    height: '100%',
                    pointerEvents: 'none',
                    zIndex: 100
                }}
            />
            {/* 툴팁 렌더링 (이제 ref 와 계산된 position 사용) */}
            {hoveredTrade && (
                <div
                    ref={tooltipRef} // ref 연결
                    style={{
                        position: 'absolute',
                        // 계산된 위치 적용
                        top: tooltipPosition ? `${tooltipPosition.top}px` : '-9999px', // 안보이는 곳에 먼저 렌더링
                        left: tooltipPosition ? `${tooltipPosition.left}px` : '-9999px',
                        // visibility 속성 대신 opacity로 제어
                        padding: '10px 15px 5px 15px', // EquityCurve 스타일
                        boxSizing: 'border-box', // EquityCurve 스타일
                        fontSize: '12.5px', // EquityCurve 스타일
                        color: '#eee', // EquityCurve 스타일
                        background: 'rgba(28, 28, 36, 0.95)', // EquityCurve 스타일
                        borderRadius: '6px', // EquityCurve 스타일
                        border: '1px solid rgba(255, 215, 0, 0.4)', // EquityCurve 스타일
                        boxShadow: '0 4px 12px rgba(0, 0, 0, 0.6)', // EquityCurve 스타일
                        pointerEvents: 'none',
                        zIndex: 1300,
                        fontFamily: "'Inter', 'Pretendard', sans-serif", // EquityCurve 스타일
                        lineHeight: '1.6', // EquityCurve 스타일
                        whiteSpace: 'nowrap', // EquityCurve 스타일
                        overflow: 'visible', // 내용에 맞춰 늘어나도록
                        textOverflow: 'clip', // 잘리지 않도록
                        opacity: tooltipPosition ? 1 : 0, // 애니메이션을 위한 opacity 추가
                        transform: tooltipPosition ? 'translateY(0) scale(1)' : 'translateY(6px) scale(0.98)', // 더 자연스러운 애니메이션을 위한 transform 추가
                        transition: 'opacity 0.3s ease, transform 0.3s ease',
                    }}
                >
                    <strong style={{
                        // EquityCurve 스타일과 유사하게, 하지만 내용에 맞게 조정
                        color: '#ffffff',
                        fontSize: '15px',
                        fontWeight: '600',
                        marginBottom: '4px',
                        display: 'block',
                        borderBottom: '1px solid rgba(255, 215, 0, 0.3)', // EquityCurve 스타일
                        paddingBottom: '5px' // EquityCurve 스타일
                    }}>
                        거래 번호 #{Number(hoveredTrade.trade["거래 번호"]).toLocaleString('en-US')}
                    </strong>
                    {/* 툴팁 내용을 2열로 배치 */}
                    <div style={{margin: '8px 0 0 0', display: 'flex', gap: '15px', width: 'max-content'}}>
                        {(() => {
                            const entries = Object.entries(hoveredTrade.trade)
                                .filter(([key]) =>
                                    key !== "거래 번호" &&
                                    key !== "심볼 이름" &&
                                    key !== "진입 방향"
                                );

                            const midPoint = Math.ceil(entries.length / 2);
                            const leftEntries = entries.slice(0, midPoint);
                            const rightEntries = entries.slice(midPoint);

                            return (
                                <>
                                    {/* 왼쪽 열 */}
                                    <div style={{flex: '1 1 auto', minWidth: '200px', maxWidth: 'none'}}>
                                        {leftEntries.map(([key, value]) => {
                                            const dividerKeys = new Set(["청산 이름", "보유 시간", "레버리지", "진입 수량", "청산 수량", "강제 청산 가격", "펀딩비 수령", "강제 청산 수수료", "순손익", "전체 순손익률", "최고 자금", "최고 드로우다운", "누적 손익률"]);
                                            const shouldAddDivider = dividerKeys.has(key);

                                            let valueColor = '#ffffff'; // 기본 흰색
                                            const numValue = typeof value === 'number' ? value : parseFloat(String(value).replace(/,/g, ''));

                                            if (!isNaN(numValue)) {
                                                if (["손익", "순손익", "개별 순손익률", "전체 순손익률", "누적 손익", "누적 손익률", "펀딩비 수령", "펀딩비 지불", "펀딩비"].includes(key)) {
                                                    // 0이면 흰색, 양수면 초록, 음수면 빨강
                                                    valueColor = numValue === 0 ? '#ffffff' : (numValue > 0 ? '#4caf50' : '#f23645');
                                                } else if (key === "현재 자금") {
                                                    const initialBalance = processedDataRef.current.initialBalance;
                                                    if (initialBalance !== null) {
                                                        valueColor = numValue >= initialBalance ? '#4caf50' : '#f23645';
                                                    }
                                                } else if (key === "최고 자금") {
                                                    valueColor = '#008000';
                                                } else if (key === "드로우다운") {
                                                    valueColor = numValue === 0 ? '#4caf50' : '#f23645';
                                                } else if (key === "최고 드로우다운") {
                                                    valueColor = numValue === 0 ? '#008000' : '#a01722';
                                                }
                                            }

                                            let displayValue = formatValue(value as string | number, key, pricePrecision, config, symbol);
                                            if ((key === "진입 시간" || key === "청산 시간") && typeof value === 'string' && value !== '-') {
                                                try {
                                                    const date = new Date(value);
                                                    if (!isNaN(date.getTime())) { // 유효한 날짜인지 확인
                                                        const days = ['일', '월', '화', '수', '목', '금', '토'];
                                                        const dayName = days[date.getDay()];
                                                        displayValue = `${value} (${dayName})`;
                                                    }
                                                } catch (e) {
                                                    console.error("툴팁 날짜 파싱 오류:", value, e);
                                                    // 파싱 오류 시 기존 formatValue 결과 사용
                                                }
                                            }

                                            return (
                                                <React.Fragment key={key}>
                                                    <div style={{
                                                        display: 'flex',
                                                        justifyContent: 'space-between',
                                                        alignItems: 'baseline',
                                                        marginBottom: '4px',
                                                        gap: '10px' // 필드명과 값 사이 간격 추가
                                                    }}>
                                                        <span style={{
                                                            color: '#aaa',
                                                            fontSize: '13px',
                                                            padding: '0 6px',
                                                            marginRight: '8px',
                                                            position: 'relative',
                                                            left: '-6px',
                                                            flexShrink: 0 // 필드명은 줄어들지 않게
                                                        }}>{key}</span>
                                                        <span style={{
                                                            color: valueColor,
                                                            fontWeight: 'normal',
                                                            fontSize: '14px',
                                                            textAlign: 'right', // 오른쪽 정렬로 변경
                                                            whiteSpace: 'nowrap' // 값이 잘리지 않게
                                                        }}>{displayValue}</span>
                                                    </div>
                                                    {shouldAddDivider && (
                                                        <div style={{
                                                            borderBottom: '1px solid rgba(255, 215, 0, 0.2)',
                                                            margin: '6px 0'
                                                        }}></div>
                                                    )}
                                                </React.Fragment>
                                            );
                                        })}
                                    </div>

                                    {/* 세로 구분선 */}
                                    <div style={{
                                        width: '1px',
                                        backgroundColor: 'rgba(255, 215, 0, 0.3)',
                                        alignSelf: 'stretch'
                                    }}></div>

                                    {/* 오른쪽 열 */}
                                    <div style={{flex: '1 1 auto', minWidth: '200px', maxWidth: 'none'}}>
                                        {rightEntries.map(([key, value]) => {
                                            const dividerKeys = new Set(["청산 이름", "보유 시간", "레버리지", "진입 수량", "청산 수량", "강제 청산 가격", "펀딩비 수령", "강제 청산 수수료", "순손익", "전체 순손익률", "최고 자금", "최고 드로우다운", "누적 손익률"]);
                                            const shouldAddDivider = dividerKeys.has(key);

                                            let valueColor = '#ffffff'; // 기본 흰색
                                            const numValue = typeof value === 'number' ? value : parseFloat(String(value).replace(/,/g, ''));

                                            if (!isNaN(numValue)) {
                                                if (["손익", "순손익", "개별 순손익률", "전체 순손익률", "누적 손익", "누적 손익률", "펀딩비 수령", "펀딩비 지불", "펀딩비"].includes(key)) {
                                                    // 0이면 흰색, 양수면 초록, 음수면 빨강
                                                    valueColor = numValue === 0 ? '#ffffff' : (numValue > 0 ? '#4caf50' : '#f23645');
                                                } else if (key === "현재 자금") {
                                                    const initialBalance = processedDataRef.current.initialBalance;
                                                    if (initialBalance !== null) {
                                                        valueColor = numValue >= initialBalance ? '#4caf50' : '#f23645';
                                                    }
                                                } else if (key === "최고 자금") {
                                                    valueColor = '#008000';
                                                } else if (key === "드로우다운") {
                                                    valueColor = numValue === 0 ? '#4caf50' : '#f23645';
                                                } else if (key === "최고 드로우다운") {
                                                    valueColor = numValue === 0 ? '#008000' : '#a01722';
                                                }
                                            }

                                            let displayValue = formatValue(value as string | number, key, pricePrecision, config, symbol);
                                            if ((key === "진입 시간" || key === "청산 시간") && typeof value === 'string' && value !== '-') {
                                                try {
                                                    const date = new Date(value);
                                                    if (!isNaN(date.getTime())) { // 유효한 날짜인지 확인
                                                        const days = ['일', '월', '화', '수', '목', '금', '토'];
                                                        const dayName = days[date.getDay()];
                                                        displayValue = `${value} (${dayName})`;
                                                    }
                                                } catch (e) {
                                                    console.error("툴팁 날짜 파싱 오류:", value, e);
                                                    // 파싱 오류 시 기존 formatValue 결과 사용
                                                }
                                            }

                                            return (
                                                <React.Fragment key={key}>
                                                    <div style={{
                                                        display: 'flex',
                                                        justifyContent: 'space-between',
                                                        alignItems: 'baseline',
                                                        marginBottom: '4px',
                                                        gap: '10px' // 필드명과 값 사이 간격 추가
                                                    }}>
                                                        <span style={{
                                                            color: '#aaa',
                                                            fontSize: '13px',
                                                            padding: '0 6px',
                                                            marginRight: '8px',
                                                            position: 'relative',
                                                            left: '-6px',
                                                            flexShrink: 0 // 필드명은 줄어들지 않게
                                                        }}>{key}</span>
                                                        <span style={{
                                                            color: valueColor,
                                                            fontWeight: 'normal',
                                                            fontSize: '14px',
                                                            textAlign: 'right', // 오른쪽 정렬로 변경
                                                            whiteSpace: 'nowrap' // 값이 잘리지 않게
                                                        }}>{displayValue}</span>
                                                    </div>
                                                    {shouldAddDivider && (
                                                        <div style={{
                                                            borderBottom: '1px solid rgba(255, 215, 0, 0.2)',
                                                            margin: '6px 0'
                                                        }}></div>
                                                    )}
                                                </React.Fragment>
                                            );
                                        })}
                                    </div>
                                </>
                            );
                        })()}
                    </div>
                </div>
            )}
        </>
    );
};

export default TradeMarkers;
