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
#include "Engines/Logger.hpp"
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

  // 레버리지 벡터를 심볼 개수로 초기화
  // 초기 레버리지는 1
  leverages_.resize(num_symbols, 1);
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
  Bar base_bar{};
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

void BaseOrderHandler::Cancel(const string& order_name) {
  const int symbol_idx = bar_->GetCurrentSymbolIndex();
  auto& pending_entries = pending_entries_[symbol_idx];

  // 진입 대기 주문에서 같은 이름이 존재할 시 삭제
  for (int order_idx = 0; order_idx < pending_entries.size(); order_idx++) {
    if (const auto pending_entry = pending_entries[order_idx];
        order_name == pending_entry->GetEntryName()) {
      // 예약 증거금 회복 과정 진행 후 삭제
      ExecuteCancelEntry(pending_entry);
      pending_entries.erase(pending_entries.begin() + order_idx);

      LogFormattedInfo(
          LogLevel::ORDER_L,
          format("{} [{}] 주문이 취소되었습니다.",
                 Order::OrderTypeToString(pending_entry->GetEntryOrderType()),
                 order_name),
          __FILE__, __LINE__);
      break;  // 동일한 진입 이름으로 진입 대기 불가능하므로 찾으면 바로 break
    }
  }

  // 청산 대기 주문에서 같은 이름이 존재할 시 삭제
  auto& pending_exits = pending_exits_[symbol_idx];

  for (int order_idx = 0; order_idx < pending_exits.size(); order_idx++) {
    if (const auto pending_exit = pending_exits[order_idx];
        order_name == pending_exit->GetExitName()) {
      // 청산 대기 주문은 예약 증거금이 필요하지 않기 때문에 삭제만 함
      pending_exits.erase(pending_exits.begin() + order_idx);

      LogFormattedInfo(
          LogLevel::ORDER_L,
          format("{} [{}] 주문이 취소되었습니다.",
                 Order::OrderTypeToString(pending_exit->GetExitOrderType()),
                 order_name),
          __FILE__, __LINE__);
      break;  // 동일한 청산 이름으로 청산 대기 불가능하므로 찾으면 바로 break
    }
  }
}

