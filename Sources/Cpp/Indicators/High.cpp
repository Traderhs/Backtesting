// 파일 헤더
#include "Indicators/High.hpp"

// 내부 헤더
#include "Engines/BarData.hpp"
#include "Engines/BarHandler.hpp"
#include "Engines/BaseBarHandler.hpp"

High::High(const string& name, const string& timeframe)
    : Indicator(name, timeframe) {
  CalculateAll();
}

void High::Initialize() {}

double High::Calculate() {
  return bar_->GetBarData(BarType::REFERENCE, this->GetTimeframe())
      .GetBar(bar_->GetCurrentSymbolIndex(), bar_->GetCurrentBarIndex())
      .high;
}