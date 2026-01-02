import React, {ReactNode, useCallback, useEffect, useMemo, useRef, useState} from 'react';
import {useTradeFilter} from '@/Components/TradeFilter';
import LoadingSpinner from '@/Components/Common/LoadingSpinner';
import NoDataMessage from '@/Components/Common/NoDataMessage';
import {Bar, BarChart, ReferenceLine, ResponsiveContainer, Tooltip, XAxis, YAxis} from 'recharts';
import {Box, FormControl, InputLabel, MenuItem, Select, SelectChangeEvent} from '@mui/material';

// 헬퍼 함수: 텍스트 너비 측정
const measureTextWidth = (text: string, font: string): number => {
    const canvas = document.createElement("canvas");
    const context = canvas.getContext("2d");
    if (!context) return 0;
    context.font = font;
    const metrics = context.measureText(text);
    return metrics.width;
};

// 커스텀 툴팁 컴포넌트
const CustomTooltip = ({active, payload, label}: any) => {
    if (active && payload && payload.length) {
        const data = payload[0].payload; // pnl과 fill 포함된 데이터 객체
        const value = data.pnl; // 순손익 값
        const valueColor = data.fill; // 막대 색상 (수익에 따라 결정됨)

        // 숫자 포맷팅
        const formattedValue = typeof value === 'number'
            ? (value >= 0 ? `$${value.toLocaleString(undefined, {minimumFractionDigits: 2, maximumFractionDigits: 2})}`
                : `-$${Math.abs(value).toLocaleString(undefined, {
                    minimumFractionDigits: 2,
                    maximumFractionDigits: 2
                })}`)
            : value;

        return (
            <div style={{
                backgroundColor: 'rgba(28, 28, 36, 0.95)',
                border: '1px solid rgba(255, 215, 0, 0.4)',
                borderRadius: '4px',
                padding: '8px 12px',
                boxShadow: '0 2px 5px rgba(0,0,0,0.3)',
                color: 'rgb(255,255,255)', // 기본 텍스트 색상
                fontSize: '14px', // 기본 폰트 크기
                fontFamily: "'Inter', 'Pretendard', sans-serif"
            }}>
                {/* 레이블(단위) 스타일 */}
                <p style={{
                    margin: '0 0 5px 0',
                    color: 'rgb(255,255,255)',
                    fontSize: '15px',
                    fontWeight: '600',
                    borderBottom: '1px solid rgba(255, 215, 0, 0.3)', // 구분선 스타일 유지 (선택적)
                    paddingBottom: '5px' // 구분선 아래 여백
                }}>{label}</p>

                {/* 순손익 값 표시 */}
                <div style={{
                    display: 'flex',
                    justifyContent: 'space-between',
                    alignItems: 'baseline',
                    marginTop: '8px', // 위 구분선과의 간격
                    marginBottom: '2px' // 아래 여백 추가
                }}>
                    <span style={{
                        color: 'rgb(255,255,255)',
                        fontSize: '13px',
                        paddingRight: '8px' // 오른쪽 여백
                    }}>순손익 합계</span>
                    <strong style={{
                        color: valueColor, // 수익에 따른 색상
                        fontWeight: 600,
                        fontSize: '14px'
                    }}>{formattedValue}</strong>
                </div>
            </div>
        );
    }

    return null;
};

// 시간 기준 타입 정의
type TimeReference = '진입 시간' | '청산 시간';

// 시간 단위 타입 정의
type TimePeriod = '연도별' | '월별' | '일별' | '요일별' | '시간별' | '분별' | '초별';

// 차트 데이터 형식
type ChartData = {
    name: string;
    pnl: number;
};

// 요일 이름 매핑 (월요일 시작)
const dayOfWeekNames = ['월요일', '화요일', '수요일', '목요일', '금요일', '토요일', '일요일'];

// 월 이름 매핑
const monthNames = ['1월', '2월', '3월', '4월', '5월', '6월', '7월', '8월', '9월', '10월', '11월', '12월'];

// 색상 정의
const positiveColor = '#4caf50'; // EquityCurve와 동일한 초록색
const negativeColor = '#f23645'; // EquityCurve와 동일한 빨간색

