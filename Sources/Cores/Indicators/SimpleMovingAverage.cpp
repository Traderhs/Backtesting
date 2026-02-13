// 표준 라이브러리
#include <algorithm>

// 파일 헤더
#include "Indicators/SimpleMovingAverage.hpp"

// 내부 헤더
#include "Engines/Logger.hpp"

SimpleMovingAverage::SimpleMovingAverage(const string& name,
                                         const string& timeframe,
                                         const Plot& plot, Indicator& source,
                                         const double period)
    : Indicator(name, timeframe, plot),
      source_(source),
      sizet_period_(static_cast<size_t>(period)),
      double_period_(period),
      count_(0),
      sum_(0),
      can_calculate_(false),
      buffer_(sizet_period_, 0.0),
      buffer_idx_(0) {
  if (period <= 0) {
    throw runtime_error(format(
        "SimpleMovingAverage 지표의 Period [{}]은(는) 0보다 커야 합니다.",
        period));
  }
}

void SimpleMovingAverage::Initialize() {
  count_ = 0;
  sum_ = 0;
  can_calculate_ = false;

  ranges::fill(buffer_, 0.0);
  buffer_idx_ = 0;
}

Numeric<double> SimpleMovingAverage::Calculate() {
  // 현재 값 읽기
  const double value = source_[0];

  // 원형 버퍼에서 이전 값을 제거하고 새로운 값을 추가
  const double old = buffer_[buffer_idx_];
  buffer_[buffer_idx_] = value;
  buffer_idx_ = (buffer_idx_ + 1) % sizet_period_;

  // 합 업데이트
  sum_ += value;

  if (!can_calculate_) {
    // 충분한 데이터가 모이지 않았으므로 NaN 리턴
    if (count_++ < sizet_period_ - 1) {
      return NAN;
    }

    can_calculate_ = true;
    // 첫 윈도우 완성 시에는 old는 0으로 초기화되어 있으므로 이전값 제거 불필요
  } else {
    // 윈도우 이동: 이전값 제거
    sum_ -= old;
  }

  return sum_ / double_period_;
}
