// 표준 라이브러리
#include <cmath>
#include <format>

// 파일 헤더
#include "Engines/BaseOrderHandler.hpp"

// 내부 헤더
#include "Engines/Analyzer.hpp"
#include "Engines/BarData.hpp"
#include "Engines/BarHandler.hpp"
#include "Engines/Config.hpp"
#include "Engines/DataUtils.hpp"
#include "Engines/Engine.hpp"
#include "Engines/Exception.hpp"
#include "Engines/Order.hpp"
#include "Engines/SymbolInfo.hpp"
#include "Engines/TechnicalAnalyzer.hpp"
#include "Engines/TimeUtils.hpp"

// 네임 스페이스
using namespace data_utils;
using namespace time_utils;

BaseOrderHandler::BaseOrderHandler()
    : current_position_size(0),
      taker_fee_(nan("")),
      maker_fee_(nan("")),
      taker_slippage_(nan("")),
      maker_slippage_(nan("")),
      just_entered_(false),
      just_exited_(false) {}
BaseOrderHandler::~BaseOrderHandler() = default;

shared_ptr<Analyzer>& BaseOrderHandler::analyzer_ = Analyzer::GetAnalyzer();
shared_ptr<BarHandler>& BaseOrderHandler::bar_ = BarHandler::GetBarHandler();
shared_ptr<Engine>& BaseOrderHandler::engine_ = Engine::GetEngine();
shared_ptr<Logger>& BaseOrderHandler::logger_ = Logger::GetLogger();
shared_ptr<TechnicalAnalyzer>& BaseOrderHandler::ta_ =
    TechnicalAnalyzer::GetTechnicalAnalyzer();
vector<SymbolInfo> BaseOrderHandler::symbol_info_;

void BaseOrderHandler::Initialize(const int num_symbols) {
  // 엔진 설정 받아오기
  const auto& config = Engine::GetConfig();
  taker_fee_ = config->GetTakerFee();
  maker_fee_ = config->GetMakerFee();
  taker_slippage_ = config->GetTakerSlippage();
  maker_slippage_ = config->GetMakerSlippage();

  // 주문들을 심볼 개수로 초기화
  pending_entries_.resize(num_symbols);
  filled_entries_.resize(num_symbols);
  pending_exits_.resize(num_symbols);

  // 마지막으로 진입 및 청산한 트레이딩 바 인덱스를 심볼 개수로 초기화
  last_entry_bar_indices_.resize(num_symbols);
  last_exit_bar_indices_.resize(num_symbols);

  // 아직 진입 및 청산이 없었던 심볼은 SIZE_MAX를 가짐
  ranges::fill(last_entry_bar_indices_, SIZE_MAX);
  ranges::fill(last_exit_bar_indices_, SIZE_MAX);

  // 마지막으로 진입 및 청산한 가격을 심볼 개수로 초기화
  last_entry_prices_.resize(num_symbols);
  last_exit_prices_.resize(num_symbols);

  // 아직 진입 및 청산이 없었던 심볼은 NaN을 가짐
  ranges::fill(last_entry_prices_, nan(""));
  ranges::fill(last_exit_prices_, nan(""));
}