// Profit/Loss Comparison Plot Component
const NetProfitLossComparisonPlot = React.memo(({
                                                    timeReference,
                                                    timePeriod
                                                }: {
    timeReference: TimeReference,
    timePeriod: TimePeriod
}): ReactNode => {
    const {filteredTrades} = useTradeFilter();
    const [chartData, setChartData] = useState<ChartData[]>([]);
    const [yAxisWidth, setYAxisWidth] = useState(60); // Y축 너비 상태 추가 (기본값 60)
    const renderCountRef = useRef(0);

    // Y축 틱 포맷터 정의 (재사용 위해)
    const yTickFormatter = useCallback((value: any) => {
        if (typeof value === 'number') {
            const roundedValue = Math.round(value); // 정수로 반올림
            return roundedValue >= 0 ? `$${roundedValue.toLocaleString()}` : `-$${Math.abs(roundedValue).toLocaleString()}`;
        }
        return value;
    }, []);

    // 선택된 기준과 시간 단위에 따라 데이터 처리 로직 구현
    useEffect(() => {
        renderCountRef.current += 1;

        if (!filteredTrades || filteredTrades.length === 1) { // 로딩 중이거나 데이터가 없을 때 빈 배열 설정
            setChartData([]);
            return;
        }

        // 거래 번호가 0인 거래 제외
        const validTrades = filteredTrades.filter(trade => trade['거래 번호'] !== 0);
        const timeField = timeReference === '진입 시간' ? '진입 시간' : '청산 시간';
        const profitField = '순손익';

        // 데이터 그룹화 및 집계
        const groupedData: Record<string, number> = {};

        validTrades.forEach(trade => {
            if (!trade[timeField] || typeof trade[timeField] !== 'string') return;

            const date = new Date(trade[timeField] as string);
            if (isNaN(date.getTime())) return;

            let key: string;

            // 시간 단위별 분류
            switch (timePeriod) {
                case '초별':
                    key = date.getSeconds().toString().padStart(2, '0') + '초';
                    break;
                case '분별':
                    key = date.getMinutes().toString().padStart(2, '0') + '분';
                    break;
                case '시간별':
                    key = date.getHours().toString().padStart(2, '0') + '시';
                    break;
                case '요일별':
                    // 월요일=0, 화요일=1, ..., 일요일=6 으로 인덱스 조정
                    key = dayOfWeekNames[(date.getDay() + 6) % 7];
                    break;
                case '일별':
                    // 키 생성 시 padStart 추가하여 internalKeys와 형식 일치
                    key = date.getDate().toString().padStart(2, '0') + '일';
                    break;
                case '월별':
                    key = monthNames[date.getMonth()];
                    break;
                case '연도별':
                    key = date.getFullYear().toString() + '년';
                    break;
                default:
                    key = '알 수 없음';
            }

            // 수익 합산
            const profit = Number(trade[profitField]) || 0;
            groupedData[key] = (groupedData[key] || 0) + profit;
        });

        // 모든 가능한 X축 항목 생성
        let allKeys: string[];
        let internalKeys: string[] = []; // 정렬 및 데이터 매핑용 내부 키
        switch (timePeriod) {
            case '초별':
                internalKeys = Array.from({length: 60}, (_, i) => i.toString().padStart(2, '0') + '초');
                allKeys = Array.from({length: 60}, (_, i) => i.toString() + '초');
                break;
            case '분별':
                internalKeys = Array.from({length: 60}, (_, i) => i.toString().padStart(2, '0') + '분');
                allKeys = Array.from({length: 60}, (_, i) => i.toString() + '분');
                break;
            case '시간별':
                internalKeys = Array.from({length: 24}, (_, i) => i.toString().padStart(2, '0') + '시');
                allKeys = Array.from({length: 24}, (_, i) => i.toString() + '시');
                break;
            case '요일별':
                internalKeys = dayOfWeekNames; // 요일은 내부 키와 표시 키 동일
                allKeys = dayOfWeekNames;
                break;
            case '일별':
                internalKeys = Array.from({length: 31}, (_, i) => (i + 1).toString().padStart(2, '0') + '일');
                allKeys = Array.from({length: 31}, (_, i) => (i + 1).toString() + '일');
                break;
            case '월별':
                internalKeys = monthNames; // 월도 내부 키와 표시 키 동일
                allKeys = monthNames;
                break;
            case '연도별':
                // 데이터에 있는 연도 범위를 찾아서 생성
                const yearsInData = Object.keys(groupedData)
                    .map(key => parseInt(key.replace('년', '')))
                    .filter(year => !isNaN(year));
                if (yearsInData.length > 0) {
                    const minYear = Math.min(...yearsInData);
                    const maxYear = Math.max(...yearsInData);
                    internalKeys = Array.from({length: maxYear - minYear + 1}, (_, i) => (minYear + i).toString() + '년');
                    allKeys = internalKeys; // 연도도 동일
                } else {
                    internalKeys = [];
                    allKeys = [];
                }
                break;
            default:
                internalKeys = Object.keys(groupedData); // 기본값은 데이터 있는 키만
                allKeys = internalKeys;
        }

        // 차트 데이터 형식으로 변환 (모든 키 포함, 없는 값은 0)
        let formattedData = allKeys.map((displayName, index) => {
            const internalKey = internalKeys[index]; // 실제 데이터 매핑에 사용할 키
            const profit = groupedData[internalKey] || 0;
            return {
                name: displayName, // 표시용 이름 (0 없음)
                pnl: profit,
                fill: profit >= 0 ? positiveColor : negativeColor // 수익에 따라 색상 지정
            };
        });

        // 특정 시간 단위별로 정렬 (internalKeys를 기준으로 정렬)
        if (timePeriod === '연도별') {
            // 연도별은 이미 internalKeys 생성 시 정렬됨
        } else if (timePeriod === '요일별') {
            // 요일별은 dayOfWeekNames 순서대로
            formattedData = formattedData.sort((a, b) => {
                return dayOfWeekNames.indexOf(a.name) - dayOfWeekNames.indexOf(b.name);
            });
        } else if (timePeriod === '월별') {
            // 월별은 monthNames 순서대로
            formattedData = formattedData.sort((a, b) => {
                return monthNames.indexOf(a.name) - monthNames.indexOf(b.name);
            });
        } else {
            // 다른 숫자 기반 형식은 숫자 값으로 정렬
            formattedData = formattedData.sort((a, b) => {
                const numA = parseInt(a.name.match(/\d+/)?.[0] || '0');
                const numB = parseInt(b.name.match(/\d+/)?.[0] || '0');
                return numA - numB;
            });
        }

        setChartData(formattedData);
    }, [filteredTrades, timeReference, timePeriod]);

    // Y축 도메인 계산 (상하 10% 마진)
    const yDomain = useMemo(() => {
        if (chartData.length === 0) return ['auto', 'auto'];

        const values = chartData.map(d => d.pnl);
        let minVal = Math.min(...values);
        let maxVal = Math.max(...values);

        // 모든 값이 0일 경우 처리
        if (minVal === 0 && maxVal === 0) {
            return [-1, 1]; // 임의의 작은 범위 설정
        }

        const range = maxVal - minVal;
        const margin = range === 0 ? 1 : range * 0.1; // range가 0일 때 기본 마진 추가

        // 0 기준선이 항상 중앙 부근에 오도록 조정
        const absMax = Math.max(Math.abs(minVal), Math.abs(maxVal));
        const newMin = -absMax - margin;
        const newMax = absMax + margin;

        return [newMin, newMax];
    }, [chartData]);

    // Y축 틱 계산 로직
    const yTicks = useMemo(() => {
        // yDomain이 숫자로 된 배열인지 확인
        if (!Array.isArray(yDomain) || typeof yDomain[0] !== 'number' || typeof yDomain[1] !== 'number') {
            return undefined; // 자동 계산 사용 또는 유효한 범위 아니면 계산 불가
        }

        const [minVal, maxVal] = yDomain as [number, number]; // 타입 단언
        if (minVal === maxVal) return undefined; // 범위 없으면 계산 불가

        const range = maxVal - minVal;
        if (range <= 0) return undefined;

        const targetTickCount = 6; // 원하는 대략적인 틱 개수
        let minInterval = range / targetTickCount;
        // 간격이 0 또는 음수가 되지 않도록 보장
        minInterval = Math.max(minInterval, 1e-9);

        // "보기 좋은" 간격 찾기 (1, 2, 5의 배수)
        const magnitude = Math.pow(10, Math.floor(Math.log10(minInterval)));
        const residual = minInterval / magnitude;
        let interval;
        if (residual > 5) {
            interval = 10 * magnitude;
        } else if (residual > 2) {
            interval = 5 * magnitude;
        } else if (residual > 1) {
            interval = 2 * magnitude;
        } else {
            interval = magnitude;
        }

        // 간격이 너무 작거나 크면 조정 (예: 최소 간격 1)
        interval = Math.max(1, interval);

        const firstTick = Math.ceil(minVal / interval) * interval;
        const lastTick = Math.floor(maxVal / interval) * interval;

        const ticks: number[] = [];
        // 무한 루프 방지용 안전 장치
        let currentTick = firstTick;
        let count = 0;
        const maxCount = 100;

        while (currentTick <= lastTick && count < maxCount) {
            // 부동소수점 오류 방지 위해 반올림
            ticks.push(Math.round(currentTick * 1e6) / 1e6);
            currentTick += interval;
            count++;
        }

        // 틱 개수가 너무 많거나 적으면 조정 (여기선 단순화)
        if (ticks.length === 0 && minVal <= 0 && maxVal >= 0) {
            return [0]; // 범위가 매우 작고 0을 포함하면 0만 표시
        }
        if (ticks.length === 1 && ticks[0] !== 0 && minVal <= 0 && maxVal >= 0) {
            ticks.push(0); // 틱이 하나인데 0을 포함하면 0 추가
            ticks.sort((a, b) => a - b);
        }
        if (ticks.length === 0 && (minVal !== 0 || maxVal !== 0)) {
            return [Math.round(minVal), Math.round(maxVal)]; // 틱 계산 안되면 최소/최대값 사용
        }

        return ticks;
    }, [yDomain]);

    // Y축 너비 계산 로직 (yTicks를 사용하도록 수정) - 로그 추가
    useEffect(() => {
        const calculateYAxisWidth = () => {
            let ticksToMeasure = yTicks;
            // yTicks 계산 실패 시 domain 값 사용 (타입 체크 추가)
            if (!ticksToMeasure || ticksToMeasure.length === 0) {
                if (Array.isArray(yDomain) && typeof yDomain[0] === 'number' && typeof yDomain[1] === 'number') {
                    ticksToMeasure = [yDomain[0], yDomain[1]];
                } else {
                    setYAxisWidth(60); // 기본값
                    return;
                }
            }

            const font = "14px 'Inter', 'Pretendard', sans-serif"; // Y축 틱 폰트 스타일
            let maxWidth = 40; // 최소 너비

            ticksToMeasure.forEach(tick => {
                const label = yTickFormatter(tick);
                const width = measureTextWidth(label, font);
                if (width > maxWidth) {
                    maxWidth = width;
                }
            });

            setYAxisWidth(Math.ceil(maxWidth) + 15); // 계산된 너비 + 좌우 여백
        };

        calculateYAxisWidth();
    }, [yDomain, yTicks, yTickFormatter]);

    return (
        <div style={{width: '100%', height: '100%'}}>
            <ResponsiveContainer width="100%" height="100%"
                                 ref={(container) => {
                                     // 차트 컨테이너를 전역 객체에 등록
                                     if (container && typeof (window as any).registerProfitLossChartInstance === 'function') {
                                         (window as any).registerProfitLossChartInstance(container);
                                     }
                                 }}
            >
                <BarChart
                    data={chartData}
                    margin={{top: 0, right: 2, left: 2, bottom: 2}} // left 마진 줄이기 (YAxis width가 공간 확보)
                >
                    <XAxis
                        dataKey="name"
                        stroke="rgb(255,255,255)"
                        strokeWidth={1}
                        tick={{
                            fill: 'rgb(255,255,255)',
                            fontSize: 14,
                            fontFamily: "'Inter', 'Pretendard', sans-serif"
                        }}
                        tickLine={{stroke: 'rgb(255,255,255)', shapeRendering: 'crispEdges'}}
                        tickMargin={5} // 틱과 라벨 사이 간격 추가
                        shapeRendering="crispEdges"
                    />
                    <YAxis
                        stroke="rgb(255,255,255)"
                        strokeWidth={1}
                        tick={{
                            fill: 'rgb(255,255,255)',
                            fontSize: 14,
                            fontFamily: "'Inter', 'Pretendard', sans-serif"
                        }}
                        tickLine={{stroke: 'rgb(255,255,255)', shapeRendering: 'crispEdges'}}
                        tickMargin={5} // 틱과 라벨 사이 간격 추가
                        tickFormatter={yTickFormatter} // 정의된 포맷터 사용
                        domain={yDomain}
                        width={yAxisWidth} // 계산된 너비 적용
                        allowDecimals={false} // 소수점 눈금 비활성화
                        ticks={yTicks} // 계산된 틱 적용
                        shapeRendering="crispEdges"
                    />
                    <ReferenceLine
                        y={0}
                        stroke="rgb(255,255,255)"
                        strokeWidth={1}
                        ifOverflow="extendDomain"
                        shapeRendering="crispEdges"
                    />
                    <Tooltip
                        content={<CustomTooltip/>}
                        cursor={{fill: 'rgba(255, 215, 0, 0.1)'}}
                        animationDuration={100}
                        offset={40}
                    />
                    <Bar dataKey="pnl"/>
                </BarChart>
            </ResponsiveContainer>
        </div>
    );
});

