// 표준 라이브러리
#include <cmath>
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
#include "Engines/Logger.hpp"
#include "Engines/TimeUtils.hpp"
#include "Engines/Trade.hpp"

// 네임 스페이스
using namespace data_utils;
using namespace time_utils;
using enum PriceType;
using enum LogLevel;
using enum OrderType;

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

void OrderHandler::CheckPendingEntries(const double price,
                                       const PriceType price_type,
                                       const int symbol_idx) {
  // 현재 심볼의 진입 대기 주문 로딩
  const auto& pending_entries = pending_entries_[symbol_idx];

  // 체결 성공 혹은 주문/체결 거부된 주문은 진입 대기 주문에서 삭제되므로
  // 역순으로 순회
  for (int order_idx = static_cast<int>(pending_entries.size() - 1);
       order_idx >= 0; order_idx--) {
    switch (pending_entries[order_idx]->GetEntryOrderType()) {
      case MARKET: {
        // 시장가는 대기 주문이 없음
        LogFormattedInfo(WARNING_L, "대기 주문에 시장가 주문이 존재합니다.",
                         __FILE__, __LINE__);
        continue;
      }

      case LIMIT: {
        CheckPendingLimitEntries(symbol_idx, order_idx, price, price_type);
        continue;
      }

      case MIT: {
        CheckPendingMitEntries(symbol_idx, order_idx, price, price_type);
        continue;
      }

      case LIT: {
        CheckPendingLitEntries(symbol_idx, order_idx, price, price_type);
        continue;
      }

      case TRAILING: {
        CheckPendingTrailingEntries(symbol_idx, order_idx, price, price_type);
        continue;
      }

      case NONE: {
        // NONE 타입은 에러
        LogFormattedInfo(WARNING_L, "대기 주문에 NONE 주문이 존재합니다.",
                         __FILE__, __LINE__);
      }
    }
  }
}

void OrderHandler::CheckPendingExits(const double price,
                                     const PriceType price_type,
                                     const int symbol_idx) {
  // 현재 심볼의 청산 대기 주문 로딩
  const auto& pending_exits = pending_exits_[symbol_idx];

  /* 체결된 주문은 청산 대기 주문에서 삭제되므로 역순으로 순회
   - 체결 시 해당 주문뿐 아니라 같은 진입 이름을 목표로 한 청산 대기 주문도 삭제
   - 따라서 체결 과정에서 현재 order_idx보다 작은 인덱스의 주문이 삭제되면,
     삭제된 개수 + 1만큼 order_idx를 감소시켜야 함 */
  int deleted_below_count = 0;
  for (int order_idx = static_cast<int>(pending_exits.size() - 1);
       order_idx >= 0;) {
    switch (const auto& pending_exit = pending_exits[order_idx];
            pending_exit->GetExitOrderType()) {
      case MARKET: {
        // 시장가는 대기 주문이 없음
        LogFormattedInfo(WARNING_L, "대기 주문에 시장가 주문이 존재합니다.",
                         __FILE__, __LINE__);
        break;
      }

      case LIMIT: {
        deleted_below_count =
            CheckPendingLimitExits(symbol_idx, order_idx, price, price_type);
        break;
      }

      case MIT: {
        deleted_below_count =
            CheckPendingMitExits(symbol_idx, order_idx, price, price_type);
        break;
      }

      case LIT: {
        deleted_below_count =
            CheckPendingLitExits(symbol_idx, order_idx, price, price_type);
        break;
      }

      case TRAILING: {
        deleted_below_count =
            CheckPendingTrailingExits(symbol_idx, order_idx, price, price_type);
        break;
      }

      case NONE: {
        // NONE 타입은 에러
        LogFormattedInfo(WARNING_L, "대기 주문에 NONE 주문이 존재합니다.",
                         __FILE__, __LINE__);
      }
    }

    // 현재 인덱스보다 작은 주문이 삭제된 개수 + 1만큼 인덱스를 감소시킴
    // 즉, 삭제된 주문이 없거나, 현재 인덱스 이상이 삭제된 경우 정상적으로 1을
    // 감소시켜 다음 주문을 확인
    order_idx -= deleted_below_count + 1;
  }
}

void OrderHandler::MarketEntry(const string& entry_name,
                               const Direction entry_direction,
                               const double entry_size, const int leverage) {
  try {
    // 바 정보 로딩
    int64_t order_time = 0;
    double order_price = 0.0;
    try {
      // On Close 전략일 시 주문 시간과 주문 가격은 다음 봉의 Open Time과 Open
      if (const auto& strategy_type = engine_->GetCurrentStrategyType();
          strategy_type == "OnClose") {
        const auto& next_bar =
            bar_->GetBarData(bar_->GetCurrentBarType(), "NONE")
                .SafeGetBar(bar_->GetCurrentSymbolIndex(),
                            bar_->GetCurrentBarIndex() + 1);
        order_time = next_bar.open_time;
        order_price = next_bar.open;
      } else {
        // After Entry, After Exit 전략일 시 주문 시간은 현재 바의 Open Time,
        // 주문 가격은 마지막 진입 가격 혹은 마지막 청산 가격

        order_time = engine_->GetCurrentOpenTime();

        if (strategy_type == "AfterEntry") {
          order_price = LastEntryPrice();
        } else if (strategy_type == "AfterExit") {
          order_price = LastExitPrice();
        }
      }
    } catch (const IndexOutOfRange& e) {
      LogFormattedInfo(
          WARNING_L,
          string(e.what()) + ": 마지막 바이기 때문에 진입할 수 없습니다.",
          __FILE__, __LINE__);
      throw OrderFailed("시장가 진입 실패");
    }

    // 유효성 검사
    try {
      IsValidEntryName(entry_name);
      IsValidDirection(entry_direction);
      IsValidPositionSize(entry_size);
      IsValidLeverage(leverage);
    } catch (const InvalidValue& e) {
      LogFormattedInfo(WARNING_L, e.what(), __FILE__, __LINE__);
      throw OrderFailed("시장가 진입 실패");
    }

    // 주문 생성
    const auto& market_entry = make_shared<Order>();
    market_entry->SetLeverage(leverage)
        .SetEntryName(entry_name)
        .SetEntryOrderType(MARKET)
        .SetEntryDirection(entry_direction)
        .SetEntryOrderTime(order_time)
        .SetEntryOrderSize(entry_size)
        .SetEntryOrderPrice(order_price)
        .SetEntryFilledTime(order_time)
        .SetEntryFilledSize(entry_size);

    // 슬리피지가 포함된 체결가
    const double slippage_filled_price =
        CalculateSlippagePrice(order_price, MARKET, entry_direction, leverage);
    market_entry->SetEntryFilledPrice(slippage_filled_price);

    // 수수료
    const double entry_commission = CalculateCommission(
        slippage_filled_price, MARKET, entry_size, leverage);
    market_entry->SetEntryCommission(entry_commission);

    // 마진콜 가격
    market_entry->SetMarginCallPrice(CalculateMarginCallPrice(
        slippage_filled_price, entry_direction, leverage));

    // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 (주문 수정)
    Cancel(entry_name);

    // 자금 관련 처리 후 체결 주문에 추가
    ExecuteMarketEntry(market_entry);

    LogFormattedInfo(ORDER_L,
                     format("시장가 [{}] 체결 | 체결 가격: {}", entry_name,
                            slippage_filled_price),
                     __FILE__, __LINE__);
  } catch ([[maybe_unused]] const OrderFailed& e) {
    LogFormattedInfo(WARNING_L,
                     format("시장가 [{}] 주문이 실패했습니다.", entry_name),
                     __FILE__, __LINE__);
  }
}

