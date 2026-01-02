import React, {useCallback, useEffect, useRef, useState} from 'react';
import {useTradeFilter} from '@/Components/TradeFilter';
import LoadingSpinner from '@/Components/Common/LoadingSpinner';
import NoDataMessage from '@/Components/Common/NoDataMessage';
import Worker from '@/Workers/HoldingTimePnlDistribution.worker.ts?worker';

// Plotly 컴포넌트를 동적으로 로드하기 위한 타입 정의
interface PlotlyComponentProps {
    data: any[];
    layout: any;
    config?: any;
    style?: React.CSSProperties;
    useResizeHandler?: boolean;
    onRelayout?: (relayoutData: any) => void;
    onHover?: (event: any) => void;
    onUnhover?: (event: any) => void;
}

// Plotly 컴포넌트 또는 fallback 컴포넌트
const PlotComponent: React.FC<PlotlyComponentProps> = ({
                                                           data,
                                                           layout,
                                                           config,
                                                           style,
                                                           useResizeHandler,
                                                           onRelayout,
                                                           onHover,
                                                           onUnhover
                                                       }) => {
    const [PlotlyPlot, setPlotlyPlot] = useState<any>(null);
    const [loadingPlotly, setLoadingPlotly] = useState(true);
    const [error, setError] = useState<string | null>(null);

    useEffect(() => {
        const loadPlotly = async () => {
            try {
                // 앱에서 미리 로드된 Plotly 사용
                const preloadedPlotly = window.plotlyPreload;

                if (preloadedPlotly) {
                    const PlotComponent = await preloadedPlotly;
                    if (PlotComponent) {
                        setPlotlyPlot(() => PlotComponent);
                        setLoadingPlotly(false);

                        return;
                    }
                }

                // Fallback: 직접 import (미리 로드 실패 시)
                const plotlyModule = await import('react-plotly.js');
                const PlotComponent = plotlyModule.default;

                setPlotlyPlot(() => PlotComponent);
                setLoadingPlotly(false);
            } catch (err) {
                console.warn('react-plotly.js not found, using fallback', err);
                setError('Plotly.js를 로드할 수 없습니다. 패키지를 설치해주세요: npm install react-plotly.js plotly.js');
                setLoadingPlotly(false);
            }
        };

        loadPlotly().then();
    }, []);

    if (loadingPlotly) {
        return <LoadingSpinner/>;
    }

    if (error || !PlotlyPlot) {
        return (
            <div style={{
                ...style,
                display: 'flex',
                flexDirection: 'column',
                justifyContent: 'center',
                alignItems: 'center',
                color: 'white',
                padding: '20px',
                textAlign: 'center'
            }}>
                <p style={{marginBottom: '10px'}}>차트를 표시할 수 없습니다</p>
                <p style={{fontSize: '14px', opacity: 0.8}}>{error}</p>
            </div>
        );
    }

    // JSX 형태로 컴포넌트 렌더링
    return (
        <PlotlyPlot
            data={data}
            layout={layout}
            config={config}
            style={style}
            useResizeHandler={useResizeHandler}
            onRelayout={onRelayout}
            onHover={onHover}
            onUnhover={onUnhover}
        />
    );
};

