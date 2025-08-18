// Sources/js/src/workers/holdingTimePnlDistribution.worker.ts

interface Trade {
    '거래 번호': number;
    '보유 시간': string;
    '순손익': number;
}

const parseHoldingTimeToSeconds = (holdingTimeStr: string): number => {
    if (!holdingTimeStr || holdingTimeStr === '-') return 0;
    
    // "동일봉 거래"인 경우 0초로 처리
    if (holdingTimeStr === '동일봉 거래') return 0;

    let totalSeconds = 0;
    const units: Record<string, number> = {
        '년': 31536000, '달': 2592000, '주': 604800, '일': 86400,
        '시간': 3600, '분': 60, '초': 1,
    };

    const regex = /(\d+)\s*(년|달|주|일|시간|분|초)/g;
    let match;
    while ((match = regex.exec(holdingTimeStr)) !== null) {
        totalSeconds += parseInt(match[1], 10) * (units[match[2]] || 0);
    }
    return totalSeconds;
};

const formatHoldingTime = (seconds: number): string => {
    if (seconds === 0) return '0초';

    const years = Math.floor(seconds / 31536000);
    const months = Math.floor((seconds % 31536000) / 2592000);
    const weeks = Math.floor((seconds % 2592000) / 604800);
    const days = Math.floor((seconds % 604800) / 86400);
    const hours = Math.floor((seconds % 86400) / 3600);
    const minutes = Math.floor((seconds % 3600) / 60);
    const remainingSeconds = Math.floor(seconds % 60);

    const parts: string[] = [];
    if (years > 0) parts.push(`${years}년`);
    if (months > 0) parts.push(`${months}달`);
    if (weeks > 0) parts.push(`${weeks}주`);
    if (days > 0) parts.push(`${days}일`);
    if (hours > 0) parts.push(`${hours}시간`);
    if (minutes > 0) parts.push(`${minutes}분`);
    if (remainingSeconds > 0) parts.push(`${remainingSeconds}초`);

    return parts.join(' ');
};

// 최소 보유 시간의 단위를 찾아서 0초, 0분, 0시간... 식으로 표시하는 함수
const getMinimalUnitForZero = (nonZeroSeconds: number[]): string => {
    if (nonZeroSeconds.length === 0) return '0초';
    
    const minSeconds = Math.min(...nonZeroSeconds);
    
    if (minSeconds < 60) return '0초';
    if (minSeconds < 3600) return '0분';
    if (minSeconds < 86400) return '0시간';
    if (minSeconds < 604800) return '0일';
    if (minSeconds < 2592000) return '0주';
    if (minSeconds < 31536000) return '0달';
    return '0년';
};

