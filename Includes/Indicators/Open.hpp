#pragma once

// 내부 헤더
#include "Engines/Indicator.hpp"

/// 전략 작성 편의성용 트레이딩 바의 시가 데이터 지표화
class Open final : public Indicator {
 public:
  explicit Open(const string& name, const string& timeframe, bool overlay,
                PlotStyle plot_style, const Color& color,
                unsigned char line_width);

 private:
  shared_ptr<BarData> reference_bar_;

  void Initialize() override;

  Numeric<double> Calculate() override;
};