void OrderHandler::LimitEntry(const string& entry_name,
                              const Direction entry_direction,
                              const double entry_size, double order_price,
                              const int leverage) {
  try {
    // 바 정보 로딩
    const int symbol_idx = bar_->GetCurrentSymbolIndex();
    int64_t order_time = 0;
    double base_price = 0.0;
    try {
      // On Close 전략일 시 주문 시간과 기준 가격은 다음 봉의 Open Time과 Open
      if (const auto& strategy_type = engine_->GetCurrentStrategyType();
          strategy_type == "OnClose") {
        const auto& next_bar =
            bar_->GetBarData(bar_->GetCurrentBarType(), "NONE")
                .SafeGetBar(symbol_idx, bar_->GetCurrentBarIndex() + 1);
        order_time = next_bar.open_time;
        base_price = next_bar.open;
      } else {
        // After Entry 혹은 After Exit 전략일 시 주문 시간은 현재 바의 Open
        // Time, 기준 가격은 마지막 진입 가격 혹은 마지막 청산 가격

        order_time = engine_->GetCurrentOpenTime();

        if (strategy_type == "AfterEntry") {
          base_price = LastEntryPrice();
        } else if (strategy_type == "AfterExit") {
          base_price = LastExitPrice();
        }
      }
    } catch (const IndexOutOfRange& e) {
      LogFormattedInfo(
          WARNING_L,
          string(e.what()) + ": 마지막 바이기 때문에 진입할 수 없습니다.",
          __FILE__, __LINE__);
      throw OrderFailed("지정가 진입 실패");
    }

    // 주문 가격을 가격의 최대 소숫점 자리수로 반올림
    order_price = RoundToDecimalPlaces(order_price,
                                       engine_->GetMaxDecimalPlace(symbol_idx));

    // 유효성 검사
    try {
      IsValidDirection(entry_direction);
      IsValidPositionSize(entry_size);
      IsValidLeverage(leverage);
      IsValidPrice(order_price);
      IsValidLimitOrderPrice(order_price, base_price, entry_direction);
    } catch (const InvalidValue& e) {
      LogFormattedInfo(WARNING_L, e.what(), __FILE__, __LINE__);
      throw OrderFailed("지정가 진입 실패");
    }

    // 주문 생성
    const auto& limit_entry = make_shared<Order>();
    limit_entry->SetLeverage(leverage)
        .SetEntryName(entry_name)
        .SetEntryOrderType(LIMIT)
        .SetEntryDirection(entry_direction)
        .SetEntryOrderTime(order_time)
        .SetEntryOrderSize(entry_size)
        .SetEntryOrderPrice(order_price);

    // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 (주문 수정)
    Cancel(entry_name);

    // 예약 증거금 계산: 1포인트 == 1달러 가정 계산
    const double entry_margin = entry_size * order_price;

    // 주문 가능 여부 체크
    try {
      HasEnoughBalance(engine_->UpdateAvailableBalance(), entry_margin,
                       "사용 가능", "지정가 주문 마진");
    } catch (const InsufficientBalance& e) {
      LogFormattedInfo(WARNING_L, e.what(), __FILE__, __LINE__);
      throw OrderFailed("지정가 진입 실패");
    }

    // 사용한 마진에 예약 증거금 증가
    engine_->IncreaseUsedMargin(entry_margin);

    // 지정가 진입 대기
    pending_entries_[symbol_idx].push_back(limit_entry);

    // 디버그 로그 기록
    LogFormattedInfo(ORDER_L,
                     format("지정가 [{}] 체결 대기 | 주문 가격: {}", entry_name,
                            order_price),
                     __FILE__, __LINE__);
  } catch ([[maybe_unused]] const OrderFailed& e) {
    LogFormattedInfo(WARNING_L,
                     format("지정가 [{}] 주문이 실패했습니다.", entry_name),
                     __FILE__, __LINE__);
  }
}

void OrderHandler::MitEntry(const string& entry_name,
                            const Direction entry_direction,
                            const double entry_size, double touch_price,
                            const int leverage) {
  try {
    // 바 정보 로딩
    const int symbol_idx = bar_->GetCurrentSymbolIndex();
    double base_price = 0.0;
    try {
      // On Close 전략일 시 기준 가격은 다음 봉의 Open
      if (const auto& strategy_type = engine_->GetCurrentStrategyType();
          strategy_type == "OnClose") {
        base_price = bar_->GetBarData(bar_->GetCurrentBarType(), "NONE")
                         .SafeGetBar(symbol_idx, bar_->GetCurrentBarIndex() + 1)
                         .open;
      } else {
        // After Entry 혹은 After Exit 전략일 시
        // 기준 가격은 마지막 진입 가격 혹은 마지막 청산 가격
        if (strategy_type == "AfterEntry") {
          base_price = LastEntryPrice();
        } else if (strategy_type == "AfterExit") {
          base_price = LastExitPrice();
        }
      }
    } catch (const IndexOutOfRange& e) {
      LogFormattedInfo(
          WARNING_L,
          string(e.what()) + ": 마지막 바이기 때문에 진입할 수 없습니다.",
          __FILE__, __LINE__);
      throw OrderFailed("MIT 진입 실패");
    }

    // 터치 가격을 가격의 최대 소숫점 자리수로 반올림
    touch_price = RoundToDecimalPlaces(touch_price,
                                       engine_->GetMaxDecimalPlace(symbol_idx));

    // 유효성 검사
    try {
      IsValidDirection(entry_direction);
      IsValidPositionSize(entry_size);
      IsValidLeverage(leverage);
      IsValidPrice(touch_price);
    } catch (const InvalidValue& e) {
      LogFormattedInfo(WARNING_L, e.what(), __FILE__, __LINE__);
      throw OrderFailed("MIT 진입 실패");
    }

    // 주문 생성
    const auto& mit_entry = make_shared<Order>();
    mit_entry->SetLeverage(leverage)
        .SetEntryName(entry_name)
        .SetEntryOrderType(MIT)
        .SetEntryDirection(entry_direction)
        .SetEntryTouchPrice(touch_price)
        .SetEntryTouchDirection(IsGreaterOrEqual(touch_price, base_price)
                                    ? Direction::LONG
                                    : Direction::SHORT)
        .SetEntryOrderSize(entry_size);

    // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 (주문 수정)
    Cancel(entry_name);

    // MIT 진입 터치 대기
    pending_entries_[symbol_idx].push_back(mit_entry);

    // 디버그 로그 기록
    LogFormattedInfo(
        ORDER_L,
        format("MIT [{}] 터치 대기 | 터치 가격: {}", entry_name, touch_price),
        __FILE__, __LINE__);
  } catch ([[maybe_unused]] const OrderFailed& e) {
    LogFormattedInfo(WARNING_L,
                     format("MIT [{}] 주문이 실패했습니다.", entry_name),
                     __FILE__, __LINE__);
  }
}

void OrderHandler::LitEntry(const string& entry_name,
                            const Direction entry_direction,
                            const double entry_size, double touch_price,
                            double order_price, const int leverage) {
  try {
    // 바 정보 로딩
    const auto symbol_idx = bar_->GetCurrentSymbolIndex();
    double base_price = 0.0;
    try {
      // On Close 전략일 시 기준 가격은 다음 봉의 Open
      if (const auto& strategy_type = engine_->GetCurrentStrategyType();
          strategy_type == "OnClose") {
        base_price = bar_->GetBarData(bar_->GetCurrentBarType(), "NONE")
                         .SafeGetBar(symbol_idx, bar_->GetCurrentBarIndex() + 1)
                         .open;
      } else {
        // After Entry 혹은 After Exit 전략일 시
        // 기준 가격은 마지막 진입 가격 혹은 마지막 청산 가격
        if (strategy_type == "AfterEntry") {
          base_price = LastEntryPrice();
        } else if (strategy_type == "AfterExit") {
          base_price = LastExitPrice();
        }
      }
    } catch (const IndexOutOfRange& e) {
      LogFormattedInfo(
          WARNING_L,
          string(e.what()) + ": 마지막 바이기 때문에 진입할 수 없습니다.",
          __FILE__, __LINE__);
      throw OrderFailed("LIT 진입 실패");
    }

    // 터치 가격과 주문 가격을 가격의 최대 소숫점 자리수로 반올림
    const auto decimal_place = engine_->GetMaxDecimalPlace(symbol_idx);
    touch_price = RoundToDecimalPlaces(touch_price, decimal_place);
    order_price = RoundToDecimalPlaces(order_price, decimal_place);

    // 유효성 검사
    try {
      IsValidDirection(entry_direction);
      IsValidPositionSize(entry_size);
      IsValidLeverage(leverage);
      IsValidPrice(touch_price);
      IsValidPrice(order_price);
      IsValidLimitOrderPrice(order_price, touch_price, entry_direction);
    } catch (const InvalidValue& e) {
      LogFormattedInfo(WARNING_L, e.what(), __FILE__, __LINE__);
      throw OrderFailed("MIT 진입 실패");
    }
    // 주문 생성
    const auto& lit_entry = make_shared<Order>();
    lit_entry->SetLeverage(leverage)
        .SetEntryName(entry_name)
        .SetEntryOrderType(LIT)
        .SetEntryDirection(entry_direction)
        .SetEntryTouchPrice(touch_price)
        .SetEntryTouchDirection(IsGreaterOrEqual(touch_price, base_price)
                                    ? Direction::LONG
                                    : Direction::SHORT)
        .SetEntryOrderSize(entry_size)
        .SetEntryOrderPrice(order_price);

    // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 (주문 수정)
    Cancel(entry_name);

    // LIT 진입 터치 대기
    pending_entries_[symbol_idx].push_back(lit_entry);

    // 디버그 로그 기록
    LogFormattedInfo(
        ORDER_L,
        format("LIT [{}] 터치 대기 | 터치 가격: {}", entry_name, touch_price),
        __FILE__, __LINE__);
  } catch ([[maybe_unused]] const OrderFailed& e) {
    LogFormattedInfo(WARNING_L,
                     format("LIT [{}] 주문이 실패했습니다.", entry_name),
                     __FILE__, __LINE__);
  }
}

