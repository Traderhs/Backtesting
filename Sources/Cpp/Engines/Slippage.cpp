// 표준 라이브러리
#include <format>

// 파일 헤더
#include "Engines/Slippage.hpp"

// 내부 헤더
#include "Engines/DataUtils.hpp"
#include "Engines/Numeric.hpp"
#include "Engines/Order.hpp"

namespace backtesting::order {

using namespace numeric;
using namespace order;
using namespace utils;

double PercentageSlippage::CalculateSlippagePrice(
    const OrderType order_type, const Direction direction,
    const double order_price, const double price_step) const {
  // 주문 타입에 따라 슬리피지율 선택
  const double slippage_percentage =
      order_type == MARKET || order_type == MIT || order_type == TRAILING
          ? taker_slippage_percentage_
          : maker_slippage_percentage_;

  // 슬리피지율이 0이면 원래 가격 반환
  if (IsEqual(slippage_percentage, 0.0)) {
    return order_price;
  }

  // 방향에 따라 슬리피지 적용
  const double slippage_ratio = slippage_percentage / 100.0;
  double slippage_price;

  if (direction == LONG) {
    // 매수 진입: 가격이 올라가므로 불리함
    slippage_price = order_price * (1.0 + slippage_ratio);
  } else {
    // 매도 진입: 가격이 내려가므로 불리함
    slippage_price = order_price * (1.0 - slippage_ratio);
  }

  // 가격 단위로 반올림
  return RoundToStep(slippage_price, price_step);
}

optional<string> PercentageSlippage::ValidateTakerSlippage() const {
  // 테이커 슬리피지율이 NaN이면 유효하지 않음
  if (isnan(taker_slippage_percentage_)) {
    return "지정된 테이커 슬리피지 퍼센트가 NaN으로 초기화됐습니다.";
  }

  // 테이커 슬리피지율이 0~100% 범위를 벗어나면 유효하지 않음
  if (IsGreater(taker_slippage_percentage_, 100.0) ||
      IsLess(taker_slippage_percentage_, 0.0)) {
    return format(
        "지정된 테이커 슬리피지 퍼센트 [{}%]는 100% 초과 혹은 "
        "0% 미만으로 설정할 수 없습니다.",
        taker_slippage_percentage_);
  }

  return nullopt;
}

optional<string> PercentageSlippage::ValidateMakerSlippage() const {
  // 메이커 슬리피지율이 NaN이면 유효하지 않음
  if (isnan(maker_slippage_percentage_)) {
    return "지정된 메이커 슬리피지 퍼센트가 NaN으로 초기화됐습니다.";
  }

  // 메이커 슬리피지율이 0~100% 범위를 벗어나면 유효하지 않음
  if (IsGreater(maker_slippage_percentage_, 100.0) ||
      IsLess(maker_slippage_percentage_, 0.0)) {
    return format(
        "지정된 메이커 슬리피지 퍼센트 [{}%]는 100% 초과 혹은 "
        "0% 미만으로 설정할 수 없습니다.",
        maker_slippage_percentage_);
  }

  return nullopt;
}

}  // namespace backtesting::order
