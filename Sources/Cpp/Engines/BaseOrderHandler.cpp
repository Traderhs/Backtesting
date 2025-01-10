// 표준 라이브러리
#include <format>

// 파일 헤더
#include "Engines\BaseOrderHandler.hpp"

// 내부 헤더
#include "Engines\BarData.hpp"
#include "Engines\BarHandler.hpp"
#include "Engines\DataUtils.hpp"
#include "Engines\Engine.hpp"
#include "Engines\TimeUtils.hpp"

// 네임 스페이스
using namespace data_utils;
using namespace time_utils;

BaseOrderHandler::BaseOrderHandler()
    : current_position_size(0), config_(engine_->GetConfig()) {}
BaseOrderHandler::~BaseOrderHandler() = default;

shared_ptr<BarHandler>& BaseOrderHandler::bar_ = BarHandler::GetBarHandler();
shared_ptr<Engine>& BaseOrderHandler::engine_ = Engine::GetEngine();
shared_ptr<Logger>& BaseOrderHandler::logger_ = Logger::GetLogger();

void BaseOrderHandler::InitializeOrders(const int num_symbols) {
  pending_entries_.reserve(num_symbols);
  filled_entries_.reserve(num_symbols);
  pending_exits_.reserve(num_symbols);
}

double BaseOrderHandler::GetUnrealizedPnl() const {
  double pnl = 0;

  // 심볼 순회
  for (int symbol_idx = 0; symbol_idx < filled_entries_.size(); ++symbol_idx) {
    // 시가 시점의 평가 손익 구해야 함
    const auto current_open =
        bar_->GetBarData(bar_->GetCurrentBarType())
            .GetOpen(symbol_idx, bar_->GetCurrentBarIndex());

    // 진입 주문 순회
    for (const auto& filled_entry : filled_entries_[symbol_idx]) {
      // 진입 방향에 따라 손익 합산
      if (filled_entry->GetEntryDirection() == Direction::LONG) {
        pnl += (current_open - filled_entry->GetEntryFilledPrice()) *
               filled_entry->GetEntryFilledSize() * filled_entry->GetLeverage();
      } else {
        pnl += (filled_entry->GetEntryFilledPrice() - current_open) *
               filled_entry->GetEntryFilledSize() * filled_entry->GetLeverage();
      }
    }
  }

  return pnl;
}

double BaseOrderHandler::GetInitialExtremePrice(const Direction direction) {
  const int symbol_idx = bar_->GetCurrentSymbolIndex();
  const auto bar_idx = bar_->GetCurrentBarIndex();

  if (direction == Direction::LONG) {  // 트레일링 매수 진입 시 최저가를 추적
    return bar_->GetBarData(bar_->GetCurrentBarType())
        .GetLow(symbol_idx, bar_idx);
  }

  if (direction == Direction::SHORT) {  // 트레일링 매도 진입 시 최고가를 추적
    return bar_->GetBarData(bar_->GetCurrentBarType())
        .GetHigh(symbol_idx, bar_idx);
  }

  return nan("");
}

double BaseOrderHandler::CalculateSlippagePrice(
    const double order_price, const OrderType order_type,
    const Direction direction, const shared_ptr<Order>& order) const {
  const double tick_size = engine_->GetTickSize(bar_->GetCurrentSymbolIndex());

  // 시장가, 지정가에 따라 슬리피지가 달라짐
  switch (order_type) {
    case OrderType::MARKET:
      [[fallthrough]];
    case OrderType::MIT:
      [[fallthrough]];
    case OrderType::TRAILING: {
      // 슬리피지 포인트 계산
      const double slippage = order_price * config_.GetSlippage().first / 100 *
                              order->GetLeverage();

      // 방향에 따라 덧셈과 뺄셈이 달라짐
      if (direction == Direction::LONG) {
        return RoundToTickSize(order_price + slippage, tick_size);
      }

      if (direction == Direction::SHORT)
        return RoundToTickSize(order_price - slippage, tick_size);
    }

    case OrderType::LIMIT:
      [[fallthrough]];
    case OrderType::LIT: {
      // 슬리피지 포인트 계산
      const double slippage = order_price * config_.GetSlippage().second / 100 *
                              order->GetLeverage();

      // 방향에 따라 덧셈과 뺄셈이 달라짐
      if (direction == Direction::LONG)
        return RoundToTickSize(order_price + slippage, tick_size);

      if (direction == Direction::SHORT)
        return RoundToTickSize(order_price - slippage, tick_size);
    }

    default: {
      return nan("");
    }
  }
}

