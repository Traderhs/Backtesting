export const getMonthOptions = (): number[] => Array.from({length: 12}, (_, i) => i + 1);
export const getDayOptions = (): number[] => Array.from({length: 31}, (_, i) => i + 1);
export const getDayOfWeekOptions = (): number[] => Array.from({length: 7}, (_, i) => i); // 0 ~ 6
export const getHourOptions = (): number[] => Array.from({length: 24}, (_, i) => i);
export const getMinuteSecondOptions = (): number[] => Array.from({length: 60}, (_, i) => i);

// 연도 옵션은 tradeData에서 추출할 때 사용
export const getYearOptions = (tradeData: any[], timeKeys: string[]): number[] => {
    const allYears = tradeData.flatMap((trade) => {
        const years: number[] = [];
        timeKeys.forEach((key) => {
            const timeStr = String(trade[key]);
            if (timeStr !== "-" && timeStr.trim() !== "") {
                const dt = new Date(timeStr.replace(" ", "T") + "Z");
                if (!isNaN(dt.getTime())) {
                    years.push(dt.getUTCFullYear());
                }
            }
        });
        return years;
    });
    if (allYears.length === 0) return [];
    const minYear = Math.min(...allYears);
    const maxYear = Math.max(...allYears);
    return Array.from({length: maxYear - minYear + 1}, (_, i) => minYear + i);
};
