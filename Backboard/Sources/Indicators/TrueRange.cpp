// 표준 라이브러리
#include <algorithm>
#include <cmath>

// 파일 헤더
#include "Indicators/TrueRange.hpp"

TrueRange::TrueRange(const string& name, const string& timeframe,
                     const Plot& plot)
    : Indicator(name, timeframe, plot),
      symbol_idx_(-1),
      prev_close_(0.0),
      first_bar_(true) {}

void TrueRange::Initialize() {
  reference_bar_ = bar_->GetBarData(REFERENCE, this->GetTimeframe());
  symbol_idx_ = bar_->GetCurrentSymbolIndex();
  prev_close_ = 0.0;
  first_bar_ = true;
}

Numeric<double> TrueRange::Calculate() {
  const auto& current_bar =
      reference_bar_->GetBar(symbol_idx_, bar_->GetCurrentBarIndex());

  const double high = current_bar.high;
  const double low = current_bar.low;
  const double close = current_bar.close;

  // 첫 번째 바: TR = high - low
  if (first_bar_) {
    first_bar_ = false;
    prev_close_ = close;
    return NAN;
  }

  // TR = max(high - low, |high - prev_close|, |low - prev_close|)
  const double hl = high - low;
  const double hc = abs(high - prev_close_);
  const double lc = abs(low - prev_close_);

  prev_close_ = close;

  return max({hl, hc, lc});
}
