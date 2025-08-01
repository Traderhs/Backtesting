/**
 * 보유 시간을 초 단위로 변환하는 함수 (C++ 백엔드 FormatTimeDiff 로직과 일치)
 * @param holdingTime 보유 시간 문자열 (예: "1일 3시간 20분", "동일봉 거래", "0.5초", "1분 30초")
 * @returns 초 단위로 변환된 숫자, 파싱에 실패하면 null 반환
 */
export function parseHoldingTime(holdingTime: string): number | null {
    holdingTime = holdingTime.trim();
    if (holdingTime === "동일봉 거래") {
        return 0; // 동일봉 거래는 0으로 처리
    }

    // C++ 백엔드와 동일한 시간 상수들 (밀리초 단위)
    const kSecond = 1000;
    const kMinute = 60 * kSecond;        // 60,000
    const kHour = 60 * kMinute;          // 3,600,000
    const kDay = 24 * kHour;             // 86,400,000
    const kWeek = 7 * kDay;              // 604,800,000
    const kMonth = 30 * kDay;            // 2,592,000,000 (30일로 가정)
    const kYear = 12 * kMonth;           // 31,104,000,000

    // 띄어쓰기로 분리해서 각 단위 처리
    const parts = holdingTime.split(/\s+/);
    let totalMs = 0;

    for (const part of parts) {
        if (part.endsWith('년')) {
            const value = parseInt(part.slice(0, -1), 10);
            if (!isNaN(value)) totalMs += value * kYear;
        } else if (part.endsWith('개월')) {
            const value = parseInt(part.slice(0, -2), 10);
            if (!isNaN(value)) totalMs += value * kMonth;
        } else if (part.endsWith('주')) {
            const value = parseInt(part.slice(0, -1), 10);
            if (!isNaN(value)) totalMs += value * kWeek;
        } else if (part.endsWith('일')) {
            const value = parseInt(part.slice(0, -1), 10);
            if (!isNaN(value)) totalMs += value * kDay;
        } else if (part.endsWith('시간')) {
            const value = parseInt(part.slice(0, -2), 10);
            if (!isNaN(value)) totalMs += value * kHour;
        } else if (part.endsWith('분')) {
            const value = parseInt(part.slice(0, -1), 10);
            if (!isNaN(value)) totalMs += value * kMinute;
        } else if (part.endsWith('초')) {
            const value = parseFloat(part.slice(0, -1));
            if (!isNaN(value)) totalMs += value * kSecond;
        }
    }
    
    // C++ 백엔드와 동일하게 1초 미만일 때는 소수점 초를 그대로 반환
    if (totalMs < kSecond) {
        return totalMs / 1000; // 소수점 초 그대로 반환
    }
    
    return Math.floor(totalMs / 1000); // 1초 이상일 때는 정수 초로 반환
}
