import React, {useCallback, useEffect, useMemo, useRef, useState} from 'react';
import {ColorType, createChart, CrosshairMode, LineSeries, LineStyle, PriceScaleMode} from 'lightweight-charts';
import {useTradeFilter} from '@/components/TradeFilter';
import {Box, FormControl, InputLabel, MenuItem, Select} from '@mui/material';
import {parseHoldingTime} from '@/components/TradeFilter/ParseHoldingTime';
import {formatDuration} from '@/components/Performance/Utils';

// 시간 상수 정의 (초 단위)
const Minute = 60;
const Hour = 60 * Minute;
const Day = 24 * Hour;
const Week = 7 * Day;
const Month = 30 * Day; // 대략적인 한 달

// timeframe 파싱 함수 - 초 단위로 반환
function parseTimeframe(timeframe: string): number {
    const unit = timeframe.slice(-1);
    const value = parseInt(timeframe.slice(0, -1));
    if (isNaN(value)) return Minute;
    switch (unit) {
        case 'm':
            return value * Minute;  // 분 단위를 초로 변환
        case 'h':
            return value * Hour;    // 시간 단위를 초로 변환
        case 'd':
            return value * Day;     // 일 단위를 초로 변환
        case 'w':
            return value * Week;    // 주 단위를 초로 변환
        case 'M':
            return value * Month;   // 월 단위를 초로 변환
        default:
            return Minute;          // 기본: 1분 = 60초
    }
}

// timeframe 단위 → 한글 단위 변환 함수
const timeframeUnitToKorean = (timeframe: string): string => {
    if (!timeframe) return '초';
    const unit = timeframe.slice(-1);
    switch (unit) {
        case 'm':
            return '분';
        case 'h':
            return '시간';
        case 'd':
            return '일';
        case 'w':
            return '주';
        case 'M':
            return '개월';
        default:
            return '초';
    }
};

// 메트릭별 포맷 함수 매핑
const getFormatFunction = (metric: string, timeframeStr?: string) => {
    switch (metric) {
        case '누적 순손익':
            return (price: number) => {
                const absPrice = Math.abs(price);
                const sign = price >= 0 ? '$' : '-$';
                return `${sign}${Number(absPrice.toFixed(2)).toLocaleString('en-US', {
                    minimumFractionDigits: 2,
                    maximumFractionDigits: 2
                })}`;
            };
        case '진입 횟수':
            return (price: number) => {
                if (price < 0) return '';

                return `${Math.round(price).toLocaleString()}회`;
            };
        case '강제 청산 횟수':
            return (price: number) => {
                if (price < 0) return '';

                return `${Math.round(price).toLocaleString()}회`;
            };
        case '펀딩 횟수':
            return (price: number) => {
                if (price < 0) return '';

                return `${Math.round(price).toLocaleString()}회`;
            };
        case '펀딩비':
            return (price: number) => {
                const absPrice = Math.abs(price);
                const sign = price >= 0 ? '$' : '-$';
                return `${sign}${Number(absPrice.toFixed(2)).toLocaleString('en-US', {
                    minimumFractionDigits: 2,
                    maximumFractionDigits: 2
                })}`;
            };
        case '승률':
            return (price: number) => {
                if (price < 0 || price > 100) return '';

                return `${Number(price.toFixed(2)).toLocaleString('en-US', {
                    minimumFractionDigits: 2,
                    maximumFractionDigits: 2
                })}%`;
            };
        case '손익비':
            return (price: number) => {
                if (price < 0) return '';

                if (price === Infinity || !isFinite(price)) return "∞";
                return Number(price.toFixed(2)).toLocaleString('en-US', {
                    minimumFractionDigits: 2,
                    maximumFractionDigits: 2
                });
            };
        case '기대값':
            return (price: number) => {
                const absPrice = Math.abs(price);
                const sign = price >= 0 ? '$' : '-$';
                return `${sign}${Number(absPrice.toFixed(2)).toLocaleString('en-US', {
                    minimumFractionDigits: 2,
                    maximumFractionDigits: 2
                })}`;
            };
        case '보유 시간 합계':
            return (seconds: number) => {
                if (seconds < 0) return '';

                if (seconds === 0) {
                    return `0${timeframeUnitToKorean(timeframeStr || '')}`;
                }

                return formatDuration(new Date(0), new Date(seconds * 1000));
            };
        default:
            return (price: number) => {
                const absPrice = Math.abs(price);
                const sign = price >= 0 ? '$' : '-$';
                return `${sign}${Math.round(absPrice).toLocaleString()}`;
            };
    }
};

// 메인 툴팁 전용 포맷 함수 (개별 함수 없이 인라인)
const getTooltipFormatFunction = (metric: string, timeframeStr?: string) => {
    switch (metric) {
        case '누적 순손익':
            return (price: number) => {
                const absPrice = Math.abs(price);
                const sign = price >= 0 ? '$' : '-$';
                return `${sign}${Number(absPrice.toFixed(2)).toLocaleString('en-US', {
                    minimumFractionDigits: 2,
                    maximumFractionDigits: 2
                })}`;
            };
        case '진입 횟수':
            return (price: number) => {
                return `${Math.round(price).toLocaleString()}회`;
            };
        case '강제 청산 횟수':
            return (price: number) => {
                return `${Math.round(price).toLocaleString()}회`;
            };
        case '펀딩 횟수':
            return (price: number) => {
                return `${Math.round(price).toLocaleString()}회`;
            };
        case '펀딩비':
            return (price: number) => {
                const absPrice = Math.abs(price);
                const sign = price >= 0 ? '$' : '-$';
                return `${sign}${Number(absPrice.toFixed(2)).toLocaleString('en-US', {
                    minimumFractionDigits: 2,
                    maximumFractionDigits: 2
                })}`;
            };
        case '승률':
            return (price: number) => {
                return `${Number(price.toFixed(2)).toLocaleString('en-US', {
                    minimumFractionDigits: 2,
                    maximumFractionDigits: 2
                })}%`;
            };
        case '손익비':
            return (price: number) => {
                if (price === Infinity || !isFinite(price)) return "∞";
                return Number(price.toFixed(2)).toLocaleString('en-US', {
                    minimumFractionDigits: 2,
                    maximumFractionDigits: 2
                });
            };
        case '기대값':
            return (price: number) => {
                const absPrice = Math.abs(price);
                const sign = price >= 0 ? '$' : '-$';
                return `${sign}${Number(absPrice.toFixed(2)).toLocaleString('en-US', {
                    minimumFractionDigits: 2,
                    maximumFractionDigits: 2
                })}`;
            };
        case '보유 시간 합계':
            return (seconds: number) => {
                if (seconds === 0) {
                    return `0${timeframeUnitToKorean(timeframeStr || '')}`;
                }
                return formatDuration(new Date(0), new Date(seconds * 1000));
            };
        default:
            return (price: number) => {
                const absPrice = Math.abs(price);
                const sign = price >= 0 ? '$' : '-$';
                return `${sign}${Math.round(absPrice).toLocaleString()}`;
            };
    }
};

export interface SymbolPerformanceProps {
    config: any;
}

