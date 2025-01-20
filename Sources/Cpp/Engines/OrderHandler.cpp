// 표준 라이브러리
#include <format>

// 파일 헤더
#include "Engines/OrderHandler.hpp"

// 내부 헤더
#include "Engines/BarData.hpp"
#include "Engines/BarHandler.hpp"
#include "Engines/Engine.hpp"
#include "Engines/Exception.hpp"
#include "Engines/TimeUtils.hpp"

// 네임 스페이스
using namespace time_utils;

OrderHandler::OrderHandler() = default;
void OrderHandler::Deleter::operator()(const OrderHandler* p) const {
  delete p;
}

mutex OrderHandler::mutex_;
unordered_map<string, shared_ptr<OrderHandler>> OrderHandler::instances_;

shared_ptr<OrderHandler>& OrderHandler::GetOrderHandler(const string& name) {
  lock_guard lock(mutex_);  // 스레드에서 안전하게 접근하기 위해 mutex 사용

  // 해당 이름으로 인스턴스가 존재하는지 확인
  if (const auto& it = instances_.find(name); it == instances_.end()) {
    // 인스턴스가 없으면 생성 후 저장
    instances_[name] = shared_ptr<OrderHandler>(new OrderHandler(), Deleter());
  }

  return instances_[name];
}

void OrderHandler::CheckPendingEntryOrders(const double price,
                                           const int64_t open_time,
                                           const bool is_open_price) {
  // 가격 유효성 검사
  try {
    IsValidPrice(price);
  } catch (const exception& e) {
    LogFormattedInfo(LogLevel::WARNING_L,
                     string(e.what()) +
                         ": 현재가가 유효하지 않으므로 대기 진입 주문의 체결을 "
                         "확인할 수 없습니다.",
                     __FILE__, __LINE__);
  }

  // 현재 심볼의 대기 주문 로딩
  const auto& current_symbol_pending_entries =
      pending_entries_[bar_->GetCurrentSymbolIndex()];

  /* 진입 대기 주문 확인 -> 체결 혹은 주문 거부 시 해당 주문이
                          대기 주문에서 삭제되므로 역순으로 순회 */
  for (int i = static_cast<int>(current_symbol_pending_entries.size() - 1);
       i >= 0; i--) {
    const auto& pending_entry = current_symbol_pending_entries[i];

    // 마진콜 체크
    // @@@@@@@@@@@@@@@@ 추후 마진콜 메커니즘 변경 시 변경

    // 진입 체결 체크
    switch (pending_entry->GetEntryOrderType()) {
      case OrderType::MARKET: {
        // 시장가는 대기 주문이 없음
        continue;
      }

      case OrderType::LIMIT: {
        // 주문 정보 로딩
        const auto entry_direction = pending_entry->GetEntryDirection();
        const auto entry_order_price = pending_entry->GetEntryOrderPrice();

        // 시가에서 진입 조건 달성 시 체결 가격은 시가가 됨 (갭 고려)
        double entry_filled_price;
        if (is_open_price) {
          entry_filled_price = price;
        } else {
          entry_filled_price = entry_order_price;
        }

        // 매수 진입의 경우, 가격이 주문 가격과 같거나 낮아지면 체결
        // 매도 진입의 경우, 가격이 주문 가격과 같거나 높아지면 체결
        if ((entry_direction == Direction::LONG &&
             price <= entry_order_price) ||
            (entry_direction == Direction::SHORT &&
             price >= entry_order_price)) {
          ExecutePendingLimitEntry(i, open_time, entry_filled_price);
          continue;
        }
      }

      case OrderType::MIT: {
        // 주문 정보 로딩
        const auto entry_touch_price = pending_entry->GetEntryTouchPrice();
        const auto entry_touch_direction =
            pending_entry->GetEntryTouchDirection();

        // 시가에서 터치 조건 달성 시 주문 가격은 시가가 됨 (갭 고려)
        double entry_order_price;
        if (is_open_price) {
          entry_order_price = price;
        } else {
          entry_order_price = entry_touch_price;
        }

        // 터치 방향이 매수인 경우, 터치 가격과 같거나 커지면 시장가 주문
        // 터치 방향이 매도인 경우, 터치 가격과 같거나 작아지면 시장가 주문
        if ((entry_touch_direction == Direction::LONG &&
             price >= entry_touch_price) ||
            (entry_touch_direction == Direction::SHORT &&
             price <= entry_touch_price)) {
          ExecutePendingMitEntry(i, open_time, entry_order_price);
          continue;
        }
      }

      case OrderType::LIT: {
        // Order Time이 설정되지 않았으면 지정가 미주문 -> 터치 확인
        if (pending_entry->GetEntryOrderTime() == -1) {
          // 주문 정보 로딩
          const auto entry_touch_price = pending_entry->GetEntryTouchPrice();
          const auto entry_touch_direction =
              pending_entry->GetEntryTouchDirection();

          // LIT에서 터치 가격은 트리거 역할로, 갭과 무관하게 조건만 확인

          // 터치 방향이 매수인 경우, 터치 가격과 같거나 커지면 지정가 주문
          // 터치 방향이 매도인 경우, 터치 가격과 같거나 작아지면 지정가 주문
          if ((entry_touch_direction == Direction::LONG &&
               price >= entry_touch_price) ||
              (entry_touch_direction == Direction::SHORT &&
               price <= entry_touch_price)) {
            if (!OrderPendingLitEntry(i, open_time)) {
              // 주문 실패 시 다음 주문 확인으로 넘어감
              continue;
            }
          }
        }

        // Order Time이 설정됐으면 지정가 주문 완료 -> 체결 확인
        // 터치 후 바로 진입될 수도 있으므로 Order Time을 다시 확인
        if (pending_entry->GetEntryOrderTime() != -1) {
          // 주문 정보 로딩
          const auto entry_direction = pending_entry->GetEntryDirection();
          const auto entry_order_price = pending_entry->GetEntryOrderPrice();

          // 시가에서 터치 조건 달성 시 주문 가격은 시가가 됨 (갭 고려)
          double entry_filled_price;
          if (is_open_price) {
            entry_filled_price = price;
          } else {
            entry_filled_price = entry_order_price;
          }

          // 매수 진입의 경우, 가격이 주문 가격과 같거나 낮아지면 체결
          // 매도 진입의 경우, 가격이 주문 가격과 같거나 높아지면 체결
          if ((entry_direction == Direction::LONG &&
               price <= entry_order_price) ||
              (entry_direction == Direction::SHORT &&
               price >= entry_order_price)) {
            ExecutePendingLimitEntry(i, open_time, entry_filled_price);
            continue;
          }
        }
      }

      case OrderType::TRAILING: {
        // Extreme Price가 지정되지 않았으면 추적 미시작 -> 터치 확인
        if (isnan(pending_entry->GetEntryExtremePrice())) {
          // 주문 정보 로딩
          const auto entry_touch_price = pending_entry->GetEntryTouchPrice();
          const auto entry_touch_direction =
              pending_entry->GetEntryTouchDirection();

          // 터치 방향이 매수 방향인 경우, 터치 가격과 같거나 커지면 추적 시작
          // 터치 방향이 매도 방향인 경우, 터치 가격과 같거나 작아지면 추적 시작
          if ((entry_touch_direction == Direction::LONG &&
               price >= entry_touch_price) ||
              (entry_touch_direction == Direction::SHORT &&
               price <= entry_touch_price)) {
            pending_entry->SetEntryExtremePrice(price);
          }
        }

        // Extreme Price가 지정되었으면 추적 시작
        // -> 고저가 업데이트 및 체결 터치 확인
        if (double entry_extreme_price = pending_entry->GetEntryExtremePrice();
            !isnan(entry_extreme_price)) {
          // 주문 정보 로딩
          const auto entry_direction = pending_entry->GetEntryDirection();
          const auto entry_trail_point = pending_entry->GetEntryTrailPoint();

          // 진입 방향이 매수인 경우, 최저가를 추적
          // 진입 방향이 매도인 경우, 최고가를 추적
          if ((entry_direction == Direction::LONG &&
               price < entry_extreme_price) ||
              (entry_direction == Direction::SHORT &&
               price > entry_extreme_price)) {
            pending_entry->SetEntryExtremePrice(price);
          }

          // Extreme Price가 업데이트될 수 있으므로 재로딩
          entry_extreme_price = pending_entry->GetEntryExtremePrice();
          if ((entry_direction == Direction::LONG &&
               price >= entry_extreme_price + entry_trail_point) ||
              (entry_direction == Direction::SHORT &&
               price <= entry_extreme_price - entry_trail_point)) {
            // Market 가능? MIT 재활용하기
          }
        }
      }

      case OrderType::NONE: {
        // 에러 방지
        continue;
      }
    }
  }
}

