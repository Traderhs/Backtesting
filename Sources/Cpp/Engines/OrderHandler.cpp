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
using enum PriceType;

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

void OrderHandler::CheckPendingEntries(const vector<PriceData>& price_queue) {
  for (const auto& [price, price_type, symbol_index] : price_queue) {
    bar_->SetCurrentSymbolIndex(symbol_index);

    // 가격 유효성 검사
    try {
      IsValidPrice(price);
    } catch (const InvalidValue& e) {
      LogFormattedInfo(
          LogLevel::WARNING_L,
          string(e.what()) +
              ": 현재가가 유효하지 않으므로 진입 대기 주문의 체결을 "
              "확인할 수 없습니다.",
          __FILE__, __LINE__);
      continue;
    }

    // 현재 심볼의 대기 주문 로딩
    const auto& pending_entries = pending_entries_[symbol_index];

    // 체결 혹은 주문, 체결 거부 시 해당 주문이 대기 주문에서 삭제되므로
    // 역순으로 순회
    for (int order_idx = static_cast<int>(pending_entries.size() - 1);
         order_idx >= 0; order_idx--) {
      switch (pending_entries[order_idx]->GetEntryOrderType()) {
        case OrderType::MARKET: {
          // 시장가는 대기 주문이 없음
          LogFormattedInfo(LogLevel::WARNING_L,
                           "대기 주문에 시장가 주문이 존재합니다.", __FILE__,
                           __LINE__);
          continue;
        }

        case OrderType::LIMIT: {
          // 지정가 체결은 시고저가에서 확인 시 종가에서 확인할 필요 없음
          if (price_type == CLOSE) {
            break;
          }

          CheckPendingLimitEntries(order_idx, price, price_type);
          continue;
        }

        case OrderType::MIT: {
          // MIT 체결은 시고저가에서 확인 시 종가에서 확인할 필요 없음
          if (price_type == CLOSE) {
            break;
          }

          CheckPendingMitEntries(order_idx, price, price_type);
          continue;
        }

        case OrderType::LIT: {
          CheckPendingLitEntries(order_idx, price, price_type);
          continue;
        }

        case OrderType::TRAILING: {
          CheckPendingTrailingEntries(order_idx, price, price_type);
          continue;
        }

        case OrderType::NONE: {
          // NONE 타입은 에러
          LogFormattedInfo(LogLevel::WARNING_L,
                           "대기 주문에 NONE 주문이 존재합니다.", __FILE__,
                           __LINE__);
        }
      }
    }
  }
}

