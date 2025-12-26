import React, {useCallback, useEffect, useRef, useState} from 'react';
import {useWebSocket} from '../Server/WebSocketContext';
import {createChart, CrosshairMode, IChartApi, LineStyle} from 'lightweight-charts';
import IndicatorSeriesContainer from './IndicatorSeriesContainer';
import TimeSlider from "./TimeSlider";
import TopInfo from "./TopInfo";
import TimeAxisTooltip from "./TimeAxisTooltip.tsx";
import PriceAxisTooltip from "./PriceAxisTooltip";
import TradeMarkers from "./TradeMarkers";
import CandleStickRenderer, {CandleData} from './CandleStickRenderer';
import Calendar from './Calendar';
import LoadingSpinner from '@/components/Common/LoadingSpinner';
import NoDataMessage from '@/components/Common/NoDataMessage';
import '@/components/Common/LoadingSpinner.css';

// 메인 차트 높이 설정 (페인 개수에 따라 변화하는 함수)
// 페인 차트 비율은 1개 25%로 시작하여 70%까지 수렴
function calculateMainChartHeight(paneCount: number, windowHeight: number): number {
    const ratio = 0.3 + 0.45 * Math.exp(-0.237 * (paneCount - 1));
    return windowHeight * ratio;
}

export interface IndicatorDataPoint {
    time: number;
    value: number | null;
}

type IndicatorDataMap = Record<string, IndicatorDataPoint[]>;

const Second = 1000;
const Minute = 60 * Second;
const Hour = 60 * Minute;
const Day = 24 * Hour;
const Week = 7 * Day;
const Month = 30 * Day;

function parseTimeframe(timeframe: string): number {
    // 'ms' (밀리초)는 두 글자 단위이므로 우선 처리
    let unit: string;
    let valueStr: string;

    if (timeframe.endsWith('ms')) {
        unit = 'ms';
        valueStr = timeframe.slice(0, -2);
    } else {
        unit = timeframe.slice(-1);
        valueStr = timeframe.slice(0, -1);
    }

    const value = parseInt(valueStr);
    if (isNaN(value)) {
        return Minute / 1000;
    }

    switch (unit) {
        case 'ms':
            return (value) / 1000; // ms -> seconds
        case 's':
            return (value * Second) / 1000;
        case 'm':
            return (value * Minute) / 1000;
        case 'h':
            return (value * Hour) / 1000;
        case 'd':
            return (value * Day) / 1000;
        case 'w':
            return (value * Week) / 1000;
        case 'M':
            return (value * Month) / 1000;
        default:
            return Minute / 1000;
    }
}

