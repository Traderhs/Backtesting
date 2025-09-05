#pragma once

// 내부 헤더
#include "Engines/Indicator.hpp"

/// 단순 이동평균 (SMA)
class SimpleMovingAverage final : public Indicator {
 public:
  explicit SimpleMovingAverage(const string& name, const string& timeframe,
                               const Plot& plot, Indicator& source,
                               double period);

 private:
  double double_period_;
  size_t sizet_period_;

  Indicator& source_;
  int count_;
  long double sum_;
  bool can_calculate_;

  void Initialize() override;
  Numeric<long double> Calculate() override;
};