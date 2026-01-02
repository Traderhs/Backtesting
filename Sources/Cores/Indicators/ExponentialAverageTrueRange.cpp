// 표준 라이브러리
#include <algorithm>
#include <cmath>

// 파일 헤더
#include "Indicators/ExponentialAverageTrueRange.hpp"

// 내부 헤더
#include "Engines/Logger.hpp"

ExponentialAverageTrueRange::ExponentialAverageTrueRange(
    const string& name, const string& timeframe, const Plot& plot,
    const double period)
    : Indicator(name, timeframe, plot),
      symbol_idx_(-1),
      prev_close_(0.0),
      first_bar_(true),
      count_(0),
      sum_(0.0),
      can_calculate_(false),
      prev_atr_(0.0),
      alpha_(0.0) {
  if (period <= 0) {
    Logger::LogAndThrowError(
        format("ExponentialAverageTrueRange 지표의 Period [{}]은(는) 0보다 "
               "커야 합니다.",
               period),
        __FILE__, __LINE__);
  }

  // 타입 안정성과 속도를 위해 미리 변환
  double_period_ = period;
  sizet_period_ = static_cast<size_t>(period);
  // EMA alpha: 2 / (N + 1)
  alpha_ = 2.0 / (double_period_ + 1.0);
}

void ExponentialAverageTrueRange::Initialize() {
  reference_bar_ = bar_->GetBarData(REFERENCE, this->GetTimeframe());
  symbol_idx_ = bar_->GetCurrentSymbolIndex();

  prev_close_ = 0.0;
  first_bar_ = true;
  count_ = 0;
  sum_ = 0.0;
  can_calculate_ = false;
  prev_atr_ = 0.0;
}

Numeric<double> ExponentialAverageTrueRange::Calculate() {
  const auto& current_bar =
      reference_bar_->GetBar(symbol_idx_, bar_->GetCurrentBarIndex());

  const double high = current_bar.high;
  const double low = current_bar.low;
  const double close = current_bar.close;

  // 첫 번째 바: TR 계산 불가
  if (first_bar_) {
    first_bar_ = false;
    prev_close_ = close;
    return NAN;
  }

  // TR 계산: max(high - low, |high - prev_close|, |low - prev_close|)
  const double hl = high - low;
  const double hc = abs(high - prev_close_);
  const double lc = abs(low - prev_close_);
  const double tr = max({hl, hc, lc});

  prev_close_ = close;

  // period 개의 TR 값을 모아 평균을 계산하여 ATR의 초기값으로 사용
  if (!can_calculate_) {
    sum_ += tr;
    if (count_++ < sizet_period_ - 1) {
      return NAN;
    }

    // period 도달: 초기 ATR 계산
    can_calculate_ = true;
    prev_atr_ = sum_ / double_period_;
    return prev_atr_;
  }

  // Standard EMA smoothing
  // EMA = alpha * tr + (1 - alpha) * prev_atr_
  prev_atr_ = alpha_ * tr + (1.0 - alpha_) * prev_atr_;
  return prev_atr_;
}