// Filtered Wrapper Component for Profit/Loss Comparison
const NetProfitLossComparison = React.memo(() => {
    const renderCountRef = useRef(0);
    const [_isReady, setIsReady] = useState(false); // Renamed to avoid confusion if used later
    const [isLoading, setIsLoading] = useState(true);
    const isMountedRef = useRef(true);
    const {filteredTrades} = useTradeFilter();
    const filteredTradesRef = useRef(filteredTrades);
    const lastRenderKeyRef = useRef(`profit-loss-comparison-${filteredTrades?.length || 0}`);

    // 시간 기준과 시간 단위 상태 추가
    const [timeReference, setTimeReference] = useState<TimeReference>('진입 시간');
    const [timePeriod, setTimePeriod] = useState<TimePeriod>('일별');

    // 드롭다운 열림 상태 추가
    const [isTimeReferenceOpen, setIsTimeReferenceOpen] = useState(false);
    const [isTimePeriodOpen, setIsTimePeriodOpen] = useState(false);

    // 글로벌 스타일 추가 함수
    const addGlobalStyles = useCallback(() => {
        // 이미 스타일이 있는지 확인
        if (document.getElementById('profit-loss-dropdown-styles')) return;

        const styleEl = document.createElement('style');
        styleEl.id = 'profit-loss-dropdown-styles';
        styleEl.innerHTML = `
            /* 드롭다운 메뉴가 차트 위에 표시되도록 */
            .profit-loss-dropdown-menu {
                z-index: 9999 !important;
            }
            
            /* Select의 팝업 메뉴 스타일 */
            .MuiPopover-root {
                z-index: 9999 !important;
            }
            
            /* 드롭다운 메뉴 아이템 스타일 */
            .profit-loss-dropdown-paper .MuiMenuItem-root {
                pointer-events: auto !important;
            }
        `;
        document.head.appendChild(styleEl);
    }, []);

    // 컴포넌트 마운트/언마운트 로깅
    useEffect(() => {
        // 글로벌 스타일 추가
        addGlobalStyles();

        return () => {
            // 언마운트시 스타일 제거 (선택사항)
            const styleEl = document.getElementById('profit-loss-dropdown-styles');
            if (styleEl) {
                try {
                    document.head.removeChild(styleEl);
                } catch (error) {
                    console.warn('스타일 요소 제거 중 오류 (무시됨):', error);
                }
            }
        };
    }, [addGlobalStyles]);

    const renderKey = useMemo(() => {
        renderCountRef.current += 1;

        const newKey = `profit-loss-comparison-${filteredTrades?.length || 0}-${timeReference}-${timePeriod}`;
        if (newKey !== lastRenderKeyRef.current) {
            lastRenderKeyRef.current = newKey;
            return newKey;
        }

        return lastRenderKeyRef.current;
    }, [filteredTrades?.length, timeReference, timePeriod]);

    useEffect(() => {
        isMountedRef.current = true;
        return () => {
            isMountedRef.current = false;
        };
    }, []);

    useEffect(() => {
        if (filteredTradesRef.current?.length !== filteredTrades?.length) {
            filteredTradesRef.current = filteredTrades;
        }
    }, [filteredTrades]);

    const updateLoadingState = useCallback(() => {
        if (!isMountedRef.current) return;
        setIsLoading(true);
        // Logic to determine readiness based on filteredTrades
        if (Array.isArray(filteredTradesRef.current)) { // Keep readiness check simple for now
            setIsReady(true); // Assume ready if trades array exists
            setIsLoading(false);
        } else {
            setIsReady(false);
            setIsLoading(false);
        }
    }, []);

    useEffect(() => {
        if (!isMountedRef.current) return;
        const timer = setTimeout(updateLoadingState, 300); // Keep delay consistent
        return () => clearTimeout(timer);
    }, [filteredTrades?.length, updateLoadingState]);

    // 시간 기준 변경 핸들러 (타입 수정)
    const handleTimeReferenceChange = (event: SelectChangeEvent) => {
        const newValue = event.target.value as TimeReference;
        Promise.resolve().then(() => {
            setTimeReference(newValue);
        });
    };

    // 시간 단위 변경 핸들러 (타입 수정)
    const handleTimePeriodChange = (event: SelectChangeEvent) => {
        const newValue = event.target.value as TimePeriod;
        Promise.resolve().then(() => {
            setTimePeriod(newValue);
        });
    };

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
        <div style={{
            width: '100%',
            height: '100%',
            minHeight: '400px', // Consistent minimum height
            position: 'relative', // 자식 요소의 absolute positioning 기준
        }}>
            {/* Control Panel */}
            <Box
                sx={{
                    position: 'absolute', // 절대 위치 사용
                    top: -20, // 상단 여백
                    right: -10, // 우측 여백
                    zIndex: 10, // 차트 위에 표시
                    display: 'flex',
                    gap: 1.5, // 간격 줄임
                    backgroundColor: 'transparent', // 배경 투명으로 변경
                    padding: '8px 12px',
                    borderRadius: '6px',
                }}
            >
                <FormControl variant="outlined" size="small" sx={{minWidth: 105}}>
                    <InputLabel id="time-reference-label" sx={{
                        color: 'white',
                        fontSize: '15px', // 폰트 더 크게
                        fontWeight: 500,
                        fontFamily: "'Inter', 'Pretendard', sans-serif",
                        // 메뉴가 열려있고 포커스 상태일 때만 노란색
                        '&.Mui-focused': {
                            color: isTimeReferenceOpen ? 'rgba(255, 215, 0, 0.8)' : 'white',
                        }
                    }}>기준 시간</InputLabel>
                    <Select
                        labelId="time-reference-label"
                        value={timeReference}
                        onChange={handleTimeReferenceChange}
                        onOpen={() => setIsTimeReferenceOpen(true)}
                        onClose={() => setIsTimeReferenceOpen(false)}
                        label="기준 시간"
                        sx={{
                            backgroundColor: '#111111', // 기본 배경색 추가
                            color: 'white',
                            fontSize: '15px', // 폰트 더 크게
                            fontWeight: 500,
                            fontFamily: "'Inter', 'Pretendard', sans-serif",
                            boxShadow: isTimeReferenceOpen ? '0 0 8px rgba(255, 215, 0, 0.4)' : 'none', // 메뉴 열려있을 때만 boxShadow
                            transition: 'box-shadow 0.3s cubic-bezier(0.4, 0, 0.2, 1), border-color 0.3s cubic-bezier(0.4, 0, 0.2, 1)', // 테두리 색상 전환 애니메이션 추가
                            '.MuiSelect-select': {
                                padding: '8px 32px 8px 14px', // 패딩 조정
                            },
                            '& .MuiOutlinedInput-notchedOutline': {
                                // 기본 테두리 색상
                                borderColor: 'rgba(255, 215, 0, 0.4)',
                                transition: 'border-color 0.3s cubic-bezier(0.4, 0, 0.2, 1)' // 애니메이션 추가
                            },
                            // 포커스 상태 (메뉴 열림 여부에 따라 색상 변경)
                            '&.Mui-focused .MuiOutlinedInput-notchedOutline': {
                                borderColor: isTimeReferenceOpen ? 'rgba(255, 215, 0, 0.8)' : 'rgba(255, 215, 0, 0.4)',
                                borderWidth: '1px', // 포커스 시 테두리 두께 유지
                            },
                            // 호버 상태 (포커스 상태보다 우선 적용되어야 함)
                            '&:hover .MuiOutlinedInput-notchedOutline': {
                                borderColor: 'rgba(255, 215, 0, 0.8)', // 호버 시 항상 밝은 골드
                            },
                            '& .MuiSvgIcon-root': {
                                color: 'rgba(255, 215, 0, 0.8)', // 드롭다운 아이콘 색상
                                transition: 'transform 0.3s cubic-bezier(0.4, 0, 0.2, 1)' // 아이콘 애니메이션
                            },
                            '&.Mui-focused .MuiSvgIcon-root': {
                                transform: isTimeReferenceOpen ? 'rotate(180deg)' : 'rotate(0deg)', // 열렸을 때만 회전
                            }
                        }}
                        MenuProps={{
                            className: 'profit-loss-dropdown-menu',
                            disablePortal: false, // 포털 사용 (DOM 최상위로 렌더링)
                            PaperProps: {
                                className: 'profit-loss-dropdown-paper',
                                sx: {
                                    backgroundColor: '#111111', // 차트 배경색과 동일하게
                                    border: '1px solid rgba(255, 215, 0, 0.4)', // 골드 테두리
                                    borderRadius: '6px',
                                    boxShadow: '0 4px 20px rgba(0, 0, 0, 0.8)', // 그림자 강화
                                    mt: 0.5, // 메뉴 위치 조정
                                    '& .MuiMenuItem-root': {
                                        color: 'white',
                                        fontSize: '15px',
                                        fontFamily: "'Inter', 'Pretendard', sans-serif",
                                        transition: 'background-color 0.2s, color 0.2s',
                                        '&:hover': {
                                            backgroundColor: 'rgba(255, 215, 0, 0.15)',
                                        },
                                        '&.Mui-selected': {
                                            backgroundColor: 'rgba(255, 215, 0, 0.2)',
                                            color: 'rgba(255, 215, 0, 0.9)',
                                            '&:hover': {
                                                backgroundColor: 'rgba(255, 215, 0, 0.25)',
                                            }
                                        },
                                        '& .MuiTouchRipple-root': {
                                            display: 'none'
                                        }
                                    },
                                    // 드롭다운 애니메이션 효과
                                    transform: 'translateY(-8px)',
                                    opacity: 0,
                                    '&.MuiPopover-paper': {
                                        transform: 'translateY(0)',
                                        opacity: 1
                                    }
                                }
                            },
                            TransitionProps: {
                                timeout: 300 // 트랜지션 타임아웃 설정
                            }
                        }}
                    >
                        <MenuItem value="진입 시간">진입 시간</MenuItem>
                        <MenuItem value="청산 시간">청산 시간</MenuItem>
                    </Select>
                </FormControl>

                <FormControl variant="outlined" size="small" sx={{minWidth: 105}}>
                    <InputLabel id="time-period-label" sx={{
                        color: 'white',
                        fontSize: '15px', // 폰트 더 크게
                        fontWeight: 500,
                        fontFamily: "'Inter', 'Pretendard', sans-serif",
                        // 메뉴가 열려있고 포커스 상태일 때만 노란색
                        '&.Mui-focused': {
                            color: isTimePeriodOpen ? 'rgba(255, 215, 0, 0.8)' : 'white',
                        }
                    }}>시간 단위</InputLabel>
                    <Select
                        labelId="time-period-label"
                        value={timePeriod}
                        onChange={handleTimePeriodChange}
                        onOpen={() => setIsTimePeriodOpen(true)}
                        onClose={() => setIsTimePeriodOpen(false)}
                        label="시간 단위"
                        sx={{
                            backgroundColor: '#111111', // 차트 배경색과 동일하게
                            color: 'white',
                            fontSize: '15px', // 폰트 더 크게
                            fontWeight: 500,
                            fontFamily: "'Inter', 'Pretendard', sans-serif",
                            boxShadow: isTimePeriodOpen ? '0 0 8px rgba(255, 215, 0, 0.4)' : 'none', // 메뉴 열려있을 때만 boxShadow
                            transition: 'box-shadow 0.3s cubic-bezier(0.4, 0, 0.2, 1), border-color 0.3s cubic-bezier(0.4, 0, 0.2, 1)', // 테두리 색상 전환 애니메이션 추가
                            '.MuiSelect-select': {
                                padding: '8px 32px 8px 14px', // 패딩 조정
                            },
                            '& .MuiOutlinedInput-notchedOutline': {
                                // 기본 테두리 색상
                                borderColor: 'rgba(255, 215, 0, 0.4)',
                                transition: 'border-color 0.3s cubic-bezier(0.4, 0, 0.2, 1)' // 애니메이션 추가
                            },
                            // 포커스 상태 (메뉴 열림 여부에 따라 색상 변경)
                            '&.Mui-focused .MuiOutlinedInput-notchedOutline': {
                                borderColor: isTimePeriodOpen ? 'rgba(255, 215, 0, 0.8)' : 'rgba(255, 215, 0, 0.4)',
                                borderWidth: '1px', // 포커스 시 테두리 두께 유지
                            },
                            // 호버 상태 (포커스 상태보다 우선 적용되어야 함)
                            '&:hover .MuiOutlinedInput-notchedOutline': {
                                borderColor: 'rgba(255, 215, 0, 0.8)', // 호버 시 항상 밝은 골드
                            },
                            '& .MuiSvgIcon-root': {
                                color: 'rgba(255, 215, 0, 0.8)', // 드롭다운 아이콘 색상
                                transition: 'transform 0.3s cubic-bezier(0.4, 0, 0.2, 1)' // 아이콘 애니메이션
                            },
                            '&.Mui-focused .MuiSvgIcon-root': {
                                transform: isTimePeriodOpen ? 'rotate(180deg)' : 'rotate(0deg)', // 열렸을 때만 회전
                            }
                        }}
                        MenuProps={{
                            className: 'profit-loss-dropdown-menu',
                            disablePortal: false, // 포털 사용 (DOM 최상위로 렌더링)
                            PaperProps: {
                                className: 'profit-loss-dropdown-paper',
                                sx: {
                                    backgroundColor: '#111111', // 차트 배경색과 동일하게
                                    border: '1px solid rgba(255, 215, 0, 0.4)', // 골드 테두리
                                    borderRadius: '6px',
                                    boxShadow: '0 4px 20px rgba(0, 0, 0, 0.8)', // 그림자 강화
                                    mt: 0.5, // 메뉴 위치 조정
                                    '& .MuiMenuItem-root': {
                                        color: 'white',
                                        fontSize: '15px',
                                        fontFamily: "'Inter', 'Pretendard', sans-serif",
                                        transition: 'background-color 0.2s, color 0.2s',
                                        '&:hover': {
                                            backgroundColor: 'rgba(255, 215, 0, 0.15)',
                                        },
                                        '&.Mui-selected': {
                                            backgroundColor: 'rgba(255, 215, 0, 0.2)',
                                            color: 'rgba(255, 215, 0, 0.9)',
                                            '&:hover': {
                                                backgroundColor: 'rgba(255, 215, 0, 0.25)',
                                            }
                                        },
                                        '& .MuiTouchRipple-root': {
                                            display: 'none'
                                        }
                                    },
                                    // 드롭다운 애니메이션 효과
                                    transform: 'translateY(-8px)',
                                    opacity: 0,
                                    '&.MuiPopover-paper': {
                                        transform: 'translateY(0)',
                                        opacity: 1
                                    }
                                }
                            },
                            TransitionProps: {
                                timeout: 300 // 트랜지션 타임아웃 설정
                            }
                        }}
                    >
                        <MenuItem value="연도별">연도별</MenuItem>
                        <MenuItem value="월별">월별</MenuItem>
                        <MenuItem value="일별">일별</MenuItem>
                        <MenuItem value="요일별">요일별</MenuItem>
                        <MenuItem value="시간별">시간별</MenuItem>
                        <MenuItem value="분별">분별</MenuItem>
                        <MenuItem value="초별">초별</MenuItem>
                    </Select>
                </FormControl>
            </Box>

            {/* Render the actual plot component */}
            <div style={{
                flexGrow: 1,
                width: '100%',
                height: '100%',
            }}>
                <NetProfitLossComparisonPlot
                    key={renderKey}
                    timeReference={timeReference}
                    timePeriod={timePeriod}
                />
            </div>
        </div>
    );
});

export default NetProfitLossComparison;
