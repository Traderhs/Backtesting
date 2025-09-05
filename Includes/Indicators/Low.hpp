#pragma once

// 내부 헤더
#include "Engines/Indicator.hpp"

/// 전략 작성 편의성용 트레이딩 바의 저가 데이터 지표화
class Low final : public Indicator {
 public:
  explicit Low(const string& name, const string& timeframe, const Plot& plot);

 private:
  shared_ptr<BarData> reference_bar_;
  int symbol_idx_;

  void Initialize() override;
  Numeric<long double> Calculate() override;
};