double BaseOrderHandler::CalculateCommission(
    const double filled_price, const OrderType order_type,
    const double filled_position_size, const shared_ptr<Order>& order) const {
  // 시장가, 지정가에 따라 수수료가 달라짐
  switch (order_type) {
    case OrderType::MARKET:
      [[fallthrough]];
    case OrderType::MIT:
      [[fallthrough]];
    case OrderType::TRAILING: {
      return filled_price * filled_position_size * order->GetLeverage() *
             (config_.GetCommission().first / 100);
    }

    case OrderType::LIMIT:
      [[fallthrough]];
    case OrderType::LIT: {
      return filled_price * filled_position_size * order->GetLeverage() *
             (config_.GetCommission().second / 100);
    }

    default: {
      return nan("");
    }
  }
}

double BaseOrderHandler::CalculateMarginCallPrice(
    const shared_ptr<Order>& order) {
  const double margin_call_percentage =
      100 / static_cast<double>(order->GetLeverage());
  double margin_call_price = 0;

  if (order->GetEntryDirection() == Direction::LONG) {
    margin_call_price =
        (1 - margin_call_percentage / 100) * order->GetEntryFilledPrice();
  } else if (order->GetEntryDirection() == Direction::SHORT) {
    margin_call_price =
        (1 + margin_call_percentage / 100) * order->GetEntryFilledPrice();
  }

  return RoundToTickSize(margin_call_price,
                         engine_->GetTickSize(bar_->GetCurrentSymbolIndex()));
}

void BaseOrderHandler::IsValidPrice(const double price) {
  if (price <= 0) {
    const auto& bar = bar_->GetBarData(bar_->GetCurrentBarType());
    const int symbol_idx = bar_->GetCurrentSymbolIndex();

    Logger::LogAndThrowError(
        format(" {} | {} | 주어진 가격 {}은(는) 0보다 커야합니다.",
               bar.GetSymbolName(symbol_idx),
               UtcTimestampToUtcDatetime(
                   bar.GetOpenTime(symbol_idx, bar_->GetCurrentBarIndex() + 1)),
               price),
        __FILE__, __LINE__);
  }
}

void BaseOrderHandler::IsValidPositionSize(const double position_size) {
  if (position_size <= 0) {
    const auto& bar = bar_->GetBarData(bar_->GetCurrentBarType());
    const int symbol_idx = bar_->GetCurrentSymbolIndex();

    Logger::LogAndThrowError(
        format(" {} | {} | 주어진 포지션 크기 {}은(는) 0보다 커야합니다.",
               bar.GetSymbolName(symbol_idx),
               UtcTimestampToUtcDatetime(
                   bar.GetOpenTime(symbol_idx, bar_->GetCurrentBarIndex() + 1)),
               position_size),
        __FILE__, __LINE__);
  }
}

void BaseOrderHandler::IsValidLeverage(const unsigned char leverage) {
  if (leverage < 1) {
    const auto& bar = bar_->GetBarData(bar_->GetCurrentBarType());
    const int symbol_idx = bar_->GetCurrentSymbolIndex();

    Logger::LogAndThrowError(
        format(" {} | {} | 주어진 레버리지 {}은(는) 1보다 커야합니다.",
               bar.GetSymbolName(symbol_idx),
               UtcTimestampToUtcDatetime(
                   bar.GetOpenTime(symbol_idx, bar_->GetCurrentBarIndex() + 1)),
               leverage),
        __FILE__, __LINE__);
  }
}

void BaseOrderHandler::IsValidEntryName(const string& entry_name) const {
  /* 같은 이름으로 체결된 Entry Name이 여러 개 존재하면, 청산 시 Target Entry
     지정할 때의 로직이 꼬이기 때문에 하나의 Entry Name은 하나의 진입 체결로
     제한 */
  for (const int symbol_idx = bar_->GetCurrentSymbolIndex();
       const auto& filled_entry : filled_entries_[symbol_idx]) {
    if (entry_name == filled_entry->GetEntryName()) {
      const auto& bar = bar_->GetBarData(bar_->GetCurrentBarType());

      Logger::LogAndThrowError(
          format(
              " {} | {} | 중복된 진입 이름 {}은(는) 동시에 체결될 수 없습니다.",
              bar.GetSymbolName(symbol_idx),
              UtcTimestampToUtcDatetime(
                  bar.GetOpenTime(symbol_idx, bar_->GetCurrentBarIndex() + 1)),
              entry_name),
          __FILE__, __LINE__);
    }
  }
}

void BaseOrderHandler::IsValidLimitOrderPrice(const double limit_price,
                                              const double base_price,
                                              const Direction direction) {
  if (direction == Direction::LONG) {
    if (limit_price >= base_price) {
      const auto& bar = bar_->GetBarData(bar_->GetCurrentBarType());
      const int symbol_idx = bar_->GetCurrentSymbolIndex();

      Logger::LogAndThrowError(
          format(" {} | {} | 지정가 {} 매수 주문은 기준가 {}보다 작아야합니다.",
                 bar.GetSymbolName(symbol_idx),
                 UtcTimestampToUtcDatetime(bar.GetOpenTime(
                     symbol_idx, bar_->GetCurrentBarIndex() + 1)),
                 limit_price, base_price),
          __FILE__, __LINE__);
    }
  } else {
    if (limit_price <= base_price) {
      const auto& bar = bar_->GetBarData(bar_->GetCurrentBarType());
      const int symbol_idx = bar_->GetCurrentSymbolIndex();

      Logger::LogAndThrowError(
          format(" {} | {} | 지정가 {} 매도 주문은 기준가 {}보다 커야합니다.",
                 bar.GetSymbolName(symbol_idx),
                 UtcTimestampToUtcDatetime(bar.GetOpenTime(
                     symbol_idx, bar_->GetCurrentBarIndex() + 1)),
                 limit_price, base_price),
          __FILE__, __LINE__);
    }
  }
}