const SymbolPerformance: React.FC<SymbolPerformanceProps> = ({config}) => {
    const chartContainerRef = useRef<HTMLDivElement>(null);
    const chartRef = useRef<any>(null);
    const {filteredTrades, loading} = useTradeFilter();
    const [selectedMetric, setSelectedMetric] = useState<string>('누적 순손익');
    const [isDropdownOpen, setIsDropdownOpen] = useState(false);

    // config에서 심볼 순서 추출 - useMemo로 메모이제이션
    const configSymbols: string[] = useMemo(() =>
            Array.isArray(config?.["심볼"]) ? config["심볼"].map((s: any) => s["심볼 이름"]) : [],
        [config]
    );

    // config에서 timeframe 가져오기 - config["심볼"][0]["트레이딩 바 데이터"]["타임프레임"]
    const timeframePeriod = useMemo(() => {
        try {
            const firstSymbol = config?.["심볼"]?.[0];
            const timeframe = firstSymbol?.["트레이딩 바 데이터"]?.["타임프레임"];

            if (timeframe) {
                return parseTimeframe(timeframe);
            }

            // 기본값: 1분
            return parseTimeframe("1m");
        } catch (error) {
            console.error('❌ Error parsing timeframe:', error);
            return parseTimeframe("1m");
        }
    }, [config]);

    const timeframeStr = useMemo(() => {
        try {
            const firstSymbol = config?.["심볼"]?.[0];
            return firstSymbol?.["트레이딩 바 데이터"]?.["타임프레임"] || '1m';
        } catch {
            return '1m';
        }
    }, [config]);

    // 실제 거래 데이터만 필터링 - 한 번만 계산
    const actualTrades = useMemo(() =>
            filteredTrades ? filteredTrades.filter((trade: any) => trade["거래 번호"] !== 0) : [],
        [filteredTrades]
    );

    // 거래별 전처리된 데이터 - 한 번만 계산해서 모든 메트릭에서 재사용
    const preprocessedData = useMemo(() => {
        if (!actualTrades || actualTrades.length === 0) {
            return {
                tradesBySymbol: new Map(),
                allExitTimes: [],
                allEntryTimes: [],
                tradeResults: [],
                symbolGroups: new Map()
            };
        }

        // 심볼별 거래 그룹화
        const tradesBySymbol = new Map<string, any[]>();
        configSymbols.forEach(symbol => {
            tradesBySymbol.set(symbol, []);
        });

        // 시간 데이터 수집
        const exitTimeSet = new Set<number>();
        const entryTimeSet = new Set<number>();

        // 거래번호별 그룹화
        const symbolGroups = new Map<number, any[]>();

        actualTrades.forEach((trade: any) => {
            const symbol = trade["심볼 이름"] as string;
            const tradeNumber = trade["거래 번호"] as number;

            // 심볼별 그룹화
            if (tradesBySymbol.has(symbol)) {
                tradesBySymbol.get(symbol)!.push(trade);
            }

            // 거래번호별 그룹화
            if (!symbolGroups.has(tradeNumber)) {
                symbolGroups.set(tradeNumber, []);
            }
            symbolGroups.get(tradeNumber)!.push(trade);

            // 시간 수집
            const exitTime = new Date(trade["청산 시간"] as string).getTime() / 1000;
            const entryTime = new Date(trade["진입 시간"] as string).getTime() / 1000;
            exitTimeSet.add(exitTime);
            entryTimeSet.add(entryTime);
        });

        // 시간 배열로 변환 및 정렬
        const allExitTimes = Array.from(exitTimeSet).sort((a, b) => a - b);
        const allEntryTimes = Array.from(entryTimeSet).sort((a, b) => a - b);

        // 거래 결과 계산 (승률, 손익비, 기대값용)
        const tradeResults: { symbol: string; exitTime: string; totalPnl: number; isWin: boolean }[] = [];

        symbolGroups.forEach((trades: any[]) => {
            if (trades.length === 0) return;

            const totalPnl = trades.reduce((sum: number, trade: any) => sum + (trade["순손익"] as number), 0);
            const isWin = totalPnl >= 0;
            const lastExitTime = trades.reduce((latest: string, trade: any) => {
                const currentTime = trade["청산 시간"] as string;
                return currentTime > latest ? currentTime : latest;
            }, trades[0]["청산 시간"] as string);
            const symbol = trades[0]["심볼 이름"] as string;

            tradeResults.push({symbol, exitTime: lastExitTime, totalPnl, isWin});
        });

        return {
            tradesBySymbol,
            allExitTimes,
            allEntryTimes,
            tradeResults,
            symbolGroups
        };
    }, [actualTrades, configSymbols]);

    // timeframe에 따른 0 포인트 추가 시간 계산
    const getTimesWithZeroPoint = useCallback((baseTimes: number[]) => {
        if (!timeframePeriod || baseTimes.length === 0) return baseTimes;

        const timesWithZero = [...baseTimes];
        const firstTimeWithOffset = baseTimes[0] - timeframePeriod;
        timesWithZero.unshift(firstTimeWithOffset);
        return timesWithZero;
    }, [timeframePeriod]);

    // 드롭다운 핸들러
    const handleMetricChange = (event: any) => {
        setSelectedMetric(event.target.value);
    };

    const toggleDropdown = useCallback(() => {
        setIsDropdownOpen(prev => !prev);
    }, []);

    // 데이터 생성 함수들 - 전처리된 데이터 활용
    const generateDataForMetric = useCallback((metric: string) => {
        if (!preprocessedData || preprocessedData.allExitTimes.length === 0) {
            return {allTimes: [], symbolDataArray: []};
        }

        switch (metric) {
            case '누적 순손익':
                return generateCumPnlData();
            case '진입 횟수':
                return generateEntryCountData();
            case '강제 청산 횟수':
                return generateLiquidationCountData();
            case '펀딩 횟수':
                return generateFundingCountData();
            case '펀딩비':
                return generateFundingFeeData();
            case '승률':
                return generateWinRateData();
            case '손익비':
                return generateProfitLossRatioData();
            case '기대값':
                return generateExpectedValueData();
            case '보유 시간 합계':
                return generateHoldingTimeTotalData();
            default:
                return generateCumPnlData();
        }
    }, [preprocessedData, timeframePeriod, configSymbols]);

    // 누적 순손익 데이터 생성 - 최적화된 버전
    const generateCumPnlData = () => {
        const allExitTimes = getTimesWithZeroPoint(preprocessedData.allExitTimes);

        const symbolDataArray: { symbol: string; data: { time: number; value: number }[] }[] = [];

        // config의 모든 심볼을 미리 초기화하고 0 포인트 명시적 추가
        configSymbols.forEach((symbol: string) => {
            const data: { time: number; value: number }[] = [];

            // timeframe이 있으면 0 포인트 추가
            if (timeframePeriod && allExitTimes.length > 0) {
                data.push({
                    time: allExitTimes[0], // 첫 시간 - timeframe
                    value: 0              // 명시적으로 0
                });
            }

            symbolDataArray.push({symbol, data});
        });

        // 심볼별 누적 순손익 계산
        configSymbols.forEach((symbol: string) => {
            const symbolTrades = preprocessedData.tradesBySymbol.get(symbol) || [];
            const symbolData = symbolDataArray.find(item => item.symbol === symbol);

            if (symbolData) {
                let cumPnl = 0; // 항상 0부터 시작

                symbolTrades.forEach((trade: any) => {
                    cumPnl += trade["순손익"] as number;
                    const timestamp = new Date(trade["청산 시간"] as string).getTime() / 1000;

                    symbolData.data.push({
                        time: timestamp,
                        value: cumPnl
                    });
                });
            }
        });

        return {allTimes: allExitTimes, symbolDataArray};
    };

    // 진입 횟수 데이터 생성 - 최적화된 버전
    const generateEntryCountData = () => {
        const allEntryTimes = getTimesWithZeroPoint(preprocessedData.allEntryTimes);
        const symbolDataArray: { symbol: string; data: { time: number; value: number }[] }[] = [];

        // config의 모든 심볼을 미리 초기화하고 0 포인트 명시적 추가
        configSymbols.forEach((symbol: string) => {
            const data: { time: number; value: number }[] = [];
            if (timeframePeriod && allEntryTimes.length > 0) {
                data.push({time: allEntryTimes[0], value: 0});
            }
            symbolDataArray.push({symbol, data});
        });

        // 각 거래를 심볼별로 카운트 계산 (같은 거래 번호는 한 번만)
        const processedTradeNumbers = new Set<number>();

        actualTrades.forEach((trade: any) => {
            const symbol = trade["심볼 이름"] as string;
            const entryTime = trade["진입 시간"] as string;
            const tradeNumber = trade["거래 번호"] as number;

            // 이미 처리된 거래 번호는 건너뛰기
            if (processedTradeNumbers.has(tradeNumber)) return;
            processedTradeNumbers.add(tradeNumber);

            // 시간을 timestamp로 변환
            const dateObject = new Date(entryTime);
            const timestamp = dateObject.getTime() / 1000;

            // symbolDataArray에서 해당 심볼 찾기
            const symbolData = symbolDataArray.find((item: any) => item.symbol === symbol);
            if (symbolData) {
                const lastValue = symbolData.data.length > 0 ? symbolData.data[symbolData.data.length - 1].value : 0;
                symbolData.data.push({
                    time: timestamp,
                    value: lastValue + 1 // 1씩 증가
                });
            }
        });

        return {allTimes: allEntryTimes, symbolDataArray};
    };

    // 승률 데이터 생성 - 최적화된 버전  
    const generateWinRateData = () => {
        const tradeResults = preprocessedData.tradeResults;

        // tradeResults에서 실제 시간들만 수집
        const tradeResultTimes = tradeResults.map((result) => {
            const dateObject = new Date(result.exitTime);
            return dateObject.getTime() / 1000;
        }).sort((a, b) => a - b);

        const allTradeResultTimes = getTimesWithZeroPoint(tradeResultTimes);
        const symbolDataArray: { symbol: string; data: { time: number; value: number }[] }[] = [];

        // config의 모든 심볼을 미리 초기화하고 0 포인트 명시적 추가
        configSymbols.forEach((symbol: string) => {
            const data: { time: number; value: number }[] = [];
            if (timeframePeriod && allTradeResultTimes.length > 0) {
                data.push({time: allTradeResultTimes[0], value: 0});
            }
            symbolDataArray.push({symbol, data});
        });

        // 각 거래 결과를 심볼별로 승률 계산
        const symbolStats = new Map<string, { wins: number; total: number }>();

        // config의 모든 심볼을 미리 초기화
        configSymbols.forEach((symbol: string) => {
            symbolStats.set(symbol, {wins: 0, total: 0});
        });

        tradeResults.forEach((result) => {
            const symbol = result.symbol;
            const exitTime = result.exitTime;
            const isWin = result.isWin;

            // 시간을 timestamp로 변환
            const dateObject = new Date(exitTime);
            const timestamp = dateObject.getTime() / 1000;

            // 심볼별 통계 업데이트
            const stats = symbolStats.get(symbol);
            if (stats) {
                stats.total += 1;
                if (isWin) stats.wins += 1;

                const winRate = stats.total > 0 ? (stats.wins / stats.total) * 100 : 0;

                // symbolDataArray에서 해당 심볼 찾기
                const symbolData = symbolDataArray.find((item: any) => item.symbol === symbol);
                if (symbolData) {
                    symbolData.data.push({
                        time: timestamp,
                        value: winRate
                    });
                }
            }
        });

        return {allTimes: allTradeResultTimes, symbolDataArray};
    };

    // 손익비 데이터 생성 - 최적화된 버전
    const generateProfitLossRatioData = () => {
        const tradeResults = preprocessedData.tradeResults;

        // tradeResults에서 실제 시간들만 수집
        const tradeResultTimes = tradeResults.map((result) => {
            const dateObject = new Date(result.exitTime);
            return dateObject.getTime() / 1000;
        }).sort((a, b) => a - b);

        const allTradeResultTimes = getTimesWithZeroPoint(tradeResultTimes);
        const symbolDataArray: { symbol: string; data: { time: number; value: number }[] }[] = [];

        // config의 모든 심볼을 미리 초기화하고 0 포인트 명시적 추가
        configSymbols.forEach((symbol: string) => {
            const data: { time: number; value: number }[] = [];
            if (timeframePeriod && allTradeResultTimes.length > 0) {
                data.push({time: allTradeResultTimes[0], value: 0});
            }
            symbolDataArray.push({symbol, data});
        });

        // 각 거래 결과를 심볼별로 손익비 계산
        const symbolStats = new Map<string, { wins: number[]; losses: number[] }>();

        // config의 모든 심볼을 미리 초기화
        configSymbols.forEach((symbol: string) => {
            symbolStats.set(symbol, {wins: [], losses: []});
        });

        tradeResults.forEach((result) => {
            const symbol = result.symbol;
            const exitTime = result.exitTime;
            const totalPnl = result.totalPnl;
            const isWin = result.isWin;

            // 시간을 timestamp로 변환
            const dateObject = new Date(exitTime);
            const timestamp = dateObject.getTime() / 1000;

            // 심볼별 통계 업데이트
            const stats = symbolStats.get(symbol);
            if (stats) {
                if (isWin) {
                    stats.wins.push(totalPnl);
                } else {
                    stats.losses.push(Math.abs(totalPnl)); // 손실은 절댓값으로 저장
                }

                // 손익비 계산 - RiskRewardMetrics.ts와 동일한 로직
                const avgWin = stats.wins.length > 0 ? stats.wins.reduce((a, b) => a + b, 0) / stats.wins.length : 0;
                const avgLoss = stats.losses.length > 0 ? stats.losses.reduce((a, b) => a + b, 0) / stats.losses.length : 0;

                // 손익비 계산을 위한 숫자 타입 변수 초기화
                let avgProfitNumber: number;
                let avgLossNumber: number;

                // avgWin 타입 체크 후 숫자 변환
                avgProfitNumber = avgWin;

                // avgLoss 타입 체크 후 숫자 변환 (절대값 사용)
                avgLossNumber = Math.abs(avgLoss);

                // 손익비 계산
                let profitLossRatio: number | null;
                if (avgLossNumber > 0) {
                    // avgProfit도 숫자인 경우에만 계산
                    profitLossRatio = avgProfitNumber > 0
                        ? avgProfitNumber / avgLossNumber
                        : 0; // 평균 수익이 0 이하면 0으로 처리
                } else {
                    // 손실이 없으면 평균 수익이 0 초과일 경우 null (차트에서는 빈 공간), 아니면 0
                    profitLossRatio = avgProfitNumber > 0 ? null : 0;
                }

                // symbolDataArray에서 해당 심볼 찾기
                const symbolData = symbolDataArray.find((item: any) => item.symbol === symbol);
                if (symbolData && profitLossRatio !== null) {
                    symbolData.data.push({
                        time: timestamp,
                        value: profitLossRatio
                    });
                }
            }
        });

        return {allTimes: allTradeResultTimes, symbolDataArray};
    };

    // 기대값 데이터 생성 - 최적화된 버전
    const generateExpectedValueData = () => {
        const tradeResults = preprocessedData.tradeResults;

        // tradeResults에서 실제 시간들만 수집
        const tradeResultTimes = tradeResults.map((result) => {
            const dateObject = new Date(result.exitTime);
            return dateObject.getTime() / 1000;
        }).sort((a, b) => a - b);

        const allTradeResultTimes = getTimesWithZeroPoint(tradeResultTimes);
        const symbolDataArray: { symbol: string; data: { time: number; value: number }[] }[] = [];

        // config의 모든 심볼을 미리 초기화하고 0 포인트 명시적 추가
        configSymbols.forEach((symbol: string) => {
            const data: { time: number; value: number }[] = [];
            if (timeframePeriod && allTradeResultTimes.length > 0) {
                data.push({time: allTradeResultTimes[0], value: 0});
            }
            symbolDataArray.push({symbol, data});
        });

        // 각 거래 결과를 심볼별로 기대값 계산
        const symbolStats = new Map<string, { wins: number[]; losses: number[]; totalTrades: number }>();

        // config의 모든 심볼을 미리 초기화
        configSymbols.forEach((symbol: string) => {
            symbolStats.set(symbol, {wins: [], losses: [], totalTrades: 0});
        });

        tradeResults.forEach((result) => {
            const symbol = result.symbol;
            const exitTime = result.exitTime;
            const totalPnl = result.totalPnl;
            const isWin = result.isWin;

            // 시간을 timestamp로 변환
            const dateObject = new Date(exitTime);
            const timestamp = dateObject.getTime() / 1000;

            // 심볼별 통계 업데이트
            const stats = symbolStats.get(symbol);
            if (stats) {
                stats.totalTrades += 1;

                if (isWin) {
                    stats.wins.push(totalPnl);
                } else {
                    stats.losses.push(Math.abs(totalPnl)); // 손실은 절댓값으로 저장
                }

                // 기대값 계산 - Report.tsx와 동일한 로직
                const winRate = stats.totalTrades > 0 ? stats.wins.length / stats.totalTrades : 0;
                const lossRate = 1 - winRate;
                const avgWin = stats.wins.length > 0 ? stats.wins.reduce((a, b) => a + b, 0) / stats.wins.length : 0;
                const avgLoss = stats.losses.length > 0 ? stats.losses.reduce((a, b) => a + b, 0) / stats.losses.length : 0;

                let winPart = 0;
                let lossPart = 0;

                if (!isNaN(avgWin)) {
                    winPart = winRate * avgWin;
                }

                if (!isNaN(avgLoss)) {
                    lossPart = lossRate * Math.abs(avgLoss);
                }

                const expectedValue = winPart - lossPart;

                // symbolDataArray에서 해당 심볼 찾기
                const symbolData = symbolDataArray.find((item: any) => item.symbol === symbol);
                if (symbolData) {
                    symbolData.data.push({
                        time: timestamp,
                        value: expectedValue
                    });
                }
            }
        });

        return {allTimes: allTradeResultTimes, symbolDataArray};
    };

    // 보유 시간 합계 데이터 생성 - 최적화된 버전 (중복 시간 범위 제거)
    const generateHoldingTimeTotalData = () => {
        const symbolDataArray: { symbol: string; data: { time: number; value: number }[] }[] = [];

        // 보유 시간 관련 시간들을 수집할 Set
        const holdingTimeSet = new Set<number>();

        // 먼저 모든 심볼에서 보유 시간 관련 시간들을 수집
        configSymbols.forEach((symbol: string) => {
            const symbolTrades = preprocessedData.tradesBySymbol.get(symbol) || [];

            symbolTrades.forEach((trade: any) => {
                const holdingTimeStr = String(trade["보유 시간"]);
                const holdingTimeSeconds = parseHoldingTime(holdingTimeStr);
                if (holdingTimeSeconds !== null && holdingTimeSeconds > 0) {
                    const timestamp = new Date(trade["청산 시간"] as string).getTime() / 1000;
                    holdingTimeSet.add(timestamp);
                }
            });
        });

        // 시간 배열로 변환 및 정렬
        const holdingTimes = Array.from(holdingTimeSet).sort((a, b) => a - b);
        const allHoldingTimes = getTimesWithZeroPoint(holdingTimes);

        // config의 모든 심볼을 미리 초기화하고 0 포인트 명시적 추가
        configSymbols.forEach((symbol: string) => {
            const data: { time: number; value: number }[] = [];
            if (timeframePeriod && allHoldingTimes.length > 0) {
                data.push({time: allHoldingTimes[0], value: 0});
            }
            symbolDataArray.push({symbol, data});
        });

        // 심볼별 보유 시간 합계 계산 (모든 거래 청산 시간 사용, 시간 범위 겹침 제거)
        configSymbols.forEach((symbol: string) => {
            const symbolTrades = preprocessedData.tradesBySymbol.get(symbol) || [];
            const symbolData = symbolDataArray.find(item => item.symbol === symbol);

            if (symbolData) {
                // 모든 거래를 청산 시간 순으로 정렬
                const sortedTrades = [...symbolTrades]
                    .filter(trade => {
                        const holdingTimeStr = String(trade["보유 시간"]);
                        const holdingTimeSeconds = parseHoldingTime(holdingTimeStr);
                        return holdingTimeSeconds !== null && holdingTimeSeconds > 0;
                    })
                    .sort((a, b) => {
                        const timeA = new Date(a["청산 시간"] as string).getTime();
                        const timeB = new Date(b["청산 시간"] as string).getTime();
                        return timeA - timeB;
                    });

                // 누적 시간 범위 관리 (겹침 제거용)
                const timeRanges: { start: number; end: number }[] = [];
                let totalHoldingTime = 0;

                sortedTrades.forEach((trade: any) => {
                    const entryTime = new Date(trade["진입 시간"] as string).getTime() / 1000;
                    const exitTime = new Date(trade["청산 시간"] as string).getTime() / 1000;

                    // 새로운 시간 범위
                    const newRange = {start: entryTime, end: exitTime};

                    // 기존 범위들과 겹치는지 확인하고 병합
                    let overlappingRanges: number[] = [];
                    let mergedRange = {...newRange};

                    for (let i = 0; i < timeRanges.length; i++) {
                        const existing = timeRanges[i];

                        // 겹치는 경우
                        if (newRange.start <= existing.end && newRange.end >= existing.start) {
                            overlappingRanges.push(i);
                            mergedRange.start = Math.min(mergedRange.start, existing.start);
                            mergedRange.end = Math.max(mergedRange.end, existing.end);
                        }
                    }

                    // 겹치는 범위들을 제거하고 새로운 병합된 범위 추가
                    if (overlappingRanges.length > 0) {
                        // 겹치는 기존 범위들의 총 시간 계산
                        let removedTime = 0;
                        for (let i = overlappingRanges.length - 1; i >= 0; i--) {
                            const idx = overlappingRanges[i];
                            const removedRange = timeRanges.splice(idx, 1)[0];
                            removedTime += (removedRange.end - removedRange.start);
                        }

                        // 병합된 범위 추가
                        timeRanges.push(mergedRange);

                        // 실제 추가된 시간 = 병합된 범위 시간 - 제거된 시간
                        const addedTime = (mergedRange.end - mergedRange.start) - removedTime;
                        totalHoldingTime += addedTime;
                    } else {
                        // 겹치지 않는 경우: 새로운 범위 추가
                        timeRanges.push(newRange);
                        totalHoldingTime += (newRange.end - newRange.start);
                    }

                    // 각 거래 청산 시점에 누적 보유 시간 추가
                    symbolData.data.push({
                        time: exitTime,
                        value: totalHoldingTime
                    });
                });
            }
        });

        return {allTimes: allHoldingTimes, symbolDataArray};
    };

    // 강제 청산 횟수 데이터 생성
    const generateLiquidationCountData = () => {
        const symbolDataArray: { symbol: string; data: { time: number; value: number }[] }[] = [];

        // 강제 청산 관련 시간들을 수집할 Set
        const liquidationTimeSet = new Set<number>();

        // 먼저 모든 심볼에서 강제 청산 관련 시간들을 수집
        configSymbols.forEach((symbol: string) => {
            const symbolTrades = preprocessedData.tradesBySymbol.get(symbol) || [];

            symbolTrades.forEach((trade: any) => {
                const exitName = String(trade["청산 이름"] || "");
                if (exitName.includes("강제 청산")) {
                    const timestamp = new Date(trade["청산 시간"] as string).getTime() / 1000;
                    liquidationTimeSet.add(timestamp);
                }
            });
        });

        // 시간 배열로 변환 및 정렬
        const liquidationTimes = Array.from(liquidationTimeSet).sort((a, b) => a - b);
        const allLiquidationTimes = getTimesWithZeroPoint(liquidationTimes);

        // config의 모든 심볼을 미리 초기화하고 0 포인트 명시적 추가
        configSymbols.forEach((symbol: string) => {
            const data: { time: number; value: number }[] = [];
            if (timeframePeriod && allLiquidationTimes.length > 0) {
                data.push({time: allLiquidationTimes[0], value: 0});
            }
            symbolDataArray.push({symbol, data});
        });

        // 심볼별 강제 청산 횟수 계산
        configSymbols.forEach((symbol: string) => {
            const symbolTrades = preprocessedData.tradesBySymbol.get(symbol) || [];
            const symbolData = symbolDataArray.find(item => item.symbol === symbol);

            if (symbolData) {
                let liquidationCount = 0;

                symbolTrades.forEach((trade: any) => {
                    const exitName = String(trade["청산 이름"] || "");
                    if (exitName.includes("강제 청산")) {
                        liquidationCount += 1;
                        const timestamp = new Date(trade["청산 시간"] as string).getTime() / 1000;

                        symbolData.data.push({
                            time: timestamp,
                            value: liquidationCount
                        });
                    }
                });
            }
        });

        return {allTimes: allLiquidationTimes, symbolDataArray};
    };

    // 펀딩 횟수 데이터 생성
    const generateFundingCountData = () => {
        const symbolDataArray: { symbol: string; data: { time: number; value: number }[] }[] = [];

        // 펀딩 횟수 관련 시간들을 수집할 Set
        const fundingTimeSet = new Set<number>();

        // 먼저 모든 심볼에서 펀딩 관련 시간들을 수집
        configSymbols.forEach((symbol: string) => {
            const symbolTrades = preprocessedData.tradesBySymbol.get(symbol) || [];

            // 거래 번호별로 그룹화
            const tradesByNumber = new Map<number, any[]>();
            symbolTrades.forEach((trade: any) => {
                const tradeNumber = trade["거래 번호"] as number;
                if (!tradesByNumber.has(tradeNumber)) {
                    tradesByNumber.set(tradeNumber, []);
                }
                tradesByNumber.get(tradeNumber)!.push(trade);
            });

            // 각 거래 번호별로 최대 펀딩 횟수를 가진 거래의 시간 수집
            tradesByNumber.forEach((trades: any[]) => {
                if (trades.length === 0) return;

                let maxFundingCount = 0;
                let maxFundingTrade = null;
                let latestExitTime = 0;

                trades.forEach((trade: any) => {
                    const fundingCount = Number(trade["펀딩 횟수"] || 0);
                    const exitTime = new Date(trade["청산 시간"] as string).getTime();

                    // 펀딩 횟수가 더 크거나, 같으면서 청산 시간이 더 늦을 때 업데이트
                    if (fundingCount > maxFundingCount ||
                        (fundingCount === maxFundingCount && exitTime > latestExitTime)) {
                        maxFundingCount = fundingCount;
                        maxFundingTrade = trade;
                        latestExitTime = exitTime;
                    }
                });

                if (maxFundingCount > 0 && maxFundingTrade) {
                    const timestamp = new Date(maxFundingTrade["청산 시간"] as string).getTime() / 1000;
                    fundingTimeSet.add(timestamp);
                }
            });
        });

        // 시간 배열로 변환 및 정렬
        const fundingTimes = Array.from(fundingTimeSet).sort((a, b) => a - b);
        const allFundingTimes = getTimesWithZeroPoint(fundingTimes);

        // config의 모든 심볼을 미리 초기화하고 0 포인트 명시적 추가
        configSymbols.forEach((symbol: string) => {
            const data: { time: number; value: number }[] = [];
            if (timeframePeriod && allFundingTimes.length > 0) {
                data.push({time: allFundingTimes[0], value: 0});
            }
            symbolDataArray.push({symbol, data});
        });

        // 거래 번호별로 그룹화하여 펀딩 횟수 계산
        configSymbols.forEach((symbol: string) => {
            const symbolTrades = preprocessedData.tradesBySymbol.get(symbol) || [];
            const symbolData = symbolDataArray.find(item => item.symbol === symbol);

            if (symbolData) {
                let totalFundingCount = 0;

                // 거래 번호별로 그룹화
                const tradesByNumber = new Map<number, any[]>();
                symbolTrades.forEach((trade: any) => {
                    const tradeNumber = trade["거래 번호"] as number;
                    if (!tradesByNumber.has(tradeNumber)) {
                        tradesByNumber.set(tradeNumber, []);
                    }
                    tradesByNumber.get(tradeNumber)!.push(trade);
                });

                // 각 거래 번호별로 최대 펀딩 횟수를 찾아 누적
                tradesByNumber.forEach((trades: any[]) => {
                    if (trades.length === 0) return;

                    // 해당 거래 번호 내에서 펀딩 횟수의 최대값과 해당 거래 찾기
                    let maxFundingCount = 0;
                    let maxFundingTrade = null;
                    let latestExitTime = 0;

                    trades.forEach((trade: any) => {
                        const fundingCount = Number(trade["펀딩 횟수"] || 0);
                        const exitTime = new Date(trade["청산 시간"] as string).getTime();

                        // 펀딩 횟수가 더 크거나, 같으면서 청산 시간이 더 늦을 때 업데이트
                        if (fundingCount > maxFundingCount ||
                            (fundingCount === maxFundingCount && exitTime > latestExitTime)) {
                            maxFundingCount = fundingCount;
                            maxFundingTrade = trade;
                            latestExitTime = exitTime;
                        }
                    });

                    if (maxFundingCount > 0 && maxFundingTrade) {
                        totalFundingCount += maxFundingCount;

                        // 가장 큰 펀딩 횟수를 가진 거래들 중 가장 늦은 청산 시간 사용
                        const timestamp = new Date(maxFundingTrade["청산 시간"] as string).getTime() / 1000;

                        symbolData.data.push({
                            time: timestamp,
                            value: totalFundingCount
                        });
                    }
                });
            }
        });

        return {allTimes: allFundingTimes, symbolDataArray};
    };

    // 펀딩비 데이터 생성
    const generateFundingFeeData = () => {
        const allExitTimes = getTimesWithZeroPoint(preprocessedData.allExitTimes);
        const symbolDataArray: { symbol: string; data: { time: number; value: number }[] }[] = [];

        // config의 모든 심볼을 미리 초기화하고 0 포인트 명시적 추가
        configSymbols.forEach((symbol: string) => {
            const data: { time: number; value: number }[] = [];
            if (timeframePeriod && allExitTimes.length > 0) {
                data.push({time: allExitTimes[0], value: 0});
            }
            symbolDataArray.push({symbol, data});
        });

        // 심볼별 펀딩비 누적 계산
        configSymbols.forEach((symbol: string) => {
            const symbolTrades = preprocessedData.tradesBySymbol.get(symbol) || [];
            const symbolData = symbolDataArray.find(item => item.symbol === symbol);

            if (symbolData) {
                // 청산 시간 기준으로 정렬
                const sortedTrades = [...symbolTrades].sort((a, b) => {
                    const timeA = new Date(a["청산 시간"] as string).getTime();
                    const timeB = new Date(b["청산 시간"] as string).getTime();
                    return timeA - timeB;
                });

                let totalFundingFee = 0;

                sortedTrades.forEach((trade: any) => {
                    const fundingFee = Number(trade["펀딩비"] || 0);
                    totalFundingFee += fundingFee;
                    const timestamp = new Date(trade["청산 시간"] as string).getTime() / 1000;

                    symbolData.data.push({
                        time: timestamp,
                        value: totalFundingFee
                    });
                });
            }
        });

        return {allTimes: allExitTimes, symbolDataArray};
    };

    // 보간 데이터 생성 함수 - 최적화된 버전
    const generateInterpolatedData = useCallback((symbolData: {
        time: number;
        value: number
    }[], allTimes: number[]) => {
        if (symbolData.length === 0) return allTimes.map(time => ({time, value: 0}));

        const interpolatedData: { time: number; value: number }[] = [];
        let currentIndex = 0;
        let currentValue = 0; // 항상 0에서 시작

        for (const time of allTimes) {
            // 현재 시간보다 작거나 같은 데이터가 있으면 값 업데이트
            while (currentIndex < symbolData.length && symbolData[currentIndex].time <= time) {
                currentValue = symbolData[currentIndex].value;
                currentIndex++;
            }

            interpolatedData.push({time, value: currentValue});
        }

        return interpolatedData;
    }, []);

    // 툴팁 관련 refs 추가
    const timeAxisLabelRef = useRef<HTMLDivElement | null>(null);
    const priceAxisLabelRef = useRef<HTMLDivElement | null>(null);
    const tooltipRef = useRef<HTMLDivElement | null>(null);
    const mainSeriesRef = useRef<any | null>(null); // 메인 시리즈 ref 추가 (EquityCurve와 동일)
    const seriesRefs = useRef<Map<string, any>>(new Map()); // 모든 시리즈 ref 저장
    const symbolsRef = useRef<string[]>(configSymbols); // 심볼 목록 ref 추가
    const colorsRef = useRef<string[]>([]); // 색상 목록 ref 추가
    const isComponentMounted = useRef(true);
    const hasTooltipAppeared = useRef(false);

    // Rainbow 색상 생성 함수
    const generateRainbowColors = (count: number): string[] => {
        const colors: string[] = [];
        for (let i = 0; i < count; i++) {
            const hue = (i * 360) / count;
            colors.push(`hsl(${hue}, 70%, 60%)`);
        }
        return colors;
    };

    // 툴팁 숨김 함수 추가
    const hideTooltips = useCallback(() => {
        if (!timeAxisLabelRef.current || !priceAxisLabelRef.current || !tooltipRef.current) return;

        const timeAxisLabel = timeAxisLabelRef.current;
        const priceAxisLabel = priceAxisLabelRef.current;
        const tooltip = tooltipRef.current;

        timeAxisLabel.style.display = 'none';
        priceAxisLabel.style.display = 'none';
        tooltip.style.opacity = '0';
        tooltip.style.transform = 'translate3d(0, 0, 0) scale(0.95)';

        // 애니메이션 완료 후 visibility 숨김
        setTimeout(() => {
            if (tooltipRef.current) {
                tooltipRef.current.style.visibility = 'hidden';
            }
        }, 200); // 페이드 아웃 애니메이션 시간과 일치시킴
    }, []);

    // 크로스헤어 이동 핸들러 추가 (EquityCurve와 동일)
    const handleCrosshairMove = useCallback((param: any) => {
        if (!isComponentMounted.current || !chartRef.current || !timeAxisLabelRef.current || !priceAxisLabelRef.current || !tooltipRef.current) return;

        const timeAxisLabel = timeAxisLabelRef.current;
        const priceAxisLabel = priceAxisLabelRef.current;
        const tooltip = tooltipRef.current;

        // param.point (마우스 좌표)가 없으면 숨김
        if (param.point === undefined) {
            hideTooltips();
            return;
        }

        // 커스텀 축 레이블 업데이트
        if (timeAxisLabel && param.time) {
            const date = new Date(Number(param.time) * 1000);
            const pad = (num: number) => String(num).padStart(2, '0');
            const yy = String(date.getFullYear());
            const mm = pad(date.getMonth() + 1);
            const dd = pad(date.getDate());
            const hh = pad(date.getHours());
            const mi = pad(date.getMinutes());
            const ss = pad(date.getSeconds());

            // 요일 계산 추가
            const days = ['일', '월', '화', '수', '목', '금', '토'];
            const dayOfWeek = days[date.getDay()];

            // 시간 포맷에 요일 추가
            timeAxisLabel.innerHTML = `${yy}-${mm}-${dd} ${hh}:${mi}:${ss} (${dayOfWeek})`;
            timeAxisLabel.style.display = 'block';
            timeAxisLabel.style.bottom = 'auto';

            const container = chartContainerRef.current;
            const timeScaleHeight = chartRef.current?.timeScale().height();

            if (container && timeScaleHeight !== undefined) {
                const yOffset = 2;
                timeAxisLabel.style.top = `${container.clientHeight - timeScaleHeight + yOffset}px`;
            } else {
                timeAxisLabel.style.bottom = '8px';
            }

            // 툴팁 위치 계산
            const labelWidth = timeAxisLabel.offsetWidth;
            const containerForLabel = chartContainerRef.current;
            let labelX = param.point.x;
            if (containerForLabel) {
                const containerWidth = containerForLabel.clientWidth;
                // 오른쪽 경계에서 label이 넘지 않게 고정
                labelX = Math.min(param.point.x, containerWidth - labelWidth);
                // 왼쪽 경계도 체크
                labelX = Math.max(labelX, 0);
            }

            const leftPriceScaleWidth = chartRef.current?.priceScale('left').width() || 0;
            if (labelX < leftPriceScaleWidth) {
                labelX = leftPriceScaleWidth;
            }

            timeAxisLabel.style.left = `${labelX}px`;
        }

        if (priceAxisLabel && param.point) {
            // EquityCurve와 동일한 방식으로 가격 찾기
            let price = null;

            if (mainSeriesRef.current && param.point) {
                price = mainSeriesRef.current.coordinateToPrice(param.point.y);
            }

            if (price !== null && price !== undefined) {
                // y축 툴팁 숨김 조건
                if (
                    ((selectedMetric === '진입 횟수' || selectedMetric === '강제 청산 횟수' || selectedMetric === '펀딩 횟수' || selectedMetric === '승률' || selectedMetric === '손익비' || selectedMetric === '보유 시간 합계') && price < 0) ||
                    (selectedMetric === '승률' && price > 100)
                ) {
                    priceAxisLabel.style.display = 'none';
                } else {
                    priceAxisLabel.style.display = 'block';
                    priceAxisLabel.style.top = `${param.point.y - priceAxisLabel.offsetHeight / 2}px`;
                    priceAxisLabel.style.right = 'auto';

                    const priceScaleWidth = chartRef.current?.priceScale('left').width();
                    if (priceScaleWidth !== undefined) {
                        priceAxisLabel.style.left = `${priceScaleWidth - priceAxisLabel.offsetWidth - 2}px`;
                    } else {
                        priceAxisLabel.style.left = '3px';
                    }

                    if (selectedMetric === '누적 순손익') {
                        const absPrice = Math.abs(price);
                        const sign = price >= 0 ? '$' : '-$';
                        priceAxisLabel.innerHTML = `${sign}${Number(absPrice.toFixed(2)).toLocaleString('en-US', {
                            minimumFractionDigits: 2,
                            maximumFractionDigits: 2
                        })}`;
                    } else {
                        priceAxisLabel.innerHTML = getFormatFunction(selectedMetric, timeframeStr)(price);
                    }
                    priceAxisLabel.style.color = '#ffffff';
                }
            } else {
                priceAxisLabel.style.display = 'none';
            }
        }

        if (tooltip && param.time) {
            // 전체 시리즈에서 모든 심볼의 y값 가져오기
            const allSeriesValues: { symbol: string; value: number; color: string }[] = [];

            // 차트의 모든 시리즈 가져오기 (Lightweight Charts 방식)
            if (chartRef.current && param.time) {
                // 저장된 모든 시리즈 ref에서 해당 시간의 실제 데이터 값 가져오기 (순서 보장)
                symbolsRef.current.forEach((symbol, index) => {
                    try {
                        const seriesRef = seriesRefs.current.get(symbol);
                        if (seriesRef) {
                            // 해당 시간의 실제 데이터 포인트 찾기
                            const data = seriesRef.data();
                            if (data && data.length > 0) {
                                // 가장 가까운 시간의 데이터 포인트 찾기
                                let closestPoint = null;
                                let minTimeDiff = Infinity;

                                for (const point of data) {
                                    const timeDiff = Math.abs(point.time - param.time);
                                    if (timeDiff < minTimeDiff) {
                                        minTimeDiff = timeDiff;
                                        closestPoint = point;
                                    }
                                }

                                if (closestPoint && closestPoint.value !== undefined) {
                                    const color = colorsRef.current[index] || '#ffffff';
                                    allSeriesValues.push({symbol, value: closestPoint.value, color});
                                }
                            } else {
                                // 데이터가 없으면 0으로 표시
                                const color = colorsRef.current[index] || '#ffffff';
                                allSeriesValues.push({symbol, value: 0, color});
                            }
                        }
                    } catch (e) {
                        console.error(`시리즈 ${symbol}에서 데이터 가져오기 오류:`, e);
                        // 에러가 나도 0으로 표시
                        const color = colorsRef.current[index] || '#ffffff';
                        allSeriesValues.push({symbol, value: 0, color});
                    }
                });
            }

            if (allSeriesValues.length > 0) {
                // 심볼 개수에 따라 열 수 결정
                const symbolCount = allSeriesValues.length;
                let columnCount = 1;
                if (symbolCount > 30) columnCount = 4;
                else if (symbolCount > 20) columnCount = 3;
                else if (symbolCount > 10) columnCount = 2;

                // 각 열별로 심볼 분할
                const symbolsPerColumn = Math.ceil(symbolCount / columnCount);
                const columns = [];
                for (let i = 0; i < columnCount; i++) {
                    const start = i * symbolsPerColumn;
                    const end = Math.min(start + symbolsPerColumn, symbolCount);
                    columns.push(allSeriesValues.slice(start, end));
                }

                // 각 열의 HTML 생성
                const columnHtmls = columns.map(column =>
                    column.map(({symbol, value, color}) => {
                        let valueColor;
                        let displayValue = value;

                        // 손익비일 때 무한대 값 처리를 위한 특별 계산
                        if (selectedMetric === '손익비') {
                            // 해당 시점까지의 실제 거래 데이터로 손익비 계산
                            const currentTime = param.time;
                            const symbolTrades = preprocessedData.tradesBySymbol.get(symbol) || [];

                            // 현재 시점까지의 거래만 필터링
                            const tradesUpToNow = symbolTrades.filter((trade: any) => {
                                const exitTime = new Date(trade["청산 시간"] as string).getTime() / 1000;
                                return exitTime <= currentTime;
                            });

                            if (tradesUpToNow.length > 0) {
                                // 거래 번호별로 그룹화하여 순손익 계산
                                const tradesByNumber = new Map<number, any[]>();
                                tradesUpToNow.forEach((trade: any) => {
                                    const tradeNumber = trade["거래 번호"] as number;
                                    if (!tradesByNumber.has(tradeNumber)) {
                                        tradesByNumber.set(tradeNumber, []);
                                    }
                                    tradesByNumber.get(tradeNumber)!.push(trade);
                                });

                                const wins: number[] = [];
                                const losses: number[] = [];

                                tradesByNumber.forEach((trades: any[]) => {
                                    const totalPnl = trades.reduce((sum: number, trade: any) => sum + (trade["순손익"] as number), 0);
                                    if (totalPnl >= 0) {
                                        wins.push(totalPnl);
                                    } else {
                                        losses.push(Math.abs(totalPnl));
                                    }
                                });

                                const avgWin = wins.length > 0 ? wins.reduce((a, b) => a + b, 0) / wins.length : 0;
                                const avgLoss = losses.length > 0 ? losses.reduce((a, b) => a + b, 0) / losses.length : 0;

                                // 무한대 여부 판단
                                if (avgLoss === 0 && avgWin > 0) {
                                    displayValue = Infinity; // 툴팁에서는 무한대로 표시
                                } else if (avgLoss > 0) {
                                    displayValue = avgWin / avgLoss;
                                } else {
                                    displayValue = 0;
                                }
                            }
                        }

                        if (selectedMetric === '진입 횟수' || selectedMetric === '강제 청산 횟수' || selectedMetric === '펀딩 횟수' || selectedMetric === '승률' || selectedMetric === '손익비' || selectedMetric === '보유 시간 합계') {
                            // 진입 횟수, 승률, 강제 청산 횟수, 펀딩 횟수, 손익비일 때 이전 값과 비교
                            let previousValue = 0;
                            let currentValue = value;

                            // 손익비일 때는 실제 계산된 displayValue 사용
                            if (selectedMetric === '손익비') {
                                currentValue = displayValue;

                                // 이전 시점의 손익비도 실제 계산
                                const currentTime = param.time;
                                const symbolTrades = preprocessedData.tradesBySymbol.get(symbol) || [];

                                // 현재 시점보다 이전의 가장 가까운 거래 시점 찾기
                                const exitTimes = symbolTrades
                                    .map((trade: any) => new Date(trade["청산 시간"] as string).getTime() / 1000)
                                    .filter((time: number) => time < currentTime)
                                    .sort((a: number, b: number) => b - a); // 내림차순 정렬

                                if (exitTimes.length > 0) {
                                    const previousTime = exitTimes[0];

                                    // 이전 시점까지의 거래만 필터링
                                    const tradesUpToPrevious = symbolTrades.filter((trade: any) => {
                                        const exitTime = new Date(trade["청산 시간"] as string).getTime() / 1000;
                                        return exitTime <= previousTime;
                                    });

                                    if (tradesUpToPrevious.length > 0) {
                                        // 거래 번호별로 그룹화하여 순손익 계산
                                        const tradesByNumber = new Map<number, any[]>();
                                        tradesUpToPrevious.forEach((trade: any) => {
                                            const tradeNumber = trade["거래 번호"] as number;
                                            if (!tradesByNumber.has(tradeNumber)) {
                                                tradesByNumber.set(tradeNumber, []);
                                            }
                                            tradesByNumber.get(tradeNumber)!.push(trade);
                                        });

                                        const wins: number[] = [];
                                        const losses: number[] = [];

                                        tradesByNumber.forEach((trades: any[]) => {
                                            const totalPnl = trades.reduce((sum: number, trade: any) => sum + (trade["순손익"] as number), 0);
                                            if (totalPnl >= 0) {
                                                wins.push(totalPnl);
                                            } else {
                                                losses.push(Math.abs(totalPnl));
                                            }
                                        });

                                        const avgWin = wins.length > 0 ? wins.reduce((a, b) => a + b, 0) / wins.length : 0;
                                        const avgLoss = losses.length > 0 ? losses.reduce((a, b) => a + b, 0) / losses.length : 0;

                                        // 이전 손익비 계산
                                        if (avgLoss === 0 && avgWin > 0) {
                                            previousValue = Infinity;
                                        } else if (avgLoss > 0) {
                                            previousValue = avgWin / avgLoss;
                                        } else {
                                            previousValue = 0;
                                        }
                                    }
                                }
                            } else {
                                // 다른 메트릭들은 기존 방식 사용
                                const seriesRef = seriesRefs.current.get(symbol);
                                if (seriesRef) {
                                    const data = seriesRef.data();
                                    if (data && data.length > 1) {
                                        // 현재 시간보다 작은 가장 가까운 이전 데이터 포인트 찾기
                                        const currentTime = param.time;
                                        let closestPrevious = null;

                                        for (let i = data.length - 1; i >= 0; i--) {
                                            if (data[i].time < currentTime) {
                                                closestPrevious = data[i];
                                                break;
                                            }
                                        }

                                        if (closestPrevious) {
                                            previousValue = closestPrevious.value;
                                        }
                                    }
                                }
                            }

                            // 승률과 손익비는 증가/감소에 따라 색상 결정
                            if (selectedMetric === '승률' || selectedMetric === '손익비') {
                                // 무한대 처리를 포함한 비교
                                let isIncreased = false;
                                let isDecreased = false;

                                if (previousValue === Infinity && currentValue === Infinity) {
                                    // 둘 다 무한대면 변화 없음
                                    isIncreased = false;
                                    isDecreased = false;
                                } else if (previousValue === Infinity && currentValue !== Infinity) {
                                    // 무한대에서 일반 값으로: 감소
                                    isDecreased = true;
                                } else if (previousValue !== Infinity && currentValue === Infinity) {
                                    // 일반 값에서 무한대로: 증가
                                    isIncreased = true;
                                } else {
                                    // 둘 다 일반 값
                                    isIncreased = currentValue > previousValue;
                                    isDecreased = currentValue < previousValue;
                                }

                                if (isIncreased) {
                                    valueColor = '#4caf50'; // 녹색 (증가)
                                } else if (isDecreased) {
                                    valueColor = '#f23645'; // 빨간색 (감소)
                                } else {
                                    valueColor = '#ffffff'; // 흰색 (변화 없음)
                                }
                            } else {
                                // 진입 횟수, 강제 청산 횟수, 펀딩 횟수, 보유 시간 합계는 변화만 감지
                                const hasChanged = currentValue !== previousValue;
                                valueColor = hasChanged ? '#4caf50' : '#ffffff';
                            }
                        } else {
                            // 다른 메트릭은 기존 색상 사용
                            valueColor = value > 0 ? '#4caf50' : value == 0 ? '#ffffff' : '#f23645';
                        }

                        // 펀딩 관련 추가 정보 계산
                        let additionalInfo = '';
                        if (selectedMetric === '펀딩 횟수' || selectedMetric === '펀딩비') {
                            const currentTime = param.time;
                            const symbolTrades = preprocessedData.tradesBySymbol.get(symbol) || [];

                            // 현재 시점까지의 거래만 필터링
                            const tradesUpToNow = symbolTrades.filter((trade: any) => {
                                const exitTime = new Date(trade["청산 시간"] as string).getTime() / 1000;
                                return exitTime <= currentTime;
                            });

                            if (selectedMetric === '펀딩 횟수') {
                                // 거래 번호별로 그룹화하여 각 거래 번호에서 가장 큰 펀딩 횟수를 가진 거래의 수령/지불 횟수만 누적
                                let totalReceiveCount = 0;
                                let totalPayCount = 0;

                                // 거래 번호별로 그룹화
                                const tradesByNumber = new Map<number, any[]>();
                                tradesUpToNow.forEach((trade: any) => {
                                    const tradeNumber = trade["거래 번호"] as number;
                                    if (!tradesByNumber.has(tradeNumber)) {
                                        tradesByNumber.set(tradeNumber, []);
                                    }
                                    tradesByNumber.get(tradeNumber)!.push(trade);
                                });

                                // 각 거래 번호별로 최대 펀딩 횟수를 가진 거래의 수령/지불 횟수만 합산
                                tradesByNumber.forEach((trades: any[]) => {
                                    if (trades.length === 0) return;

                                    let maxFundingCount = 0;
                                    let maxFundingTrade = null;

                                    trades.forEach((trade: any) => {
                                        const fundingCount = Number(trade["펀딩 횟수"] || 0);
                                        if (fundingCount > maxFundingCount) {
                                            maxFundingCount = fundingCount;
                                            maxFundingTrade = trade;
                                        }
                                    });

                                    if (maxFundingTrade) {
                                        const receiveCount = Number(maxFundingTrade["펀딩 수령 횟수"] || 0);
                                        const payCount = Number(maxFundingTrade["펀딩 지불 횟수"] || 0);
                                        totalReceiveCount += receiveCount;
                                        totalPayCount += payCount;
                                    }
                                });

                                // 천단위 쉼표 적용
                                const formatCount = (count: number) => count.toLocaleString('en-US');
                                additionalInfo = `<div style="margin-top: 2px; font-size: 11px; color: #ccc;">
                                    수령: ${formatCount(totalReceiveCount)}회, 지불: ${formatCount(totalPayCount)}회
                                </div>`;
                            } else if (selectedMetric === '펀딩비') {
                                // 펀딩비 수령과 펀딩비 지불 누적 계산
                                let totalReceiveFee = 0;
                                let totalPayFee = 0;

                                tradesUpToNow.forEach((trade: any) => {
                                    const receiveFee = Number(trade["펀딩비 수령"] || 0);
                                    const payFee = Number(trade["펀딩비 지불"] || 0);
                                    totalReceiveFee += receiveFee;
                                    totalPayFee += payFee;
                                });

                                const formatFee = (fee: number) => {
                                    const absPrice = Math.abs(fee);
                                    const sign = fee >= 0 ? '$' : '-$';
                                    return `${sign}${Number(absPrice.toFixed(2)).toLocaleString('en-US', {
                                        minimumFractionDigits: 2,
                                        maximumFractionDigits: 2
                                    })}`;
                                };

                                additionalInfo = `<div style="margin-top: 2px; font-size: 11px; color: #ccc;">
                                    수령: ${formatFee(totalReceiveFee)}, 지불: ${formatFee(totalPayFee)}
                                </div>`;
                            }
                        }

                        return `
            <div style="display: flex; justify-content: space-between; align-items: baseline; margin-bottom: 4px;">
              <span style="color: ${color}; font-size: 13px; padding: 0 6px; margin-right: 8px; position: relative; left: -6px;">${symbol}</span>
              <div style="text-align: right;">
                <strong style="color: ${valueColor}; font-weight: 600; font-size: 14px;">${getTooltipFormatFunction(selectedMetric, timeframeStr)(displayValue)}</strong>
                ${additionalInfo}
              </div>
            </div>
          `;
                    }).join('')
                );

                // 구분선과 열 HTML 조합
                const columnElements = [];
                for (let i = 0; i < columnCount; i++) {
                    columnElements.push(`<div style="flex: 1;">${columnHtmls[i]}</div>`);
                    if (i < columnCount - 1) {
                        columnElements.push(`<div style="border-right: 1px solid rgba(255, 215, 0, 0.3); margin: 4px 15px 8px;"></div>`);
                    }
                }

                tooltip.innerHTML = `
          <div style="display: flex; align-items: stretch; margin: 0;">
            ${columnElements.join('')}
          </div>
        `;

                // 가장 가까운 데이터 포인트의 위치를 기준으로 툴팁 위치 조정
                const pointX = param.point.x;
                const pointY = param.point.y;

                requestAnimationFrame(() => {
                    if (!isComponentMounted.current || !tooltipRef.current) return;

                    // y축(왼쪽 price scale) width 구하기
                    const leftPriceScaleWidth = chartRef.current?.priceScale('left').width() || 0;
                    const container = chartContainerRef.current;

                    // 위치 업데이트 (차트 내부 기준)
                    let finalX = leftPriceScaleWidth + pointX + 25; // y축 오른쪽에서 시작
                    let finalY = pointY + 25;

                    if (container) {
                        const containerRect = container.getBoundingClientRect();
                        const tooltipWidth = tooltip.offsetWidth;
                        const tooltipHeight = tooltip.offsetHeight;
                        const chartWidth = containerRect.width;

                        // 오른쪽 경계 체크 (툴팁이 차트 내부에서만 왼쪽 전환)
                        const rightEdge = finalX + tooltipWidth;
                        if (rightEdge > chartWidth) {
                            finalX = leftPriceScaleWidth + pointX - tooltipWidth - 25;
                        }

                        // 왼쪽 경계 체크 (툴팁이 y축보다 왼쪽으로 안가게)
                        if (finalX < leftPriceScaleWidth) {
                            finalX = leftPriceScaleWidth;
                        }

                        // 아래쪽 경계 체크
                        const bottomEdge = finalY + tooltipHeight;
                        const timeScaleHeight = chartRef.current?.timeScale().height() ?? 0;
                        const chartBottomBoundary = containerRect.height - timeScaleHeight;
                        if (bottomEdge > chartBottomBoundary) {
                            finalY = pointY - tooltipHeight - 25;
                        }
                    }

                    tooltip.style.left = `${finalX}px`;
                    tooltip.style.top = `${finalY}px`;

                    // 트랜지션 동적 설정 (위치 설정 후)
                    if (!hasTooltipAppeared.current) {
                        // 첫 등장: top/left 애니메이션 없음
                        tooltip.style.transition = 'opacity 0.2s ease-in-out, transform 0.2s cubic-bezier(0.23, 1, 0.32, 1)';
                        hasTooltipAppeared.current = true; // 첫 등장 플래그 설정 (스타일 적용 직후)
                    } else {
                        // 이후 등장: top/left 애니메이션 포함
                        tooltip.style.transition = 'opacity 0.2s ease-in-out, top 0.25s cubic-bezier(0.23, 1, 0.32, 1), left 0.25s cubic-bezier(0.23, 1, 0.32, 1), transform 0.2s cubic-bezier(0.23, 1, 0.32, 1)';
                    }

                    // 툴팁 표시 및 애니메이션 효과 적용
                    tooltip.style.visibility = 'visible'; // visibility를 먼저 변경
                    tooltip.style.opacity = '1';
                    tooltip.style.transform = 'translate3d(0, 0, 0) scale(1)';
                });
            } else {
                tooltip.style.opacity = '0';
                tooltip.style.transform = 'translate3d(0, 0, 0) scale(0.95)';
                hasTooltipAppeared.current = false;
                setTimeout(() => {
                    if (tooltipRef.current) {
                        tooltipRef.current.style.visibility = 'hidden';
                    }
                }, 200);
            }
        }
    }, [hideTooltips, selectedMetric, timeframeStr]);

    useEffect(() => {
        if (!chartContainerRef.current || loading || !filteredTrades || filteredTrades.length === 0) return;

        // Clean up previous chart and tooltips
        if (chartRef.current) {
            // 이벤트 구독 해제를 먼저
            try {
                chartRef.current.unsubscribeCrosshairMove(handleCrosshairMove);
            } catch (error) {
                console.error("이벤트 해제 오류:", error);
            }

            // 기존 툴팁들 정리
            if (timeAxisLabelRef.current) {
                timeAxisLabelRef.current.remove();
                timeAxisLabelRef.current = null;
            }
            if (priceAxisLabelRef.current) {
                priceAxisLabelRef.current.remove();
                priceAxisLabelRef.current = null;
            }
            if (tooltipRef.current) {
                tooltipRef.current.remove();
                tooltipRef.current = null;
            }

            chartRef.current.remove();
            chartRef.current = null;
        }

        // 툴팁 상태 리셋
        hasTooltipAppeared.current = false;


        const width = chartContainerRef.current.clientWidth || 400;
        const height = chartContainerRef.current.clientHeight || 300;

        // 진입 횟수일 때는 다른 priceScale 옵션 적용
        const formatFunction = getFormatFunction(selectedMetric);
        const priceScaleOptions = {
            borderColor: '#ffffff',
            scaleMargins: {
                top: 0.1,
                bottom: 0.1,
            },
            mode: PriceScaleMode.Normal,
            borderVisible: true,
            visible: true,
            entireTextOnly: true,
            format: {
                type: 'custom',
                formatter: formatFunction,
            }
        };

        const chart = createChart(chartContainerRef.current, {
            width,
            height,
            layout: {
                background: {type: ColorType.Solid, color: 'transparent'},
                textColor: '#ffffff',
                fontSize: 14,
                fontFamily: "'Inter', 'Pretendard', sans-serif",
            },
            grid: {
                vertLines: {visible: false},
                horzLines: {visible: false},
            },
            leftPriceScale: priceScaleOptions,
            rightPriceScale: {
                visible: false,
            },
            timeScale: {
                borderColor: '#ffffff',
                timeVisible: true,
                secondsVisible: true,
                borderVisible: true,
                fixLeftEdge: true,
                fixRightEdge: true,
                lockVisibleTimeRangeOnResize: true,
                allowBoldLabels: false,
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
            }
        });
        chartRef.current = chart;

        // 축 옵션 적용
        chart.priceScale('left').applyOptions({
            ticksVisible: true,
            minimumWidth: selectedMetric === '보유 시간 합계' || selectedMetric === '승률' || selectedMetric === '펀딩 횟수' ? 90 : 0,
        });

        chart.timeScale().applyOptions({
            ticksVisible: true,
        });

        // 커스텀 축 레이블 추가
        const timeAxisLabel = document.createElement('div');
        timeAxisLabel.style.position = 'absolute';
        timeAxisLabel.style.display = 'none';
        timeAxisLabel.style.top = 'auto';
        timeAxisLabel.style.backgroundColor = 'rgba(28, 28, 36, 0.95)';
        timeAxisLabel.style.color = '#ffffff';
        timeAxisLabel.style.padding = '2px 7px 3px 7px';
        timeAxisLabel.style.borderRadius = '3px';
        timeAxisLabel.style.fontSize = '14px';
        timeAxisLabel.style.fontWeight = 'normal';
        timeAxisLabel.style.zIndex = '60';
        timeAxisLabel.style.border = '1px solid rgba(255, 215, 0, 0.4)';
        timeAxisLabel.style.whiteSpace = 'nowrap';
        timeAxisLabel.style.fontFamily = "'Inter', 'Pretendard', sans-serif";
        chartContainerRef.current.appendChild(timeAxisLabel);
        timeAxisLabelRef.current = timeAxisLabel;

        const priceAxisLabel = document.createElement('div');
        priceAxisLabel.style.position = 'absolute';
        priceAxisLabel.style.display = 'none';
        priceAxisLabel.style.right = 'auto';
        priceAxisLabel.style.backgroundColor = 'rgba(28, 28, 36, 0.95)';
        priceAxisLabel.style.color = '#ffffff';
        priceAxisLabel.style.padding = '2px 7px 3px 7px';
        priceAxisLabel.style.borderRadius = '3px';
        priceAxisLabel.style.fontSize = '14px';
        priceAxisLabel.style.fontWeight = 'normal';
        priceAxisLabel.style.zIndex = '51';
        priceAxisLabel.style.border = '1px solid rgba(255, 215, 0, 0.4)';
        priceAxisLabel.style.whiteSpace = 'nowrap';
        priceAxisLabel.style.fontFamily = "'Inter', 'Pretendard', sans-serif";
        chartContainerRef.current.appendChild(priceAxisLabel);
        priceAxisLabelRef.current = priceAxisLabel;

        // 마우스를 따라다니는 툴팁 추가
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
        tooltip.style.zIndex = '100';
        tooltip.style.fontFamily = "'Inter', 'Pretendard', sans-serif"; // 폰트 적용 (기존에도 있었지만 명시적으로 다시 설정)
        tooltip.style.lineHeight = '1.6';
        tooltip.style.opacity = '0';
        tooltip.style.visibility = 'hidden'; // 초기에는 완전히 숨김
        tooltip.style.transform = 'translate3d(0, 0, 0) scale(0.95)'; // 하드웨어 가속 활성화 및 초기 약간 작게 시작
        tooltip.style.left = '-9999px'; // 화면 바깥으로 초기 위치 이동
        tooltip.style.top = '-9999px'; // 화면 바깥으로 초기 위치 이동
        tooltip.style.whiteSpace = 'nowrap'; // 줄바꿈 방지 추가
        chartContainerRef.current.appendChild(tooltip);
        tooltipRef.current = tooltip;

        // selectedMetric에 따라 데이터 생성
        const result = generateDataForMetric(selectedMetric);
        const {allTimes, symbolDataArray} = result;

        if (allTimes.length === 0) return;

        const startTime = allTimes[0];
        const endTime = allTimes[allTimes.length - 1];


        // 심볼별 라인 시리즈 생성 (보간 적용) - 순서 보장
        const symbols = configSymbols; // config 순서가 보장된 배열 사용
        const colors = generateRainbowColors(symbols.length);

        // ref에 저장 (툴팁에서 사용)
        symbolsRef.current = symbols;
        colorsRef.current = colors;

        symbols.forEach((symbol: string, index: number) => {
            const symbolDataItem = symbolDataArray.find((item: {
                symbol: string;
                data: { time: number; value: number }[]
            }) => item.symbol === symbol);
            const data = symbolDataItem?.data || [];

            // 모든 심볼 추가 (데이터가 없어도 0으로 초기화)
            let interpolatedData;
            if (data.length === 0) {
                // 데이터가 없으면 0으로 초기화된 데이터 생성
                interpolatedData = allTimes.map((time: number) => ({time, value: 0}));
            } else {
                // 실제 거래 시간에 대한 보간 데이터 생성
                interpolatedData = generateInterpolatedData(data, allTimes);
            }

            const series = chart.addSeries(LineSeries, {
                color: colors[index],
                lineWidth: 2,
                lastValueVisible: false,
                priceLineVisible: false,
                priceFormat: {
                    type: 'custom',
                    formatter: getFormatFunction(selectedMetric, timeframeStr),
                },
            });

            series.setData(interpolatedData.map((item: { time: number; value: number }) => ({
                time: item.time as any,
                value: item.value
            })));

            // 시리즈 ref 저장
            seriesRefs.current.set(symbol, series);

            // 첫 번째 시리즈를 메인 시리즈로 설정 (EquityCurve와 동일)
            if (index === 0) {
                mainSeriesRef.current = series;
            }
        });

        // 초기 라인 (0선) 추가 - 항상 제일 위에 그려지도록 마지막에 추가
        const initialLineData = [
            {time: startTime as any, value: 0},
            {time: endTime as any, value: 0},
        ];
        const initialLine = chart.addSeries(LineSeries, {
            color: 'rgb(255, 255, 255)',
            lineWidth: 2,
            lineStyle: LineStyle.Dashed,
            lastValueVisible: false,
            priceLineVisible: false,
            crosshairMarkerVisible: false,
            priceFormat: {
                type: 'custom',
                formatter: getFormatFunction(selectedMetric, timeframeStr),
            },
        });
        initialLine.setData(initialLineData);

        // Resize observer for responsiveness
        const resizeObserver = new window.ResizeObserver(entries => {
            for (let entry of entries) {
                const {width, height} = entry.contentRect;
                chart.applyOptions({width, height});
            }
        });
        resizeObserver.observe(chartContainerRef.current);

        // 타임스케일 틱 스타일 설정 (EquityCurve와 동일)
        chart.timeScale().applyOptions({
            borderVisible: true,
            barSpacing: 8,
            timeVisible: true,
            secondsVisible: true,
            fixLeftEdge: true,
            fixRightEdge: true,
            rightOffset: 5,
        });

        // 초기 화면을 제일 축소한 상태로 설정 (마지막에 호출)
        chart.timeScale().fitContent();

        // 이벤트 구독
        chart.subscribeCrosshairMove(handleCrosshairMove);

        // 컴포넌트 마운트 상태 설정
        isComponentMounted.current = true;

        return () => {
            isComponentMounted.current = false;

            // 이벤트 핸들러 등록 해제를 가장 먼저
            if (chartRef.current) {
                try {
                    chartRef.current.unsubscribeCrosshairMove(handleCrosshairMove);
                } catch (error) {
                    console.error("이벤트 해제 오류:", error);
                }
            }

            // 차트와 툴팁 즉시 제거
            try {
                if (chartRef.current) {
                    chartRef.current.remove();
                    chartRef.current = null;
                }

                // 툴팁들 정리
                if (timeAxisLabelRef.current) {
                    timeAxisLabelRef.current.remove();
                    timeAxisLabelRef.current = null;
                }
                if (priceAxisLabelRef.current) {
                    priceAxisLabelRef.current.remove();
                    priceAxisLabelRef.current = null;
                }
                if (tooltipRef.current) {
                    tooltipRef.current.remove();
                    tooltipRef.current = null;
                }

                resizeObserver.disconnect();
                hasTooltipAppeared.current = false;
            } catch (error) {
                console.error("차트 제거 중 오류 발생:", error);
            }
        };
    }, [filteredTrades, loading, handleCrosshairMove, selectedMetric, timeframeStr]);

    // 마운트/업데이트 시 configSymbols로 심볼 ref 동기화
    useEffect(() => {
        symbolsRef.current = configSymbols;
    }, [configSymbols]);

    if (loading) {
        return (
            <div
                id="symbol-performance-container"
                style={{
                    width: '100%',
                    height: '100%',
                    position: 'relative',
                    overflow: 'visible',
                    margin: 'auto',
                    boxSizing: 'border-box',
                    minHeight: '400px',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center'
                }}
            >
                <div style={{color: '#ffffff', fontSize: '16px'}}>데이터 로딩 중...</div>
            </div>
        );
    }

    return (
        <div style={{
            width: '100%',
            height: '100%',
            position: 'relative',
            display: 'flex',
            flexDirection: 'column'
        }}>
            <Box sx={{
                position: 'absolute',
                top: '-12px',
                right: '2px',
                zIndex: 10,
                backgroundColor: 'transparent'
            }}>
                <FormControl variant="outlined" size="small">
                    <InputLabel id="metric-select-label" sx={{
                        color: 'white',
                        fontSize: '16px',
                        fontWeight: 400,
                        fontFamily: "'Inter', 'Pretendard', sans-serif",
                        '&.Mui-focused': {
                            color: isDropdownOpen ? 'rgba(255, 215, 0, 0.8)' : 'white',
                        }
                    }}>성과 지표</InputLabel>
                    <Select
                        labelId="metric-select-label"
                        value={selectedMetric}
                        onChange={handleMetricChange}
                        onOpen={toggleDropdown}
                        onClose={toggleDropdown}
                        label="성과 지표"
                        sx={{
                            backgroundColor: '#111111',
                            color: 'white',
                            fontSize: '15px',
                            fontWeight: 500,
                            fontFamily: "'Inter', 'Pretendard', sans-serif",
                            boxShadow: isDropdownOpen ? '0 0 8px rgba(255, 215, 0, 0.4)' : 'none',
                            transition: 'box-shadow 0.3s cubic-bezier(0.4, 0, 0.2, 1), border-color 0.3s cubic-bezier(0.4, 0, 0.2, 1)',
                            '.MuiSelect-select': {
                                padding: '8px 32px 8px 14px',
                            },
                            '& .MuiOutlinedInput-notchedOutline': {
                                borderColor: 'rgba(255, 215, 0, 0.4)',
                                transition: 'border-color 0.3s cubic-bezier(0.4, 0, 0.2, 1)'
                            },
                            '&.Mui-focused .MuiOutlinedInput-notchedOutline': {
                                borderColor: isDropdownOpen ? 'rgba(255, 215, 0, 0.8)' : 'rgba(255, 215, 0, 0.4)',
                                borderWidth: '1px',
                            },
                            '&:hover .MuiOutlinedInput-notchedOutline': {
                                borderColor: 'rgba(255, 215, 0, 0.8)',
                            },
                            '& .MuiSvgIcon-root': {
                                color: 'rgba(255, 215, 0, 0.8)',
                                transition: 'transform 0.3s cubic-bezier(0.4, 0, 0.2, 1)'
                            },
                            '&.Mui-focused .MuiSvgIcon-root': {
                                transform: isDropdownOpen ? 'rotate(180deg)' : 'rotate(0deg)',
                            }
                        }}
                        MenuProps={{
                            disablePortal: false,
                            PaperProps: {
                                sx: {
                                    backgroundColor: '#111111',
                                    border: '1px solid rgba(255, 215, 0, 0.4)',
                                    borderRadius: '6px',
                                    boxShadow: '0 4px 20px rgba(0, 0, 0, 0.8)',
                                    mt: 0.5,
                                    maxHeight: '200px',
                                    '& .MuiMenuItem-root': {
                                        color: 'white',
                                        fontSize: '15px',
                                        fontFamily: "'Inter', 'Pretendard', sans-serif",
                                        minHeight: 'auto',
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
                                    transform: 'translateY(-8px)',
                                    opacity: 0,
                                    '&.MuiPopover-paper': {
                                        transform: 'translateY(0)',
                                        opacity: 1
                                    }
                                }
                            },
                            TransitionProps: {
                                timeout: 300
                            }
                        }}
                    >
                        <MenuItem value="누적 순손익">누적 순손익</MenuItem>
                        <MenuItem value="진입 횟수">진입 횟수</MenuItem>
                        <MenuItem value="강제 청산 횟수">강제 청산 횟수</MenuItem>
                        <MenuItem value="펀딩 횟수">펀딩 횟수</MenuItem>
                        <MenuItem value="펀딩비">펀딩비</MenuItem>
                        <MenuItem value="승률">승률</MenuItem>
                        <MenuItem value="손익비">손익비</MenuItem>
                        <MenuItem value="기대값">기대값</MenuItem>
                        <MenuItem value="보유 시간 합계">보유 시간 합계</MenuItem>
                    </Select>
                </FormControl>
            </Box>
            <div
                id="symbol-performance-container"
                ref={chartContainerRef}
                style={{
                    width: '100%',
                    height: '100%',
                    position: 'relative',
                    overflow: 'visible',
                    margin: 'auto',
                    boxSizing: 'border-box',
                    minHeight: '400px'
                }}
            />
        </div>
    );
};

export default SymbolPerformance;
