// 표준 라이브러리
#include <format>

// 내부 헤더
#include "Engines/Logger.hpp"
#include "Engines/TimeUtils.hpp"

// 파일 헤더
#include "Engines/OrderHandler.hpp"

// 네임 스페이스
using namespace time_utils;

OrderHandler::OrderHandler() = default;
OrderHandler::~OrderHandler() = default;

mutex OrderHandler::mutex_;
unordered_map<string, unique_ptr<OrderHandler>> OrderHandler::instances_;

OrderHandler& OrderHandler::GetOrderHandler(const string& name) {
  lock_guard lock(mutex_);  // // 스레드에서 안전하게 접근하기 위해 mutex 사용

  // 해당 이름으로 인스턴스가 존재하는지 확인
  if (const auto& it = instances_.find(name); it == instances_.end()) {
    // 인스턴스가 없으면 생성 후 저장
    instances_[name] = make_unique<OrderHandler>();
  }

  return *instances_[name];
}

void OrderHandler::MarketEntry(const string& entry_name,
                               const Direction entry_direction,
                               const double entry_size,
                               const unsigned char leverage) {
  // 변수 계산
  const int symbol_idx = bar_.current_symbol_idx_;
  const size_t bar_idx = bar_.GetCurrentBarIndex(symbol_idx);
  const auto& trading_bar = bar_.GetBarData(BaseBarHandler::BarType::TRADING);
  int64_t next_open_time;  // 진입 시간: 다음 바의 시작 시간
  double next_open;        // 주문 가격: 다음 바의 시가
  try {
    next_open_time = trading_bar.GetOpenTime(symbol_idx, bar_idx + 1);
    next_open = trading_bar.GetOpen(symbol_idx, bar_idx + 1);
  } catch (...) {
    logger_.Log(Logger::LogLevel::WARNING_L,
                "마지막 바이기 때문에 진입할 수 없습니다.", __FILE__, __LINE__);
    return;
  }

  // 예외 체크
  try {
    IsValidEntryName(entry_name, symbol_idx);
    IsValidPositionSize(entry_size);
    IsValidLeverage(leverage);
  } catch (...) {
    return;
  }

  // 주문 생성
  const auto& entry_market = make_shared<Order>();
  entry_market->SetLeverage(leverage)
      .SetEntryName(entry_name)
      .SetEntryOrderType(OrderType::MARKET)
      .SetEntryDirection(entry_direction)
      .SetEntryOrderTime(next_open_time)
      .SetEntryOrderSize(entry_size)
      .SetEntryOrderPrice(next_open)
      .SetEntryFilledTime(next_open_time)
      .SetEntryFilledSize(entry_size);

  // 슬리피지가 포함된 체결가
  const double filled_price = CalculateSlippagePrice(
      next_open, OrderType::MARKET, entry_direction, entry_market);
  entry_market->SetEntryFilledPrice(filled_price);

  // 수수료
  const double commission = CalculateCommission(filled_price, OrderType::MARKET,
                                                entry_size, entry_market);
  entry_market->SetEntryCommission(commission);

  // 마진콜 가격
  const double margin_call_price = CalculateMarginCallPrice(entry_market);
  entry_market->SetMarginCallPrice(margin_call_price);

  // 현재 바에 자금 업데이트를 진행하지 않았다면 업데이트
  if (!engine_.unrealized_pnl_updated_) {
    engine_.UpdateUnrealizedPnl();
  }

  // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 후 주문
  for (const auto& pending_entry : pending_entries_[symbol_idx]) {
    if (entry_name == pending_entry->GetEntryName()) {
      // cancel @@@@@@@@@@@@@@@@@@@@@@
      ExecuteMarketEntry(entry_market, symbol_idx);
      return;
    }
  }

  // 해당 주문과 반대 방향의 체결 주문이 있으면 모두 청산
  for (const auto& filled_entry : filled_entries_[symbol_idx]) {
    if (entry_direction != filled_entry->GetEntryDirection()) {
      exit();
    }
  }

  ExecuteMarketEntry(entry_market, symbol_idx);
}

