#pragma once

// 내부 헤더
#include "Indicator.hpp"

/// 표준 편차
class StandardDeviation final : public Indicator {
 public:
  explicit StandardDeviation(const string& name, const string& timeframe,
                             const Plot& plot, Indicator& source,
                             double period);

 private:
  Indicator& source_;
  double double_period_;
  size_t sizet_period_;

  size_t count_;
  double sum_;
  double sum_sq_;
  bool can_calc_;

  vector<double> buffer_;
  size_t buffer_idx_;

  void Initialize() override;
  Numeric<double> Calculate() override;
};
