#pragma once

// 내부 헤더
#include "Engines/Indicator.hpp"

/// Simple Average True Range
class BACKTESTING_API SimpleAverageTrueRange final : public Indicator {
 public:
  explicit SimpleAverageTrueRange(const string& name, const string& timeframe,
                                  const Plot& plot, double period);

 private:
  shared_ptr<BarData> reference_bar_;
  int symbol_idx_;

  size_t sizet_period_;
  double double_period_;

  // TR 계산용
  double prev_close_;
  bool first_bar_;

  // SMA 계산용
  size_t count_;
  double sum_;
  bool can_calculate_;

  vector<double> buffer_;
  size_t buffer_idx_;

  void Initialize() override;
  Numeric<double> Calculate() override;
};