const HoldingTimePnlDistribution = () => {
    const containerRef = useRef<HTMLDivElement>(null);
    const tooltipRef = useRef<HTMLDivElement | null>(null);
    const {filteredTrades} = useTradeFilter();
    const [plotData, setPlotData] = useState<any[]>([]);
    const [isLoading, setIsLoading] = useState(true);
    const [isMounted, setIsMounted] = useState(false); // For initial animation
    const [layout, setLayout] = useState<any>(null);
    // Plotly 렌더링 시점을 지연하여 초기 진입 애니메이션을 부드럽게 함
    const [shouldRenderPlot, setShouldRenderPlot] = useState(false);
    const workerRef = useRef<Worker | null>(null);

    // Worker를 설정하고 메시지를 처리합니다.
    useEffect(() => {
        const worker = new Worker();
        workerRef.current = worker;

        worker.onmessage = (event: MessageEvent<{ plotData: any[], layoutData: any | null }>) => {
            const {plotData: newPlotData, layoutData} = event.data;
            setPlotData(newPlotData);

            if (layoutData) {
                const {xAxisRange, tickvals, ticktext} = layoutData;
                setLayout({
                    dragmode: false,
                    xaxis: {
                        type: 'log',
                        autorange: false,
                        range: xAxisRange,
                        fixedrange: true,
                        title: {
                            text: '',
                            font: {family: 'Inter, Pretendard, sans-serif', size: 14, color: 'rgb(255, 255, 255)'}
                        },
                        tickvals: tickvals,
                        ticktext: ticktext,
                        tickfont: {family: 'Inter, Pretendard, sans-serif', size: 14, color: 'rgb(255, 255, 255)'},
                        showgrid: false,
                        showline: true,
                        linecolor: 'rgb(255, 255, 255)',
                        linewidth: 1,
                        ticks: 'outside',
                        tickcolor: 'rgb(255, 255, 255)',
                        ticklen: 4,
                        tickwidth: 1
                    },
                    yaxis: {
                        fixedrange: true,
                        title: {
                            text: '',
                            font: {family: 'Inter, Pretendard, sans-serif', size: 14, color: 'rgb(255, 255, 255)'}
                        },
                        automargin: true,
                        tickformat: '$,.0f',
                        tickfont: {family: 'Inter, Pretendard, sans-serif', size: 14, color: 'rgb(255, 255, 255)'},
                        showgrid: false,
                        zeroline: true,
                        zerolinecolor: 'rgb(255, 255, 255)',
                        zerolinewidth: 1,
                        showline: true,
                        linecolor: 'rgb(255, 255, 255)',
                        linewidth: 1,
                        ticks: 'outside',
                        tickcolor: 'rgb(255, 255, 255)',
                        ticklen: 4,
                        tickwidth: 1
                    },
                    plot_bgcolor: 'transparent',
                    paper_bgcolor: 'transparent',
                    font: {family: 'Inter, Pretendard, sans-serif', color: 'rgb(255, 255, 255)'},
                    showlegend: false,
                    hovermode: 'closest',
                    hoverlabel: {
                        bgcolor: 'rgba(28, 28, 36, 0.95)',
                        bordercolor: 'rgba(255, 215, 0, 0.4)',
                        font: {family: 'Inter, Pretendard, sans-serif', size: 14, color: '#ffffff'}
                    },
                    hoverdistance: 15,
                    margin: {l: 65, r: 40, t: 20, b: 40}
                });
            } else {
                setLayout(null);
            }
            setIsLoading(false);
        };

        // 컴포넌트가 언마운트될 때 Worker를 정리합니다.
        return () => {
            worker.terminate();
        };
    }, []); // 이 useEffect는 마운트 시 한 번만 실행됩니다.

    // filteredTrades가 변경될 때마다 Worker에 데이터를 전송합니다.
    useEffect(() => {
        if (workerRef.current) {
            setIsLoading(true);
            workerRef.current.postMessage({filteredTrades});
        }
    }, [filteredTrades]);


    // 차트 나타나는 애니메이션 트리거
    useEffect(() => {
        if (!isLoading) {
            const timer = setTimeout(() => {
                setIsMounted(true);
            }, 50); // 약간의 딜레이 후 애니메이션 시작
            return () => clearTimeout(timer);
        } else {
            // 로딩 중에는 애니메이션 및 Plotly 렌더링 초기화
            setIsMounted(false);
            setShouldRenderPlot(false);
        }
    }, [isLoading]);

    // isMounted 상태가 true가 된 뒤에 Plotly 컴포넌트를 렌더링하여 렉을 줄임
    useEffect(() => {
        if (isMounted) {
            const timer = setTimeout(() => setShouldRenderPlot(true), 10);
            return () => clearTimeout(timer);
        }
    }, [isMounted]);

    // 툴팁 DOM 생성 (EquityCurve 스타일)
    useEffect(() => {
        if (containerRef.current && !tooltipRef.current) {
            const tooltip = document.createElement('div');
            tooltip.style.position = 'absolute';
            tooltip.style.padding = '10px 15px 5px 15px';
            tooltip.style.boxSizing = 'border-box';
            tooltip.style.fontSize = '12.5px';
            tooltip.style.color = '#eee';
            tooltip.style.background = 'rgba(28, 28, 36, 0.95)';
            tooltip.style.borderRadius = '6px';
            tooltip.style.border = '1px solid rgba(255, 215, 0, 0.4)';
            tooltip.style.boxShadow = '0 4px 12px rgba(0, 0, 0, 0.6)';
            tooltip.style.pointerEvents = 'none';
            tooltip.style.zIndex = '50';
            tooltip.style.fontFamily = "'Inter', 'Pretendard', sans-serif";
            tooltip.style.lineHeight = '1.6';
            tooltip.style.opacity = '0';
            tooltip.style.visibility = 'hidden';
            tooltip.style.transform = 'translate3d(0, 0, 0) scale(0.95)';
            tooltip.style.left = '-9999px';
            tooltip.style.top = '-9999px';
            tooltip.style.whiteSpace = 'nowrap';
            tooltip.style.transition = 'opacity 0.25s ease-out, transform 0.35s cubic-bezier(0.22,1,0.36,1)';
            containerRef.current.appendChild(tooltip);
            tooltipRef.current = tooltip;
        }

        return () => {
            if (containerRef.current && tooltipRef.current) {
                try {
                    containerRef.current.removeChild(tooltipRef.current);
                } catch (error) {
                    console.warn('툴팁 요소 제거 중 오류 (무시됨):', error);
                }
                tooltipRef.current = null;
            }
        };
    }, [isLoading]);

    // Hover / Unhover handlers
    const handleHover = useCallback((hoverEvent: any) => {
        if (!tooltipRef.current || !containerRef.current || !layout) return;

        const point = hoverEvent.points && hoverEvent.points[0];
        if (!point || !point.customdata) return;

        const [tradeNum, holdingTimeStr, pnlValue] = point.customdata;

        const pnlNumber = Number(pnlValue);
        const pnlFormatted = pnlNumber >= 0
            ? `$${pnlNumber.toLocaleString(undefined, {minimumFractionDigits: 2, maximumFractionDigits: 2})}`
            : `-$${Math.abs(pnlNumber).toLocaleString(undefined, {
                minimumFractionDigits: 2,
                maximumFractionDigits: 2
            })}`;

        // Update tooltip content
        tooltipRef.current.innerHTML = `
          <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:4px; border-bottom:1px solid rgba(255, 215, 0, 0.3); padding-bottom:5px;">
             <strong style="color:#ffffff; font-size:15px; font-weight:600;">거래 번호 #${tradeNum}</strong>
          </div>
          <div style="margin-top:8px;">
            <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:4px;">
              <span style="color:#aaa; font-size:13px; padding:0 6px; margin-right:8px; position:relative; left:-6px;">보유 시간</span>
              <strong style="color:#ffffff; font-weight:600; font-size:14px; text-align:right;">${holdingTimeStr}</strong>
            </div>
            <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:2px;">
              <span style="color:#aaa; font-size:13px; padding:0 6px; margin-right:8px; position:relative; left:-6px;">순손익</span>
              <strong style="color:${pnlNumber >= 0 ? '#4caf50' : '#f23645'}; font-weight:600; font-size:14px; text-align:right;">${pnlFormatted}</strong>
            </div>
          </div>
        `;

        // Positioning with requestAnimationFrame
        requestAnimationFrame(() => {
            if (!tooltipRef.current || !containerRef.current || !layout) return;

            const tooltip = tooltipRef.current;
            const container = containerRef.current;
            const containerRect = container.getBoundingClientRect();
            const tooltipWidth = tooltip.offsetWidth;
            const tooltipHeight = tooltip.offsetHeight;

            let x = hoverEvent.event.clientX - containerRect.left + 15;
            let y = hoverEvent.event.clientY - containerRect.top + 15;

            const plotAreaLeft = layout.margin.l;
            const plotAreaRight = containerRect.width - layout.margin.r;
            const plotAreaTop = layout.margin.t;
            const plotAreaBottom = containerRect.height - layout.margin.b;

            if (x + tooltipWidth > plotAreaRight) {
                x = hoverEvent.event.clientX - containerRect.left - tooltipWidth - 15;
            }
            if (x < plotAreaLeft) {
                x = plotAreaLeft;
            }

            if (y + tooltipHeight > plotAreaBottom) {
                y = hoverEvent.event.clientY - containerRect.top - tooltipHeight - 15;
            }
            if (y < plotAreaTop) {
                y = plotAreaTop;
            }

            tooltip.style.left = `${x}px`;
            tooltip.style.top = `${y}px`;
            tooltip.style.visibility = 'visible';
            tooltip.style.opacity = '1';
            tooltip.style.transform = 'translate3d(0, -4px, 0) scale(1)';
        });
    }, [layout]);

    const handleUnhover = useCallback(() => {
        if (!tooltipRef.current) return;
        tooltipRef.current.style.opacity = '0';
        tooltipRef.current.style.visibility = 'hidden';
        tooltipRef.current.style.transform = 'translate3d(0, 0, 0) scale(0.9)';
    }, []);

    // 차트 인스턴스 등록
    useEffect(() => {
        if (containerRef.current) {
            if (typeof (window as any).registerHoldingTimePnlChartInstance === 'function') {
                (window as any).registerHoldingTimePnlChartInstance(containerRef.current);
            }
        }
    }, []);

    // Plotly 기본 hoverlabel 화살표(path) 숨기기용 CSS 주입
    useEffect(() => {
        const STYLE_ID = 'plotly-hide-arrow';
        if (!document.getElementById(STYLE_ID)) {
            const style = document.createElement('style');
            style.id = STYLE_ID;
            style.textContent = `
              .plot-container .hoverlayer .hovertext path {
                display: none !important;
              }
            `;
            document.head.appendChild(style);
        }

        return () => {
            // 컴포넌트 언마운트 시 스타일 제거
            const styleEl = document.getElementById(STYLE_ID);
            if (styleEl) {
                try {
                    document.head.removeChild(styleEl);
                } catch (error) {
                    console.warn('Plotly 스타일 요소 제거 중 오류 (무시됨):', error);
                }
            }
        };
    }, []);

    // 거래 데이터가 없으면 메시지만 표시
    if (!filteredTrades || filteredTrades.length === 1) {
        return (
            <div style={{
                width: '100%',
                height: '100%',
                minHeight: '400px',
                position: 'relative',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center'
            }}>
                <NoDataMessage message="거래 내역이 존재하지 않습니다."/>
            </div>
        );
    }

    if (isLoading) {
        return <LoadingSpinner/>;
    }

    return (
        <div ref={containerRef} style={{
            width: 'calc(100% + 60px)',
            height: '104%',
            position: 'relative',
            left: '-20px',
            bottom: '22px',
            opacity: isMounted ? 1 : 0,
            transform: isMounted ? 'scale(1)' : 'scale(0.98)',
            transition: 'opacity 0.6s ease-in-out, transform 0.6s ease-out'
        }}>
            {shouldRenderPlot && (
                <PlotComponent
                    data={plotData}
                    layout={layout}
                    onRelayout={undefined}
                    config={{
                        scrollZoom: false, // 휠 줌 비활성화
                        displayModeBar: false,
                        displaylogo: false,
                        modeBarButtonsToRemove: [
                            'select2d',
                            'lasso2d',
                            'autoScale2d',
                            'hoverClosestCartesian',
                            'hoverCompareCartesian',
                            'toggleSpikelines'
                        ],
                        responsive: true,
                        doubleClick: false // 더블클릭 비활성화
                    }}
                    style={{width: '100%', height: '100%'}}
                    useResizeHandler={true}
                    onHover={handleHover}
                    onUnhover={handleUnhover}
                />
            )}
        </div>
    );
};

export default HoldingTimePnlDistribution;
