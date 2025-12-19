#pragma once

// 표준 라이브러리
#include <vector>

// 내부 헤더
#include "Engines/Indicator.hpp"

/// 단순 이동평균 (SMA)
class SimpleMovingAverage final : public Indicator {
 public:
  explicit SimpleMovingAverage(const string& name, const string& timeframe,
                               const Plot& plot, Indicator& source,
                               double period);

 private:
  Indicator& source_;
  double double_period_;
  size_t sizet_period_;

  int count_;
  double sum_;
  bool can_calculate_;

  vector<double> buffer_;
  size_t buffer_idx_;

  void Initialize() override;
  Numeric<double> Calculate() override;
};