void OrderHandler::LimitEntry(const string& entry_name,
                              const Direction entry_direction,
                              const double entry_size,
                              const unsigned char leverage,
                              const double order_price) {
  // 변수 계산
  const int symbol_idx = bar_.current_symbol_idx_;
  const size_t bar_idx = bar_.GetCurrentBarIndex(symbol_idx);
  const auto& trading_bar = bar_.GetBarData(BaseBarHandler::BarType::TRADING);
  int64_t next_open_time;
  double next_open;
  try {
    next_open_time = trading_bar.GetOpenTime(symbol_idx, bar_idx + 1);
    next_open = trading_bar.GetOpen(symbol_idx, bar_idx + 1);
  } catch (...) {
    logger_.Log(Logger::LogLevel::WARNING_L,
                "마지막 바이기 때문에 진입할 수 없습니다.", __FILE__, __LINE__);
    return;
  }
  // @@@@@@@@@@@@@@@@@@@@@@@@@ Limit부터 Trailing까지 체결 시 EntryName Valid
  // Check할 것 엔트리 네임은 하나만 체결 가능함

  // 예외 체크
  try {
    IsValidPositionSize(entry_size);
    IsValidLeverage(leverage);
    IsValidPrice(order_price);
    IsValidLimitOrderPrice(order_price, next_open, entry_direction, symbol_idx,
                           next_open_time);
  } catch (...) {
    return;
  }

  // 주문 생성
  const auto& entry_limit = make_shared<Order>();
  entry_limit->SetLeverage(leverage)
      .SetEntryName(entry_name)
      .SetEntryOrderType(OrderType::LIMIT)
      .SetEntryDirection(entry_direction)
      .SetEntryOrderTime(next_open_time)
      .SetEntryOrderSize(entry_size)
      .SetEntryOrderPrice(order_price);

  // 현재 바에 자금 업데이트를 진행하지 않았다면 업데이트
  if (!engine_.unrealized_pnl_updated_) {
    engine_.UpdateUnrealizedPnl();
  }

  // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 후 주문
  for (const auto& pending_entry : pending_entries_[symbol_idx]) {
    if (entry_name == pending_entry->GetEntryName()) {
      // @@@@@@@@@@@@@@@@ cancel
      ExecuteLimitEntry(entry_limit, symbol_idx);
      return;
    }
  }

  // 없으면 바로 주문
  ExecuteLimitEntry(entry_limit, symbol_idx);
}

void OrderHandler::MitEntry(const string& entry_name, Direction entry_direction,
                            const double entry_size,
                            const unsigned char leverage,
                            const double touch_price) {
  // 예외 체크
  try {
    IsValidPositionSize(entry_size);
    IsValidLeverage(leverage);
    IsValidPrice(touch_price);
  } catch (...) {
    return;
  }

  // 변수 계산
  const int symbol_idx = bar_.current_symbol_idx_;

  // 주문 생성
  const auto& entry_mit = make_shared<Order>();
  entry_mit->SetLeverage(leverage)
      .SetEntryName(entry_name)
      .SetEntryOrderType(OrderType::MIT)
      .SetEntryDirection(entry_direction)
      .SetEntryTouchPrice(touch_price)
      .SetEntryOrderSize(entry_size);

  // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 후 주문
  for (const auto& pending_entry : pending_entries_[symbol_idx]) {
    if (entry_name == pending_entry->GetEntryName()) {
      // @@@@@@@@@@@@@@@@ cancel
      ExecuteMitEntry(entry_mit, symbol_idx);
      return;
    }
  }

  // 없으면 바로 주문
  ExecuteMitEntry(entry_mit, symbol_idx);
}

void OrderHandler::LitEntry(const string& entry_name,
                            const Direction entry_direction,
                            const double entry_size,
                            const unsigned char leverage,
                            const double touch_price,
                            const double order_price) {
  // 변수 계산
  const int symbol_idx = bar_.current_symbol_idx_;
  const size_t bar_idx = bar_.GetCurrentBarIndex(symbol_idx);
  int64_t log_time;
  try {
    log_time = bar_.GetBarData(BaseBarHandler::BarType::TRADING)  // 로그용 시간
                   .GetOpenTime(symbol_idx, bar_idx + 1);
  } catch (...) {
    logger_.Log(Logger::LogLevel::WARNING_L,
                "마지막 바이기 때문에 진입할 수 없습니다.", __FILE__, __LINE__);
    return;
  }

  // 예외 체크
  try {
    IsValidPositionSize(entry_size);
    IsValidLeverage(leverage);
    IsValidPrice(touch_price);
    IsValidPrice(order_price);
    IsValidLimitOrderPrice(order_price, touch_price, entry_direction,
                           symbol_idx, log_time);
  } catch (...) {
    return;
  }

  // 주문 생성
  const auto& entry_lit = make_shared<Order>();
  entry_lit->SetLeverage(leverage)
      .SetEntryName(entry_name)
      .SetEntryOrderType(OrderType::LIT)
      .SetEntryDirection(entry_direction)
      .SetEntryTouchPrice(touch_price)
      .SetEntryOrderSize(entry_size)
      .SetEntryOrderPrice(order_price);

  // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 후 주문
  for (const auto& pending_entry : pending_entries_[symbol_idx]) {
    if (entry_name == pending_entry->GetEntryName()) {
      // @@@@@@@@@@@@@@@@ cancel
      ExecuteLitEntry(entry_lit, symbol_idx);
      return;
    }
  }

  // 없으면 바로 주문
  ExecuteLitEntry(entry_lit, symbol_idx);
}

