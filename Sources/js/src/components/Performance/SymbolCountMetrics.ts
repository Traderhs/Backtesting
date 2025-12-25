export interface Trade {
    [key: string]: any;
}

export function calculateSymbolCountMetrics(actualTrades: Trade[]) {
    if (actualTrades.length === 0) {
        return {
            avgSymbolCount: "0개",
            maxSymbolCount: "0개",
            minSymbolCount: "0개"
        };
    }

    // 모든 "보유 심볼 수" 값 배열 생성
    const symbolCounts = actualTrades.map(trade => {
        const value = trade["보유 심볼 수"];
        // 값이 '-'이거나 빈 값이면 null 반환 (계산에서 제외)
        if (value === '-' || value === null || value === undefined || value === '') {
            return null;
        }
        return Number(value);
    });

    // null이 아닌 값들만 필터링
    const validSymbolCounts = symbolCounts.filter(count => count !== null);

    // 유효한 값이 없으면 모두 '-' 반환
    if (validSymbolCounts.length === 0) {
        return {
            avgSymbolCount: "-",
            maxSymbolCount: "-",
            minSymbolCount: "-"
        };
    }

    // 평균, 최대, 최소 심볼 개수 계산
    const totalSymbolCount = validSymbolCounts.reduce((sum, count) => sum + count, 0);
    const avgCount = Math.round(totalSymbolCount / validSymbolCounts.length);
    const maxCount = Math.max(...validSymbolCounts);
    const minCount = Math.min(...validSymbolCounts);

    return {
        avgSymbolCount: isNaN(avgCount) ? '-' : Math.round(avgCount).toString(),
        maxSymbolCount: isNaN(maxCount) ? '-' : Math.round(maxCount).toString(),
        minSymbolCount: isNaN(minCount) ? '-' : Math.round(minCount).toString()
    };
}