void OrderHandler::CheckPendingExitOrders(const double price,
                                          const int64_t open_time,
                                          const bool is_open_price) {
  // 가격 유효성 검사
  try {
    IsValidPrice(price);
  } catch (const exception& e) {
    LogFormattedInfo(LogLevel::WARNING_L,
                     string(e.what()) +
                         ": 현재가가 유효하지 않으므로 대기 청산 주문의 체결을 "
                         "확인할 수 없습니다.",
                     __FILE__, __LINE__);
  }

  // 현재 심볼의 대기 주문 로딩
  const auto& current_symbol_pending_exits =
      pending_exits_[bar_->GetCurrentSymbolIndex()];

  // @@@@@@@@@@@@@@@@@ Execute들 만들 때 체결 거부당하면 pending에서 제거
  // @@@@@@@@@@@@@@@@@ 체결되면 pending에서 제거 후 filled에 추가, 같은 진입
  // 목표 제거 등 시장가 참조

  /* 청산 대기 주문 확인 -> 체결 시 대기 주문에서 삭제되므로 역순으로 순회
     체결 내부 함수에서 대기 주문을 삭제한 개수만큼 인덱스를 감소
      -> 해당 주문 및 같은 진입 이름을 목표로 한 대기 주문이 삭제될 수 있음 */
  for (int i = static_cast<int>(current_symbol_pending_exits.size() - 1);
       i >= 0;) {
    int deleted_count = 0;
    const auto& pending_exit = current_symbol_pending_exits[i];

    // 마진콜 체크
    // @@@@@@@@@@@@@@@@ 추후 마진콜 메커니즘 변경 시 변경
    // 마진콜이면 바로 continue해야할텐데 deleted_count 관리하고 하기

    // 청산 체결 체크
    switch (pending_exit->GetExitOrderType()) {
      case OrderType::MARKET: {
        // 시장가는 대기 주문이 없음
        continue;
      }

      case OrderType::LIMIT: {
        deleted_count = ExecutePendingLimitExit();
      }

      case OrderType::MIT: {
      }

      case OrderType::LIT: {
      }

      case OrderType::TRAILING: {
      }

      case OrderType::NONE: {
        // 에러 방지
        // continue;
      }
    }

    if (deleted_count > 0) {
      i -= deleted_count;  // 삭제된 개수만큼 인덱스를 감소시킴
    } else {
      i--;  // 삭제가 없으면 정상적으로 한 단계 감소
    }
  }
}

