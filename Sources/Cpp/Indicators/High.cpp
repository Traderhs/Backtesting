// 파일 헤더
#include "Indicators/High.hpp"

// 내부 헤더
#include "Engines/BarData.hpp"
#include "Engines/BarHandler.hpp"
#include "Engines/BaseBarHandler.hpp"

High::High(const string& name, const string& timeframe, const Plot& plot)
    : Indicator(name, timeframe, plot), symbol_idx_(-1) {}

void High::Initialize() {
  reference_bar_ = bar_->GetBarData(REFERENCE, this->GetTimeframe());
  symbol_idx_ = bar_->GetCurrentSymbolIndex();
}

Numeric<long double> High::Calculate() {
  return reference_bar_->GetBar(symbol_idx_, bar_->GetCurrentBarIndex()).high;
}