void BaseOrderHandler::SetSymbolInfo(const vector<SymbolInfo>& symbol_info) {
  symbol_info_ = symbol_info;
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

void BaseOrderHandler::InitializeJustEntered() { just_entered_ = false; }
void BaseOrderHandler::InitializeJustExited() { just_exited_ = false; }

double BaseOrderHandler::GetUnrealizedLoss(const int symbol_idx,
                                           const PriceType price_type) const {
  // 체결된 주문이 없는 경우 0을 반환
  if (filled_entries_[symbol_idx].empty()) {
    return 0;
  }

  const auto original_bar_type = bar_->GetCurrentBarType();

  // 현재 마크 가격 바의 Close Time이 메인 Close Time과 같다면 유효하므로
  // 마크 가격 기준으로 Loss 계산. 그렇지 않다면 트레이딩 바를 기준으로 계산
  Bar base_bar;
  bar_->SetCurrentBarType(BarType::MARK_PRICE, "NONE");
  if (const auto& current_mark_bar =
          bar_->GetBarData(BarType::MARK_PRICE, "NONE")
              ->GetBar(symbol_idx, bar_->GetCurrentBarIndex());
      current_mark_bar.close_time == engine_->GetCurrentCloseTime()) {
    base_bar = current_mark_bar;
  } else {
    bar_->SetCurrentBarType(BarType::TRADING, "NONE");
    base_bar = bar_->GetBarData(BarType::TRADING, "NONE")
                   ->GetBar(symbol_idx, bar_->GetCurrentBarIndex());
  }

  double base_price = 0;
  switch (price_type) {
    case PriceType::OPEN: {
      base_price = base_bar.open;
      break;
    }

    case PriceType::HIGH: {
      base_price = base_bar.high;
      break;
    }

    case PriceType::LOW: {
      base_price = base_bar.low;
      break;
    }

    case PriceType::CLOSE: {
      base_price = base_bar.close;
      break;
    }
  }

  // 해당 심볼의 체결된 진입 주문 순회
  double sum_loss = 0;
  for (const auto& filled_entry : filled_entries_[symbol_idx]) {
    // 진입 방향에 따라 손실 합산
    // 부분 청산 시 남은 진입 물량만 미실현 손실에 포함됨
    const auto loss = CalculatePnl(
        filled_entry->GetEntryDirection(), base_price,
        filled_entry->GetEntryFilledPrice(),
        filled_entry->GetEntryFilledSize() - filled_entry->GetExitFilledSize());

    if (loss < 0) {
      sum_loss += abs(loss);
    }
  }

  bar_->SetCurrentBarType(original_bar_type, "NONE");
  return sum_loss;
}

bool BaseOrderHandler::GetJustEntered() const { return just_entered_; }
bool BaseOrderHandler::GetJustExited() const { return just_exited_; }

double BaseOrderHandler::BarsSinceEntry() const {
  // 전략 실행 시 무조건 트레이딩 바를 사용하므로 원본 바 타입은 저장하지 않음
  const auto last_entry_bar_index =
      last_entry_bar_indices_[bar_->GetCurrentSymbolIndex()];

  if (last_entry_bar_index == SIZE_MAX) {
    // 아직 진입이 없었던 심볼은 NaN을 반환 (기본 -1로 초기화 됨)
    return nan("");
  }

  // 진입이 있었던 심볼은 현재 바 인덱스와의 차이를 구해 반환
  return static_cast<double>(bar_->GetCurrentBarIndex() - last_entry_bar_index);
}
double BaseOrderHandler::BarsSinceExit() const {
  // 전략 실행 시 무조건 트레이딩 바를 사용하므로 원본 바 타입은 저장하지 않음
  const auto last_exit_bar_index =
      last_exit_bar_indices_[bar_->GetCurrentSymbolIndex()];

  if (last_exit_bar_index == SIZE_MAX) {
    // 아직 진입이 없었던 심볼은 NaN을 반환 (기본 SIZE_MAX로 초기화 됨)
    return nan("");
  }

  // 진입이 있었던 심볼은 현재 바 인덱스와의 차이를 구해 반환
  return static_cast<double>(bar_->GetCurrentBarIndex() - last_exit_bar_index);
}

double BaseOrderHandler::LastEntryPrice() const {
  return last_entry_prices_[bar_->GetCurrentSymbolIndex()];
}

double BaseOrderHandler::LastExitPrice() const {
  return last_exit_prices_[bar_->GetCurrentSymbolIndex()];
}

double BaseOrderHandler::CalculateSlippagePrice(
    const OrderType order_type, const Direction direction,
    const double order_price) const {
  double slippage_points = 0;

  // 시장가, 지정가에 따라 슬리피지가 달라짐
  switch (order_type) {
    case OrderType::MARKET:
      [[fallthrough]];
    case OrderType::MIT:
      [[fallthrough]];
    case OrderType::TRAILING: {
      // 테이커 슬리피지 포인트 계산
      slippage_points = order_price * taker_slippage_ / 100;
      break;
    }

    case OrderType::LIMIT:
      [[fallthrough]];
    case OrderType::LIT: {
      // 메이커 슬리피지 포인트 계산
      slippage_points = order_price * maker_slippage_ / 100;
      break;
    }

    case OrderType::NONE: {
      break;
    }
  }

  // 계산된 슬리피지 포인트가 0이면 슬리피지는 없음
  if (IsEqual(slippage_points, 0.0)) {
    return order_price;
  }

  // 방향에 따라 덧셈과 뺄셈이 달라짐
  if (direction == Direction::LONG) {
    return RoundToTickSize(
        order_price + slippage_points,
        symbol_info_[bar_->GetCurrentSymbolIndex()].GetTickSize());
  }

  if (direction == Direction::SHORT) {
    return RoundToTickSize(
        order_price - slippage_points,
        symbol_info_[bar_->GetCurrentSymbolIndex()].GetTickSize());
  }

  throw runtime_error("슬리피지 계산 오류");
}

double BaseOrderHandler::CalculateTradingFee(const OrderType order_type,
                                             const double filled_price,
                                             const double filled_size) const {
  // 테이커, 메이커에 따라 수수료가 달라짐
  switch (order_type) {
    case OrderType::MARKET:
      [[fallthrough]];
    case OrderType::MIT:
      [[fallthrough]];
    case OrderType::TRAILING: {
      return filled_price * filled_size * (taker_fee_ / 100);
    }

    case OrderType::LIMIT:
      [[fallthrough]];
    case OrderType::LIT: {
      return filled_price * filled_size * (maker_fee_ / 100);
    }

    default: {
      return nan("");
    }
  }
}

double BaseOrderHandler::CalculateLiquidationPrice(
    const int leverage, const Direction entry_direction,
    const double entry_filled_price) {
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

double BaseOrderHandler::CalculateMargin(const double order_price,
                                         const double entry_size,
                                         const int leverage,
                                         const PriceType price_type) const {
  // 가격 * 수량 / 레버리지 + 해당 심볼의 미실현 손실
  return order_price * entry_size / leverage +
         GetUnrealizedLoss(bar_->GetCurrentSymbolIndex(), price_type);
}

double BaseOrderHandler::CalculatePnl(const Direction entry_direction,
                                      const double base_price,
                                      const double entry_price,
                                      const double position_size) {
  if (entry_direction == Direction::LONG) {
    return (base_price - entry_price) * position_size;
  }

  if (entry_direction == Direction::SHORT) {
    return (entry_price - base_price) * position_size;
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
  if (IsLessOrEqual(price, 0.0) || isnan(price)) {
    throw InvalidValue(
        format("주어진 가격 [{}]은(는) 양수로 지정해야 합니다.", price));
  }
}

void BaseOrderHandler::IsValidPositionSize(const double position_size,
                                           const OrderType order_type) {
  if (IsLessOrEqual(position_size, 0.0)) {
    throw InvalidValue(
        format("주어진 포지션 크기 [{}]은(는) 양수로 지정해야 합니다.",
               position_size));
  }

  const auto& symbol_info = symbol_info_[bar_->GetCurrentSymbolIndex()];

  // 포지션 수량 단위 확인
  if (const auto qty_step = symbol_info.GetQtyStep();
      round(position_size / qty_step) * qty_step != position_size) {
    throw InvalidValue(
        format("주어진 포지션 크기 [{}]은(는) 포지션 수량 단위 [{}]의 배수가 "
               "아닙니다.",
               position_size, qty_step));
  }

  // 수량 제한 확인
  switch (order_type) {
    case OrderType::MARKET:
      [[fallthrough]];
    case OrderType::MIT:
      [[fallthrough]];
    case OrderType::TRAILING: {
      // 시장가 최고 수량보다 많거나 최저 수량보다 적으면 오류
      const auto max_qty = symbol_info.GetMarketMaxQty();
      if (const auto min_qty = symbol_info.GetMarketMinQty();
          IsGreater(position_size, max_qty) || IsLess(position_size, min_qty)) {
        throw InvalidValue(
            format("주어진 포지션 크기 [{}]은(는) 시장가 최대 수량 [{}]을(를) "
                   "초과했거나 "
                   "시장가 최소 수량 [{}]보다 작습니다.",
                   position_size, max_qty, min_qty));
      }
      break;
    }
    case OrderType::LIMIT:
      [[fallthrough]];
    case OrderType::LIT: {
      // 지정가 최고 수량보다 많거나 최저 수량보다 적으면 오류
      const auto max_qty = symbol_info.GetLimitMaxQty();
      if (const auto min_qty = symbol_info.GetLimitMinQty();
          IsGreater(position_size, max_qty) || IsLess(position_size, min_qty)) {
        throw InvalidValue(
            format("주어진 포지션 크기 [{}]은(는) 지정가 최대 수량 [{}]을(를) "
                   "초과했거나 "
                   "지정가 최소 수량 [{}]보다 작습니다.",
                   position_size, max_qty, min_qty));
      }
      break;
    }

    case OrderType::NONE: {
      Logger::LogAndThrowError("잘못된 주문 타입이 지정되었습니다.", __FILE__,
                               __LINE__);
    }
  }
}

void BaseOrderHandler::IsValidNotionalValue(const double order_price,
                                            const double position_size) {
  // 명목 가치가 해당 심볼의 최소 명목 가치보다 작으면 오류
  const auto notional = order_price * position_size;
  if (const auto min_notional =
          symbol_info_[bar_->GetCurrentSymbolIndex()].GetMinNotional();
      IsLess(notional, min_notional)) {
    throw InvalidValue(
        format("주어진 주문 가격 [{}]과 포지션 크기 [{}]의 곱인 명목 가치 "
               "[{}]는 최소 명목 가치 [{}]보다 커야 합니다.",
               order_price, position_size,
               FormatDollar(RoundToDecimalPlaces(notional, 2)),
               FormatDollar(RoundToDecimalPlaces(min_notional, 2))));
  }
}

void BaseOrderHandler::IsValidLeverage(const int leverage) {
  if (IsLess(leverage, 1)) {
    throw InvalidValue(format(
        "주어진 레버리지 [{}]은(는) 1보다 크거나 같아야 합니다.", leverage));
  }
}

void BaseOrderHandler::IsValidEntryName(const string& entry_name) const {
  /* 같은 이름으로 체결된 Entry Name이 여러 개 존재하면, 청산 시 Target Entry
     지정할 때의 로직이 꼬이기 때문에 하나의 Entry Name은 하나의 진입 체결로
     제한 */
  for (const auto& filled_entry :
       filled_entries_[bar_->GetCurrentSymbolIndex()]) {
    /* 체결된 진입 주문 중 같은 이름이 하나라도 존재하면
       해당 entry_name으로 진입 불가 */
    if (entry_name == filled_entry->GetEntryName()) {
      throw InvalidValue(
          format("중복된 진입 이름 [{}]은(는) 동시에 체결될 수 없습니다.",
                 entry_name));
    }
  }
}

void BaseOrderHandler::IsValidLimitOrderPrice(const double limit_price,
                                              const double base_price,
                                              const Direction direction) {
  if (direction == Direction::LONG && IsGreater(limit_price, base_price)) {
    throw InvalidValue(
        format("지정가 [{}] 매수 주문은 기준가 [{}]보다 작거나 같아야 합니다.",
               limit_price, base_price));
  }

  if (direction == Direction::SHORT && IsLess(limit_price, base_price)) {
    throw InvalidValue(
        format("지정가 [{}] 매도 주문은 기준가 [{}]보다 크거나 같아야 합니다.",
               limit_price, base_price));
  }
}

void BaseOrderHandler::IsValidTrailingTouchPrice(const double touch_price) {
  if (IsLess(touch_price, 0.0)) {
    throw InvalidValue(format(
        "주어진 트레일링 터치 가격 [{}]은(는) 0보다 크거나 같아야 합니다.",
        touch_price));
  }
}

void BaseOrderHandler::IsValidTrailPoint(double trail_point) {
  if (IsLessOrEqual(trail_point, 0.0)) {
    throw InvalidValue(
        format("주어진 트레일링 포인트 [{}]은(는) 양수로 지정해야 합니다.",
               trail_point));
  }
}

bool BaseOrderHandler::IsLimitPriceSatisfied(const Direction order_direction,
                                             const double price,
                                             const double order_price) {
  return (order_direction == Direction::LONG &&
          IsLessOrEqual(price, order_price)) ||
         (order_direction == Direction::SHORT &&
          IsGreaterOrEqual(price, order_price));
}

bool BaseOrderHandler::IsPriceTouched(const Direction touch_direction,
                                      const double price,
                                      const double touch_price) {
  return (touch_direction == Direction::LONG &&
          IsGreaterOrEqual(price, touch_price)) ||
         (touch_direction == Direction::SHORT &&
          IsLessOrEqual(price, touch_price));
}

void BaseOrderHandler::HasEnoughBalance(const double balance,
                                        const double needed_balance,
                                        const string& balance_type_msg,
                                        const string& purpose_msg) {
  if (IsLess(balance, needed_balance)) {
    throw InsufficientBalance(
        format("자금이 부족합니다. | {} 자금: {} | {}: {}", balance_type_msg,
               FormatDollar(RoundToDecimalPlaces(balance, 2)), purpose_msg,
               FormatDollar(RoundToDecimalPlaces(needed_balance, 2))));
  }
}

void BaseOrderHandler::UpdateLastEntryBarIndex(const int symbol_idx) {
  const auto original_bar_type = bar_->GetCurrentBarType();

  bar_->SetCurrentBarType(BarType::TRADING, "NONE");

  last_entry_bar_indices_[symbol_idx] = bar_->GetCurrentBarIndex();

  // 진입 시에는 트레이딩, 돋보기 바만 사용하며, 진입 바 인덱스는 진입 시에만
  // 업데이트 되므로 타임프레임은 필요하지 않음
  bar_->SetCurrentBarType(original_bar_type, "NONE");
}

void BaseOrderHandler::UpdateLastExitBarIndex(const int symbol_idx) {
  const auto original_bar_type = bar_->GetCurrentBarType();

  bar_->SetCurrentBarType(BarType::TRADING, "NONE");

  last_exit_bar_indices_[symbol_idx] = bar_->GetCurrentBarIndex();

  // 진입 시에는 트레이딩, 돋보기 바만 사용하며, 진입 바 인덱스는 진입 시에만
  // 업데이트 되므로 타임프레임은 필요하지 않음
  bar_->SetCurrentBarType(original_bar_type, "NONE");
}