void OrderHandler::MarketEntry(const string& entry_name,
                               const Direction entry_direction,
                               const double entry_size,
                               const unsigned char leverage) {
  // 예외 체크
  try {
    IsValidEntryName(entry_name);
    IsValidPositionSize(entry_size);
    IsValidLeverage(leverage);
  } catch (const exception& e) {
    LogFormattedInfo(LogLevel::WARNING_L, e.what(), __FILE__, __LINE__);
    return;
  }

  // 변수 계산
  const int symbol_idx = bar_->GetCurrentSymbolIndex();
  const size_t bar_idx = bar_->GetCurrentBarIndex();
  const auto& bar = bar_->GetBarData(bar_->GetCurrentBarType(),
                                     bar_->GetCurrentReferenceTimeframe());

  int64_t next_open_time;  // 진입 시간: 다음 바의 시작 시간
  double next_open;        // 주문 가격: 다음 바의 시가
  try {
    next_open_time = bar.GetOpenTime(symbol_idx, bar_idx + 1);
    next_open = bar.GetOpen(symbol_idx, bar_idx + 1);
  } catch (const exception& e) {
    LogFormattedInfo(
        LogLevel::WARNING_L,
        string(e.what()) + ": 마지막 바이기 때문에 진입할 수 없습니다.",
        __FILE__, __LINE__);
    return;
  }

  // 주문 생성
  const auto& market_entry = make_shared<Order>();
  market_entry->SetLeverage(leverage)
      .SetEntryName(entry_name)
      .SetEntryOrderType(OrderType::MARKET)
      .SetEntryDirection(entry_direction)
      .SetEntryOrderTime(next_open_time)
      .SetEntryOrderSize(entry_size)
      .SetEntryOrderPrice(next_open)
      .SetEntryFilledTime(next_open_time)
      .SetEntryFilledSize(entry_size);

  // 슬리피지가 포함된 체결가
  const double slippage_filled_price = CalculateSlippagePrice(
      next_open, OrderType::MARKET, entry_direction, leverage);
  market_entry->SetEntryFilledPrice(slippage_filled_price);

  // 수수료
  const double entry_commission = CalculateCommission(
      slippage_filled_price, OrderType::MARKET, entry_size, leverage);
  market_entry->SetEntryCommission(entry_commission);

  // 마진콜 가격
  market_entry->SetMarginCallPrice(CalculateMarginCallPrice(
      slippage_filled_price, entry_direction, leverage));

  // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 (주문 수정)
  Cancel(entry_name);

  // 해당 주문과 반대 방향의 체결 주문이 있으면 모두 청산
  ExitOppositeFilledEntries(entry_direction);

  // 진입 증거금 계산: 1포인트 == 1달러 가정 계산
  const double entry_margin = entry_size * slippage_filled_price;

  // 미실현 손익 업데이트
  engine_->UpdateUnrealizedPnl();

  // 진입 가능 여부 체크 (진입 가능 자금 > 진입 증거금 + 진입 수수료)
  if (!HasEnoughBalance(engine_->GetAvailableBalance(),
                        entry_margin + entry_commission)) {
    return;
  }

  // 현재 자금(주문 가능 자금)에서 수수료 감소
  engine_->DecreaseWalletBalance(entry_commission);

  // 주문 가능 자금에서 진입 증거금 감소
  engine_->DecreaseAvailableBalance(entry_margin);

  // 시장가 진입
  filled_entries_[symbol_idx].push_back(market_entry);

  LogFormattedInfo(
      LogLevel::DEBUG_L,
      format("{} | 시장가 {} 체결", slippage_filled_price, entry_name),
      __FILE__, __LINE__);
}

void OrderHandler::LimitEntry(const string& entry_name,
                              const Direction entry_direction,
                              const double entry_size,
                              const unsigned char leverage,
                              const double order_price) {
  // 변수 계산
  const int symbol_idx = bar_->GetCurrentSymbolIndex();
  const size_t bar_idx = bar_->GetCurrentBarIndex();
  const auto& bar = bar_->GetBarData(bar_->GetCurrentBarType(),
                                     bar_->GetCurrentReferenceTimeframe());

  int64_t next_open_time;
  double next_open;
  try {
    next_open_time = bar.GetOpenTime(symbol_idx, bar_idx + 1);
    next_open = bar.GetOpen(symbol_idx, bar_idx + 1);
  } catch (const exception& e) {
    LogFormattedInfo(
        LogLevel::WARNING_L,
        string(e.what()) + ": 마지막 바이기 때문에 진입할 수 없습니다.",
        __FILE__, __LINE__);
    return;
  }

  // 예외 체크
  try {
    IsValidPositionSize(entry_size);
    IsValidLeverage(leverage);
    IsValidPrice(order_price);
    IsValidLimitOrderPrice(order_price, next_open, entry_direction);
  } catch (const exception& e) {
    LogFormattedInfo(LogLevel::WARNING_L, e.what(), __FILE__, __LINE__);
    return;
  }

  // 주문 생성
  const auto& limit_entry = make_shared<Order>();
  limit_entry->SetLeverage(leverage)
      .SetEntryName(entry_name)
      .SetEntryOrderType(OrderType::LIMIT)
      .SetEntryDirection(entry_direction)
      .SetEntryOrderTime(next_open_time)
      .SetEntryOrderSize(entry_size)
      .SetEntryOrderPrice(order_price);

  // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 (주문 수정)
  Cancel(entry_name);

  // 예약 증거금 계산: 1포인트 == 1달러 가정 계산
  const double entry_margin = entry_size * order_price;

  // 미실현 손익 업데이트
  engine_->UpdateUnrealizedPnl();

  // 주문 가능 여부 체크
  if (!HasEnoughBalance(engine_->GetAvailableBalance(), entry_margin)) {
    return;
  }

  // 주문 가능 자금에서 예약 증거금 감소
  engine_->DecreaseAvailableBalance(entry_margin);

  // 지정가 진입 대기
  pending_entries_[symbol_idx].push_back(limit_entry);

  // 디버그 로그 기록
  LogFormattedInfo(LogLevel::DEBUG_L,
                   format("{} | 지정가 {} 체결 대기", order_price, entry_name),
                   __FILE__, __LINE__);
}

void OrderHandler::MitEntry(const string& entry_name,
                            const Direction entry_direction,
                            const double entry_size,
                            const unsigned char leverage,
                            const double touch_price) {
  // 예외 체크
  try {
    IsValidPositionSize(entry_size);
    IsValidLeverage(leverage);
    IsValidPrice(touch_price);
  } catch (const exception& e) {
    LogFormattedInfo(LogLevel::WARNING_L, e.what(), __FILE__, __LINE__);
    return;
  }

  // 변수 계산
  const int symbol_idx = bar_->GetCurrentSymbolIndex();
  double next_open;
  try {
    next_open = bar_->GetBarData(bar_->GetCurrentBarType(),
                                 bar_->GetCurrentReferenceTimeframe())
                    .GetOpen(symbol_idx, bar_->GetCurrentBarIndex() + 1);
  } catch (const exception& e) {
    LogFormattedInfo(
        LogLevel::WARNING_L,
        string(e.what()) + ": 마지막 바이기 때문에 진입할 수 없습니다.",
        __FILE__, __LINE__);
    return;
  }

  // 주문 생성
  const auto& mit_entry = make_shared<Order>();
  mit_entry->SetLeverage(leverage)
      .SetEntryName(entry_name)
      .SetEntryOrderType(OrderType::MIT)
      .SetEntryDirection(entry_direction)
      .SetEntryTouchPrice(touch_price)
      .SetEntryTouchDirection(next_open >= touch_price ? Direction::LONG
                                                       : Direction::SHORT)
      .SetEntryOrderSize(entry_size);

  // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 (주문 수정)
  Cancel(entry_name);

  // MIT 진입 터치 대기
  pending_entries_[symbol_idx].push_back(mit_entry);

  // 디버그 로그 기록
  LogFormattedInfo(LogLevel::DEBUG_L,
                   format("{} | MIT {} 터치 대기", touch_price, entry_name),
                   __FILE__, __LINE__);
}

