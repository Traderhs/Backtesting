// 표준 라이브러리
#include <cmath>
#include <format>

// 파일 헤더
#include "Engines/BaseOrderHandler.hpp"

// 내부 헤더
#include "Engines/Analyzer.hpp"
#include "Engines/BarData.hpp"
#include "Engines/BarHandler.hpp"
#include "Engines/DataUtils.hpp"
#include "Engines/Engine.hpp"
#include "Engines/Exception.hpp"
#include "Engines/TimeUtils.hpp"

// 네임 스페이스
using namespace data_utils;
using namespace time_utils;

BaseOrderHandler::BaseOrderHandler()
    : current_position_size(0), config_(engine_->GetConfig()) {}
BaseOrderHandler::~BaseOrderHandler() = default;

shared_ptr<Analyzer>& BaseOrderHandler::analyzer_ = Analyzer::GetAnalyzer();
shared_ptr<BarHandler>& BaseOrderHandler::bar_ = BarHandler::GetBarHandler();
shared_ptr<Engine>& BaseOrderHandler::engine_ = Engine::GetEngine();
shared_ptr<Logger>& BaseOrderHandler::logger_ = Logger::GetLogger();

void BaseOrderHandler::InitializeOrderHandler(const int num_symbols) {
  config_ = engine_->GetConfig();

  pending_entries_.resize(num_symbols);
  filled_entries_.resize(num_symbols);
  pending_exits_.resize(num_symbols);
}

double BaseOrderHandler::GetUnrealizedPnl() const {
  // 사용 중인 정보 저장
  const auto original_symbol_idx = bar_->GetCurrentSymbolIndex();

  // 바 데이터 로딩
  const auto& bar = bar_->GetBarData(bar_->GetCurrentBarType(),
                                     bar_->GetCurrentReferenceTimeframe());

  // 심볼별 체결된 진입 순회
  double pnl = 0;
  for (int symbol_idx = 0; symbol_idx < bar.GetNumSymbols(); symbol_idx++) {
    bar_->SetCurrentSymbolIndex(symbol_idx);

    // 해당 심볼의 체결된 진입 주문 순회
    for (const auto& filled_entry : filled_entries_[symbol_idx]) {
      // 진입 방향에 따라 손익 합산
      // 부분 청산 시 남은 진입 물량만 미실현 손익에 포함됨
      pnl +=
          CalculatePnl(bar.GetBar(symbol_idx, bar_->GetCurrentBarIndex()).open,
                       filled_entry->GetEntryDirection(),
                       filled_entry->GetEntryFilledPrice(),
                       filled_entry->GetEntryFilledSize() -
                           filled_entry->GetExitFilledSize(),
                       filled_entry->GetLeverage());
    }
  }

  // 사용 중이던 정보 복원
  bar_->SetCurrentSymbolIndex(original_symbol_idx);

  return pnl;
}

void BaseOrderHandler::UpdateCurrentPositionSize() {
  double sum_position_size = 0;

  for (const auto& filled_entry :
       filled_entries_[bar_->GetCurrentSymbolIndex()]) {
    double position_size =
        filled_entry->GetEntryFilledSize() - filled_entry->GetExitFilledSize();

    position_size = filled_entry->GetEntryDirection() == Direction::LONG
                        ? position_size
                        : -position_size;
    sum_position_size += position_size;
  }

  current_position_size = sum_position_size;
}

double BaseOrderHandler::CalculateSlippagePrice(const double order_price,
                                                const OrderType order_type,
                                                const Direction direction,
                                                const int leverage) const {
  double slippage_points = 0;

  // 시장가, 지정가에 따라 슬리피지가 달라짐
  switch (order_type) {
    case OrderType::MARKET:
      [[fallthrough]];
    case OrderType::MIT:
      [[fallthrough]];
    case OrderType::TRAILING: {
      // 시장가 슬리피지 포인트 계산
      slippage_points =
          order_price * config_.GetSlippage().first / 100 * leverage;
      break;
    }

    case OrderType::LIMIT:
      [[fallthrough]];
    case OrderType::LIT: {
      // 지정가 슬리피지 포인트 계산
      slippage_points =
          order_price * config_.GetSlippage().second / 100 * leverage;
      break;
    }

    case OrderType::NONE: {
      break;
    }
  }

  // 계산된 슬리피지 포인트가 0이면 슬리피지는 없음
  if (slippage_points == 0) {
    return order_price;
  }

  // 방향에 따라 덧셈과 뺄셈이 달라짐
  if (direction == Direction::LONG) {
    return RoundToDecimalPlaces(order_price + slippage_points,
                                CountDecimalPlaces(order_price));
  }

  if (direction == Direction::SHORT) {
    return RoundToDecimalPlaces(order_price - slippage_points,
                                CountDecimalPlaces(order_price));
  }

  LogFormattedInfo(LogLevel::WARNING_L, "슬리피지 계산 중 에러가 발생했습니다.",
                   __FILE__, __LINE__);
  return -1;
}

