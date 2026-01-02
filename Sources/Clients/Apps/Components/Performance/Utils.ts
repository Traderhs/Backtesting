export const safeNumber = (value: any): number => {
    const num = Number(value);

    return isNaN(num) ? 0 : num;
};

export const parseDate = (dateString: string | undefined): Date | null => {
    if (!dateString || dateString === '-') {
        return null;
    }

    const date = new Date(dateString);

    return isNaN(date.getTime()) ? null : date;
};

export const formatDateTime = (date: Date | null): string => {
    if (!date) {
        return '-';
    }

    const year = date.getFullYear();
    const month = String(date.getMonth() + 1).padStart(2, '0');
    const day = String(date.getDate()).padStart(2, '0');

    const hours = String(date.getHours()).padStart(2, '0');
    const minutes = String(date.getMinutes()).padStart(2, '0');
    const seconds = String(date.getSeconds()).padStart(2, '0');
    return `${year}-${month}-${day} ${hours}:${minutes}:${seconds}`;
};

export const formatDateTimeWithWeekday = (date: Date | null): string => {
    if (!date) {
        return '-';
    }

    const year = date.getFullYear();
    const month = String(date.getMonth() + 1).padStart(2, '0');
    const day = String(date.getDate()).padStart(2, '0');

    const hours = String(date.getHours()).padStart(2, '0');
    const minutes = String(date.getMinutes()).padStart(2, '0');
    const seconds = String(date.getSeconds()).padStart(2, '0');
    const weekdays = ['일', '월', '화', '수', '목', '금', '토'];
    const weekday = weekdays[date.getDay()];
    return `${year}-${month}-${day} ${hours}:${minutes}:${seconds} (${weekday})`;
};

export const formatDuration = (start: Date | null, end: Date | null): string => {
    if (!start || !end || start.getTime() > end.getTime()) {
        return "-";
    }

    let diff = Math.floor((end.getTime() - start.getTime()) / 1000);
    if (diff < 0) {
        return "-";
    } else if (diff === 0) {
        return "0초";
    }

    const units = [
        {value: 31536000, name: '년'},      // kYear
        {value: 2592000, name: '개월'},     // kMonth
        {value: 604800, name: '주'},        // kWeek
        {value: 86400, name: '일'},         // kDay
        {value: 3600, name: '시간'},        // kHour
        {value: 60, name: '분'},            // kMinute
        {value: 1, name: '초'}              // kSecond
    ];

    const resultUnits: string[] = [];
    let remainder = diff;

    // 첫 번째 단위 찾기
    for (const unit of units) {
        if (remainder >= unit.value) {
            const count = Math.floor(remainder / unit.value);
            resultUnits.push(`${count}${unit.name}`);
            remainder = remainder % unit.value;
            break;
        }
    }

    // 2번째 단위 찾기 (0이 아닌 경우만, 최대한 높은 단위)
    for (const unit of units) {
        if (remainder >= unit.value) {
            const count = Math.floor(remainder / unit.value);
            if (count > 0) {
                resultUnits.push(`${count}${unit.name}`);
                break;
            }
        }
    }

    // 결과가 없으면 초 단위로 표시
    if (resultUnits.length === 0) {
        return `${diff}초`;
    }

    return resultUnits.join(' ');
};

export const calculatePercentage = (value: number, initial: number): string => {
    if (initial === 0) return "-";
    const percentage = ((value - initial) / initial) * 100;
    return `${percentage.toLocaleString('en-US', {minimumFractionDigits: 2, maximumFractionDigits: 2})}%`;
};

export const formatDollar = (value: number | string): string => {
    if (value === '-' || value === undefined || value === null) {
        return '-';
    }

    const numValue = typeof value === 'string' ? parseFloat(value) : value;

    if (isNaN(numValue)) {
        return '-';
    }

    if (numValue > 0) {
        return `$${numValue.toLocaleString('en-US', {minimumFractionDigits: 2, maximumFractionDigits: 2})}`;
    } else if (numValue < 0) {
        return `-$${Math.abs(numValue).toLocaleString('en-US', {minimumFractionDigits: 2, maximumFractionDigits: 2})}`;
    } else {
        return '$0.00';
    }
};

export const formatPercent = (value: string | number): string => {
    if (value === undefined || value === null || value === '-') return '-';

    // 문자열인 경우 숫자 부분 추출 (예: "12.34%" -> "12.34")
    // 숫자인 경우 그대로 사용
    const percentValue = typeof value === 'string'
        ? parseFloat(value.replace('%', '').replace(/,/g, ''))
        : value;

    if (isNaN(percentValue)) {
        return typeof value === 'string' ? value : '-';
    }

    // 숫자를 3자리 쉼표가 있는 형식으로 변환하고 % 추가
    return `${percentValue.toLocaleString('en-US', {minimumFractionDigits: 2, maximumFractionDigits: 2})}%`;
};
