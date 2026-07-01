#pragma once

// 표준 라이브러리
#include <vector>

// 내부 헤더
#include "Engines/Indicator.hpp"

/// 거래량 부스트 비율
class BACKTESTING_API VolumeBoost final : public Indicator {
 public:
  explicit VolumeBoost(const string& name, const string& timeframe,
                       const Plot& plot, double period);

 private:
  shared_ptr<BarData> reference_bar_;
  int symbol_idx_;

  size_t sizet_period_;
  double double_period_;

  size_t count_;
  double sum_;
  bool can_calculate_;

  vector<double> buffer_;
  size_t buffer_idx_;

  void Initialize() override;
  Numeric<double> Calculate() override;
};