void OrderHandler::TrailingEntry(const string& entry_name,
                                 const Direction entry_direction,
                                 const double entry_size,
                                 const unsigned char leverage,
                                 const double touch_price,
                                 const double trail_point) {
  // 변수 계산
  const int symbol_idx = bar_.current_symbol_idx_;
  const auto bar_idx = bar_.GetCurrentBarIndex(symbol_idx);
  double next_open;
  try {
    next_open = bar_.GetBarData(BaseBarHandler::BarType::TRADING)
                    .GetOpen(symbol_idx, bar_idx + 1);
  } catch (...) {
    logger_.Log(Logger::LogLevel::WARNING_L,
                "마지막 바이기 때문에 진입할 수 없습니다.", __FILE__, __LINE__);
    return;
  }

  // 예외 체크
  try {
    IsValidPositionSize(entry_size);
    IsValidLeverage(leverage);
    IsValidTrailingTouchPrice(touch_price, symbol_idx);
    IsValidTrailPoint(trail_point, symbol_idx);
  } catch (...) {
    return;
  }

  // 주문 생성
  const auto& entry_trailing = make_shared<Order>();
  entry_trailing->SetLeverage(leverage)
      .SetEntryName(entry_name)
      .SetEntryOrderType(OrderType::TRAILING)
      .SetEntryDirection(entry_direction)
      .SetEntryTouchPrice(touch_price)
      .SetEntryTrailPoint(trail_point);

  // touch_price가 0이면 다음 시가부터 최고저가 추적을 시작
  if (!touch_price) {
    entry_trailing->SetEntryExtremePrice(next_open);
  }

  // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 후 주문
  for (const auto& pending_entry : pending_entries_[symbol_idx]) {
    if (entry_name == pending_entry->GetEntryName()) {
      // @@@@@@@@@@@@@@@@ cancel
      ExecuteTrailingEntry(entry_trailing, symbol_idx);
      return;
    }
  }

  // 없으면 바로 주문
  ExecuteTrailingEntry(entry_trailing, symbol_idx);
}

void OrderHandler::MarketExit(const string& exit_name,
                              const string& target_entry,
                              const double exit_size) {
  // 예외 체크
  try {
    IsValidPositionSize(exit_size);
  } catch (...) {
    return;
  }

  // 같은 이름의 대기 중인 청산 주문이 있으면 취소 후 재주문
  for (const auto& pending_exit : pending_exits_[bar_.current_symbol_idx_]) {
    if (exit_name == pending_exit->GetExitName()) {
      // cancel @@@@@@@@@@@
      ExecuteMarketExit(exit_name, target_entry, exit_size);
      return;
    }
  }

  // 없으면 바로 주문
  ExecuteMarketExit(exit_name, target_entry, exit_size);
}

void OrderHandler::LimitExit(const string& exit_name,
                             const string& target_entry, const double exit_size,
                             const double order_price) {
  // 예외 체크
  try {
    IsValidPositionSize(exit_size);
    IsValidPrice(order_price);
  } catch (...) {
    return;
  }

  // 같은 이름의 대기 중인 청산 주문이 있으면 취소 후 재주문
  for (const auto& pending_exit : pending_exits_[bar_.current_symbol_idx_]) {
    if (exit_name == pending_exit->GetExitName()) {
      // cancel @@@@@@@@@@@
      ExecuteLimitExit(exit_name, target_entry, exit_size, order_price);
      return;
    }
  }

  // 없으면 바로 주문
  ExecuteLimitExit(exit_name, target_entry, exit_size, order_price);
}

void OrderHandler::MitExit(const string& exit_name, const string& target_entry,
                           const double exit_size, const double touch_price) {
  // 예외 체크
  try {
    IsValidPositionSize(exit_size);
    IsValidPrice(touch_price);
  } catch (...) {
    return;
  }

  // 같은 이름의 대기 중인 청산 주문이 있으면 취소 후 재주문
  for (const auto& pending_exit : pending_exits_[bar_.current_symbol_idx_]) {
    if (exit_name == pending_exit->GetExitName()) {
      // cancel @@@@@@@@@@@
      ExecuteMitExit(exit_name, target_entry, exit_size, touch_price);
      return;
    }
  }

  // 없으면 바로 주문
  ExecuteMitExit(exit_name, target_entry, exit_size, touch_price);
}

void OrderHandler::LitExit(const string& exit_name, const string& target_entry,
                           const double exit_size, const double touch_price,
                           const double order_price) {
  // 예외 체크
  try {
    IsValidPositionSize(exit_size);
    IsValidPrice(touch_price);
    IsValidPrice(order_price);
  } catch (...) {
    return;
  }

  // 같은 이름의 대기 중인 청산 주문이 있으면 취소 후 재주문
  for (const auto& pending_exit : pending_exits_[bar_.current_symbol_idx_]) {
    if (exit_name == pending_exit->GetExitName()) {
      // cancel @@@@@@@@@@@
      ExecuteLitExit(exit_name, target_entry, exit_size, touch_price,
                     order_price);
      return;
    }
  }

  // 없으면 바로 주문
  ExecuteLitExit(exit_name, target_entry, exit_size, touch_price, order_price);
}

