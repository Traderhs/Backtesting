import React, { useState, useRef, useEffect } from 'react';
import { createPortal } from 'react-dom';
import './FilterCalendar.css';

interface FilterCalendarProps {
  onClose: () => void;
  onDateTimeSelected: (date: Date, time: string) => void;
  timeframe?: string;
  lastSelectedDate?: Date | null;
  lastSelectedTime?: string;
  title?: string;
}

// 타임프레임에 따른 시간 간격 생성 함수
const generateTimeOptions = (timeframe: string | undefined): string[] => {
  const options: string[] = [];
  
  if (!timeframe) {
    options.push('00:00');
    return options;
  }
  
  const seenTimes = new Set<string>();
  
  if (timeframe.includes('m')) {
    const minutes = parseInt(timeframe.replace('m', ''));
    let currentMinute = 0;
    
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
    const hours = parseInt(timeframe.replace('h', ''));
    let currentHour = 0;
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
      
      if (seenTimes.size === 24 || (iterations > 1 && currentHour === 0)) {
        break;
      }
    }
  } else {
    options.push('00:00');
  }
  
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

// 일일 이상 타임프레임 확인 함수
const isDailyOrHigherTimeframe = (timeframe: string | undefined): boolean => {
  if (!timeframe) return false;
  return /^[0-9]+[dwM]$/.test(timeframe);
};

// 날짜 포맷팅 함수
const formatDate = (date: Date) => {
  const year = date.getFullYear();
  const month = (date.getMonth() + 1).toString().padStart(2, '0');
  const day = date.getDate().toString().padStart(2, '0');
  return `${year}-${month}-${day}`;
};

