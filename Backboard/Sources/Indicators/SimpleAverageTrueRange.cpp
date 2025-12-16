// 표준 라이브러리
#include <algorithm>
#include <cmath>

// 파일 헤더
#include "Indicators/SimpleAverageTrueRange.hpp"

// 내부 헤더
#include "Logger.hpp"

SimpleAverageTrueRange::SimpleAverageTrueRange(const string& name,
                                               const string& timeframe,
                                               const Plot& plot,
                                               const double period)
    : Indicator(name, timeframe, plot),
      symbol_idx_(-1),
      prev_close_(0.0),
      first_bar_(true),
      count_(0),
      sum_(0.0),
      can_calculate_(false),
      buffer_idx_(0) {
  if (period <= 0) {
    Logger::LogAndThrowError(
        format("SimpleAverageTrueRange 지표의 Period [{}]은(는) 0보다 커야 "
               "합니다.",
               period),
        __FILE__, __LINE__);
  }

  // 타입 안정성과 속도를 위해 미리 변환
  double_period_ = period;
  sizet_period_ = static_cast<size_t>(period);

  buffer_.assign(sizet_period_, 0.0);
}

void SimpleAverageTrueRange::Initialize() {
  reference_bar_ = bar_->GetBarData(REFERENCE, this->GetTimeframe());
  symbol_idx_ = bar_->GetCurrentSymbolIndex();

  prev_close_ = 0.0;
  first_bar_ = true;
  count_ = 0;
  sum_ = 0.0;
  can_calculate_ = false;

  ranges::fill(buffer_, 0.0);
  buffer_idx_ = 0;
}

Numeric<double> SimpleAverageTrueRange::Calculate() {
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

  // SMA 계산
  const double old = buffer_[buffer_idx_];
  buffer_[buffer_idx_] = tr;
  buffer_idx_ = (buffer_idx_ + 1) % sizet_period_;

  sum_ += tr;

  if (!can_calculate_) {
    if (count_++ < sizet_period_ - 1) {
      return NAN;
    }

    can_calculate_ = true;
  } else {
    sum_ -= old;
  }

  return sum_ / double_period_;
}
