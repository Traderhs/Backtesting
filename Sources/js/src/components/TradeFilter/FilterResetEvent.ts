// 필터 초기화 이벤트 상수 정의
export const RESET_STRATEGY_NAME_FILTER = 'resetStrategyNameFilter';
export const RESET_SYMBOL_NAME_FILTER = 'resetSymbolNameFilter';
export const RESET_ENTRY_NAME_FILTER = 'resetEntryNameFilter';
export const RESET_EXIT_NAME_FILTER = 'resetExitNameFilter';
export const RESET_ENTRY_DIRECTION_FILTER = 'resetDirectionFilter';
export const RESET_ENTRY_TIME_FILTER = 'resetEntryTimeFilter';
export const RESET_EXIT_TIME_FILTER = 'resetExitTimeFilter';
export const RESET_HOLDING_TIME_FILTER = 'resetHoldingTimeFilter';
export const RESET_NUMERIC_FILTER = 'resetNumericFilters';

// 모든 필터 초기화 이벤트 발생 함수
export const resetAllFilters = () => {
  // 각 필터 초기화 이벤트 발생
  document.dispatchEvent(new CustomEvent(RESET_STRATEGY_NAME_FILTER));
  document.dispatchEvent(new CustomEvent(RESET_SYMBOL_NAME_FILTER));
  document.dispatchEvent(new CustomEvent(RESET_ENTRY_NAME_FILTER));
  document.dispatchEvent(new CustomEvent(RESET_EXIT_NAME_FILTER));
  document.dispatchEvent(new CustomEvent(RESET_ENTRY_DIRECTION_FILTER));
  document.dispatchEvent(new CustomEvent(RESET_ENTRY_TIME_FILTER));
  document.dispatchEvent(new CustomEvent(RESET_EXIT_TIME_FILTER));
  document.dispatchEvent(new CustomEvent(RESET_HOLDING_TIME_FILTER));
  document.dispatchEvent(new CustomEvent(RESET_NUMERIC_FILTER));
}; 