import React, {useEffect, useRef, useState} from "react";
import {IChartApi} from "lightweight-charts";
import {useLogo} from "@/Contexts/LogoContext";
import "./TopInfo.css";

interface Candle {
    time: number | string;
    open: number;
    high: number;
    low: number;
    close: number;
    volume: number;
}

interface TopInfoProps {
    symbol: string;
    chart: IChartApi | null;
    candleStickData: Candle[];
    pricePrecision: number;
    containerRef?: React.RefObject<HTMLDivElement | null>;
}

const TopInfo: React.FC<TopInfoProps> = ({symbol, chart, candleStickData, pricePrecision, containerRef}) => {
    // 가격 및 지표 정보를 업데이트하기 위한 상태
    const [priceInfo, setPriceInfo] = useState<string>("");
    const [indicatorInfo, setIndicatorInfo] = useState<string>("");
    const prevPaneHeightsRef = useRef<number[]>([]);
    const volumePrecisionRef = useRef<number>(0);

    // 심볼별 설정 정보를 저장할 ref
    const symbolConfigRef = useRef<any>(null);
    // 지표 설정 정보를 저장할 ref
    const indicatorConfigsRef = useRef<any[]>([]);

    const {getLogoUrl} = useLogo();

    // 현재 심볼에 대한 로고 URL 직접 가져오기
    const currentLogoUrl = getLogoUrl(symbol);

    // config.json에서 설정 정보를 가져오는 함수
    const loadConfigData = async () => {
        try {
            const res = await fetch('/api/config');
            const config = await res.json();

            // 현재 심볼 설정 찾기
            const currentSymbolConfig = config['심볼'].find((s: any) => s['심볼 이름'] === symbol);
            symbolConfigRef.current = currentSymbolConfig;

            // 지표 설정 정보 저장
            indicatorConfigsRef.current = config['지표'] || [];

            // 볼륨 정밀도 설정
            if (currentSymbolConfig && currentSymbolConfig['거래소 정보'] &&
                currentSymbolConfig['거래소 정보']['수량 최소 단위'] !== undefined) {
                const minQtyStep = currentSymbolConfig['거래소 정보']['수량 최소 단위'];
                if (minQtyStep < 1) {
                    const minQtyStepStr = minQtyStep.toString();
                    const decimalStr = minQtyStepStr.split('.')[1];
                    volumePrecisionRef.current = decimalStr ? decimalStr.length : 0;
                } else {
                    volumePrecisionRef.current = 0;
                }
            }
        } catch (error) {
            console.error("설정 정보 로드 중 오류:", error);
        }
    };

    // 지표 포맷 및 정밀도에 따라 값을 포맷팅하는 함수
    const formatIndicatorValue = (value: number, indicatorName: string): string => {
        if (isNaN(value)) {
            return "∅";
        }

        // 지표 설정 찾기
        const indicatorConfig = indicatorConfigsRef.current.find((i: any) => i['지표 이름'] === indicatorName);
        if (!indicatorConfig || !indicatorConfig['플롯']) return value.toString();

        const format = indicatorConfig['플롯']['포맷'];
        let precision = indicatorConfig['플롯']['소수점 정밀도'];

        // 정밀도 결정
        let precisionValue = 0;
        if (precision === "기본값") {
            if (format === "없음") {
                // 심볼 가격 소수점 정밀도 사용
                precisionValue = symbolConfigRef.current && symbolConfigRef.current['거래소 정보'] &&
                symbolConfigRef.current['거래소 정보']['가격 소수점 정밀도'] !== undefined ?
                    symbolConfigRef.current['거래소 정보']['가격 소수점 정밀도'] : 0;
            } else if (format === "퍼센트" || format === "달러") {
                precisionValue = 2;
            } else if (format === "볼륨") {
                // 1000 미만은 수량 최소 단위 정밀도
                precisionValue = value < 1000 ? volumePrecisionRef.current : 2;
            }
        } else {
            // 특정 정밀도가 설정된 경우
            precisionValue = parseInt(precision, 10);
        }

        // 값 포맷팅
        let formattedValue: string;

        if (format === "없음") {
            formattedValue = value.toLocaleString(undefined, {
                minimumFractionDigits: precisionValue,
                maximumFractionDigits: precisionValue
            });
        } else if (format === "퍼센트") {
            formattedValue = `${value.toLocaleString(undefined, {
                minimumFractionDigits: precisionValue,
                maximumFractionDigits: precisionValue
            })}%`;
        } else if (format === "달러") {
            formattedValue = `${value >= 0 ? '$' : '-$'}${Math.abs(value).toLocaleString(undefined, {
                minimumFractionDigits: precisionValue,
                maximumFractionDigits: precisionValue
            })}`;
        } else if (format === "볼륨") {
            // 볼륨 포맷 (1000 이상일 때 K, M, B, T 표시)
            if (value >= 1000) {
                const units = ['', 'K', 'M', 'B', 'T'];
                const unitIndex = Math.floor(Math.log10(value) / 3);
                const unitValue = value / Math.pow(1000, unitIndex);

                // 단위가 있을 때는 소수점 2자리까지 표시
                formattedValue = `${unitValue.toFixed(2)}${units[unitIndex]}`;
            } else {
                // 단위가 없을 때는 원래 정밀도 유지
                formattedValue = value.toLocaleString(undefined, {
                    minimumFractionDigits: precisionValue,
                    maximumFractionDigits: precisionValue
                });
            }
        } else {
            // 기본값 (알 수 없는 포맷)
            formattedValue = value.toLocaleString(undefined, {
                minimumFractionDigits: precisionValue,
                maximumFractionDigits: precisionValue
            });
        }

        // 뒷자리 0 절삭 처리 제거
        return formattedValue;
    };

    // 수량 최소 단위를 기반으로 거래량의 precision을 계산하는 함수
    const getVolumePrecision = async (symbolName: string): Promise<number> => {
        // 심볼 관련 설정을 가져오기 위해 fetch 요청
        try {
            const res = await fetch('/api/config');
            const config = await res.json();

            // 해당 심볼의 설정 찾기
            const symbolConfig = config['심볼'].find((s: any) => s['심볼 이름'] === symbolName);

            // 수량 최소 단위 확인
            if (symbolConfig) {
                let minQtyStep = null;

                if (symbolConfig['거래소 정보'] && symbolConfig['거래소 정보']['수량 최소 단위'] !== undefined) {
                    minQtyStep = symbolConfig['거래소 정보']['수량 최소 단위'];
                }

                // 최소 단위가 있으면 소수점 자릿수 계산
                if (minQtyStep !== null) {
                    if (minQtyStep < 1) {
                        const minQtyStepStr = minQtyStep.toString();
                        const decimalStr = minQtyStepStr.split('.')[1];
                        const precision: number = decimalStr.length;

                        return decimalStr ? precision : 0;
                    }

                    return 0;  // 오류 발생 시 기본값 0 반환
                }
            }

            return 0;  // 오류 발생 시 기본값 0 반환
        } catch (error) {
            console.error("거래량 precision 계산 중 오류:", error);

            return 0;  // 오류 발생 시 기본값 0 반환
        }
    }

    // 심볼이 변경되면 설정 정보 로드 및 거래량 precision 업데이트
    useEffect(() => {
        const loadVolumePrecision = async () => {
            await loadConfigData();

            volumePrecisionRef.current = await getVolumePrecision(symbol);
        };

        loadVolumePrecision().then();
    }, [symbol]);

    // 특정 페인의 top 위치(픽셀)를 계산하는 헬퍼 함수
    // (pane 0부터 paneIdx-1까지의 높이를 누적)
    const getPaneTopOffset = (paneIdx: number): number => {
        if (!chart) {
            return 0;
        }

        const panes = chart.panes();
        let offset = 0;

        for (let i = 0; i < paneIdx; i++) {
            offset += panes[i].getHeight();
        }

        return offset;
    };

    // 각 페인의 실제 높이 총합을 구하여
    // paneIndicatorDivs[p] 위치를 재배치하는 함수
    const repositionPaneLabels = () => {
        if (!chart) return;
        const panes = chart.panes(); // IPaneApi[] 배열

        // pane 0은 메인 차트이므로 pane 1부터 배치
        for (let p = 1; p < panes.length; p++) {
            if (window.paneIndicatorDivs && window.paneIndicatorDivs[p]) {
                let offset = 0;

                // 0 ~ (p-1)까지 높이를 누적
                for (let i = 0; i < p; i++) {
                    offset += panes[i].getHeight();
                }

                // 해당 offset을 top 값으로 설정하여 정확한 위치에 배치
                window.paneIndicatorDivs[p].style.top = offset + "px";
            }
        }
    };

    // 페인 높이 변화 감지 및 재배치 이벤트 설정
    useEffect(() => {
        if (!chart) {
            return;
        }

        let rafId: number | null = null;

        const loop = () => {
            try {
                const panes = chart.panes();
                const currentHeights = panes.map((p) => p.getHeight());

                let changed = false;
                if (currentHeights.length !== prevPaneHeightsRef.current.length) {
                    changed = true;
                } else {
                    for (let i = 0; i < currentHeights.length; i++) {
                        if (currentHeights[i] !== prevPaneHeightsRef.current[i]) {
                            changed = true;
                            break;
                        }
                    }
                }

                if (changed) {
                    prevPaneHeightsRef.current = currentHeights;

                    repositionPaneLabels();
                }
            } catch (e) {
                // 안전을 위해 예외 무시
            }

            rafId = requestAnimationFrame(loop);
        };

        rafId = requestAnimationFrame(loop);

        return () => {
            if (rafId !== null) {
                cancelAnimationFrame(rafId);
            }
        };
    }, [chart]);

    // 차트의 crosshair 이벤트에 구독하여 현재 캔들 데이터를 기반으로 정보를 업데이트
    useEffect(() => {
        if (!chart) return;

        const onCrosshairMove = (param: any) => {
            // crosshair가 차트 영역 밖이면 정보 그대로 유지
            if (!param.time || !param.point) {
                return;
            }

            // 캔들 데이터가 비어있는지 확인
            if (candleStickData.length === 0) {
                return;
            }

            // crosshair의 time과 일치하는 캔들 인덱스 찾기 (없으면 마지막 캔들 사용)
            let currentIndex = candleStickData.findIndex(item => item.time === param.time);
            if (currentIndex === -1) {
                currentIndex = candleStickData.length - 1;
            }

            // 인덱스가 유효한지 확인
            if (currentIndex < 0 || !candleStickData[currentIndex]) {
                return;
            }

            const currentCandle = candleStickData[currentIndex];

            // 전 캔들 대비 등락률(%) 계산
            let percentChange = "";
            if (currentIndex > 0 && candleStickData[currentIndex - 1]) {
                const previousCandle = candleStickData[currentIndex - 1];
                percentChange = (((currentCandle.close / previousCandle.close) - 1) * 100).toFixed(2);
            }
            const percentString = percentChange ? ` (${percentChange}%)` : "";

            // 양봉이면 초록, 음봉이면 빨강
            const priceColor = currentCandle.close >= currentCandle.open ? "#4caf50" : "#f23645";

            // 숫자 포맷팅 함수 - 3자리마다 쉼표 추가
            const formatPrice = (value: number) => {
                return value.toLocaleString(undefined, {
                    minimumFractionDigits: pricePrecision,
                    maximumFractionDigits: pricePrecision
                });
            };

            // 거래량 포맷팅 함수 - config.json의 '수량 최소 단위' 소수점 자릿수 적용
            const formatVolume = (value: number) => {
                // 큰 수 단위 변환 (K, M, B, T)
                if (value >= 1000) {
                    const units = ['', 'K', 'M', 'B', 'T'];
                    const unitIndex = Math.floor(Math.log10(value) / 3);
                    const unitValue = value / Math.pow(1000, unitIndex);

                    // 단위가 있을 때는 소수점 2자리까지 표시하되, 소수점 뒤 0 제거 코드 제거
                    let formattedValue = unitValue.toFixed(2);
                    // 소수점 뒤 0 제거 코드 제거

                    return `${formattedValue}${units[unitIndex]}`;
                } else {
                    // 단위가 없을 때는 원래 정밀도 유지
                    return value.toLocaleString(undefined, {
                        minimumFractionDigits: volumePrecisionRef.current,
                        maximumFractionDigits: volumePrecisionRef.current
                    });
                }
            };

            // 가격 정보를 구성 (심볼 오른쪽에 수평으로 표시)
            const priceHtml =
                `
                <span class="price-group">
                  <span class="price-label">시가</span>
                  <span class="price-value" style="color: ${priceColor}">${formatPrice(currentCandle.open)}</span>
                </span>
                <span class="price-group">
                  <span class="price-label">고가</span>
                  <span class="price-value" style="color: ${priceColor}">${formatPrice(currentCandle.high)}</span>
                </span>
                <span class="price-group">
                  <span class="price-label">저가</span>
                  <span class="price-value" style="color: ${priceColor}">${formatPrice(currentCandle.low)}</span>
                </span>
                <span class="price-group">
                  <span class="price-label">종가</span>
                  <span class="price-value" style="color: ${priceColor}">${formatPrice(currentCandle.close)}${percentString}</span>
                </span>
                <span class="price-group">
                  <span class="price-label">거래량</span>
                  <span class="price-value" style="color: ${priceColor}">${formatVolume(currentCandle.volume)}</span>
                </span>
                `;
            setPriceInfo(priceHtml);

            // ------------------------------------------------------
            // 지표 데이터 업데이트
            // ------------------------------------------------------
            // indicatorSeriesInfo: 시리즈별 정보(라인 색, 지표 이름 등)
            // indicatorData: 실제 시리즈별 (time, value, color) 배열
            // indicatorInfoPerPane: pane별 HTML 텍스트를 임시로 저장할 객체

            const indicatorInfoPerPane: { [key: number]: string[] } = {};

            if (window.indicatorSeriesInfo && window.indicatorData) {
                for (const seriesId in window.indicatorSeriesInfo) {
                    const info = window.indicatorSeriesInfo[seriesId];

                    // 수정: 시리즈 참조를 통해 최신 데이터 확인
                    let dataArr = window.indicatorData[seriesId];

                    // 이 부분 추가: window.indicatorSeriesRefs에서 데이터 확인 시도
                    if (window.indicatorSeriesRefs && window.indicatorSeriesRefs[seriesId] &&
                        window.indicatorSeriesRefs[seriesId].getDataCache) {
                        try {
                            const seriesDataCache = window.indicatorSeriesRefs[seriesId].getDataCache();
                            if (seriesDataCache && seriesDataCache.length > 0) {
                                // 시리즈 캐시 데이터가 있으면 그것을 우선 사용
                                dataArr = seriesDataCache;
                            }
                        } catch (e) {
                            console.error(`[TopInfo] ${seriesId} 캐시 데이터 가져오기 실패:`, e);
                        }
                    }

                    if (!dataArr) continue;

                    let value: string | number = "∅";
                    let valueColor = "#ffffff"; // 기본 흰색

                    // 현재 커서의 time에 해당하는 데이터를 찾음
                    for (let i = 0; i < dataArr.length; i++) {
                        if (dataArr[i].time == param.time) {
                            const indicatorValue = dataArr[i].value;

                            if (indicatorValue !== null) {
                                value = indicatorValue;
                            }

                            // 시리즈 유형별(Area, Baseline, Histogram, Line)로 색상 결정
                            switch (info.seriesType) {
                                case "Area":
                                    // Area: lineColor(또는 기본 흰색)
                                    valueColor = info.lineColor || "#ffffff";
                                    break;

                                case "Baseline":
                                    // Baseline: value가 baseValue보다 크면 topLineColor, 작으면 bottomLineColor
                                    if (typeof value === "number" && !isNaN(value) && info.baseValue !== undefined) {
                                        if (value > info.baseValue) {
                                            valueColor = info.topLineColor || "#ffffff";
                                        } else {
                                            valueColor = info.bottomLineColor || "#ffffff";
                                        }
                                    }
                                    break;

                                case "Histogram":
                                    // Histogram: 각 bar 데이터의 color
                                    valueColor = dataArr[i].color || "#ffffff";
                                    break;

                                case "Line":
                                    // Line: 설정된 lineColor
                                    valueColor = info.lineColor || "#ffffff";
                                    break;
                            }
                            break;
                        }
                    }

                    // 지표 값 포맷팅
                    let formattedValue = typeof value === "number" && !isNaN(value) ?
                        formatIndicatorValue(value, info.name) : value;

                    // pane 구분 (0 => 메인 차트, 1 이상 => 별도 페인)
                    if (!indicatorInfoPerPane[info.pane]) {
                        indicatorInfoPerPane[info.pane] = [];
                    }

                    // HTML로 감싸서 색상 적용
                    const lineHtml = `
                        ${info.name}&nbsp;&nbsp;<span
                            style="
                            color: ${valueColor};
                            position: relative;
                            display: inline-block;
                            top: 1px;
                            margin-top: 1px;">
                        ${formattedValue}</span>
                    `;

                    indicatorInfoPerPane[info.pane].push(lineHtml);
                }
            }

            // 메인 차트(오버레이) 지표: pane 0에 표시
            if (indicatorInfoPerPane[0]) {
                const mainIndicatorHTML = indicatorInfoPerPane[0]
                    .map((line) => `<div style="margin-bottom:4px;">${line}</div>`)
                    .join("");
                setIndicatorInfo(mainIndicatorHTML);
            }

            // ------------------------------------------------------
            // 추가 페인(1 이상) 지표 정보 표시
            // ------------------------------------------------------
            window.paneIndicatorDivs = window.paneIndicatorDivs || {};

            if (chart) {
                const paneCount = chart.panes().length - 1; // 메인 차트 제외한 페인 개수

                // 추가 페인 정보 업데이트
                for (let pane = 1; pane <= paneCount; pane++) {
                    // 아직 해당 pane용 DIV가 없다면 생성
                    if (!window.paneIndicatorDivs[pane]) {
                        const div = document.createElement("div");
                        div.id = "indicatorInfo_pane_" + pane;
                        div.style.position = "absolute";
                        div.style.left = "16px";
                        div.style.top = getPaneTopOffset(pane) + "px"; // 실제 높이를 기반으로 계산
                        div.style.marginTop = "8px";
                        div.style.zIndex = "1000";
                        div.style.background = "rgba(17,17,17,0)";
                        div.style.color = "white";
                        div.style.padding = "8px 12px";
                        div.style.borderRadius = "4px";
                        div.style.fontFamily = "'Inter', 'Pretendard', sans-serif";
                        div.style.lineHeight = "1.4";
                        div.style.fontSize = "13px";
                        div.style.pointerEvents = "none";

                        // containerRef가 제공된 경우 사용, 없으면 document.body에 추가
                        const container = containerRef?.current || document.body;
                        container.appendChild(div);

                        window.paneIndicatorDivs[pane] = div;
                    }

                    // indicatorInfoPerPane[pane]에 데이터가 있으면 표시, 없으면 빈 문자열
                    if (window.paneIndicatorDivs[pane]) {
                        window.paneIndicatorDivs[pane].innerHTML = indicatorInfoPerPane[pane]
                            ? indicatorInfoPerPane[pane]
                                .map((line) => `<div style="margin-bottom:2px;">${line}</div>`)
                                .join("")
                            : "";
                    }
                }

                // 페인 위치 재조정
                repositionPaneLabels();
            }
        };

        chart.subscribeCrosshairMove(onCrosshairMove);
        return () => {
            chart.unsubscribeCrosshairMove(onCrosshairMove);

            // 페인 인디케이터 DIV 정리
            Object.values(window.paneIndicatorDivs || {}).forEach(div => {
                if (div instanceof HTMLElement && div.parentNode) {
                    div.parentNode.removeChild(div);
                }
            });
            window.paneIndicatorDivs = {};
        };
    }, [chart, candleStickData, pricePrecision, containerRef]);

    return (
        <div className="top-info">
            <div className="top-info-row">
                <div className="symbol-container">
                    <img
                        className="symbol-icon"
                        src={currentLogoUrl}
                        alt={symbol}
                        style={{
                            borderRadius: '50%',
                            objectFit: 'cover',
                        }}
                    />
                    <span className="symbol-text">{symbol}</span>
                </div>
                <div
                    id="candleData"
                    className="price-data"
                    dangerouslySetInnerHTML={{__html: priceInfo}}
                ></div>
            </div>
            <div
                id="mainIndicatorInfo"
                className="indicator-info"
                dangerouslySetInnerHTML={{__html: indicatorInfo}}
            ></div>
        </div>
    );
};

export default TopInfo;
