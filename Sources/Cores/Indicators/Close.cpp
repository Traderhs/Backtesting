// 파일 헤더
#include "Indicators/Close.hpp"

Close::Close(const string& name, const string& timeframe, const Plot& plot)
    : Indicator(name, timeframe, plot), symbol_idx_(-1) {}

void Close::Initialize() {
  reference_bar_ = bar_->GetBarData(REFERENCE, this->GetTimeframe());
  symbol_idx_ = bar_->GetCurrentSymbolIndex();
}

Numeric<double> Close::Calculate() {
  return reference_bar_->GetBar(symbol_idx_, bar_->GetCurrentBarIndex()).close;
}