void OrderHandler::LitEntry(const string& entry_name,
                            const Direction entry_direction,
                            const double entry_size,
                            const unsigned char leverage,
                            const double touch_price,
                            const double order_price) {
  // 예외 체크
  try {
    IsValidPositionSize(entry_size);
    IsValidLeverage(leverage);
    IsValidPrice(touch_price);
    IsValidPrice(order_price);
    IsValidLimitOrderPrice(order_price, touch_price, entry_direction);
  } catch (const exception& e) {
    LogFormattedInfo(LogLevel::WARNING_L, e.what(), __FILE__, __LINE__);
    return;
  }

  // 변수 계산
  const auto symbol_idx = bar_->GetCurrentSymbolIndex();
  double next_open;
  try {
    next_open = bar_->GetBarData(bar_->GetCurrentBarType(),
                                 bar_->GetCurrentReferenceTimeframe())
                    .GetOpen(symbol_idx, bar_->GetCurrentBarIndex() + 1);
  } catch (const exception& e) {
    LogFormattedInfo(
        LogLevel::WARNING_L,
        string(e.what()) + ": 마지막 바이기 때문에 진입할 수 없습니다.",
        __FILE__, __LINE__);
    return;
  }

  // 주문 생성
  const auto& lit_entry = make_shared<Order>();
  lit_entry->SetLeverage(leverage)
      .SetEntryName(entry_name)
      .SetEntryOrderType(OrderType::LIT)
      .SetEntryDirection(entry_direction)
      .SetEntryTouchPrice(touch_price)
      .SetEntryTouchDirection(next_open >= touch_price ? Direction::LONG
                                                       : Direction::SHORT)
      .SetEntryOrderSize(entry_size)
      .SetEntryOrderPrice(order_price);

  // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 (주문 수정)
  Cancel(entry_name);

  // LIT 진입 터치 대기
  pending_entries_[symbol_idx].push_back(lit_entry);

  // 디버그 로그 기록
  LogFormattedInfo(LogLevel::DEBUG_L,
                   format("{} | LIT {} 터치 대기", touch_price, entry_name),
                   __FILE__, __LINE__);
}

void OrderHandler::TrailingEntry(const string& entry_name,
                                 const Direction entry_direction,
                                 const double entry_size,
                                 const unsigned char leverage,
                                 const double touch_price,
                                 const double trail_point) {
  // 예외 체크
  try {
    IsValidPositionSize(entry_size);
    IsValidLeverage(leverage);
    IsValidTrailingTouchPrice(touch_price);
    IsValidTrailPoint(trail_point);
  } catch (const exception& e) {
    LogFormattedInfo(LogLevel::WARNING_L, e.what(), __FILE__, __LINE__);
    return;
  }

  // 변수 계산
  const int symbol_idx = bar_->GetCurrentSymbolIndex();
  double next_open;
  try {
    next_open = bar_->GetBarData(bar_->GetCurrentBarType(),
                                 bar_->GetCurrentReferenceTimeframe())
                    .GetOpen(symbol_idx, bar_->GetCurrentBarIndex() + 1);
  } catch (const exception& e) {
    LogFormattedInfo(
        LogLevel::WARNING_L,
        string(e.what()) + ": 마지막 바이기 때문에 진입할 수 없습니다.",
        __FILE__, __LINE__);
    return;
  }

  // 주문 생성
  const auto& trailing_entry = make_shared<Order>();
  trailing_entry->SetLeverage(leverage)
      .SetEntryName(entry_name)
      .SetEntryOrderType(OrderType::TRAILING)
      .SetEntryDirection(entry_direction)
      .SetEntryTouchPrice(touch_price)
      .SetEntryTouchDirection(next_open >= touch_price ? Direction::LONG
                                                       : Direction::SHORT)
      .SetEntryTrailPoint(trail_point)
      .SetEntryOrderSize(entry_size);

  // touch_price가 0이면 다음 시가부터 최고저가 추적을 시작
  if (touch_price == 0) {
    trailing_entry->SetEntryExtremePrice(next_open);
  }

  // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 (주문 수정)
  Cancel(entry_name);

  // 트레이딩 진입 터치 대기
  pending_entries_[symbol_idx].push_back(trailing_entry);

  // 디버그 로그 기록
  if (touch_price != 0) {
    LogFormattedInfo(
        LogLevel::DEBUG_L,
        format("{} | 트레일링 {} 터치 대기", touch_price, entry_name), __FILE__,
        __LINE__);
  } else {
    // touch_price가 0이라 바로 추적 시작한 경우
    LogFormattedInfo(LogLevel::DEBUG_L,
                     format("트레일링 {} 체결 대기", entry_name), __FILE__,
                     __LINE__);
  }
}

