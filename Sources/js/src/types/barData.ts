/**
 * 바 데이터 타입 열거형
 * C++ BarDataType enum과 일치
 */
export enum BarDataType {
    TRADING = '트레이딩',
    MAGNIFIER = '돋보기',
    REFERENCE = '참조',
    MARK_PRICE = '마크 가격'
}

/**
 * 타임프레임 단위
 */
export enum TimeframeUnit {
    MILLISECOND = '밀리초',
    SECOND = '초',
    MINUTE = '분',
    HOUR = '시간',
    DAY = '일',
    WEEK = '주',
    MONTH = '개월'
}

/**
 * 타임프레임 설정
 */
export interface TimeframeConfig {
    value: number;
    unit: TimeframeUnit;
}

/**
 * 바 데이터 설정
 */
export interface BarDataConfig {
    timeframe: TimeframeConfig;   // 타임프레임 (숫자 + 단위)
    klinesDirectory: string;      // 바 데이터 폴더 경로
    barDataType: BarDataType;     // 바 데이터 타입
}

/**
 * 타임프레임을 문자열로 변환 (예: { value: 1, unit: '시간' } -> "1h")
 */
export function timeframeToString(config: TimeframeConfig): string {
    const unitMap: Record<TimeframeUnit, string> = {
        [TimeframeUnit.MILLISECOND]: 'ms',
        [TimeframeUnit.SECOND]: 's',
        [TimeframeUnit.MINUTE]: 'm',
        [TimeframeUnit.HOUR]: 'h',
        [TimeframeUnit.DAY]: 'd',
        [TimeframeUnit.WEEK]: 'w',
        [TimeframeUnit.MONTH]: 'M'
    };
    return `${config.value}${unitMap[config.unit]}`;
}
