// 파일 헤더
#include "Indicators/Open.hpp"

// 내부 헤더
#include "Engines/BarData.hpp"
#include "Engines/BarHandler.hpp"
#include "Engines/BaseBarHandler.hpp"

Open::Open(const string& name, const string& timeframe)
    : Indicator(name, timeframe) {
  CalculateAll();
}

void Open::Initialize() {}

Numeric<double> Open::Calculate() {
  return bar_->GetBarData(BarType::REFERENCE, this->GetTimeframe())
      .GetBar(bar_->GetCurrentSymbolIndex(), bar_->GetCurrentBarIndex())
      .open;
}