void BaseOrderHandler::IsValidTrailingTouchPrice(const double touch_price) {
  if (touch_price < 0) {
    const auto& bar = bar_->GetBarData(bar_->GetCurrentBarType());
    const int symbol_idx = bar_->GetCurrentSymbolIndex();

    Logger::LogAndThrowError(
        format(" {} | {} | 주어진 트레일링 터치 가격 "
               "{}은(는) 0과 같거나 커야합니다.",
               bar.GetSymbolName(symbol_idx),
               UtcTimestampToUtcDatetime(
                   bar.GetOpenTime(symbol_idx, bar_->GetCurrentBarIndex() + 1)),
               touch_price),
        __FILE__, __LINE__);
  }
}

void BaseOrderHandler::IsValidTrailPoint(double trail_point) {
  if (trail_point <= 0) {
    const auto& bar = bar_->GetBarData(bar_->GetCurrentBarType());
    const int symbol_idx = bar_->GetCurrentSymbolIndex();

    Logger::LogAndThrowError(
        format(" {} | {} | 주어진 트레일링 포인트 {}은(는) 0보다 커야합니다.",
               bar.GetSymbolName(symbol_idx),
               UtcTimestampToUtcDatetime(
                   bar.GetOpenTime(symbol_idx, bar_->GetCurrentBarIndex() + 1)),
               trail_point),
        __FILE__, __LINE__);
  }
}

void BaseOrderHandler::InvalidEntryName(const string& entry_name) {
  try {
    if (engine_->debug_mode_) {
      const auto& bar = bar_->GetBarData(bar_->GetCurrentBarType());
      const int symbol_idx = bar_->GetCurrentSymbolIndex();

      logger_->Log(
          LogLevel::WARNING_L,
          format(" {} | {} | 지정된 진입명 {}이(가) 존재하지 않아 청산할 "
                 "수 없습니다.",
                 bar.GetSymbolName(symbol_idx),
                 UtcTimestampToUtcDatetime(bar.GetOpenTime(
                     symbol_idx, bar_->GetCurrentBarIndex() + 1)),
                 entry_name),
          __FILE__, __LINE__);
    }
  } catch (...) {
    // return
  }
}

void BaseOrderHandler::LogCancelAndReorder(const string& order_name) {
  try {
    if (engine_->debug_mode_) {
      const auto& bar = bar_->GetBarData(bar_->GetCurrentBarType());
      const int symbol_idx = bar_->GetCurrentSymbolIndex();

      logger_->Log(LogLevel::DEBUG_L,
                   format(" {} | {} | {} 주문을 취소 후 재주문합니다.",
                          bar.GetSymbolName(symbol_idx),
                          UtcTimestampToUtcDatetime(bar.GetOpenTime(
                              symbol_idx, bar_->GetCurrentBarIndex() + 1)),
                          order_name),
                   __FILE__, __LINE__);
    }
  } catch (...) {
    // return
  }
}

void BaseOrderHandler::FormattedDebugLog(const string& formatted_msg) {
  try {
    if (engine_->debug_mode_) {
      const auto& bar = bar_->GetBarData(bar_->GetCurrentBarType());
      const int symbol_idx = bar_->GetCurrentSymbolIndex();

      logger_->Log(
          LogLevel::DEBUG_L,
          format(" {} | {} | {}", bar.GetSymbolName(symbol_idx),
                 bar.GetOpenTime(symbol_idx, bar_->GetCurrentBarIndex() + 1),
                 formatted_msg),
          __FILE__, __LINE__);
    }
  } catch (...) {
    // return
  }
}

void BaseOrderHandler::FormattedWarningLog(const string& formatted_msg) {
  try {
    if (engine_->debug_mode_) {
      const auto& bar = bar_->GetBarData(bar_->GetCurrentBarType());
      const int symbol_idx = bar_->GetCurrentSymbolIndex();

      logger_->Log(
          LogLevel::WARNING_L,
          format(" {} | {} | {}",
                 bar.GetSymbolName(symbol_idx),
                 bar.GetOpenTime(symbol_idx, bar_->GetCurrentBarIndex() + 1),
                 formatted_msg),
          __FILE__, __LINE__);
    }
  } catch (...) {
    // return
  }
}