void OrderHandler::MarketExit(const string& exit_name,
                              const string& target_entry,
                              const double exit_size) {
  // 예외 체크
  try {
    IsValidPositionSize(exit_size);
  } catch (const exception& e) {
    LogFormattedInfo(LogLevel::WARNING_L, e.what(), __FILE__, __LINE__);
    return;
  }

  // 변수 계산
  const auto symbol_idx = bar_->GetCurrentSymbolIndex();
  const auto bar_idx = bar_->GetCurrentBarIndex();
  const auto& bar = bar_->GetBarData(bar_->GetCurrentBarType(),
                                     bar_->GetCurrentReferenceTimeframe());

  int64_t next_open_time;
  double next_open;
  try {
    next_open_time = bar.GetOpenTime(symbol_idx, bar_idx + 1);
    next_open = bar.GetOpen(symbol_idx, bar_idx + 1);
  } catch (const exception& e) {
    LogFormattedInfo(
        LogLevel::WARNING_L,
        string(e.what()) + ": 마지막 바이기 때문에 청산할 수 없습니다.",
        __FILE__, __LINE__);
    return;
  }

  // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 (주문 수정)
  Cancel(exit_name);

  // 체결된 진입들 순회
  for (int i = 0; i < filled_entries_[symbol_idx].size(); i++) {
    // target_entry와 같은 이름의 진입이 있으면 청산
    if (const auto& filled_entry = filled_entries_[symbol_idx][i];
        filled_entry->GetEntryName() == target_entry) {
      // Entry Order 복사
      const auto& market_exit = make_shared<Order>(*filled_entry);

      // 총 청산 체결 수량이 진입 체결 수량보다 크지 않게 조정
      const double exit_filled_size =
          GetAdjustedExitFilledSize(exit_size, market_exit);

      // 청산 주문 생성
      market_exit->SetExitName(exit_name)
          .SetExitOrderType(OrderType::MARKET)
          .SetExitDirection(market_exit->GetEntryDirection() == Direction::LONG
                                ? Direction::SHORT
                                : Direction::LONG)
          .SetExitOrderTime(next_open_time)
          .SetExitOrderSize(exit_filled_size)
          .SetExitOrderPrice(next_open)
          .SetExitFilledTime(next_open_time)
          .SetExitFilledSize(exit_filled_size);

      // 레버리지 로딩
      const auto leverage = market_exit->GetLeverage();

      // 슬리피지가 포함된 체결가
      const double slippage_filled_price =
          CalculateSlippagePrice(next_open, OrderType::MARKET,
                                 market_exit->GetExitDirection(), leverage);
      market_exit->SetExitFilledPrice(slippage_filled_price);

      // 수수료
      market_exit->SetExitCommission(
          CalculateCommission(slippage_filled_price, OrderType::MARKET,
                              exit_filled_size, leverage));

      // 원본 진입 객체에 청산 체결 수량 추가
      filled_entry->SetExitFilledSize(filled_entry->GetExitFilledSize() +
                                      exit_filled_size);

      /* 원본 진입 객체의 청산 체결 수량이 진입 체결 수량과 같으면
         filled_entries에서 삭제 (부동 소수점 오류 방지용 '>=') */
      if (filled_entry->GetExitFilledSize() >=
          filled_entry->GetEntryFilledSize()) {
        filled_entries_[symbol_idx].erase(filled_entries_[symbol_idx].begin() +
                                          i);

        // 같은 진입 이름을 목표로 하는 청산 대기 주문 취소
        erase_if(pending_exits_[symbol_idx],
                 [&](const shared_ptr<Order>& pending_exit) {
                   return target_entry == pending_exit->GetEntryName();
                 });
      }

      // 자금, 통계 업데이트
      ExecuteExit(market_exit);

      // 디버그 로그 기록
      LogFormattedInfo(
          LogLevel::DEBUG_L,
          format("{} | 시장가 {} 체결", slippage_filled_price, exit_name),
          __FILE__, __LINE__);
      return;
    }
  }

  // 목표한 진입 이름을 찾지 못하면 경고 로그
  LogFormattedInfo(
      LogLevel::WARNING_L,
      format("지정된 진입명 {}이(가) 존재하지 않아 청산할 수 없습니다.",
             target_entry),
      __FILE__, __LINE__);
}

void OrderHandler::LimitExit(const string& exit_name,
                             const string& target_entry, const double exit_size,
                             const double order_price) {
  // 예외 체크
  try {
    IsValidPositionSize(exit_size);
    IsValidPrice(order_price);
  } catch (const exception& e) {
    LogFormattedInfo(LogLevel::WARNING_L, e.what(), __FILE__, __LINE__);
    return;
  }

  // 변수 계산
  const auto symbol_idx = bar_->GetCurrentSymbolIndex();
  const auto bar_idx = bar_->GetCurrentBarIndex();
  const auto& bar = bar_->GetBarData(bar_->GetCurrentBarType(),
                                     bar_->GetCurrentReferenceTimeframe());

  int64_t next_open_time;
  double next_open;
  try {
    next_open_time = bar.GetOpenTime(symbol_idx, bar_idx + 1);
    next_open = bar.GetOpen(symbol_idx, bar_idx + 1);
  } catch (const exception& e) {
    LogFormattedInfo(
        LogLevel::WARNING_L,
        string(e.what()) + ": 마지막 바이기 때문에 청산할 수 없습니다.",
        __FILE__, __LINE__);
    return;
  }

  // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 (주문 수정)
  Cancel(exit_name);

  // 체결된 진입들 순회
  for (int i = 0; i < filled_entries_[symbol_idx].size(); i++) {
    // target_entry와 같은 이름의 진입이 있으면 청산
    if (const auto& filled_entry = filled_entries_[symbol_idx][i];
        filled_entry->GetEntryName() == target_entry) {
      // Entry Order 복사
      const auto& limit_exit = make_shared<Order>(*filled_entry);

      // 청산 주문 생성
      limit_exit->SetExitName(exit_name)
          .SetExitOrderType(OrderType::LIMIT)
          .SetExitDirection(limit_exit->GetEntryDirection() == Direction::LONG
                                ? Direction::SHORT
                                : Direction::LONG)
          .SetExitOrderTime(next_open_time)
          .SetExitOrderSize(exit_size)
          .SetExitOrderPrice(order_price);

      // 유효성 검사
      try {
        IsValidLimitOrderPrice(order_price, next_open,
                               limit_exit->GetExitDirection());
      } catch (const exception& e) {
        LogFormattedInfo(LogLevel::WARNING_L, e.what(), __FILE__, __LINE__);
        return;
      }

      // 대기 중인 청산에 추가
      pending_exits_[symbol_idx].push_back(limit_exit);

      // 디버그 로그 기록
      LogFormattedInfo(
          LogLevel::DEBUG_L,
          format("{} | 지정가 {} 체결 대기", order_price, exit_name), __FILE__,
          __LINE__);
      return;
    }
  }

  // 목표한 진입 이름을 찾지 못하면 경고 로그
  LogFormattedInfo(
      LogLevel::WARNING_L,
      format("지정된 진입명 {}이(가) 존재하지 않아 청산할 수 없습니다.",
             target_entry),
      __FILE__, __LINE__);
}

