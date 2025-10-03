// 표준 라이브러리
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

// 네임 스페이스
namespace backtesting {
using namespace exception;
using namespace utils;
}  // namespace backtesting

namespace backtesting::order {

BaseOrderHandler::BaseOrderHandler()
    : current_position_size_(0),
      just_entered_(false),
      just_exited_(false),
      is_reverse_exit_(false),
      reverse_exit_price_(NAN),
      is_initialized_(false) {}
BaseOrderHandler::~BaseOrderHandler() = default;

shared_ptr<Analyzer>& BaseOrderHandler::analyzer_ = Analyzer::GetAnalyzer();
shared_ptr<BarHandler>& BaseOrderHandler::bar_ = BarHandler::GetBarHandler();
shared_ptr<Config>& BaseOrderHandler::config_ = Engine::GetConfig();
shared_ptr<Engine>& BaseOrderHandler::engine_ = Engine::GetEngine();
shared_ptr<Logger>& BaseOrderHandler::logger_ = Logger::GetLogger();
vector<SymbolInfo> BaseOrderHandler::symbol_info_;

void BaseOrderHandler::Cancel(const string& order_name) {
  const int symbol_idx = bar_->GetCurrentSymbolIndex();
  auto& pending_entries = pending_entries_[symbol_idx];
  auto& pending_exits = pending_exits_[symbol_idx];

  // 진입 대기 주문에서 같은 이름이 존재할 시 삭제
  for (int order_idx = 0; order_idx < pending_entries.size(); order_idx++) {
    if (const auto pending_entry = pending_entries[order_idx];
        order_name == pending_entry->GetEntryName()) {
      // 예약 증거금 회복 과정 진행 후 삭제
      DecreaseUsedMarginOnEntryCancel(pending_entry);
      pending_entries.erase(pending_entries.begin() + order_idx);

      LogFormattedInfo(
          INFO_L,
          format("{} [{}] 주문 취소",
                 Order::OrderTypeToString(pending_entry->GetEntryOrderType()),
                 order_name),
          __FILE__, __LINE__);
      engine_->LogBalance();

      // 동일한 진입 이름으로 진입 대기 불가능하므로 찾으면 바로 break
      // (동일한 진입 이름으로 주문 시 기존 주문이 수정됨)
      break;
    }
  }

  // 청산 대기 주문에서 같은 이름이 존재할 시 삭제
  for (int order_idx = 0; order_idx < pending_exits.size(); order_idx++) {
    if (const auto pending_exit = pending_exits[order_idx];
        order_name == pending_exit->GetExitName()) {
      // 청산 대기 주문은 예약 증거금이 필요하지 않기 때문에 삭제만 함
      pending_exits.erase(pending_exits.begin() + order_idx);

      LogFormattedInfo(
          INFO_L,
          format("{} [{}] 주문 취소",
                 Order::OrderTypeToString(pending_exit->GetExitOrderType()),
                 order_name),
          __FILE__, __LINE__);

      // 동일한 청산 이름으로 청산 대기 불가능하므로 찾으면 바로 break
      // (동일한 청산 이름으로 주문 시 기존 주문이 수정됨)
      break;
    }
  }
}

double BaseOrderHandler::BarsSinceEntry() const {
  // 전략 실행 시 무조건 트레이딩 바를 사용하므로 원본 바 타입은 저장하지 않음
  const auto last_entry_bar_index =
      last_entry_bar_indices_[bar_->GetCurrentSymbolIndex()];

  if (last_entry_bar_index == SIZE_MAX) {
    // 아직 진입이 없었던 심볼은 NaN을 반환 (기본 SIZE_MAX로 초기화 됨)
    return NAN;
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
    return NAN;
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

double BaseOrderHandler::GetCurrentPositionSize() const {
  return current_position_size_;
}

double BaseOrderHandler::GetUnrealizedLoss(const int symbol_idx,
                                           const PriceType price_type) const {
  const auto& filled_entries = filled_entries_[symbol_idx];

  // 체결된 주문이 없는 경우 0을 반환
  if (filled_entries.empty()) {
    return 0.0;
  }

  const auto original_bar_type = bar_->GetCurrentBarType();

  // 현재 마크 가격 바의 Close Time이 현재 진행 중인 Close Time과 같다면
  // 유효하므로 마크 가격 기준으로 Loss 계산. 그렇지 않다면 전략을 실행한 바
  // 타입을 기준으로 계산 (트레이딩 바 or 돋보기 바)
  Bar base_bar{};
  bar_->SetCurrentBarType(MARK_PRICE, "");
  if (const auto& current_mark_bar =
          bar_->GetBarData(MARK_PRICE)
              ->GetBar(symbol_idx, bar_->GetCurrentBarIndex());
      current_mark_bar.close_time == engine_->GetCurrentCloseTime()) {
    base_bar = current_mark_bar;
  } else {
    bar_->SetCurrentBarType(original_bar_type, "");
    base_bar = bar_->GetBarData(original_bar_type)
                   ->GetBar(symbol_idx, bar_->GetCurrentBarIndex());
  }

  double base_price = 0;
  switch (price_type) {
    case OPEN: {
      base_price = base_bar.open;
      break;
    }

    case HIGH: {
      base_price = base_bar.high;
      break;
    }

    case LOW: {
      base_price = base_bar.low;
      break;
    }

    case CLOSE: {
      base_price = base_bar.close;
      break;
    }
  }

  // 해당 심볼의 체결된 진입 주문 순회
  double sum_loss = 0;
  for (const auto& filled_entry : filled_entries) {
    // 진입 방향에 따라 손실 합산
    // 부분 청산 시 남은 진입 물량만 미실현 손실에 포함됨
    const auto pnl = CalculatePnl(
        filled_entry->GetEntryDirection(), base_price,
        filled_entry->GetEntryFilledPrice(),
        filled_entry->GetEntryFilledSize() - filled_entry->GetExitFilledSize());

    if (IsLess(pnl, 0.0)) {
      sum_loss += fabs(pnl);
    }
  }

  bar_->SetCurrentBarType(original_bar_type, "");
  return sum_loss;
}

bool BaseOrderHandler::IsJustEntered() const { return just_entered_; }
bool BaseOrderHandler::IsJustExited() const { return just_exited_; }

void BaseOrderHandler::LogFormattedInfo(const LogLevel log_level,
                                        const string& formatted_message,
                                        const char* file, const int line) {
  logger_->Log(log_level,
               format("[{}] {}",
                      bar_->GetBarData(bar_->GetCurrentBarType())
                          ->GetSymbolName(bar_->GetCurrentSymbolIndex()),
                      formatted_message),
               file, line);
}

double BaseOrderHandler::CalculateMargin(const double price,
                                         const double entry_size,
                                         const PriceType price_type,
                                         const int symbol_idx) const {
  // 가격 * 수량 / 레버리지 + 해당 심볼의 미실현 손실의 절댓값
  return price * entry_size / leverages_[symbol_idx] +
         GetUnrealizedLoss(symbol_idx, price_type);
}

double BaseOrderHandler::CalculateLiquidationPrice(
    const Direction entry_direction, const double order_price,
    const double position_size, const double margin, const int symbol_idx) {
  // 청산 가격
  // = (마진 + 유지 금액 - (진입 가격 * 포지션 크기: 롱 양수, 숏 음수))
  //    / (포지션 크기 절댓값 * 유지 증거금율 - (포지션 크기: 롱 양수, 숏 음수))

  const auto abs_position_size = fabs(position_size);
  const auto signed_position_size =
      entry_direction == LONG ? abs_position_size : -abs_position_size;
  const auto& leverage_bracket =
      GetLeverageBracket(symbol_idx, order_price, abs_position_size);

  const double numerator = margin + leverage_bracket.maintenance_amount -
                           order_price * signed_position_size;
  const double denominator =
      abs_position_size * leverage_bracket.maintenance_margin_rate -
      signed_position_size;

  if (const double result = numerator / denominator;
      IsLessOrEqual(result, 0.0)) {
    // 롱의 경우, 강제 청산 가격이 0 이하면 절대 청산되지 않음
    return 0.0;
  } else {
    return RoundToStep(result, symbol_info_[symbol_idx].GetTickSize());
  }
}

optional<string> BaseOrderHandler::AdjustLeverage(const int leverage,
                                                  const int symbol_idx) {
  const auto current_leverage = leverages_[symbol_idx];

  // 현재 설정된 레버리지 값과 같으면 변경 없이 리턴
  if (current_leverage == leverage) {
    return nullopt;
  }

  // 체결된 진입 주문이 존재하면 레버리지 변경 불가
  if (!filled_entries_[symbol_idx].empty()) {
    return format("레버리지 [{}x] → [{}x] 변경 불가 (체결된 진입 주문 존재)",
                  current_leverage, leverage);
  }

  // 레버리지 변경
  leverages_[symbol_idx] = leverage;

  LogFormattedInfo(
      INFO_L, format("레버리지 [{}x] → [{}x] 변경", current_leverage, leverage),
      __FILE__, __LINE__);

  // 진입 대기 주문 확인
  // (현재 레버리지가 최대 레버리지를 초과하였다면 주문이 삭제되므로 역순 순회)
  const auto& pending_entries = pending_entries_[symbol_idx];
  for (int order_idx = static_cast<int>(pending_entries.size()) - 1;
       order_idx >= 0; order_idx--) {
    const auto& pending_entry = pending_entries[order_idx];

    double order_price = 0;

    // 주문 타입별로 체결 예상 시의 명목 가치를 계산하는 가격이 다르므로 분기
    switch (pending_entry->GetEntryOrderType()) {
      case MARKET:
        [[fallthrough]];
      case LIMIT:
        [[fallthrough]];
      case LIT: {
        order_price = pending_entry->GetEntryOrderPrice();
        break;
      }

      case MIT: {
        order_price = pending_entry->GetEntryTouchPrice();
        break;
      }

      case TRAILING: {
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
        order_price = pending_entry->GetEntryDirection() == LONG
                          ? start_price + trail_point
                          : start_price - trail_point;

        break;
      }

      default:;
    }

    // 현재 주문의 명목 가치에 해당되는 레버리지 구간의 레버리지 최대값보다
    // 변경된 레버리지가 크면 대기 주문 유지 불가
    if (const auto& warn =
            IsValidLeverage(leverage, order_price,
                            pending_entry->GetEntryOrderSize(), symbol_idx)) {
      LogFormattedInfo(WARN_L, *warn, __FILE__, __LINE__);

      // 현재 레버리지가 최대 레버리지를 초과하였다면 주문 취소
      Cancel(pending_entry->GetEntryName());
      continue;
    }

    // Limit 또는 LIT 주문으로 예약 증거금이 잡혀있으면 재설정
    if (const auto entry_margin = pending_entry->GetEntryMargin();
        !IsEqual(entry_margin, 0.0)) {
      // 새로운 마진 계산
      // ON_CLOSE, AFTER 전략 모두에서 이 함수가 실행될 수 있으므로
      // 초기 마진 계산 시 미실현 손실을 계산하는 기준 가격 타입은 '시가'로 통일
      const auto updated_margin =
          CalculateMargin(pending_entry->GetEntryOrderPrice(),
                          pending_entry->GetEntryOrderSize(), OPEN, symbol_idx);

      // 마진 재설정 후 진입 가능 자금과 재비교
      engine_->DecreaseUsedMargin(entry_margin);
      pending_entry->SetEntryMargin(updated_margin)
          .SetLeftMargin(updated_margin);

      const auto& order_type_str =
          Order::OrderTypeToString(pending_entry->GetEntryOrderType());

      if (const auto& warn =
              HasEnoughBalance(engine_->GetAvailableBalance(), updated_margin,
                               "사용 가능", order_type_str + " 진입 마진")) {
        LogFormattedInfo(WARN_L, *warn, __FILE__, __LINE__);

        // 위쪽에서 마진 재설정을 위해 DecreaseUsedMargin 함수를 이미 호출했고,
        // Cancel 함수에서 내부적으로 사용한 마진을 감소시키므로 중복 감소 방지
        pending_entry->SetEntryMargin(0);

        // 새로운 마진을 충당할 수 없으면 주문 취소
        Cancel(pending_entry->GetEntryName());
        continue;
      }

      // 사용 가능 자금이 충분하다면 사용한 마진을 증가하여 주문 마진 재설정
      engine_->IncreaseUsedMargin(updated_margin);
    }

    // 최종적으로 문제가 없다면 현재 주문의 레버리지 재설정
    pending_entry->SetLeverage(leverage);
  }

  return nullopt;
}

int BaseOrderHandler::GetLeverage(const int symbol_idx) const {
  return leverages_[symbol_idx];
}

double BaseOrderHandler::CalculateSlippagePrice(const OrderType order_type,
                                                const Direction direction,
                                                const double order_price,
                                                const int symbol_idx) {
  double slippage_points = 0;

  // 시장가, 지정가에 따라 슬리피지가 달라짐
  switch (order_type) {
    case MARKET:
      [[fallthrough]];
    case MIT:
      [[fallthrough]];
    case TRAILING: {
      // 테이커 슬리피지 포인트 계산
      slippage_points =
          order_price * config_->GetTakerSlippagePercentage() / 100;
      break;
    }

    case LIMIT:
      [[fallthrough]];
    case LIT: {
      // 메이커 슬리피지 포인트 계산
      slippage_points =
          order_price * config_->GetMakerSlippagePercentage() / 100;
      break;
    }

    [[unlikely]] case ORDER_NONE: {
      Logger::LogAndThrowError(
          "슬리피지 계산 오류: 주문 타입이 NONE으로 지정됨.", __FILE__,
          __LINE__);
    }
  }

  // 계산된 슬리피지 포인트가 0이면 슬리피지는 없음
  if (IsEqual(slippage_points, 0.0)) {
    return order_price;
  }

  // 방향에 따라 덧셈과 뺄셈이 달라짐
  if (direction == LONG) {
    return RoundToStep(order_price + slippage_points,
                       symbol_info_[symbol_idx].GetTickSize());
  }

  if (direction == SHORT) {
    return RoundToStep(order_price - slippage_points,
                       symbol_info_[symbol_idx].GetTickSize());
  }

  [[unlikely]] Logger::LogAndThrowError(
      "슬리피지 계산 오류: 체결 방향이 NONE으로 지정됨.", __FILE__, __LINE__);
  [[unlikely]] throw;
}

double BaseOrderHandler::CalculateTradingFee(const OrderType order_type,
                                             const double filled_price,
                                             const double filled_size) {
  // 테이커, 메이커에 따라 수수료가 달라짐
  switch (order_type) {
    case MARKET:
      [[fallthrough]];
    case MIT:
      [[fallthrough]];
    case TRAILING: {
      return filled_price * filled_size *
             (config_->GetTakerFeePercentage() / 100);
    }

    case LIMIT:
      [[fallthrough]];
    case LIT: {
      return filled_price * filled_size *
             (config_->GetMakerFeePercentage() / 100);
    }

    [[unlikely]] case ORDER_NONE: {
      Logger::LogAndThrowError("수수료 계산 오류: 주문 타입이 NONE으로 지정됨.",
                               __FILE__, __LINE__);
      return NAN;
    }
  }

  [[unlikely]] return NAN;
}

LeverageBracket BaseOrderHandler::GetLeverageBracket(
    const int symbol_idx, const double order_price,
    const double position_size) {
  const auto notional_value = order_price * position_size;

  for (const auto& leverage_bracket :
       symbol_info_[symbol_idx].GetLeverageBracket()) {
    // 최소 명목 가치 <= 주문의 명목 가치 < 최대 명목 가치
    if (IsLessOrEqual(leverage_bracket.min_notional_value, notional_value) &&
        IsLess(notional_value, leverage_bracket.max_notional_value)) {
      return leverage_bracket;
    }
  }

  [[unlikely]] Logger::LogAndThrowError(
      format("엔진 오류: 명목 가치 [{}]에 해당되는 레버리지 구간 미존재",
             FormatDollar(notional_value, true)),
      __FILE__, __LINE__);
  [[unlikely]] throw;
}

double BaseOrderHandler::CalculatePnl(const Direction entry_direction,
                                      const double base_price,
                                      const double entry_price,
                                      const double position_size) {
  if (entry_direction == LONG) {
    return (base_price - entry_price) * position_size;
  }

  if (entry_direction == SHORT) {
    return (entry_price - base_price) * position_size;
  }

  [[unlikely]] Logger::LogAndThrowError("손익 계산 중 방향 오지정", __FILE__,
                                        __LINE__);
  [[unlikely]] return NAN;
}

optional<string> BaseOrderHandler::IsValidDirection(const Direction direction) {
  if (direction == DIRECTION_NONE) [[unlikely]] {
    return format("방향 [NONE] 오류 (조건: [LONG] 또는 [SHORT])");
  }

  return nullopt;
}

optional<string> BaseOrderHandler::IsValidPrice(const double price) {
  if (IsLessOrEqual(price, 0.0) || isnan(price)) [[unlikely]] {
    return format("가격 [{}] 오류 (조건: 0 초과 및 NaN이 아닌 실수)", price);
  }

  return nullopt;
}

optional<string> BaseOrderHandler::IsValidPositionSize(
    const double position_size, const OrderType order_type,
    const int symbol_idx) const {
  if (IsLessOrEqual(position_size, 0.0)) [[unlikely]] {
    return format("포지션 크기 [{}] 미달 (조건: 0 초과)", position_size);
  }

  // 포지션 수량 단위 확인
  const auto& symbol_info = symbol_info_[symbol_idx];
  if (const auto qty_step = symbol_info.GetQtyStep();
      !IsEqual(RoundToStep(position_size, qty_step), position_size)) {
    return format("포지션 크기 [{}] 지정 오류 (조건: 수량 단위 [{}]의 배수)",
                  position_size, qty_step);
  }

  // 수량 제한 확인
  if (!is_reverse_exit_) {
    switch (order_type) {
      case MARKET:
        [[fallthrough]];
      case MIT:
        [[fallthrough]];
      case TRAILING: {
        // 시장가 최고 수량보다 많거나 최저 수량보다 적으면 오류
        const auto max_qty = symbol_info.GetMarketMaxQty();
        if (const auto min_qty = symbol_info.GetMarketMinQty();
            IsGreater(position_size, max_qty) ||
            IsLess(position_size, min_qty)) {
          return format(
              "포지션 크기 [{}] 지정 오류 (조건: 시장가 최대 수량 [{}] 이하 및 "
              "최소 수량 [{}] 이상)",
              position_size, max_qty, min_qty);
        }

        break;
      }

      case LIMIT:
        [[fallthrough]];
      case LIT: {
        // 지정가 최고 수량보다 많거나 최저 수량보다 적으면 오류
        const auto max_qty = symbol_info.GetLimitMaxQty();
        if (const auto min_qty = symbol_info.GetLimitMinQty();
            IsGreater(position_size, max_qty) ||
            IsLess(position_size, min_qty)) {
          return format(
              "포지션 크기 [{}] 지정 오류 (조건: 지정가 최대 수량 [{}] 이하 및 "
              "최소 수량 [{}] 이상)",
              position_size, max_qty, min_qty);
        }

        break;
      }

      case ORDER_NONE:
        [[unlikely]] {
          Logger::LogAndThrowError(
              "엔진 오류: 포지션 크기 계산 중 주문 타입 오류", __FILE__,
              __LINE__);
        }
    }
  }

  return nullopt;
}

optional<string> BaseOrderHandler::IsValidNotionalValue(
    const double order_price, const double position_size,
    const int symbol_idx) {
  // 명목 가치가 해당 심볼의 최소 명목 가치보다 작으면 오류
  const auto notional = order_price * position_size;
  if (const auto min_notional = symbol_info_[symbol_idx].GetMinNotionalValue();
      IsLess(notional, min_notional)) {
    return format("명목 가치 [{}] 부족 (조건: 심볼의 최소 명목 가치 [{}] 이상)",
                  FormatDollar(notional, true),
                  FormatDollar(min_notional, true));
  }

  return nullopt;
}

optional<string> BaseOrderHandler::IsValidLeverage(const int leverage,
                                                   const double order_price,
                                                   const double position_size,
                                                   const int symbol_idx) {
  if (const auto max_leverage =
          GetLeverageBracket(symbol_idx, order_price, position_size)
              .max_leverage;
      leverage < 1 || leverage > max_leverage) {
    return format(
        "레버리지 [{}x] 조건 미만족 (조건: [1x] 이상 및 명목 가치 [{}] "
        "레버리지 구간의 최대 레버리지 [{}x] 이하)",
        leverage, FormatDollar(order_price * position_size, true),
        max_leverage);
  }

  return nullopt;
}

optional<string> BaseOrderHandler::IsValidEntryName(
    const string& entry_name, const int symbol_idx) const {
  /* 같은 이름으로 체결된 Entry Name이 여러 개 존재하면, 청산 시 Target Entry
     지정할 때의 로직이 꼬이기 때문에 하나의 Entry Name은 하나의 진입 체결로
     제한 */
  for (const auto& filled_entry : filled_entries_[symbol_idx]) {
    /* 체결된 진입 주문 중 같은 이름이 하나라도 존재하면
       해당 entry_name으로 진입 불가 */
    if (entry_name == filled_entry->GetEntryName()) {
      return format("중복된 진입 이름 [{}] 동시 체결 불가", entry_name);
    }
  }

  return nullopt;
}

optional<string> BaseOrderHandler::IsValidExitName(
    const string& exit_name) const {
  // 강제 청산을 청산 이름으로 사용하면 혼선이 있을 수 있으며,
  // 백보드에서 강제 청산 카운트에서 오류가 생기므로 원칙적 금지
  if (exit_name.find("강제 청산") != string::npos) [[unlikely]] {
    return "청산 이름에 \"강제 청산\" 단어 포함 금지";
  }

  // 리버스는 리버스 청산을 위한 시스템 이름이므로 사용 금지
  if (!is_reverse_exit_) {
    if (exit_name.find("리버스") != string::npos) [[unlikely]] {
      return "청산 이름에 \"리버스\" 단어 포함 금지";
    }
  }

  return nullopt;
}

optional<string> BaseOrderHandler::IsValidLimitOrderPrice(
    const double limit_price, const double base_price,
    const Direction direction) {
  if (direction == LONG && IsGreater(limit_price, base_price)) {
    return format("지정가 [{}]에서 주문 불가 (조건: 기준가 [{}] 이하",
                  limit_price, base_price);
  }

  if (direction == SHORT && IsLess(limit_price, base_price)) {
    return format("지정가 [{}]에서 주문 불가 (조건: 기준가 [{}] 이상",
                  limit_price, base_price);
  }

  return nullopt;
}

optional<string> BaseOrderHandler::IsValidTrailingTouchPrice(
    const double touch_price) {
  if (IsLess(touch_price, 0.0)) [[unlikely]] {
    return format("트레일링 터치 가격 [{}] 미달 (조건: 0 이상)", touch_price);
  }

  return nullopt;
}

optional<string> BaseOrderHandler::IsValidTrailPoint(double trail_point) {
  if (IsLessOrEqual(trail_point, 0.0)) [[unlikely]] {
    return format("트레일링 포인트 [{}] 미달 (조건: 0 초과)", trail_point);
  }

  return nullopt;
}

bool BaseOrderHandler::IsLimitPriceSatisfied(const Direction order_direction,
                                             const double price,
                                             const double order_price) {
  return (order_direction == LONG && IsLessOrEqual(price, order_price)) ||
         (order_direction == SHORT && IsGreaterOrEqual(price, order_price));
}

bool BaseOrderHandler::IsPriceTouched(const Direction touch_direction,
                                      const double price,
                                      const double touch_price) {
  return (touch_direction == LONG && IsGreaterOrEqual(price, touch_price)) ||
         (touch_direction == SHORT && IsLessOrEqual(price, touch_price));
}

optional<string> BaseOrderHandler::HasEnoughBalance(
    const double balance, const double needed_balance,
    const string& balance_type_msg, const string& purpose_msg) {
  if (IsLess(balance, needed_balance)) {
    return format("{} 자금 [{}] 부족 (필요 자금: {} [{}])", balance_type_msg,
                  FormatDollar(balance, true), purpose_msg,
                  FormatDollar(needed_balance, true));
  }

  return nullopt;
}

void BaseOrderHandler::UpdateLastEntryBarIndex(const int symbol_idx) {
  const auto original_bar_type = bar_->GetCurrentBarType();

  bar_->SetCurrentBarType(TRADING, "");

  last_entry_bar_indices_[symbol_idx] = bar_->GetCurrentBarIndex();

  // 진입 시에는 트레이딩, 돋보기 바만 사용하며, 진입 바 인덱스는 진입 시에만
  // 업데이트 되므로 타임프레임은 필요하지 않음
  bar_->SetCurrentBarType(original_bar_type, "");
}

void BaseOrderHandler::UpdateLastExitBarIndex(const int symbol_idx) {
  const auto original_bar_type = bar_->GetCurrentBarType();

  bar_->SetCurrentBarType(TRADING, "");

  last_exit_bar_indices_[symbol_idx] = bar_->GetCurrentBarIndex();

  // 진입 시에는 트레이딩, 돋보기 바만 사용하며, 진입 바 인덱스는 진입 시에만
  // 업데이트 되므로 타임프레임은 필요하지 않음
  bar_->SetCurrentBarType(original_bar_type, "");
}

void BaseOrderHandler::Initialize(const int num_symbols) {
  if (is_initialized_) [[unlikely]] {
    Logger::LogAndThrowError(
        "주문 핸들러가 이미 초기화가 완료되어 다시 초기화할 수 없습니다.",
        __FILE__, __LINE__);
  }

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
  ranges::fill(last_entry_prices_, NAN);
  ranges::fill(last_exit_prices_, NAN);

  // 레버리지 벡터를 심볼 개수로 초기화
  // 초기 레버리지는 1x
  leverages_.resize(num_symbols, 1);

  is_initialized_ = true;
}

void BaseOrderHandler::SetSymbolInfo(const vector<SymbolInfo>& symbol_info) {
  if (symbol_info_.empty()) {
    symbol_info_ = symbol_info;
  } else [[unlikely]] {
    Logger::LogAndThrowError(
        "심볼 정보가 이미 초기화되어 다시 초기화할 수 없습니다.", __FILE__,
        __LINE__);
  }
}

void BaseOrderHandler::UpdateCurrentPositionSize(const int symbol_idx) {
  double sum_position_size = 0;

  for (const auto& filled_entry : filled_entries_[symbol_idx]) {
    double position_size =
        filled_entry->GetEntryFilledSize() - filled_entry->GetExitFilledSize();

    position_size = filled_entry->GetEntryDirection() == LONG
                        ? fabs(position_size)
                        : -fabs(position_size);
    sum_position_size += position_size;
  }

  current_position_size_ = sum_position_size;
}

void BaseOrderHandler::InitializeJustEntered() { just_entered_ = false; }
void BaseOrderHandler::InitializeJustExited() { just_exited_ = false; }

void BaseOrderHandler::DecreaseUsedMarginOnEntryCancel(
    const shared_ptr<Order>& cancel_order) {
  switch (cancel_order->GetEntryOrderType()) {
    case MARKET:  // 시장가는 예약 증거금이 없음
      [[fallthrough]];
    case MIT: /* MIT Touch 대기 중에는 예약 증거금을 사용하지 않으며,
                 Touch 이후에는 시장가로 체결하므로 대기 주문이 없음  */
      [[fallthrough]];
    case TRAILING: {
      /* Trailing Touch 대기 중에는 예약 증거금을 사용하지 않으며,
         Touch 이후에는 가격을 추적하다 시장가로 체결하므로 대기 주문이 없음 */
      return;
    }

    case LIMIT: {
      // 사용한 자금에서 예약 증거금 감소
      if (const auto entry_margin = cancel_order->GetEntryMargin();
          !IsEqual(entry_margin, 0.0)) {
        engine_->DecreaseUsedMargin(entry_margin);
      }

      return;
    }

    case LIT: {
      if (cancel_order->GetEntryOrderTime() != -1) {
        /* Entry Order Time이 설정되었다는 것은 Touch 했다는 의미이며,
           Touch 이후에는 지정가로 예약 증거금을 사용하므로 사용한 자금에서 예약
           증거금을 감소시켜야 함 */
        if (const auto entry_margin = cancel_order->GetEntryMargin();
            !IsEqual(entry_margin, 0.0)) {
          engine_->DecreaseUsedMargin(entry_margin);
        }
      }
    }

    [[unlikely]] case ORDER_NONE: {
      Logger::LogAndThrowError(
          "진입 대기 주문 취소를 위해 예약 마진 감소 중 오류 발생: 주문 타입이 "
          "NONE으로 지정됨.",
          __FILE__, __LINE__);
    }
  }
}

}  // namespace backtesting::order