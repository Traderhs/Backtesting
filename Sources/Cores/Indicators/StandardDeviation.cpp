// 표준 라이브러리
#include <algorithm>
#include <cmath>

// 파일 헤더
#include "Indicators/StandardDeviation.hpp"

// 내부 헤더
#include "Engines/Logger.hpp"

StandardDeviation::StandardDeviation(const string& name,
                                     const string& timeframe, const Plot& plot,
                                     Indicator& source, const double period)
    : Indicator(name, timeframe, plot),
      source_(source),
      sizet_period_(static_cast<size_t>(period)),
      double_period_(period),
      count_(0),
      sum_(0.0),
      sum_sq_(0.0),
      can_calc_(false),
      buffer_(sizet_period_, 0.0),
      buffer_idx_(0) {
  if (period <= 0) {
    throw runtime_error(
        format("StandardDeviation 지표의 Period [{}]은(는) 0보다 커야 합니다.",
               period));
  }
}

void StandardDeviation::Initialize() {
  // 지표가 각 심볼별로 재초기화될 때 호출됨
  count_ = 0;
  sum_ = 0.0;
  sum_sq_ = 0.0;
  can_calc_ = false;

  ranges::fill(buffer_, 0.0);
  buffer_idx_ = 0;
}

Numeric<double> StandardDeviation::Calculate() {
  // 현재 값 읽기
  const double value = source_[0];

  // 입력값이 유효하지 않으면
  // 1. 값을 누적하지 않고 NaN 반환
  // 2. 이미 계산이 가능한 상태이면 이전 표준편차값 반환
  if (!isfinite(value)) {
    if (!can_calc_) {
      return NAN;
    }

    const double mean = sum_ / double_period_;

    double var = sum_sq_ / double_period_ - mean * mean;
    if (var < 0 && var > -1e-12) {
      var = 0;
    }

    return sqrt(var);
  }

  // 원형 버퍼에서 이전 값을 제거하고 새로운 값을 추가
  const double old = buffer_[buffer_idx_];
  buffer_[buffer_idx_] = value;
  buffer_idx_ = (buffer_idx_ + 1) % sizet_period_;

  // 합과 제곱합 업데이트
  sum_ += value;
  sum_sq_ += value * value;

  if (!can_calc_) {
    // 충분한 데이터가 모이지 않았으므로 NaN 리턴
    if (count_++ < sizet_period_ - 1) {
      return NAN;
    }

    can_calc_ = true;
    // 첫 윈도우 완성 시에는 old는 0으로 초기화되어 있으므로 이전값 제거 불필요
  } else {
    // 윈도우 이동: 이전값 제거
    sum_ -= old;
    sum_sq_ -= old * old;
  }

  // 평균 및 분산 계산
  const double mean = sum_ / double_period_;
  double var = sum_sq_ / double_period_ - mean * mean;

  // 경미한 음수 보정
  if (var < 0 && var > -1e-12) {
    var = 0;
  }

  return sqrt(var);
}