const FilterCalendar: React.FC<FilterCalendarProps> = ({
  onClose,
  onDateTimeSelected,
  timeframe,
  lastSelectedDate,
  lastSelectedTime,
  title,
}) => {
  const now = new Date();
  
  // 초기값 설정
  const getInitialDate = () => {
    return lastSelectedDate || now;
  };
  
  const getInitialTime = () => {
    if (isDailyOrHigherTimeframe(timeframe)) {
      return '00:00';
    }
    return lastSelectedTime || '00:00';
  };
  
  const initialDate = getInitialDate();
  const initialTime = getInitialTime();
  
  // 타임프레임이 1d 이상인지 확인
  const isDailyOrHigher = isDailyOrHigherTimeframe(timeframe);
  
  // 시간 옵션 생성
  const timeOptions = generateTimeOptions(timeframe);
  
  // 상태 관리
  const [currentMonth, setCurrentMonth] = useState<number>(initialDate.getMonth());
  const [currentYear, setCurrentYear] = useState<number>(initialDate.getFullYear());
  const [selectedDate, setSelectedDate] = useState<Date>(initialDate);
  const [selectedTime, setSelectedTime] = useState<string>(initialTime);
  const [dateInputValue, setDateInputValue] = useState<string>(formatDate(initialDate));
  const [timeInputValue, setTimeInputValue] = useState<string>(initialTime);
  const [isTimeDropdownOpen, setIsTimeDropdownOpen] = useState(false);
  const [isDragging, setIsDragging] = useState(false);
  const [startDragPos, setStartDragPos] = useState({ x: 0, y: 0, initialLeft: 0, initialTop: 0 });
  const [isCloseButtonPressed, setIsCloseButtonPressed] = useState(false);
  const [wasDragging, setWasDragging] = useState(false);
  
  const timeDropdownRef = useRef<HTMLDivElement>(null);
  const timeOptionsRef = useRef<HTMLDivElement>(null);
  const calendarRef = useRef<HTMLDivElement>(null);
  const dragRef = useRef<HTMLDivElement>(null);

  // 요일 및 월 이름
  const weekDays = ['일', '월', '화', '수', '목', '금', '토'];
  const months = ['1월', '2월', '3월', '4월', '5월', '6월', '7월', '8월', '9월', '10월', '11월', '12월'];

  // FilterCalendar가 열릴 때 차트 캘린더 닫기
  useEffect(() => {
    // 커스텀 이벤트 발생시켜서 차트 캘린더에게 닫으라고 알림
    const closeChartCalendarEvent = new CustomEvent('closeChartCalendar');
    window.dispatchEvent(closeChartCalendarEvent);
  }, []); // 컴포넌트 마운트 시에만 실행

  // 월 관련 계산 함수들
  const getDaysInMonth = (year: number, month: number) => {
    return new Date(year, month + 1, 0).getDate();
  };

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

  // ESC 키 처리
  useEffect(() => {
    const handleKeyDown = (event: KeyboardEvent) => {
      if (event.key === 'Escape') {
        onClose();
      } else if (event.key === 'Enter') {
        handleConfirm();
      }
    };

    document.addEventListener('keydown', handleKeyDown);
    return () => {
      document.removeEventListener('keydown', handleKeyDown);
    };
  }, [onClose, selectedDate, selectedTime]);

  // 달력 외부 클릭 감지
  useEffect(() => {
    let isMouseDownOutside = false;

    const handleMouseDownOutside = (event: MouseEvent) => {
      const target = event.target as HTMLElement;
      // 시간 필터 버튼 클릭은 무시 (버튼 자체 로직으로 열리도록)
      if (target.closest('.time-filter-button')) {
        isMouseDownOutside = false;
        return;
      }
      if (calendarRef.current && !calendarRef.current.contains(event.target as Node)) {
        isMouseDownOutside = true;
      } else {
        isMouseDownOutside = false;
      }
    };

    const handleMouseUpOutside = (event: MouseEvent) => {
      const target = event.target as HTMLElement;
      if (target.closest('.time-filter-button')) {
        return;
      }
      if (isMouseDownOutside && calendarRef.current && !calendarRef.current.contains(event.target as Node)) {
        onClose();
      }
      isMouseDownOutside = false; // 상태 초기화
    };

    document.addEventListener('mousedown', handleMouseDownOutside);
    document.addEventListener('mouseup', handleMouseUpOutside);

    return () => {
      document.removeEventListener('mousedown', handleMouseDownOutside);
      document.removeEventListener('mouseup', handleMouseUpOutside);
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
          
          // 메인 컨텐츠 영역 너비 (윈도우 전체 너비 - 사이드바 너비)
          const mainContentWidth = windowWidth - sidebarWidthPx;
          
          // 달력 요소의 크기 가져오기
          const calendarWidth = calendarRef.current.offsetWidth;
          const calendarHeight = calendarRef.current.offsetHeight;
          
          // 새로운 위치 계산 (드래그 이동)
          let newLeft = startDragPos.initialLeft + deltaX;
          let newTop = startDragPos.initialTop + deltaY;
          
          // 경계 제한: 좌/우/상/하 모두 메인 컨텐츠 영역 내로 제한
          // 왼쪽(0부터 시작), 오른쪽(메인 컨텐츠 너비 - 달력 너비까지)
          newLeft = Math.max(0, Math.min(newLeft, mainContentWidth - calendarWidth));
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

  // 윈도우 리사이즈 시 달력 위치 조정
  useEffect(() => {
    const centerCalendar = () => {
      if (!calendarRef.current) return;
      
      // 사이드바 너비는 App.tsx에 정의된 대로 18rem (288px)
      const sidebarWidthRem = 18;
      const sidebarWidthPx = sidebarWidthRem * 16; // 1rem = 16px (일반적인 경우)
      
      // 윈도우 전체 너비
      const windowWidth = window.innerWidth;
      const windowHeight = window.innerHeight;
      
      // 메인 컨텐츠 영역 너비 (윈도우 전체 너비 - 사이드바 너비)
      const mainContentWidth = windowWidth - sidebarWidthPx;
      
      // 달력 요소의 크기 가져오기
      const calendarWidth = calendarRef.current.offsetWidth;
      const calendarHeight = calendarRef.current.offsetHeight;
      
      // 항상 중앙으로 이동
        const centerLeft = Math.max(0, (mainContentWidth - calendarWidth) / 2);
        const centerTop = Math.max(0, (windowHeight - calendarHeight) / 2);
        
        calendarRef.current.style.position = 'absolute';
        calendarRef.current.style.left = `${centerLeft}px`;
        calendarRef.current.style.top = `${centerTop}px`;
    };

    // 처음 모달이 열릴 때 중앙 정렬
    // 모달이 DOM에 렌더링된 후 위치 조정을 위해 짧은 지연 추가
    setTimeout(centerCalendar, 10);

    // 윈도우 리사이즈 이벤트에 대한 핸들러 등록
    window.addEventListener('resize', centerCalendar);
    return () => {
      window.removeEventListener('resize', centerCalendar);
    };
  }, []);

  // 날짜 입력값 변경 시 유효한 값이면 달력 UI 업데이트
  useEffect(() => {
    const datePattern = /^(\d{4})-(\d{1,2})-(\d{1,2})$/;
    const match = dateInputValue.match(datePattern);
    
    if (match) {
      const year = parseInt(match[1]);
      const month = parseInt(match[2]) - 1;
      const day = parseInt(match[3]);
      
      const newDate = new Date(year, month, day);
      
      if (
        newDate.getFullYear() === year &&
        newDate.getMonth() === month &&
        newDate.getDate() === day
      ) {
        setSelectedDate(newDate);
        setCurrentYear(year);
        setCurrentMonth(month);
      }
    }
  }, [dateInputValue]);

  // 시간 입력값 변경 시 유효한 값이면 드롭다운 UI 업데이트
  useEffect(() => {
    const timePattern = /^(\d{1,2}):(\d{1,2})$/;
    const match = timeInputValue.match(timePattern);
    
    if (match) {
      const hours = parseInt(match[1]);
      const minutes = parseInt(match[2]);
      
      if (hours >= 0 && hours < 24 && minutes >= 0 && minutes < 60) {
        const exactMatch = timeOptions.find(option => {
          const [optH, optM] = option.split(':').map(Number);
          return optH === hours && optM === minutes;
        });
        
        if (exactMatch) {
          setSelectedTime(exactMatch);
          
          if (isTimeDropdownOpen) {
            immediateScrollToTime(exactMatch);
          }
        }
      }
    }
  }, [timeInputValue, isTimeDropdownOpen, timeOptions]);

  // 월 네비게이션 함수들
  const goToPreviousMonth = () => {
    setCurrentMonth(prev => {
      if (prev === 0) {
        setCurrentYear(year => year - 1);
        return 11;
      }
      return prev - 1;
    });
  };

  const goToNextMonth = () => {
    setCurrentMonth(prev => {
      if (prev === 11) {
        setCurrentYear(year => year + 1);
        return 0;
      }
      return prev + 1;
    });
  };

  const goToPreviousYear = () => {
    setCurrentYear(prev => prev - 1);
  };

  const goToNextYear = () => {
    setCurrentYear(prev => prev + 1);
  };

  // 시간 선택 위치로 즉시 스크롤 이동
  const immediateScrollToTime = (time: string) => {
    if (!timeOptionsRef.current) return;
    
    const timeElements = timeOptionsRef.current.getElementsByClassName('time-option');
    for (let i = 0; i < timeElements.length; i++) {
      const element = timeElements[i] as HTMLElement;
      if (element.textContent === time) {
        if (isTimeDropdownOpen) {
          element.scrollIntoView({ block: 'nearest' });
        }
        break;
      }
    }
  };
  
  // 시간 입력창 클릭 시 드롭다운 표시
  const handleTimeInputClick = (e: React.MouseEvent) => {
    e.stopPropagation();
    setIsTimeDropdownOpen(true);
    
    requestAnimationFrame(() => {
      immediateScrollToTime(selectedTime);
    });
  };
  
  // 날짜 선택 시
  const handleDateClick = (day: number) => {
    const newDate = new Date(currentYear, currentMonth, day);
    setSelectedDate(newDate);
    setDateInputValue(formatDate(newDate));
  };
  
  // 시간 선택 시
  const handleTimeSelect = (time: string) => {
    setSelectedTime(time);
    setTimeInputValue(time);
    setIsTimeDropdownOpen(false);
  };
  
  // 날짜 입력 관련 함수들
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
  
  const handleDateInputBlur = () => {
    const datePattern = /^(\d{4})-(\d{1,2})-(\d{1,2})$/;
    const match = dateInputValue.match(datePattern);
    
    if (match) {
      const year = parseInt(match[1]);
      const month = parseInt(match[2]) - 1;
      const day = parseInt(match[3]);
      
      const newDate = new Date(year, month, day);
      
      if (
        newDate.getFullYear() === year &&
        newDate.getMonth() === month &&
        newDate.getDate() === day
      ) {
        setSelectedDate(newDate);
        setCurrentYear(year);
        setCurrentMonth(month);
        return;
      }
    }
    
    setDateInputValue(formatDate(selectedDate));
  };
  
  const handleTimeInputBlur = () => {
    if (isDailyOrHigher) {
      setSelectedTime('00:00');
      setTimeInputValue('00:00');
      return;
    }

    const timePattern = /^(\d{1,2}):(\d{1,2})$/;
    const match = timeInputValue.match(timePattern);
    
    if (match) {
      let hours = parseInt(match[1]);
      let minutes = parseInt(match[2]);
      
      if (hours >= 0 && hours < 24 && minutes >= 0 && minutes < 60) {
        if (timeframe && timeframe.includes('m')) {
          const tfMinutes = parseInt(timeframe.replace('m', ''));
          minutes = Math.round(minutes / tfMinutes) * tfMinutes;
          if (minutes === 60) {
            minutes = 0;
            hours = (hours + 1) % 24;
          }
        } else if (timeframe && timeframe.includes('h')) {
          const tfHours = parseInt(timeframe.replace('h', ''));
          hours = Math.round(hours / tfHours) * tfHours;
          if (hours === 24) hours = 0;
          minutes = 0;
        }
        
        const formattedTime = `${hours.toString().padStart(2, '0')}:${minutes.toString().padStart(2, '0')}`;
        
        if (timeOptions.includes(formattedTime)) {
          setSelectedTime(formattedTime);
          setTimeInputValue(formattedTime);
          return;
        } else {
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
    
    setTimeInputValue(selectedTime);
  };

  // 달력 그리드 생성
  const renderCalendarGrid = () => {
    const daysInMonth = getDaysInMonth(currentYear, currentMonth);
    const firstDay = getFirstDayOfMonth(currentYear, currentMonth);
    
    const days = [];
    
    const prevMonthDays = currentMonth === 0 
      ? getDaysInMonth(currentYear - 1, 11)
      : getDaysInMonth(currentYear, currentMonth - 1);
    
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
          }}
        >
          {day}
        </div>
      );
    }
    
    for (let i = 1; i <= daysInMonth; i++) {
      const date = new Date(currentYear, currentMonth, i);
      const isSelected = selectedDate && 
        date.getDate() === selectedDate.getDate() && 
        date.getMonth() === selectedDate.getMonth() && 
        date.getFullYear() === selectedDate.getFullYear();
      
      days.push(
        <div 
          key={`current-${i}`} 
          className={`calendar-day ${isSelected ? 'selected' : ''}`}
          onClick={() => handleDateClick(i)}
        >
          {i}
        </div>
      );
    }
    
    const totalCells = 42;
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
          }}
        >
          {i}
        </div>
      );
    }
    
    return days;
  };

  // 확인 버튼 처리
  const handleConfirm = () => {
    // UTC 기준으로 날짜 객체 생성 (시간대 차이 방지)
    const utcDate = new Date(Date.UTC(
      selectedDate.getFullYear(),
      selectedDate.getMonth(),
      selectedDate.getDate()
    ));
    
    if (!isDailyOrHigher) {
      const [hours, minutes] = selectedTime.split(':').map(Number);
      utcDate.setUTCHours(hours, minutes, 0, 0); // GMT 기준으로 시간 설정
    } else {
      utcDate.setUTCHours(0, 0, 0, 0); // GMT 기준으로 00:00 설정
    }
    
    onDateTimeSelected(utcDate, selectedTime);
    onClose();
  };

  // 드래그 시작 핸들러
  const handleMouseDown = (e: React.MouseEvent) => {
    // 입력 필드나 드롭다운, 버튼 내부에서는 드래그 방지
    const target = e.target as HTMLElement;
    if (target.closest('.date-input') || target.closest('.time-input') || target.closest('.time-options') || target.closest('.close-button') || target.closest('button')) {
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

  // X 버튼 마우스 다운 핸들러
  const handleCloseButtonMouseDown = (e: React.MouseEvent) => {
    e.stopPropagation(); // 드래그 방지
    setIsCloseButtonPressed(true);
  };

  // X 버튼 마우스 업 핸들러
  const handleCloseButtonMouseUp = (e: React.MouseEvent) => {
    e.stopPropagation(); // 드래그 방지
    if (isCloseButtonPressed) {
      onClose();
    }
    setIsCloseButtonPressed(false);
  };

  // X 버튼 마우스 리브 핸들러 (버튼을 벗어날 때)
  const handleCloseButtonMouseLeave = () => {
    setIsCloseButtonPressed(false);
  };

  const calendarElement = (
    <div className="filter-calendar-overlay">
      <div 
        ref={calendarRef}
        className="filter-calendar-container"
      >
        <div 
          ref={dragRef}
          className="filter-calendar-header"
          style={{ cursor: isDragging ? 'grabbing' : 'grab' }}
          onMouseDown={handleMouseDown}
        >
          {title && (
            <div className="calendar-title-row">
              <h3 className="filter-calendar-title">{title}</h3>
              <button 
                className="close-button" 
                onMouseDown={handleCloseButtonMouseDown}
                onMouseUp={handleCloseButtonMouseUp}
                onMouseLeave={handleCloseButtonMouseLeave}
              >×</button>
            </div>
          )}
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
                <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="16" height="16" fill="currentColor">
                  <path d="M19,4H18V2H16V4H8V2H6V4H5C3.89,4 3,4.9 3,6V20A2,2 0 0,0 5,22H19A2,2 0 0,0 21,20V6A2,2 0 0,0 19,4M19,20H5V10H19V20M19,8H5V6H19V8Z" />
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
                      <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="16" height="16" fill="currentColor">
                        <path d="M12,20A8,8 0 0,0 20,12A8,8 0 0,0 12,4A8,8 0 0,0 4,12A8,8 0 0,0 12,20M12,2A10,10 0 0,1 22,12A10,10 0 0,1 12,22C6.47,22 2,17.5 2,12A10,10 0 0,1 12,2M12.5,7V12.25L17,14.92L16.25,16.15L11,13V7H12.5Z" />
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
          {!title && (
            <button 
              className="close-button" 
              onMouseDown={handleCloseButtonMouseDown}
              onMouseUp={handleCloseButtonMouseUp}
              onMouseLeave={handleCloseButtonMouseLeave}
            >×</button>
          )}
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
            >
              적용
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

  return createPortal(calendarElement, document.body);
};

export default FilterCalendar; 