void OrderHandler::TrailingExit(const string& exit_name,
                                const string& target_entry,
                                const double exit_size,
                                const double touch_price,
                                const double trail_point) {
  // 변수 계산
  const int symbol_idx = bar_.current_symbol_idx_;

  // 예외 체크
  try {
    IsValidPositionSize(exit_size);
    IsValidTrailingTouchPrice(touch_price, symbol_idx);
    IsValidTrailPoint(trail_point, symbol_idx);
  } catch (...) {
    return;
  }

  // 같은 이름의 대기 중인 청산 주문이 있으면 취소 후 재주문
  for (const auto& pending_exit : pending_exits_[symbol_idx]) {
    if (exit_name == pending_exit->GetExitName()) {
      // cancel @@@@@@@@@@@
      ExecuteTrailingExit(exit_name, target_entry, exit_size, touch_price,
                          trail_point);
      return;
    }
  }

  // 없으면 바로 주문
  ExecuteTrailingExit(exit_name, target_entry, exit_size, touch_price,
                      trail_point);
}

void OrderHandler::ExecuteMarketEntry(const shared_ptr<Order>& order,
                                      const int symbol_idx) {
  // 변수 계산
  const double filled_price = order->GetEntryFilledPrice();
  const int64_t filled_time = order->GetEntryFilledTime();
  const double commission = order->GetEntryCommission();

  // 진입 증거금 계산: 1포인트 == 1달러 가정 계산
  const double entry_margin = order->GetEntryFilledSize() * filled_price;

  // 진입 가능 여부 체크 (진입 가능 자금 > 진입 증거금 + 수수료)
  if (!HasEnoughBalance(engine_.GetAvailableBalance(),
                        entry_margin + commission, symbol_idx, filled_time)) {
    return;
  }

  // 현재 자금(주문 가능 자금)에서 수수료 감소
  engine_.DecreaseWalletBalance(commission);

  // 주문 가능 자금에서 진입 증거금 감소
  engine_.DecreaseAvailableBalance(entry_margin);

  // 시장가 진입
  filled_entries_[symbol_idx].push_back(order);

  // 디버그 로그 기록
  if (engine_.debug_mode_) {
    string direction =
        order->GetEntryDirection() == Direction::LONG ? "매수" : "매도";
    logger_.Log(
        Logger::LogLevel::DEBUG_L,
        format("{} {} {} | 시장가 {} 진입 체결",
               bar_.GetBarData(BaseBarHandler::BarType::TRADING)
                   .GetSymbolName(symbol_idx),
               UtcTimestampToUtcDatetime(filled_time), filled_price, direction),
        __FILE__, __LINE__);
  }
}

void OrderHandler::ExecuteLimitEntry(const shared_ptr<Order>& order,
                                     const int symbol_idx) {
  // 변수 계산
  const double order_price = order->GetEntryOrderPrice();
  const double order_size = order->GetEntryOrderSize();

  // 예약 증거금 계산: 1포인트 == 1달러 가정 계산
  const double entry_margin = order_price * order_size;

  // 진입 가능 여부 체크
  if (!HasEnoughBalance(engine_.GetAvailableBalance(), entry_margin, symbol_idx,
                        order->GetEntryOrderTime())) {
    return;
  }

  // 주문 가능 자금에서 예약 증거금 감소
  engine_.DecreaseAvailableBalance(entry_margin);

  // 지정가 진입 대기
  pending_entries_[symbol_idx].push_back(order);

  // 디버그 로그 기록
  if (engine_.debug_mode_) {
    string direction =
        order->GetEntryDirection() == Direction::LONG ? "매수" : "매도";
    logger_.Log(Logger::LogLevel::DEBUG_L,
                format("{} {} {} | 지정가 {} 진입 대기",
                       bar_.GetBarData(BaseBarHandler::BarType::TRADING)
                           .GetSymbolName(symbol_idx),
                       time_utils::UtcTimestampToUtcDatetime(
                           order->GetEntryOrderTime()),
                       order_price, direction),
                __FILE__, __LINE__);
  }
}

void OrderHandler::ExecuteMitEntry(const shared_ptr<Order>& order,
                                   const int symbol_idx) {
  // MIT 진입 터치 대기
  pending_entries_[symbol_idx].push_back(order);

  // 디버그 로그 기록
  if (engine_.debug_mode_) {
    string direction =
        order->GetEntryDirection() == Direction::LONG ? "매수" : "매도";
    logger_.Log(Logger::LogLevel::DEBUG_L,
                format("{} {} | MIT {} 진입 터치 대기",
                       bar_.GetBarData(BaseBarHandler::BarType::TRADING)
                           .GetSymbolName(symbol_idx),
                       order->GetEntryTouchPrice(), direction),
                __FILE__, __LINE__);
  }
}

