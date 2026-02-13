import React, {useEffect, useRef, useState} from 'react';
import {IChartApi} from 'lightweight-charts';
import {useWebSocket} from '../Server/WebSocketContext';
import './Calendar.css';

interface CalendarProps {
    chart: IChartApi | null;
    candleStickData: any[];
    onClose: () => void;
    onDateSelected: (timestamp: number) => void;
    timeframe?: string; // 타임프레임 추가
    lastSelectedDate: Date | null; // props로 이전 선택 날짜 받기
    lastSelectedTime: string; // props로 이전 선택 시간 받기
    onDateTimeSelected: (date: Date, time: string) => void; // 날짜/시간 선택 시 부모에게 알리는 콜백
    symbol: string;
    result: string;
    onLoadingStart: () => void; // 캘린더 로딩 시작 콜백
    onLoadingEnd: () => void; // 캘린더 로딩 종료 콜백
    config: any;
}

// 타임프레임에 따른 시간 간격 생성 함수
const generateTimeOptions = (timeframe: string | undefined): string[] => {
    const options: string[] = [];

    // timeframe이 없으면 기본 시간만 반환
    if (!timeframe) {
        options.push('00:00');
        return options;
    }

    // 모든 시간을 중복 없이 추적하기 위한 Set
    const seenTimes = new Set<string>();

    // timeframe에 따라 시간 옵션 생성
    if (timeframe.includes('m')) {
        // 분 단위 타임프레임 (예: 1m, 5m, 15m, 30m)
        const minutes = parseInt(timeframe.replace('m', ''));
        let currentMinute = 0; // 0부터 시작

        while (currentMinute < 24 * 60) {
            const hours = Math.floor(currentMinute / 60);
            const mins = currentMinute % 60;
            const timeStr = `${hours.toString().padStart(2, '0')}:${mins.toString().padStart(2, '0')}`;

            if (!seenTimes.has(timeStr)) {
                seenTimes.add(timeStr);
                options.push(timeStr);
            }

            currentMinute += minutes;
        }
    } else if (timeframe.includes('h')) {
        // 시간 단위 타임프레임 (예: 1h, 2h, 4h, 7h)
        const hours = parseInt(timeframe.replace('h', ''));

        // 모든 가능한 시간 표시 (24시간 주기 이상까지)
        // 7h의 경우: 00, 07, 14, 21, 04, 11, 18, 01, 08, 15, 22 ...
        let currentHour = 0;

        // 무한루프 방지를 위한 제한
        const maxIterations = 30;
        let iterations = 0;

        while (iterations < maxIterations) {
            const timeStr = `${currentHour.toString().padStart(2, '0')}:00`;

            if (!seenTimes.has(timeStr)) {
                seenTimes.add(timeStr);
                options.push(timeStr);
            }

            currentHour = (currentHour + hours) % 24;
            iterations++;

            // 모든 가능한 시간을 다 봤거나 처음으로 돌아왔으면 종료
            if (seenTimes.size === 24 || (iterations > 1 && currentHour === 0)) {
                break;
            }
        }
    } else {
        // 일/주/월 단위는 00:00만 사용
        options.push('00:00');
    }

    // 옵션을 시간 순으로 정렬
    options.sort((a, b) => {
        const hourA = parseInt(a.split(':')[0]);
        const minuteA = parseInt(a.split(':')[1]);
        const hourB = parseInt(b.split(':')[0]);
        const minuteB = parseInt(b.split(':')[1]);

        if (hourA !== hourB) {
            return hourA - hourB;
        }
        return minuteA - minuteB;
    });

    return options;
};

// timeframe 파싱 함수
const isDailyOrHigherTimeframe = (timeframe: string | undefined): boolean => {
    if (!timeframe) {
        return false;
    }

    const timeframe_str = String(timeframe).trim();
    if (!timeframe_str) {
        return false;
    }

    const rawUnit = timeframe_str[timeframe_str.length - 1];

    // 일/주/월 명시자
    if (rawUnit === 'd' || rawUnit === 'w' || rawUnit === 'M') return true;

    const num = parseFloat(timeframe_str.slice(0, -1));
    if (Number.isNaN(num)) {
        return false;
    }

    let multiplier = 0;
    if (rawUnit === 's') {
        multiplier = 1;
    } else if (rawUnit === 'm') {
        multiplier = 60;
    } else if (rawUnit === 'h') {
        multiplier = 3600;
    } else {
        return false;
    }

    const seconds = num * multiplier;
    return seconds >= 24 * 3600;
};

// 날짜 포맷팅 함수를 먼저 정의
const formatDate = (date: Date) => {
    const year = date.getFullYear();
    const month = (date.getMonth() + 1).toString().padStart(2, '0');
    const day = date.getDate().toString().padStart(2, '0');
    return `${year}-${month}-${day}`;
};

