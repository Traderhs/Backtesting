/**
 * 보유 시간을 초 단위로 변환하는 함수
 * @param holdingTime 보유 시간 문자열 (예: "1일 3시간 20분", "동일봉 거래")
 * @returns 초 단위로 변환된 숫자, 파싱에 실패하면 null 반환
 */
export function parseHoldingTime(holdingTime: string): number | null {
    holdingTime = holdingTime.trim();
    if (holdingTime === "동일봉 거래") {
        return 0; // 동일봉 거래는 0으로 처리
    }

    const dayMatch = holdingTime.match(/(\d+)\s*일/);
    const hourMatch = holdingTime.match(/(\d+)\s*시간/);
    const minuteMatch = holdingTime.match(/(\d+)\s*분/);
    const secondMatch = holdingTime.match(/(\d+)\s*초/);

    const days = dayMatch ? parseInt(dayMatch[1], 10) : 0;
    const hours = hourMatch ? parseInt(hourMatch[1], 10) : 0;
    const minutes = minuteMatch ? parseInt(minuteMatch[1], 10) : 0;
    const seconds = secondMatch ? parseInt(secondMatch[1], 10) : 0;

    return (days * 86400) + (hours * 3600) + (minutes * 60) + seconds;
}