void OrderHandler::ExecuteLitEntry(const shared_ptr<Order>& order,
                                   const int symbol_idx) {
  // LIT 진입 터치 대기
  pending_entries_[symbol_idx].push_back(order);

  // 디버그 로그 기록
  if (engine_.debug_mode_) {
    string direction =
        order->GetEntryDirection() == Direction::LONG ? "매수" : "매도";
    logger_.Log(Logger::LogLevel::DEBUG_L,
                format("{} {} | LIT {} 진입 터치 대기",
                       bar_.GetBarData(BaseBarHandler::BarType::TRADING)
                           .GetSymbolName(symbol_idx),
                       order->GetEntryTouchPrice(), direction),
                __FILE__, __LINE__);
  }
}

void OrderHandler::ExecuteTrailingEntry(const shared_ptr<Order>& order,
                                        const int symbol_idx) {
  // Trailing 진입 터치 대기
  pending_entries_[symbol_idx].push_back(order);

  // 디버그 로그 기록
  if (engine_.debug_mode_) {
    string direction =
        order->GetEntryDirection() == Direction::LONG ? "매수" : "매도";
    logger_.Log(Logger::LogLevel::DEBUG_L,
                format("{} {} | Trailing {} 진입 터치 대기",
                       bar_.GetBarData(BaseBarHandler::BarType::TRADING)
                           .GetSymbolName(symbol_idx),
                       order->GetEntryTouchPrice(), direction),
                __FILE__, __LINE__);
  }
}

void OrderHandler::ExecuteMarketExit(const string& exit_name,
                                     const string& target_entry,
                                     const double exit_size) {
  // 변수 계산
  const auto symbol_idx = bar_.current_symbol_idx_;
  const auto bar_idx = bar_.GetCurrentBarIndex(symbol_idx);
  const auto& trading_bar = bar_.GetBarData(BaseBarHandler::BarType::TRADING);

  // 체결된 진입들 순회
  for (int i = 0; i < filled_entries_[symbol_idx].size(); i++) {
    // target_entry와 같은 이름의 진입이 있으면 청산
    if (const auto& filled_entry = filled_entries_[symbol_idx][i];
        filled_entry->GetEntryName() == target_entry) {
      // Entry Order 복사
      const auto& exit_market = make_shared<Order>(*filled_entry);

      // 변수 계산
      int64_t next_open_time;
      double next_open;
      try {
        next_open_time = trading_bar.GetOpenTime(symbol_idx, bar_idx + 1);
        next_open = trading_bar.GetOpen(symbol_idx, bar_idx + 1);
      } catch (...) {
        logger_.Log(Logger::LogLevel::WARNING_L,
                    "마지막 바이기 때문에 청산할 수 없습니다.", __FILE__,
                    __LINE__);
        return;
      }

      const double adjusted_exit_size =
          CalculateAdjustedExitSize(exit_size, exit_market);

      // 청산 주문 생성
      exit_market->SetExitName(exit_name)
          .SetExitOrderType(OrderType::MARKET)
          .SetExitDirection(exit_market->GetEntryDirection() == Direction::LONG
                                ? Direction::SHORT
                                : Direction::LONG)
          .SetExitOrderTime(next_open_time)
          .SetExitOrderSize(adjusted_exit_size)
          .SetExitOrderPrice(next_open)
          .SetExitFilledTime(next_open_time)
          .SetExitFilledSize(adjusted_exit_size);

      // 슬리피지가 포함된 체결가
      const double filled_price =
          CalculateSlippagePrice(next_open, OrderType::MARKET,
                                 exit_market->GetExitDirection(), exit_market);
      exit_market->SetExitFilledPrice(filled_price);

      // 수수료
      const double commission = CalculateCommission(
          filled_price, OrderType::MARKET, adjusted_exit_size, exit_market);
      exit_market->SetExitCommission(commission);

      // 원본 진입 객체에 청산 체결 수량 추가
      filled_entry->SetExitFilledSize(filled_entry->GetExitFilledSize() +
                                      adjusted_exit_size);

      /* 원본 진입 객체의 청산 체결 수량이 진입 체결 수량과 같으면
        filled_entries에서 삭제 (부동 소수점 오류 방지용 '>=') */
      if (filled_entry->GetExitFilledSize() >=
          filled_entry->GetEntryFilledSize()) {
        filled_entries_[symbol_idx].erase(filled_entries_[symbol_idx].begin() +
                                          i);
      }

      /* 청산 체결 -> 같은 진입 목표의 청산 대기 주문 취소
       * ※ 중요: 취소하는 이유는 원본 진입 객체의 청산 체결 수량이 달라졌기
       * 때문에, 기존 청산 대기 주문들의 청산 주문 수량이 진입 체결 수량을
       * 초과할 가능성이 있기 때문. 이때, 엔진이 청산 대기 주문의 수량 변경을
       * 강요할 수 없기 때문에 자체적으로 일괄 취소하는 것. 사용자가 재주문해야
       * 함.
       */
      erase_if(pending_exits_[symbol_idx],
               [&](const shared_ptr<Order>& pending_exit) {
                 return pending_exit->GetEntryName() == target_entry;
               });

      // 자금, 통계 업데이트
      ExecuteExit(exit_market);

      if (engine_.debug_mode_) {
        string direction = exit_market->GetExitDirection() == Direction::LONG
                               ? "매수"
                               : "매도";
        logger_.Log(Logger::LogLevel::DEBUG_L,
                    format("{} {} {} | 시장가 {} 청산 체결",
                           bar_.GetBarData(BaseBarHandler::BarType::TRADING)
                               .GetSymbolName(symbol_idx),
                           UtcTimestampToUtcDatetime(next_open_time),
                           filled_price, direction),
                    __FILE__, __LINE__);
      }

      return;
    }
  }

  // 목표한 진입 이름을 찾지 못하면 경고 로그
  NotExistEntryName(target_entry);
}

