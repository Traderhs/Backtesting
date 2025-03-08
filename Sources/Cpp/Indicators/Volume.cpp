// 파일 헤더
#include "Indicators/Volume.hpp"

// 내부 헤더
#include "Engines/BarData.hpp"
#include "Engines/BarHandler.hpp"
#include "Engines/BaseBarHandler.hpp"

Volume::Volume(const string& name, const string& timeframe)
    : Indicator(name, timeframe) {}

void Volume::Initialize() {
  reference_bar_ = bar_->GetBarData(BarType::REFERENCE, this->GetTimeframe());
}

Numeric<double> Volume::Calculate() {
  return reference_bar_
      ->GetBar(bar_->GetCurrentSymbolIndex(), bar_->GetCurrentBarIndex())
      .volume;
}