const Calendar: React.FC<CalendarProps> = ({
                                               chart,
                                               candleStickData,
                                               onClose,
                                               onDateSelected,
                                               timeframe,
                                               lastSelectedDate,
                                               lastSelectedTime,
                                               onDateTimeSelected,
                                               symbol,
                                               result,
                                               onLoadingStart,
                                               onLoadingEnd,
                                               config
                                           }) => {
    const {ws} = useWebSocket();
    const now = new Date();

    // 첫 데이터 포인트 시간을 기본값으로 사용 - 항상 호출되도록 수정
    const initialTimeData = (() => {
        if (candleStickData && candleStickData.length > 0) {
            const firstPoint = candleStickData[0];
            const timestamp = firstPoint.time * 1000; // 초 -> 밀리초

            // GMT 기준 날짜 생성
            const date = new Date(timestamp);

            // 타임프레임에 맞는 시간 포맷팅
            let timeStr = '00:00';

            // 일일 이상이 아닌 경우만 시간 표시
            if (!isDailyOrHigherTimeframe(timeframe)) {
                const hours = date.getUTCHours(); // UTC 시간 사용
                const minutes = date.getUTCMinutes(); // UTC 분 사용
                timeStr = `${hours.toString().padStart(2, '0')}:${minutes.toString().padStart(2, '0')}`;
            }

            return {date, timeStr};
        }

        // 데이터가 없으면 현재 시간 사용
        return {
            date: now,
            timeStr: isDailyOrHigherTimeframe(timeframe) ? '00:00' : `${now.getUTCHours().toString().padStart(2, '0')}:${now.getUTCMinutes().toString().padStart(2, '0')}`
        };
    })();

    // lastSelectedDate와 lastSelectedTime이 없을 때만 초기 데이터 포인트 시간 사용
    const getInitialDate = () => {
        return lastSelectedDate || initialTimeData.date;
    };

    const getInitialTime = () => {
        if (isDailyOrHigherTimeframe(timeframe)) {
            return '00:00';
        }

        return lastSelectedTime || initialTimeData.timeStr;
    };

    // 초기값 설정 (콘솔에 로깅)
    const initialDate = getInitialDate();
    const initialTime = getInitialTime();

    // 타임프레임이 1d 이상인지 확인
    const isDailyOrHigher = isDailyOrHigherTimeframe(timeframe);

    // 시간 옵션 생성
    const timeOptions = generateTimeOptions(timeframe);

    // 상태 초기화
    const [currentMonth, setCurrentMonth] = useState<number>(initialDate.getMonth());
    const [currentYear, setCurrentYear] = useState<number>(initialDate.getFullYear());
    const [selectedDate, setSelectedDate] = useState<Date>(initialDate);
    const [selectedTime, setSelectedTime] = useState<string>(initialTime);
    const [dateInputValue, setDateInputValue] = useState<string>(formatDate(initialDate));
    const [timeInputValue, setTimeInputValue] = useState<string>(initialTime);
    const [isLoading, setIsLoading] = useState<boolean>(false);
    const [isDragging, setIsDragging] = useState(false);
    const [startDragPos, setStartDragPos] = useState({x: 0, y: 0, initialLeft: 0, initialTop: 0});
    const [isTimeDropdownOpen, setIsTimeDropdownOpen] = useState(false);
    const [wasDragging, setWasDragging] = useState(false);

    const calendarRef = useRef<HTMLDivElement>(null);
    const dragRef = useRef<HTMLDivElement>(null);
    const timeDropdownRef = useRef<HTMLDivElement>(null);
    const timeOptionsRef = useRef<HTMLDivElement>(null);

    // 일요일부터 토요일까지의 요일 표시
    const weekDays = ['일', '월', '화', '수', '목', '금', '토'];

    // 월 이름
    const months = ['1월', '2월', '3월', '4월', '5월', '6월', '7월', '8월', '9월', '10월', '11월', '12월'];

    // 현재 달의 일수 계산
    const getDaysInMonth = (year: number, month: number) => {
        return new Date(year, month + 1, 0).getDate();
    };

    // 현재 달의 첫 날 요일 (0: 일요일, 1: 월요일, ...)
    const getFirstDayOfMonth = (year: number, month: number) => {
        return new Date(year, month, 1).getDay();
    };

    // 타임 드롭다운 외부 클릭 감지
    useEffect(() => {
        const handleClickOutside = (event: MouseEvent) => {
            if (timeDropdownRef.current && !timeDropdownRef.current.contains(event.target as Node)) {
                setIsTimeDropdownOpen(false);
            }
        };

        document.addEventListener('mousedown', handleClickOutside);
        return () => {
            document.removeEventListener('mousedown', handleClickOutside);
        };
    }, []);

    // 날짜 입력값이 변경될 때 유효한 값이면 달력 UI 업데이트
    useEffect(() => {
        // 입력된 날짜 형식 확인 (YYYY-MM-DD)
        const datePattern = /^(\d{4})-(\d{1,2})-(\d{1,2})$/;
        const match = dateInputValue.match(datePattern);

        if (match) {
            const year = parseInt(match[1]);
            const month = parseInt(match[2]) - 1; // 0-based 월
            const day = parseInt(match[3]);

            // 날짜 유효성 검사
            const newDate = new Date(year, month, day);

            // 유효한 날짜인지 확인
            if (
                newDate.getFullYear() === year &&
                newDate.getMonth() === month &&
                newDate.getDate() === day
            ) {
                // 유효한 날짜면 달력만 업데이트 (입력값은 그대로 유지)
                setSelectedDate(newDate);
                setCurrentYear(year);
                setCurrentMonth(month);
            }
        }
        // 유효하지 않은 경우 아무것도 하지 않음 (입력값 그대로 유지)
    }, [dateInputValue]);

    // 시간 입력값이 변경될 때 유효한 값이면 드롭다운 UI 업데이트
    useEffect(() => {
        // 입력된 시간 형식 확인 (HH:MM)
        const timePattern = /^(\d{1,2}):(\d{1,2})$/;
        const match = timeInputValue.match(timePattern);

        if (match) {
            const hours = parseInt(match[1]);
            const minutes = parseInt(match[2]);

            // 시간, 분 범위 제한
            if (hours >= 0 && hours < 24 && minutes >= 0 && minutes < 60) {
                // 옵션에 정확히 일치하는 값이 있는지 확인 (숫자값 비교)
                const exactMatch = timeOptions.find(option => {
                    const [optH, optM] = option.split(':').map(Number);
                    return optH === hours && optM === minutes;
                });

                if (exactMatch) {
                    // 정확히 일치하는 옵션이 있으면 선택 상태 업데이트 (입력값은 그대로 유지)
                    setSelectedTime(exactMatch);

                    // 드롭다운이 열려 있으면 스크롤 위치 업데이트
                    if (isTimeDropdownOpen) {
                        immediateScrollToTime(exactMatch);
                    }
                }
            }
        }
        // 유효하지 않은 경우 아무것도 하지 않음 (입력값 그대로 유지)
    }, [timeInputValue, isTimeDropdownOpen, timeOptions]);

    // 이전 달로 이동
    const goToPreviousMonth = () => {
        setCurrentMonth(prev => {
            if (prev === 0) {
                setCurrentYear(year => year - 1);
                return 11;
            }
            return prev - 1;
        });
    };

    // 다음 달로 이동
    const goToNextMonth = () => {
        setCurrentMonth(prev => {
            if (prev === 11) {
                setCurrentYear(year => year + 1);
                return 0;
            }
            return prev + 1;
        });
    };

    // 이전 연도로 이동
    const goToPreviousYear = () => {
        setCurrentYear(prev => prev - 1);
    };

    // 다음 연도로 이동
    const goToNextYear = () => {
        setCurrentYear(prev => prev + 1);
    };

    // 캘린더 외부 클릭시 닫기
    useEffect(() => {
        const handleClickOutside = (event: MouseEvent) => {
            // 캘린더 컨테이너 외부 클릭 확인
            const isOutsideCalendar = calendarRef.current && !calendarRef.current.contains(event.target as Node);

            // 차트 영역을 클릭했는지 확인 (.chart-container 클래스를 가진 요소 체크)
            let element = event.target as HTMLElement;
            let isInsideChart = false;

            // 클릭한 요소부터 상위로 올라가면서 차트 컨테이너인지 확인
            while (element && !isInsideChart) {
                if (element.classList && (
                    element.classList.contains('chart-container') ||
                    element.classList.contains('tv-lightweight-charts')
                )) {
                    isInsideChart = true;
                    break;
                }
                element = element.parentElement as HTMLElement;
                if (!element) break;
            }

            // 차트 영역을 클릭한 경우에만 캘린더 닫기
            if (isOutsideCalendar && isInsideChart) {
                onClose();
            }
        };

        document.addEventListener('mousedown', handleClickOutside);
        return () => {
            document.removeEventListener('mousedown', handleClickOutside);
        };
    }, [onClose, chart]);

    // ESC 키 및 Enter 키 처리
    useEffect(() => {
        const handleKeyDown = (event: KeyboardEvent) => {
            if (event.key === 'Escape') {
                onClose();
            } else if (event.key === 'Enter') {
                handleConfirm().then();
            }
        };

        document.addEventListener('keydown', handleKeyDown);
        return () => {
            document.removeEventListener('keydown', handleKeyDown);
        };
    }, [onClose, selectedDate, selectedTime]);

    // FilterCalendar가 열릴 때 차트 캘린더 닫기 이벤트 감지
    useEffect(() => {
        const handleCloseChartCalendar = () => {
            onClose();
        };

        window.addEventListener('closeChartCalendar', handleCloseChartCalendar);
        return () => {
            window.removeEventListener('closeChartCalendar', handleCloseChartCalendar);
        };
    }, [onClose]);

    // 드래그 기능 구현
    useEffect(() => {
        const handleMouseMove = (e: MouseEvent) => {
            if (isDragging) {
                const deltaX = e.clientX - startDragPos.x;
                const deltaY = e.clientY - startDragPos.y;

                // 달력 컨테이너 위치 변경
                if (calendarRef.current) {
                    // 사이드바 너비는 App.tsx에 정의된 대로 18rem (288px)
                    const sidebarWidthRem = 18;
                    const sidebarWidthPx = sidebarWidthRem * 16; // 1rem = 16px (일반적인 경우)

                    // 윈도우 전체 너비
                    const windowWidth = window.innerWidth;
                    const windowHeight = window.innerHeight;

                    // 차트 영역 너비 (윈도우 전체 너비 - 사이드바 너비)
                    const chartWidth = windowWidth - sidebarWidthPx;

                    // 달력 요소의 크기 가져오기
                    const calendarWidth = calendarRef.current.offsetWidth;
                    const calendarHeight = calendarRef.current.offsetHeight;

                    // 새로운 위치 계산 (드래그 이동)
                    let newLeft = startDragPos.initialLeft + deltaX;
                    let newTop = startDragPos.initialTop + deltaY;

                    // 경계 제한: 좌/우/상/하 모두 차트 영역 내로 제한
                    // 왼쪽(0부터 시작), 오른쪽(차트 너비 - 달력 너비까지)
                    newLeft = Math.max(0, Math.min(newLeft, chartWidth - calendarWidth));
                    // 상단(0부터 시작), 하단(창 높이 - 달력 높이까지)
                    newTop = Math.max(0, Math.min(newTop, windowHeight - calendarHeight));

                    // 위치 적용
                    calendarRef.current.style.position = 'absolute';
                    calendarRef.current.style.left = `${newLeft}px`;
                    calendarRef.current.style.top = `${newTop}px`;
                }
            }
        };

        const handleMouseUp = () => {
            if (isDragging) {
                setWasDragging(true);
                // 짧은 지연 후 wasDragging 리셋 (클릭 이벤트가 처리되기 전)
                setTimeout(() => setWasDragging(false), 50);
            }
            setIsDragging(false);
            // 드래그 종료 시 커서 스타일 변경
            if (dragRef.current) {
                dragRef.current.style.cursor = 'grab';
            }
        };

        if (isDragging) {
            document.addEventListener('mousemove', handleMouseMove);
            document.addEventListener('mouseup', handleMouseUp);
            // 드래그 중 커서 스타일 변경
            if (dragRef.current) {
                dragRef.current.style.cursor = 'grabbing';
            }
        }

        return () => {
            document.removeEventListener('mousemove', handleMouseMove);
            document.removeEventListener('mouseup', handleMouseUp);
        };
    }, [isDragging, startDragPos]);

    // 윈도우 리사이즈 시 캘린더 위치 조정
    useEffect(() => {
        const handleResize = () => {
            if (!calendarRef.current) return;

            // 사이드바 너비는 App.tsx에 정의된 대로 18rem (288px)
            const sidebarWidthRem = 18;
            const sidebarWidthPx = sidebarWidthRem * 16; // 1rem = 16px (일반적인 경우)

            // 윈도우 전체 너비
            const windowWidth = window.innerWidth;
            const windowHeight = window.innerHeight;

            // 차트 영역 너비 (윈도우 전체 너비 - 사이드바 너비)
            const chartWidth = windowWidth - sidebarWidthPx;

            // 달력 요소의 크기 가져오기
            const calendarWidth = calendarRef.current.offsetWidth;
            const calendarHeight = calendarRef.current.offsetHeight;

            // 리사이즈 시 무조건 중앙으로 이동
            const centerLeft = Math.max(0, (chartWidth - calendarWidth) / 2);
            const centerTop = Math.max(0, (windowHeight - calendarHeight) / 2);

            calendarRef.current.style.position = 'absolute';
            calendarRef.current.style.left = `${centerLeft}px`;
            calendarRef.current.style.top = `${centerTop}px`;
        };

        window.addEventListener('resize', handleResize);
        return () => {
            window.removeEventListener('resize', handleResize);
        };
    }, []);

    const handleMouseDown = (e: React.MouseEvent) => {
        // 입력 필드나 드롭다운 내부에서는 드래그 방지
        const target = e.target as HTMLElement;
        if (target.closest('.date-input') || target.closest('.time-input') || target.closest('.time-options') || target.closest('.close-button')) {
            return;
        }

        if (dragRef.current) {
            setIsDragging(true);
            setStartDragPos({
                x: e.clientX,
                y: e.clientY,
                initialLeft: calendarRef.current?.offsetLeft || 0,
                initialTop: calendarRef.current?.offsetTop || 0
            });
        }
    };

    // X 버튼 클릭 이벤트 핸들러
    const handleCloseButtonClick = (e: React.MouseEvent) => {
        e.stopPropagation(); // 이벤트 버블링 방지
        onClose();
    };

    // 날짜 입력값 변경 시 - 숫자와 하이픈만 허용
    const handleDateInputChange = (e: React.ChangeEvent<HTMLInputElement>) => {
        let value = e.target.value.replace(/[^0-9-]/g, '');

        // 하이픈 개수 확인
        const hyphenCount = (value.match(/-/g) || []).length;

        // 첫 번째 하이픈 자동 추가 (없는 경우에만)
        if (value.length > 4 && !value.includes('-')) {
            value = value.slice(0, 4) + '-' + value.slice(4);
        }

        // 두 번째 하이픈 자동 추가 (첫 번째 하이픈이 있고, 두 번째가 없는 경우)
        if (value.length > 7 && hyphenCount === 1) {
            value = value.slice(0, 7) + '-' + value.slice(7);
        }

        // 최대 길이 제한 (YYYY-MM-DD)
        if (value.length > 10) {
            value = value.slice(0, 10);
        }

        setDateInputValue(value);
    };

    // 시간 입력값 변경 시 - 숫자와 콜론만 허용
    const handleTimeInputChange = (e: React.ChangeEvent<HTMLInputElement>) => {
        let value = e.target.value.replace(/[^0-9:]/g, '');

        // 자동 콜론 추가 (수동 입력된 콜론이 없는 경우에만)
        if (!value.includes(':')) {
            if (value.length > 2) {
                value = value.slice(0, 2) + ':' + value.slice(2);
            }
        }
        // 최대 길이 제한 (HH:MM)
        if (value.length > 5) {
            value = value.slice(0, 5);
        }

        // 시간과 분 범위 제한
        if (value.length >= 2) {
            const hours = parseInt(value.slice(0, 2));
            if (hours > 23) {
                value = '23' + value.slice(2);
            }
        }
        if (value.length >= 5) {
            const minutes = parseInt(value.slice(3, 5));
            if (minutes > 59) {
                value = value.slice(0, 3) + '59';
            }
        }

        setTimeInputValue(value);
    };

    // 날짜 입력에서 키 입력 제한 및 엔터 키 처리
    const handleDateInputKeyDown = (e: React.KeyboardEvent<HTMLInputElement>) => {
        // Ctrl+A 허용
        if (e.ctrlKey && e.key === 'a') {
            return;
        }

        // 숫자(0-9)와 '-'만 허용, 백스페이스, 삭제, 화살표 키, 탭 등은 허용
        const allowedKeys = ['Backspace', 'Delete', 'ArrowLeft', 'ArrowRight', 'ArrowUp', 'ArrowDown', 'Tab', 'Enter', 'Home', 'End'];
        const isNumber = /^[0-9]$/.test(e.key);
        const isDash = e.key === '-';

        if (!isNumber && !isDash && !allowedKeys.includes(e.key)) {
            e.preventDefault();
            return;
        }

        if (e.key === 'Enter') {
            handleDateInputBlur();
        }
    };

    // 시간 입력에서 키 입력 제한 및 엔터 키 처리
    const handleTimeInputKeyDown = (e: React.KeyboardEvent<HTMLInputElement>) => {
        // Ctrl+A 허용
        if (e.ctrlKey && e.key === 'a') {
            return;
        }

        // 숫자(0-9)와 ':'만 허용, 백스페이스, 삭제, 화살표 키, 탭 등은 허용
        const allowedKeys = ['Backspace', 'Delete', 'ArrowLeft', 'ArrowRight', 'ArrowUp', 'ArrowDown', 'Tab', 'Enter', 'Home', 'End'];
        const isNumber = /^[0-9]$/.test(e.key);
        const isColon = e.key === ':';

        if (!isNumber && !isColon && !allowedKeys.includes(e.key)) {
            e.preventDefault();
            return;
        }

        if (e.key === 'Enter') {
            handleTimeInputBlur();
        }
    };

    // 시간 선택 위치로 즉시 스크롤 이동
    const immediateScrollToTime = (time: string) => {
        if (!timeOptionsRef.current) return;

        const timeElements = timeOptionsRef.current.getElementsByClassName('time-option');
        for (let i = 0; i < timeElements.length; i++) {
            const element = timeElements[i] as HTMLElement;
            if (element.textContent === time) {
                // 드롭다운이 열려있을 때만 스크롤
                if (isTimeDropdownOpen) {
                    element.scrollIntoView({block: 'nearest'});
                }
                break;
            }
        }
    };

    // 시간 입력창 클릭 시 드롭다운 표시
    const handleTimeInputClick = (e: React.MouseEvent) => {
        e.stopPropagation();
        setIsTimeDropdownOpen(true);

        // 선택된 시간 위치로 즉시 스크롤
        requestAnimationFrame(() => {
            immediateScrollToTime(selectedTime);
        });
    };

    // 날짜 선택 시
    const handleDateClick = (day: number) => {
        const newDate = new Date(currentYear, currentMonth, day);
        setSelectedDate(newDate);
        setDateInputValue(formatDate(newDate)); // 입력 필드도 업데이트

        // 부모 컴포넌트에 선택 정보 전달
        onDateTimeSelected(newDate, selectedTime);
    };

    // 시간 선택 시
    const handleTimeSelect = (time: string) => {
        setSelectedTime(time);
        setTimeInputValue(time); // 입력 필드도 업데이트
        setIsTimeDropdownOpen(false);

        // 부모 컴포넌트에 선택 정보 전달
        onDateTimeSelected(selectedDate, time);
    };

    // 날짜 입력 포커스 아웃 시 유효성 검사
    const handleDateInputBlur = () => {
        // 입력된 날짜 형식 확인 (YYYY-MM-DD)
        const datePattern = /^(\d{4})-(\d{1,2})-(\d{1,2})$/;
        const match = dateInputValue.match(datePattern);

        if (match) {
            const year = parseInt(match[1]);
            const month = parseInt(match[2]) - 1; // 0-based 월
            const day = parseInt(match[3]);

            // 날짜 유효성 검사
            const newDate = new Date(year, month, day);

            // 유효한 날짜인지 확인
            if (
                newDate.getFullYear() === year &&
                newDate.getMonth() === month &&
                newDate.getDate() === day
            ) {
                // 유효한 날짜면 선택된 날짜와 달력 업데이트
                setSelectedDate(newDate);
                setCurrentYear(year);
                setCurrentMonth(month);
                return;
            }
        }

        // 유효하지 않은 날짜는 현재 선택된 날짜로 다시 설정
        setDateInputValue(formatDate(selectedDate));
    };

    // 시간 입력 포커스 아웃 시 유효성 검사
    const handleTimeInputBlur = () => {
        // 일일 이상 타임프레임에서는 항상 00:00으로 고정
        if (isDailyOrHigher) {
            setSelectedTime('00:00');
            setTimeInputValue('00:00');
            return;
        }

        // 입력된 시간 형식 확인 (HH:MM)
        const timePattern = /^(\d{1,2}):(\d{1,2})$/;
        const match = timeInputValue.match(timePattern);

        if (match) {
            let hours = parseInt(match[1]);
            let minutes = parseInt(match[2]);

            // 시간, 분 범위 제한
            if (hours >= 0 && hours < 24 && minutes >= 0 && minutes < 60) {
                // 타임프레임에 맞게 조정
                if (timeframe && timeframe.includes('m')) {
                    const tfMinutes = parseInt(timeframe.replace('m', ''));
                    // 가장 가까운 타임프레임 간격으로 조정
                    minutes = Math.round(minutes / tfMinutes) * tfMinutes;
                    if (minutes === 60) {
                        minutes = 0;
                        hours = (hours + 1) % 24;
                    }
                } else if (timeframe && timeframe.includes('h')) {
                    const tfHours = parseInt(timeframe.replace('h', ''));
                    // 가장 가까운 타임프레임 간격으로 조정
                    hours = Math.round(hours / tfHours) * tfHours;
                    if (hours === 24) hours = 0;
                    minutes = 0;
                }

                const formattedTime = `${hours.toString().padStart(2, '0')}:${minutes.toString().padStart(2, '0')}`;

                // 타임프레임에 있는 시간인지 확인
                if (timeOptions.includes(formattedTime)) {
                    setSelectedTime(formattedTime);
                    setTimeInputValue(formattedTime);
                    return;
                } else {
                    // 가장 가까운 시간 옵션 찾기
                    const hoursMinutes = hours * 60 + minutes;

                    let closestOption = timeOptions[0];
                    let minDiff = Infinity;

                    for (const option of timeOptions) {
                        const [h, m] = option.split(':').map(Number);
                        const optionMinutes = h * 60 + m;
                        const diff = Math.abs(optionMinutes - hoursMinutes);

                        if (diff < minDiff) {
                            minDiff = diff;
                            closestOption = option;
                        }
                    }

                    setSelectedTime(closestOption);
                    setTimeInputValue(closestOption);
                    return;
                }
            }
        }

        // 유효하지 않은 시간은 현재 선택된 시간으로 다시 설정
        setTimeInputValue(selectedTime);
    };

    // 달력 그리드 생성
    const renderCalendarGrid = () => {
        const daysInMonth = getDaysInMonth(currentYear, currentMonth);
        const firstDay = getFirstDayOfMonth(currentYear, currentMonth);

        const days = [];

        // 이전 달의 마지막 날짜들
        const prevMonthDays = currentMonth === 0
            ? getDaysInMonth(currentYear - 1, 11)
            : getDaysInMonth(currentYear, currentMonth - 1);

        // 첫 주의 이전 달 날짜들 채우기
        for (let i = 0; i < firstDay; i++) {
            const day = prevMonthDays - firstDay + i + 1;
            days.push(
                <div
                    key={`prev-${i}`}
                    className="calendar-day prev-month"
                    onClick={() => {
                        // 이전 달로 이동하고 해당 날짜 선택
                        const newMonth = currentMonth === 0 ? 11 : currentMonth - 1;
                        const newYear = currentMonth === 0 ? currentYear - 1 : currentYear;
                        setCurrentMonth(newMonth);
                        setCurrentYear(newYear);
                        const newDate = new Date(newYear, newMonth, day);
                        setSelectedDate(newDate);
                        setDateInputValue(formatDate(newDate));

                        // 부모 컴포넌트에 선택 정보 전달
                        onDateTimeSelected(newDate, selectedTime);
                    }}
                >
                    {day}
                </div>
            );
        }

        // 현재 달 날짜들
        for (let i = 1; i <= daysInMonth; i++) {
            const date = new Date(currentYear, currentMonth, i);
            const isSelected = selectedDate &&
                date.getDate() === selectedDate.getDate() &&
                date.getMonth() === selectedDate.getMonth() &&
                date.getFullYear() === selectedDate.getFullYear();

            const isToday = new Date().toDateString() === date.toDateString();

            days.push(
                <div
                    key={`current-${i}`}
                    className={`calendar-day ${isSelected ? 'selected' : ''} ${isToday ? 'today' : ''}`}
                    onClick={() => handleDateClick(i)}
                >
                    {i}
                </div>
            );
        }

        // 다음 달 날짜들로 나머지 채우기
        const totalCells = 42; // 6주 × 7일
        const remainingCells = totalCells - days.length;

        for (let i = 1; i <= remainingCells; i++) {
            days.push(
                <div
                    key={`next-${i}`}
                    className="calendar-day next-month"
                    onClick={() => {
                        // 다음 달로 이동하고 해당 날짜 선택
                        const newMonth = currentMonth === 11 ? 0 : currentMonth + 1;
                        const newYear = currentMonth === 11 ? currentYear + 1 : currentYear;
                        setCurrentMonth(newMonth);
                        setCurrentYear(newYear);
                        const newDate = new Date(newYear, newMonth, i);
                        setSelectedDate(newDate);
                        setDateInputValue(formatDate(newDate));

                        // 부모 컴포넌트에 선택 정보 전달
                        onDateTimeSelected(newDate, selectedTime);
                    }}
                >
                    {i}
                </div>
            );
        }

        return days;
    };

    // 선택한 날짜로 이동하는 함수
    const handleConfirm = async () => {
        if (!chart || candleStickData.length === 0) return;

        // 캘린더 바로 닫기
        onClose();

        setIsLoading(true);
        onLoadingStart(); // 로딩 시작 알림

        // 선택한 날짜의 타임스탬프 계산
        // UTC 기준으로 날짜 객체 생성 (시간대 차이 방지)
        const utcDate = new Date(Date.UTC(
            selectedDate.getFullYear(),
            selectedDate.getMonth(),
            selectedDate.getDate()
        ));

        // 타임프레임이 1d 이상인 경우 시간은 무시하고, 그렇지 않으면 시간 설정
        if (!isDailyOrHigher) {
            const [hours, minutes] = selectedTime.split(':').map(Number);
            utcDate.setUTCHours(hours, minutes, 0, 0); // GMT 기준으로 시간 설정
        } else {
            utcDate.setUTCHours(0, 0, 0, 0); // GMT 기준으로 00:00 설정
        }

        const timestamp = Math.floor(utcDate.getTime() / 1000);

        // 이미 로드된 데이터 범위 확인
        const firstCandleTime = candleStickData[0]?.time;
        const lastCandleTime = candleStickData[candleStickData.length - 1]?.time;

        try {
            // 데이터 로드 범위 안에 있는지 확인
            if (timestamp >= firstCandleTime && timestamp <= lastCandleTime) {
                // 이미 로드된 범위 안에 있으므로 바로 이동
                onDateSelected(timestamp);
                onDateTimeSelected(selectedDate, selectedTime);
            } else {
                // 요청한 시간이 현재 로딩되어 있는 차트 범위를 벗어나는 경우
                // 데이터 재로딩 필요
                await reloadDataFromTimestamp(timestamp);
                onDateTimeSelected(selectedDate, selectedTime);
            }
        } finally {
            setIsLoading(false);
            onLoadingEnd(); // 로딩 종료 알림
        }
    };

    // 데이터 재로딩 함수
    const reloadDataFromTimestamp = async (timestamp: number) => {
        if (!chart) return;

        try {
            // 1. 차트의 모든 시리즈는 Chart.tsx에서 관리되므로 여기서는 제거하지 않음
            // 대신 새 데이터 요청을 통해 Chart.tsx가 자동으로 업데이트하도록 함

            // 2. WebSocket으로 새로운 데이터 요청
            if (!ws || ws.readyState !== WebSocket.OPEN) {
                console.error('WebSocket 연결이 없습니다');
                return;
            }

            // 3. 요청한 시간이 포함된 파일로부터 과거로 5개 파일 요청
            const indicatorsToLoad = config?.['지표']
                ? config['지표']
                    .filter((i: any) => i['플롯']?.['플롯 종류'] !== '비활성화' && i['데이터 경로'])
                    .map((i: any) => i['지표 이름'])
                : [];

            const requestPayload = {
                action: "loadChartData",
                symbol: symbol,
                result: result,
                indicators: indicatorsToLoad,
                fileRequest: {
                    type: "date",
                    referenceTime: timestamp,
                    count: 5, // 과거로 5개 파일 요청
                    dataPoints: 50000
                },
                isCalendarDateRequest: true // Calendar에서 온 요청임을 표시
            };

            // 4. 응답 대기
            return new Promise((resolve, reject) => {
                let isResponseReceived = false;

                const messageListener = (event: MessageEvent) => {
                    try {
                        const msg = JSON.parse(event.data);
                        if (msg.action === "loadChartDataResponse") {
                            ws.removeEventListener("message", messageListener);
                            isResponseReceived = true;

                            if (msg.candleData && Array.isArray(msg.candleData) && msg.candleData.length > 0) {
                                // 5. 데이터 재설정은 Chart.tsx에서 자동으로 처리됨
                                // 6. Chart.tsx에서 데이터 초기화 완료 신호를 기다림
                                const waitForDataReset = () => {
                                    const checkInterval = setInterval(() => {
                                        // Chart.tsx에서 Calendar 데이터 초기화 완료 신호 확인
                                        if ((window as any).calendarDataResetComplete === true) {
                                            clearInterval(checkInterval);
                                            (window as any).calendarDataResetComplete = false; // 플래그 리셋

                                            // 이제 안전하게 차트 이동
                                            setTimeout(() => {
                                                onDateSelected(timestamp);
                                                resolve(true);
                                            }, 100);
                                        }
                                    }, 50); // 50ms마다 체크

                                    // 최대 3초 대기
                                    setTimeout(() => {
                                        clearInterval(checkInterval);
                                        // 타임아웃이어도 일단 이동 시도
                                        onDateSelected(timestamp);
                                        resolve(true);
                                    }, 3000);
                                };

                                waitForDataReset();
                            } else {
                                reject(new Error('데이터 로드 실패'));
                            }
                        }
                    } catch (error) {
                        if (!isResponseReceived) {
                            ws.removeEventListener("message", messageListener);
                            reject(error);
                        }
                    }
                };

                ws.addEventListener("message", messageListener);

                // Calendar에서 온 date 요청임을 전역적으로 표시
                (window as any).lastCalendarDateRequest = true;

                ws.send(JSON.stringify(requestPayload));

                // 타임아웃 설정
                setTimeout(() => {
                    ws.removeEventListener("message", messageListener);
                    reject(new Error('타임아웃'));
                }, 10000);
            });

        } catch (error) {
            console.error('데이터 재로딩 실패:', error);
            throw error;
        }
    };

    return (
        <div className="calendar-overlay">
            <div
                ref={calendarRef}
                className="calendar-container"
            >
                <div
                    ref={dragRef}
                    className="calendar-header"
                    onMouseDown={handleMouseDown}
                >
                    <div className="calendar-title-row">
                        <h3 className="chart-calendar-title">이동</h3>
                        <button className="close-button" onClick={handleCloseButtonClick}>×</button>
                    </div>
                    <div className="date-display">
                        <div className="date-input-container">
                            <input
                                type="text"
                                className="date-input"
                                value={dateInputValue}
                                onChange={handleDateInputChange}
                                onBlur={handleDateInputBlur}
                                onKeyDown={handleDateInputKeyDown}
                                placeholder="날짜"
                            />
                            <div className="calendar-icon">
                                <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="16" height="16"
                                     fill="currentColor">
                                    <path
                                        d="M19,4H18V2H16V4H8V2H6V4H5C3.89,4 3,4.9 3,6V20A2,2 0 0,0 5,22H19A2,2 0 0,0 21,20V6A2,2 0 0,0 19,4M19,20H5V10H19V20M19,8H5V6H19V8Z"/>
                                </svg>
                            </div>
                        </div>
                        {!isDailyOrHigher && (
                            <div className="selected-time">
                                <div
                                    className="time-dropdown"
                                    ref={timeDropdownRef}
                                >
                                    <div className="time-display" onClick={() => {
                                        if (!wasDragging) {
                                            setIsTimeDropdownOpen(!isTimeDropdownOpen);
                                        }
                                    }}>
                                        <input
                                            type="text"
                                            className="time-input"
                                            value={timeInputValue}
                                            onChange={handleTimeInputChange}
                                            onBlur={handleTimeInputBlur}
                                            onKeyDown={handleTimeInputKeyDown}
                                            placeholder="시간"
                                            onClick={handleTimeInputClick}
                                        />
                                        <div className="clock-icon" onClick={(e) => {
                                            e.stopPropagation();
                                            if (!wasDragging) {
                                                setIsTimeDropdownOpen(!isTimeDropdownOpen);
                                            }
                                        }}>
                                            <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="16"
                                                 height="16" fill="currentColor">
                                                <path
                                                    d="M12,20A8,8 0 0,0 20,12A8,8 0 0,0 12,4A8,8 0 0,0 4,12A8,8 0 0,0 12,20M12,2A10,10 0 0,1 22,12A10,10 0 0,1 12,22C6.47,22 2,17.5 2,12A10,10 0 0,1 12,2M12.5,7V12.25L17,14.92L16.25,16.15L11,13V7H12.5Z"/>
                                            </svg>
                                        </div>
                                    </div>
                                    {isTimeDropdownOpen && (
                                        <div className="time-options" ref={timeOptionsRef}>
                                            {timeOptions.map(time => (
                                                <div
                                                    key={time}
                                                    className={`time-option ${time === selectedTime ? 'selected' : ''}`}
                                                    onClick={() => handleTimeSelect(time)}
                                                >
                                                    {time}
                                                </div>
                                            ))}
                                        </div>
                                    )}
                                </div>
                            </div>
                        )}
                    </div>
                </div>

                <div className="month-navigator">
                    <div className="navigator-buttons">
                        <button onClick={goToPreviousYear} className="year-btn">&lt;&lt;</button>
                        <button onClick={goToPreviousMonth} className="month-btn">&lt;</button>
                    </div>
                    <span className="current-month">{currentYear}년 {months[currentMonth]}</span>
                    <div className="navigator-buttons">
                        <button onClick={goToNextMonth} className="month-btn">&gt;</button>
                        <button onClick={goToNextYear} className="year-btn">&gt;&gt;</button>
                    </div>
                </div>

                <div className="calendar-body">
                    <div className="weekdays">
                        {weekDays.map(day => (
                            <div key={day} className="weekday">{day}</div>
                        ))}
                    </div>

                    <div className="days-grid">
                        {renderCalendarGrid()}
                    </div>
                </div>

                <div className="calendar-footer">
                    <div className="footer-buttons">
                        <button
                            className="confirm-button"
                            onClick={handleConfirm}
                            disabled={isLoading}
                        >
                            {isLoading ? '이동 중...' : '이동'}
                        </button>
                        <button
                            className="cancel-button"
                            onClick={onClose}
                        >
                            취소
                        </button>
                    </div>
                </div>
            </div>
        </div>
    );
};

export default Calendar;
