// 파일 헤더
#include "Indicators\Low.hpp"

// 내부 헤더
#include <Engines/BarData.hpp>
#include <Engines/BarHandler.hpp>
#include <Engines/BaseBarHandler.hpp>

Low::Low(const string& name, const string& timeframe)
    : Indicator(name, timeframe) {
  CalculateAll();
}

double Low::Calculate() {
  return bar_->GetBarData(BarType::REFERENCE, GetTimeframe())
      .GetLow(bar_->GetCurrentSymbolIndex(), bar_->GetCurrentBarIndex());
}