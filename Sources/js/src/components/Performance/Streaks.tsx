import {useState, useEffect} from 'react';
import {useTradeFilter} from '@/components/TradeFilter';
import {formatDollar} from './Utils';

interface StreakData {
    count: number;   // 해당 연승/연패의 등장 건수
    totalPnl: number; // 해당 연승/연패의 누적 PnL 합계
}

type StreaksMap = Record<number, StreakData>; // 연승/연패 길이별 데이터

// 외부에서도 사용할 수 있도록 calculateStreaks 함수 export
export const calculateStreaks = (actualTrades: any[]) => {
    // Trade Number 별 Net PnL 집계 (분할 청산 합산)
    const netPnlByTradeNumber: Record<number, number> = {};
    const tradeOrder: number[] = [];

    // 거래 번호별 순손익 합산
    for (const trade of actualTrades) {
        const tradeNumber = Number(trade["거래 번호"]);
        if (tradeNumber === 0) continue; // 0번 거래 번호 제외

        const pnlNet = Number(trade["순손익"] || 0);

        if (!netPnlByTradeNumber[tradeNumber]) {
            tradeOrder.push(tradeNumber);
            netPnlByTradeNumber[tradeNumber] = pnlNet;
        } else {
            netPnlByTradeNumber[tradeNumber] += pnlNet;
        }
    }

    // 연승/연패 계산
    const winStreaks: StreaksMap = {};
    const loseStreaks: StreaksMap = {};

    let currentWinning = false; // 현재 연승(true)인지 연패(false)인지
    let streakLength = 0;       // 현재 몇 연승/연패 중인지
    let pnlSum = 0;             // 현재 연승/연패의 누적 PnL
    let first = true;           // 첫 거래 여부

    // 거래 번호 순서대로 순회하면서 연승 연패 계산
    for (const tradeNum of tradeOrder) {
        const pnl = netPnlByTradeNumber[tradeNum];
        const isWin = pnl >= 0;

        // 새 연승/연패 시작: 첫 반복이거나 승패가 바뀌었을 때
        if (first || isWin !== currentWinning) {
            if (!first) {
                // 연승/연패 종료 → 저장
                const streaks = currentWinning ? winStreaks : loseStreaks;
                if (!streaks[streakLength]) {
                    streaks[streakLength] = {count: 1, totalPnl: pnlSum};
                } else {
                    streaks[streakLength].count += 1;
                    streaks[streakLength].totalPnl += pnlSum;
                }
            }

            // 연승/연패 리셋
            currentWinning = isWin;
            streakLength = 1;
            pnlSum = pnl;
            first = false;
        } else {
            // 같은 승/패 연속 → 연승/연패 연장
            streakLength++;
            pnlSum += pnl;
        }
    }

    // 마지막 연승/연패가 남아있으면 저장
    if (!first) {
        const streaks = currentWinning ? winStreaks : loseStreaks;
        if (!streaks[streakLength]) {
            streaks[streakLength] = {count: 1, totalPnl: pnlSum};
        } else {
            streaks[streakLength].count += 1;
            streaks[streakLength].totalPnl += pnlSum;
        }
    }

    // 최대 연승/연패 길이 계산
    const maxWinStreak = Object.keys(winStreaks).length > 0
        ? Math.max(...Object.keys(winStreaks).map(k => parseInt(k)))
        : 0;

    const maxLoseStreak = Object.keys(loseStreaks).length > 0
        ? Math.max(...Object.keys(loseStreaks).map(k => parseInt(k)))
        : 0;

    return {
        winStreaks,
        loseStreaks,
        maxWinStreak,
        maxLoseStreak,
    };
};

// 컴포넌트 Props 타입 정의
interface StreaksProps {
    onReady?: () => void; // 데이터 준비 완료 시 호출될 콜백
}

