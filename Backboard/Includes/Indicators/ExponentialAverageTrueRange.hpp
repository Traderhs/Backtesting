#pragma once

// 내부 헤더
#include "Indicator.hpp"

/// Exponential Average True Range
class ExponentialAverageTrueRange final : public Indicator {
 public:
  explicit ExponentialAverageTrueRange(const string& name,
                                       const string& timeframe,
                                       const Plot& plot, double period);

 private:
  shared_ptr<BarData> reference_bar_;
  int symbol_idx_;

  double double_period_;
  size_t sizet_period_;

  // TR 계산용
  double prev_close_;
  bool first_bar_;

  // 초기 ATR 계산용 (첫 period개의 TR 평균)
  size_t count_;
  double sum_;
  bool can_calculate_;

  // EMA 계산용
  double prev_atr_;
  double alpha_;

  void Initialize() override;
  Numeric<double> Calculate() override;
};
