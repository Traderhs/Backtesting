// 표준 라이브러리
#include <format>

// 파일 헤더
#include "Engines\OrderHandler.hpp"

// 내부 헤더
#include "Engines\BarData.hpp"
#include "Engines\BarHandler.hpp"
#include "Engines\Engine.hpp"
#include "Engines\TimeUtils.hpp"

// 네임 스페이스
using namespace time_utils;

OrderHandler::OrderHandler() = default;
void OrderHandler::Deleter::operator()(const OrderHandler* p) const {
  delete p;
}

mutex OrderHandler::mutex_;
unordered_map<string, shared_ptr<OrderHandler>> OrderHandler::instances_;

shared_ptr<OrderHandler>& OrderHandler::GetOrderHandler(const string& name) {
  lock_guard lock(mutex_);  // // 스레드에서 안전하게 접근하기 위해 mutex 사용

  // 해당 이름으로 인스턴스가 존재하는지 확인
  if (const auto& it = instances_.find(name); it == instances_.end()) {
    // 인스턴스가 없으면 생성 후 저장
    instances_[name] = shared_ptr<OrderHandler>(new OrderHandler(), Deleter());
  }

  return instances_[name];
}

void OrderHandler::MarketEntry(const string& entry_name,
                               const Direction entry_direction,
                               const double entry_size,
                               const unsigned char leverage) {
  // 변수 계산
  const int symbol_idx = bar_->GetCurrentSymbolIndex();
  const size_t bar_idx = bar_->GetCurrentBarIndex();
  const auto& bar = bar_->GetBarData(bar_->GetCurrentBarType());
  int64_t next_open_time;  // 진입 시간: 다음 바의 시작 시간
  double next_open;        // 주문 가격: 다음 바의 시가
  try {
    next_open_time = bar.GetOpenTime(symbol_idx, bar_idx + 1);
    next_open = bar.GetOpen(symbol_idx, bar_idx + 1);
  } catch (...) {
    logger_->Log(LogLevel::WARNING_L,
                 "마지막 바이기 때문에 진입할 수 없습니다.", __FILE__,
                 __LINE__);
    return;
  }

  // 예외 체크
  try {
    IsValidEntryName(entry_name);
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
  if (!engine_->unrealized_pnl_updated_) {
    engine_->UpdateUnrealizedPnl();
  }

  // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 후 주문
  for (const auto& pending_entry : pending_entries_[symbol_idx]) {
    if (entry_name == pending_entry->GetEntryName()) {
      LogCancelAndReorder(entry_name);
      Cancel(entry_name);
      ExecuteMarketEntry(entry_market);
      return;
    }
  }

  // 해당 주문과 반대 방향의 체결 주문이 있으면 모두 청산
  for (const auto& filled_entry : filled_entries_[symbol_idx]) {
    if (entry_direction != filled_entry->GetEntryDirection()) {
      const string& direction =
          filled_entry->GetEntryDirection() == Direction::LONG ? "매도"
                                                               : "매수";

      MarketExit("리버스 시장가 " + direction, filled_entry->GetEntryName(),
                 filled_entry->GetEntryFilledSize());
    }
  }

  ExecuteMarketEntry(entry_market);
}

void OrderHandler::LimitEntry(const string& entry_name,
                              const Direction entry_direction,
                              const double entry_size,
                              const unsigned char leverage,
                              const double order_price) {
  // 변수 계산
  const int symbol_idx = bar_->GetCurrentSymbolIndex();
  const size_t bar_idx = bar_->GetCurrentBarIndex();
  const auto& bar = bar_->GetBarData(bar_->GetCurrentBarType());
  int64_t next_open_time;
  double next_open;
  try {
    next_open_time = bar.GetOpenTime(symbol_idx, bar_idx + 1);
    next_open = bar.GetOpen(symbol_idx, bar_idx + 1);
  } catch (...) {
    logger_->Log(LogLevel::WARNING_L,
                 "마지막 바이기 때문에 진입할 수 없습니다.", __FILE__,
                 __LINE__);
    return;
  }
  // @@@@@@@@@@@@@@@@@@@@@@@@@ Limit부터 Trailing까지 체결 시 EntryName Valid
  // Check할 것 엔트리 네임은 하나만 체결 가능함

  // 예외 체크
  try {
    IsValidPositionSize(entry_size);
    IsValidLeverage(leverage);
    IsValidPrice(order_price);
    IsValidLimitOrderPrice(order_price, next_open, entry_direction);
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
  if (!engine_->unrealized_pnl_updated_) {
    engine_->UpdateUnrealizedPnl();
  }

  // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 후 주문
  for (const auto& pending_entry : pending_entries_[symbol_idx]) {
    if (entry_name == pending_entry->GetEntryName()) {
      LogCancelAndReorder(entry_name);
      Cancel(entry_name);
      ExecuteLimitEntry(entry_limit);
      return;
    }
  }

  // 없으면 바로 주문
  ExecuteLimitEntry(entry_limit);
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
  const int symbol_idx = bar_->GetCurrentSymbolIndex();

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
      LogCancelAndReorder(entry_name);
      Cancel(entry_name);
      ExecuteMitEntry(entry_mit);
      return;
    }
  }

  // 없으면 바로 주문
  ExecuteMitEntry(entry_mit);
}

