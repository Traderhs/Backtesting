// 표준 라이브러리
#include <algorithm>
#include <cmath>

// 파일 헤더
#include "Indicators/VolumeBoost.hpp"

// 내부 헤더
#include "Engines/Logger.hpp"

VolumeBoost::VolumeBoost(const string& name, const string& timeframe,
                         const Plot& plot, const double period)
    : Indicator(name, timeframe, plot),
      symbol_idx_(-1),
      sizet_period_(0),
      double_period_(period),
      count_(0),
      sum_(0.0),
      can_calculate_(false),
      buffer_idx_(0) {
  if (!isfinite(period) || period <= 0 || floor(period) != period) {
    throw runtime_error(
        format("VolumeBoost 지표의 Period [{}]은(는) 유한한 양의 정수여야 "
               "합니다.",
               period));
  }

  sizet_period_ = static_cast<size_t>(period);
  buffer_ = vector<double>(sizet_period_, 0.0);
}

void VolumeBoost::Initialize() {
  reference_bar_ = bar_->GetBarData(REFERENCE, this->GetTimeframe());
  symbol_idx_ = bar_->GetCurrentSymbolIndex();

  count_ = 0;
  sum_ = 0.0;
  can_calculate_ = false;

  ranges::fill(buffer_, 0.0);
  buffer_idx_ = 0;
}

Numeric<double> VolumeBoost::Calculate() {
  const double volume =
      reference_bar_->GetBar(symbol_idx_, bar_->GetCurrentBarIndex()).volume;

  if (!isfinite(volume)) {
    return NAN;
  }

  const double old = buffer_[buffer_idx_];
  buffer_[buffer_idx_] = volume;
  buffer_idx_ = (buffer_idx_ + 1) % sizet_period_;

  sum_ += volume;

  if (!can_calculate_) {
    if (count_++ < sizet_period_ - 1) {
      return NAN;
    }

    can_calculate_ = true;
  } else {
    sum_ -= old;
  }

  const double average_volume = sum_ / double_period_;
  return average_volume == 0.0 ? NAN : volume / average_volume;
}