void OrderHandler::TrailingEntry(const string& entry_name,
                                 const Direction entry_direction,
                                 const double entry_size, double touch_price,
                                 const double trail_point, const int leverage) {
  try {
    // 바 정보 로딩
    const int symbol_idx = bar_->GetCurrentSymbolIndex();
    double base_price = 0.0;
    try {
      // On Close 전략일 시 기준 가격은 다음 봉의 Open
      if (const auto& strategy_type = engine_->GetCurrentStrategyType();
          strategy_type == "OnClose") {
        base_price = bar_->GetBarData(bar_->GetCurrentBarType(), "NONE")
                         .SafeGetBar(symbol_idx, bar_->GetCurrentBarIndex() + 1)
                         .open;
      } else {
        // After Entry 혹은 After Exit 전략일 시
        // 기준 가격은 마지막 진입 가격 혹은 마지막 청산 가격
        if (strategy_type == "AfterEntry") {
          base_price = LastEntryPrice();
        } else if (strategy_type == "AfterExit") {
          base_price = LastExitPrice();
        }
      }
    } catch (const IndexOutOfRange& e) {
      LogFormattedInfo(
          WARNING_L,
          string(e.what()) + ": 마지막 바이기 때문에 진입할 수 없습니다.",
          __FILE__, __LINE__);
      throw OrderFailed("트레일링 진입 실패");
    }

    // 터치 가격을 가격의 최대 소숫점 자리수로 반올림
    touch_price = RoundToDecimalPlaces(touch_price,
                                       engine_->GetMaxDecimalPlace(symbol_idx));

    // 유효성 검사
    try {
      IsValidDirection(entry_direction);
      IsValidPositionSize(entry_size);
      IsValidLeverage(leverage);
      IsValidTrailingTouchPrice(touch_price);
      IsValidTrailPoint(trail_point);
    } catch (const InvalidValue& e) {
      LogFormattedInfo(WARNING_L, e.what(), __FILE__, __LINE__);
      throw OrderFailed("트레일링 진입 실패");
    }

    // 주문 생성
    const auto& trailing_entry = make_shared<Order>();
    trailing_entry->SetLeverage(leverage)
        .SetEntryName(entry_name)
        .SetEntryOrderType(TRAILING)
        .SetEntryDirection(entry_direction)
        .SetEntryTouchPrice(touch_price)
        .SetEntryTouchDirection(IsGreaterOrEqual(touch_price, base_price)
                                    ? Direction::LONG
                                    : Direction::SHORT)
        .SetEntryTrailPoint(trail_point)
        .SetEntryOrderSize(entry_size);

    // touch_price가 0이면 다음 시가부터 최고저가 추적을 시작
    if (IsEqual(touch_price, 0.0)) {
      trailing_entry->SetEntryExtremePrice(base_price);
    }

    // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 (주문 수정)
    Cancel(entry_name);

    // 트레일링 진입 터치 대기
    pending_entries_[symbol_idx].push_back(trailing_entry);

    // 디버그 로그 기록
    if (!IsEqual(touch_price, 0.0)) {
      LogFormattedInfo(ORDER_L,
                       format("트레일링 [{}] 터치 대기 | 터치 가격: {}",
                              entry_name, touch_price),
                       __FILE__, __LINE__);
    } else {
      // touch_price가 0이라 바로 추적 시작한 경우
      LogFormattedInfo(ORDER_L, format("트레일링 [{}] 체결 대기", entry_name),
                       __FILE__, __LINE__);
    }
  } catch ([[maybe_unused]] const OrderFailed& e) {
    LogFormattedInfo(WARNING_L,
                     format("트레일링 [{}] 주문이 실패했습니다.", entry_name),
                     __FILE__, __LINE__);
  }
}

void OrderHandler::MarketExit(const string& exit_name,
                              const string& target_entry_name,
                              const double exit_size) {
  try {
    // 바 정보 로딩
    const auto symbol_idx = bar_->GetCurrentSymbolIndex();
    int64_t order_time = 0;
    double order_price = 0.0;
    try {
      // On Close 전략일 시 주문 시간과 주문 가격은 다음 봉의 Open Time과 Open
      if (const auto& strategy_type = engine_->GetCurrentStrategyType();
          strategy_type == "OnClose") {
        const auto& next_bar =
            bar_->GetBarData(bar_->GetCurrentBarType(), "NONE")
                .SafeGetBar(bar_->GetCurrentSymbolIndex(),
                            bar_->GetCurrentBarIndex() + 1);
        order_time = next_bar.open_time;
        order_price = next_bar.open;
      } else {
        // After Entry 혹은 After Exit 전략일 시 주문 시간은 현재 바의 Open
        // Time, 주문 가격은 마지막 진입 가격 혹은 마지막 청산 가격

        order_time = engine_->GetCurrentOpenTime();

        if (strategy_type == "AfterEntry") {
          order_price = LastEntryPrice();
        } else if (strategy_type == "AfterExit") {
          order_price = LastExitPrice();
        }
      }
    } catch ([[maybe_unused]] const IndexOutOfRange& e) {
      // 마지막 바인 경우 현재 바 종가에 청산
      const auto& current_bar =
          bar_->GetBarData(bar_->GetCurrentBarType(), "NONE")
              .GetBar(bar_->GetCurrentSymbolIndex(),
                      bar_->GetCurrentBarIndex());
      order_time = current_bar.open_time;
      order_price = current_bar.close;
    }

    // 유효성 검사
    try {
      IsValidPositionSize(exit_size);
    } catch (const InvalidValue& e) {
      LogFormattedInfo(WARNING_L, e.what(), __FILE__, __LINE__);
      throw OrderFailed("시장가 청산 실패");
    }

    // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 (주문 수정)
    Cancel(exit_name);

    // 원본 진입 주문 찾기
    shared_ptr<Order> entry_order;
    int entry_order_idx;
    try {
      const auto [order, index] = FindMatchingEntryOrder(target_entry_name);
      entry_order = order;
      entry_order_idx = index;
    } catch ([[maybe_unused]] const EntryOrderNotFound& e) {
      // 원본 진입 주문을 찾지 못하면 청산 실패
      throw OrderFailed("시장가 청산 실패");
    }

    // 진입 주문 복사
    const auto& market_exit = make_shared<Order>(*entry_order);

    // 총 청산 체결 수량이 진입 체결 수량보다 크지 않게 조정
    const double exit_filled_size =
        GetAdjustedExitFilledSize(exit_size, entry_order);

    // 청산 주문 생성
    market_exit->SetExitName(exit_name)
        .SetExitOrderType(MARKET)
        .SetExitDirection(market_exit->GetEntryDirection() == Direction::LONG
                              ? Direction::SHORT
                              : Direction::LONG)
        .SetExitOrderTime(order_time)
        .SetExitOrderSize(exit_filled_size)
        .SetExitOrderPrice(order_price)
        .SetExitFilledTime(order_time)
        .SetExitFilledSize(exit_filled_size);

    // 주문 정보 로딩
    const auto leverage = market_exit->GetLeverage();

    // 슬리피지가 포함된 체결가
    const double slippage_filled_price = CalculateSlippagePrice(
        order_price, MARKET, market_exit->GetExitDirection(), leverage);
    market_exit->SetExitFilledPrice(slippage_filled_price);

    // 수수료
    market_exit->SetExitCommission(CalculateCommission(
        slippage_filled_price, MARKET, exit_filled_size, leverage));

    // 원본 진입 주문에 청산 체결 수량 추가
    const double total_exit_filled_size =
        entry_order->GetExitFilledSize() + exit_filled_size;
    entry_order->SetExitFilledSize(total_exit_filled_size);

    // 원본 진입 주문의 청산 체결 수량이 진입 체결 수량과 같으면
    // filled_entries에서 삭제
    if (IsEqual(total_exit_filled_size, entry_order->GetEntryFilledSize())) {
      auto& filled_entries = filled_entries_[symbol_idx];
      filled_entries.erase(filled_entries.begin() + entry_order_idx);

      // 같은 진입 이름을 목표로 하는 청산 대기 주문 취소
      erase_if(pending_exits_[symbol_idx],
               [&](const shared_ptr<Order>& pending_exit) {
                 return target_entry_name == pending_exit->GetEntryName();
               });
    }

    // 자금, 통계 업데이트
    ExecuteExit(market_exit);

    // 디버그 로그 기록
    LogFormattedInfo(ORDER_L,
                     format("시장가 [{}] 체결 | 체결 가격: {}", exit_name,
                            slippage_filled_price),
                     __FILE__, __LINE__);
  } catch ([[maybe_unused]] const OrderFailed& e) {
    LogFormattedInfo(WARNING_L,
                     format("시장가 [{}] 주문이 실패했습니다.", exit_name),
                     __FILE__, __LINE__);
  }
}