void OrderHandler::LitEntry(const string& entry_name,
                            const Direction entry_direction,
                            const double entry_size,
                            const unsigned char leverage,
                            const double touch_price,
                            const double order_price) {
  // 변수 계산
  const int symbol_idx = bar_->GetCurrentSymbolIndex();

  // 예외 체크
  try {
    IsValidPositionSize(entry_size);
    IsValidLeverage(leverage);
    IsValidPrice(touch_price);
    IsValidPrice(order_price);
    IsValidLimitOrderPrice(order_price, touch_price, entry_direction);
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
      LogCancelAndReorder(entry_name);
      Cancel(entry_name);
      ExecuteLitEntry(entry_lit);
      return;
    }
  }

  // 없으면 바로 주문
  ExecuteLitEntry(entry_lit);
}

void OrderHandler::TrailingEntry(const string& entry_name,
                                 const Direction entry_direction,
                                 const double entry_size,
                                 const unsigned char leverage,
                                 const double touch_price,
                                 const double trail_point) {
  // 변수 계산
  const int symbol_idx = bar_->GetCurrentSymbolIndex();
  const auto bar_idx = bar_->GetCurrentBarIndex();
  double next_open;
  try {
    next_open =
        bar_->GetBarData(BarType::TRADING).GetOpen(symbol_idx, bar_idx + 1);
  } catch (...) {
    logger_->Log(LogLevel::WARNING_L,
                 "마지막 바이기 때문에 진입할 수 없습니다.", __FILE__,
                 __LINE__);
    return;
  }

  // 예외 체크
  try {
    IsValidPositionSize(entry_size);
    IsValidLeverage(leverage);
    IsValidTrailingTouchPrice(touch_price);
    IsValidTrailPoint(trail_point);
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
  if (touch_price == 0) {
    entry_trailing->SetEntryExtremePrice(next_open);
  }

  // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 후 주문
  for (const auto& pending_entry : pending_entries_[symbol_idx]) {
    if (entry_name == pending_entry->GetEntryName()) {
      LogCancelAndReorder(entry_name);
      Cancel(entry_name);
      ExecuteTrailingEntry(entry_trailing);
      return;
    }
  }

  // 없으면 바로 주문
  ExecuteTrailingEntry(entry_trailing);
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
  for (const auto& pending_exit :
       pending_exits_[bar_->GetCurrentSymbolIndex()]) {
    if (exit_name == pending_exit->GetExitName()) {
      LogCancelAndReorder(exit_name);
      Cancel(exit_name);
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
  for (const auto& pending_exit :
       pending_exits_[bar_->GetCurrentSymbolIndex()]) {
    if (exit_name == pending_exit->GetExitName()) {
      LogCancelAndReorder(exit_name);
      Cancel(exit_name);
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
  for (const auto& pending_exit :
       pending_exits_[bar_->GetCurrentSymbolIndex()]) {
    if (exit_name == pending_exit->GetExitName()) {
      LogCancelAndReorder(exit_name);
      Cancel(exit_name);
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
  for (const auto& pending_exit :
       pending_exits_[bar_->GetCurrentSymbolIndex()]) {
    if (exit_name == pending_exit->GetExitName()) {
      LogCancelAndReorder(exit_name);
      Cancel(exit_name);
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
  // 예외 체크
  try {
    IsValidPositionSize(exit_size);
    IsValidTrailingTouchPrice(touch_price);
    IsValidTrailPoint(trail_point);
  } catch (...) {
    return;
  }

  // 같은 이름의 대기 중인 청산 주문이 있으면 취소 후 재주문
  for (const auto& pending_exit :
       pending_exits_[bar_->GetCurrentSymbolIndex()]) {
    if (exit_name == pending_exit->GetExitName()) {
      LogCancelAndReorder(exit_name);
      Cancel(exit_name);
      ExecuteTrailingExit(exit_name, target_entry, exit_size, touch_price,
                          trail_point);
      return;
    }
  }

  // 없으면 바로 주문
  ExecuteTrailingExit(exit_name, target_entry, exit_size, touch_price,
                      trail_point);
}

void OrderHandler::Cancel(const string& order_name) {
  const int symbol_idx = bar_->GetCurrentSymbolIndex();

  for (int i = 0; i < pending_entries_[symbol_idx].size(); i++) {
    if (order_name == pending_entries_[symbol_idx][i]->GetEntryName()) {
      // 예약 증거금 회복 과정 진행 후 삭제
      ExecuteCancelEntry(pending_entries_[symbol_idx][i]);
      pending_entries_[symbol_idx].erase(pending_entries_[symbol_idx].begin() +
                                         i);
      break;  // 동일한 진입 이름으로 진입 대기 불가능하므로 찾으면 바로 break
    }
  }

  for (int i = 0; i < pending_exits_[symbol_idx].size(); i++) {
    if (order_name == pending_exits_[symbol_idx][i]->GetExitName()) {
      pending_exits_[symbol_idx].erase(pending_exits_[symbol_idx].begin() + i);
      break;  // 청산 주문은 예약 증거금이 필요하지 않기 때문에 삭제만 함
    }
  }
}

void OrderHandler::ExecuteMarketEntry(const shared_ptr<Order>& order) {
  // 변수 계산
  const int symbol_idx = bar_->GetCurrentSymbolIndex();
  const double filled_price = order->GetEntryFilledPrice();
  const double commission = order->GetEntryCommission();

  // 진입 증거금 계산: 1포인트 == 1달러 가정 계산
  const double entry_margin = order->GetEntryFilledSize() * filled_price;

  // 진입 가능 여부 체크 (진입 가능 자금 > 진입 증거금 + 수수료)
  if (!HasEnoughBalance(engine_->GetAvailableBalance(),
                        entry_margin + commission)) {
    return;
  }

  // 현재 자금(주문 가능 자금)에서 수수료 감소
  engine_->DecreaseWalletBalance(commission);

  // 주문 가능 자금에서 진입 증거금 감소
  engine_->DecreaseAvailableBalance(entry_margin);

  // 시장가 진입
  filled_entries_[symbol_idx].push_back(order);

  // 디버그 로그 기록
  FormattedDebugLog(
      format("{} 시장가 {} 진입 체결", filled_price,
             order->GetEntryDirection() == Direction::LONG ? "매수" : "매도"));
}

void OrderHandler::ExecuteLimitEntry(const shared_ptr<Order>& order) {
  // 변수 계산
  const double order_size = order->GetEntryOrderSize();
  const double order_price = order->GetEntryOrderPrice();

  // 예약 증거금 계산: 1포인트 == 1달러 가정 계산
  const double entry_margin = order_size * order_price;

  // 진입 가능 여부 체크
  if (!HasEnoughBalance(engine_->GetAvailableBalance(), entry_margin)) {
    return;
  }

  // 주문 가능 자금에서 예약 증거금 감소
  engine_->DecreaseAvailableBalance(entry_margin);

  // 지정가 진입 대기
  pending_entries_[bar_->GetCurrentSymbolIndex()].push_back(order);

  // 디버그 로그 기록
  FormattedDebugLog(
      format("{} 지정가 {} 진입 대기", order_price,
             order->GetEntryDirection() == Direction::LONG ? "매수" : "매도"));
}

void OrderHandler::ExecuteMitEntry(const shared_ptr<Order>& order) {
  // MIT 진입 터치 대기
  pending_entries_[bar_->GetCurrentSymbolIndex()].push_back(order);

  // 디버그 로그 기록
  FormattedDebugLog(
      format("{} MIT {} 진입 터치 대기", order->GetEntryTouchPrice(),
             order->GetEntryDirection() == Direction::LONG ? "매수" : "매도"));
}

void OrderHandler::ExecuteLitEntry(const shared_ptr<Order>& order) {
  // LIT 진입 터치 대기
  pending_entries_[bar_->GetCurrentSymbolIndex()].push_back(order);

  // 디버그 로그 기록
  FormattedDebugLog(
      format("{} LIT {} 진입 터치 대기", order->GetEntryTouchPrice(),
             order->GetEntryDirection() == Direction::LONG ? "매수" : "매도"));
}

void OrderHandler::ExecuteTrailingEntry(const shared_ptr<Order>& order) {
  // Trailing 진입 터치 대기
  pending_entries_[bar_->GetCurrentSymbolIndex()].push_back(order);

  // 디버그 로그 기록
  if (isnan(order->GetEntryExtremePrice())) {
    FormattedDebugLog(format(
        "{} 트레일링 {} 진입 터치 대기", order->GetEntryTouchPrice(),
        order->GetEntryDirection() == Direction::LONG ? "매수" : "매도"));
  } else {
    // touch_price가 0이라 바로 추적 시작한 경우
    FormattedDebugLog(format(
        "트레일링 {} 진입 대기",
        order->GetEntryDirection() == Direction::LONG ? "매수" : "매도"));
  }
}

void OrderHandler::ExecuteMarketExit(const string& exit_name,
                                     const string& target_entry,
                                     const double exit_size) {
  // 변수 계산
  const auto symbol_idx = bar_->GetCurrentSymbolIndex();
  const auto bar_idx = bar_->GetCurrentBarIndex();
  const auto& bar = bar_->GetBarData(bar_->GetCurrentBarType());

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
        next_open_time = bar.GetOpenTime(symbol_idx, bar_idx + 1);
        next_open = bar.GetOpen(symbol_idx, bar_idx + 1);
      } catch (...) {
        logger_->Log(LogLevel::WARNING_L,
                     "마지막 바이기 때문에 청산할 수 없습니다.", __FILE__,
                     __LINE__);
        return;
      }

      // 총 청산 체결 수량이 진입 체결 수량보다 크지 않게 조정
      const double exit_filled_size =
          GetAdjustedExitFilledSize(exit_size, exit_market);

      // 청산 주문 생성
      exit_market->SetExitName(exit_name)
          .SetExitOrderType(OrderType::MARKET)
          .SetExitDirection(exit_market->GetEntryDirection() == Direction::LONG
                                ? Direction::SHORT
                                : Direction::LONG)
          .SetExitOrderTime(next_open_time)
          .SetExitOrderSize(exit_filled_size)
          .SetExitOrderPrice(next_open)
          .SetExitFilledTime(next_open_time)
          .SetExitFilledSize(exit_filled_size);

      // 슬리피지가 포함된 체결가
      const double filled_price =
          CalculateSlippagePrice(next_open, OrderType::MARKET,
                                 exit_market->GetExitDirection(), exit_market);
      exit_market->SetExitFilledPrice(filled_price);

      // 수수료
      const double commission = CalculateCommission(
          filled_price, OrderType::MARKET, exit_filled_size, exit_market);
      exit_market->SetExitCommission(commission);

      // 원본 진입 객체에 청산 체결 수량 추가
      filled_entry->SetExitFilledSize(filled_entry->GetExitFilledSize() +
                                      exit_filled_size);

      /* 원본 진입 객체의 청산 체결 수량이 진입 체결 수량과 같으면
         (부동 소수점 오류 방지용 '>=')                        */
      if (filled_entry->GetExitFilledSize() >=
          filled_entry->GetEntryFilledSize()) {
        // filled_entries에서 삭제
        filled_entries_[symbol_idx].erase(filled_entries_[symbol_idx].begin() +
                                          i);

        // 같은 진입 이름을 목표로 하는 청산 대기 주문 취소
        erase_if(pending_exits_[symbol_idx],
                 [&](const shared_ptr<Order>& pending_exit) {
                   return target_entry == pending_exit->GetEntryName();
                 });
      }

      // 자금, 통계 업데이트
      ExecuteExit(exit_market);

      // 디버그 로그 기록
      FormattedDebugLog(
          format("{} 시장가 {} 청산 체결", filled_price,
                 exit_market->GetEntryDirection() == Direction::LONG ? "매수"
                                                                     : "매도"));
      return;
    }
  }

  // 목표한 진입 이름을 찾지 못하면 경고 로그
  InvalidEntryName(target_entry);
}

void OrderHandler::ExecuteLimitExit(const string& exit_name,
                                    const string& target_entry,
                                    const double exit_size,
                                    const double order_price) {
  // 변수 계산
  const auto symbol_idx = bar_->GetCurrentSymbolIndex();
  const auto bar_idx = bar_->GetCurrentBarIndex();
  const auto& bar = bar_->GetBarData(bar_->GetCurrentBarType());

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
        next_open_time = bar.GetOpenTime(symbol_idx, bar_idx + 1);
        next_open = bar.GetOpen(symbol_idx, bar_idx + 1);
      } catch (...) {
        logger_->Log(LogLevel::WARNING_L,
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
          .SetExitOrderSize(exit_size)
          .SetExitOrderPrice(order_price);

      // 유효성 검사
      try {
        IsValidLimitOrderPrice(order_price, next_open,
                               exit_limit->GetExitDirection());
      } catch (...) {
        return;
      }

      // 대기 중인 청산에 추가
      pending_exits_[symbol_idx].push_back(exit_limit);
      return;
    }
  }

  // 목표한 진입 이름을 찾지 못하면 경고 로그
  InvalidEntryName(target_entry);
}

void OrderHandler::ExecuteMitExit(const string& exit_name,
                                  const string& target_entry,
                                  const double exit_size,
                                  const double touch_price) {
  // 변수 계산
  const auto symbol_idx = bar_->GetCurrentSymbolIndex();

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
          .SetExitOrderSize(exit_size);

      // 대기 중인 청산에 추가
      pending_exits_[symbol_idx].push_back(exit_mit);
      return;
    }
  }

  // 목표한 진입 이름을 찾지 못하면 경고 로그
  InvalidEntryName(target_entry);
}

void OrderHandler::ExecuteLitExit(const string& exit_name,
                                  const string& target_entry,
                                  const double exit_size,
                                  const double touch_price,
                                  const double order_price) {
  // 변수 계산
  const auto symbol_idx = bar_->GetCurrentSymbolIndex();

  // 체결된 진입들 순회
  for (int i = 0; i < filled_entries_[symbol_idx].size(); i++) {
    // target_entry와 같은 이름의 진입이 있으면 청산
    if (const auto& filled_entry = filled_entries_[symbol_idx][i];
        filled_entry->GetEntryName() == target_entry) {
      // Entry Order 복사
      const auto& exit_lit = make_shared<Order>(*filled_entry);

      // 청산 주문 생성
      exit_lit->SetExitName(exit_name)
          .SetExitOrderType(OrderType::LIT)
          .SetExitDirection(exit_lit->GetEntryDirection() == Direction::LONG
                                ? Direction::SHORT
                                : Direction::LONG)
          .SetExitTouchPrice(touch_price)
          .SetExitOrderSize(exit_size)
          .SetExitOrderPrice(order_price);

      // 유효성 검사
      try {
        IsValidLimitOrderPrice(order_price, touch_price,
                               exit_lit->GetExitDirection());
      } catch (...) {
        return;
      }

      // 대기 중인 청산에 추가
      pending_exits_[symbol_idx].push_back(exit_lit);
      return;
    }
  }

  // 목표한 진입 이름을 찾지 못하면 경고 로그
  InvalidEntryName(target_entry);
}

void OrderHandler::ExecuteTrailingExit(const string& exit_name,
                                       const string& target_entry,
                                       const double exit_size,
                                       const double touch_price,
                                       const double trail_point) {
  // 변수 계산
  const auto symbol_idx = bar_->GetCurrentSymbolIndex();
  const auto bar_idx = bar_->GetCurrentBarIndex();

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
        next_open =
            bar_->GetBarData(BarType::TRADING).GetOpen(symbol_idx, bar_idx + 1);
      } catch (...) {
        logger_->Log(LogLevel::WARNING_L,
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
          .SetExitOrderSize(exit_size);

      // touch_price가 0이면 다음 시가부터 최고저가 추적을 시작
      if (touch_price == 0) {
        exit_trailing->SetEntryExtremePrice(next_open);
      }

      // 대기 중인 청산에 추가
      pending_exits_[symbol_idx].push_back(exit_trailing);
      return;
    }
  }

  // 목표한 진입 이름을 찾지 못하면 경고 로그
  InvalidEntryName(target_entry);
}

void OrderHandler::ExecuteExit(const shared_ptr<Order>& exit_order) {
  const double exit_filled_size = exit_order->GetExitFilledSize();

  // 미실현 손익 업데이트
  engine_->UpdateUnrealizedPnl();

  // 현재 자금(주문 가능 자금)에서 수수료 감소
  engine_->DecreaseWalletBalance(exit_order->GetExitCommission());

  // 진입 증거금 회복
  engine_->IncreaseAvailableBalance(exit_filled_size *
                                   exit_order->GetEntryFilledPrice());

  // 손익 계산
  double pnl;
  if (exit_order->GetEntryDirection() == Direction::LONG) {
    pnl = (exit_order->GetExitFilledPrice() - exit_order->GetEntryFilledPrice()) *
          exit_filled_size * exit_order->GetLeverage();
  } else {
    pnl = (exit_order->GetEntryFilledPrice() - exit_order->GetExitFilledPrice()) *
          exit_filled_size * exit_order->GetLeverage();
  }

  // 현재 자금(주문 가능 자금)에 손익 합산
  if (pnl > 0) {
    engine_->IncreaseWalletBalance(pnl);
  } else if (pnl < 0) {
    engine_->DecreaseWalletBalance(abs(pnl));
  }

  // 엔진 업데이트 항목들 업데이트 @@@@@@@@@@@@ mdd 같은거: 함수로 만들자요

  filled_exits_.push_back(exit_order);  // 체결된 청산에 추가
  // 통계 추가
  // filled_exit 단일 벡터로 만들고 심볼정보 등 통계를 위해 필요한 항목들 모두
  // 추가후 마지막에 일괄 계산하여 트레이딩 리스트 작성하자 -> 오버헤드 감소용
  // @@@@@@@@@@@@@@@@@@@@@@ 추후 추가하자
}

void OrderHandler::ExecuteCancelEntry(const shared_ptr<Order>& cancel_order) {
  switch (cancel_order->GetEntryOrderType()) {
    case OrderType::MARKET: {
      // 시장가는 바로 체결하므로 대기 주문이 없음
      return;
    }

    case OrderType::LIMIT: {
      // 주문 가능 자금에서 예약 증거금 회복
      engine_->IncreaseAvailableBalance(
        cancel_order->GetEntryOrderSize() * cancel_order->GetEntryOrderPrice());
      return;
    }

    case OrderType::MIT: {
      /* MIT Touch 대기 중에는 예약 증거금을 사용하지 않으며,
         Touch 이후에는 시장가로 체결하므로 대기 주문이 없음  */
      return;
    }

    case OrderType::LIT: {
      if (cancel_order->GetEntryOrderTime() != -1) {
        /* Entry Order Time이 설정되었다는 것은 Touch 했다는 의미이며,
           Touch 이후에는 지정가로 예약 증거금을 사용하므로 회복해야 함 */
        engine_->IncreaseAvailableBalance(
          cancel_order->GetEntryOrderSize() * cancel_order->GetEntryOrderPrice());
      }
      return;
    }

    case OrderType::TRAILING: {
      /* Trailing Touch 대기 중에는 예약 증거금을 사용하지 않으며,
         Touch 이후에는 가격을 추적하다 시장가로 체결하므로 대기 주문이 없음 */
      return;
    }

    default:
      return;
  }
}

double OrderHandler::GetAdjustedExitFilledSize(
    const double exit_size, const shared_ptr<Order>& exit_order) {
  // 청산 수량 + 분할 청산한 수량이 진입 수량보다 많다면
  if (const auto entry_size = exit_order->GetEntryFilledSize();
      exit_size + exit_order->GetExitFilledSize() > entry_size) {
    // 경고 로그 기록
    FormattedWarningLog(format("합계 청산 크기 {}이(가) 진입 크기 {}을(를) 초과했습니다.",
      exit_size + exit_order->GetExitFilledSize(), entry_size));

    // 최대값으로 조정하여 반환
    return entry_size - exit_order->GetExitFilledSize();
  }

  return exit_size;
}

bool OrderHandler::HasEnoughBalance(const double available_balance,
                                    const double needed_balance) {
  if (available_balance < needed_balance) {
    // 경고 로그 기록
    FormattedWarningLog(format("주문 가능 자금 ${}는 필요 자금 ${}보다 많아야합니다.",
      available_balance, needed_balance));

    return false;
  }

  return true;
}
