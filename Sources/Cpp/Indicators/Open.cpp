// 파일 헤더
#include "Indicators/Open.hpp"

// 내부 헤더
#include "Engines/BarData.hpp"
#include "Engines/BarHandler.hpp"
#include "Engines/BaseBarHandler.hpp"

Open::Open(const string& name, const string& timeframe, const bool overlay,
           const PlotStyle plot_style, const Color& color,
           const unsigned char line_width)
    : Indicator(name, timeframe, overlay, plot_style, color, line_width) {}

void Open::Initialize() {
  reference_bar_ = bar_->GetBarData(REFERENCE, this->GetTimeframe());
}

Numeric<double> Open::Calculate() {
  return reference_bar_
      ->GetBar(bar_->GetCurrentSymbolIndex(), bar_->GetCurrentBarIndex())
      .open;
}