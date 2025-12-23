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
    NULL = '',
    SECOND = '초',
    MINUTE = '분',
    HOUR = '시간',
    DAY = '일',
    WEEK = '주',
    MONTH = '개월',
    YEAR = '년'
}

/**
 * 타임프레임 설정
 */
export interface TimeframeConfig {
    value: number | null;
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
        [TimeframeUnit.NULL]: '',
        [TimeframeUnit.SECOND]: 's',
        [TimeframeUnit.MINUTE]: 'm',
        [TimeframeUnit.HOUR]: 'h',
        [TimeframeUnit.DAY]: 'd',
        [TimeframeUnit.WEEK]: 'w',
        [TimeframeUnit.MONTH]: 'M',
        [TimeframeUnit.YEAR]: 'y'
    };

    // 값이 null 또는 단위가 NULL(미선택)인 경우 빈 문자열로 표현
    if (config == null) return '';
    if (config.value === null || config.unit === TimeframeUnit.NULL) {
        return '';
    }

    return `${config.value}${unitMap[config.unit]}`;
} 

// 타임프레임 문자열 파싱
export function parseTimeframeString(tfString: string | null | undefined): { value: number | null; unit: TimeframeUnit } {
    if (tfString === '' || tfString === null || tfString === undefined) {
        return {value: null, unit: TimeframeUnit.NULL}
    }

    // 단위 문자는 단일 문자로 처리 (예: s, m, h, d, w, M, y)
    const match = tfString.match(/^(\d+)([smhdwMy])$/);
    if (!match) {
        return {value: null, unit: TimeframeUnit.NULL};
    }

    const value = parseInt(match[1]);
    const unitMap: Record<string, TimeframeUnit> = {
        's': TimeframeUnit.SECOND,
        'm': TimeframeUnit.MINUTE,
        'h': TimeframeUnit.HOUR,
        'd': TimeframeUnit.DAY,
        'w': TimeframeUnit.WEEK,
        'M': TimeframeUnit.MONTH,
        'y': TimeframeUnit.YEAR
    };

    return {value, unit: unitMap[match[2]] || TimeframeUnit.HOUR};
}