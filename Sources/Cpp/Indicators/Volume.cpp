// 파일 헤더
#include "Indicators/Volume.hpp"

// 내부 헤더
#include "Engines/BarData.hpp"
#include "Engines/BarHandler.hpp"
#include "Engines/BaseBarHandler.hpp"

Volume::Volume(const string& name, const string& timeframe)
    : Indicator(name, timeframe) {
  CalculateAll();
}

void Volume::Initialize() {}

double Volume::Calculate() {
  return bar_->GetBarData(BarType::REFERENCE, GetTimeframe())
      .GetVolume(bar_->GetCurrentSymbolIndex(), bar_->GetCurrentBarIndex());
}