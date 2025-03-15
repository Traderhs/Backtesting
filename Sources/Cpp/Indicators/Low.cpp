// 파일 헤더
#include "Indicators/Low.hpp"

// 내부 헤더
#include "Engines/BarData.hpp"
#include "Engines/BarHandler.hpp"
#include "Engines/BaseBarHandler.hpp"

Low::Low(const string& name, const string& timeframe, const bool overlay,
         const PlotStyle plot_style, const Color& color,
         const unsigned char line_width)
    : Indicator(name, timeframe, overlay, plot_style, color, line_width) {}

void Low::Initialize() {
  reference_bar_ = bar_->GetBarData(REFERENCE, this->GetTimeframe());
}

Numeric<double> Low::Calculate() {
  return reference_bar_
      ->GetBar(bar_->GetCurrentSymbolIndex(), bar_->GetCurrentBarIndex())
      .low;
}