void OrderHandler::LimitExit(const string& exit_name,
                             const string& target_entry_name,
                             const double exit_size, double order_price) {
  try {
    // 바 정보 로딩
    const auto symbol_idx = bar_->GetCurrentSymbolIndex();
    int64_t order_time;
    double base_price = 0.0;
    try {
      // On Close 전략일 시 주문 시간과 기준 가격은 다음 봉의 Open Time과 Open
      if (const auto& strategy_type = engine_->GetCurrentStrategyType();
          strategy_type == "OnClose") {
        const auto& next_bar =
            bar_->GetBarData(bar_->GetCurrentBarType(), "NONE")
                .SafeGetBar(symbol_idx, bar_->GetCurrentBarIndex() + 1);
        order_time = next_bar.open_time;
        base_price = next_bar.open;
      } else {
        // After Entry 혹은 After Exit 전략일 시 주문 시간은 현재 바의 Open
        // Time, 기준 가격은 마지막 진입 가격 혹은 마지막 청산 가격

        order_time = engine_->GetCurrentOpenTime();

        if (strategy_type == "AfterEntry") {
          base_price = LastEntryPrice();
        } else if (strategy_type == "AfterExit") {
          base_price = LastExitPrice();
        }
      }
    } catch (const IndexOutOfRange& e) {
      LogFormattedInfo(
          WARNING_L,
          string(e.what()) + ": 마지막 바이기 때문에 청산할 수 없습니다.",
          __FILE__, __LINE__);
      throw OrderFailed("지정가 청산 실패");
    }

    // 주문 가격을 가격의 최대 소숫점 자리수로 반올림
    order_price = RoundToDecimalPlaces(order_price,
                                       engine_->GetMaxDecimalPlace(symbol_idx));

    // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 (주문 수정)
    Cancel(exit_name);

    // 원본 진입 주문 찾기
    shared_ptr<Order> entry_order;
    try {
      entry_order = FindMatchingEntryOrder(target_entry_name).first;
    } catch ([[maybe_unused]] const EntryOrderNotFound& e) {
      // 원본 진입 주문을 찾지 못하면 청산 실패
      throw OrderFailed("지정가 청산 실패");
    }

    // 진입 주문 복사
    const auto& limit_exit = make_shared<Order>(*entry_order);

    // 청산 주문 생성
    limit_exit->SetExitName(exit_name)
        .SetExitOrderType(LIMIT)
        .SetExitDirection(limit_exit->GetEntryDirection() == Direction::LONG
                              ? Direction::SHORT
                              : Direction::LONG)
        .SetExitOrderTime(order_time)
        .SetExitOrderSize(exit_size)
        .SetExitOrderPrice(order_price);

    // 유효성 검사
    try {
      IsValidPositionSize(exit_size);
      IsValidPrice(order_price);
      IsValidLimitOrderPrice(order_price, base_price,
                             limit_exit->GetExitDirection());
    } catch (const InvalidValue& e) {
      LogFormattedInfo(WARNING_L, e.what(), __FILE__, __LINE__);
      throw OrderFailed("지정가 청산 실패");
    }

    // 대기 중인 청산에 추가
    pending_exits_[symbol_idx].push_back(limit_exit);

    // 디버그 로그 기록
    LogFormattedInfo(
        ORDER_L,
        format("지정가 [{}] 체결 대기 | 주문 가격: {}", exit_name, order_price),
        __FILE__, __LINE__);
  } catch ([[maybe_unused]] const OrderFailed& e) {
    LogFormattedInfo(WARNING_L,
                     format("지정가 [{}] 주문이 실패했습니다.", exit_name),
                     __FILE__, __LINE__);
  }
}

void OrderHandler::MitExit(const string& exit_name,
                           const string& target_entry_name,
                           const double exit_size, double touch_price) {
  try {
    // 바 정보 로딩
    const auto symbol_idx = bar_->GetCurrentSymbolIndex();
    double base_price = 0.0;
    try {
      // On Close 전략일 시 기준 가격은 다음 봉의 Open
      if (const auto& strategy_type = engine_->GetCurrentStrategyType();
          strategy_type == "OnClose") {
        const auto& next_bar =
            bar_->GetBarData(bar_->GetCurrentBarType(), "NONE")
                .SafeGetBar(symbol_idx, bar_->GetCurrentBarIndex() + 1);
        base_price = next_bar.open;
      } else {
        // After Entry 혹은 After Exit 전략일 시
        // 기준 가격은 마지막 진입 가격 혹은 마지막 청산 가격

        if (strategy_type == "AfterEntry") {
          base_price = LastEntryPrice();
        } else if (strategy_type == "AfterExit") {
          base_price = LastExitPrice();
        }
      }
    } catch (const IndexOutOfRange& e) {
      LogFormattedInfo(
          WARNING_L,
          string(e.what()) + ": 마지막 바이기 때문에 청산할 수 없습니다.",
          __FILE__, __LINE__);
      throw OrderFailed("MIT 청산 실패");
    }

    // 터치 가격을 가격의 최대 소숫점 자리수로 반올림
    touch_price = RoundToDecimalPlaces(touch_price,
                                       engine_->GetMaxDecimalPlace(symbol_idx));

    // 유효성 검사
    try {
      IsValidPositionSize(exit_size);
      IsValidPrice(touch_price);
    } catch (const InvalidValue& e) {
      LogFormattedInfo(WARNING_L, e.what(), __FILE__, __LINE__);
      throw OrderFailed("MIT 청산 실패");
    }

    // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 (주문 수정)
    Cancel(exit_name);

    // 원본 진입 주문 찾기
    shared_ptr<Order> entry_order;
    try {
      entry_order = FindMatchingEntryOrder(target_entry_name).first;
    } catch ([[maybe_unused]] const EntryOrderNotFound& e) {
      // 원본 진입 주문을 찾지 못하면 청산 실패
      throw OrderFailed("MIT 청산 실패");
    }

    // 진입 주문 복사
    const auto& mit_exit = make_shared<Order>(*entry_order);

    // 청산 주문 생성
    mit_exit->SetExitName(exit_name)
        .SetExitOrderType(MIT)
        .SetExitDirection(mit_exit->GetEntryDirection() == Direction::LONG
                              ? Direction::SHORT
                              : Direction::LONG)
        .SetExitTouchPrice(touch_price)
        .SetExitTouchDirection(IsGreaterOrEqual(touch_price, base_price)
                                   ? Direction::LONG
                                   : Direction::SHORT)
        .SetExitOrderSize(exit_size);

    // 대기 중인 청산에 추가
    pending_exits_[symbol_idx].push_back(mit_exit);

    // 디버그 로그 기록
    LogFormattedInfo(
        ORDER_L,
        format("MIT [{}] 터치 대기 | 터치 가격: {}", exit_name, touch_price),
        __FILE__, __LINE__);
  } catch ([[maybe_unused]] const OrderFailed& e) {
    LogFormattedInfo(WARNING_L,
                     format("MIT [{}] 주문이 실패했습니다.", exit_name),
                     __FILE__, __LINE__);
  }
}

void OrderHandler::LitExit(const string& exit_name,
                           const string& target_entry_name,
                           const double exit_size, double touch_price,
                           double order_price) {
  try {
    // 바 정보 로딩
    const auto symbol_idx = bar_->GetCurrentSymbolIndex();
    double base_price = 0.0;
    try {
      // On Close 전략일 시 기준 가격은 다음 봉의 Open
      if (const auto& strategy_type = engine_->GetCurrentStrategyType();
          strategy_type == "OnClose") {
        const auto& next_bar =
            bar_->GetBarData(bar_->GetCurrentBarType(), "NONE")
                .SafeGetBar(symbol_idx, bar_->GetCurrentBarIndex() + 1);
        base_price = next_bar.open;
      } else {
        // After Entry 혹은 After Exit 전략일 시
        // 기준 가격은 마지막 진입 가격 혹은 마지막 청산 가격

        if (strategy_type == "AfterEntry") {
          base_price = LastEntryPrice();
        } else if (strategy_type == "AfterExit") {
          base_price = LastExitPrice();
        }
      }
    } catch (const IndexOutOfRange& e) {
      LogFormattedInfo(
          WARNING_L,
          string(e.what()) + ": 마지막 바이기 때문에 청산할 수 없습니다.",
          __FILE__, __LINE__);
      throw OrderFailed("LIT 청산 실패");
    }

    // 터치 가격과 주문 가격을 가격의 최대 소숫점 자리수로 반올림
    const auto decimal_place = engine_->GetMaxDecimalPlace(symbol_idx);
    touch_price = RoundToDecimalPlaces(touch_price, decimal_place);
    order_price = RoundToDecimalPlaces(order_price, decimal_place);

    // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 (주문 수정)
    Cancel(exit_name);

    // 원본 진입 주문 찾기
    shared_ptr<Order> entry_order;
    try {
      entry_order = FindMatchingEntryOrder(target_entry_name).first;
    } catch ([[maybe_unused]] const EntryOrderNotFound& e) {
      // 원본 진입 주문을 찾지 못하면 청산 실패
      throw OrderFailed("LIT 청산 실패");
    }

    // 진입 주문 복사
    const auto& lit_exit = make_shared<Order>(*entry_order);

    // 청산 주문 생성
    lit_exit->SetExitName(exit_name)
        .SetExitOrderType(LIT)
        .SetExitDirection(lit_exit->GetEntryDirection() == Direction::LONG
                              ? Direction::SHORT
                              : Direction::LONG)
        .SetExitTouchPrice(touch_price)
        .SetExitTouchDirection(IsGreaterOrEqual(touch_price, base_price)
                                   ? Direction::LONG
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
      LogFormattedInfo(WARNING_L, e.what(), __FILE__, __LINE__);
      throw OrderFailed("LIT 청산 실패");
    }

    // 대기 중인 청산에 추가
    pending_exits_[symbol_idx].push_back(lit_exit);

    // 디버그 로그 기록
    LogFormattedInfo(
        ORDER_L,
        format("LIT [{}] 터치 대기 | 터치 가격: {}", exit_name, touch_price),
        __FILE__, __LINE__);
  } catch ([[maybe_unused]] const OrderFailed& e) {
    LogFormattedInfo(WARNING_L,
                     format("LIT [{}] 주문이 실패했습니다.", exit_name),
                     __FILE__, __LINE__);
  }
}