void OrderHandler::CheckPendingExits(const double* prices,
                                     const int64_t open_time) {
  // @@@@@@@@@@@@@@@@@@@ Execute와 시장가 Exit에서 공통 부분은 ExecuteExit으로
  // 묶고 주석 변경

  // 가격 유효성 검사
  try {
    IsValidPrice(price);
  } catch (const InvalidValue& e) {
    LogFormattedInfo(LogLevel::WARNING_L,
                     string(e.what()) +
                         ": 현재가가 유효하지 않으므로 청산 대기 주문의 체결을 "
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
      -> 체결 시 해당 주문 및 같은 진입 이름을 목표로 한 대기 주문이 삭제될 수
     있음 */
  int deleted_count = 0;
  for (int i = static_cast<int>(current_symbol_pending_exits.size() - 1);
       i >= 0;) {
    switch (const auto& pending_exit = current_symbol_pending_exits[i];
            pending_exit->GetExitOrderType()) {
      case OrderType::MARKET: {
        // 시장가는 대기 주문이 없음
        LogFormattedInfo(LogLevel::WARNING_L,
                         "대기 주문에 시장가 주문이 존재합니다.", __FILE__,
                         __LINE__);
        break;
      }

      case OrderType::LIMIT: {
        // 주문 정보 로딩
        const auto exit_direction = pending_exit->GetExitDirection();

        // 매수 청산의 경우, 가격이 주문 가격과 같거나 낮아지면 체결
        // 매도 청산의 경우, 가격이 주문 가격과 같거나 높아지면 체결
        if (const auto exit_order_price = pending_exit->GetExitOrderPrice();
            (exit_direction == Direction::LONG &&
             PriceType <= exit_order_price) ||
            (exit_direction == Direction::SHORT &&
             PriceType >= exit_order_price)) {
          // 시가에서 진입 조건 달성 시 체결 가격은 시가가 됨 (갭 고려)
          deleted_count = ExecutePendingLimitExit(
              i, open_time, is_open_price_ ? PriceType : exit_order_price);
        }

        break;
      }

      case OrderType::MIT: {
      }

      case OrderType::LIT: {
      }

      case OrderType::TRAILING: {
      }

      case OrderType::NONE: {
        // NONE 타입은 에러
        LogFormattedInfo(LogLevel::WARNING_L,
                         "대기 주문에 NONE 주문이 존재합니다.", __FILE__,
                         __LINE__);
      }
    }

    if (deleted_count > 0) {
      i -= deleted_count;  // 삭제된 개수만큼 인덱스를 감소시킴
    } else {
      i--;  // 삭제가 없으면 정상적으로 한 단계 감소
    }

    deleted_count = 0;
  }
}

void OrderHandler::MarketEntry(const string& entry_name,
                               const Direction entry_direction,
                               const double entry_size,
                               const unsigned char leverage) {
  // 유효성 검사
  try {
    IsValidEntryName(entry_name);
    IsValidDirection(entry_direction);
    IsValidPositionSize(entry_size);
    IsValidLeverage(leverage);
  } catch (const InvalidValue& e) {
    LogFormattedInfo(LogLevel::WARNING_L, e.what(), __FILE__, __LINE__);
    throw OrderFailed("시장가 진입 주문이 실패했습니다.");
  }

  // 바 정보 로딩
  const int symbol_idx = bar_->GetCurrentSymbolIndex();
  const size_t bar_idx = bar_->GetCurrentBarIndex();
  const auto& bar = bar_->GetBarData(bar_->GetCurrentBarType(),
                                     bar_->GetCurrentReferenceTimeframe());

  int64_t next_open_time;  // 진입 시간: 다음 바의 시작 시간
  double next_open;        // 주문 가격: 다음 바의 시가
  try {
    next_open_time = bar.SafeGetBar(symbol_idx, bar_idx + 1).open_time;
    next_open = bar.SafeGetBar(symbol_idx, bar_idx + 1).open;
  } catch (const IndexOutOfRange& e) {
    LogFormattedInfo(
        LogLevel::WARNING_L,
        string(e.what()) + ": 마지막 바이기 때문에 진입할 수 없습니다.",
        __FILE__, __LINE__);
    throw OrderFailed("시장가 진입 주문이 실패했습니다.");
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

  // 자금 관련 처리 후 체결 주문에 추가
  try {
    ExecuteMarketEntry(market_entry);
  } catch ([[maybe_unused]] const OrderFailed& e) {
    throw;
  }

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
  // 바 정보 로딩
  const int symbol_idx = bar_->GetCurrentSymbolIndex();
  const size_t bar_idx = bar_->GetCurrentBarIndex();
  const auto& bar = bar_->GetBarData(bar_->GetCurrentBarType(),
                                     bar_->GetCurrentReferenceTimeframe());

  int64_t next_open_time;
  double next_open;
  try {
    next_open_time = bar.SafeGetBar(symbol_idx, bar_idx + 1).open_time;
    next_open = bar.SafeGetBar(symbol_idx, bar_idx + 1).open;
  } catch (const IndexOutOfRange& e) {
    LogFormattedInfo(
        LogLevel::WARNING_L,
        string(e.what()) + ": 마지막 바이기 때문에 진입할 수 없습니다.",
        __FILE__, __LINE__);
    throw OrderFailed("지정가 진입 주문이 실패했습니다.");
  }

  // 예외 체크
  try {
    IsValidDirection(entry_direction);
    IsValidPositionSize(entry_size);
    IsValidLeverage(leverage);
    IsValidPrice(order_price);
    IsValidLimitOrderPrice(order_price, next_open, entry_direction);
  } catch (const InvalidValue& e) {
    LogFormattedInfo(LogLevel::WARNING_L, e.what(), __FILE__, __LINE__);
    throw OrderFailed("지정가 진입 주문이 실패했습니다.");
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

  // 주문 가능 여부 체크
  try {
    HasEnoughBalance(engine_->GetAvailableBalance(), entry_margin);
  } catch (const InsufficientBalance& e) {
    LogFormattedInfo(LogLevel::WARNING_L, e.what(), __FILE__, __LINE__);
    throw OrderFailed("지정가 진입 주문이 실패했습니다.");
  }

  // 사용한 마진에 예약 증거금 증가
  engine_->IncreaseUsedMargin(entry_margin);

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
  // 유효성 검사
  try {
    IsValidPositionSize(entry_size);
    IsValidLeverage(leverage);
    IsValidPrice(touch_price);
  } catch (const InvalidValue& e) {
    LogFormattedInfo(LogLevel::WARNING_L, e.what(), __FILE__, __LINE__);
    throw OrderFailed("MIT 진입 주문이 실패했습니다.");
  }

  // 바 정보 로딩
  const int symbol_idx = bar_->GetCurrentSymbolIndex();
  double next_open;
  try {
    next_open = bar_->GetBarData(bar_->GetCurrentBarType(),
                                 bar_->GetCurrentReferenceTimeframe())
                    .SafeGetBar(symbol_idx, bar_->GetCurrentBarIndex() + 1)
                    .open;
  } catch (const IndexOutOfRange& e) {
    LogFormattedInfo(
        LogLevel::WARNING_L,
        string(e.what()) + ": 마지막 바이기 때문에 진입할 수 없습니다.",
        __FILE__, __LINE__);
    throw OrderFailed("MIT 진입 주문이 실패했습니다.");
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
  // 유효성 검사
  try {
    IsValidPositionSize(entry_size);
    IsValidLeverage(leverage);
    IsValidPrice(touch_price);
    IsValidPrice(order_price);
    IsValidLimitOrderPrice(order_price, touch_price, entry_direction);
  } catch (const InvalidValue& e) {
    LogFormattedInfo(LogLevel::WARNING_L, e.what(), __FILE__, __LINE__);
    throw OrderFailed("LIT 진입 주문이 실패했습니다.");
  }

  // 바 정보 로딩
  const auto symbol_idx = bar_->GetCurrentSymbolIndex();
  double next_open;
  try {
    next_open = bar_->GetBarData(bar_->GetCurrentBarType(),
                                 bar_->GetCurrentReferenceTimeframe())
                    .SafeGetBar(symbol_idx, bar_->GetCurrentBarIndex() + 1)
                    .open;
  } catch (const IndexOutOfRange& e) {
    LogFormattedInfo(
        LogLevel::WARNING_L,
        string(e.what()) + ": 마지막 바이기 때문에 진입할 수 없습니다.",
        __FILE__, __LINE__);
    throw OrderFailed("LIT 진입 주문이 실패했습니다.");
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
  // 유효성 검사
  try {
    IsValidPositionSize(entry_size);
    IsValidLeverage(leverage);
    IsValidTrailingTouchPrice(touch_price);
    IsValidTrailPoint(trail_point);
  } catch (const InvalidValue& e) {
    LogFormattedInfo(LogLevel::WARNING_L, e.what(), __FILE__, __LINE__);
    throw OrderFailed("트레일링 진입 주문이 실패했습니다.");
  }

  // 바 정보 로딩
  const int symbol_idx = bar_->GetCurrentSymbolIndex();
  double next_open;
  try {
    next_open = bar_->GetBarData(bar_->GetCurrentBarType(),
                                 bar_->GetCurrentReferenceTimeframe())
                    .SafeGetBar(symbol_idx, bar_->GetCurrentBarIndex() + 1)
                    .open;
  } catch (const IndexOutOfRange& e) {
    LogFormattedInfo(
        LogLevel::WARNING_L,
        string(e.what()) + ": 마지막 바이기 때문에 진입할 수 없습니다.",
        __FILE__, __LINE__);
    throw OrderFailed("트레일링 진입 주문이 실패했습니다.");
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

  // 트레일링 진입 터치 대기
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
                              const string& target_entry_name,
                              const double exit_size) {
  // 유효성 검사
  try {
    IsValidPositionSize(exit_size);
  } catch (const InvalidValue& e) {
    LogFormattedInfo(LogLevel::WARNING_L, e.what(), __FILE__, __LINE__);
    throw OrderFailed("시장가 청산 주문이 실패했습니다.");
  }

  // 바 정보 로딩
  const auto symbol_idx = bar_->GetCurrentSymbolIndex();
  const auto bar_idx = bar_->GetCurrentBarIndex();
  const auto& bar = bar_->GetBarData(bar_->GetCurrentBarType(),
                                     bar_->GetCurrentReferenceTimeframe());

  int64_t next_open_time;
  double next_open;
  try {
    next_open_time = bar.SafeGetBar(symbol_idx, bar_idx + 1).open_time;
    next_open = bar.SafeGetBar(symbol_idx, bar_idx + 1).open;
  } catch (const IndexOutOfRange& e) {
    LogFormattedInfo(
        LogLevel::WARNING_L,
        string(e.what()) + ": 마지막 바이기 때문에 청산할 수 없습니다.",
        __FILE__, __LINE__);
    throw OrderFailed("시장가 청산 주문이 실패했습니다.");
  }

  // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 (주문 수정)
  Cancel(exit_name);

  // 목표 진입 찾기
  const auto& [filled_entry, order_idx] =
      FindMatchingEntryOrder(target_entry_name);
  if (order_idx == -1) {
    // 목표한 진입 이름을 찾지 못하면 청산 실패
    LogFormattedInfo(
        LogLevel::WARNING_L,
        format("지정된 진입명 {}이(가) 존재하지 않아 청산할 수 없습니다.",
               target_entry_name),
        __FILE__, __LINE__);
    throw OrderFailed("시장가 청산 주문이 실패했습니다.");
  }

  // 진입 주문 복사
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
  const double slippage_filled_price = CalculateSlippagePrice(
      next_open, OrderType::MARKET, market_exit->GetExitDirection(), leverage);
  market_exit->SetExitFilledPrice(slippage_filled_price);

  // 수수료
  market_exit->SetExitCommission(CalculateCommission(
      slippage_filled_price, OrderType::MARKET, exit_filled_size, leverage));

  // 원본 진입 주문에 청산 체결 수량 추가
  filled_entry->SetExitFilledSize(filled_entry->GetExitFilledSize() +
                                  exit_filled_size);

  /* 원본 진입 주문의 청산 체결 수량이 진입 체결 수량과 같으면
     filled_entries에서 삭제 (부동 소수점 오류 방지용 '>=') */
  if (filled_entry->GetExitFilledSize() >= filled_entry->GetEntryFilledSize()) {
    filled_entries_[symbol_idx].erase(filled_entries_[symbol_idx].begin() +
                                      order_idx);

    // 같은 진입 이름을 목표로 하는 청산 대기 주문 취소
    erase_if(pending_exits_[symbol_idx],
             [&](const shared_ptr<Order>& pending_exit) {
               return target_entry_name == pending_exit->GetEntryName();
             });
  }

  // 자금, 통계 업데이트
  ExecuteExit(market_exit);

  // 디버그 로그 기록
  LogFormattedInfo(
      LogLevel::DEBUG_L,
      format("{} | 시장가 {} 체결", slippage_filled_price, exit_name), __FILE__,
      __LINE__);
}

void OrderHandler::LimitExit(const string& exit_name,
                             const string& target_entry_name,
                             const double exit_size, const double order_price) {
  // 바 정보 로딩
  const auto symbol_idx = bar_->GetCurrentSymbolIndex();
  const auto bar_idx = bar_->GetCurrentBarIndex();
  const auto& bar = bar_->GetBarData(bar_->GetCurrentBarType(),
                                     bar_->GetCurrentReferenceTimeframe());

  int64_t next_open_time;
  double next_open;
  try {
    next_open_time = bar.SafeGetBar(symbol_idx, bar_idx + 1).open_time;
    next_open = bar.SafeGetBar(symbol_idx, bar_idx + 1).open;
  } catch (const IndexOutOfRange& e) {
    LogFormattedInfo(
        LogLevel::WARNING_L,
        string(e.what()) + ": 마지막 바이기 때문에 청산할 수 없습니다.",
        __FILE__, __LINE__);
    throw OrderFailed("지정가 청산 주문이 실패했습니다.");
  }

  // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 (주문 수정)
  Cancel(exit_name);

  // 목표 진입 찾기
  const auto& [filled_entry, order_idx] =
      FindMatchingEntryOrder(target_entry_name);
  if (order_idx == -1) {
    // 목표한 진입 이름을 찾지 못하면 경고 로그
    LogFormattedInfo(
        LogLevel::WARNING_L,
        format("지정된 진입명 {}이(가) 존재하지 않아 청산할 수 없습니다.",
               target_entry_name),
        __FILE__, __LINE__);
    throw OrderFailed("지정가 청산 주문이 실패했습니다.");
  }

  // 진입 주문 복사
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
    IsValidPositionSize(exit_size);
    IsValidPrice(order_price);
    IsValidLimitOrderPrice(order_price, next_open,
                           limit_exit->GetExitDirection());
  } catch (const InvalidValue& e) {
    LogFormattedInfo(LogLevel::WARNING_L, e.what(), __FILE__, __LINE__);
    throw OrderFailed("지정가 청산 주문이 실패했습니다.");
  }

  // 대기 중인 청산에 추가
  pending_exits_[symbol_idx].push_back(limit_exit);

  // 디버그 로그 기록
  LogFormattedInfo(LogLevel::DEBUG_L,
                   format("{} | 지정가 {} 체결 대기", order_price, exit_name),
                   __FILE__, __LINE__);
}

void OrderHandler::MitExit(const string& exit_name,
                           const string& target_entry_name,
                           const double exit_size, const double touch_price) {
  // 유효성 검사
  try {
    IsValidPositionSize(exit_size);
    IsValidPrice(touch_price);
  } catch (const InvalidValue& e) {
    LogFormattedInfo(LogLevel::WARNING_L, e.what(), __FILE__, __LINE__);
    throw OrderFailed("MIT 청산 주문이 실패했습니다.");
  }

  // 바 정보 로딩
  const auto symbol_idx = bar_->GetCurrentSymbolIndex();
  double next_open;
  try {
    next_open = bar_->GetBarData(bar_->GetCurrentBarType(),
                                 bar_->GetCurrentReferenceTimeframe())
                    .SafeGetBar(symbol_idx, bar_->GetCurrentBarIndex() + 1)
                    .open;
  } catch (const IndexOutOfRange& e) {
    LogFormattedInfo(
        LogLevel::WARNING_L,
        string(e.what()) + ": 마지막 바이기 때문에 청산할 수 없습니다.",
        __FILE__, __LINE__);
    throw OrderFailed("MIT 청산 주문이 실패했습니다.");
  }

  // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 (주문 수정)
  Cancel(exit_name);

  // 목표 진입 찾기
  const auto& [filled_entry, order_idx] =
      FindMatchingEntryOrder(target_entry_name);
  if (order_idx == -1) {
    // 목표한 진입 이름을 찾지 못하면 경고 로그
    LogFormattedInfo(
        LogLevel::WARNING_L,
        format("지정된 진입명 {}이(가) 존재하지 않아 청산할 수 없습니다.",
               target_entry_name),
        __FILE__, __LINE__);
    throw OrderFailed("MIT 청산 주문이 실패했습니다.");
  }

  // 진입 주문 복사
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
}

void OrderHandler::LitExit(const string& exit_name,
                           const string& target_entry_name,
                           const double exit_size, const double touch_price,
                           const double order_price) {
  // 바 정보 로딩
  const auto symbol_idx = bar_->GetCurrentSymbolIndex();
  double next_open;
  try {
    next_open = bar_->GetBarData(bar_->GetCurrentBarType(),
                                 bar_->GetCurrentReferenceTimeframe())
                    .SafeGetBar(symbol_idx, bar_->GetCurrentBarIndex() + 1)
                    .open;
  } catch (const IndexOutOfRange& e) {
    LogFormattedInfo(
        LogLevel::WARNING_L,
        string(e.what()) + ": 마지막 바이기 때문에 청산할 수 없습니다.",
        __FILE__, __LINE__);
    throw OrderFailed("LIT 청산 주문이 실패했습니다.");
  }

  // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 (주문 수정)
  Cancel(exit_name);

  // 목표 진입 찾기
  const auto& [filled_entry, order_idx] =
      FindMatchingEntryOrder(target_entry_name);
  if (order_idx == -1) {
    // 목표한 진입 이름을 찾지 못하면 경고 로그
    LogFormattedInfo(
        LogLevel::WARNING_L,
        format("지정된 진입명 {}이(가) 존재하지 않아 청산할 수 없습니다.",
               target_entry_name),
        __FILE__, __LINE__);
    throw OrderFailed("LIT 청산 주문이 실패했습니다.");
  }

  // 진입 주문 복사
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
    IsValidPositionSize(exit_size);
    IsValidPrice(touch_price);
    IsValidPrice(order_price);
    IsValidLimitOrderPrice(order_price, touch_price,
                           lit_exit->GetExitDirection());
  } catch (const InvalidValue& e) {
    LogFormattedInfo(LogLevel::WARNING_L, e.what(), __FILE__, __LINE__);
    throw OrderFailed("LIT 청산 주문이 실패했습니다.");
  }

  // 대기 중인 청산에 추가
  pending_exits_[symbol_idx].push_back(lit_exit);

  // 디버그 로그 기록
  LogFormattedInfo(LogLevel::DEBUG_L,
                   format("{} | LIT {} 터치 대기", touch_price, exit_name),
                   __FILE__, __LINE__);
}

void OrderHandler::TrailingExit(const string& exit_name,
                                const string& target_entry_name,
                                const double exit_size,
                                const double touch_price,
                                const double trail_point) {
  // 유효성 검사
  try {
    IsValidPositionSize(exit_size);
    IsValidTrailingTouchPrice(touch_price);
    IsValidTrailPoint(trail_point);
  } catch (const InvalidValue& e) {
    LogFormattedInfo(LogLevel::WARNING_L, e.what(), __FILE__, __LINE__);
    throw OrderFailed("트레일링 청산 주문이 실패했습니다.");
  }

  // 바 정보 로딩
  const auto symbol_idx = bar_->GetCurrentSymbolIndex();
  double next_open;
  try {
    next_open = bar_->GetBarData(bar_->GetCurrentBarType(),
                                 bar_->GetCurrentReferenceTimeframe())
                    .SafeGetBar(symbol_idx, bar_->GetCurrentBarIndex() + 1)
                    .open;
  } catch (const IndexOutOfRange& e) {
    LogFormattedInfo(
        LogLevel::WARNING_L,
        string(e.what()) + ": 마지막 바이기 때문에 청산할 수 없습니다.",
        __FILE__, __LINE__);
    throw OrderFailed("트레일링 청산 주문이 실패했습니다.");
  }

  // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 (주문 수정)
  Cancel(exit_name);

  // 목표 진입 찾기
  const auto& [filled_entry, order_idx] =
      FindMatchingEntryOrder(target_entry_name);
  if (order_idx == -1) {
    // 목표한 진입 이름을 찾지 못하면 경고 로그
    LogFormattedInfo(
        LogLevel::WARNING_L,
        format("지정된 진입명 {}이(가) 존재하지 않아 청산할 수 없습니다.",
               target_entry_name),
        __FILE__, __LINE__);
    throw OrderFailed("트레일링 청산 주문이 실패했습니다.");
  }

  // 진입 주문 복사
  const auto& trailing_exit = make_shared<Order>(*filled_entry);

  // 청산 주문 생성
  trailing_exit->SetExitName(exit_name)
      .SetExitOrderType(OrderType::TRAILING)
      .SetExitDirection(trailing_exit->GetEntryDirection() == Direction::LONG
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
        format("{} | 트레일링 {} 터치 대기", touch_price, exit_name), __FILE__,
        __LINE__);
  } else {
    // touch_price가 0이라 바로 추적 시작한 경우
    LogFormattedInfo(LogLevel::DEBUG_L,
                     format("트레일링 {} 체결 대기", exit_name), __FILE__,
                     __LINE__);
  }
}

void OrderHandler::Cancel(const string& order_name) {
  const int symbol_idx = bar_->GetCurrentSymbolIndex();

  // 진입 대기 주문에서 같은 이름이 존재할 시 삭제
  auto& pending_entries = pending_entries_[symbol_idx];
  for (int order_idx = 0; order_idx < pending_entries.size(); order_idx++) {
    if (const auto& pending_entry = pending_entries[order_idx];
        order_name == pending_entry->GetEntryName()) {
      // 예약 증거금 회복 과정 진행 후 삭제
      ExecuteCancelEntry(pending_entry);
      pending_entries.erase(pending_entries.begin() + order_idx);

      LogFormattedInfo(LogLevel::DEBUG_L,
                       order_name + " 주문이 취소되었습니다.", __FILE__,
                       __LINE__);
      break;  // 동일한 진입 이름으로 진입 대기 불가능하므로 찾으면 바로 break
    }
  }

  // 청산 대기 주문에서 같은 이름이 존재할 시 삭제
  auto& pending_exits = pending_exits_[symbol_idx];
  for (int order_idx = 0; order_idx < pending_exits.size(); order_idx++) {
    if (order_name == pending_exits[order_idx]->GetExitName()) {
      // 청산 주문은 예약 증거금이 필요하지 않기 때문에 삭제만 함
      pending_exits.erase(pending_exits.begin() + order_idx);

      LogFormattedInfo(LogLevel::DEBUG_L,
                       order_name + " 주문이 취소되었습니다.", __FILE__,
                       __LINE__);
      break;  // 동일한 청산 이름으로 청산 대기 불가능하므로 찾으면 바로 break
    }
  }
}

void OrderHandler::ExecuteMarketEntry(const shared_ptr<Order>& market_entry) {
  // 해당 주문과 반대 방향의 체결 주문이 있으면 모두 청산
  ExitOppositeFilledEntries(market_entry->GetEntryDirection());

  // 진입 증거금 계산: 1포인트 == 1달러 가정 계산
  const double entry_margin =
      market_entry->GetEntryFilledSize() * market_entry->GetEntryFilledPrice();

  // 진입 수수료 로딩
  const auto entry_commission = market_entry->GetEntryCommission();

  // 진입 가능 여부 체크 (사용 가능 자금 >= 진입 증거금 + 수수료)
  try {
    HasEnoughBalance(engine_->GetAvailableBalance(),
                     entry_margin + entry_commission);
  } catch (const InsufficientBalance& e) {
    LogFormattedInfo(LogLevel::WARNING_L, e.what(), __FILE__, __LINE__);
    throw OrderFailed("시장가 진입 체결이 실패했습니다.");
  }

  // 지갑 자금에서 수수료 감소
  engine_->DecreaseWalletBalance(entry_commission);

  // 사용한 마진에 진입 증거금 증가
  engine_->IncreaseUsedMargin(entry_margin);

  // 시장가 진입
  filled_entries_[bar_->GetCurrentSymbolIndex()].push_back(market_entry);
}

void OrderHandler::ExitOppositeFilledEntries(const Direction direction) {
  for (const auto& filled_entry :
       filled_entries_[bar_->GetCurrentSymbolIndex()]) {
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
  // 주문 정보 로딩
  const double exit_filled_size = exit_order->GetExitFilledSize();
  const double entry_filled_price = exit_order->GetEntryFilledPrice();

  // 지갑 자금에서 수수료 감소
  if (!engine_->DecreaseWalletBalance(exit_order->GetExitCommission())) {
    engine_->SetBankruptcy();
    throw Bankruptcy("파산");
  }

  // 사용한 마진에서 진입 증거금 감소
  engine_->DecreaseUsedMargin(exit_filled_size * entry_filled_price);

  // 실현 손익 계산
  const double realized_pnl = CalculatePnl(
      exit_order->GetEntryDirection(), exit_order->GetExitFilledPrice(),
      entry_filled_price, exit_filled_size, exit_order->GetLeverage());

  // 지갑 자금에 실현 손익 합산
  if (realized_pnl > 0) {
    engine_->IncreaseWalletBalance(realized_pnl);
  } else if (realized_pnl < 0) {
    if (!engine_->DecreaseWalletBalance(abs(realized_pnl))) {
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
      // 사용한 자금에서 예약 증거금 감소
      engine_->DecreaseUsedMargin(cancel_order->GetEntryOrderSize() *
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
           Touch 이후에는 지정가로 예약 증거금을 사용하므로 사용한 자금에서 예약
           증거금을 감소시켜야 함 */
        engine_->DecreaseUsedMargin(cancel_order->GetEntryOrderSize() *
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

void OrderHandler::CheckPendingLimitEntries(const int order_idx,
                                            const double current_price,
                                            const PriceType price_type) {
  const auto& pending_entry =
      pending_entries_[bar_->GetCurrentSymbolIndex()][order_idx];

  if (const auto order_price = pending_entry->GetEntryOrderPrice();
      IsLimitPriceSatisfied(pending_entry->GetEntryDirection(), current_price,
                            order_price)) {
    try {
      // 시가에서 진입 조건 달성 시 체결 가격은 시가가 됨 (갭 고려)
      FillPendingLimitEntry(order_idx,
                            price_type == OPEN ? current_price : order_price);
    } catch ([[maybe_unused]] const OrderFailed& e) {
      LogFormattedInfo(LogLevel::WARNING_L,
                       "지정가 대기 주문 체결이 실패했습니다.", __FILE__,
                       __LINE__);
    }
  }
}

void OrderHandler::CheckPendingMitEntries(const int order_idx,
                                          const double current_price,
                                          const PriceType price_type) {
  const auto& pending_entry =
      pending_entries_[bar_->GetCurrentSymbolIndex()][order_idx];

  if (const auto touch_price = pending_entry->GetEntryTouchPrice();
      IsPriceTouched(pending_entry->GetEntryTouchDirection(), current_price,
                     touch_price)) {
    try {
      // 시가에서 터치 조건 달성 시 주문 가격은 시가가 됨 (갭 고려)
      FillPendingMarketEntry(order_idx,
                             price_type == OPEN ? current_price : touch_price);
    } catch ([[maybe_unused]] const OrderFailed& e) {
      LogFormattedInfo(LogLevel::WARNING_L,
                       "MIT 대기 주문 체결이 실패했습니다.", __FILE__,
                       __LINE__);
    }
  }
}

void OrderHandler::CheckPendingLitEntries(const int order_idx,
                                          const double current_price,
                                          const PriceType price_type) {
  const auto& pending_entry =
      pending_entries_[bar_->GetCurrentSymbolIndex()][order_idx];

  // Order Time이 설정되지 않았으면 지정가 미주문 -> 터치 확인
  // LIT 터치 확인은 시고저가에서 확인 시 종가에서 확인할 필요 없음
  if (price_type != CLOSE && pending_entry->GetEntryOrderTime() == -1) {
    if (IsPriceTouched(pending_entry->GetEntryTouchDirection(), current_price,
                       pending_entry->GetEntryTouchPrice())) {
      try {
        OrderPendingLitEntry(order_idx);
      } catch (const OrderFailed& e) {
        // 주문 실패 시 해당 주문 삭제 후 다음 주문 확인으로 넘어감
        LogFormattedInfo(LogLevel::WARNING_L, e.what(), __FILE__, __LINE__);
        return;
      }
    }
  }

  // Order Time이 설정됐으면 지정가 주문 완료 -> 체결 확인
  // 터치 후 바로 진입될 수도 있으므로 Order Time을 다시 불러와서 확인
  if (pending_entry->GetEntryOrderTime() != -1) {
    if (const auto order_price = pending_entry->GetEntryOrderPrice();
        IsLimitPriceSatisfied(pending_entry->GetEntryDirection(), current_price,
                              order_price)) {
      try {
        // 시가에서 터치 조건 달성 시 체결 가격은 시가가 됨 (갭 고려)
        FillPendingLimitEntry(order_idx,
                              price_type == OPEN ? current_price : order_price);
      } catch ([[maybe_unused]] const OrderFailed& e) {
        LogFormattedInfo(LogLevel::WARNING_L,
                         "LIT 대기 주문 체결이 실패했습니다.", __FILE__,
                         __LINE__);
      }
    }
  }
}

void OrderHandler::CheckPendingTrailingEntries(const int order_idx,
                                               const double current_price,
                                               const PriceType price_type) {
  const auto& pending_entry =
      pending_entries_[bar_->GetCurrentSymbolIndex()][order_idx];

  // Extreme Price가 지정되지 않았으면 추적 미시작 -> 터치 확인
  // 트레일링 터치 확인은 시고저가에서 확인 시 종가에서 확인할 필요 없음
  if (price_type != CLOSE && isnan(pending_entry->GetEntryExtremePrice())) {
    if (IsPriceTouched(pending_entry->GetEntryTouchDirection(), current_price,
                       pending_entry->GetEntryTouchPrice())) {
      pending_entry->SetEntryExtremePrice(current_price);
    }
  }

  try {
    // Extreme Price가 지정되었으면 추적 시작
    // -> 고저가 업데이트 및 체결 터치 확인
    if (double extreme_price = pending_entry->GetEntryExtremePrice();
        !isnan(extreme_price)) {
      // 주문 정보 로딩
      const auto entry_direction = pending_entry->GetEntryDirection();
      const auto trail_point = pending_entry->GetEntryTrailPoint();

      if (entry_direction == Direction::LONG) {
        // 진입 방향이 매수인 경우, 최저가를 추적
        // 최저가 추적은 시저가에서만 확인하면 됨
        if ((price_type == OPEN || price_type == LOW) &&
            current_price < extreme_price) {
          pending_entry->SetEntryExtremePrice(current_price);
          extreme_price = current_price;
        }

        // 진입 방향이 매수인 경우, 최저가로부터 Trail Point 증가 시 진입
        if (const double trail_price = extreme_price + trail_point;
            current_price >= trail_price) {
          // 시가에서 조건 달성 시 주문 가격은 시가가 됨 (갭 고려)
          FillPendingMarketEntry(
              order_idx, price_type == OPEN ? current_price : trail_price);
        }
      } else if (entry_direction == Direction::SHORT) {
        // 진입 방향이 매도인 경우, 최고가를 추적
        // 최고가 추적은 시고에서만 확인하면 됨
        if ((price_type == OPEN || price_type == HIGH) &&
            current_price > extreme_price) {
          pending_entry->SetEntryExtremePrice(current_price);
          extreme_price = current_price;
        }

        // 진입 방향이 매도인 경우, 최고가로부터 Trail Point 감소 시 진입
        if (const double trail_price = extreme_price - trail_point;
            current_price <= trail_price) {
          // 시가에서 조건 달성 시 주문 가격은 시가가 됨 (갭 고려)
          FillPendingMarketEntry(
              order_idx, price_type == OPEN ? current_price : trail_price);
        }
      }
    }
  } catch ([[maybe_unused]] const OrderFailed& e) {
    LogFormattedInfo(LogLevel::WARNING_L,
                     "트레일링 대기 주문 체결이 실패했습니다.", __FILE__,
                     __LINE__);
  }
}

void OrderHandler::FillPendingMarketEntry(const int order_idx,
                                          const double order_price) {
  // 바 정보 로딩
  const auto symbol_idx = bar_->GetCurrentSymbolIndex();
  const auto current_open_time = engine_->GetCurrentOpenTime();

  // 대기 주문을 받아오고 대기 주문에서 삭제
  const auto& market_entry = pending_entries_[symbol_idx][order_idx];
  pending_entries_[symbol_idx].erase(pending_entries_[symbol_idx].begin() +
                                     order_idx);

  // 예외 체크
  try {
    IsValidEntryName(market_entry->GetEntryName());
  } catch (const InvalidValue& e) {
    LogFormattedInfo(LogLevel::WARNING_L, e.what(), __FILE__, __LINE__);
    throw OrderFailed("대기 주문 체결 실패");
  }

  // 주문 정보 로딩
  const auto filled_size = market_entry->GetEntryOrderSize();
  const auto entry_direction = market_entry->GetEntryDirection();
  const auto leverage = market_entry->GetLeverage();

  // 주문 업데이트
  market_entry->SetEntryOrderTime(current_open_time)
      .SetEntryOrderPrice(order_price)
      .SetEntryFilledTime(current_open_time)
      .SetEntryFilledSize(filled_size);

  // 슬리피지가 포함된 체결가
  const double slippage_filled_price = CalculateSlippagePrice(
      order_price, OrderType::MARKET, entry_direction, leverage);
  market_entry->SetEntryFilledPrice(slippage_filled_price);

  // 수수료
  const double entry_commission = CalculateCommission(
      slippage_filled_price, OrderType::MARKET, filled_size, leverage);
  market_entry->SetEntryCommission(entry_commission);

  // 마진콜 가격
  market_entry->SetMarginCallPrice(CalculateMarginCallPrice(
      slippage_filled_price, entry_direction, leverage));

  // 자금 관련 처리 후 체결 주문에 추가
  try {
    ExecuteMarketEntry(market_entry);
  } catch ([[maybe_unused]] const OrderFailed& e) {
    throw;
  }

  // 디버그 로그 기록
  LogFormattedInfo(
      LogLevel::DEBUG_L,
      format("{} | {} {} 체결", slippage_filled_price,
             market_entry->GetEntryOrderType() == OrderType::MIT ? "MIT"
                                                                 : "트레일링",
             market_entry->GetEntryName()),
      __FILE__, __LINE__);
}

void OrderHandler::FillPendingLimitEntry(const int order_idx,
                                         const double filled_price) {
  // 바 정보 로딩
  const auto symbol_idx = bar_->GetCurrentSymbolIndex();

  // 대기 주문을 받아오고 대기 주문에서 삭제
  const auto& limit_entry = pending_entries_[symbol_idx][order_idx];
  pending_entries_[symbol_idx].erase(pending_entries_[symbol_idx].begin() +
                                     order_idx);

  // 예외 체크
  try {
    IsValidEntryName(limit_entry->GetEntryName());
  } catch (const InvalidValue& e) {
    LogFormattedInfo(LogLevel::WARNING_L, e.what(), __FILE__, __LINE__);
    throw OrderFailed("대기 주문 체결 실패");
  }

  // 주문 정보 로딩
  const auto entry_filled_size = limit_entry->GetEntryOrderSize();
  const auto entry_direction = limit_entry->GetEntryDirection();
  const auto leverage = limit_entry->GetLeverage();

  // 주문 업데이트
  limit_entry->SetEntryFilledTime(engine_->GetCurrentOpenTime())
      .SetEntryFilledSize(entry_filled_size);

  // 슬리피지가 포함된 체결가
  const double slippage_filled_price = CalculateSlippagePrice(
      filled_price, OrderType::LIMIT, entry_direction, leverage);
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

  // 진입 가능 여부 체크 (지갑 자금 >= 수수료)
  try {
    HasEnoughBalance(engine_->GetWalletBalance(), entry_commission);
  } catch (const InsufficientBalance& e) {
    LogFormattedInfo(LogLevel::WARNING_L, e.what(), __FILE__, __LINE__);

    // 주문 실패 시 사용한 마진에서 예약 증거금 감소
    engine_->DecreaseUsedMargin(limit_entry->GetEntryOrderSize() *
                                limit_entry->GetEntryOrderPrice());
    throw OrderFailed("대기 주문 체결 실패");
  }

  // 지갑 자금에서 수수료 감소
  if (!engine_->DecreaseWalletBalance(entry_commission)) {
    engine_->SetBankruptcy();
    throw Bankruptcy("파산");
  }

  // 지정가 진입
  filled_entries_[symbol_idx].push_back(limit_entry);

  // 디버그 로그 기록
  LogFormattedInfo(
      LogLevel::DEBUG_L,
      format("{} | {} {} 체결", slippage_filled_price,
             limit_entry->GetEntryOrderType() == OrderType::LIMIT ? "지정가"
                                                                  : "LIT",
             limit_entry->GetEntryName()),
      __FILE__, __LINE__);
}

void OrderHandler::OrderPendingLitEntry(const int order_idx) {
  // 바 정보 로딩
  const auto symbol_idx = bar_->GetCurrentSymbolIndex();

  // 대기 주문 로딩
  const auto& lit_entry = pending_entries_[symbol_idx][order_idx];

  // 주문 업데이트
  lit_entry->SetEntryOrderTime(engine_->GetCurrentOpenTime());

  // 예약 증거금 계산: 1포인트 == 1달러 가정 계산
  const double entry_margin =
      lit_entry->GetEntryOrderSize() * lit_entry->GetEntryOrderPrice();

  // 주문 가능 여부 체크
  try {
    HasEnoughBalance(engine_->GetAvailableBalance(), entry_margin);
  } catch (const InsufficientBalance& e) {
    LogFormattedInfo(LogLevel::WARNING_L, e.what(), __FILE__, __LINE__);

    // 주문 실패 시 대기 주문에서 삭제
    pending_entries_[symbol_idx].erase(pending_entries_[symbol_idx].begin() +
                                       order_idx);
    throw OrderFailed("LIT 진입 주문이 실패했습니다.");
  }

  // 사용한 마진에 예약 증거금 증가
  engine_->IncreaseUsedMargin(entry_margin);

  // 디버그 로그 기록
  LogFormattedInfo(
      LogLevel::DEBUG_L,
      format("{} | LIT {} 체결 대기", lit_entry->GetEntryOrderPrice(),
             lit_entry->GetEntryName()),
      __FILE__, __LINE__);
}

int OrderHandler::ExecutePendingLimitExit(const int order_idx,
                                          const int64_t open_time,
                                          const double exit_order_price) {
  // 다시 확실히 보고 검사도 다했늦지 체크 @@@@@@@@@@@@@@@@@

  int deleted_count = 0;

  // 바 정보 로딩
  const auto symbol_idx = bar_->GetCurrentSymbolIndex();

  // 대기 주문을 받아오고 대기 주문에서 삭제
  const auto& limit_exit = pending_exits_[symbol_idx][order_idx];
  pending_exits_[symbol_idx].erase(pending_exits_[symbol_idx].begin() +
                                   order_idx);
  deleted_count++;

  // 주문 정보 로딩
  const auto& target_entry_name = limit_exit->GetEntryName();
  const auto exit_filled_size = limit_exit->GetExitOrderSize();
  const auto exit_direction = limit_exit->GetExitDirection();
  const auto leverage = limit_exit->GetLeverage();

  // 주문 업데이트
  limit_exit->SetExitFilledTime(open_time).SetExitFilledSize(exit_filled_size);

  // 슬리피지가 포함된 체결가
  const double slippage_filled_price = CalculateSlippagePrice(
      exit_order_price, OrderType::LIMIT, exit_direction, leverage);
  limit_exit->SetExitFilledPrice(slippage_filled_price);

  // 수수료
  limit_exit->SetExitCommission(
      CalculateCommission(slippage_filled_price, OrderType::LIMIT,
                          exit_filled_size, leverage));

  // 원본 진입 주문 찾기
  const auto& [filled_entry, order_idx] = FindMatchingEntryOrder(target_entry_name);

  // 원본 진입 주문에 청산 체결 수량 추가
  filled_entry->SetExitFilledSize(filled_entry->GetExitFilledSize() +
                                  exit_filled_size);

  /* 원본 진입 주문의 청산 체결 수량이 진입 체결 수량과 같으면
     filled_entries에서 삭제 (부동 소수점 오류 방지용 '>=') */
  if (filled_entry->GetExitFilledSize() >= filled_entry->GetEntryFilledSize()) {
    filled_entries_[symbol_idx].erase(filled_entries_[symbol_idx].begin() +
                                      order_idx);

    // 같은 진입 이름을 목표로 하는 청산 대기 주문 취소
    for (int i = static_cast<int>(pending_exits_[symbol_idx].size()) - 1; i >= 0; i--) {
      if (target_entry_name == pending_exits_[symbol_idx][i]->GetEntryName()) {
        pending_exits_[symbol_idx].erase(pending_exits_[symbol_idx].begin() + i);

        // 이 함수를 호출한 order_idx보다 작을 때만 deleted_count를 증가
        // 같거나 클 때 증가시키면 CheckPendingExits 함수에서 order_idx보다 작은 인덱스의 청산 체결 체크가 누락됨
        if (i < order_idx) {
          deleted_count++;
        }
      }
    }
  }

  // 자금, 통계 업데이트
  ExecuteExit(limit_exit);

  // 디버그 로그 기록
  LogFormattedInfo(
      LogLevel::DEBUG_L,
      format("{} | 지정가 {} 체결", slippage_filled_price, limit_exit->GetExitName()),
             __FILE__, __LINE__);

  return deleted_count;
}

bool OrderHandler::IsLimitPriceSatisfied(const Direction direction,
                                         const double price,
                                         const double order_price) {
  return (direction == Direction::LONG && price <= order_price) ||
       (direction == Direction::SHORT && price >= order_price);
}

bool OrderHandler::IsPriceTouched(const Direction direction, const double price,
                                  const double touch_price) {
  return (direction == Direction::LONG && price >= touch_price) ||
       (direction == Direction::SHORT && price >= touch_price);
}

pair<shared_ptr<Order>, int> OrderHandler::FindMatchingEntryOrder(
    const string& target_entry_name) const {
  // 체결된 진입들 순회
  for (int order_idx = 0; order_idx < filled_entries_[bar_->GetCurrentSymbolIndex()].size(); order_idx++) {
    // target_entry_name과 같은 이름의 진입이 있으면 반환
    if (const auto& filled_entry = filled_entries_[bar_->GetCurrentSymbolIndex()][order_idx];
      filled_entry->GetEntryName() == target_entry_name) {
      return {filled_entry, order_idx};
    }
  }

  // 존재하지 않으면 빈 Order 반환
  return {make_shared<Order>(), -1};
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

void OrderHandler::HasEnoughBalance(const double balance,
                                    const double needed_balance) {
  if (balance < needed_balance) {
    throw InsufficientBalance(
      format("자금 ${}는 필요 자금 ${}보다 많아야합니다.",
             balance, needed_balance));
  }
}
