// 파일 헤더
#include "Indicators/Close.hpp"

// 내부 헤더
#include "Engines/BarData.hpp"
#include "Engines/BarHandler.hpp"
#include "Engines/BaseBarHandler.hpp"

Close::Close(const string& name, const string& timeframe)
    : Indicator(name, timeframe) {
  CalculateAll();
}

void Close::Initialize() {}

Numeric<double> Close::Calculate() {
  return bar_->GetBarData(BarType::REFERENCE, this->GetTimeframe())
      .GetBar(bar_->GetCurrentSymbolIndex(), bar_->GetCurrentBarIndex())
      .close;
}