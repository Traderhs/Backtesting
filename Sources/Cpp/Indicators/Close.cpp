// 파일 헤더
#include "Indicators/Close.hpp"

// 내부 헤더
#include "Engines/BarData.hpp"
#include "Engines/BarHandler.hpp"
#include "Engines/BaseBarHandler.hpp"

Close::Close(const string& name, const string& timeframe, const Plot& plot)
    : Indicator(name, timeframe, plot) {}

void Close::Initialize() {
  reference_bar_ = bar_->GetBarData(REFERENCE, this->GetTimeframe());
}

Numeric<double> Close::Calculate() {
  return reference_bar_
      ->GetBar(bar_->GetCurrentSymbolIndex(), bar_->GetCurrentBarIndex())
      .close;
}