void OrderHandler::TrailingExit(const string& exit_name,
                                const string& target_entry_name,
                                const double exit_size, double touch_price,
                                const double trail_point) {
  try {
    // 바 정보 로딩
    const auto symbol_idx = bar_->GetCurrentSymbolIndex();
    double base_price = 0.0;
    try {
      // On Close 전략일 시 기준 가격은 다음 봉의 Open
      if (const auto& strategy_type = engine_->GetCurrentStrategyType();
          strategy_type == "OnClose") {
        const auto& next_bar =
            bar_->GetBarData(bar_->GetCurrentBarType(), "NONE")
                .SafeGetBar(symbol_idx, bar_->GetCurrentBarIndex() + 1);
        base_price = next_bar.open;
      } else {
        // After Entry 혹은 After Exit 전략일 시
        // 기준 가격은 마지막 진입 가격 혹은 마지막 청산 가격

        if (strategy_type == "AfterEntry") {
          base_price = LastEntryPrice();
        } else if (strategy_type == "AfterExit") {
          base_price = LastExitPrice();
        }
      }
    } catch (const IndexOutOfRange& e) {
      LogFormattedInfo(
          WARNING_L,
          string(e.what()) + ": 마지막 바이기 때문에 청산할 수 없습니다.",
          __FILE__, __LINE__);
      throw OrderFailed("트레일링 청산 실패");
    }

    // 터치 가격을 가격의 최대 소숫점 자리수로 반올림
    touch_price = RoundToDecimalPlaces(touch_price,
                                       engine_->GetMaxDecimalPlace(symbol_idx));

    // 유효성 검사
    try {
      IsValidPositionSize(exit_size);
      IsValidTrailingTouchPrice(touch_price);
      IsValidTrailPoint(trail_point);
    } catch (const InvalidValue& e) {
      LogFormattedInfo(WARNING_L, e.what(), __FILE__, __LINE__);
      throw OrderFailed("트레일링 청산 실패");
    }

    // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 (주문 수정)
    Cancel(exit_name);

    // 원본 진입 주문 찾기
    shared_ptr<Order> entry_order;
    try {
      entry_order = FindMatchingEntryOrder(target_entry_name).first;
    } catch ([[maybe_unused]] const EntryOrderNotFound& e) {
      // 원본 진입 주문을 찾지 못하면 청산 실패
      throw OrderFailed("트레일링 청산 실패");
    }

    // 진입 주문 복사
    const auto& trailing_exit = make_shared<Order>(*entry_order);

    // 청산 주문 생성
    trailing_exit->SetExitName(exit_name)
        .SetExitOrderType(TRAILING)
        .SetExitDirection(trailing_exit->GetEntryDirection() == Direction::LONG
                              ? Direction::SHORT
                              : Direction::LONG)
        .SetExitTouchPrice(touch_price)
        .SetExitTouchDirection(IsGreaterOrEqual(touch_price, base_price)
                                   ? Direction::LONG
                                   : Direction::SHORT)
        .SetExitTrailPoint(trail_point)
        .SetExitOrderSize(exit_size);

    // touch_price가 0이면 다음 시가부터 최고저가 추적을 시작
    if (IsEqual(touch_price, 0.0)) {
      trailing_exit->SetEntryExtremePrice(base_price);
    }

    // 대기 중인 청산에 추가
    pending_exits_[symbol_idx].push_back(trailing_exit);

    // 디버그 로그 기록
    if (!IsEqual(touch_price, 0.0)) {
      LogFormattedInfo(ORDER_L,
                       format("트레일링 [{}] 터치 대기 | 터치 가격: {}",
                              exit_name, touch_price),
                       __FILE__, __LINE__);
    } else {
      // touch_price가 0이라 바로 추적 시작한 경우
      LogFormattedInfo(ORDER_L, format("트레일링 [{}] 체결 대기", exit_name),
                       __FILE__, __LINE__);
    }
  } catch ([[maybe_unused]] const OrderFailed& e) {
    LogFormattedInfo(WARNING_L,
                     format("트레일링 [{}] 주문이 실패했습니다.", exit_name),
                     __FILE__, __LINE__);
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

      LogFormattedInfo(ORDER_L,
                       format("[{}] 주문이 취소되었습니다.", order_name),
                       __FILE__, __LINE__);
      break;  // 동일한 진입 이름으로 진입 대기 불가능하므로 찾으면 바로 break
    }
  }

  // 청산 대기 주문에서 같은 이름이 존재할 시 삭제
  auto& pending_exits = pending_exits_[symbol_idx];
  for (int order_idx = 0; order_idx < pending_exits.size(); order_idx++) {
    if (order_name == pending_exits[order_idx]->GetExitName()) {
      // 청산 주문은 예약 증거금이 필요하지 않기 때문에 삭제만 함
      pending_exits.erase(pending_exits.begin() + order_idx);

      LogFormattedInfo(ORDER_L,
                       format("[{}] 주문이 취소되었습니다.", order_name),
                       __FILE__, __LINE__);
      break;  // 동일한 청산 이름으로 청산 대기 불가능하므로 찾으면 바로 break
    }
  }
}

void OrderHandler::CloseAllPositions() {
  for (const auto& filled_entry :
       filled_entries_[bar_->GetCurrentSymbolIndex()]) {
    MarketExit(
        "종료 청산", filled_entry->GetEntryName(),
        filled_entry->GetEntryFilledSize() - filled_entry->GetExitFilledSize());
  }
}

void OrderHandler::ExecuteMarketEntry(const shared_ptr<Order>& market_entry) {
  // 해당 주문과 반대 방향의 체결 주문이 있으면 모두 청산
  ExitOppositeFilledEntries(market_entry->GetEntryDirection());

  // 진입 증거금 계산: 1포인트 == 1달러 가정 계산
  const auto entry_filled_price = market_entry->GetEntryFilledPrice();
  const double entry_margin =
      market_entry->GetEntryFilledSize() * entry_filled_price;

  // 진입 수수료 로딩
  const auto entry_commission = market_entry->GetEntryCommission();

  // 진입 가능 여부 체크 (사용 가능 자금 >= 진입 증거금 + 수수료)
  try {
    HasEnoughBalance(engine_->UpdateAvailableBalance(),
                     entry_margin + entry_commission, "사용 가능",
                     "시장가 주문 마진 및 진입 수수료");
  } catch (const InsufficientBalance& e) {
    LogFormattedInfo(WARNING_L, e.what(), __FILE__, __LINE__);
    throw OrderFailed("시장가 진입 실패");
  }

  // 지갑 자금에서 진입 수수료 감소
  engine_->DecreaseWalletBalance(entry_commission);

  // 사용한 마진에 진입 증거금 증가
  engine_->IncreaseUsedMargin(entry_margin);

  // 전역 항목들 업데이트
  const auto symbol_idx = bar_->GetCurrentSymbolIndex();
  UpdateLastEntryBarIndex(symbol_idx);
  last_entry_prices_[symbol_idx] = entry_filled_price;
  just_entered_ = true;

  // 시장가 진입
  filled_entries_[symbol_idx].push_back(market_entry);
}

void OrderHandler::ExitOppositeFilledEntries(const Direction direction) {
  for (const auto& filled_entry :
       filled_entries_[bar_->GetCurrentSymbolIndex()]) {
    if (const auto entry_direction = filled_entry->GetEntryDirection();
        direction != entry_direction) {
      const string& exit_name =
          entry_direction == Direction::LONG ? "리버스 매도" : "리버스 매수";

      MarketExit(exit_name, filled_entry->GetEntryName(),
                 // 분할 청산했을 수도 있으므로 잔량만 청산
                 filled_entry->GetEntryFilledSize() -
                     filled_entry->GetExitFilledSize());
    }
  }
}

void OrderHandler::ExecuteExit(const shared_ptr<Order>& exit_order) {
  // 주문 정보 로딩
  const auto symbol_idx = bar_->GetCurrentSymbolIndex();
  const double entry_filled_price = exit_order->GetEntryFilledPrice();
  const double exit_filled_size = exit_order->GetExitFilledSize();
  const double exit_filled_price = exit_order->GetExitFilledPrice();

  // 지갑 자금에서 청산 수수료 감소
  engine_->DecreaseWalletBalance(exit_order->GetExitCommission());

  // 사용한 마진에서 청산 수량에 해당되는 마진만큼 진입 증거금 감소
  engine_->DecreaseUsedMargin(exit_filled_size * entry_filled_price);

  // 실현 손익 계산
  const double realized_pnl = CalculatePnl(
      exit_filled_price, exit_order->GetEntryDirection(), entry_filled_price,
      exit_filled_size, exit_order->GetLeverage());

  // 지갑 자금에 실현 손익 계산
  if (IsGreaterOrEqual(realized_pnl, 0.0)) {
    engine_->IncreaseWalletBalance(realized_pnl);
  } else if (IsLess(realized_pnl, 0.0)) {
    engine_->DecreaseWalletBalance(abs(realized_pnl));
  }

  // 전역 항목들 업데이트
  engine_->UpdateStatistics();
  UpdateLastExitBarIndex(symbol_idx);
  last_exit_prices_[symbol_idx] = exit_filled_price;
  just_exited_ = true;

  // 분석기에 청산된 거래 추가
  AddTrade(exit_order, realized_pnl);
}

