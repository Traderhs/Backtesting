// 파일 헤더
#include "Indicators/Volume.hpp"

Volume::Volume(const string& name, const string& timeframe, const Plot& plot)
    : Indicator(name, timeframe, plot), symbol_idx_(-1) {}

void Volume::Initialize() {
  reference_bar_ = bar_->GetBarData(REFERENCE, this->GetTimeframe());
  symbol_idx_ = bar_->GetCurrentSymbolIndex();
}

Numeric<double> Volume::Calculate() {
  return reference_bar_->GetBar(symbol_idx_, bar_->GetCurrentBarIndex()).volume;
}