void OrderHandler::MitExit(const string& exit_name, const string& target_entry,
                           const double exit_size, const double touch_price) {
  // 예외 체크
  try {
    IsValidPositionSize(exit_size);
    IsValidPrice(touch_price);
  } catch (const exception& e) {
    LogFormattedInfo(LogLevel::WARNING_L, e.what(), __FILE__, __LINE__);
    return;
  }

  // 변수 계산
  const auto symbol_idx = bar_->GetCurrentSymbolIndex();
  double next_open;
  try {
    next_open = bar_->GetBarData(bar_->GetCurrentBarType(),
                                 bar_->GetCurrentReferenceTimeframe())
                    .GetOpen(symbol_idx, bar_->GetCurrentBarIndex() + 1);
  } catch (const exception& e) {
    LogFormattedInfo(
        LogLevel::WARNING_L,
        string(e.what()) + ": 마지막 바이기 때문에 청산할 수 없습니다.",
        __FILE__, __LINE__);
    return;
  }

  // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 (주문 수정)
  Cancel(exit_name);

  // 체결된 진입들 순회
  for (int i = 0; i < filled_entries_[symbol_idx].size(); i++) {
    // target_entry와 같은 이름의 진입이 있으면 청산
    if (const auto& filled_entry = filled_entries_[symbol_idx][i];
        filled_entry->GetEntryName() == target_entry) {
      // Entry Order 복사
      const auto& mit_exit = make_shared<Order>(*filled_entry);

      // 청산 주문 생성
      mit_exit->SetExitName(exit_name)
          .SetExitOrderType(OrderType::MIT)
          .SetExitDirection(mit_exit->GetEntryDirection() == Direction::LONG
                                ? Direction::SHORT
                                : Direction::LONG)
          .SetExitTouchPrice(touch_price)
          .SetExitTouchDirection(next_open >= touch_price ? Direction::LONG
                                                          : Direction::SHORT)
          .SetExitOrderSize(exit_size);

      // 대기 중인 청산에 추가
      pending_exits_[symbol_idx].push_back(mit_exit);

      // 디버그 로그 기록
      LogFormattedInfo(LogLevel::DEBUG_L,
                       format("{} | MIT {} 터치 대기", touch_price, exit_name),
                       __FILE__, __LINE__);
      return;
    }
  }

  // 목표한 진입 이름을 찾지 못하면 경고 로그
  LogFormattedInfo(
      LogLevel::WARNING_L,
      format("지정된 진입명 {}이(가) 존재하지 않아 청산할 수 없습니다.",
             target_entry),
      __FILE__, __LINE__);
}

void OrderHandler::LitExit(const string& exit_name, const string& target_entry,
                           const double exit_size, const double touch_price,
                           const double order_price) {
  // 예외 체크
  try {
    IsValidPositionSize(exit_size);
    IsValidPrice(touch_price);
    IsValidPrice(order_price);
  } catch (const exception& e) {
    LogFormattedInfo(LogLevel::WARNING_L, e.what(), __FILE__, __LINE__);
    return;
  }

  // 변수 계산
  const auto symbol_idx = bar_->GetCurrentSymbolIndex();
  double next_open;
  try {
    next_open = bar_->GetBarData(bar_->GetCurrentBarType(),
                                 bar_->GetCurrentReferenceTimeframe())
                    .GetOpen(symbol_idx, bar_->GetCurrentBarIndex() + 1);
  } catch (const exception& e) {
    LogFormattedInfo(
        LogLevel::WARNING_L,
        string(e.what()) + ": 마지막 바이기 때문에 청산할 수 없습니다.",
        __FILE__, __LINE__);
    return;
  }

  // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 (주문 수정)
  Cancel(exit_name);

  // 체결된 진입들 순회
  for (int i = 0; i < filled_entries_[symbol_idx].size(); i++) {
    // target_entry와 같은 이름의 진입이 있으면 청산
    if (const auto& filled_entry = filled_entries_[symbol_idx][i];
        filled_entry->GetEntryName() == target_entry) {
      // Entry Order 복사
      const auto& lit_exit = make_shared<Order>(*filled_entry);

      // 청산 주문 생성
      lit_exit->SetExitName(exit_name)
          .SetExitOrderType(OrderType::LIT)
          .SetExitDirection(lit_exit->GetEntryDirection() == Direction::LONG
                                ? Direction::SHORT
                                : Direction::LONG)
          .SetExitTouchPrice(touch_price)
          .SetExitTouchDirection(next_open >= touch_price ? Direction::LONG
                                                          : Direction::SHORT)
          .SetExitOrderSize(exit_size)
          .SetExitOrderPrice(order_price);

      // 유효성 검사
      try {
        IsValidLimitOrderPrice(order_price, touch_price,
                               lit_exit->GetExitDirection());
      } catch (const exception& e) {
        LogFormattedInfo(LogLevel::WARNING_L, e.what(), __FILE__, __LINE__);
        return;
      }

      // 대기 중인 청산에 추가
      pending_exits_[symbol_idx].push_back(lit_exit);

      // 디버그 로그 기록
      LogFormattedInfo(LogLevel::DEBUG_L,
                       format("{} | LIT {} 터치 대기", touch_price, exit_name),
                       __FILE__, __LINE__);
      return;
    }
  }

  // 목표한 진입 이름을 찾지 못하면 경고 로그
  LogFormattedInfo(
      LogLevel::WARNING_L,
      format("지정된 진입명 {}이(가) 존재하지 않아 청산할 수 없습니다.",
             target_entry),
      __FILE__, __LINE__);
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
  } catch (const exception& e) {
    LogFormattedInfo(LogLevel::WARNING_L, e.what(), __FILE__, __LINE__);
    return;
  }

  // 변수 계산
  const auto symbol_idx = bar_->GetCurrentSymbolIndex();
  double next_open;
  try {
    next_open = bar_->GetBarData(bar_->GetCurrentBarType(),
                                 bar_->GetCurrentReferenceTimeframe())
                    .GetOpen(symbol_idx, bar_->GetCurrentBarIndex() + 1);
  } catch (const exception& e) {
    LogFormattedInfo(
        LogLevel::WARNING_L,
        string(e.what()) + ": 마지막 바이기 때문에 청산할 수 없습니다.",
        __FILE__, __LINE__);
    return;
  }

  // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 (주문 수정)
  Cancel(exit_name);

  // 체결된 진입들 순회
  for (int i = 0; i < filled_entries_[symbol_idx].size(); i++) {
    // target_entry와 같은 이름의 진입이 있으면 청산
    if (const auto& filled_entry = filled_entries_[symbol_idx][i];
        filled_entry->GetEntryName() == target_entry) {
      // Entry Order 복사
      const auto& trailing_exit = make_shared<Order>(*filled_entry);

      // 청산 주문 생성
      trailing_exit->SetExitName(exit_name)
          .SetExitOrderType(OrderType::TRAILING)
          .SetExitDirection(trailing_exit->GetEntryDirection() ==
                                    Direction::LONG
                                ? Direction::SHORT
                                : Direction::LONG)
          .SetExitTouchPrice(touch_price)
          .SetExitTouchDirection(next_open >= touch_price ? Direction::LONG
                                                          : Direction::SHORT)
          .SetExitTrailPoint(trail_point)
          .SetExitOrderSize(exit_size);

      // touch_price가 0이면 다음 시가부터 최고저가 추적을 시작
      if (touch_price == 0) {
        trailing_exit->SetEntryExtremePrice(next_open);
      }

      // 대기 중인 청산에 추가
      pending_exits_[symbol_idx].push_back(trailing_exit);

      // 디버그 로그 기록
      if (touch_price != 0) {
        LogFormattedInfo(
            LogLevel::DEBUG_L,
            format("{} | 트레일링 {} 터치 대기", touch_price, exit_name),
            __FILE__, __LINE__);
      } else {
        // touch_price가 0이라 바로 추적 시작한 경우
        LogFormattedInfo(LogLevel::DEBUG_L,
                         format("트레일링 {} 체결 대기", exit_name), __FILE__,
                         __LINE__);
      }
      return;
    }
  }

  // 목표한 진입 이름을 찾지 못하면 경고 로그
  LogFormattedInfo(
      LogLevel::WARNING_L,
      format("지정된 진입명 {}이(가) 존재하지 않아 청산할 수 없습니다.",
             target_entry),
      __FILE__, __LINE__);
}