void OrderHandler::ExecuteLimitExit(const string& exit_name,
                                    const string& target_entry,
                                    const double exit_size,
                                    const double order_price) {
  // 변수 계산
  const auto symbol_idx = bar_.current_symbol_idx_;
  const auto bar_idx = bar_.GetCurrentBarIndex(symbol_idx);
  const auto& trading_bar = bar_.GetBarData(BaseBarHandler::BarType::TRADING);

  // 체결된 진입들 순회
  for (int i = 0; i < filled_entries_[symbol_idx].size(); i++) {
    // target_entry와 같은 이름의 진입이 있으면 청산
    if (const auto& filled_entry = filled_entries_[symbol_idx][i];
        filled_entry->GetEntryName() == target_entry) {
      // Entry Order 복사
      const auto& exit_limit = make_shared<Order>(*filled_entry);

      // 변수 계산
      int64_t next_open_time;
      double next_open;
      try {
        next_open_time = trading_bar.GetOpenTime(symbol_idx, bar_idx + 1);
        next_open = trading_bar.GetOpen(symbol_idx, bar_idx + 1);
      } catch (...) {
        logger_.Log(Logger::LogLevel::WARNING_L,
                    "마지막 바이기 때문에 청산할 수 없습니다.", __FILE__,
                    __LINE__);
        return;
      }

      // 청산 주문 생성
      exit_limit->SetExitName(exit_name)
          .SetExitOrderType(OrderType::LIMIT)
          .SetExitDirection(exit_limit->GetEntryDirection() == Direction::LONG
                                ? Direction::SHORT
                                : Direction::LONG)
          .SetExitOrderTime(next_open_time)
          .SetExitOrderSize(CalculateAdjustedExitSize(exit_size, exit_limit))
          .SetExitOrderPrice(order_price);

      // 유효성 검사
      try {
        IsValidLimitOrderPrice(order_price, next_open,
                               exit_limit->GetExitDirection(), symbol_idx,
                               next_open_time);
      } catch (...) {
        return;
      }

      // 대기 중인 청산에 추가
      pending_exits_[symbol_idx].push_back(exit_limit);
      return;
    }
  }

  // 목표한 진입 이름을 찾지 못하면 경고 로그
  NotExistEntryName(target_entry);
}

void OrderHandler::ExecuteMitExit(const string& exit_name,
                                  const string& target_entry,
                                  const double exit_size,
                                  const double touch_price) {
  // 변수 계산
  const auto symbol_idx = bar_.current_symbol_idx_;

  // 체결된 진입들 순회
  for (int i = 0; i < filled_entries_[symbol_idx].size(); i++) {
    // target_entry와 같은 이름의 진입이 있으면 청산
    if (const auto& filled_entry = filled_entries_[symbol_idx][i];
        filled_entry->GetEntryName() == target_entry) {
      // Entry Order 복사
      const auto& exit_mit = make_shared<Order>(*filled_entry);

      // 청산 주문 생성
      exit_mit->SetExitName(exit_name)
          .SetExitOrderType(OrderType::MIT)
          .SetExitDirection(exit_mit->GetEntryDirection() == Direction::LONG
                                ? Direction::SHORT
                                : Direction::LONG)
          .SetExitTouchPrice(touch_price)
          .SetExitOrderSize(CalculateAdjustedExitSize(exit_size, exit_mit));

      // 대기 중인 청산에 추가
      pending_exits_[symbol_idx].push_back(exit_mit);
      return;
    }
  }

  // 목표한 진입 이름을 찾지 못하면 경고 로그
  NotExistEntryName(target_entry);
}

