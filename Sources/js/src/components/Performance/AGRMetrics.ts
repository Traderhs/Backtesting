// AGR (Annualized Growth Rate) 계산 함수
const calculateRate = (initialBalance: number, endingBalance: number, daysPerPeriod: number, totalDays: number): string => {
    // 초기 자본금이 0 이하거나 거래 기간이 없는 경우 계산 불가
    if (initialBalance <= 0 || totalDays <= 0) {
        return "-";
    }
    // 수익률 계산
    const rate = Math.pow(endingBalance / initialBalance, daysPerPeriod / totalDays) - 1;
    // 퍼센트로 변환하고 소수점 2자리까지 표시
    return (rate * 100).toFixed(2) + '%';
};

export interface AGRMetricsResult {
    cdgr: string; // Compounded Daily Growth Rate
    cwgr: string; // Compounded Weekly Growth Rate
    cmgr: string; // Compounded Monthly Growth Rate
    cqgr: string; // Compounded Quarterly Growth Rate
    cagr: string; // Compounded Annual Growth Rate
}

// AGR 관련 지표 계산 함수
export const calculateAGRMetrics = (
    initialBalance: number,
    endingBalance: number,
    startDate: Date | null,
    endDate: Date | null
): AGRMetricsResult => {

    // 시작일 또는 종료일이 없으면 계산 불가
    if (!startDate || !endDate) {
        return {cdgr: "-", cwgr: "-", cmgr: "-", cqgr: "-", cagr: "-"};
    }

    // 총 거래 기간(일) 계산: 크립토 시스템이므로 365일 기준
    const totalDays = (endDate.getTime() - startDate.getTime()) / (1000 * 60 * 60 * 24);

    // 기간이 충분한 경우에만 각 비율 계산
    const cdgr = totalDays >= 1 ? calculateRate(initialBalance, endingBalance, 1, totalDays) : "-";
    const cwgr = totalDays >= 7 ? calculateRate(initialBalance, endingBalance, 7, totalDays) : "-";
    const cmgr = totalDays >= 30.4375 ? calculateRate(initialBalance, endingBalance, 30.4375, totalDays) : "-";
    const cqgr = totalDays >= 91.3125 ? calculateRate(initialBalance, endingBalance, 91.3125, totalDays) : "-";
    const cagr = totalDays >= 365.25 ? calculateRate(initialBalance, endingBalance, 365.25, totalDays) : "-";

    return {
        cdgr,
        cwgr,
        cmgr,
        cqgr,
        cagr
    };
};