void BaseOrderHandler::SetLeverage(const int leverage) {
  const auto symbol_idx = bar_->GetCurrentSymbolIndex();
  const auto current_leverage = leverages_[symbol_idx];

  // 현재 설정된 레버리지 값과 같으면 변경 없이 리턴
  if (current_leverage == leverage) {
    return;
  }

  // 체결된 진입 주문이 존재하면 레버리지 변경 불가
  if (!filled_entries_[symbol_idx].empty()) {
    LogFormattedInfo(LogLevel::WARNING_L,
                     format("체결된 진입 주문이 존재하므로 현재 레버리지 "
                            "[{}]에서 [{}](으)로 변경할 수 없습니다.",
                            current_leverage, leverage),
                     __FILE__, __LINE__);
    return;
  }

  // 레버리지가 유효한 값인지 확인
  // 최대 레버리지는 브라켓 첫 요소의 레버리지
  if (const auto max_leverage =
          symbol_info_[symbol_idx].GetLeverageBracket().front().max_leverage;
      IsLess(leverage, 1) || IsGreater(leverage, max_leverage)) {
    LogFormattedInfo(
        LogLevel::WARNING_L,
        format("주어진 레버리지 [{}]은(는) [1] 미만 이거나 최대 "
               "레버리지 [{}]을(를) 초과하기 때문에 변경할 수 없습니다.",
               leverage, max_leverage),
        __FILE__, __LINE__);
    return;
  }

  // 레버리지 변경
  leverages_[symbol_idx] = leverage;

  LogFormattedInfo(LogLevel::ORDER_L,
                   format("레버리지가 [{}](으)로 변경되었습니다.", leverage),
                   __FILE__, __LINE__);

  // 진입 대기 주문 확인
  for (const auto& pending_entry : pending_entries_[symbol_idx]) {
    try {
      double target_price = 0;

      // 주문 타입별로 체결 예상 시의 명목 가치를 계산하는 가격이 다르므로 분기
      switch (pending_entry->GetEntryOrderType()) {
        case OrderType::MARKET: {
          // 시장가는 대기 주문이 없음
          break;
        }

        case OrderType::LIMIT: {
          target_price = pending_entry->GetEntryOrderPrice();
          break;
        }

        case OrderType::MIT: {
          target_price = pending_entry->GetEntryTouchPrice();
          break;
        }

        case OrderType::LIT: {
          target_price = pending_entry->GetEntryOrderPrice();
          break;
        }

        case OrderType::TRAILING: {
          // 최고저가가 설정되지 않았으면 touch_price ± trail_point
          // 설정됐으면 extreme_price ± trail_point
          double start_price;
          if (const auto extreme_price = pending_entry->GetEntryExtremePrice();
              isnan(extreme_price)) {
            start_price = pending_entry->GetEntryTouchPrice();
          } else {
            start_price = extreme_price;
          }

          // 진입 방향별로 가장 불리한 진입가를 기준으로 명목 가치를 평가
          const auto trail_point = pending_entry->GetEntryTrailPoint();
          target_price = pending_entry->GetEntryDirection() == Direction::LONG
                             ? start_price + trail_point
                             : start_price - trail_point;

          break;
        }
        default:;
      }

      // 현재 주문의 명목 가치에 해당되는 레버리지 구간의 레버리지 최대값보다
      // 변경된 레버리지가 크면 대기 주문 유지 불가
      IsValidLeverage(target_price, pending_entry->GetEntryOrderSize());
    } catch (const InvalidValue& e) {
      LogFormattedInfo(LogLevel::WARNING_L, e.what(), __FILE__, __LINE__);

      // 현재 레버리지가 최대 레버리지를 초과하였다면 주문 취소
      Cancel(pending_entry->GetEntryName());
      continue;
    }

    // Limit 또는 LIT 주문으로 예약 증거금이 잡혀있으면 재설정
    if (const auto entry_margin = pending_entry->GetMargin();
        entry_margin != 0) {
      // 새로운 마진 계산
      // ON_CLOSE, AFTER 전략 모두에서 이 함수가 실행될 수 있으므로
      // 초기 마진 계산 시 미실현 손실을 계산하는 기준 가격 타입은 '시가'로 통일
      const auto updated_margin =
          CalculateMargin(pending_entry->GetEntryOrderPrice(),
                          pending_entry->GetEntryOrderSize(), PriceType::OPEN);

      // 마진 재설정 후 진입 가능 자금과 재비교
      engine_->DecreaseUsedMargin(entry_margin);
      pending_entry->SetMargin(updated_margin);

      const auto& order_type_str =
          Order::OrderTypeToString(pending_entry->GetEntryOrderType());
      try {
        HasEnoughBalance(engine_->UpdateAvailableBalance(), updated_margin,
                         "사용 가능", format("{} 진입 마진", order_type_str));
      } catch (const InsufficientBalance& e) {
        LogFormattedInfo(LogLevel::WARNING_L, e.what(), __FILE__, __LINE__);

        // Cancel 내부적으로 사용한 마진을 감소시키므로 중복 감소 방지
        pending_entry->SetMargin(0);

        // 새로운 마진을 충당할 수 없으면 주문 취소
        Cancel(pending_entry->GetEntryName());
        continue;
      }

      // 사용한 마진 증가
      engine_->IncreaseUsedMargin(updated_margin);
    }

    // 현재 주문의 레버리지 재설정
    pending_entry->SetLeverage(leverage);
  }
}

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

void BaseOrderHandler::LogFormattedInfo(const LogLevel log_level,
                                        const string& formatted_message,
                                        const char* file, const int line) {
  logger_->Log(log_level,
               format("[{}] [{}] | {}", engine_->GetCurrentStrategyName(),
                      bar_->GetBarData(bar_->GetCurrentBarType())
                          ->GetSymbolName(bar_->GetCurrentSymbolIndex()),
                      formatted_message),
               file, line);
}