void OrderHandler::Cancel(const string& order_name) {
  const int symbol_idx = bar_->GetCurrentSymbolIndex();

  for (int i = 0; i < pending_entries_[symbol_idx].size(); i++) {
    if (order_name == pending_entries_[symbol_idx][i]->GetEntryName()) {
      // 예약 증거금 회복 과정 진행 후 삭제
      ExecuteCancelEntry(pending_entries_[symbol_idx][i]);
      pending_entries_[symbol_idx].erase(pending_entries_[symbol_idx].begin() +
                                         i);

      LogFormattedInfo(LogLevel::DEBUG_L,
                       order_name + " 주문이 취소되었습니다.", __FILE__,
                       __LINE__);
      break;  // 동일한 진입 이름으로 진입 대기 불가능하므로 찾으면 바로 break
    }
  }

  for (int i = 0; i < pending_exits_[symbol_idx].size(); i++) {
    if (order_name == pending_exits_[symbol_idx][i]->GetExitName()) {
      // 청산 주문은 예약 증거금이 필요하지 않기 때문에 삭제만 함
      pending_exits_[symbol_idx].erase(pending_exits_[symbol_idx].begin() + i);

      LogFormattedInfo(LogLevel::DEBUG_L,
                       order_name + " 주문이 취소되었습니다.", __FILE__,
                       __LINE__);
      break;  // 동일한 청산 이름으로 청산 대기 불가능하므로 찾으면 바로 break
    }
  }
}

void OrderHandler::ExitOppositeFilledEntries(const Direction direction) {
  for (const auto symbol_idx = bar_->GetCurrentSymbolIndex();
       const auto& filled_entry : filled_entries_[symbol_idx]) {
    if (const auto entry_direction = filled_entry->GetEntryDirection();
        direction != entry_direction) {
      MarketExit("리버스 시장가 " + (entry_direction == Direction::LONG)
                     ? "매도"
                     : "매수",
                 filled_entry->GetEntryName(),
                 // 분할 청산했을 수도 있으므로 잔량만 청산
                 filled_entry->GetEntryFilledSize() -
                     filled_entry->GetExitFilledSize());
    }
  }
}