void OrderHandler::ExecuteLitExit(const string& exit_name,
                                  const string& target_entry,
                                  const double exit_size,
                                  const double touch_price,
                                  const double order_price) {
  // 변수 계산
  const auto symbol_idx = bar_.current_symbol_idx_;
  const auto bar_idx = bar_.GetCurrentBarIndex(symbol_idx);

  // 체결된 진입들 순회
  for (int i = 0; i < filled_entries_[symbol_idx].size(); i++) {
    // target_entry와 같은 이름의 진입이 있으면 청산
    if (const auto& filled_entry = filled_entries_[symbol_idx][i];
        filled_entry->GetEntryName() == target_entry) {
      // Entry Order 복사
      const auto& exit_lit = make_shared<Order>(*filled_entry);

      // 변수 계산
      int64_t log_time;
      try {
        log_time = bar_.GetBarData(BaseBarHandler::BarType::TRADING)
                       .GetOpenTime(symbol_idx, bar_idx + 1);
      } catch (...) {
        return;
      }

      // 청산 주문 생성
      exit_lit->SetExitName(exit_name)
          .SetExitOrderType(OrderType::LIT)
          .SetExitDirection(exit_lit->GetEntryDirection() == Direction::LONG
                                ? Direction::SHORT
                                : Direction::LONG)
          .SetExitTouchPrice(touch_price)
          .SetExitOrderSize(CalculateAdjustedExitSize(exit_size, exit_lit))
          .SetExitOrderPrice(order_price);

      // 유효성 검사
      try {
        IsValidLimitOrderPrice(order_price, touch_price,
                               exit_lit->GetExitDirection(), symbol_idx,
                               log_time);
      } catch (...) {
        return;
      }

      // 대기 중인 청산에 추가
      pending_exits_[symbol_idx].push_back(exit_lit);
      return;
    }
  }

  // 목표한 진입 이름을 찾지 못하면 경고 로그
  NotExistEntryName(target_entry);
}

void OrderHandler::ExecuteTrailingExit(const string& exit_name,
                                       const string& target_entry,
                                       const double exit_size,
                                       const double touch_price,
                                       const double trail_point) {
  // 변수 계산
  const auto symbol_idx = bar_.current_symbol_idx_;
  const auto bar_idx = bar_.GetCurrentBarIndex(symbol_idx);

  // 체결된 진입들 순회
  for (int i = 0; i < filled_entries_[symbol_idx].size(); i++) {
    // target_entry와 같은 이름의 진입이 있으면 청산
    if (const auto& filled_entry = filled_entries_[symbol_idx][i];
        filled_entry->GetEntryName() == target_entry) {
      // Entry Order 복사
      const auto& exit_trailing = make_shared<Order>(*filled_entry);

      // 변수 계산
      double next_open;
      try {
        next_open = bar_.GetBarData(BaseBarHandler::BarType::TRADING)
                        .GetOpen(symbol_idx, bar_idx + 1);
      } catch (...) {
        logger_.Log(Logger::LogLevel::WARNING_L,
                    "마지막 바이기 때문에 청산할 수 없습니다.", __FILE__,
                    __LINE__);
        return;
      }

      // 청산 주문 생성
      exit_trailing->SetExitName(exit_name)
          .SetExitOrderType(OrderType::TRAILING)
          .SetExitDirection(exit_trailing->GetEntryDirection() ==
                                    Direction::LONG
                                ? Direction::SHORT
                                : Direction::LONG)
          .SetExitTouchPrice(touch_price)
          .SetExitTrailPoint(trail_point)
          .SetExitOrderSize(
              CalculateAdjustedExitSize(exit_size, exit_trailing));

      // touch_price가 0이면 다음 시가부터 최고저가 추적을 시작
      if (!touch_price) {
        exit_trailing->SetEntryExtremePrice(next_open);
      }

      // 대기 중인 청산에 추가
      pending_exits_[symbol_idx].push_back(exit_trailing);
      return;
    }
  }

  // 목표한 진입 이름을 찾지 못하면 경고 로그
  NotExistEntryName(target_entry);
}

void OrderHandler::ExecuteExit(const shared_ptr<Order>& exit) {
  const double exit_filled_size = exit->GetExitFilledSize();

  // 미실현 손익 업데이트
  engine_.UpdateUnrealizedPnl();

  // 현재 자금(주문 가능 자금)에서 수수료 감소
  engine_.DecreaseWalletBalance(exit->GetExitCommission());

  // 진입 증거금 회복
  engine_.IncreaseAvailableBalance(exit_filled_size *
                                   exit->GetEntryFilledPrice());

  // 손익 계산
  double pnl;
  if (exit->GetEntryDirection() == Direction::LONG) {
    pnl = (exit->GetExitFilledPrice() - exit->GetEntryFilledPrice()) *
          exit_filled_size * exit->GetLeverage();
  } else {
    pnl = (exit->GetEntryFilledPrice() - exit->GetExitFilledPrice()) *
          exit_filled_size * exit->GetLeverage();
  }

  // 현재 자금(주문 가능 자금)에 손익 합산
  if (pnl > 0) {
    engine_.IncreaseWalletBalance(pnl);
  } else if (pnl < 0) {
    engine_.DecreaseWalletBalance(abs(pnl));
  }

  // 엔진 업데이트 항목들 업데이트 @@@@@@@@@@@@ mdd 같은거: 함수로 만들자요

  filled_exits_.push_back(exit);  // 체결된 청산에 추가
  // 통계 추가
  // filled_exit 단일 벡터로 만들고 심볼정보 등 통계를 위해 필요한 항목들 모두 추가후
  // 마지막에 일괄 계산하여 트레이딩 리스트 작성하자 -> 오버헤드 감소용
  // @@@@@@@@@@@@@@@@@@@@@@ 추후 추가하자
}

