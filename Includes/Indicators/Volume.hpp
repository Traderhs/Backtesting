#pragma once

// 내부 헤더
#include "Engines/Indicator.hpp"

/// 전략 작성 편의성용 거래량 데이터 지표화
class Volume final : public Indicator {
 public:
  explicit Volume(const string& name, const string& timeframe,
                  const Plot& plot);

 private:
  shared_ptr<BarData> reference_bar_;
  int symbol_idx_;

  void Initialize() override;
  Numeric<double> Calculate() override;
};