const Chart: React.FC<{
    symbol: string;
    timeframe: string;
    priceStep: number;
    pricePrecision: number;
    config?: any;
    onChartLoaded?: () => void
}> = ({
          symbol,
          timeframe,
          priceStep,
          pricePrecision,
          config,
          onChartLoaded
      }) => {
    const chartContainerRef = useRef<HTMLDivElement>(null);
    const chartRef = useRef<IChartApi | null>(null);
    const candleStickRendererRef = useRef<CandleStickRenderer | null>(null);
    const {ws} = useWebSocket();
    const secondsPerBar = parseTimeframe(timeframe);

    // 심볼 탭 전환 관련 언마운트 플래그 추가
    const isUnmountingRef = useRef<boolean>(false);

    // 캔들 데이터 관련 상태 및 ref
    const [candleStickData, setCandleStickData] = useState<CandleData[]>([]);
    const dataCacheRef = useRef<CandleData[]>([]);
    const loadedFromRef = useRef<number | null>(null);
    const loadedToRef = useRef<number | null>(null);
    const pendingRequestRef = useRef<boolean>(false);

    // 지표 데이터 관련
    const pendingIndicatorRequestsRef = useRef<{ [key: string]: boolean }>({});
    const candleDataUpdatedRef = useRef<boolean>(false);
    const [indicatorDataMap, setIndicatorDataMap] = useState<IndicatorDataMap>({});

    // 차트 표시 관련
    const [isLoading, setIsLoading] = useState(true);
    const [hasError, setHasError] = useState(false);
    const [showSpinner, setShowSpinner] = useState(true);
    const [isCandleReady, setIsCandleReady] = useState(false);
    const [chartVisible, setChartVisible] = useState(true);
    const [showNoDataMessage, setShowNoDataMessage] = useState(false);
    const [errorType, setErrorType] = useState<'candlestick' | 'indicator' | 'general'>('general');

    // 차트 초기화 완료 여부를 상태로 관리
    const [isChartInitialized, setIsChartInitialized] = useState(false);

    // 데이터 로딩 요청 실패 횟수 추적
    const requestFailCountRef = useRef<number>(0);
    const MAX_RETRY_COUNT = 3;

    // 타임아웃 처리를 위한 타이머
    const timeoutTimerRef = useRef<NodeJS.Timeout | null>(null);
    const TIMEOUT_DURATION = 30000; // 30초 타임아웃

    // 지표 요청 중인지 추적하는 플래그 추가
    const indicatorRequestInProgressRef = useRef<boolean>(false);

    // 최신 isLoading 상태를 추적하는 ref
    const isLoadingRef = useRef<boolean>(true);

    // 현재 요청 타입을 추적하는 ref 추가 (date 타입일 때 데이터 초기화 위함)
    const currentRequestTypeRef = useRef<string | null>(null);

    // TimeSlider 잠금 상태를 관리하는 state 추가
    const [isTimeSliderLocked, setIsTimeSliderLocked] = useState(true);

    // TimeSlider로 이동할 목표 시간을 관리하는 state 추가
    const [targetTime, setTargetTime] = useState<number | undefined>(undefined);

    // 달력 이동 완료 여부를 추적하는 ref 추가
    const calendarMoveCompletedRef = useRef<boolean>(false);

    // 달력 표시 상태 관리
    const [showCalendar, setShowCalendar] = useState(false);

    // 달력 선택 정보를 저장하는 상태
    const [calendarLastSelectedDate, setCalendarLastSelectedDate] = useState<Date | null>(null);
    const [calendarLastSelectedTime, setCalendarLastSelectedTime] = useState<string>('00:00');

    // 달력이 처음 열리는 것인지 추적 (true면 첫 데이터 포인트 시간 사용)
    const [isCalendarFirstOpen, setIsCalendarFirstOpen] = useState(true);

    // 캘린더 이동 중 로딩 상태 관리
    const [isCalendarLoading, setIsCalendarLoading] = useState(false);

    // 마우스 오른쪽 클릭 이벤트 핸들러
    const handleContextMenu = useCallback((e: React.MouseEvent) => {
        e.preventDefault(); // 기본 컨텍스트 메뉴 방지
        setShowCalendar(true);
    }, []);

    // 캘린더 로딩 상태 제어 콜백
    const handleCalendarLoadingStart = useCallback(() => {
        setIsCalendarLoading(true);
    }, []);

    const handleCalendarLoadingEnd = useCallback(() => {
        setIsCalendarLoading(false);
    }, []);

    // 특정 시간으로 차트 이동
    const moveToTimestamp = useCallback((timestamp: number) => {
        if (!chartRef.current || dataCacheRef.current.length === 0) return;

        // 달력 이동 시작 플래그 설정
        calendarMoveCompletedRef.current = false;

        // 새로운 방식: 같은 타임스탬프가 연속으로 선택되어도 이동할 수 있도록
        // 먼저 undefined로 설정했다가 바로 새 값으로 설정
        setTargetTime(undefined);

        // setTimeout을 사용하여 다음 렌더링 사이클에서 새 값을 설정
        setTimeout(() => {
            setTargetTime(timestamp);
        }, 0);
    }, []);

    /*──────────────────────────── 데이터 로드 함수 (통합) ────────────────────────────*/
    const loadChartData = (params: { initial?: boolean; referenceTime?: number }) => {
        if (!ws || ws.readyState !== WebSocket.OPEN) {
            if (params.initial) { // 초기 로드 재시도 관련
                requestFailCountRef.current += 1;

                if (requestFailCountRef.current >= MAX_RETRY_COUNT) {
                    setHasError(true);
                    setIsLoading(false); // 로딩 상태 해제
                    pendingRequestRef.current = false; // 요청 상태 해제
                } else {
                    setTimeout(() => loadChartData({initial: true}), 1000);
                }
            } else {
                // 초기 요청이 아닌 경우, 요청 실패 시 pendingRequestRef 해제
                pendingRequestRef.current = false;
            }
            return;
        }

        const isInitialCall = !!params.initial;

        if (isInitialCall) { // 초기 로드
            setIsLoading(true);
            isLoadingRef.current = true; // isLoadingRef도 동기화
            setHasError(false);
            pendingIndicatorRequestsRef.current = {};
            candleDataUpdatedRef.current = false;
            indicatorRequestInProgressRef.current = false;
            pendingRequestRef.current = true; // 초기 로드도 요청 중으로 표시
            setIsTimeSliderLocked(true); // 데이터 로드 시작 시 슬라이더 잠금
        } else { // 추가 데이터 로드
            if (pendingRequestRef.current || isLoadingRef.current) {
                return;
            }

            setIsTimeSliderLocked(true); // 데이터 로드 시작 시 슬라이더 잠금
            pendingRequestRef.current = true; // 요청 시작 플래그
        }

        const indicatorsToLoad = window.indicatorPaths ? Object.keys(window.indicatorPaths) : [];

        const requestPayload: any = {
            action: "loadChartData",
            symbol,
            indicators: indicatorsToLoad,
            fileRequest: {} // 파일 요청 객체 초기화
        };


        // 캔들 데이터의 시간 범위(from, to) 계산
        let from = null, to = null;
        if (isInitialCall && dataCacheRef.current.length > 0) {
            from = dataCacheRef.current[0].time;
            to = dataCacheRef.current[dataCacheRef.current.length - 1].time;
        } else if (!isInitialCall && dataCacheRef.current.length > 0) {
            // 추가 로딩 시 기존 데이터의 마지막 이후부터 요청
            from = dataCacheRef.current[dataCacheRef.current.length - 1].time;
            to = from; // 최소 단위로 맞춤 (서버에서 확장 처리)
        }

        if (isInitialCall) {
            // 초기 로딩
            requestPayload.fileRequest = {
                type: "initial",
                count: 5,
                dataPoints: 50000,
                from,
                to
            };
            currentRequestTypeRef.current = "initial";
        } else if (params.referenceTime !== undefined) {
            // 추가 로딩
            requestPayload.fileRequest = {
                type: "newer",
                count: 1,
                referenceTime: params.referenceTime,
                dataPoints: 10000,
                from,
                to
            };
            currentRequestTypeRef.current = "newer";
        } else {
            // 유효한 fileRequest를 만들 수 없는 경우 (e.g. referenceTime 누락) 요청 중단
            pendingRequestRef.current = false; // 요청 중단 시 플래그 해제
            if (isInitialCall) {
                setIsLoading(false); // 유효하지 않은 요청 시 로딩 상태 해제
                isLoadingRef.current = false;
            }
            return;
        }

        ws.send(JSON.stringify(requestPayload));

        // 타임아웃 설정
        if (timeoutTimerRef.current) {
            clearTimeout(timeoutTimerRef.current);
        }

        timeoutTimerRef.current = setTimeout(() => {
            console.error('[차트] 데이터 로드 타임아웃');
            setErrorType('general');
            setHasError(true);
            setIsLoading(false);
            isLoadingRef.current = false;
            setShowSpinner(false);
            pendingRequestRef.current = false;
        }, TIMEOUT_DURATION);
    };

    /*──────────────────────────── useEffect 초기화 ───────────────────────────*/
    useEffect(() => {
        // 초기화
        isUnmountingRef.current = false;
        loadedFromRef.current = null;
        loadedToRef.current = null;
        dataCacheRef.current = [];
        setCandleStickData([]);
        setIndicatorDataMap({});
        pendingRequestRef.current = false;
        pendingIndicatorRequestsRef.current = {};
        candleDataUpdatedRef.current = false;

        // 상태 초기화
        setIsLoading(true);
        setHasError(false);
        setIsCandleReady(false);
        setChartVisible(true);
        setIsChartInitialized(false); // 차트 초기화 상태 초기화

        requestFailCountRef.current = 0;
        window.paneCount = undefined;
        if (!chartContainerRef.current) return;

        const chart = createChart(chartContainerRef.current, {
            layout: {
                background: {color: "#111111"},
                textColor: "#ffffff",
                fontFamily: "'Inter', 'Pretendard', sans-serif",
                fontSize: 13,
                panes: {
                    separatorColor: "rgba(255, 215, 0, 0.3)",
                    separatorHoverColor: "transparent"
                }
            },
            grid: {
                vertLines: {visible: false},
                horzLines: {visible: false},
            },
            crosshair: {
                mode: CrosshairMode.Normal,
                vertLine: {
                    color: 'rgba(255, 215, 0, 0.5)',
                    width: 1,
                    style: LineStyle.Dashed,
                    labelVisible: false,
                    visible: true
                },
                horzLine: {
                    color: 'rgba(255, 215, 0, 0.5)',
                    width: 1,
                    style: LineStyle.Dashed,
                    labelVisible: false,
                    visible: true
                }
            },
            timeScale: {
                timeVisible: true,
                shiftVisibleRangeOnNewBar: false,
                ticksVisible: true,
                borderColor: "#ffffff"
            },
            handleScroll: {pressedMouseMove: true, mouseWheel: true},
            handleScale: {axisPressedMouseMove: true, mouseWheel: true},
            localization: {
                locale: 'ko-KR',
                priceFormatter: (price: number) => {
                    return price.toLocaleString(undefined, {
                        minimumFractionDigits: pricePrecision,
                        maximumFractionDigits: pricePrecision
                    });
                }
            },
        });

        chart.priceScale("right").applyOptions({mode: 1});
        chart.priceScale("right").applyOptions({
            ticksVisible: true,
            borderColor: "#ffffff"
        });

        chartRef.current = chart;

        // 초기 크기 설정
        if (chartContainerRef.current) {
            try {
                chart.applyOptions({
                    width: chartContainerRef.current.clientWidth,
                    height: chartContainerRef.current.clientHeight,
                });
            } catch (e) {
            }
        }

        const handleResize = () => {
            // offsetParent 확인하여 실제로 보이는지 검사
            if (isUnmountingRef.current || !chartRef.current || !chartContainerRef.current || chartContainerRef.current.offsetParent === null) return;

            // 컨테이너 크기가 유효할 때만 차트 크기 업데이트
            if (chartContainerRef.current.clientWidth > 0 && chartContainerRef.current.clientHeight > 0) {
                try {
                    chartRef.current.applyOptions({
                        width: chartContainerRef.current.clientWidth,
                        height: chartContainerRef.current.clientHeight,
                    });
                } catch (e) {
                }
            }
        };
        window.addEventListener("resize", handleResize);

        candleStickRendererRef.current = new CandleStickRenderer({
            chart,
            priceStep: priceStep,
            pricePrecision: pricePrecision
        });

        // 현재 심볼 정보를 전역 변수로 설정
        (window as any).symbol = symbol;

        /* WebSocket: Open */
        const onWsOpen = () => {
            loadChartData({initial: true});
        };

        /* WebSocket: Message */
        const onWsMessage = (event: MessageEvent) => {
            // 언마운트된 경우 처리 중단
            if (isUnmountingRef.current) return;

            // 추가: 차트 컨테이너가 실제로 보이는지 확인 (WebSocket 메시지 처리 전)
            const isChartVisible = chartContainerRef.current?.offsetParent !== null;

            if (!chartRef.current || !candleStickRendererRef.current) {
                pendingRequestRef.current = false; // 중요한 참조가 없으면 요청 상태 해제
                if (isLoadingRef.current) setIsLoading(false); // 로딩 중이었다면 해제
                return;
            }
            try {
                const msg = JSON.parse(event.data);
                const isInitialLoadResponse = dataCacheRef.current.length === 0; // 더 명확한 초기 로드 응답 감지

                if (msg.action === "loadChartDataResponse") {
                    // Calendar에서 온 date 요청인지 확인 (전역적으로 저장된 플래그 확인)
                    const isCalendarDateRequest = (window as any).lastCalendarDateRequest === true;
                    if (isCalendarDateRequest) {
                        (window as any).lastCalendarDateRequest = false; // 플래그 리셋
                    }

                    const candleData: CandleData[] = msg.candleData;
                    const indicatorResults: {
                        indicatorName: string,
                        data: IndicatorDataPoint[],
                        error?: string
                    }[] = msg.indicatorResults;

                    let candleDataReceived = false;
                    // 캔들 데이터 처리
                    if (Array.isArray(candleData) && candleData.length > 0) {
                        candleDataReceived = true;

                        if (isInitialLoadResponse || isCalendarDateRequest) {
                            if (isCalendarDateRequest) {
                                // 지표 데이터도 초기화
                                setIndicatorDataMap({});

                                // loadedFrom/To 범위도 리셋
                                loadedFromRef.current = null;
                                loadedToRef.current = null;
                            }
                            dataCacheRef.current = candleData;
                        } else {
                            // 시간 순 정렬된 데이터이므로 단순 concat으로 빠른 병합
                            dataCacheRef.current = dataCacheRef.current.concat(candleData);
                        }

                        if (dataCacheRef.current.length > 0) {
                            loadedFromRef.current = Math.floor(dataCacheRef.current[0].time);
                            loadedToRef.current = Math.floor(dataCacheRef.current[dataCacheRef.current.length - 1].time);
                        }

                        // 상태 업데이트하여 TimeSlider 리렌더링 트리거
                        setCandleStickData([...dataCacheRef.current]);

                        candleDataUpdatedRef.current = true; // 캔들 데이터 업데이트됨
                    } else if (isInitialLoadResponse && (!candleData || candleData.length === 0)) {
                        // 초기 로드인데 캔들 데이터가 없는 경우 (에러 상황 간주 가능)
                        console.error('[차트] 초기 로드에서 캔들스틱 데이터가 없습니다.');

                        // 타임아웃 해제
                        if (timeoutTimerRef.current) {
                            clearTimeout(timeoutTimerRef.current);
                            timeoutTimerRef.current = null;
                        }

                        setErrorType('candlestick');
                        setHasError(true);
                        setIsLoading(false);
                        isLoadingRef.current = false;
                        setShowSpinner(false);
                    }

                    // 지표 데이터 처리
                    let hasIndicatorError = false;

                    if (Array.isArray(indicatorResults)) {
                        // Calendar 요청인 경우 기존 지표 데이터 완전 초기화
                        const newIndicatorDataMap = isCalendarDateRequest ? {} : {...indicatorDataMap};

                        indicatorResults.forEach(result => {
                            const {indicatorName, data, error} = result;
                            if (error || !Array.isArray(data) || data.length === 0) {
                                hasIndicatorError = true;
                                if (pendingIndicatorRequestsRef.current[indicatorName]) {
                                    pendingIndicatorRequestsRef.current[indicatorName] = false;
                                }
                            } else {
                                // Calendar 요청인 경우 기존 데이터 병합하지 않고 새로 설정
                                newIndicatorDataMap[indicatorName] = data;

                                // TopInfo의 crosshairMove 이벤트에서 참조하는 window.indicatorData 업데이트
                                if (window.indicatorSeriesRefs && window.indicatorSeriesRefs[indicatorName] && isChartVisible) {
                                    try {
                                        const series = window.indicatorSeriesRefs[indicatorName];
                                        if (isCalendarDateRequest) {
                                            // Calendar 요청인 경우: reset 옵션으로 기존 데이터 초기화
                                            series.updateData(data, {reset: true});
                                        } else if (isInitialLoadResponse) {
                                            // 초기 로드인 경우도 reset 옵션 사용
                                            series.updateData(data, {reset: true});
                                        } else {
                                            // 기존 방식: 데이터 병합
                                            series.updateData(data);
                                        }
                                    } catch (e) {
                                        console.error(`[지표] ${indicatorName} 시리즈 updateData 오류:`, e);
                                    }
                                }
                                if (pendingIndicatorRequestsRef.current[indicatorName]) {
                                    pendingIndicatorRequestsRef.current[indicatorName] = false;
                                }
                            }
                        });

                        // 한 번에 상태 업데이트
                        setIndicatorDataMap(newIndicatorDataMap);
                    } else {
                        // 지표 결과가 없거나 배열이 아닌 경우도 에러로 처리
                        const firstCandle = dataCacheRef.current[0];
                        const lastCandle = dataCacheRef.current[dataCacheRef.current.length - 1];
                        const fromTime = firstCandle ? new Date(firstCandle.time * 1000).toISOString() : 'N/A';
                        const toTime = lastCandle ? new Date(lastCandle.time * 1000).toISOString() : 'N/A';
                        console.error('[차트][지표 데이터 없음] 심볼:', symbol, ', 타임프레임:', timeframe, ', 데이터범위:', fromTime, '~', toTime);
                        hasIndicatorError = true;
                    }

                    // 지표 에러가 있으면 에러 상태 설정 (초기 로드에서만)
                    if (hasIndicatorError && isInitialLoadResponse) {
                        console.error('[차트] 일부 지표 로딩에 실패했습니다.');

                        // 타임아웃 해제
                        if (timeoutTimerRef.current) {
                            clearTimeout(timeoutTimerRef.current);
                            timeoutTimerRef.current = null;
                        }

                        setErrorType('indicator');
                        setHasError(true);
                        setIsLoading(false);
                        isLoadingRef.current = false;
                        setShowSpinner(false);
                    }

                    // 요청 완료 후 상태 업데이트
                    pendingRequestRef.current = false;

                    // 타임아웃 해제
                    if (timeoutTimerRef.current) {
                        clearTimeout(timeoutTimerRef.current);
                        timeoutTimerRef.current = null;
                    }

                    if (isInitialLoadResponse || (!candleDataReceived && msg.candleData?.length === 0)) { // 초기로드 응답이거나, 스크롤 시 빈 데이터 응답이어도 로딩 해제
                        setIsLoading(false);
                        isLoadingRef.current = false;
                    }


                    // 모든 데이터 (캔들 + 지표) 수신 후 UI 업데이트 로직
                    if (isChartVisible) {
                        if ((isInitialLoadResponse || isCalendarDateRequest) && candleDataReceived) { // 초기 로드 또는 Calendar 요청이고 캔들 데이터가 실제로 왔을 때
                            setIsCandleReady(true);

                            // 지표 에러가 없을 때만 에러 상태 해제
                            if (!hasIndicatorError) {
                                setHasError(false); // 성공적으로 데이터 받았으므로 에러 상태 해제
                                setErrorType('general'); // 에러 타입도 초기화
                            }

                            if (chartRef.current && dataCacheRef.current.length > 0) {
                                // 캔들스틱 및 거래량 시리즈 생성, 데이터 설정
                                // Calendar 요청도 초기 로드처럼 처리
                                candleStickRendererRef.current.updateData(dataCacheRef.current);

                                if (window.paneCount && chartRef.current.panes().length > 0) {
                                    const height = calculateMainChartHeight(window.paneCount, window.innerHeight);
                                    chartRef.current.panes()[0].setHeight(height);
                                }

                                requestAnimationFrame(() => {
                                    if (!chartRef.current || !isChartVisible) return;
                                    chartRef.current.timeScale().fitContent();

                                    // 2단계 렌더링 보장
                                    setTimeout(() => {
                                        // 차트가 완전히 초기화된 후에 요청 허용
                                        setIsChartInitialized(true);
                                        setIsTimeSliderLocked(false); // 초기 로드 및 모든 처리 완료 후 슬라이더 잠금 해제

                                        // 첫 데이터 포인트로 이동하여 화면 중앙에 표시
                                        if (chartRef.current && dataCacheRef.current.length > 0 && isChartVisible) {
                                            // 첫 데이터 포인트의 인덱스는 0
                                            const firstPointIndex = 0;
                                            // 화면 너비의 절반 정도를 확보하여 첫 포인트가 화면 중앙에 오도록 설정
                                            const visibleBars = Math.floor((chartRef.current.timeScale().width() / 10) * 0.8); // 대략적인 화면에 표시되는 바 개수
                                            // 첫 포인트가 화면 중앙에 오도록 범위 설정
                                            chartRef.current.timeScale().setVisibleLogicalRange({
                                                from: firstPointIndex - Math.floor(visibleBars / 2),
                                                to: firstPointIndex + Math.floor(visibleBars / 2)
                                            });
                                        }

                                        if (candleStickRendererRef.current && isChartVisible) {
                                            // 마지막 업데이트 강제
                                            candleStickRendererRef.current.updateData([...dataCacheRef.current], {});
                                        }

                                        // Calendar 요청인 경우 완료 신호 전송
                                        if (isCalendarDateRequest) {
                                            setTimeout(() => {
                                                (window as any).calendarDataResetComplete = true;

                                                // 달력 이동 완료 후 targetTime 초기화
                                                calendarMoveCompletedRef.current = true;
                                                setTargetTime(undefined);
                                            }, 100);
                                        }
                                    }, 100);
                                });
                            }
                        } else if (!isInitialLoadResponse && candleDataUpdatedRef.current) { // 스크롤 응답 & 캔들 업데이트 필요시
                            // 볼륨 시리즈 및 메인 시리즈 모두 업데이트
                            if (candleStickRendererRef.current && chartRef.current && !isUnmountingRef.current) {
                                // 다른 방향 요청은 일반 업데이트
                                candleStickRendererRef.current.updateData(dataCacheRef.current);
                                setIsTimeSliderLocked(false); // 오른쪽 요청 및 기타 업데이트 후 슬라이더 잠금 해제
                            }
                            candleDataUpdatedRef.current = false; // 업데이트 완료
                        }
                    } else { // 차트가 보이지 않을 때
                        if (isInitialLoadResponse) {
                            setIsCandleReady(true);

                            // 지표 에러가 없을 때만 에러 상태 해제
                            if (!hasIndicatorError) {
                                setHasError(false);
                                setErrorType('general'); // 에러 타입도 초기화
                            }
                        }
                        candleDataUpdatedRef.current = false;
                        pendingIndicatorRequestsRef.current = {};
                    }
                } else if (msg.action === "error") {
                    console.error('[차트] 서버 에러:', msg.message || '알 수 없는 에러가 발생했습니다.');

                    // 타임아웃 해제
                    if (timeoutTimerRef.current) {
                        clearTimeout(timeoutTimerRef.current);
                        timeoutTimerRef.current = null;
                    }

                    setErrorType('general');
                    setHasError(true);
                    setIsLoading(false);
                    isLoadingRef.current = false;
                    pendingRequestRef.current = false;
                }
            } catch (error) {
                console.error('[클라이언트] WebSocket 메시지 처리 오류:', error);

                // 타임아웃 해제
                if (timeoutTimerRef.current) {
                    clearTimeout(timeoutTimerRef.current);
                    timeoutTimerRef.current = null;
                }

                setErrorType('general');
                setHasError(true);
                setIsLoading(false);
                isLoadingRef.current = false;
                pendingRequestRef.current = false;
                setShowSpinner(false); // 스피너도 숨김
            }
        };

        if (ws) {
            if (ws.readyState === WebSocket.OPEN) {
                loadChartData({initial: true});
            } else {
                ws.addEventListener("open", onWsOpen);
            }
            ws.addEventListener("message", onWsMessage);
            ws.addEventListener("error", () => {
                console.error('[차트] WebSocket 연결 에러');

                // 타임아웃 해제
                if (timeoutTimerRef.current) {
                    clearTimeout(timeoutTimerRef.current);
                    timeoutTimerRef.current = null;
                }

                setErrorType('general');
                setHasError(true);
                setIsLoading(false);
                isLoadingRef.current = false;
                setShowSpinner(false);
            });
            ws.addEventListener("close", () => {
                console.error('[차트] WebSocket 연결이 종료되었습니다');

                // 타임아웃 해제
                if (timeoutTimerRef.current) {
                    clearTimeout(timeoutTimerRef.current);
                    timeoutTimerRef.current = null;
                }

                setErrorType('general');
                setHasError(true);
                setIsLoading(false);
                isLoadingRef.current = false;
                setShowSpinner(false);
            });
        }

        return () => {
            isUnmountingRef.current = true;
            window.removeEventListener("resize", handleResize);
            if (ws) {
                ws.removeEventListener("open", onWsOpen);
                ws.removeEventListener("message", onWsMessage);
                ws.removeEventListener("error", () => {
                });
                ws.removeEventListener("close", () => {
                });
            }

            pendingRequestRef.current = false;
            pendingIndicatorRequestsRef.current = {};

            // 타임아웃 해제
            if (timeoutTimerRef.current) {
                clearTimeout(timeoutTimerRef.current);
                timeoutTimerRef.current = null;
            }

            // 언마운트 시 로딩 상태 재설정
            setIsLoading(false);
            setHasError(false);
            setErrorType('general');

            // 지표 시리즈 정리 (dispose만 호출하고, window.indicatorSeriesRefs 초기화는 IndicatorSeriesContainer에서 담당)
            if (window.indicatorSeriesRefs) {
                try {
                    Object.values(window.indicatorSeriesRefs).forEach(seriesRef => {
                        if (seriesRef && typeof seriesRef.dispose === 'function') {
                            seriesRef.dispose();
                        }
                    });
                } catch (e) {
                }
            }

            // 캔들스틱 렌더러 정리
            if (candleStickRendererRef.current) {
                try {
                    candleStickRendererRef.current.dispose();
                } catch (e) {
                }
                candleStickRendererRef.current = null;
            }

            // 차트 정리
            if (chartRef.current) {
                try {
                    chartRef.current.remove();
                } catch (e) {
                }
                chartRef.current = null;
            }

            // 상태 초기화
            setCandleStickData([]);
            setIndicatorDataMap({});
            setIsCandleReady(false);
            setChartVisible(true);
        };
    }, [symbol, timeframe, priceStep, pricePrecision, secondsPerBar, ws]);

    // isLoading 상태를 감시하는 useEffect에 추가
    useEffect(() => {
        // isLoadingRef를 항상 최신 상태로 유지
        isLoadingRef.current = isLoading;

        let timerId: NodeJS.Timeout | null = null;
        if (isLoading) {
            setShowSpinner(true);
        } else {
            timerId = setTimeout(() => {
                // 언마운트 확인
                if (isUnmountingRef.current) return;
                setShowSpinner(false);

                // 스피너 숨김 후 로딩 완료 콜백 호출
                if (onChartLoaded) {
                    onChartLoaded();
                }
            }, 500);
        }

        // 컴포넌트 언마운트 또는 isLoading 변경 시 타이머 정리
        return () => {
            if (timerId) {
                clearTimeout(timerId);
            }
        };
    }, [isLoading, onChartLoaded]);

    // 지표 로딩 완료 감지해서 isLoading 상태 업데이트
    useEffect(() => {
        const checkLoadingStatus = () => {
            const pendingIndicators = Object.keys(pendingIndicatorRequestsRef.current);
            const allIndicatorsReceived =
                pendingIndicators.length === 0 ||
                pendingIndicators.every(key => !pendingIndicatorRequestsRef.current[key]);

            if (allIndicatorsReceived && isCandleReady && isLoadingRef.current && !pendingRequestRef.current) {
                setIsLoading(false);
            }
        };

        // 간격을 500ms에서 1000ms로 늘림
        const intervalId = setInterval(checkLoadingStatus, 1000);

        return () => clearInterval(intervalId);
    }, [isCandleReady, isLoading]); // isLoading 의존성 추가

    useEffect(() => {
        if (chartVisible && candleStickRendererRef.current) {
            candleStickRendererRef.current.updateData([...dataCacheRef.current], {});
        }
    }, [chartVisible]);

    // 에러 상태 감지하여 NoDataMessage 표시 여부 결정
    useEffect(() => {
        if (hasError && (dataCacheRef.current.length === 0 || errorType === 'indicator')) {
            setShowNoDataMessage(true);
        } else {
            setShowNoDataMessage(false);
        }
    }, [hasError, errorType]);


    // 전역 차트 초기화 이벤트 및 데이터 로드 재시도 타이머 관리
    useEffect(() => {
        let initTimer: NodeJS.Timeout | null = null;

        // 초기화 완료 시점에 isChartInitialized를 설정하는 함수
        const completeInitialization = () => {
            setIsChartInitialized(true);
        };

        // 데이터 로드 후 일정 시간 지연 후 초기화 완료 설정
        if (isCandleReady && dataCacheRef.current.length > 0 && !isChartInitialized) {
            initTimer = setTimeout(completeInitialization, 1000);
        }

        return () => {
            if (initTimer) {
                clearTimeout(initTimer);
            }
        };
    }, [isCandleReady, isChartInitialized]);

    // 차트 오른쪽 경계 도달 시 추가 데이터 로드 이벤트 추가
    useEffect(() => {
        if (!chartRef.current || !isChartInitialized) {
            return;
        }

        // 스로틀링 구현을 위한 변수들
        let isThrottled = false;
        let pendingRangeCheck = false;
        const THROTTLE_TIME = 1000; // 1초 스로틀링

        // 오른쪽 경계 도달 감지 핸들러
        const handleVisibleRangeChange = () => {
            if (!chartRef.current || !isChartInitialized) {
                return;
            }

            if (pendingRequestRef.current || isLoadingRef.current) {
                return; // 이미 로딩 중이면 무시
            }

            // 스로틀링 적용 - 마지막 요청 후 일정 시간 동안 추가 요청 방지
            if (isThrottled) {
                pendingRangeCheck = true; // 스로틀링 중에 요청이 있었다고 표시
                return;
            }

            try {
                // 현재 보이는 logical range 가져오기
                const logicalRange = chartRef.current.timeScale().getVisibleLogicalRange();
                if (!logicalRange) {
                    return;
                }

                // 마지막 캔들 인덱스 (가장 최근 데이터)
                const lastCandleIndex = dataCacheRef.current.length - 1;

                // 화면의 오른쪽 끝 인덱스가 데이터의 마지막에 가까워졌는지 확인
                // 마지막 1000개 이내로 들어오면 추가 데이터 요청
                if (logicalRange.to >= lastCandleIndex - 1000) {
                    if (dataCacheRef.current.length > 0 && loadedToRef.current) {
                        // 마지막 캔들의 시간을 referenceTime으로 사용하여 추가 데이터 요청
                        const lastCandleTime = dataCacheRef.current[lastCandleIndex].time;

                        // 스로틀링 시작
                        isThrottled = true;
                        pendingRangeCheck = false;

                        // 요청 실행
                        loadChartData({referenceTime: lastCandleTime});

                        // 스로틀링 타이머 설정
                        setTimeout(() => {
                            isThrottled = false;

                            // 스로틀링 중에 요청이 있었다면 다시 체크
                            if (pendingRangeCheck) {
                                handleVisibleRangeChange();
                            }
                        }, THROTTLE_TIME);
                    } else {
                    }
                }
            } catch (error) {
                console.error('[차트] 오른쪽 경계 감지 오류:', error);
            }
        };

        // 이벤트 구독
        chartRef.current.timeScale().subscribeVisibleLogicalRangeChange(handleVisibleRangeChange);

        // 클린업 함수
        return () => {
            if (chartRef.current) {
                try {
                    chartRef.current.timeScale().unsubscribeVisibleLogicalRangeChange(handleVisibleRangeChange);
                } catch (error) {
                    console.error('[차트] 이벤트 해제 오류:', error);
                }
            }
        };
    }, [symbol, isChartInitialized]);

    // 에러가 있고 데이터가 없으면 NoDataMessage 표시
    if (showNoDataMessage) {
        let errorMessage: string;

        switch (errorType) {
            case 'candlestick':
                errorMessage = "캔들스틱 데이터를 불러올 수 없습니다.";
                break;
            case 'indicator':
                errorMessage = "지표 데이터를 불러올 수 없습니다.";
                break;
            case 'general':
            default:
                errorMessage = "차트 데이터를 불러올 수 없습니다.";
                break;
        }

        return <NoDataMessage message={errorMessage}/>;
    }

    return (
        <div style={{position: "relative", width: "100%", height: "100%", overflow: "hidden"}}>
            {/* 로딩 중일 때 스피너 표시 (showSpinner 상태 사용) */}
            {showSpinner && <LoadingSpinner/>}

            {/* 캘린더 로딩 중일 때 블러 처리된 오버레이와 스피너 */}
            {isCalendarLoading && (
                <div style={{
                    position: "fixed",
                    top: 0,
                    left: 0,
                    right: 0,
                    bottom: 0,
                    backdropFilter: "blur(4px)",
                    backgroundColor: "rgba(0, 0, 0, 0.3)",
                    display: "flex",
                    justifyContent: "center",
                    alignItems: "center",
                    zIndex: 10000,
                    isolation: "isolate",
                    transform: "translateZ(0)",
                    backfaceVisibility: "hidden",
                    WebkitFontSmoothing: "antialiased",
                    MozOsxFontSmoothing: "grayscale"
                }}>
                    <div style={{
                        width: "42px",
                        height: "42px",
                        borderRadius: "50%",
                        border: "3px solid rgba(20, 20, 20, 0.15)",
                        borderTopColor: "#FFD700",
                        animation: "spin 1s cubic-bezier(0.4, 0.1, 0.3, 1) infinite",
                        margin: "0 auto",
                        boxShadow: "0 0 20px rgba(255, 215, 0, 0.15)",
                        transform: "translateZ(0)",
                        willChange: "transform"
                    }}/>
                </div>
            )}

            <div
                ref={chartContainerRef}
                style={{
                    width: "100%",
                    height: "100%",
                    // showSpinner 상태에 따라 opacity 및 pointerEvents 조절
                    opacity: showSpinner ? 0 : 1,
                    pointerEvents: showSpinner ? "none" : "auto",
                    transition: "opacity 0.3s ease"
                }}
                onContextMenu={handleContextMenu}
            >
                {(() => {
                    return (
                        <>
                            {showCalendar && (
                                <Calendar
                                    chart={chartRef.current}
                                    candleStickData={dataCacheRef.current}
                                    onClose={() => setShowCalendar(false)}
                                    onDateSelected={moveToTimestamp}
                                    timeframe={timeframe}
                                    symbol={symbol}
                                    // 저장된 달력 상태 전달 (처음 열 때는 null 전달하여 첫 데이터 사용)
                                    lastSelectedDate={isCalendarFirstOpen ? null : calendarLastSelectedDate}
                                    lastSelectedTime={isCalendarFirstOpen ? '' : (calendarLastSelectedTime || '00:00')}
                                    // 달력에서 선택 시 상태 업데이트 콜백
                                    onDateTimeSelected={(date: Date, time: string) => {
                                        setCalendarLastSelectedDate(date);
                                        setCalendarLastSelectedTime(time);
                                        setIsCalendarFirstOpen(false); // 날짜 선택 후에는 첫 열기 아님으로 변경
                                    }}
                                    // 캘린더 로딩 상태 제어 콜백 추가
                                    onLoadingStart={handleCalendarLoadingStart}
                                    onLoadingEnd={handleCalendarLoadingEnd}
                                />
                            )}
                            <IndicatorSeriesContainer
                                key={symbol}
                                chart={chartRef.current}
                                indicatorDataMap={indicatorDataMap}
                                priceStep={priceStep}
                                pricePrecision={pricePrecision}
                            />
                            <TimeSlider
                                chart={chartRef.current}
                                candleStickData={candleStickData}
                                containerRef={chartContainerRef}
                                isLocked={isTimeSliderLocked}
                                targetTime={targetTime}
                            />
                            <TopInfo
                                symbol={symbol}
                                chart={chartRef.current}
                                candleStickData={candleStickData}
                                pricePrecision={pricePrecision}
                                containerRef={chartContainerRef}
                            />
                            <TimeAxisTooltip
                                chart={chartRef.current}
                                candleStickData={candleStickData}
                                containerRef={chartContainerRef}
                            />
                            <PriceAxisTooltip
                                chart={chartRef.current}
                                containerRef={chartContainerRef}
                                pricePrecision={pricePrecision}
                            />
                            <TradeMarkers
                                symbol={symbol}
                                chart={chartRef.current}
                                mainSeries={window.mainSeries}
                                candleStickData={candleStickData}
                                containerRef={chartContainerRef}
                                pricePrecision={pricePrecision}
                                config={config}
                                candleInterval={secondsPerBar}
                            />
                        </>
                    );
                })()}
            </div>
        </div>
    );
}

export default Chart;