void OrderHandler::ExecuteExit(const shared_ptr<Order>& exit_order) {
  const double exit_filled_size = exit_order->GetExitFilledSize();

  // 미실현 손익 업데이트
  engine_->UpdateUnrealizedPnl();

  // 현재 자금(주문 가능 자금)에서 수수료 감소
  if (!engine_->DecreaseWalletBalance(exit_order->GetExitCommission())) {
    engine_->SetBankruptcy();
    throw Bankruptcy("파산");
  }

  // 주문 가능 자금에서 진입 증거금 회복
  engine_->IncreaseAvailableBalance(exit_filled_size *
                                    exit_order->GetEntryFilledPrice());

  // 손익 계산
  double pnl;
  if (exit_order->GetEntryDirection() == Direction::LONG) {
    pnl =
        (exit_order->GetExitFilledPrice() - exit_order->GetEntryFilledPrice()) *
        exit_filled_size * exit_order->GetLeverage();
  } else {
    pnl =
        (exit_order->GetEntryFilledPrice() - exit_order->GetExitFilledPrice()) *
        exit_filled_size * exit_order->GetLeverage();
  }

  // 현재 자금(주문 가능 자금)에 손익 합산
  if (pnl > 0) {
    engine_->IncreaseWalletBalance(pnl);
  } else if (pnl < 0) {
    if (!engine_->DecreaseWalletBalance(abs(pnl))) {
      engine_->SetBankruptcy();
      throw Bankruptcy("파산");
    }
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
      engine_->IncreaseAvailableBalance(cancel_order->GetEntryOrderSize() *
                                        cancel_order->GetEntryOrderPrice());
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
        engine_->IncreaseAvailableBalance(cancel_order->GetEntryOrderSize() *
                                          cancel_order->GetEntryOrderPrice());
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

void OrderHandler::ExecutePendingLimitEntry(const int order_idx,
                                            const int64_t open_time,
                                            const double entry_filled_price) {
  // 변수 계산
  const auto symbol_idx = bar_->GetCurrentSymbolIndex();

  // 대기 주문을 받아오고 대기 주문에서 삭제
  const auto& limit_entry = filled_entries_[symbol_idx][order_idx];
  filled_entries_[symbol_idx].erase(filled_entries_[symbol_idx].begin() +
                                    order_idx);

  // 주문 정보 로딩
  const auto entry_filled_size = limit_entry->GetEntryOrderSize();
  const auto entry_direction = limit_entry->GetEntryDirection();
  const auto leverage = limit_entry->GetLeverage();

  // 주문 업데이트
  limit_entry->SetEntryFilledTime(open_time)
             .SetEntryFilledSize(entry_filled_size);

  // 슬리피지가 포함된 체결가
  const double slippage_filled_price = CalculateSlippagePrice(
      entry_filled_price, OrderType::LIMIT, entry_direction, leverage);
  limit_entry->SetEntryFilledPrice(slippage_filled_price);

  // 수수료
  const double entry_commission = CalculateCommission(
      slippage_filled_price, OrderType::LIMIT, entry_filled_size, leverage);
  limit_entry->SetEntryCommission(entry_commission);

  // 마진콜 가격
  limit_entry->SetMarginCallPrice(CalculateMarginCallPrice(
      slippage_filled_price, entry_direction, leverage));

  // 해당 주문과 반대 방향의 체결 주문이 있으면 모두 청산
  ExitOppositeFilledEntries(entry_direction);

  // 현재 자금(주문 가능 자금)에서 수수료 감소
  if (!engine_->DecreaseWalletBalance(entry_commission)) {
    engine_->SetBankruptcy();
    throw Bankruptcy("파산");
  }

  // 지정가 진입
  filled_entries_[symbol_idx].push_back(limit_entry);

  // 디버그 로그 기록
  LogFormattedInfo(
      LogLevel::DEBUG_L,
      format("{} | 지정가 {} 체결", slippage_filled_price, limit_entry->GetEntryName()),
      __FILE__, __LINE__);
}

void OrderHandler::ExecutePendingMitEntry(const int order_idx,
                                          const int64_t open_time,
                                          const double entry_order_price) {
  // 변수 로딩
  const auto symbol_idx = bar_->GetCurrentSymbolIndex();

  // 대기 주문을 받아오고 대기 주문에서 삭제
  const auto& mit_entry = filled_entries_[symbol_idx][order_idx];
  filled_entries_[symbol_idx].erase(filled_entries_[symbol_idx].begin() + order_idx);

  // 주문 정보 로딩
  const auto entry_filled_size = mit_entry->GetEntryOrderSize();
  const auto entry_direction = mit_entry->GetEntryDirection();
  const auto leverage = mit_entry->GetLeverage();

  // 주문 업데이트
  mit_entry->SetEntryOrderTime(open_time)
           .SetEntryOrderPrice(entry_order_price)
           .SetEntryFilledTime(open_time)
           .SetEntryFilledSize(entry_filled_size);

  // 슬리피지가 포함된 체결가
  const double slippage_filled_price = CalculateSlippagePrice(
    entry_order_price, OrderType::MIT, entry_direction, leverage);
  mit_entry->SetEntryFilledPrice(slippage_filled_price);

  // 수수료
  const double entry_commission = CalculateCommission(
      slippage_filled_price, OrderType::LIMIT, entry_filled_size, leverage);
  mit_entry->SetEntryCommission(entry_commission);

  // 마진콜 가격
  mit_entry->SetMarginCallPrice(CalculateMarginCallPrice(
      slippage_filled_price, entry_direction, leverage));

  // 해당 주문과 반대 방향의 체결 주문이 있으면 모두 청산
  ExitOppositeFilledEntries(entry_direction);

  // 진입 증거금 계산: 1포인트 == 1달러 가정 계산
  const double entry_margin = entry_filled_size * slippage_filled_price;

  // 미실현 손익 업데이트
  engine_->UpdateUnrealizedPnl();

  // 진입 가능 여부 체크 (진입 가능 자금 > 진입 증거금 + 수수료)
  if (!HasEnoughBalance(engine_->GetAvailableBalance(),
                        entry_margin + entry_commission)) {
    return;
  }

  // 현재 자금(주문 가능 자금)에서 수수료 감소
  if (!engine_->DecreaseWalletBalance(entry_commission)) {
    engine_->SetBankruptcy();
    throw Bankruptcy("파산");
  }

  // 주문 가능 자금에서 진입 증거금 감소
  engine_->DecreaseAvailableBalance(entry_margin);

  // MIT 진입
  filled_entries_[symbol_idx].push_back(mit_entry);

  // 디버그 로그 기록
  LogFormattedInfo(
      LogLevel::DEBUG_L,
      format("{} | MIT {} 체결", slippage_filled_price, mit_entry->GetEntryName()),
      __FILE__, __LINE__);
}

bool OrderHandler::OrderPendingLitEntry(const int order_idx, const int64_t open_time) {
  // 대기 주문 로딩
  const auto& lit_entry = filled_entries_[bar_->GetCurrentSymbolIndex()][order_idx];

  // 주문 업데이트
  lit_entry->SetEntryOrderTime(open_time);

  // 예약 증거금 계산: 1포인트 == 1달러 가정 계산
  const double entry_margin = lit_entry->GetEntryOrderSize() *
                                lit_entry->GetEntryOrderPrice();

  // 미실현 손익 업데이트
  engine_->UpdateUnrealizedPnl();

  // 주문 가능 여부 체크
  if (!HasEnoughBalance(engine_->GetAvailableBalance(), entry_margin)) {
    // 주문 실패 시 대기 주문에서 삭제
    filled_entries_[order_idx].erase(filled_entries_[order_idx].begin() + order_idx);
    return false;
  }

  // 주문 가능 자금에서 예약 증거금 감소
  engine_->DecreaseAvailableBalance(entry_margin);

  // 디버그 로그 기록
  LogFormattedInfo(
      LogLevel::DEBUG_L,
      format("{} | LIT {} 체결 대기", lit_entry->GetEntryOrderPrice(), lit_entry->GetEntryName()),
      __FILE__, __LINE__);

  return true;
}

double OrderHandler::GetAdjustedExitFilledSize(
    const double exit_size, const shared_ptr<Order>& exit_order) {
  // 청산 수량 + 분할 청산한 수량이 진입 수량보다 많다면
  if (const auto entry_size = exit_order->GetEntryFilledSize();
      exit_size + exit_order->GetExitFilledSize() > entry_size) {
    // 경고 로그 기록
    LogFormattedInfo(
      LogLevel::WARNING_L,
      format("합계 청산 크기 {}이(가) 진입 크기 {}을(를) 초과했습니다.",
        exit_size + exit_order->GetExitFilledSize(), entry_size),
      __FILE__, __LINE__);

    // 최대값으로 조정하여 반환
    return entry_size - exit_order->GetExitFilledSize();
  }

  return exit_size;
}

bool OrderHandler::HasEnoughBalance(const double available_balance,
                                    const double needed_balance) {
  if (available_balance < needed_balance) {
    // 경고 로그 기록
    LogFormattedInfo(
      LogLevel::WARNING_L,
      format("주문 가능 자금 ${}는 필요 자금 ${}보다 많아야합니다.",
        available_balance, needed_balance),
      __FILE__, __LINE__);

    return false;
  }

  return true;
}