self.onmessage = (e: MessageEvent<{ filteredTrades: Trade[] }>) => {
    const { filteredTrades } = e.data;

    if (!filteredTrades || filteredTrades.length === 0) {
        self.postMessage({ plotData: [], layoutData: null });
        return;
    }

    const validTrades = filteredTrades.filter(trade => trade["거래 번호"] !== 0);

    if (validTrades.length === 0) {
        self.postMessage({ plotData: [], layoutData: null });
        return;
    }

    const xData: number[] = [];
    const yData: number[] = [];
    const customData: (string | number)[][] = [];
    const colors: string[] = [];

    // 먼저 0이 아닌 보유 시간들을 수집하여 최소 단위 결정
    const nonZeroSeconds = validTrades
        .map(trade => parseHoldingTimeToSeconds(String(trade["보유 시간"] || '')))
        .filter(seconds => seconds > 0);
    
    const zeroDisplayText = getMinimalUnitForZero(nonZeroSeconds);
    
    // 0이 아닌 값들 중 최소값을 찾아서 0초 거래의 x값 결정
    const minNonZeroValue = nonZeroSeconds.length > 0 ? Math.min(...nonZeroSeconds) : 1;
    const zeroXValue = minNonZeroValue * 0.5; // 최소값의 절반으로 설정

    validTrades.forEach(trade => {
        const holdingTimeStr = String(trade["보유 시간"] || '');
        const holdingTimeSeconds = parseHoldingTimeToSeconds(holdingTimeStr);
        const netPnl = Number(trade["순손익"] || 0);

        // 0초인 경우 최소값보다 약간 작은 값으로 설정
        const xValue = holdingTimeSeconds === 0 ? zeroXValue : holdingTimeSeconds;
        xData.push(xValue);
        yData.push(netPnl);

        // 툴팁 표시용 시간 텍스트
        const formattedTime = holdingTimeSeconds === 0 ? zeroDisplayText : formatHoldingTime(holdingTimeSeconds);
        
        // 거래번호에 천 단위 쉼표 추가
        const tradeNumFormatted = trade["거래 번호"].toLocaleString();
        customData.push([
            tradeNumFormatted,
            formattedTime,
            netPnl.toFixed(2)
        ]);
        colors.push(netPnl >= 0 ? '#4caf50' : '#f23645');
    });

    const yValues = yData.slice();
    if (yValues.length > 1) {
        const yRange = Math.max(...yValues) - Math.min(...yValues);
        const jitterAmount = yRange > 0 ? yRange * 0.05 : 0;
        if (jitterAmount > 0) {
            for (let i = 0; i < yData.length; i++) {
                const original = yData[i];
                if (original === 0) continue;
                const randomOffset = (Math.random() - 0.5) * jitterAmount;
                let candidate = original + randomOffset;
                if ((original > 0 && candidate <= 0) || (original < 0 && candidate >= 0)) {
                    candidate = original + Math.sign(original) * Math.abs(randomOffset);
                }
                yData[i] = candidate;
            }
        }
    }

    const xJitterFactor = 0.05;
    for (let i = 0; i < xData.length; i++) {
        // 모든 값이 양수이므로 안전하게 jitter 적용
        const randomFactor = 1 + (Math.random() - 0.5) * xJitterFactor;
        xData[i] *= randomFactor;
    }

    let finalColors: string[] = colors;
    if (xData.length > 0) {
        const numBinsX = 20;
        const numBinsY = 20;
        const logXData = xData.map(x => Math.log10(x)); // x는 이미 양수로 보장됨
        const logXMin = Math.min(...logXData);
        const logXMax = Math.max(...logXData);
        const yMin = Math.min(...yData);
        const yMax = Math.max(...yData);
        const binCounts: Record<string, number> = {};
        for (let i = 0; i < xData.length; i++) {
            const xBin = Math.floor(((logXData[i] - logXMin) / (logXMax - logXMin + 1e-9)) * (numBinsX - 1));
            const yBin = Math.floor(((yData[i] - yMin) / (yMax - yMin + 1e-9)) * (numBinsY - 1));
            const key = `${xBin}-${yBin}`;
            binCounts[key] = (binCounts[key] || 0) + 1;
        }
        const densities = xData.map((_, i) => {
            const xBin = Math.floor(((logXData[i] - logXMin) / (logXMax - logXMin + 1e-9)) * (numBinsX - 1));
            const yBin = Math.floor(((yData[i] - yMin) / (yMax - yMin + 1e-9)) * (numBinsY - 1));
            return binCounts[`${xBin}-${yBin}`] || 1;
        });
        const maxDensity = Math.max(...densities);
        finalColors = colors.map((baseColor, idx) => {
            const density = densities[idx];
            const opacity = 0.4 + (density / maxDensity) * 0.6;
            const hex = baseColor.replace('#', '');
            const r = parseInt(hex.substring(0, 2), 16);
            const g = parseInt(hex.substring(2, 4), 16);
            const b = parseInt(hex.substring(4, 6), 16);
            return `rgba(${r}, ${g}, ${b}, ${opacity.toFixed(2)})`;
        });
    }

    const minX = xData.length > 0 ? Math.min(...xData) : 0;
    const maxX = xData.length > 0 ? Math.max(...xData) : 0;

    let trendData: any | null = null;
    if (xData.length >= 2) {
        const logX = xData.map(x => Math.log10(x)); // x는 이미 양수로 보장됨
        const n = logX.length;
        const meanX = logX.reduce((sum, v) => sum + v, 0) / n;
        const meanY = yData.reduce((sum, v) => sum + v, 0) / n;
        let numerator = 0;
        let denominator = 0;
        for (let i = 0; i < n; i++) {
            numerator += (logX[i] - meanX) * (yData[i] - meanY);
            denominator += (logX[i] - meanX) ** 2;
        }
        const slope = denominator !== 0 ? numerator / denominator : 0;
        const intercept = meanY - slope * meanX;
        const yStart = slope * Math.log10(minX) + intercept;
        const yEnd = slope * Math.log10(maxX) + intercept;
        trendData = {
            x: [minX, maxX],
            y: [yStart, yEnd],
            mode: 'lines',
            type: 'scatter',
            name: '추세선',
            line: { color: '#ffffff', width: 2, dash: 'dot' },
            hoverinfo: 'skip'
        };
    }

    const scatterData = {
        x: xData,
        y: yData,
        mode: 'markers',
        type: 'scattergl',
        name: '거래',
        marker: { size: 8, color: finalColors, line: { width: 0 }, opacity: 1 },
        customdata: customData,
        hovertemplate: '<extra></extra>'
    };

    const plotData = trendData ? [scatterData, trendData] : [scatterData];

    const logMinX = Math.log10(minX);
    const logMaxX = Math.log10(maxX);
    const range = logMaxX - logMinX;
    const margin = range * 0.05;
    const paddedMinX = logMinX - margin;
    const paddedMaxX = logMaxX + margin;
    const xAxisRange = [paddedMinX, paddedMaxX];

    const tickOptions: {v: number, label: string}[] = [];
    const pushTick = (value: number, lbl: string) => tickOptions.push({v: value, label: lbl});
    
    // 동일봉 거래가 있는지 확인
    const hasZeroTimeTrades = validTrades.some(trade => 
        parseHoldingTimeToSeconds(String(trade["보유 시간"] || '')) === 0
    );
    
    // 동일봉 거래가 있을 때만 해당 단위의 0 틱 추가
    if (hasZeroTimeTrades) {
        // 0이 아닌 보유 시간들을 다시 수집 (스코프 문제 해결)
        const nonZeroSecondsForTick = validTrades
            .map(trade => parseHoldingTimeToSeconds(String(trade["보유 시간"] || '')))
            .filter(seconds => seconds > 0);
        const minNonZeroValueForTick = nonZeroSecondsForTick.length > 0 ? Math.min(...nonZeroSecondsForTick) : 1;
        const zeroXValueForTick = minNonZeroValueForTick * 0.5;
        const zeroDisplayTextForTick = getMinimalUnitForZero(nonZeroSecondsForTick);
        
        pushTick(zeroXValueForTick, zeroDisplayTextForTick);
    }
    
    [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 15, 20, 30, 45].forEach(s => pushTick(s, `${s}초`));
    [1, 2, 3, 4, 5, 6, 8, 10, 12, 15, 20, 30, 45].forEach(m => pushTick(m * 60, `${m}분`));
    [1, 2, 3, 4, 6, 8, 12, 18].forEach(h => pushTick(h * 3600, `${h}시간`));
    [1, 2, 3, 4, 5, 6].forEach(d => pushTick(d * 86400, `${d}일`));
    [1, 2, 3].forEach(w => pushTick(w * 7 * 86400, `${w}주`));
    [1, 2, 3, 4, 6, 9].forEach(mo => pushTick(mo * 30 * 86400, `${mo}달`));
    [1, 2, 3, 5, 10].forEach(y => pushTick(y * 365 * 86400, `${y}년`));
    
    const axisMinSec = Math.pow(10, xAxisRange[0]);
    const axisMaxSec = Math.pow(10, xAxisRange[1]);
    const filteredTicks = tickOptions.filter(opt => opt.v >= axisMinSec && opt.v <= axisMaxSec);
    let tickvals = filteredTicks.map(opt => opt.v);
    let ticktext = filteredTicks.map(opt => opt.label);
    const MAX_TICKS = 20;
    if (tickvals.length > MAX_TICKS) {
        const step = Math.ceil(tickvals.length / MAX_TICKS);
        tickvals = tickvals.filter((_, idx) => idx % step === 0);
        ticktext = ticktext.filter((_, idx) => idx % step === 0);
    }

    const layoutData = { xAxisRange, tickvals, ticktext };

    self.postMessage({ plotData, layoutData });
};