// 표준 라이브러리
#include <cmath>
#include <cstdint>
#include <deque>
#include <format>

// 파일 헤더
#include "Engines/OrderHandler.hpp"

// 내부 헤더
#include "Engines/Analyzer.hpp"
#include "Engines/BarData.hpp"
#include "Engines/BarHandler.hpp"
#include "Engines/DataUtils.hpp"
#include "Engines/Engine.hpp"
#include "Engines/Exception.hpp"
#include "Engines/Order.hpp"
#include "Engines/SymbolInfo.hpp"
#include "Engines/TimeUtils.hpp"
#include "Engines/Trade.hpp"

// 네임 스페이스
namespace backtesting {
using namespace exception;
using namespace utils;
}  // namespace backtesting

namespace backtesting::order {

// 동적으로 변경가능한 경고 메세지
inline string warn_msg;

// 경고 메세지를 로깅 후 false를 리턴하는 매크로
#define WARN_AND_RET_FALSE()                                 \
  LogFormattedInfo(WARNING_L, warn_msg, __FILE__, __LINE__); \
  return false;

// 경고 메세지를 로깅 후 단순 리턴하는 매크로
#define WARN_AND_RET()                                       \
  LogFormattedInfo(WARNING_L, warn_msg, __FILE__, __LINE__); \
  return;

// valid 검사 함수들의 리턴값을 검사하여 Invalid하면 원인과 경고 메시지를
// 로깅 후 false를 리턴하는 매크로
#define RET_FALSE_IF_INVALID(expr)                          \
  if (const optional<string>& warn = expr) {                \
    LogFormattedInfo(WARNING_L, *warn, __FILE__, __LINE__); \
    WARN_AND_RET_FALSE()                                    \
  }

// valid 검사 함수들의 리턴값을 검사하여 Invalid하면 원인과 경고 메시지를
// 로깅 후 단순 리턴하는 매크로
#define RET_IF_INVALID(expr)                                \
  if (const optional<string>& warn = expr) {                \
    LogFormattedInfo(WARNING_L, *warn, __FILE__, __LINE__); \
    WARN_AND_RET()                                          \
  }

OrderHandler::OrderHandler() = default;
void OrderHandler::Deleter::operator()(const OrderHandler* p) const {
  delete p;
}

mutex OrderHandler::mutex_;
shared_ptr<OrderHandler> OrderHandler::instance_;

shared_ptr<OrderHandler>& OrderHandler::GetOrderHandler() {
  lock_guard lock(mutex_);  // 스레드에서 안전하게 접근하기 위해 mutex 사용

  // 인스턴스가 존재하는지 확인
  if (!instance_) {
    // 인스턴스가 없으면 생성 후 저장
    instance_ = shared_ptr<OrderHandler>(new OrderHandler(), Deleter());
  }

  return instance_;
}

bool OrderHandler::MarketEntry(const string& entry_name,
                               const Direction entry_direction,
                               const double order_size, const int leverage) {
  warn_msg = format("시장가 [{}] 대기 주문 실패", entry_name);
  const auto symbol_idx = bar_->GetCurrentSymbolIndex();

  // 현재 바 혹은 다음 바 시가에서 진입할지 결정하는 플래그
  bool entry_now;

  // 바 정보 로딩
  int64_t order_time = 0;
  double order_price = 0.0;

  if (const auto& strategy_type = engine_->GetCurrentStrategyType();
      strategy_type == ON_CLOSE) {
    const auto& bar_data = bar_->GetBarData(bar_->GetCurrentBarType());

    // 현재 바가 마지막 바가 아닐 때만 진입 대기 주문 가능
    if (const auto current_bar_idx = bar_->GetCurrentBarIndex();
        current_bar_idx < bar_data->GetNumBars(symbol_idx) - 1) {
      const auto& next_bar = bar_data->GetBar(symbol_idx, current_bar_idx + 1);

      // On Close 전략일 시 주문 시간과 주문 가격은 다음 봉의 Open Time과 Open
      order_time = next_bar.open_time;
      order_price = next_bar.open;

      entry_now = false;
    } else [[unlikely]] {
      LogFormattedInfo(WARNING_L, "마지막 바에서 시장가 진입 대기 주문 불가",
                       __FILE__, __LINE__);

      WARN_AND_RET_FALSE()
    }
  } else {
    // After Entry, After Exit 전략일 시 주문 시간은 현재 바의 Open Time,
    // 주문 가격은 마지막 진입 가격 혹은 마지막 청산 가격
    order_time = engine_->GetCurrentOpenTime();

    if (strategy_type == AFTER_ENTRY) {
      order_price = LastEntryPrice();
    } else if (strategy_type == AFTER_EXIT) {
      order_price = LastExitPrice();
    }

    entry_now = true;
  }

  // 유효성 검사
  RET_FALSE_IF_INVALID(IsValidDirection(entry_direction))
  RET_FALSE_IF_INVALID(IsValidPositionSize(order_size, MARKET, symbol_idx))
  RET_FALSE_IF_INVALID(
      IsValidNotionalValue(order_price, order_size, symbol_idx))
  RET_FALSE_IF_INVALID(
      IsValidLeverage(leverage, order_price, order_size, symbol_idx))

  // 주문 생성
  const auto market_entry = make_shared<Order>();
  market_entry->SetLeverage(leverage)
      .SetWbWhenEntryOrder(engine_->GetWalletBalance())
      .SetEntryName(entry_name)
      .SetEntryOrderType(MARKET)
      .SetEntryDirection(entry_direction)
      .SetEntryOrderTime(order_time)
      .SetEntryOrderPrice(order_price)
      .SetEntryOrderSize(order_size)
      .SetEntryFilledTime(order_time)
      .SetEntryFilledSize(order_size);

  // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 (주문 수정)
  Cancel(entry_name);

  if (entry_now) {
    // 슬리피지가 포함된 체결가
    const double slippage_filled_price = CalculateSlippagePrice(
        MARKET, entry_direction, order_price, symbol_idx);
    market_entry->SetEntryFilledPrice(slippage_filled_price);

    // 수수료
    market_entry->SetEntryFee(
        CalculateTradingFee(MARKET, slippage_filled_price, order_size));

    // 자금 관련 처리 후 체결 주문에 추가
    return FillMarketEntry(market_entry, symbol_idx, CLOSE);
  }

  // ON_CLOSE 전략에서의 시장가 진입 대기 → 실제 체결은 다음 봉의 시가
  pending_entries_[symbol_idx].push_back(market_entry);

  return true;
}

bool OrderHandler::LimitEntry(const string& entry_name,
                              const Direction entry_direction,
                              double order_price, const double order_size,
                              const int leverage) {
  warn_msg = format("지정가 [{}] 대기 주문 실패", entry_name);
  const int symbol_idx = bar_->GetCurrentSymbolIndex();

  // 바 정보 로딩
  int64_t order_time = 0;
  double base_price = 0.0;

  if (const auto& strategy_type = engine_->GetCurrentStrategyType();
      strategy_type == ON_CLOSE) {
    const auto& bar_data = bar_->GetBarData(bar_->GetCurrentBarType());

    // 현재 바가 마지막 바가 아닐 때만 진입 대기 주문 가능
    if (const auto current_bar_idx = bar_->GetCurrentBarIndex();
        current_bar_idx < bar_data->GetNumBars(symbol_idx) - 1) {
      const auto& next_bar = bar_data->GetBar(symbol_idx, current_bar_idx + 1);

      // On Close 전략일 시 주문 시간과 기준 가격은 다음 봉의 Open Time과 Open
      order_time = next_bar.open_time;
      base_price = next_bar.open;
    } else [[unlikely]] {
      LogFormattedInfo(WARNING_L, "마지막 바에서 지정가 진입 대기 주문 불가",
                       __FILE__, __LINE__);

      WARN_AND_RET_FALSE()
    }
  } else {
    // After Entry 혹은 After Exit 전략일 시 주문 시간은 현재 바의 Open Time,
    // 기준 가격은 마지막 진입 가격 혹은 마지막 청산 가격
    order_time = engine_->GetCurrentOpenTime();

    if (strategy_type == AFTER_ENTRY) {
      base_price = LastEntryPrice();
    } else if (strategy_type == AFTER_EXIT) {
      base_price = LastExitPrice();
    }
  }

  // 주문 가격을 틱 사이즈로 반올림
  order_price =
      RoundToStep(order_price, symbol_info_[symbol_idx].GetTickSize());

  // 주문 생성
  const auto limit_entry = make_shared<Order>();
  limit_entry->SetLeverage(leverage)
      .SetWbWhenEntryOrder(engine_->GetWalletBalance())
      .SetEntryName(entry_name)
      .SetEntryOrderType(LIMIT)
      .SetEntryDirection(entry_direction)
      .SetEntryOrderTime(order_time)
      .SetEntryOrderPrice(order_price)
      .SetEntryOrderSize(order_size);

  // 유효성 검사
  RET_FALSE_IF_INVALID(IsValidDirection(entry_direction))
  RET_FALSE_IF_INVALID(IsValidPrice(order_price))
  RET_FALSE_IF_INVALID(
      IsValidLimitOrderPrice(order_price, base_price, entry_direction))
  RET_FALSE_IF_INVALID(IsValidPositionSize(order_size, LIMIT, symbol_idx))
  RET_FALSE_IF_INVALID(
      IsValidNotionalValue(order_price, order_size, symbol_idx))
  RET_FALSE_IF_INVALID(
      IsValidLeverage(leverage, order_price, order_size, symbol_idx))

  // 예약 증거금 계산
  const double entry_margin =
      CalculateMargin(order_price, order_size, CLOSE, symbol_idx);

  limit_entry->SetEntryMargin(entry_margin).SetLeftMargin(entry_margin);

  // 주문 가능 여부 체크
  RET_FALSE_IF_INVALID(HasEnoughBalance(engine_->GetAvailableBalance(),
                                        entry_margin, "사용 가능",
                                        "지정가 주문 마진"))

  // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 (주문 수정)
  Cancel(entry_name);

  // 사용한 마진에 예약 증거금 증가
  engine_->IncreaseUsedMargin(entry_margin);

  // 지정가 진입 대기
  pending_entries_[symbol_idx].push_back(limit_entry);

  LogFormattedInfo(INFO_L,
                   format("지정가 [{}] 주문 (주문가 {} | 주문량 {})",
                          entry_name, order_price, order_size),
                   __FILE__, __LINE__);
  engine_->LogBalance();

  return true;
}

bool OrderHandler::MitEntry(const string& entry_name,
                            const Direction entry_direction, double touch_price,
                            const double order_size, const int leverage) {
  warn_msg = format("MIT [{}] 대기 주문 실패", entry_name);
  const int symbol_idx = bar_->GetCurrentSymbolIndex();

  // 바 정보 로딩
  double base_price = 0.0;

  if (const auto& strategy_type = engine_->GetCurrentStrategyType();
      strategy_type == ON_CLOSE) {
    const auto& bar_data = bar_->GetBarData(bar_->GetCurrentBarType());

    // 현재 바가 마지막 바가 아닐 때만 진입 대기 주문 가능
    if (const auto current_bar_idx = bar_->GetCurrentBarIndex();
        current_bar_idx < bar_data->GetNumBars(symbol_idx) - 1) {
      const auto& next_bar = bar_data->GetBar(symbol_idx, current_bar_idx + 1);

      // On Close 전략일 시 기준 가격은 다음 봉의 Open
      base_price = next_bar.open;
    } else [[unlikely]] {
      LogFormattedInfo(WARNING_L, "마지막 바에서 MIT 진입 대기 주문 불가",
                       __FILE__, __LINE__);

      WARN_AND_RET_FALSE(order_failed_msg)
    }
  } else {
    // After Entry 혹은 After Exit 전략일 시
    // 기준 가격은 마지막 진입 가격 혹은 마지막 청산 가격
    if (strategy_type == AFTER_ENTRY) {
      base_price = LastEntryPrice();
    } else if (strategy_type == AFTER_EXIT) {
      base_price = LastExitPrice();
    }
  }

  // 터치 가격을 틱 사이즈로 반올림
  touch_price =
      RoundToStep(touch_price, symbol_info_[symbol_idx].GetTickSize());

  // 주문 생성
  const auto mit_entry = make_shared<Order>();
  mit_entry->SetLeverage(leverage)
      .SetWbWhenEntryOrder(engine_->GetWalletBalance())
      .SetEntryName(entry_name)
      .SetEntryOrderType(MIT)
      .SetEntryDirection(entry_direction)
      .SetEntryTouchPrice(touch_price)
      .SetEntryTouchDirection(IsGreaterOrEqual(touch_price, base_price) ? LONG
                                                                        : SHORT)
      .SetEntryOrderSize(order_size);

  // 유효성 검사
  RET_FALSE_IF_INVALID(IsValidDirection(entry_direction))
  RET_FALSE_IF_INVALID(IsValidPrice(touch_price))
  RET_FALSE_IF_INVALID(IsValidPositionSize(order_size, MIT, symbol_idx))
  RET_FALSE_IF_INVALID(
      IsValidNotionalValue(touch_price, order_size, symbol_idx))
  RET_FALSE_IF_INVALID(
      IsValidLeverage(leverage, touch_price, order_size, symbol_idx))

  // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 (주문 수정)
  Cancel(entry_name);

  // MIT 진입 터치 대기
  pending_entries_[symbol_idx].push_back(mit_entry);

  LogFormattedInfo(INFO_L,
                   format("MIT [{}] 대기 주문 (터치가 {} | 주문량 {})",
                          entry_name, touch_price, order_size),
                   __FILE__, __LINE__);

  return true;
}

bool OrderHandler::LitEntry(const string& entry_name,
                            const Direction entry_direction, double touch_price,
                            double order_price, const double order_size,
                            const int leverage) {
  warn_msg = format("LIT [{}] 대기 주문 실패", entry_name);
  const auto symbol_idx = bar_->GetCurrentSymbolIndex();

  // 바 정보 로딩
  double base_price = 0.0;

  if (const auto& strategy_type = engine_->GetCurrentStrategyType();
      strategy_type == ON_CLOSE) {
    const auto& bar_data = bar_->GetBarData(bar_->GetCurrentBarType());

    // 현재 바가 마지막 바가 아닐 때만 진입 대기 주문 가능
    if (const auto current_bar_idx = bar_->GetCurrentBarIndex();
        current_bar_idx < bar_data->GetNumBars(symbol_idx) - 1) {
      const auto& next_bar = bar_data->GetBar(symbol_idx, current_bar_idx + 1);

      // On Close 전략일 시 기준 가격은 다음 봉의 Open
      base_price = next_bar.open;
    } else [[unlikely]] {
      LogFormattedInfo(WARNING_L, "마지막 바에서 LIT 진입 대기 주문 불가",
                       __FILE__, __LINE__);

      WARN_AND_RET_FALSE()
    }
  } else {
    // After Entry 혹은 After Exit 전략일 시
    // 기준 가격은 마지막 진입 가격 혹은 마지막 청산 가격
    if (strategy_type == AFTER_ENTRY) {
      base_price = LastEntryPrice();
    } else if (strategy_type == AFTER_EXIT) {
      base_price = LastExitPrice();
    }
  }

  // 터치 가격과 주문 가격을 틱 사이즈로 반올림
  const auto tick_size = symbol_info_[symbol_idx].GetTickSize();
  touch_price = RoundToStep(touch_price, tick_size);
  order_price = RoundToStep(order_price, tick_size);

  // 주문 생성
  const auto lit_entry = make_shared<Order>();
  lit_entry->SetLeverage(leverage)
      .SetWbWhenEntryOrder(engine_->GetWalletBalance())
      .SetEntryName(entry_name)
      .SetEntryOrderType(LIT)
      .SetEntryDirection(entry_direction)
      .SetEntryTouchPrice(touch_price)
      .SetEntryTouchDirection(IsGreaterOrEqual(touch_price, base_price) ? LONG
                                                                        : SHORT)
      .SetEntryOrderPrice(order_price)
      .SetEntryOrderSize(order_size);

  // 유효성 검사
  RET_FALSE_IF_INVALID(IsValidDirection(entry_direction))
  RET_FALSE_IF_INVALID(IsValidPrice(touch_price))
  RET_FALSE_IF_INVALID(IsValidPrice(order_price))
  RET_FALSE_IF_INVALID(
      IsValidLimitOrderPrice(order_price, touch_price, entry_direction))
  RET_FALSE_IF_INVALID(IsValidPositionSize(order_size, LIT, symbol_idx))
  RET_FALSE_IF_INVALID(
      IsValidNotionalValue(order_price, order_size, symbol_idx))
  RET_FALSE_IF_INVALID(
      IsValidLeverage(leverage, order_price, order_size, symbol_idx))

  // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 (주문 수정)
  Cancel(entry_name);

  // LIT 진입 터치 대기
  pending_entries_[symbol_idx].push_back(lit_entry);

  LogFormattedInfo(
      INFO_L,
      format("LIT [{}] 대기 주문 (터치가 {} | 주문가 {} | 주문량 {})",
             entry_name, touch_price, order_price, order_size),
      __FILE__, __LINE__);

  return true;
}

bool OrderHandler::TrailingEntry(const string& entry_name,
                                 const Direction entry_direction,
                                 double touch_price, const double trail_point,
                                 const double order_size, const int leverage) {
  warn_msg = format("트레일링 [{}] 대기 주문 실패", entry_name);
  const int symbol_idx = bar_->GetCurrentSymbolIndex();

  // 바 정보 로딩
  double base_price = 0.0;

  if (const auto& strategy_type = engine_->GetCurrentStrategyType();
      strategy_type == ON_CLOSE) {
    const auto& bar_data = bar_->GetBarData(bar_->GetCurrentBarType());

    // 현재 바가 마지막 바가 아닐 때만 진입 대기 주문 가능
    if (const auto current_bar_idx = bar_->GetCurrentBarIndex();
        current_bar_idx < bar_data->GetNumBars(symbol_idx) - 1) {
      const auto& next_bar = bar_data->GetBar(symbol_idx, current_bar_idx + 1);

      // On Close 전략일 시 기준 가격은 다음 봉의 Open
      base_price = next_bar.open;
    } else [[unlikely]] {
      LogFormattedInfo(WARNING_L, "마지막 바에서 트레일링 진입 대기 주문 불가",
                       __FILE__, __LINE__);

      WARN_AND_RET_FALSE()
    }
  } else {
    // After Entry 혹은 After Exit 전략일 시
    // 기준 가격은 마지막 진입 가격 혹은 마지막 청산 가격
    if (strategy_type == AFTER_ENTRY) {
      base_price = LastEntryPrice();
    } else if (strategy_type == AFTER_EXIT) {
      base_price = LastExitPrice();
    }
  }

  // 터치 가격을 틱 사이즈로 반올림
  touch_price =
      RoundToStep(touch_price, symbol_info_[symbol_idx].GetTickSize());

  // 주문 생성
  const auto trailing_entry = make_shared<Order>();
  trailing_entry->SetLeverage(leverage)
      .SetWbWhenEntryOrder(engine_->GetWalletBalance())
      .SetEntryName(entry_name)
      .SetEntryOrderType(TRAILING)
      .SetEntryDirection(entry_direction)
      .SetEntryTouchPrice(touch_price)
      .SetEntryTouchDirection(IsGreaterOrEqual(touch_price, base_price) ? LONG
                                                                        : SHORT)
      .SetEntryTrailPoint(trail_point)
      .SetEntryOrderSize(order_size);

  // touch_price가 0이면 다음 시가부터 최고저가 추적을 시작
  if (IsEqual(touch_price, 0.0)) {
    trailing_entry->SetEntryExtremePrice(base_price);
  }

  // 유효성 검사
  RET_FALSE_IF_INVALID(IsValidDirection(entry_direction))
  RET_FALSE_IF_INVALID(IsValidTrailingTouchPrice(touch_price))
  RET_FALSE_IF_INVALID(IsValidTrailPoint(trail_point))
  RET_FALSE_IF_INVALID(IsValidPositionSize(order_size, TRAILING, symbol_idx))

  double start_price;
  if (IsEqual(touch_price, 0.0)) {
    start_price = base_price;
  } else {
    start_price = touch_price;
  }

  const double target_price =  // 가장 불리한 진입가로 검사
      entry_direction == LONG ? start_price + trail_point
                              : start_price - trail_point;

  RET_FALSE_IF_INVALID(
      IsValidNotionalValue(target_price, order_size, symbol_idx))
  RET_FALSE_IF_INVALID(
      IsValidLeverage(leverage, target_price, order_size, symbol_idx))

  // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 (주문 수정)
  Cancel(entry_name);

  // 트레일링 진입 터치 대기
  pending_entries_[symbol_idx].push_back(trailing_entry);

  LogFormattedInfo(INFO_L,
                   format("트레일링 [{}] 대기 주문 (터치가 {} | "
                          "트레일 포인트 {} | 주문량 {})",
                          entry_name, touch_price, trail_point, order_size),
                   __FILE__, __LINE__);

  return true;
}

bool OrderHandler::MarketExit(const string& exit_name,
                              const string& target_name, double order_size) {
  warn_msg = format("시장가 [{}] 대기 주문 실패", exit_name);
  const auto symbol_idx = bar_->GetCurrentSymbolIndex();

  // 현재 바 혹은 다음 바 시가에서 청산할지 결정하는 플래그
  bool exit_now;

  // 바 정보 로딩
  int64_t order_time = 0;
  double order_price = 0.0;

  // 바 종가에 시장가 청산하도록 만드는 람다 함수
  const auto market_exit_on_close = [&] {
    const auto& current_bar =
        bar_->GetBarData(bar_->GetCurrentBarType())
            ->GetBar(symbol_idx, bar_->GetCurrentBarIndex());

    order_time = current_bar.close_time;
    order_price = current_bar.close;

    exit_now = true;
  };

  // 청산 시간 및 청산 가격을 결정
  // 모든 심볼의 트레이딩이 끝나지 않았다면 세부 로직에 따라 처리.
  // 끝났다면 종가에 청산.
  if (!engine_->IsAllTradingEnded()) {
    // 리버스 청산일 시 주문 시간과 주문 가격은 현재 봉의 Open Time과
    // 리버스 목표 진입 주문의 진입 주문 가격
    if (is_reverse_exit_) {
      order_time = engine_->GetCurrentOpenTime();
      order_price = reverse_exit_price_;

      exit_now = true;
    } else if (const auto& strategy_type = engine_->GetCurrentStrategyType();
               strategy_type == ON_CLOSE) {
      const auto& bar_data = bar_->GetBarData(bar_->GetCurrentBarType());

      // 현재 바가 마지막 바가 아닐 때만 청산 대기 주문 가능
      if (const auto current_bar_idx = bar_->GetCurrentBarIndex();
          current_bar_idx < bar_data->GetNumBars(symbol_idx) - 1) {
        const auto& next_bar =
            bar_data->GetBar(symbol_idx, current_bar_idx + 1);

        // On Close 전략일 시 주문 시간과 주문 가격은 다음 봉의 Open Time과 Open
        order_time = next_bar.open_time;
        order_price = next_bar.open;

        exit_now = false;
      } else {
        // 현재 바가 마지막 바인 경우 현재 바 종가에 청산
        market_exit_on_close();
      }
    } else {
      // After Entry, After Exit 전략일 시 주문 시간은 현재 바의 Open Time,
      // 주문 가격은 마지막 진입 가격 혹은 마지막 청산 가격
      order_time = engine_->GetCurrentOpenTime();

      if (strategy_type == AFTER_ENTRY) {
        order_price = LastEntryPrice();
      } else if (strategy_type == AFTER_EXIT) {
        order_price = LastExitPrice();
      }

      exit_now = true;
    }
  } else [[unlikely]] {
    // 모든 트레이딩이 끝나고 전량 청산은 바 종가에서 시장가 청산
    market_exit_on_close();
  }

  // 원본 진입 주문 찾기
  shared_ptr<Order> entry_order;
  int entry_order_idx;

  if (const auto& result = FindEntryOrder(target_name, symbol_idx)) {
    const auto& [order, index] = *result;
    entry_order = order;
    entry_order_idx = index;
  } else [[unlikely]] {
    // 원본 진입 주문을 찾지 못하면 청산 실패
    LogFormattedInfo(
        WARNING_L,
        format("원본 진입 주문 [{}] 미존재로 청산 불가", target_name), __FILE__,
        __LINE__);

    WARN_AND_RET_FALSE()
  }

  // 시장가에서 주문 가격은 항상 바 데이터 가격이므로 틱 사이즈로 조정 불필요

  // 총 청산 주문 수량이 진입 체결 수량보다 크지 않게 조정
  order_size = GetAdjustedExitSize(order_size, entry_order);

  // 청산 주문 생성
  const auto market_exit = make_shared<Order>(*entry_order);
  market_exit->SetExitName(exit_name)
      .SetExitOrderType(MARKET)
      .SetExitDirection(market_exit->GetEntryDirection() == LONG ? SHORT : LONG)
      .SetExitOrderTime(order_time)
      .SetExitOrderPrice(order_price)
      .SetExitOrderSize(order_size)
      .SetExitFilledTime(order_time)
      .SetExitFilledSize(order_size);

  // 유효성 검사
  RET_FALSE_IF_INVALID(IsValidExitName(exit_name))
  RET_FALSE_IF_INVALID(IsValidPositionSize(order_size, MARKET, symbol_idx))
  RET_FALSE_IF_INVALID(
      IsValidNotionalValue(order_price, order_size, symbol_idx))

  // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 (주문 수정)
  Cancel(exit_name);

  if (exit_now) {
    // 슬리피지가 포함된 체결가
    const double slippage_filled_price = CalculateSlippagePrice(
        MARKET, market_exit->GetExitDirection(), order_price, symbol_idx);
    market_exit->SetExitFilledPrice(slippage_filled_price);

    // 수수료
    market_exit->SetExitFee(
        CalculateTradingFee(MARKET, slippage_filled_price, order_size));

    // 원본 진입 주문에 청산 체결 수량 추가
    const double total_exit_filled_size =
        entry_order->GetExitFilledSize() + order_size;
    entry_order->SetExitFilledSize(total_exit_filled_size);

    // 원본 진입 주문의 청산 체결 수량이 진입 체결 수량과 같으면
    // filled_entries에서 삭제
    if (IsEqual(total_exit_filled_size, entry_order->GetEntryFilledSize())) {
      auto& filled_entries = filled_entries_[symbol_idx];
      filled_entries.erase(filled_entries.begin() + entry_order_idx);

      // 같은 진입 이름을 목표로 하는 청산 대기 주문 취소
      // 여러 주문이 삭제될 수 있으므로 역순으로 순회
      auto& pending_exits = pending_exits_[symbol_idx];
      for (int order_idx = static_cast<int>(pending_exits.size()) - 1;
           order_idx >= 0; order_idx--) {
        if (const auto& pending_exit = pending_exits[order_idx];
            target_name == pending_exit->GetEntryName()) {
          // 청산 대기 주문은 예약 증거금이 필요하지 않기 때문에 삭제만 함
          pending_exits.erase(pending_exits.begin() + order_idx);

          LogFormattedInfo(
              INFO_L,
              format("{} [{}] 주문 취소 (원본 진입 전량 청산)",
                     Order::OrderTypeToString(pending_exit->GetExitOrderType()),
                     pending_exit->GetExitName()),
              __FILE__, __LINE__);
        }
      }
    }

    // 자금, 통계 업데이트
    ExecuteExit(market_exit, symbol_idx);
  } else {
    // ON_CLOSE 전략에서의 시장가 청산 대기 → 실제 체결은 다음 봉의 시가
    pending_exits_[symbol_idx].push_back(market_exit);
  }

  return true;
}

bool OrderHandler::LimitExit(const string& exit_name, const string& target_name,
                             double order_price, double order_size) {
  warn_msg = format("지정가 [{}] 대기 주문 실패", exit_name);
  const int symbol_idx = bar_->GetCurrentSymbolIndex();

  // 바 정보 로딩
  int64_t order_time = 0;
  double base_price = 0.0;

  if (const auto& strategy_type = engine_->GetCurrentStrategyType();
      strategy_type == ON_CLOSE) {
    const auto& bar_data = bar_->GetBarData(bar_->GetCurrentBarType());

    // 현재 바가 마지막 바가 아닐 때만 청산 대기 주문 가능
    if (const auto current_bar_idx = bar_->GetCurrentBarIndex();
        current_bar_idx < bar_data->GetNumBars(symbol_idx) - 1) {
      const auto& next_bar = bar_data->GetBar(symbol_idx, current_bar_idx + 1);

      // On Close 전략일 시 주문 시간과 주문 가격은 다음 봉의 Open Time과 Open
      order_time = next_bar.open_time;
      base_price = next_bar.open;
    } else [[unlikely]] {
      LogFormattedInfo(WARNING_L, "마지막 바에서 지정가 청산 대기 주문 불가",
                       __FILE__, __LINE__);

      WARN_AND_RET_FALSE()
    }
  } else {
    // After Entry 혹은 After Exit 전략일 시 주문 시간은 현재 바의 Open Time,
    // 기준 가격은 마지막 진입 가격 혹은 마지막 청산 가격
    order_time = engine_->GetCurrentOpenTime();

    if (strategy_type == AFTER_ENTRY) {
      base_price = LastEntryPrice();
    } else if (strategy_type == AFTER_EXIT) {
      base_price = LastExitPrice();
    }
  }

  // 원본 진입 주문 찾기
  shared_ptr<Order> entry_order;

  if (const auto& result = FindEntryOrder(target_name, symbol_idx)) {
    entry_order = result->first;
  } else [[unlikely]] {
    // 원본 진입 주문을 찾지 못하면 청산 실패
    LogFormattedInfo(
        WARNING_L,
        format("원본 진입 주문 [{}] 미존재로 청산 불가", target_name), __FILE__,
        __LINE__);

    WARN_AND_RET_FALSE()
  }

  // 주문 가격을 틱 사이즈로 반올림
  order_price =
      RoundToStep(order_price, symbol_info_[symbol_idx].GetTickSize());

  // 총 청산 주문 수량이 진입 체결 수량보다 크지 않게 조정
  order_size = GetAdjustedExitSize(order_size, entry_order);

  // 청산 주문 생성
  const auto limit_exit = make_shared<Order>(*entry_order);
  limit_exit->SetExitName(exit_name)
      .SetExitOrderType(LIMIT)
      .SetExitDirection(limit_exit->GetEntryDirection() == LONG ? SHORT : LONG)
      .SetExitOrderTime(order_time)
      .SetExitOrderPrice(order_price)
      .SetExitOrderSize(order_size);

  // 유효성 검사
  RET_FALSE_IF_INVALID(IsValidExitName(exit_name))
  RET_FALSE_IF_INVALID(IsValidPrice(order_price))
  RET_FALSE_IF_INVALID(IsValidLimitOrderPrice(order_price, base_price,
                                              limit_exit->GetExitDirection()))
  RET_FALSE_IF_INVALID(IsValidPositionSize(order_size, LIMIT, symbol_idx))
  RET_FALSE_IF_INVALID(
      IsValidNotionalValue(order_price, order_size, symbol_idx))

  // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 (주문 수정)
  Cancel(exit_name);

  // 대기 중인 청산에 추가
  pending_exits_[symbol_idx].push_back(limit_exit);

  LogFormattedInfo(INFO_L,
                   format("지정가 [{}] 대기 주문 (주문가 {} | 주문량 {})",
                          exit_name, order_price, order_size),
                   __FILE__, __LINE__);

  return true;
}

bool OrderHandler::MitExit(const string& exit_name, const string& target_name,
                           double touch_price, double order_size) {
  warn_msg = format("MIT [{}] 대기 주문 실패", exit_name);
  const auto symbol_idx = bar_->GetCurrentSymbolIndex();

  // 바 정보 로딩
  double base_price = 0.0;

  if (const auto& strategy_type = engine_->GetCurrentStrategyType();
      strategy_type == ON_CLOSE) {
    const auto& bar_data = bar_->GetBarData(bar_->GetCurrentBarType());

    // 현재 바가 마지막 바가 아닐 때만 청산 대기 주문 가능
    if (const auto current_bar_idx = bar_->GetCurrentBarIndex();
        current_bar_idx < bar_data->GetNumBars(symbol_idx) - 1) {
      const auto& next_bar = bar_data->GetBar(symbol_idx, current_bar_idx + 1);

      // On Close 전략일 시 기준 가격은 다음 봉의 Open
      base_price = next_bar.open;
    } else [[unlikely]] {
      LogFormattedInfo(WARNING_L, "마지막 바에서 MIT 청산 대기 주문 불가",
                       __FILE__, __LINE__);

      WARN_AND_RET_FALSE()
    }
  } else {
    // After Entry 혹은 After Exit 전략일 시
    // 기준 가격은 마지막 진입 가격 혹은 마지막 청산 가격
    if (strategy_type == AFTER_ENTRY) {
      base_price = LastEntryPrice();
    } else if (strategy_type == AFTER_EXIT) {
      base_price = LastExitPrice();
    }
  }

  // 원본 진입 주문 찾기
  shared_ptr<Order> entry_order;

  if (const auto& result = FindEntryOrder(target_name, symbol_idx)) {
    entry_order = result->first;
  } else [[unlikely]] {
    // 원본 진입 주문을 찾지 못하면 청산 실패
    LogFormattedInfo(
        WARNING_L,
        format("원본 진입 주문 [{}] 미존재로 청산 불가", target_name), __FILE__,
        __LINE__);

    WARN_AND_RET_FALSE()
  }

  // 터치 가격을 틱 사이즈로 반올림
  touch_price =
      RoundToStep(touch_price, symbol_info_[symbol_idx].GetTickSize());

  // 총 청산 주문 수량이 진입 체결 수량보다 크지 않게 조정
  order_size = GetAdjustedExitSize(order_size, entry_order);

  // 청산 주문 생성
  const auto mit_exit = make_shared<Order>(*entry_order);
  mit_exit->SetExitName(exit_name)
      .SetExitOrderType(MIT)
      .SetExitDirection(mit_exit->GetEntryDirection() == LONG ? SHORT : LONG)
      .SetExitTouchPrice(touch_price)
      .SetExitTouchDirection(IsGreaterOrEqual(touch_price, base_price) ? LONG
                                                                       : SHORT)
      .SetExitOrderSize(order_size);

  // 유효성 검사
  RET_FALSE_IF_INVALID(IsValidExitName(exit_name))
  RET_FALSE_IF_INVALID(IsValidPrice(touch_price))
  RET_FALSE_IF_INVALID(IsValidPositionSize(order_size, MIT, symbol_idx))
  RET_FALSE_IF_INVALID(
      IsValidNotionalValue(touch_price, order_size, symbol_idx))

  // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 (주문 수정)
  Cancel(exit_name);

  // 대기 중인 청산에 추가
  pending_exits_[symbol_idx].push_back(mit_exit);

  LogFormattedInfo(INFO_L,
                   format("MIT [{}] 대기 주문 (터치가 {} | 주문량 {})",
                          exit_name, touch_price, order_size),
                   __FILE__, __LINE__);

  return true;
}

bool OrderHandler::LitExit(const string& exit_name, const string& target_name,
                           double touch_price, double order_price,
                           double order_size) {
  warn_msg = format("LIT [{}] 대기 주문 실패", exit_name);
  const auto symbol_idx = bar_->GetCurrentSymbolIndex();

  // 바 정보 로딩
  double base_price = 0.0;

  if (const auto& strategy_type = engine_->GetCurrentStrategyType();
      strategy_type == ON_CLOSE) {
    const auto& bar_data = bar_->GetBarData(bar_->GetCurrentBarType());

    // 현재 바가 마지막 바가 아닐 때만 청산 대기 주문 가능
    if (const auto current_bar_idx = bar_->GetCurrentBarIndex();
        current_bar_idx < bar_data->GetNumBars(symbol_idx) - 1) {
      const auto& next_bar = bar_data->GetBar(symbol_idx, current_bar_idx + 1);

      // On Close 전략일 시 기준 가격은 다음 봉의 Open
      base_price = next_bar.open;
    } else [[unlikely]] {
      LogFormattedInfo(WARNING_L, "마지막 바에서 LIT 청산 대기 주문 불가",
                       __FILE__, __LINE__);

      WARN_AND_RET_FALSE()
    }
  } else {
    // After Entry 혹은 After Exit 전략일 시
    // 기준 가격은 마지막 진입 가격 혹은 마지막 청산 가격
    if (strategy_type == AFTER_ENTRY) {
      base_price = LastEntryPrice();
    } else if (strategy_type == AFTER_EXIT) {
      base_price = LastExitPrice();
    }
  }

  // 원본 진입 주문 찾기
  shared_ptr<Order> entry_order;

  if (const auto& result = FindEntryOrder(target_name, symbol_idx)) {
    entry_order = result->first;
  } else [[unlikely]] {
    // 원본 진입 주문을 찾지 못하면 청산 실패
    LogFormattedInfo(
        WARNING_L,
        format("원본 진입 주문 [{}] 미존재로 청산 불가", target_name), __FILE__,
        __LINE__);

    WARN_AND_RET_FALSE()
  }

  // 터치 가격과 주문 가격을 틱 사이즈로 반올림
  const auto tick_size = symbol_info_[symbol_idx].GetTickSize();
  touch_price = RoundToStep(touch_price, tick_size);
  order_price = RoundToStep(order_price, tick_size);

  // 총 청산 주문 수량이 진입 체결 수량보다 크지 않게 조정
  order_size = GetAdjustedExitSize(order_size, entry_order);

  // 청산 주문 생성
  const auto lit_exit = make_shared<Order>(*entry_order);
  lit_exit->SetExitName(exit_name)
      .SetExitOrderType(LIT)
      .SetExitDirection(lit_exit->GetEntryDirection() == LONG ? SHORT : LONG)
      .SetExitTouchPrice(touch_price)
      .SetExitTouchDirection(IsGreaterOrEqual(touch_price, base_price) ? LONG
                                                                       : SHORT)
      .SetExitOrderPrice(order_price)
      .SetExitOrderSize(order_size);

  // 유효성 검사
  RET_FALSE_IF_INVALID(IsValidExitName(exit_name))
  RET_FALSE_IF_INVALID(IsValidPrice(touch_price))
  RET_FALSE_IF_INVALID(IsValidPrice(order_price))
  RET_FALSE_IF_INVALID(IsValidLimitOrderPrice(order_price, touch_price,
                                              lit_exit->GetExitDirection()))
  RET_FALSE_IF_INVALID(IsValidPositionSize(order_size, LIT, symbol_idx))
  RET_FALSE_IF_INVALID(
      IsValidNotionalValue(order_price, order_size, symbol_idx))

  // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 (주문 수정)
  Cancel(exit_name);

  // 대기 중인 청산에 추가
  pending_exits_[symbol_idx].push_back(lit_exit);

  LogFormattedInfo(
      INFO_L,
      format("LIT [{}] 대기 주문 (터치가 {} | 주문가 {} | 주문량 {})",
             exit_name, touch_price, order_price, order_size),
      __FILE__, __LINE__);

  return true;
}

bool OrderHandler::TrailingExit(const string& exit_name,
                                const string& target_name, double touch_price,
                                const double trail_point, double order_size) {
  warn_msg = format("트레일링 [{}] 대기 주문 실패", exit_name);
  const auto symbol_idx = bar_->GetCurrentSymbolIndex();

  // 바 정보 로딩
  double base_price = 0.0;

  if (const auto& strategy_type = engine_->GetCurrentStrategyType();
      strategy_type == ON_CLOSE) {
    const auto& bar_data = bar_->GetBarData(bar_->GetCurrentBarType());

    // 현재 바가 마지막 바가 아닐 때만 청산 대기 주문 가능
    if (const auto current_bar_idx = bar_->GetCurrentBarIndex();
        current_bar_idx < bar_data->GetNumBars(symbol_idx) - 1) {
      const auto& next_bar = bar_data->GetBar(symbol_idx, current_bar_idx + 1);

      // On Close 전략일 시 기준 가격은 다음 봉의 Open
      base_price = next_bar.open;
    } else [[unlikely]] {
      LogFormattedInfo(WARNING_L, "마지막 바에서 트레일링 청산 대기 주문 불가",
                       __FILE__, __LINE__);

      WARN_AND_RET_FALSE()
    }
  } else {
    // After Entry 혹은 After Exit 전략일 시
    // 기준 가격은 마지막 진입 가격 혹은 마지막 청산 가격
    if (strategy_type == AFTER_ENTRY) {
      base_price = LastEntryPrice();
    } else if (strategy_type == AFTER_EXIT) {
      base_price = LastExitPrice();
    }
  }

  // 원본 진입 주문 찾기
  shared_ptr<Order> entry_order;

  if (const auto& result = FindEntryOrder(target_name, symbol_idx)) {
    entry_order = result->first;
  } else [[unlikely]] {
    // 원본 진입 주문을 찾지 못하면 청산 실패
    LogFormattedInfo(
        WARNING_L,
        format("원본 진입 주문 [{}] 미존재로 청산 불가", target_name), __FILE__,
        __LINE__);

    WARN_AND_RET_FALSE()
  }

  // 터치 가격을 틱 사이즈로 반올림
  touch_price =
      RoundToStep(touch_price, symbol_info_[symbol_idx].GetTickSize());

  // 총 청산 주문 수량이 진입 체결 수량보다 크지 않게 조정
  order_size = GetAdjustedExitSize(order_size, entry_order);

  // 청산 주문 생성
  const auto trailing_exit = make_shared<Order>(*entry_order);
  trailing_exit->SetExitName(exit_name)
      .SetExitOrderType(TRAILING)
      .SetExitDirection(trailing_exit->GetEntryDirection() == LONG ? SHORT
                                                                   : LONG)
      .SetExitTouchPrice(touch_price)
      .SetExitTouchDirection(IsGreaterOrEqual(touch_price, base_price) ? LONG
                                                                       : SHORT)
      .SetExitTrailPoint(trail_point)
      .SetExitOrderSize(order_size);

  // touch_price가 0이면 다음 시가부터 최고저가 추적을 시작
  if (IsEqual(touch_price, 0.0)) {
    trailing_exit->SetEntryExtremePrice(base_price);
  }

  // 유효성 검사
  RET_FALSE_IF_INVALID(IsValidExitName(exit_name))
  RET_FALSE_IF_INVALID(IsValidTrailingTouchPrice(touch_price))
  RET_FALSE_IF_INVALID(IsValidTrailPoint(trail_point))
  RET_FALSE_IF_INVALID(IsValidPositionSize(order_size, TRAILING, symbol_idx))

  double start_price;
  if (IsEqual(touch_price, 0.0)) {
    start_price = base_price;
  } else {
    start_price = touch_price;
  }

  RET_FALSE_IF_INVALID(IsValidNotionalValue(  // 가장 불리한 청산가로 검사
      trailing_exit->GetExitDirection() == LONG ? start_price + trail_point
                                                : start_price - trail_point,
      order_size, symbol_idx))

  // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 (주문 수정)
  Cancel(exit_name);

  // 대기 중인 청산에 추가
  pending_exits_[symbol_idx].push_back(trailing_exit);

  LogFormattedInfo(INFO_L,
                   format("트레일링 [{}] 대기 주문 (터치가 {} | "
                          "트레일 포인트 {} | 주문량 {})",
                          exit_name, touch_price, trail_point, order_size),
                   __FILE__, __LINE__);

  return true;
}

void OrderHandler::CancelAll() {
  const auto symbol_idx = bar_->GetCurrentSymbolIndex();

  // 루프 중간에 주문이 삭제되므로 역순으로 순회
  const auto& pending_entries = pending_entries_[symbol_idx];
  for (int order_idx = static_cast<int>(pending_entries.size()) - 1;
       order_idx >= 0; order_idx--) {
    Cancel(pending_entries[order_idx]->GetEntryName());
  }

  const auto& pending_exits = pending_exits_[symbol_idx];
  for (int order_idx = static_cast<int>(pending_exits.size()) - 1;
       order_idx >= 0; order_idx--) {
    Cancel(pending_exits[order_idx]->GetExitName());
  }
}

void OrderHandler::CloseAll() {
  const auto original_strategy_type = engine_->GetCurrentStrategyType();

  // 다음 바 시가에서 청산시키기 위하여 ON_CLOSE 전략으로 일시 설정
  engine_->SetCurrentStrategyType(ON_CLOSE);

  for (const auto& filled_entry :
       filled_entries_[bar_->GetCurrentSymbolIndex()]) {
    MarketExit(
        "전량 청산", filled_entry->GetEntryName(),
        filled_entry->GetEntryFilledSize() - filled_entry->GetExitFilledSize());
  }

  engine_->SetCurrentStrategyType(original_strategy_type);
}

vector<FillInfo> OrderHandler::CheckLiquidation(
    const BarType market_bar_type, const int symbol_idx, const double price,
    const PriceType price_type) const {
  const auto market_bar_data = bar_->GetBarData(market_bar_type);

  const auto& filled_entries = filled_entries_[symbol_idx];
  vector<FillInfo> should_liquidate_filled_entries;
  should_liquidate_filled_entries.reserve(filled_entries.size());

  for (const auto& filled_entry : filled_entries) {
    const auto liquidation_price = filled_entry->GetLiquidationPrice();

    // 매수 진입 → 현재 가격이 강제 청산 가격과 같거나 밑일 때
    // 매도 진입 → 현재 가격이 강제 청산 가격과 같거나 위일 때
    if (const auto entry_direction = filled_entry->GetEntryDirection();
        (entry_direction == LONG && IsLessOrEqual(price, liquidation_price)) ||
        (entry_direction == SHORT &&
         IsGreaterOrEqual(price, liquidation_price))) {
      // 실제 시장 가격 찾기
      double fill_price = NAN;

      // 마크 가격과 시장 가격이 다르기 때문에 실제 시장 체결 가격으로 조정 필요
      // ex) Mark High에서 LP까지의 차이 == Market High에서 LP까지의 차이
      switch (price_type) {
        case OPEN: {
          // 시가에서의 강제 청산은 갭 때문이므로 체결 가격은 시가 그 자체
          fill_price =
              market_bar_data->GetBar(symbol_idx, bar_->GetCurrentBarIndex())
                  .open;
          break;
        }

        case HIGH: {
          // 마크 고가에서 청산 가격까지의 차이를 구하여 실제 시장 가격에서 조정
          fill_price =
              market_bar_data->GetBar(symbol_idx, bar_->GetCurrentBarIndex())
                  .high -
              fabs(price - liquidation_price);
          break;
        }

        case LOW: {
          // 청산 가격에서 마크 저가까지의 차이를 구하여 실제 시장 가격에서 조정
          fill_price =
              market_bar_data->GetBar(symbol_idx, bar_->GetCurrentBarIndex())
                  .low +
              fabs(liquidation_price - price);
          break;
        }

        case CLOSE: {
          fill_price =
              market_bar_data->GetBar(symbol_idx, bar_->GetCurrentBarIndex())
                  .close;
          break;
        }
      }

      // 체결된 진입을 넣음에 주의
      should_liquidate_filled_entries.push_back(
          FillInfo{filled_entry, LIQUIDATION, fill_price});
    }
  }

  return should_liquidate_filled_entries;
}

vector<FillInfo> OrderHandler::CheckPendingExits(
    const int symbol_idx, const double price,
    const PriceType price_type) const {
  const auto& pending_exits = pending_exits_[symbol_idx];
  vector<FillInfo> should_fill_pending_exits;
  should_fill_pending_exits.reserve(pending_exits.size());

  for (const auto& pending_exit : pending_exits) {
    // 주문 타입별로 조건이 만족되면 체결해야 하는 청산 주문 목록에 주문을 추가
    switch (pending_exit->GetExitOrderType()) {
      case MARKET: {
        // 시장가 대기 주문은 ON_CLOSE 주문에서만 생기므로,
        // 이 함수에 들어온 타이밍은 무조건 시가이기 때문에 즉시 체결 가능
        // 체결 가격은 시장가 정의에 따라 시가로 결정
        should_fill_pending_exits.push_back(
            FillInfo{pending_exit, EXIT, price});

        continue;
      }

      case LIMIT: {
        if (const auto& order_info =
                CheckPendingLimitExit(pending_exit, price, price_type)) {
          should_fill_pending_exits.push_back(*order_info);
        }

        continue;
      }

      case MIT: {
        if (const auto& order_info =
                CheckPendingMitExit(pending_exit, price, price_type)) {
          should_fill_pending_exits.push_back(*order_info);
        }

        continue;
      }

      case LIT: {
        if (const auto& order_info =
                CheckPendingLitExit(pending_exit, price, price_type)) {
          should_fill_pending_exits.push_back(*order_info);
        }

        continue;
      }

      case TRAILING: {
        if (const auto& order_info =
                CheckPendingTrailingExit(pending_exit, price, price_type)) {
          should_fill_pending_exits.push_back(*order_info);
        }

        continue;
      }

      [[unlikely]] case ORDER_NONE: {
        LogFormattedInfo(WARNING_L, "청산 대기 주문에 NONE 주문 존재", __FILE__,
                         __LINE__);

        throw;
      }
    }
  }

  return should_fill_pending_exits;
}

vector<FillInfo> OrderHandler::CheckPendingEntries(const int symbol_idx,
                                                   const double price,
                                                   const PriceType price_type) {
  const auto& pending_entries = pending_entries_[symbol_idx];
  vector<FillInfo> should_fill_pending_entries;
  should_fill_pending_entries.reserve(pending_entries.size());

  for (int order_idx = 0; order_idx < pending_entries.size(); order_idx++) {
    // 주문 타입별로 조건이 만족되면 체결해야 하는 진입 주문 목록에 주문을 추가
    switch (const auto& pending_entry = pending_entries[order_idx];
            pending_entry->GetEntryOrderType()) {
      case MARKET: {
        // 시장가 대기 주문은 ON_CLOSE 주문에서만 생기므로,
        // 이 함수에 들어온 타이밍은 무조건 시가이기 때문에 즉시 체결 가능
        // 체결 가격은 시장가 정의에 따라 시가로 결정
        should_fill_pending_entries.push_back(
            FillInfo{pending_entry, ENTRY, price});

        continue;
      }

      case LIMIT: {
        if (const auto& order_info =
                CheckPendingLimitEntry(pending_entry, price, price_type)) {
          should_fill_pending_entries.push_back(*order_info);
        }

        continue;
      }

      case MIT: {
        if (const auto& order_info =
                CheckPendingMitEntry(pending_entry, price, price_type)) {
          should_fill_pending_entries.push_back(*order_info);
        }

        continue;
      }

      case LIT: {
        if (const auto& order_info = CheckPendingLitEntry(
                pending_entry, order_idx, symbol_idx, price, price_type)) {
          should_fill_pending_entries.push_back(*order_info);
        }

        continue;
      }

      case TRAILING: {
        if (const auto& order_info =
                CheckPendingTrailingEntry(pending_entry, price, price_type)) {
          should_fill_pending_entries.push_back(*order_info);
        }

        continue;
      }

      [[unlikely]] case ORDER_NONE: {
        LogFormattedInfo(ERROR_L, "진입 대기 주문에 NONE 주문 존재", __FILE__,
                         __LINE__);

        throw;
      }
    }
  }

  return should_fill_pending_entries;
}

void OrderHandler::FillOrder(const FillInfo& order_info, const int symbol_idx,
                             const PriceType price_type) {
  switch (const auto& [order, order_signal, fill_price] = order_info;
          order_signal) {
    case LIQUIDATION: {
      // 먼저 다른 주문에서 원본 진입 주문을 청산했으면
      // 강제 청산의 의미가 없으므로 바로 리턴
      if (FindEntryOrder(order->GetEntryName(), symbol_idx)) {
        // 강제 청산의 order는 CheckLiquidation에서 filled_entry에 있는
        // 체결된 진입을 추가함 (강제 청산은 대기 주문 미존재)
        FillLiquidation(order, "강제 청산 (청산가)", symbol_idx, fill_price);
      }

      return;
    }

    case EXIT: {
      // 먼저 다른 주문에서 원본 진입 주문을 청산했으면 청산 불가
      // → 이 청산 주문 자체는 다른 청산 체결 시 이미 취소됨
      if (const auto& entry_order =
              FindEntryOrder(order->GetEntryName(), symbol_idx)) {
        FillPendingExitOrder(order, *entry_order, symbol_idx, fill_price);
      }

      return;
    }

    // 진입 대기 주문은 다른 체결 주문에 의해 영향 없음
    case ENTRY: {
      switch (order->GetEntryOrderType()) {
        case MARKET:
          [[fallthrough]];
        case MIT:
          [[fallthrough]];
        case TRAILING: {
          FillPendingMarketEntry(order, symbol_idx, fill_price, price_type);

          return;
        }

        case LIMIT:
          [[fallthrough]];
        case LIT: {
          FillPendingLimitEntry(order, symbol_idx, fill_price, price_type);

          return;
        }

        default: {
          Logger::LogAndThrowError(
              "엔진 오류: 잘못된 전략 타입 ORDER_NONE이 지정되었으므로 "
              "주문을 실행할 수 없습니다.",
              __FILE__, __LINE__);
        }
      }
    }
  }
}

void OrderHandler::ExecuteFunding(const double funding_rate,
                                  const string& funding_time,
                                  const double funding_price,
                                  const int symbol_idx) {
  // 주문마다 펀딩비 정산
  // ※ 체결된 주문이 없으면 펀딩비는 없음
  // ※ 펀딩비 부족 시 강제 청산될 수 있고, 강제 청산된 주문은 진입 체결 주문에서
  //    삭제되므로 역순으로 조회
  const auto& filled_entries = filled_entries_[symbol_idx];
  for (int order_idx = static_cast<int>(filled_entries.size() - 1);
       order_idx >= 0; order_idx--) {
    auto& filled_entry = filled_entries[order_idx];
    const auto& entry_name = filled_entry->GetEntryName();

    // 펀딩비: 펀딩 비율 * 펀딩 가격(마크 가격) * 진입 포지션 잔량
    const auto entry_direction = filled_entry->GetEntryDirection();
    const auto left_position_size =
        filled_entry->GetEntryFilledSize() - filled_entry->GetExitFilledSize();
    double funding_amount =
        fabs(funding_rate * funding_price * left_position_size);
    const double abs_funding_amount = funding_amount;

    // 포지션 방향에 따라 지불인지 수령인지 결정
    // 펀딩 비율 양수: 롱은 지불, 숏은 수령
    // 펀딩 비율 음수: 롱은 수령, 숏은 지불
    if ((IsGreater(funding_rate, 0.0) && entry_direction == LONG) ||
        (IsLess(funding_rate, 0.0) && entry_direction == SHORT)) {
      funding_amount = -funding_amount;
    }

    if (IsGreater(funding_amount, 0.0)) {
      // 펀딩비 수령은 단순히 지갑 자금에 합산
      engine_->IncreaseWalletBalance(funding_amount);

      const auto received_funding_amount =
          filled_entry->GetReceivedFundingAmount() + abs_funding_amount;

      // 진입 주문에 펀딩비 정산
      filled_entry->AddReceivedFundingCount().SetReceivedFundingAmount(
          received_funding_amount);

      // 해당 진입 주문을 목표로 하는 청산 대기 주문에 펀딩비 정산
      for (const auto& pending_exit : pending_exits_[symbol_idx]) {
        if (pending_exit->GetEntryName() == entry_name) {
          pending_exit->AddReceivedFundingCount().SetReceivedFundingAmount(
              received_funding_amount);
        }
      }

      LogFormattedInfo(
          INFO_L,
          format("[{}] 펀딩비 [{}] 수령 (펀딩 시간 {} | 펀딩 비율 "
                 "{} | 펀딩 가격 {} | 포지션 수량 {})",
                 entry_name, FormatDollar(abs_funding_amount, true),
                 funding_time, FormatPercentage(funding_rate * 100, false),
                 funding_price, left_position_size),
          __FILE__, __LINE__);
    } else {
      // 펀딩비가 사용 가능 자금보다 적을 경우 지갑 자금에서 지불
      if (const auto available_balance = engine_->GetAvailableBalance();
          IsLessOrEqual(abs_funding_amount, available_balance)) {
        engine_->DecreaseWalletBalance(abs_funding_amount);

        const auto paid_funding_amount =
            filled_entry->GetPaidFundingAmount() - abs_funding_amount;

        // 진입 주문에 펀딩비 정산
        filled_entry->AddPaidFundingCount().SetPaidFundingAmount(
            paid_funding_amount);

        // 해당 진입 주문을 목표로 하는 청산 대기 주문에 펀딩비 정산
        for (const auto& pending_exit : pending_exits_[symbol_idx]) {
          if (pending_exit->GetEntryName() == entry_name) {
            pending_exit->AddPaidFundingCount().SetPaidFundingAmount(
                paid_funding_amount);
          }
        }

        LogFormattedInfo(
            INFO_L,
            format("[{}] 펀딩비 [{}] 지불 (펀딩 시간 {} | 펀딩 비율 "
                   "{} | 펀딩 가격 {} | 포지션 수량 {})",
                   entry_name, FormatDollar(abs_funding_amount, true),
                   funding_time, FormatPercentage(funding_rate * 100, false),
                   funding_price, left_position_size),
            __FILE__, __LINE__);
      } else {
        // 펀딩비가 사용 가능 자금보다 많을 경우 우선 사용 가능 자금에서 최대한
        // 지불 후 주문 마진에서 감소

        // 1. 우선 사용 가능 자금에서 최대한 감소
        if (IsGreater(available_balance, 0.0)) {
          engine_->DecreaseWalletBalance(available_balance);

          LogFormattedInfo(
              WARNING_L,
              format("[{}] 일부 펀딩비 [{}] 지불 (펀딩 시간 {} | 펀딩 비율 "
                     "{} | 펀딩 가격 {} | 포지션 수량 {} | 전체 펀딩비 {})",
                     entry_name, FormatDollar(available_balance, true),
                     funding_time, FormatPercentage(funding_rate * 100, false),
                     funding_price, left_position_size,
                     FormatDollar(abs_funding_amount, true)),
              __FILE__, __LINE__);

          engine_->LogBalance();
        }

        // 2. 남은 펀딩비는 진입 주문의 잔여 마진에서 감소
        const auto margin_deduction =
            abs_funding_amount - available_balance;  // 마진에서 충당할 금액

        // 2.1 남은 펀딩비가 잔여 마진보다 적을 경우, 잔여 마진에서 남은
        //     펀딩비를 전부 부과
        if (const auto left_margin = filled_entry->GetLeftMargin();
            IsLess(margin_deduction, left_margin)) {
          // 포지션의 잔여 마진과 강제 청산 가격 조정
          const auto adjusted_margin = left_margin - margin_deduction;
          const auto original_liquidation_price =
              filled_entry->GetLiquidationPrice();
          const auto adjusted_liquidation_price = CalculateLiquidationPrice(
              entry_direction, filled_entry->GetEntryFilledPrice(),
              left_position_size, adjusted_margin, symbol_idx);
          const auto paid_funding_amount =
              filled_entry->GetPaidFundingAmount() - abs_funding_amount;

          // 진입 주문의 잔여 마진, 강제 청산 가격, 펀딩비 지불 재설정
          filled_entry->SetLeftMargin(adjusted_margin)
              .SetLiquidationPrice(adjusted_liquidation_price)
              .AddPaidFundingCount()
              .SetPaidFundingAmount(paid_funding_amount);

          // 해당 진입 주문을 목표로 하는 청산 대기 주문의 잔여 마진,
          // 강제 청산 가격, 펀딩비 지불 재설정
          for (const auto& pending_exit : pending_exits_[symbol_idx]) {
            if (pending_exit->GetEntryName() == entry_name) {
              pending_exit->SetLeftMargin(adjusted_margin)
                  .SetLiquidationPrice(adjusted_liquidation_price)
                  .AddPaidFundingCount()
                  .SetPaidFundingAmount(paid_funding_amount);
            }
          }

          // Used Margin은 포지션의 Left Margin보다 무조건 같거나 많으므로,
          // 음수는 절대 되지 않으므로 단순 감소시키면 됨
          // ※ 사용한 마진으로 잡힌 지갑 자금을 감소시키고 펀딩비를 지불하는
          //    것이기 때문에 사용한 마진과 지갑 자금을 둘 다 감소시켜야 함
          engine_->DecreaseUsedMargin(margin_deduction);
          engine_->DecreaseWalletBalance(margin_deduction);

          LogFormattedInfo(
              WARNING_L,
              format("[{}] 남은 펀딩비 [{}]를 마진 [{}]에서 지불 "
                     "(펀딩 시간 {} | 펀딩 비율 {} | 펀딩 가격 {} | "
                     "포지션 수량 {} | 전체 펀딩비 {})",
                     entry_name, FormatDollar(margin_deduction, true),
                     FormatDollar(left_margin, true), funding_time,
                     FormatPercentage(funding_rate * 100, false), funding_price,
                     left_position_size,
                     FormatDollar(abs_funding_amount, true)),
              __FILE__, __LINE__);

          LogFormattedInfo(
              WARNING_L,
              format(
                  "[{}] 펀딩비를 지불할 사용 가능 자금 부족으로 인해 잔여 마진 "
                  "[{}] → [{}], 강제 청산 가격 [{}] → [{}] 조정",
                  entry_name, FormatDollar(left_margin, true),
                  FormatDollar(adjusted_margin, true),
                  original_liquidation_price, adjusted_liquidation_price),
              __FILE__, __LINE__);
        } else {
          // 2.2 남은 펀딩비가 잔여 마진보다 많거나 같을 경우, 잔여 마진까지만
          //     펀딩비가 부과되고 남은 펀딩비는 보험 기금에서 충당되며 포지션은
          //     강제 청산 됨

          // 진입 주문의 잔여 마진은 0
          // FundingAmount에서 잔여 마진은 항상 양수이고 지불하는 것이니 빼기
          filled_entry->SetLeftMargin(0)
              .SetLiquidationPrice(CalculateLiquidationPrice(
                  entry_direction, filled_entry->GetEntryFilledPrice(),
                  left_position_size, 0, symbol_idx))
              .AddPaidFundingCount()
              .SetPaidFundingAmount(filled_entry->GetPaidFundingAmount() -
                                    left_margin);

          // 어차피 강제 청산 되므로 해당 진입을 목표로 하는 청산 주문은 수정할
          // 필요가 없음 (자동으로 취소됨)

          engine_->DecreaseUsedMargin(left_margin);
          engine_->DecreaseWalletBalance(left_margin);

          LogFormattedInfo(
              WARNING_L,
              format("[{}] 남은 펀딩비 [{}]가 잔여 마진 [{}]"
                     "보다 많거나 같으므로, 잔여 마진 전체를 펀딩비로 지불하고 "
                     "강제 청산됩니다. (부족분은 보험 기금에서 충당됩니다.)",
                     entry_name, FormatDollar(margin_deduction, true),
                     FormatDollar(left_margin, true)),
              __FILE__, __LINE__);

          LogFormattedInfo(
              WARNING_L,
              format("[{}] 남은 펀딩비 일부를 마진 [{}]로 전체 지불 "
                     "(펀딩 시간 {} | 펀딩 비율 {} | 펀딩 가격 {} | "
                     "포지션 수량 {} | 전체 펀딩비 {})",
                     entry_name, FormatDollar(left_margin, true), funding_time,
                     FormatPercentage(funding_rate * 100, false), funding_price,
                     left_position_size,
                     FormatDollar(abs_funding_amount, true)),
              __FILE__, __LINE__);

          engine_->LogBalance();

          // 현재 Open 가격에서 강제 청산
          // (펀딩비는 항상 OHLC 시작 전에 정산되므로 시가에서 강제 청산이 타당)
          // 현재 바의 Open Time이 현재 진행 시간의 Open Time과 같지 않으면
          // 펀딩비는 부과되지 않으므로 이 함수 실행 시점에서는 단순히 Open
          // 가격만 가져와서 사용하면 됨
          FillLiquidation(filled_entry, "강제 청산 (펀딩비)", symbol_idx,
                          (bar_->GetCurrentBarType() == TRADING
                               ? bar_->GetBarData(TRADING)
                               : bar_->GetBarData(MAGNIFIER))
                              ->GetBar(symbol_idx, bar_->GetCurrentBarIndex())
                              .open);

          // 강제 청산 시 LogBalance가 실행되므로 따로 호출할 필요가 없으므로
          // 바로 다음 펀딩비 정산으로 넘어감
          continue;
        }
      }
    }

    // 펀딩비 정산 후 현재 자금 로그
    engine_->LogBalance();
  }
}

void OrderHandler::FillLiquidation(const shared_ptr<Order>& filled_entry,
                                   const string& exit_name,
                                   const int symbol_idx,
                                   const double fill_price) {
  // 현재 바 시간 로딩
  const auto current_open_time = engine_->GetCurrentOpenTime();

  // 강제 청산 주문 생성
  const auto liquidation_exit = make_shared<Order>(*filled_entry);
  const auto exit_size = liquidation_exit->GetEntryFilledSize() -
                         liquidation_exit->GetExitFilledSize();
  liquidation_exit->SetExitName(exit_name)
      .SetExitOrderType(MARKET)
      .SetExitDirection(liquidation_exit->GetEntryDirection() == LONG ? SHORT
                                                                      : LONG)
      .SetExitOrderTime(current_open_time)
      .SetExitOrderPrice(fill_price)
      .SetExitOrderSize(exit_size)
      .SetExitFilledTime(current_open_time)
      .SetExitFilledSize(exit_size);

  // 해당 진입 주문을 진입 체결 주문에서 삭제
  // 강제 청산 주문 생성 전 삭제 시, 진입 주문 객체가 삭제되어 주문 생성 오류가
  // 발생할 수 있으므로 강제 청산 주문 생성 후 삭제
  erase(filled_entries_[symbol_idx], filled_entry);

  // 슬리피지가 포함된 체결가 계산
  const double slippage_filled_price = CalculateSlippagePrice(
      MARKET, liquidation_exit->GetExitDirection(), fill_price, symbol_idx);

  // 체결가 및 청산/강제 청산 수수료 설정
  liquidation_exit->SetExitFilledPrice(slippage_filled_price)
      .SetExitFee(CalculateTradingFee(MARKET, slippage_filled_price, exit_size))
      .SetLiquidationFee(slippage_filled_price * exit_size *
                         symbol_info_[symbol_idx].GetLiquidationFeeRate());

  // 강제 청산된 진입 이름을 목표로 하는 청산 대기 주문 취소
  // 여러 주문이 취소될 수도 있으므로 역순으로 순회
  const auto& target_name = liquidation_exit->GetEntryName();
  auto& pending_exits = pending_exits_[symbol_idx];

  for (int order_idx = static_cast<int>(pending_exits.size()) - 1;
       order_idx >= 0; order_idx--) {
    if (const auto& pending_exit = pending_exits[order_idx];
        target_name == pending_exit->GetEntryName()) {
      // 청산 대기 주문은 예약 증거금이 필요하지 않기 때문에 삭제만 함
      pending_exits.erase(pending_exits.begin() + order_idx);

      LogFormattedInfo(
          INFO_L,
          format("{} [{}] 주문 취소 (원본 진입 강제 청산)",
                 Order::OrderTypeToString(pending_exit->GetExitOrderType()),
                 pending_exit->GetExitName()),
          __FILE__, __LINE__);
    }
  }

  // 자금, 통계 업데이트
  ExecuteExit(liquidation_exit, symbol_idx);
}

bool OrderHandler::FillMarketEntry(const shared_ptr<Order>& market_entry,
                                   const int symbol_idx,
                                   const PriceType price_type) {
  // 필요한 정보 로딩
  const auto entry_direction = market_entry->GetEntryDirection();
  const auto entry_filled_price = market_entry->GetEntryFilledPrice();
  const auto entry_filled_size = market_entry->GetEntryFilledSize();
  const auto& order_type_str =
      Order::OrderTypeToString(market_entry->GetEntryOrderType());
  const string& entry_name = market_entry->GetEntryName();
  const auto entry_fee = market_entry->GetEntryFee();
  warn_msg = format("{} [{}] 체결 실패", order_type_str, entry_name);

  // 해당 주문과 반대 방향의 체결 주문이 있으면 모두 청산
  ExitOppositeFilledEntries(entry_direction, market_entry->GetEntryOrderPrice(),
                            symbol_idx);

  // 레버리지 설정
  // 사용자가 AdjustLeverage를 호출하지 못 하게 한 이유는,
  // 반대 방향 주문을 확실하게 청산하고 목표하는 레버리지로 확실하게 진입할 수
  // 있게 하기 위함과, 목표 레버리지를 진입 시점에 직접 변경하는 것보다 엔진
  // 내부 처리가 편리하기 때문
  RET_FALSE_IF_INVALID(AdjustLeverage(market_entry->GetLeverage(), symbol_idx))

  // 시장가 진입 마진을 계산 후 설정
  const double entry_margin = CalculateMargin(
      entry_filled_price, entry_filled_size, price_type, symbol_idx);

  market_entry->SetEntryMargin(entry_margin).SetLeftMargin(entry_margin);

  // 강제 청산 가격 계산
  market_entry->SetLiquidationPrice(
      CalculateLiquidationPrice(entry_direction, entry_filled_price,
                                entry_filled_size, entry_margin, symbol_idx));

  // 진입 체결 가능 여부 체크 (사용 가능 자금 >= 시장가 진입 마진 + 진입 수수료)
  RET_FALSE_IF_INVALID(HasEnoughBalance(
      engine_->GetAvailableBalance(), entry_margin + entry_fee, "사용 가능",
      format("{} 진입 마진 및 진입 수수료", order_type_str)))

  // 지갑 자금에서 진입 수수료 차감
  engine_->DecreaseWalletBalance(entry_fee);

  // 수수료 차감 로그
  LogFormattedInfo(INFO_L,
                   format("{} [{}] 진입 수수료 [{}] 차감", order_type_str,
                          entry_name, FormatDollar(entry_fee, true)),
                   __FILE__, __LINE__);
  engine_->LogBalance();

  // 사용한 마진에 시장가 진입 마진 증가
  engine_->IncreaseUsedMargin(entry_margin);

  // 전역 항목들 업데이트
  UpdateLastEntryBarIndex(symbol_idx);
  last_entry_prices_[symbol_idx] = entry_filled_price;
  just_entered_ = true;

  // 시장가 진입
  filled_entries_[symbol_idx].push_back(market_entry);

  LogFormattedInfo(INFO_L,
                   format("{} [{}] 체결 (체결가 {} | 체결량 {} | 진입 마진 {})",
                          order_type_str, entry_name, entry_filled_price,
                          entry_filled_size, FormatDollar(entry_margin, true)),
                   __FILE__, __LINE__);
  engine_->LogBalance();

  return true;
}

void OrderHandler::ExitOppositeFilledEntries(
    const Direction target_entry_direction, const double entry_order_price,
    const int symbol_idx) {
  is_reverse_exit_ = true;
  reverse_exit_price_ = entry_order_price;

  // 여러 주문이 청산될 수도 있으므로 역순으로 순회
  const auto& filled_entries = filled_entries_[symbol_idx];
  for (int order_idx = static_cast<int>(filled_entries.size()) - 1;
       order_idx >= 0; order_idx--) {
    const auto& filled_entry = filled_entries[order_idx];

    if (const auto filled_entry_direction = filled_entry->GetEntryDirection();
        target_entry_direction != filled_entry_direction) {
      if (!MarketExit(filled_entry_direction == LONG ? "매수 청산 (리버스)"
                                                     : "매도 청산 (리버스)",
                      filled_entry->GetEntryName(),
                      filled_entry->GetEntryFilledSize() -
                          filled_entry->GetExitFilledSize())) {
        Logger::LogAndThrowError("엔진 오류: 리버스 청산 실패", __FILE__,
                                 __LINE__);
      }
    }
  }

  is_reverse_exit_ = false;
  reverse_exit_price_ = NAN;
}

void OrderHandler::ExecuteExit(const shared_ptr<Order>& exit_order,
                               const int symbol_idx) {
  // 주문 정보 로딩
  const auto entry_direction = exit_order->GetEntryDirection();
  const auto& entry_name = exit_order->GetEntryName();
  const auto entry_filled_price = exit_order->GetEntryFilledPrice();
  const auto entry_filled_size = exit_order->GetEntryFilledSize();
  const auto exit_filled_price = exit_order->GetExitFilledPrice();
  const auto exit_filled_size = exit_order->GetExitFilledSize();
  const auto left_margin = exit_order->GetLeftMargin();
  const auto liquidation_price = exit_order->GetLiquidationPrice();
  const auto& order_type_str =
      Order::OrderTypeToString(exit_order->GetExitOrderType());
  const auto& exit_name = exit_order->GetExitName();

  // 청산 횟수 추가
  exit_order->AddExitCount();

  // 지갑 자금에서 청산 수수료 차감
  const auto exit_fee = exit_order->GetExitFee();
  engine_->DecreaseWalletBalance(exit_fee);

  // 수수료 차감 로그
  LogFormattedInfo(INFO_L,
                   format("{} [{}] 청산 수수료 [{}] 차감", order_type_str,
                          exit_name, FormatDollar(exit_fee, true)),
                   __FILE__, __LINE__);
  engine_->LogBalance();

  // 원본 진입 찾기 (존재하면 분할 청산, 미존재하면 전량 청산)
  if (const auto& result = FindEntryOrder(entry_name, symbol_idx)) {
    const auto& entry_order = result->first;

    // 이번 청산 실행 전 보유 포지션 수량 대비 청산 수량 비율 계산
    // ExecuteExit 함수 호출 전, 진입 주문의 총 청산량에 이번 청산의 청산 수량이
    // 포함되도록 계산을 하기 때문에 때문에 조정이 필요함
    const auto total_exit_filled_size = entry_order->GetExitFilledSize();
    const auto exit_size_ratio =
        exit_filled_size /
        (entry_filled_size - total_exit_filled_size + exit_filled_size);

    // 분할 청산이라면 이번 청산 수량 비율만큼 사용한 마진 감소
    const auto exit_margin = left_margin * exit_size_ratio;

    // 사용한 마진 감소
    engine_->DecreaseUsedMargin(exit_margin);

    // 부분 청산 시 포지션 수량 비율에 따른 펀딩비 조정
    const auto received_funding_amount =
        entry_order->GetReceivedFundingAmount();
    const auto exit_received_funding_amount =
        received_funding_amount * exit_size_ratio;
    exit_order->SetReceivedFundingAmount(exit_received_funding_amount);

    const auto paid_funding_amount = entry_order->GetPaidFundingAmount();
    const auto exit_paid_funding_amount = paid_funding_amount * exit_size_ratio;
    exit_order->SetPaidFundingAmount(exit_paid_funding_amount);

    // 진입 마진이 감소했으므로 원본 진입과 원본 진입을 목표로 하는
    // 청산 대기 주문들의 잔여 마진과 강제 청산 가격 수정
    //
    // 진입 주문으로 청산 체결량을 로딩한 이유는, 원본 진입의 청산 체결량이
    // 누적 청산 체결량이기 때문에 정확한 진입 잔량을 구할 수 있기 때문
    const auto adjusted_margin = left_margin - exit_margin;
    const auto adjusted_liquidation_price =
        CalculateLiquidationPrice(entry_direction, entry_filled_price,
                                  entry_filled_size - total_exit_filled_size,
                                  adjusted_margin, symbol_idx);
    const auto adjusted_received_funding_amount =
        received_funding_amount - exit_received_funding_amount;
    const auto adjusted_paid_funding_amount =
        paid_funding_amount - exit_paid_funding_amount;

    entry_order->SetLeftMargin(adjusted_margin)
        .SetLiquidationPrice(adjusted_liquidation_price)
        .SetReceivedFundingAmount(adjusted_received_funding_amount)
        .SetPaidFundingAmount(adjusted_paid_funding_amount)
        .AddExitCount();

    for (const auto& pending_exit : pending_exits_[symbol_idx]) {
      if (pending_exit->GetEntryName() == entry_name) {
        pending_exit->SetLeftMargin(adjusted_margin)
            .SetLiquidationPrice(adjusted_liquidation_price)
            .SetReceivedFundingAmount(adjusted_received_funding_amount)
            .SetPaidFundingAmount(adjusted_paid_funding_amount)
            .AddExitCount();
      }
    }

    LogFormattedInfo(
        INFO_L,
        format(
            "[{}] 부분 청산으로 인해 잔여 마진 [{}] → [{}], 강제 청산 가격 "
            "[{}] → [{}], 펀딩비 수령 [{}] → [{}], 펀딩비 지불 [{}] -> [{}] "
            "조정 (진입 수량 [{}] | 부분 청산 수량 [{}] | 전체 청산 수량 [{}])",
            entry_name, FormatDollar(left_margin, true),
            FormatDollar(adjusted_margin, true), liquidation_price,
            adjusted_liquidation_price,
            FormatDollar(received_funding_amount, true),
            FormatDollar(adjusted_received_funding_amount, true),
            FormatDollar(paid_funding_amount, true),
            FormatDollar(adjusted_paid_funding_amount, true), entry_filled_size,
            exit_filled_size, total_exit_filled_size),
        __FILE__, __LINE__);

    // 명목 가치가 감소했는데 레버리지 구간에 의한 현재 레버리지 조정을 하지
    // 않는 이유는, 명목 가치가 감소할수록 레버리지 구간에서 최대 레버리지가
    // 증가하기 때문
  } else {
    // 전량 청산이라면 이 함수 실행 전 진입 주문에서 이 주문의 원본 주문이
    // 삭제됨 즉, 진입 주문을 찾지 못하면 전량 청산된 것
    //
    // 전량 청산이라면 잔여 마진만큼 사용한 마진 감소
    engine_->DecreaseUsedMargin(left_margin);
  }

  // 실현 손익 계산
  const double calculated_pnl = CalculatePnl(
      entry_direction, exit_filled_price, entry_filled_price, exit_filled_size);
  double realized_pnl = calculated_pnl;
  const double abs_realized_pnl = fabs(calculated_pnl);
  bool is_bankruptcy_position = false;

  // 지갑 자금에 실현 손익 계산
  if (IsGreaterOrEqual(calculated_pnl, 0.0)) {
    engine_->IncreaseWalletBalance(calculated_pnl);
  } else {
    // 실현 손실이 청산 시 잔여 마진과 같거나 크다면 파산 포지션으로,
    // 추가 손실은 보험 기금으로 충당됨.
    // 즉, 최대 손실은 잔여 마진
    if (IsGreaterOrEqual(abs_realized_pnl, left_margin)) {
      realized_pnl = -left_margin;
      is_bankruptcy_position = true;
    } else {
      realized_pnl = -abs_realized_pnl;
    }

    engine_->DecreaseWalletBalance(fabs(realized_pnl));
  }

  LogFormattedInfo(
      IsEqual(exit_order->GetLiquidationFee(), 0.0) ? INFO_L : ERROR_L,
      format("{} [{}] 체결 ({} [{}] | 체결가 {} | 체결량 {} | 마진 {} | "
             "손익 {} | 실손익 {})",
             order_type_str, exit_name,
             Order::OrderTypeToString(exit_order->GetEntryOrderType()),
             entry_name, exit_filled_price,
             RoundToStep(exit_order->GetExitFilledSize(),
                         symbol_info_[symbol_idx].GetQtyStep()),
             FormatDollar(left_margin, true),
             FormatDollar(calculated_pnl, true),
             FormatDollar(realized_pnl, true)),
      __FILE__, __LINE__);
  engine_->LogBalance();

  // 강제 청산이고 파산 포지션이 아니라면 강제 청산 수수료 부과.
  // 수수료는 청산 시 잔여 마진에서 실현 손실을 뺀 잔여 마진까지만 부과
  if (const auto liquidation_fee = exit_order->GetLiquidationFee();
      !IsEqual(liquidation_fee, 0.0)) {
    // 파산 포지션이거나, 잔여 마진이 0인 경우에는 강제 청산 수수료는
    // 보험 기금에서 충당
    // -> PnL이 양수여도 펀딩비 부족으로 강제 청산 당할 수 있음
    if (!is_bankruptcy_position && !IsEqual(left_margin, 0.0)) {
      double left_margin_after_exit;
      double real_liquidation_fee;

      if (left_margin_after_exit = left_margin - abs_realized_pnl;
          IsGreaterOrEqual(liquidation_fee, left_margin_after_exit)) {
        // 계산된 강제 청산 수수료가 PnL 정산 후 잔여 마진보다 많다면 잔여
        // 마진까지만 부과
        real_liquidation_fee = left_margin_after_exit;
      } else {
        real_liquidation_fee = liquidation_fee;
      }

      exit_order->SetLiquidationFee(real_liquidation_fee);
      engine_->DecreaseWalletBalance(real_liquidation_fee);

      LogFormattedInfo(
          ERROR_L,
          format(
              "[{}] 강제 청산 수수료 [{}] 차감 (계산된 강제 청산 수수료 {} | "
              "청산 후 잔여 마진 {})",
              entry_name, FormatDollar(real_liquidation_fee, true),
              FormatDollar(liquidation_fee, true),
              FormatDollar(left_margin_after_exit, true)),
          __FILE__, __LINE__);
      engine_->LogBalance();
    } else {
      exit_order->SetLiquidationFee(0);
    }
  }

  // 전역 항목들 업데이트
  engine_->UpdateStatistics();
  UpdateLastExitBarIndex(symbol_idx);
  last_exit_prices_[symbol_idx] = exit_filled_price;
  just_exited_ = true;

  // 분석기에 청산된 거래 추가
  AddTrade(exit_order, realized_pnl, symbol_idx);
}

optional<FillInfo> OrderHandler::CheckPendingLimitEntry(
    const shared_ptr<Order>& limit_entry, const double price,
    const PriceType price_type) {
  // 지정가 조건 만족 시 체결 가능
  if (const auto order_price = limit_entry->GetEntryOrderPrice();
      IsLimitPriceSatisfied(limit_entry->GetEntryDirection(), price,
                            order_price)) {
    // 시가에서 진입 조건 달성 시 체결 가격은 시가가 됨 (갭 고려)
    return FillInfo{limit_entry, ENTRY,
                    price_type == OPEN ? price : order_price};
  }

  return nullopt;
}

optional<FillInfo> OrderHandler::CheckPendingMitEntry(
    const shared_ptr<Order>& mit_entry, const double price,
    const PriceType price_type) {
  // 터치 시 시장가 체결 가능
  if (const auto touch_price = mit_entry->GetEntryTouchPrice();
      IsPriceTouched(mit_entry->GetEntryTouchDirection(), price, touch_price)) {
    // 시가에서 진입 조건 달성 시 체결 가격은 시가가 됨 (갭 고려)
    return FillInfo{mit_entry, ENTRY, price_type == OPEN ? price : touch_price};
  }

  return nullopt;
}

optional<FillInfo> OrderHandler::CheckPendingLitEntry(
    const shared_ptr<Order>& lit_entry, int& order_idx, const int symbol_idx,
    const double price, const PriceType price_type) {
  // Order Time이 설정되지 않았으면 지정가 미주문 -> 터치 확인
  if (lit_entry->GetEntryOrderTime() == -1) {
    if (IsPriceTouched(lit_entry->GetEntryTouchDirection(), price,
                       lit_entry->GetEntryTouchPrice())) {
      // 터치 시 지정가 주문 접수
      if (!OrderPendingLitEntry(lit_entry, order_idx, symbol_idx, price_type)) {
        // 주문 실패 시 함수 내에서 해당 주문 삭제 후, 다음 주문 확인으로 넘어감
        return nullopt;
      }
    }
  }

  // Order Time이 설정됐으면 지정가 주문 완료 -> 체결 확인
  // 터치 후 바로 체결 조건이 만족될 수 있으므로 Order Time을 다시 불러와서 확인
  if (lit_entry->GetEntryOrderTime() != -1) {
    if (const auto order_price = lit_entry->GetEntryOrderPrice();
        IsLimitPriceSatisfied(lit_entry->GetEntryDirection(), price,
                              order_price)) {
      // 시가에서 진입 조건 달성 시 체결 가격은 시가가 됨 (갭 고려)
      return FillInfo{lit_entry, ENTRY,
                      price_type == OPEN ? price : order_price};
    }
  }

  return nullopt;
}

optional<FillInfo> OrderHandler::CheckPendingTrailingEntry(
    const shared_ptr<Order>& trailing_entry, const double price,
    const PriceType price_type) {
  // Extreme Price가 지정되지 않았으면 추적 미시작 -> 터치 확인
  if (isnan(trailing_entry->GetEntryExtremePrice())) {
    if (IsPriceTouched(trailing_entry->GetEntryTouchDirection(), price,
                       trailing_entry->GetEntryTouchPrice())) {
      trailing_entry->SetEntryExtremePrice(price);
    } else {
      // 추적 미시작인데 터치도 안 됐으면 체결이 안 되므로 얼리 리턴
      return nullopt;
    }
  }

  // Extreme Price가 지정되었으면 추적 시작
  // -> 고저가 업데이트 및 체결 터치 확인
  if (double extreme_price = trailing_entry->GetEntryExtremePrice();
      !isnan(extreme_price)) {
    // 주문 정보 로딩
    const auto entry_direction = trailing_entry->GetEntryDirection();
    const auto trail_point = trailing_entry->GetEntryTrailPoint();

    double trail_price = NAN;
    bool should_entry = false;

    if (entry_direction == LONG) {
      // 진입 방향이 매수인 경우, 최저가를 추적
      if (IsLess(price, extreme_price)) {
        trailing_entry->SetEntryExtremePrice(price);
        extreme_price = price;
      }

      // 진입 방향이 매수인 경우, 최저가로부터 Trail Point 증가 시 체결 가능
      if (trail_price = extreme_price + trail_point;
          IsGreaterOrEqual(price, trail_price)) {
        should_entry = true;
      }
    } else if (entry_direction == SHORT) {
      // 진입 방향이 매도인 경우, 최고가를 추적
      if (IsGreater(price, extreme_price)) {
        trailing_entry->SetEntryExtremePrice(price);
        extreme_price = price;
      }

      // 진입 방향이 매도인 경우, 최고가로부터 Trail Point 감소 시 체결 가능
      if (trail_price = extreme_price - trail_point;
          IsLessOrEqual(price, trail_price)) {
        should_entry = true;
      }
    }

    if (should_entry) {
      // 시가에서 진입 조건 달성 시 체결 가격은 시가가 됨 (갭 고려)
      return FillInfo{trailing_entry, ENTRY,
                      price_type == OPEN ? price : trail_price};
    }
  }

  return nullopt;
}

void OrderHandler::FillPendingMarketEntry(const shared_ptr<Order>& market_entry,
                                          const int symbol_idx,
                                          const double fill_price,
                                          const PriceType price_type) {
  const auto& entry_name = market_entry->GetEntryName();
  warn_msg = format("{} [{}] 체결 실패",
                    Order::OrderTypeToString(market_entry->GetEntryOrderType()),
                    entry_name);

  // 진입 대기 주문에서 삭제
  erase(pending_entries_[symbol_idx], market_entry);

  // 해당 진입 이름으로 체결된 주문이 없는지 확인
  RET_IF_INVALID(IsValidEntryName(entry_name, symbol_idx))

  // 현재 바 시간 로딩
  const auto current_open_time = engine_->GetCurrentOpenTime();

  // 주문 정보 로딩
  const auto entry_filled_size = market_entry->GetEntryOrderSize();
  const auto entry_direction = market_entry->GetEntryDirection();

  // 주문 업데이트
  market_entry->SetEntryOrderTime(current_open_time)
      .SetEntryOrderPrice(fill_price)
      .SetEntryFilledTime(current_open_time)
      .SetEntryFilledSize(entry_filled_size);

  // 슬리피지가 포함된 체결가
  const double slippage_filled_price =
      CalculateSlippagePrice(MARKET, entry_direction, fill_price, symbol_idx);
  market_entry->SetEntryFilledPrice(slippage_filled_price);

  // 수수료
  market_entry->SetEntryFee(
      CalculateTradingFee(MARKET, slippage_filled_price, entry_filled_size));

  // 자금 관련 처리 후 체결 주문에 추가
  FillMarketEntry(market_entry, symbol_idx, price_type);
}

void OrderHandler::FillPendingLimitEntry(const shared_ptr<Order>& limit_entry,
                                         const int symbol_idx,
                                         const double fill_price,
                                         const PriceType price_type) {
  const auto& entry_name = limit_entry->GetEntryName();
  const auto order_type_str =
      Order::OrderTypeToString(limit_entry->GetEntryOrderType());
  warn_msg = format("{} [{}] 체결 실패", order_type_str, entry_name);

  // 진입 대기 주문에서 삭제
  erase(pending_entries_[symbol_idx], limit_entry);

  // 해당 진입 이름으로 체결된 주문이 없는지 확인
  if (const auto& warn = IsValidEntryName(entry_name, symbol_idx)) {
    // 중복된 진입 이름이 존재하면 체결 실패
    // 사용한 마진(예약 증거금) 감소
    LogFormattedInfo(WARNING_L, *warn, __FILE__, __LINE__);
    engine_->DecreaseUsedMargin(limit_entry->GetEntryMargin());

    WARN_AND_RET()
  }

  // 주문 정보 로딩
  const auto entry_filled_size = limit_entry->GetEntryOrderSize();
  const auto entry_direction = limit_entry->GetEntryDirection();

  // 주문 업데이트
  limit_entry->SetEntryFilledTime(engine_->GetCurrentOpenTime())
      .SetEntryFilledSize(entry_filled_size);

  // 슬리피지가 포함된 체결가
  const double slippage_filled_price =
      CalculateSlippagePrice(LIMIT, entry_direction, fill_price, symbol_idx);
  limit_entry->SetEntryFilledPrice(slippage_filled_price);

  // 수수료
  const double entry_fee =
      CalculateTradingFee(LIMIT, slippage_filled_price, entry_filled_size);
  limit_entry->SetEntryFee(entry_fee);

  // ===========================================================================
  // 해당 주문과 반대 방향의 체결 주문이 있으면 모두 청산
  ExitOppositeFilledEntries(entry_direction, limit_entry->GetEntryOrderPrice(),
                            symbol_idx);

  // 레버리지 설정
  // 사용자가 AdjustLeverage를 호출하지 못 하게 한 이유는,
  // 반대 방향 주문을 확실하게 청산하고 목표하는 레버리지로 확실하게 진입할 수
  // 있게 하기 위함과, 목표 레버리지를 진입 시점에 직접 변경하는 것보다 엔진
  // 내부 처리가 편리하기 때문
  if (const auto& warn =
          AdjustLeverage(limit_entry->GetLeverage(), symbol_idx)) {
    // 레버리지 변경이 실패하면 체결 실패
    // 사용한 마진(예약 증거금) 감소
    LogFormattedInfo(WARNING_L, *warn, __FILE__, __LINE__);
    engine_->DecreaseUsedMargin(limit_entry->GetEntryMargin());

    WARN_AND_RET()
  }

  // 현재 미실현 손실을 반영한 지정가 진입 마진 재계산
  const auto entry_margin = CalculateMargin(
      slippage_filled_price, entry_filled_size, price_type, symbol_idx);

  // 지정가 예약 마진을 감소 후 재설정
  engine_->DecreaseUsedMargin(limit_entry->GetEntryMargin());
  limit_entry->SetEntryMargin(entry_margin).SetLeftMargin(entry_margin);

  // 강제 청산 가격 계산
  limit_entry->SetLiquidationPrice(
      CalculateLiquidationPrice(entry_direction, slippage_filled_price,
                                entry_filled_size, entry_margin, symbol_idx));

  // 진입 가능 여부 체크 (사용 가능 자금 >= 지정가 진입 마진 + 진입 수수료)
  RET_IF_INVALID(HasEnoughBalance(
      engine_->GetAvailableBalance(), entry_margin + entry_fee, "사용 가능",
      format("{} 진입 마진 및 수수료", order_type_str)))

  // 지갑 자금에서 진입 수수료 차감
  engine_->DecreaseWalletBalance(entry_fee);

  // 수수료 차감 로그
  LogFormattedInfo(INFO_L,
                   format("{} [{}] 진입 수수료 [{}] 차감", order_type_str,
                          entry_name, FormatDollar(entry_fee, true)),
                   __FILE__, __LINE__);
  engine_->LogBalance();

  // 사용한 마진에 지정가 진입 마진 증가
  engine_->IncreaseUsedMargin(entry_margin);

  // 전역 항목들 업데이트
  UpdateLastEntryBarIndex(symbol_idx);
  last_entry_prices_[symbol_idx] = slippage_filled_price;
  just_entered_ = true;

  // 지정가 진입
  filled_entries_[symbol_idx].push_back(limit_entry);

  LogFormattedInfo(INFO_L,
                   format("{} [{}] 체결 (체결가 {} | 체결량 {} | 진입 마진 {})",
                          order_type_str, entry_name, slippage_filled_price,
                          entry_filled_size, FormatDollar(entry_margin, true)),
                   __FILE__, __LINE__);
  engine_->LogBalance();
}

bool OrderHandler::OrderPendingLitEntry(const shared_ptr<Order>& lit_entry,
                                        int& order_idx, const int symbol_idx,
                                        const PriceType price_type) {
  auto& pending_entries = pending_entries_[symbol_idx];
  const auto order_price = lit_entry->GetEntryOrderPrice();
  const auto order_size = lit_entry->GetEntryOrderSize();

  // 주문 업데이트
  lit_entry->SetEntryOrderTime(engine_->GetCurrentOpenTime());

  // 예약 증거금 계산
  const double entry_margin =
      CalculateMargin(order_price, order_size, price_type, symbol_idx);
  lit_entry->SetEntryMargin(entry_margin).SetLeftMargin(entry_margin);

  // 주문 가능 여부 체크
  if (const auto& warn =
          HasEnoughBalance(engine_->GetAvailableBalance(), entry_margin,
                           "사용 가능", "LIT 주문 마진")) {
    LogFormattedInfo(WARNING_L, *warn, __FILE__, __LINE__);

    // 주문 실패 시 대기 주문에서 삭제
    pending_entries.erase(pending_entries.begin() + order_idx);

    LogFormattedInfo(WARNING_L,
                     format("LIT [{}] 주문 취소 (사용 가능 자금 부족)",
                            lit_entry->GetEntryName()),
                     __FILE__, __LINE__);

    // CheckPendingEntries에서 주문 순회 시 order_idx를 무조건 증가시키는데,
    // 이 주문이 삭제되면 주문을 하나 건너뛰게 되므로 1을 감소시켜 인덱스를 유지
    order_idx--;

    return false;
  }

  // 사용한 마진에 예약 증거금 증가
  engine_->IncreaseUsedMargin(entry_margin);

  LogFormattedInfo(
      INFO_L,
      format("터치로 인해 LIT [{}] 대기 주문 (주문가 {} | 주문량 {})",
             lit_entry->GetEntryName(), order_price, order_size),
      __FILE__, __LINE__);
  engine_->LogBalance();

  return true;
}

optional<FillInfo> OrderHandler::CheckPendingLimitExit(
    const shared_ptr<Order>& limit_exit, const double price,
    const PriceType price_type) {
  // 지정가 조건 만족 시 체결 가능
  if (const auto order_price = limit_exit->GetExitOrderPrice();
      IsLimitPriceSatisfied(limit_exit->GetExitDirection(), price,
                            order_price)) {
    // 시가에서 청산 조건 달성 시 체결 가격은 시가가 됨 (갭 고려)
    return FillInfo{limit_exit, EXIT, price_type == OPEN ? price : order_price};
  }

  return nullopt;
}

optional<FillInfo> OrderHandler::CheckPendingMitExit(
    const shared_ptr<Order>& mit_exit, const double price,
    const PriceType price_type) {
  // 터치 시 시장가 체결 가능
  if (const auto touch_price = mit_exit->GetExitTouchPrice();
      IsPriceTouched(mit_exit->GetExitTouchDirection(), price, touch_price)) {
    // 시가에서 청산 조건 달성 시 체결 가격은 시가가 됨 (갭 고려)
    return FillInfo{mit_exit, EXIT, price_type == OPEN ? price : touch_price};
  }

  return nullopt;
}

optional<FillInfo> OrderHandler::CheckPendingLitExit(
    const shared_ptr<Order>& lit_exit, const double price,
    const PriceType price_type) {
  // Order Time이 설정되지 않았으면 지정가 미주문 -> 터치 확인
  if (lit_exit->GetExitOrderTime() == -1) {
    if (IsPriceTouched(lit_exit->GetExitTouchDirection(), price,
                       lit_exit->GetExitTouchPrice())) {
      // 주문 업데이트:
      // 청산은 마진이 잡히지 않기 때문에 따로 주문이 필요없으므로 시간만 설정
      lit_exit->SetExitOrderTime(engine_->GetCurrentOpenTime());

      LogFormattedInfo(
          INFO_L,
          format("LIT [{}] 주문 (주문가 {} | 주문량 {})",
                 lit_exit->GetEntryName(), lit_exit->GetEntryOrderPrice(),
                 lit_exit->GetEntryOrderSize()),
          __FILE__, __LINE__);
    }
  }

  // Order Time이 설정됐으면 지정가 주문 완료 -> 체결 확인
  // 터치 후 바로 체결 조건이 만족될 수 있으므로 Order Time을 다시 불러와서 확인
  if (lit_exit->GetExitOrderTime() != -1) {
    if (const auto order_price = lit_exit->GetExitOrderPrice();
        IsLimitPriceSatisfied(lit_exit->GetExitDirection(), price,
                              order_price)) {
      // 시가에서 청산 조건 달성 시 체결 가격은 시가가 됨 (갭 고려)
      return FillInfo{lit_exit, EXIT, price_type == OPEN ? price : order_price};
    }
  }

  return nullopt;
}

optional<FillInfo> OrderHandler::CheckPendingTrailingExit(
    const shared_ptr<Order>& trailing_exit, const double price,
    const PriceType price_type) {
  // Extreme Price가 지정되지 않았으면 추적 미시작 -> 터치 확인
  if (isnan(trailing_exit->GetExitExtremePrice())) {
    if (IsPriceTouched(trailing_exit->GetExitTouchDirection(), price,
                       trailing_exit->GetExitTouchPrice())) {
      trailing_exit->SetExitExtremePrice(price);
    } else {
      // 추적 미시작인데 터치도 안 됐으면 체결이 안되므로 얼리 리턴
      return nullopt;
    }
  }

  // Extreme Price가 지정되었으면 추적 시작
  // -> 고저가 업데이트 및 체결 터치 확인
  if (double extreme_price = trailing_exit->GetExitExtremePrice();
      !isnan(extreme_price)) {
    // 주문 정보 로딩
    const auto exit_direction = trailing_exit->GetExitDirection();
    const auto trail_point = trailing_exit->GetExitTrailPoint();

    double trail_price = NAN;
    bool should_exit = false;

    if (exit_direction == LONG) {
      // 청산 방향이 매수인 경우, 최저가를 추적
      if (IsLess(price, extreme_price)) {
        trailing_exit->SetExitExtremePrice(price);
        extreme_price = price;
      }

      // 청산 방향이 매수인 경우, 최저가로부터 Trail Point 증가 시 청산
      if (trail_price = extreme_price + trail_point;
          IsGreaterOrEqual(price, trail_price)) {
        should_exit = true;
      }
    } else if (exit_direction == SHORT) {
      // 청산 방향이 매도인 경우, 최고가를 추적
      if (IsGreater(price, extreme_price)) {
        trailing_exit->SetExitExtremePrice(price);
        extreme_price = price;
      }

      // 청산 방향이 매도인 경우, 최고가로부터 Trail Point 감소 시 청산
      if (trail_price = extreme_price - trail_point;
          IsLessOrEqual(price, trail_price)) {
        should_exit = true;
      }
    }

    if (should_exit) {
      // 시가에서 청산 조건 달성 시 체결 가격은 시가가 됨 (갭 고려)
      return FillInfo{trailing_exit, EXIT,
                      price_type == OPEN ? price : trail_price};
    }
  }

  return nullopt;
}

void OrderHandler::FillPendingExitOrder(
    const shared_ptr<Order>& exit_order,
    const pair<shared_ptr<Order>, int>& target_entry_order,
    const int symbol_idx, const double fill_price) {
  // 현재 바 시간 로딩
  const auto current_open_time = engine_->GetCurrentOpenTime();

  // 청산 대기 주문에서 삭제
  auto& pending_exits = pending_exits_[symbol_idx];
  erase(pending_exits, exit_order);

  // 주문 정보 로딩
  const auto& target_name = exit_order->GetEntryName();
  const auto order_type = exit_order->GetExitOrderType();
  const auto exit_direction = exit_order->GetExitDirection();

  // 원본 진입 주문 로딩
  const shared_ptr<Order>& entry_order = target_entry_order.first;
  const int entry_order_idx = target_entry_order.second;

  // 총 청산 체결 수량이 진입 체결 수량보다 크지 않게 조정
  const double exit_filled_size =
      GetAdjustedExitSize(exit_order->GetExitOrderSize(), entry_order);

  // 주문 업데이트
  if (order_type == MIT || order_type == TRAILING) {
    exit_order->SetExitOrderTime(current_open_time)
        .SetExitOrderPrice(fill_price);
  }

  exit_order->SetExitFilledTime(current_open_time)
      .SetExitFilledSize(exit_filled_size);

  // 슬리피지가 포함된 체결가
  const double slippage_filled_price = CalculateSlippagePrice(
      order_type, exit_direction, fill_price, symbol_idx);
  exit_order->SetExitFilledPrice(slippage_filled_price);

  // 수수료
  exit_order->SetExitFee(
      CalculateTradingFee(order_type, slippage_filled_price, exit_filled_size));

  // 원본 진입 주문에 청산 체결 수량 추가
  const double total_exit_filled_size =
      entry_order->GetExitFilledSize() + exit_filled_size;
  entry_order->SetExitFilledSize(total_exit_filled_size);

  // 원본 진입 주문의 청산 체결 수량이 진입 체결 수량과 같으면
  // filled_entries에서 원본 진입 주문 삭제
  if (IsEqual(total_exit_filled_size, entry_order->GetEntryFilledSize())) {
    auto& filled_entries = filled_entries_[symbol_idx];
    filled_entries.erase(filled_entries.begin() + entry_order_idx);

    // 같은 진입 이름을 목표로 하는 청산 대기 주문 취소
    for (int order_idx = static_cast<int>(pending_exits.size()) - 1;
         order_idx >= 0; order_idx--) {
      if (const auto pending_exit = pending_exits[order_idx];
          target_name == pending_exit->GetEntryName()) {
        // 청산 대기 주문은 예약 증거금이 필요하지 않기 때문에 삭제만 함
        pending_exits.erase(pending_exits.begin() + order_idx);

        LogFormattedInfo(
            INFO_L,
            format("{} [{}] 주문 취소 (원본 진입 강제 청산)",
                   Order::OrderTypeToString(pending_exit->GetExitOrderType()),
                   pending_exit->GetExitName()),
            __FILE__, __LINE__);
      }
    }
  }

  // 자금, 통계 업데이트
  ExecuteExit(exit_order, symbol_idx);
}

optional<pair<shared_ptr<Order>, int>> OrderHandler::FindEntryOrder(
    const string& target_name, const int symbol_idx) const {
  const auto& filled_entries = filled_entries_[symbol_idx];

  // 현재 심볼의 체결된 진입들 순회
  for (int order_idx = 0; order_idx < filled_entries.size(); order_idx++) {
    // target_entry_name과 같은 이름의 진입이 있으면 반환
    if (const auto& filled_entry = filled_entries[order_idx];
        filled_entry->GetEntryName() == target_name) {
      return make_pair(filled_entry, order_idx);
    }
  }

  // 원본 진입 주문을 찾지 못하면 nullopt 반환
  return nullopt;
}

double OrderHandler::GetAdjustedExitSize(const double exit_size,
                                         const shared_ptr<Order>& entry_order) {
  const auto entry_filled_size = entry_order->GetEntryFilledSize();
  const auto exit_filled_size = entry_order->GetExitFilledSize();

  // 청산 수량 + 분할 청산한 수량이 진입 수량보다 많다면
  if (const auto total_exit_size = exit_size + exit_filled_size;
      IsGreater(total_exit_size, entry_filled_size)) {
    // 청산 가능한 최대값으로 조정하여 반환
    return entry_filled_size - exit_filled_size;
  }

  return exit_size;
}

void OrderHandler::AddTrade(const shared_ptr<Order>& exit_order,
                            const double realized_pnl,
                            const int symbol_idx) const {
  // 중복 사용 변수 로딩
  const auto qty_step = symbol_info_[symbol_idx].GetQtyStep();
  const int64_t entry_time = exit_order->GetEntryFilledTime();
  const int64_t exit_time = exit_order->GetExitFilledTime();
  const auto entry_filled_size = exit_order->GetEntryFilledSize();
  const auto exit_filled_size = exit_order->GetExitFilledSize();

  // 진입 수량 대비 청산 수량만큼 진입 수수료를 배분
  // 배분하는 이유는, 첫 분할 청산에서 모든 진입 수수료를 감당하면
  // 손익률이 왜곡되기 때문
  const auto entry_fee =
      exit_filled_size / entry_filled_size * exit_order->GetEntryFee();
  const auto exit_fee = exit_order->GetExitFee();
  const auto liquidation_fee = exit_order->GetLiquidationFee();
  const auto received_funding_amount = exit_order->GetReceivedFundingAmount();
  const auto received_funding_count = exit_order->GetReceivedFundingCount();
  const auto paid_funding_amount = exit_order->GetPaidFundingAmount();
  const auto paid_funding_count = exit_order->GetPaidFundingCount();
  const auto total_funding_amount =
      received_funding_amount + paid_funding_amount;
  const double realized_net_pnl =  // 펀딩비를 정산하고 진입, 청산, 강제 청산
                                   // 수수료를 제외한 순손익
      realized_pnl + total_funding_amount - entry_fee - exit_fee -
      liquidation_fee;

  const auto current_wallet_balance = engine_->GetWalletBalance();
  const auto initial_balance = config_->GetInitialBalance();
  const auto cum_pnl = current_wallet_balance - initial_balance;

  // 동시 보유 심볼 개수 카운트
  int symbol_count = 0;

  // 모든 심볼의 체결된 진입 순회
  for (const auto& filled_entry : filled_entries_) {
    // 해당 심볼 덱에 체결된 진입이 있다면 심볼 개수 추가
    if (!filled_entry.empty()) {
      symbol_count++;
    }
  }

  // 거래 내역에 거래 추가
  analyzer_->AddTrade(
      Trade()
          .SetSymbolName(bar_->GetBarData(TRADING)->GetSymbolName(symbol_idx))
          .SetEntryName(exit_order->GetEntryName())
          .SetExitName(exit_order->GetExitName())
          .SetEntryDirection(exit_order->GetEntryDirection() == LONG ? "매수"
                                                                     : "매도")
          .SetEntryTime(UtcTimestampToUtcDatetime(entry_time))
          .SetExitTime(UtcTimestampToUtcDatetime(exit_time))
          // 단순한 차를 쓰는 이유는 체결 시간의 정확성을 보장할 수 없으므로
          // 평균값을 사용하기 때문. 예를 들면, 1시 ~ 4시 주문일 시
          // 평균 1시 30분 진입 ~ 평균 4시 30분 청산 => 3시간 보유
          .SetHoldingTime(FormatTimeDiff(exit_time - entry_time))
          .SetLeverage(exit_order->GetLeverage())
          .SetEntryPrice(exit_order->GetEntryFilledPrice())
          .SetEntrySize(  // 부동 소수점 오류 방지를 위해 반올림
              RoundToStep(exit_order->GetEntryFilledSize(), qty_step))
          .SetExitPrice(exit_order->GetExitFilledPrice())
          .SetExitSize(  // 부동 소수점 오류 방지를 위해 반올림
              RoundToStep(exit_order->GetExitFilledSize(), qty_step))
          .SetLiquidationPrice(exit_order->GetLiquidationPrice())
          .SetReceivedFundingCount(received_funding_count)
          .SetReceivedFundingAmount(received_funding_amount)
          .SetPaidFundingCount(paid_funding_count)
          .SetPaidFundingAmount(paid_funding_amount)
          .SetTotalFundingCount(received_funding_count + paid_funding_count)
          .SetTotalFundingAmount(total_funding_amount)
          .SetEntryFee(entry_fee)
          .SetExitFee(exit_fee)
          .SetLiquidationFee(liquidation_fee)
          .SetPnl(realized_pnl)
          .SetNetPnl(realized_net_pnl)
          .SetIndividualPnlPer(realized_net_pnl / exit_order->GetEntryMargin() *
                               100)
          .SetTotalPnlPer(realized_net_pnl / exit_order->GetWbWhenEntryOrder() *
                          100)
          .SetWalletBalance(current_wallet_balance)
          .SetMaxWalletBalance(engine_->GetMaxWalletBalance())
          .SetDrawdown(engine_->GetDrawdown())
          .SetMaxDrawdown(engine_->GetMaxDrawdown())
          .SetCumPnl(cum_pnl)
          .SetCumPnlPer(cum_pnl / initial_balance * 100)
          .SetSymbolCount(symbol_count),
      exit_order->GetExitCount());
}

}  // namespace backtesting::order