int BaseOrderHandler::GetLeverage(const int symbol_idx) const {
  return leverages_[symbol_idx];
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
    const Direction entry_direction, const double order_price,
    const double position_size, const double entry_margin) {
  // 청산 가격
  // = (진입 마진 + 유지 금액 - (진입 가격 * 포지션 크기: 롱 양수, 숏 음수))
  //    / (포지션 크기 절댓값 * 유지 증거금율 - (포지션 크기: 롱 양수, 숏 음수))
  const auto symbol_idx = bar_->GetCurrentSymbolIndex();
  const auto& leverage_bracket =
      GetLeverageBracket(symbol_idx, order_price, position_size);
  const auto signed_position_size =
      entry_direction == Direction::LONG ? position_size : -position_size;

  const auto numerator = entry_margin + leverage_bracket.maintenance_amount -
                         order_price * signed_position_size;
  const auto denominator =
      position_size * leverage_bracket.maintenance_margin_rate -
      signed_position_size;

  if (const auto result = numerator / denominator; result <= 0) {
    // 롱의 경우, 청산가가 음수면 절대 청산되지 않음
    return 0;
  } else {
    return RoundToTickSize(result, symbol_info_[symbol_idx].GetTickSize());
  }
}

LeverageBracket BaseOrderHandler::GetLeverageBracket(
    const int symbol_idx, const double order_price,
    const double position_size) {
  const auto notional_value = order_price * position_size;

  for (const auto& leverage_bracket :
       symbol_info_[symbol_idx].GetLeverageBracket()) {
    if (leverage_bracket.min_notional_value <= notional_value &&
        notional_value < leverage_bracket.max_notional_value) {
      return leverage_bracket;
    }
  }

  throw InvalidValue(
      format("명목 가치 [{}]에 해당되는 레버리지 구간이 존재하지 않습니다.",
             FormatDollar(notional_value)));
}

double BaseOrderHandler::CalculateMargin(const double order_price,
                                         const double entry_size,
                                         const PriceType price_type) const {
  const auto symbol_idx = bar_->GetCurrentSymbolIndex();

  // 가격 * 수량 / 레버리지 + 해당 심볼의 미실현 손실의 절댓값
  return order_price * entry_size / leverages_[symbol_idx] +
         GetUnrealizedLoss(symbol_idx, price_type);
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
                   "초과했거나 시장가 최소 수량 [{}]보다 작습니다.",
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
                   "초과했거나 지정가 최소 수량 [{}]보다 작습니다.",
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

void BaseOrderHandler::IsValidLeverage(const double order_price,
                                       const double position_size) const {
  const auto symbol_idx = bar_->GetCurrentSymbolIndex();
  const auto current_leverage = leverages_[symbol_idx];

  if (const auto max_leverage =
          GetLeverageBracket(symbol_idx, order_price, position_size)
              .max_leverage;
      IsGreater(current_leverage, max_leverage)) {
    throw InvalidValue(format(
        "현재 레버리지 [{}]은(는) 명목 가치 [{}]에 해당되는 레버리지 "
        "구간의 최대 레버리지 [{}]을(를) 초과하기 때문에 주문할 수 없습니다.",
        current_leverage, FormatDollar(order_price * position_size),
        max_leverage));
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

void BaseOrderHandler::ExecuteCancelEntry(
    const shared_ptr<Order>& cancel_order) {
  switch (cancel_order->GetEntryOrderType()) {
    case OrderType::MARKET: {
      // 시장가는 바로 체결하므로 대기 주문이 없음
      return;
    }

    case OrderType::LIMIT: {
      // 사용한 자금에서 예약 증거금 감소
      if (const auto entry_margin = cancel_order->GetMargin();
          entry_margin != 0) {
        engine_->DecreaseUsedMargin(entry_margin);
      }
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
        if (const auto entry_margin = cancel_order->GetMargin();
            entry_margin != 0) {
          engine_->DecreaseUsedMargin(entry_margin);
        }
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