bool OrderHandler::HasEnoughBalance(const double available_balance,
                                    const double needed_balance,
                                    const int symbol_idx,
                                    const int64_t order_time) {
  if (available_balance < needed_balance) {
    logger_.Log(Logger::WARNING_L,
                format("{} {} | 필요 자금이 부족합니다. | 주문 가능 자금: {} | "
                       "필요 자금: {}",
                       bar_.GetBarData(BaseBarHandler::BarType::TRADING)
                           .GetSymbolName(symbol_idx),
                       time_utils::UtcTimestampToUtcDatetime(order_time),
                       available_balance, needed_balance),
                __FILE__, __LINE__);

    return false;
  }

  return true;
}

double OrderHandler::CalculateAdjustedExitSize(const double exit_size,
                                               const shared_ptr<Order>& exit_order) {
  // 청산 수량 + 분할 청산한 수량이 진입 수량보다 많다면
  if (const auto entry_size = exit_order->GetEntryFilledSize();
    exit_size + exit_order->GetExitFilledSize() > entry_size) {
    // 최대값으로 조정하여 반환
    return entry_size - exit_order->GetExitFilledSize();
  }

  return exit_size;
}

void OrderHandler::IsValidEntryName(const string& entry_name, const int symbol_idx) const {
  /* 같은 이름으로 체결된 Entry Name이 여러 개 존재하면, 청산 시 Target Entry 지정할 때의
     로직이 꼬이기 때문에 하나의 Entry Name은 하나의 진입 체결로 제한 */
  for (const auto& filled_entry : filled_entries_[bar_.current_symbol_idx_]) {
    if (entry_name == filled_entry->GetEntryName()) {
      Logger::LogAndThrowError(
        format("{} | 중복된 진입 이름 {}(으)로 동시에 진입 체결이 불가능합니다. "
               "다른 진입 이름으로 주문을 넣어주세요.",
               bar_.GetBarData(BaseBarHandler::BarType::TRADING).GetSymbolName(symbol_idx), entry_name),
               __FILE__, __LINE__);
    }
  }
}

void OrderHandler::IsValidLimitOrderPrice(const double limit_price,
                                     const double base_price,
                                     const Direction direction,
                                     const int symbol_idx,
                                     const int64_t order_time) {
  if (direction == Direction::LONG) {
    if (limit_price >= base_price) {
      Logger::LogAndThrowError(
          format("{} {} | 지정가 {} 매수 주문은 기준가 {}보다 작아야합니다.",
                 bar_.GetBarData(BaseBarHandler::BarType::TRADING).GetSymbolName(symbol_idx),
                 UtcTimestampToUtcDatetime(order_time), limit_price, base_price),
                 __FILE__, __LINE__);
    }
  } else {
    if (limit_price <= base_price) {
      Logger::LogAndThrowError(
        format("{} {} | 지정가 {} 매도 주문은 기준가 {}보다 커야합니다.",
          bar_.GetBarData(BaseBarHandler::BarType::TRADING).GetSymbolName(symbol_idx),
          time_utils::UtcTimestampToUtcDatetime(order_time), limit_price, base_price),
          __FILE__, __LINE__);
    }
  }
}

void OrderHandler::IsValidTrailingTouchPrice(const double touch_price,
                                             const int symbol_idx) {
  if (touch_price < 0) {
    Logger::LogAndThrowError(
        format("{} | 주어진 트레일링 터치 가격 {}은(는) 0과 같거나 커야합니다.",
          bar_.GetBarData(BaseBarHandler::BarType::TRADING).GetSymbolName(symbol_idx), touch_price),
          __FILE__, __LINE__);
  }
}

void OrderHandler::IsValidTrailPoint(double trail_point, const int symbol_idx) {
  if (trail_point <= 0) {
    Logger::LogAndThrowError(
          format("{} | 주어진 트레일링 포인트 {}은(는) 0보다 커야합니다.",
            bar_.GetBarData(BaseBarHandler::BarType::TRADING).GetSymbolName(symbol_idx),
            trail_point),
            __FILE__, __LINE__);
  }
}

void OrderHandler::NotExistEntryName(const string& entry_name) {
  if (engine_.debug_mode_) {
    logger_.Log(
        Logger::LogLevel::WARNING_L,
        format("지정된 진입명 {}이(가) 존재하지 않아 청산할 수 없습니다.",
               entry_name),
        __FILE__, __LINE__);
  }
}