double BaseOrderHandler::CalculateCommission(const double filled_price,
                                             const OrderType order_type,
                                             const double filled_position_size,
                                             const int leverage) const {
  // 시장가, 지정가에 따라 수수료가 달라짐
  switch (order_type) {
    case OrderType::MARKET:
      [[fallthrough]];
    case OrderType::MIT:
      [[fallthrough]];
    case OrderType::TRAILING: {
      return filled_price * filled_position_size * leverage *
             (config_.GetCommission().first / 100);
    }

    case OrderType::LIMIT:
      [[fallthrough]];
    case OrderType::LIT: {
      return filled_price * filled_position_size * leverage *
             (config_.GetCommission().second / 100);
    }

    default: {
      return nan("");
    }
  }
}

double BaseOrderHandler::CalculateMarginCallPrice(
    const double entry_filled_price, const Direction entry_direction,
    const int leverage) {
  const double margin_call_percentage = 100 / static_cast<double>(leverage);

  double margin_call_price = 0;
  if (entry_direction == Direction::LONG) {
    margin_call_price = (1 - margin_call_percentage / 100) * entry_filled_price;
  } else if (entry_direction == Direction::SHORT) {
    margin_call_price = (1 + margin_call_percentage / 100) * entry_filled_price;
  }

  return RoundToDecimalPlaces(margin_call_price,
                              CountDecimalPlaces(entry_filled_price));
}

double BaseOrderHandler::CalculatePnl(const double base_price,
                                      const Direction entry_direction,
                                      const double entry_price,
                                      const double position_size,
                                      const int leverage) {
  if (entry_direction == Direction::LONG) {
    return (base_price - entry_price) * position_size * leverage;
  }

  if (entry_direction == Direction::SHORT) {
    return (entry_price - base_price) * position_size * leverage;
  }

  Logger::LogAndThrowError("방향이 잘못 지정되었습니다.", __FILE__, __LINE__);
  return nan("");
}

void BaseOrderHandler::IsValidDirection(const Direction direction) {
  if (direction == Direction::NONE) {
    throw InvalidValue(
        "주어진 방향 NONE은 유효하지 않으며, "
        "LONG 혹은 SHORT으로 지정해야 합니다.");
  }
}

void BaseOrderHandler::IsValidPrice(const double price) {
  if (price <= 0 || isnan(price)) {
    throw InvalidValue(format("주어진 가격 {}은(는) 0보다 커야합니다.", price));
  }
}

void BaseOrderHandler::IsValidPositionSize(const double position_size) {
  if (position_size <= 0) {
    throw InvalidValue(
        format("주어진 포지션 크기 {}은(는) 0보다 커야합니다.", position_size));
  }
}

void BaseOrderHandler::IsValidLeverage(const int leverage) {
  if (leverage < 1) {
    throw InvalidValue(
        format("주어진 레버리지 {}은(는) 1과 같거나 커야합니다.", leverage));
  }
}

void BaseOrderHandler::IsValidEntryName(const string& entry_name) const {
  /* 같은 이름으로 체결된 Entry Name이 여러 개 존재하면, 청산 시 Target Entry
     지정할 때의 로직이 꼬이기 때문에 하나의 Entry Name은 하나의 진입 체결로
     제한 */
  for (const int symbol_idx = bar_->GetCurrentSymbolIndex();
       const auto& filled_entry : filled_entries_[symbol_idx]) {
    /* 체결된 진입 주문 중 같은 이름이 하나라도 존재하면
       해당 entry_name으로 진입 불가 */
    if (entry_name == filled_entry->GetEntryName()) {
      throw InvalidValue(format(
          "중복된 진입 이름 {}은(는) 동시에 체결될 수 없습니다.", entry_name));
    }
  }
}

void BaseOrderHandler::IsValidLimitOrderPrice(const double limit_price,
                                              const double base_price,
                                              const Direction direction) {
  if (direction == Direction::LONG && limit_price > base_price) {
    throw InvalidValue(
        format("지정가 {} 매수 주문은 기준가 {}과 같거나 작아야합니다.",
               limit_price, base_price));
  }

  if (direction == Direction::SHORT && limit_price < base_price) {
    throw InvalidValue(
        format("지정가 {} 매도 주문은 기준가 {}과 같거나 커야합니다.",
               limit_price, base_price));
  }
}

void BaseOrderHandler::IsValidTrailingTouchPrice(const double touch_price) {
  if (touch_price < 0) {
    throw InvalidValue(
        format("주어진 트레일링 터치 가격 {}은(는) 0과 같거나 커야합니다.",
               touch_price));
  }
}

void BaseOrderHandler::IsValidTrailPoint(double trail_point) {
  if (trail_point <= 0) {
    throw InvalidValue(format(
        "주어진 트레일링 포인트 {}은(는) 0보다 커야합니다.", trail_point));
  }
}

void BaseOrderHandler::LogFormattedInfo(const LogLevel log_level,
                                        const string& formatted_message,
                                        const char* file, const int line) {
  const auto& bar = bar_->GetBarData(bar_->GetCurrentBarType());
  const int symbol_idx = bar_->GetCurrentSymbolIndex();

  logger_->Log(
      log_level,
      format("{} | {} | {}", bar.GetSymbolName(symbol_idx),
             UtcTimestampToUtcDatetime(
                 bar.GetBar(symbol_idx, bar_->GetCurrentBarIndex()).open_time),
             formatted_message),
      file, line);
}
