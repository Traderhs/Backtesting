// 표준 라이브러리
#include <cmath>

// 파일 헤더
#include "Indicators/ExponentialMovingAverage.hpp"

// 내부 헤더
#include "Engines/Logger.hpp"

ExponentialMovingAverage::ExponentialMovingAverage(const string& name,
                                                   const string& timeframe,
                                                   const Plot& plot,
                                                   Indicator& source,
                                                   const double period)
    : Indicator(name, timeframe, plot),
      source_(source),
      sizet_period_(static_cast<size_t>(period)),
      double_period_(period),
      count_(0),
      sum_(0.0),
      can_calculate_(false),
      prev_(0.0),
      alpha_(2.0 / (period + 1.0)) {
  if (period <= 0) {
    throw runtime_error(
        format("ExponentialMovingAverage 지표의 Period "
               "[{}]은(는) 0보다 커야 합니다.",
               period));
  }
}

void ExponentialMovingAverage::Initialize() {
  // 지표가 각 심볼별로 재초기화될 때 호출됨
  count_ = 0;
  sum_ = 0.0;
  can_calculate_ = false;
  prev_ = 0.0;
}

Numeric<double> ExponentialMovingAverage::Calculate() {
  // 기준 지표의 현재 값을 읽음
  const double value = source_[0];

  // 입력값이 유효하지 않으면
  // 1. 값을 누적하지 않고 NaN 반환
  // 2. 이미 계산이 가능한 상태이면 이전 EMA 값을 그대로 반환
  if (!isfinite(value)) {
    if (!can_calculate_) {
      return NAN;
    }

    return prev_;
  }

  // period 개의 값을 모아 SMA를 계산하여 EMA의 초기값으로 사용
  if (!can_calculate_) {
    sum_ += value;
    if (count_++ < sizet_period_ - 1) {
      // 아직 period 미만: 유효값 없음
      return NAN;
    }

    // period 도달: 초기 SMA 계산
    can_calculate_ = true;
    prev_ = sum_ / double_period_;
    return prev_;
  }

  // 정상적인 EMA 업데이트: prev_은 직전 EMA 값
  prev_ = alpha_ * value + (1.0 - alpha_) * prev_;
  return prev_;
}