void OrderHandler::ExecuteCancelEntry(const shared_ptr<Order>& cancel_order) {
  switch (cancel_order->GetEntryOrderType()) {
    case MARKET: {
      // 시장가는 바로 체결하므로 대기 주문이 없음
      return;
    }

    case LIMIT: {
      // 사용한 자금에서 예약 증거금 감소
      engine_->DecreaseUsedMargin(cancel_order->GetEntryOrderSize() *
                                  cancel_order->GetEntryOrderPrice());
      return;
    }

    case MIT: {
      /* MIT Touch 대기 중에는 예약 증거금을 사용하지 않으며,
         Touch 이후에는 시장가로 체결하므로 대기 주문이 없음  */
      return;
    }

    case LIT: {
      if (cancel_order->GetEntryOrderTime() != -1) {
        /* Entry Order Time이 설정되었다는 것은 Touch 했다는 의미이며,
           Touch 이후에는 지정가로 예약 증거금을 사용하므로 사용한 자금에서 예약
           증거금을 감소시켜야 함 */
        engine_->DecreaseUsedMargin(cancel_order->GetEntryOrderSize() *
                                    cancel_order->GetEntryOrderPrice());
      }
      return;
    }

    case TRAILING: {
      /* Trailing Touch 대기 중에는 예약 증거금을 사용하지 않으며,
         Touch 이후에는 가격을 추적하다 시장가로 체결하므로 대기 주문이 없음 */
      return;
    }

    default:
      return;
  }
}

void OrderHandler::CheckPendingLimitEntries(const int symbol_idx,
                                            const int order_idx,
                                            const double price,
                                            const PriceType price_type) {
  const auto& limit_entry = pending_entries_[symbol_idx][order_idx];

  if (const auto order_price = limit_entry->GetEntryOrderPrice();
      IsLimitPriceSatisfied(limit_entry->GetEntryDirection(), price,
                            order_price)) {
    try {
      // 시가에서 진입 조건 달성 시 체결 가격은 시가가 됨 (갭 고려)
      FillPendingLimitEntry(symbol_idx, order_idx,
                            price_type == OPEN ? price : order_price);
    } catch ([[maybe_unused]] const OrderFailed& e) {
      LogFormattedInfo(WARNING_L,
                       format("지정가 [{}] 체결이 실패했습니다.",
                              limit_entry->GetEntryName()),
                       __FILE__, __LINE__);
    }
  }
}

void OrderHandler::CheckPendingMitEntries(const int symbol_idx,
                                          const int order_idx,
                                          const double price,
                                          const PriceType price_type) {
  const auto& mit_entry = pending_entries_[symbol_idx][order_idx];

  if (const auto touch_price = mit_entry->GetEntryTouchPrice();
      IsPriceTouched(mit_entry->GetEntryTouchDirection(), price, touch_price)) {
    try {
      // 시가에서 터치 조건 달성 시 주문 가격은 시가가 됨 (갭 고려)
      FillPendingMarketEntry(symbol_idx, order_idx,
                             price_type == OPEN ? price : touch_price);
    } catch ([[maybe_unused]] const OrderFailed& e) {
      LogFormattedInfo(
          WARNING_L,
          format("MIT [{}] 체결이 실패했습니다.", mit_entry->GetEntryName()),
          __FILE__, __LINE__);
    }
  }
}

void OrderHandler::CheckPendingLitEntries(const int symbol_idx,
                                          const int order_idx,
                                          const double price,
                                          const PriceType price_type) {
  const auto& lit_entry = pending_entries_[symbol_idx][order_idx];

  // Order Time이 설정되지 않았으면 지정가 미주문 -> 터치 확인
  if (lit_entry->GetEntryOrderTime() == -1) {
    if (IsPriceTouched(lit_entry->GetEntryTouchDirection(), price,
                       lit_entry->GetEntryTouchPrice())) {
      try {
        OrderPendingLitEntry(symbol_idx, order_idx);
      } catch ([[maybe_unused]] const OrderFailed& e) {
        // 주문 실패 시 해당 주문 삭제 후 다음 주문 확인으로 넘어감
        LogFormattedInfo(
            WARNING_L,
            format("LIT [{}] 주문이 실패했습니다.", lit_entry->GetEntryName()),
            __FILE__, __LINE__);
        return;
      }
    }
  }

  // Order Time이 설정됐으면 지정가 주문 완료 -> 체결 확인
  // 터치 후 바로 진입될 수도 있으므로 Order Time을 다시 불러와서 확인
  if (lit_entry->GetEntryOrderTime() != -1) {
    if (const auto order_price = lit_entry->GetEntryOrderPrice();
        IsLimitPriceSatisfied(lit_entry->GetEntryDirection(), price,
                              order_price)) {
      try {
        // 시가에서 터치 조건 달성 시 체결 가격은 시가가 됨 (갭 고려)
        FillPendingLimitEntry(symbol_idx, order_idx,
                              price_type == OPEN ? price : order_price);
      } catch ([[maybe_unused]] const OrderFailed& e) {
        LogFormattedInfo(
            WARNING_L,
            format("LIT [{}] 체결이 실패했습니다.", lit_entry->GetEntryName()),
            __FILE__, __LINE__);
      }
    }
  }
}

void OrderHandler::CheckPendingTrailingEntries(const int symbol_idx,
                                               const int order_idx,
                                               const double price,
                                               const PriceType price_type) {
  const auto& trailing_entry = pending_entries_[symbol_idx][order_idx];

  // Extreme Price가 지정되지 않았으면 추적 미시작 -> 터치 확인
  if (isnan(trailing_entry->GetEntryExtremePrice())) {
    if (IsPriceTouched(trailing_entry->GetEntryTouchDirection(), price,
                       trailing_entry->GetEntryTouchPrice())) {
      trailing_entry->SetEntryExtremePrice(price);
    }
  }

  try {
    // Extreme Price가 지정되었으면 추적 시작
    // -> 고저가 업데이트 및 체결 터치 확인
    if (double extreme_price = trailing_entry->GetEntryExtremePrice();
        !isnan(extreme_price)) {
      // 주문 정보 로딩
      const auto entry_direction = trailing_entry->GetEntryDirection();
      const auto trail_point = trailing_entry->GetEntryTrailPoint();

      if (entry_direction == Direction::LONG) {
        // 진입 방향이 매수인 경우, 최저가를 추적
        if (IsLess(price, extreme_price)) {
          trailing_entry->SetEntryExtremePrice(price);
          extreme_price = price;
        }

        // 진입 방향이 매수인 경우, 최저가로부터 Trail Point 증가 시 진입
        if (const double trail_price = extreme_price + trail_point;
            IsGreaterOrEqual(price, trail_price)) {
          // 시가에서 조건 달성 시 주문 가격은 시가가 됨 (갭 고려)
          FillPendingMarketEntry(symbol_idx, order_idx,
                                 price_type == OPEN ? price : trail_price);
        }
      } else if (entry_direction == Direction::SHORT) {
        // 진입 방향이 매도인 경우, 최고가를 추적
        if (IsGreater(price, extreme_price)) {
          trailing_entry->SetEntryExtremePrice(price);
          extreme_price = price;
        }

        // 진입 방향이 매도인 경우, 최고가로부터 Trail Point 감소 시 진입
        if (const double trail_price = extreme_price - trail_point;
            IsLessOrEqual(price, trail_price)) {
          // 시가에서 조건 달성 시 주문 가격은 시가가 됨 (갭 고려)
          FillPendingMarketEntry(symbol_idx, order_idx,
                                 price_type == OPEN ? price : trail_price);
        }
      }
    }
  } catch ([[maybe_unused]] const OrderFailed& e) {
    LogFormattedInfo(WARNING_L,
                     format("트레일링 [{}] 체결이 실패했습니다.",
                            trailing_entry->GetEntryName()),
                     __FILE__, __LINE__);
  }
}

