// 표준 라이브러리
#include <format>

// 내부 헤더
#include "Engines/BaseEngine.hpp"
#include "Engines/Config.hpp"
#include "Engines/DataUtils.hpp"

// 파일 헤더
#include "Engines/BaseOrderHandler.hpp"

// 네임 스페이스
using namespace data_utils;

BaseOrderHandler::BaseOrderHandler()
    : current_position_size(0),
      config_(engine_.GetEngineConfig()) {}
BaseOrderHandler::~BaseOrderHandler() = default;

BarHandler& BaseOrderHandler::bar_ = BarHandler::GetBarHandler();
Engine& BaseOrderHandler::engine_ = Engine::GetEngine();
Logger& BaseOrderHandler::logger_ = Logger::GetLogger();

void BaseOrderHandler::InitializeOrders(const int num_symbols) {
  pending_entries_.reserve(num_symbols);
  filled_entries_.reserve(num_symbols);
  pending_exits_.reserve(num_symbols);
}

double BaseOrderHandler::GetUnrealizedPnl() const {
  double pnl = 0;

  // 심볼 순회
  for (int symbol_idx = 0; symbol_idx < filled_entries_.size(); ++symbol_idx) {
    const auto bar_idx = bar_.GetCurrentBarIndex(symbol_idx);
    const auto current_open =
        bar_.GetBarData(BaseBarHandler::BarType::MAGNIFIER)
            .GetOpen(symbol_idx, bar_idx);

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
  const int symbol_idx = bar_.current_symbol_idx_;
  const auto bar_idx = bar_.GetCurrentBarIndex(symbol_idx);

  if (direction == Direction::LONG) {  // 트레일링 매수 진입 시 최저가를 추적
    return bar_.GetBarData(bar_.current_bar_type_).GetLow(symbol_idx, bar_idx);
  }

  if (direction == Direction::SHORT) {  // 트레일링 매도 진입 시 최고가를 추적
    return bar_.GetBarData(bar_.current_bar_type_).GetHigh(symbol_idx, bar_idx);
  }

  return nan("");
}

double BaseOrderHandler::CalculateSlippagePrice(
    const double order_price, const OrderType order_type,
    const Direction direction, const shared_ptr<Order>& order) const {
  const double tick_size = engine_.GetTickSize(bar_.current_symbol_idx_);

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
                         engine_.GetTickSize(bar_.current_symbol_idx_));
}

void BaseOrderHandler::IsValidPrice(const double price) {
  if (price <= 0) {
    Logger::LogAndThrowError(
        format("주어진 가격 {}은(는) 0보다 커야합니다.", price), __FILE__,
        __LINE__);
  }
}

void BaseOrderHandler::IsValidPositionSize(const double position_size) {
  if (position_size <= 0) {
    Logger::LogAndThrowError(
          format("주어진 포지션 크기 {}은(는) 0보다 커야합니다.", position_size),
          __FILE__, __LINE__);
  }
}

void BaseOrderHandler::IsValidLeverage(const unsigned char leverage) {
  if (leverage < 1) {
    Logger::LogAndThrowError(
        format("주어진 레버리지 {}은(는) 1보다 커야합니다.", leverage),
        __FILE__, __LINE__);
  }
}
