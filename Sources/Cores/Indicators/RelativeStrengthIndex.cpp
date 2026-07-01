// 표준 라이브러리
#include <algorithm>
#include <cmath>

// 파일 헤더
#include "Indicators/RelativeStrengthIndex.hpp"

// 내부 헤더
#include "Engines/Logger.hpp"

RelativeStrengthIndex::RelativeStrengthIndex(const string& name,
                                             const string& timeframe,
                                             const Plot& plot,
                                             Indicator& source,
                                             const double period)
    : Indicator(name, timeframe, plot),
      source_(source),
      sizet_period_(0),
      double_period_(period),
      prev_source_(0.0),
      has_prev_source_(false),
      count_(0),
      sum_up_(0.0),
      sum_down_(0.0),
      avg_up_(0.0),
      avg_down_(0.0),
      can_calculate_(false) {
  if (!isfinite(period) || period <= 0 || floor(period) != period) {
    throw runtime_error(
        format("RelativeStrengthIndex 지표의 Period [{}]은(는) 유한한 양의 "
               "정수여야 합니다.",
               period));
  }

  sizet_period_ = static_cast<size_t>(period);
}

void RelativeStrengthIndex::Initialize() {
  prev_source_ = 0.0;
  has_prev_source_ = false;

  count_ = 0;
  sum_up_ = 0.0;
  sum_down_ = 0.0;
  avg_up_ = 0.0;
  avg_down_ = 0.0;
  can_calculate_ = false;
}

Numeric<double> RelativeStrengthIndex::Calculate() {
  const double value = source_[0];

  if (!isfinite(value)) {
    return can_calculate_ ? CalculateRsi() : NAN;
  }

  if (!has_prev_source_) {
    prev_source_ = value;
    has_prev_source_ = true;
    return NAN;
  }

  const double change = value - prev_source_;
  prev_source_ = value;

  const double up = max(change, 0.0);
  const double down = -min(change, 0.0);

  if (!can_calculate_) {
    sum_up_ += up;
    sum_down_ += down;

    if (count_++ < sizet_period_ - 1) {
      return NAN;
    }

    avg_up_ = sum_up_ / double_period_;
    avg_down_ = sum_down_ / double_period_;
    can_calculate_ = true;
    return CalculateRsi();
  }

  avg_up_ = (avg_up_ * (double_period_ - 1.0) + up) / double_period_;
  avg_down_ = (avg_down_ * (double_period_ - 1.0) + down) / double_period_;

  return CalculateRsi();
}

double RelativeStrengthIndex::CalculateRsi() const {
  if (avg_down_ == 0.0) {
    return 100.0;
  }

  if (avg_up_ == 0.0) {
    return 0.0;
  }

  return 100.0 - (100.0 / (1.0 + avg_up_ / avg_down_));
}