void OrderHandler::FillPendingMarketEntry(const int symbol_idx,
                                          const int order_idx,
                                          const double order_price) {
  // 바 정보 로딩
  const auto current_open_time = engine_->GetCurrentOpenTime();

  // 대기 주문을 받아오고 대기 주문에서 삭제
  auto& pending_entries = pending_entries_[symbol_idx];
  const auto& market_entry = pending_entries[order_idx];
  pending_entries.erase(pending_entries.begin() + order_idx);

  // 예외 체크
  try {
    IsValidEntryName(market_entry->GetEntryName());
  } catch (const InvalidValue& e) {
    LogFormattedInfo(WARNING_L, e.what(), __FILE__, __LINE__);
    throw OrderFailed("시장가 대기 주문 체결 실패");
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
  const double slippage_filled_price =
      CalculateSlippagePrice(order_price, MARKET, entry_direction, leverage);
  market_entry->SetEntryFilledPrice(slippage_filled_price);

  // 수수료
  const double entry_commission =
      CalculateCommission(slippage_filled_price, MARKET, filled_size, leverage);
  market_entry->SetEntryCommission(entry_commission);

  // 마진콜 가격
  market_entry->SetMarginCallPrice(CalculateMarginCallPrice(
      slippage_filled_price, entry_direction, leverage));

  // 자금 관련 처리 후 체결 주문에 추가
  ExecuteMarketEntry(market_entry);

  // 디버그 로그 기록
  LogFormattedInfo(
      ORDER_L,
      format("{} [{}] 체결 | 체결 가격: {}",
             market_entry->GetEntryOrderType() == MIT ? "MIT" : "트레일링",
             market_entry->GetEntryName(), slippage_filled_price),
      __FILE__, __LINE__);
}

void OrderHandler::FillPendingLimitEntry(const int symbol_idx,
                                         const int order_idx,
                                         const double filled_price) {
  // 대기 주문을 받아오고 대기 주문에서 삭제
  auto& pending_entries = pending_entries_[symbol_idx];
  const auto limit_entry = pending_entries[order_idx];
  pending_entries.erase(pending_entries.begin() + order_idx);

  // 예외 체크
  try {
    IsValidEntryName(limit_entry->GetEntryName());
  } catch (const InvalidValue& e) {
    LogFormattedInfo(WARNING_L, e.what(), __FILE__, __LINE__);
    throw OrderFailed("지정가 대기 주문 체결 실패");
  }

  // 주문 정보 로딩
  const auto entry_filled_size = limit_entry->GetEntryOrderSize();
  const auto entry_direction = limit_entry->GetEntryDirection();
  const auto leverage = limit_entry->GetLeverage();

  // 주문 업데이트
  limit_entry->SetEntryFilledTime(engine_->GetCurrentOpenTime())
      .SetEntryFilledSize(entry_filled_size);

  // 슬리피지가 포함된 체결가
  const double slippage_filled_price =
      CalculateSlippagePrice(filled_price, LIMIT, entry_direction, leverage);
  limit_entry->SetEntryFilledPrice(slippage_filled_price);

  // 수수료
  const double entry_commission = CalculateCommission(
      slippage_filled_price, LIMIT, entry_filled_size, leverage);
  limit_entry->SetEntryCommission(entry_commission);

  // 마진콜 가격
  limit_entry->SetMarginCallPrice(CalculateMarginCallPrice(
      slippage_filled_price, entry_direction, leverage));

  // 해당 주문과 반대 방향의 체결 주문이 있으면 모두 청산
  ExitOppositeFilledEntries(entry_direction);

  // 진입 가능 여부 체크 (지갑 자금 >= 수수료)
  try {
    HasEnoughBalance(engine_->GetWalletBalance(), entry_commission, "지갑 자금",
                     "진입 수수료");
  } catch (const InsufficientBalance& e) {
    LogFormattedInfo(WARNING_L, e.what(), __FILE__, __LINE__);

    // 주문 실패 시 사용한 마진에서 예약 증거금 감소
    engine_->DecreaseUsedMargin(limit_entry->GetEntryOrderSize() *
                                limit_entry->GetEntryOrderPrice());
    throw OrderFailed("지정가 대기 주문 체결 실패");
  }

  // 지갑 자금에서 진입 수수료 감소
  engine_->DecreaseWalletBalance(entry_commission);

  // 전역 항목들 업데이트
  UpdateLastEntryBarIndex(symbol_idx);
  last_entry_prices_[symbol_idx] = slippage_filled_price;
  just_entered_ = true;

  // 지정가 진입
  filled_entries_[symbol_idx].push_back(limit_entry);

  // 디버그 로그 기록
  LogFormattedInfo(
      ORDER_L,
      format("{} [{}] 체결 | 체결 가격: {}",
             limit_entry->GetEntryOrderType() == LIMIT ? "지정가" : "LIT",
             limit_entry->GetEntryName(), slippage_filled_price),
      __FILE__, __LINE__);
}

void OrderHandler::OrderPendingLitEntry(const int symbol_idx,
                                        const int order_idx) {
  // 대기 주문 로딩
  auto& pending_entries = pending_entries_[symbol_idx];
  const auto& lit_entry = pending_entries[order_idx];

  // 주문 업데이트
  lit_entry->SetEntryOrderTime(engine_->GetCurrentOpenTime());

  // 예약 증거금 계산: 1포인트 == 1달러 가정 계산
  const double entry_margin =
      lit_entry->GetEntryOrderSize() * lit_entry->GetEntryOrderPrice();

  // 주문 가능 여부 체크
  try {
    HasEnoughBalance(engine_->UpdateAvailableBalance(), entry_margin,
                     "사용 가능", "지정가 주문 마진");
  } catch (const InsufficientBalance& e) {
    LogFormattedInfo(WARNING_L, e.what(), __FILE__, __LINE__);

    // 주문 실패 시 대기 주문에서 삭제
    pending_entries.erase(pending_entries.begin() + order_idx);
    throw OrderFailed("LIT 주문 실패");
  }

  // 사용한 마진에 예약 증거금 증가
  engine_->IncreaseUsedMargin(entry_margin);

  // 디버그 로그 기록
  LogFormattedInfo(
      ORDER_L,
      format("LIT [{}] 체결 대기 | 주문 가격: {}", lit_entry->GetEntryName(),
             lit_entry->GetEntryOrderPrice()),
      __FILE__, __LINE__);
}

int OrderHandler::CheckPendingLimitExits(const int symbol_idx,
                                         const int order_idx,
                                         const double price,
                                         const PriceType price_type) {
  const auto& limit_exit = pending_exits_[symbol_idx][order_idx];

  if (const auto order_price = limit_exit->GetExitOrderPrice();
      IsLimitPriceSatisfied(limit_exit->GetExitDirection(), price,
                            order_price)) {
    try {
      // 시가에서 진입 조건 달성 시 체결 가격은 시가가 됨 (갭 고려)
      return FillPendingExitOrder(symbol_idx, order_idx,
                                  price_type == OPEN ? price : order_price);
    } catch ([[maybe_unused]] const OrderFailed& e) {
      LogFormattedInfo(
          WARNING_L,
          format("지정가 [{}] 체결이 실패했습니다.", limit_exit->GetExitName()),
          __FILE__, __LINE__);
    }
  }

  return 0;
}

int OrderHandler::CheckPendingMitExits(const int symbol_idx,
                                       const int order_idx, const double price,
                                       const PriceType price_type) {
  const auto& mit_exit = pending_exits_[symbol_idx][order_idx];

  if (const auto touch_price = mit_exit->GetExitTouchPrice();
      IsPriceTouched(mit_exit->GetExitTouchDirection(), price, touch_price)) {
    try {
      // 시가에서 터치 조건 달성 시 주문 가격은 시가가 됨 (갭 고려)
      return FillPendingExitOrder(symbol_idx, order_idx,
                                  price_type == OPEN ? price : touch_price);
    } catch ([[maybe_unused]] const OrderFailed& e) {
      LogFormattedInfo(
          WARNING_L,
          format("MIT [{}] 체결이 실패했습니다.", mit_exit->GetExitName()),
          __FILE__, __LINE__);
    }
  }

  return 0;
}

int OrderHandler::CheckPendingLitExits(const int symbol_idx,
                                       const int order_idx, const double price,
                                       const PriceType price_type) {
  const auto& lit_exit = pending_exits_[symbol_idx][order_idx];

  // Order Time이 설정되지 않았으면 지정가 미주문 -> 터치 확인
  if (lit_exit->GetExitOrderTime() == -1) {
    if (IsPriceTouched(lit_exit->GetExitTouchDirection(), price,
                       lit_exit->GetExitTouchPrice())) {
      // 주문 업데이트
      lit_exit->SetExitOrderTime(engine_->GetCurrentOpenTime());

      // 디버그 로그 기록
      LogFormattedInfo(
          ORDER_L,
          format("LIT [{}] 체결 대기 | 주문 가격: {}", lit_exit->GetEntryName(),
                 lit_exit->GetEntryOrderPrice()),
          __FILE__, __LINE__);
    }
  }

  // Order Time이 설정됐으면 지정가 주문 완료 -> 체결 확인
  // 터치 후 바로 진입될 수도 있으므로 Order Time을 다시 불러와서 확인
  if (lit_exit->GetExitOrderTime() != -1) {
    if (const auto order_price = lit_exit->GetExitOrderPrice();
        IsLimitPriceSatisfied(lit_exit->GetExitDirection(), price,
                              order_price)) {
      try {
        // 시가에서 터치 조건 달성 시 체결 가격은 시가가 됨 (갭 고려)
        return FillPendingExitOrder(symbol_idx, order_idx,
                                    price_type == OPEN ? price : order_price);
      } catch ([[maybe_unused]] const OrderFailed& e) {
        LogFormattedInfo(
            WARNING_L,
            format("LIT [{}] 체결이 실패했습니다.", lit_exit->GetExitName()),
            __FILE__, __LINE__);
      }
    }
  }

  return 0;
}

int OrderHandler::CheckPendingTrailingExits(const int symbol_idx,
                                            const int order_idx,
                                            const double price,
                                            const PriceType price_type) {
  const auto& trailing_exit = pending_exits_[symbol_idx][order_idx];

  // Extreme Price가 지정되지 않았으면 추적 미시작 -> 터치 확인
  if (isnan(trailing_exit->GetExitExtremePrice())) {
    if (IsPriceTouched(trailing_exit->GetExitTouchDirection(), price,
                       trailing_exit->GetExitTouchPrice())) {
      trailing_exit->SetExitExtremePrice(price);
    }
  }

  try {
    // Extreme Price가 지정되었으면 추적 시작
    // -> 고저가 업데이트 및 체결 터치 확인
    if (double extreme_price = trailing_exit->GetExitExtremePrice();
        !isnan(extreme_price)) {
      // 주문 정보 로딩
      const auto exit_direction = trailing_exit->GetExitDirection();
      const auto trail_point = trailing_exit->GetExitTrailPoint();

      if (exit_direction == Direction::LONG) {
        // 청산 방향이 매수인 경우, 최저가를 추적
        if (IsLess(price, extreme_price)) {
          trailing_exit->SetExitExtremePrice(price);
          extreme_price = price;
        }

        // 청산 방향이 매수인 경우, 최저가로부터 Trail Point 증가 시 진입
        if (const double trail_price = extreme_price + trail_point;
            IsGreaterOrEqual(price, trail_price)) {
          // 시가에서 조건 달성 시 주문 가격은 시가가 됨 (갭 고려)
          return FillPendingExitOrder(symbol_idx, order_idx,
                                      price_type == OPEN ? price : trail_price);
        }
      } else if (exit_direction == Direction::SHORT) {
        // 청산 방향이 매도인 경우, 최고가를 추적
        if (IsGreater(price, extreme_price)) {
          trailing_exit->SetExitExtremePrice(price);
          extreme_price = price;
        }

        // 진입 방향이 매도인 경우, 최고가로부터 Trail Point 감소 시 진입
        if (const double trail_price = extreme_price - trail_point;
            IsLessOrEqual(price, trail_price)) {
          // 시가에서 조건 달성 시 주문 가격은 시가가 됨 (갭 고려)
          return FillPendingExitOrder(symbol_idx, order_idx,
                                      price_type == OPEN ? price : trail_price);
        }
      }
    }
  } catch ([[maybe_unused]] const OrderFailed& e) {
    LogFormattedInfo(WARNING_L,
                     format("트레일링 [{}] 체결이 실패했습니다.",
                            trailing_exit->GetExitName()),
                     __FILE__, __LINE__);
  }

  return 0;
}

int OrderHandler::FillPendingExitOrder(const int symbol_idx,
                                       const int order_idx,
                                       const double filled_price) {
  // 바 정보 로딩
  const auto current_open_time = engine_->GetCurrentOpenTime();

  // 대기 주문을 받아오고 대기 주문에서 삭제
  auto& pending_exits = pending_exits_[symbol_idx];
  const auto exit_order = pending_exits[order_idx];
  pending_exits.erase(pending_exits.begin() + order_idx);

  // 주문 정보 로딩
  const auto& target_entry_name = exit_order->GetEntryName();
  const auto order_type = exit_order->GetExitOrderType();
  const auto exit_direction = exit_order->GetExitDirection();
  const auto leverage = exit_order->GetLeverage();

  // 원본 진입 주문 찾기
  shared_ptr<Order> entry_order;
  int entry_order_idx;
  try {
    const auto [order, index] = FindMatchingEntryOrder(target_entry_name);
    entry_order = order;
    entry_order_idx = index;
  } catch ([[maybe_unused]] const EntryOrderNotFound& e) {
    // 대기 주문 시 이미 진입 주문을 찾았으므로 원본 진입은 무조건 존재
    LogFormattedInfo(
        ERROR_L, "청산 대기 주문을 체결하는데 원본 주문이 존재하지 않습니다.",
        __FILE__, __LINE__);
    throw OrderFailed("대기 주문 청산 실패");
  }

  // 총 청산 체결 수량이 진입 체결 수량보다 크지 않게 조정
  const double exit_filled_size =
      GetAdjustedExitFilledSize(exit_order->GetExitOrderSize(), entry_order);

  // 주문 업데이트
  if (order_type == MIT || order_type == TRAILING) {
    exit_order->SetExitOrderTime(current_open_time)
        .SetExitOrderPrice(filled_price);
  }

  exit_order->SetExitFilledTime(current_open_time)
      .SetExitFilledSize(exit_filled_size);

  // 슬리피지가 포함된 체결가
  const double slippage_filled_price = CalculateSlippagePrice(
      filled_price, order_type, exit_direction, leverage);
  exit_order->SetExitFilledPrice(slippage_filled_price);

  // 수수료
  exit_order->SetExitCommission(CalculateCommission(
      slippage_filled_price, order_type, exit_filled_size, leverage));

  // 원본 진입 주문에 청산 체결 수량 추가
  const double total_exit_filled_size =
      entry_order->GetExitFilledSize() + exit_filled_size;
  entry_order->SetExitFilledSize(total_exit_filled_size);

  /* 원본 진입 주문의 청산 체결 수량이 진입 체결 수량과 같으면
     filled_entries에서 삭제                              */
  int deleted_below_count = 0;
  if (IsEqual(total_exit_filled_size, entry_order->GetEntryFilledSize())) {
    auto& filled_entries = filled_entries_[symbol_idx];
    filled_entries.erase(filled_entries.begin() + entry_order_idx);

    // 같은 진입 이름을 목표로 하는 청산 대기 주문 취소
    for (int i = static_cast<int>(pending_exits.size()) - 1; i >= 0; i--) {
      if (target_entry_name == pending_exits[i]->GetEntryName()) {
        pending_exits.erase(pending_exits.begin() + i);

        /* 이 함수를 호출한 order_idx보다 작을 때만 deleted_count를 증가
           같거나 클 때 증가시키면 CheckPendingExits 함수에서 order_idx보다
           작은 인덱스의 청산 체결 체크가 누락됨 */
        if (i < order_idx) {
          deleted_below_count++;
        }
      }
    }
  }

  // 자금, 통계 업데이트
  ExecuteExit(exit_order);

  // 디버그 로그 기록
  string order_type_str;
  if (order_type == LIMIT) {
    order_type_str = "지정가";
  } else if (order_type == MIT) {
    order_type_str = "MIT";
  } else if (order_type == LIT) {
    order_type_str = "LIT";
  } else if (order_type == TRAILING) {
    order_type_str = "트레일링";
  }

  LogFormattedInfo(ORDER_L,
                   format("{} [{}] 체결 | 체결 가격: {}", order_type_str,
                          exit_order->GetExitName(), slippage_filled_price),
                   __FILE__, __LINE__);

  return deleted_below_count;
}

pair<shared_ptr<Order>, int> OrderHandler::FindMatchingEntryOrder(
    const string& target_entry_name) const {
  // 현재 심볼의 체결된 진입들 순회
  const auto& filled_entries = filled_entries_[bar_->GetCurrentSymbolIndex()];
  for (int order_idx = 0; order_idx < filled_entries.size(); order_idx++) {
    // target_entry_name과 같은 이름의 진입이 있으면 반환
    if (const auto& filled_entry = filled_entries[order_idx];
        filled_entry->GetEntryName() == target_entry_name) {
      return {filled_entry, order_idx};
    }
  }

  // 원본 진입 주문을 찾지 못하면 청산 실패
  LogFormattedInfo(
      WARNING_L,
      format("지정된 진입명 [{}]이(가) 존재하지 않아 청산할 수 없습니다.",
             target_entry_name),
      __FILE__, __LINE__);
  throw EntryOrderNotFound("");
}

double OrderHandler::GetAdjustedExitFilledSize(
    const double exit_size, const shared_ptr<Order>& entry_order) {
  // 청산 수량 + 분할 청산한 수량이 진입 수량보다 많다면
  const auto entry_filled_size = entry_order->GetEntryFilledSize();
  const auto exit_filled_size = entry_order->GetExitFilledSize();
  if (const auto total_exit_size = exit_size + exit_filled_size;
      IsGreater(total_exit_size, entry_filled_size)) {
    // 최대값으로 조정하여 반환
    return entry_filled_size - exit_filled_size;
  }

  return exit_size;
}

void OrderHandler::AddTrade(const shared_ptr<Order>& exit_order,
                            const double realized_pnl) const {
  // 보유 심볼 개수 카운트
  int symbol_count = 0;

  // 모든 심볼의 체결된 진입 순회
  for (const auto& filled_entry : filled_entries_) {
    // 해당 심볼에 체결된 진입이 있다면 심볼 개수 추가
    if (!filled_entry.empty()) {
      symbol_count++;
    }
  }

  // 진입 및 청산 체결 시간
  const int64_t entry_time = exit_order->GetEntryFilledTime();
  const int64_t exit_time = exit_order->GetExitFilledTime();

  //@@@@@@@@@@@@@@@@@@@@@@ 포지션 사이즈 바이낸스에서 Minimum Qty 받아와서
  // Qty가 정수면 묵시적으로 소숫점 없애고 실수면 Qty의 최대 소숫점 자리까지
  // 반올림 & 최소 사이즈 검사 -> 진입 시 검사

  // 거래 목록에 거래 추가
  analyzer_->AddTrade(
      Trade()
          .SetSymbolName(bar_->GetBarData(BarType::TRADING, "NONE")
                             .GetSymbolName(bar_->GetCurrentSymbolIndex()))
          .SetStrategyName(engine_->GetCurrentStrategyName())
          .SetEntryName(exit_order->GetEntryName())
          .SetExitName(exit_order->GetExitName())
          .SetEntryDirection(exit_order->GetEntryDirection() == Direction::LONG
                                 ? "매수"
                                 : "매도")
          .SetEntryTime(UtcTimestampToUtcDatetime(entry_time))
          .SetExitTime(UtcTimestampToUtcDatetime(exit_time))
          .SetHoldingTime(FormatTimeDiff(exit_time - entry_time))
          .SetEntrySize(exit_order->GetEntryFilledSize())
          .SetExitSize(exit_order->GetExitFilledSize())
          .SetEntryPrice(exit_order->GetEntryFilledPrice())
          .SetExitPrice(exit_order->GetExitFilledPrice())
          .SetLeverage(exit_order->GetLeverage())
          .SetEntryCommission(exit_order->GetEntryCommission())
          .SetExitCommission(exit_order->GetExitCommission())
          .SetProfitLoss(realized_pnl)
          .SetWalletBalance(engine_->GetWalletBalance())
          .SetMaxWalletBalance(engine_->GetMaxWalletBalance())
          .SetDrawdown(engine_->GetDrawdown())
          .SetMaxDrawdown(engine_->GetMaxDrawdown())
          .SetSymbolCount(symbol_count));
}