export default function Streaks({onReady}: StreaksProps) {
    const {filteredTrades} = useTradeFilter();
    const [streaksData, setStreaksData] = useState<ReturnType<typeof calculateStreaks> | null>(null);

    // 데이터 계산 함수
    const computeStreaks = () => {
        try {
            if (!filteredTrades || filteredTrades.length <= 1) {
                setStreaksData(null);
                return;
            }
      
            // 연속 거래 계산 (0번 거래는 제외)
            const actualTrades = filteredTrades.slice(1);
            const result = calculateStreaks(actualTrades);
            setStreaksData(result);
        } catch (error) {
            console.error("연속 거래 계산 중 오류 발생:", error);
            setStreaksData(null);
        }
    };

    useEffect(() => {
        try {
            // filteredTrades가 유효할 때 계산 수행
            computeStreaks();
        } finally {
            // 계산 여부와 무관하게 무조건 준비 완료 상태 알림
            if (onReady) {
                onReady();
            }
        }
    }, [filteredTrades, onReady]);

    // 데이터가 없거나 충분하지 않은 경우
    if (!streaksData || !filteredTrades || filteredTrades.length <= 1) {
        return;
    }

    // streaksData에서 값 추출
    const {winStreaks, loseStreaks} = streaksData;

    // 모든 연승/연패 길이의 합집합 구하기
    const allStreakLengths = new Set<number>();
    Object.keys(winStreaks).forEach(k => allStreakLengths.add(parseInt(k)));
    Object.keys(loseStreaks).forEach(k => allStreakLengths.add(parseInt(k)));

    // 연승/연패만 별도로 정렬 (빈칸 없이 표시하기 위함)
    const sortedWinStreakLengths = Object.keys(winStreaks)
        .map(k => parseInt(k))
        .sort((a, b) => a - b);

    const sortedLoseStreakLengths = Object.keys(loseStreaks)
        .map(k => parseInt(k))
        .sort((a, b) => a - b);

    // 연승, 연패 개별 행 렌더링 함수
    const renderWinStreakRow = (streakLength: number, index: number) => {
        const winData = winStreaks[streakLength];
        if (!winData || winData.count === 0) return null;

        const winAvgPerTrade = winData.totalPnl / (streakLength * winData.count);

        return (
            <tr key={`win-${streakLength}`}
                className={`${index % 2 === 0 ? 'bg-[#4d4000]' : 'bg-[#665400]'} hover:bg-[#cca300]`}>
                <td className="p-2 text-center">{`${(() => { const val = Number(streakLength); return isNaN(val) ? '0' : val.toLocaleString('en-US'); })()}연승`}</td>
                <td className="p-2 text-center">{`${(() => { const val = Number(winData.count); return isNaN(val) ? '0' : val.toLocaleString('en-US'); })()}건`}</td>
                <td className="p-2 text-center positive">{formatDollar(winData.totalPnl)}</td>
                <td className="p-2 text-center positive">{formatDollar(winAvgPerTrade)}</td>
            </tr>
        );
    };

    const renderLoseStreakRow = (streakLength: number, index: number) => {
        const loseData = loseStreaks[streakLength];
        if (!loseData || loseData.count === 0) return null;

        const loseAvgPerTrade = loseData.totalPnl / (streakLength * loseData.count);

        return (
            <tr key={`lose-${streakLength}`}
                className={`${index % 2 === 0 ? 'bg-[#4d4000]' : 'bg-[#665400]'} hover:bg-[#cca300]`}>
                <td className="p-2 text-center">{`${(() => { const val = Number(streakLength); return isNaN(val) ? '0' : val.toLocaleString('en-US'); })()}연패`}</td>
                <td className="p-2 text-center">{`${(() => { const val = Number(loseData.count); return isNaN(val) ? '0' : val.toLocaleString('en-US'); })()}건`}</td>
                <td className="p-2 text-center negative">{formatDollar(loseData.totalPnl)}</td>
                <td className="p-2 text-center negative">{formatDollar(loseAvgPerTrade)}</td>
            </tr>
        );
    };

    return (
        <div className="text-white">
            <div className="flex flex-col space-y-4">
                {/* Win Streak Section (Top Right) */}
                {sortedWinStreakLengths.length > 0 && (
                    <div>
                        <div> {/* Win Analysis Table */}
                            <table className="w-full border-collapse">
                                <thead>
                                <tr className="bg-[#b39700]">
                                    <th className="p-2 text-center streaks-header-cell">연승 횟수</th>
                                    <th className="p-2 text-center streaks-header-cell">연승 건수</th>
                                    <th className="p-2 text-center streaks-header-cell">순수익 합계</th>
                                    <th className="p-2 text-center streaks-header-cell">진입당 평균 순수익</th>
                                </tr>
                                </thead>
                                <tbody>
                                {sortedWinStreakLengths.map((len, idx) => renderWinStreakRow(len, idx))}
                                </tbody>
                            </table>
                        </div>
                    </div>
                )}

                {/* Lose Streak Section (Bottom Right) */}
                {sortedLoseStreakLengths.length > 0 && (
                    <div>
                        <div> {/* Lose Analysis Table */}
                            <table className="w-full border-collapse">
                                <thead>
                                <tr className="bg-[#b39700]">
                                    <th className="p-2 text-center streaks-header-cell">연패 횟수</th>
                                    <th className="p-2 text-center streaks-header-cell">연패 건수</th>
                                    <th className="p-2 text-center streaks-header-cell">순손실 합계</th>
                                    <th className="p-2 text-center streaks-header-cell">진입당 평균 순손실</th>
                                </tr>
                                </thead>
                                <tbody>
                                {sortedLoseStreakLengths.map((len, idx) => renderLoseStreakRow(len, idx))}
                                </tbody>
                            </table>
                        </div>
                    </div>
                )}
            </div>
        </div>
    );
} 