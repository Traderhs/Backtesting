#pragma once

// 내부 헤더
#include "Engines\Indicator.hpp"

class SimpleMovingAverage final : public Indicator {
 public:
  explicit SimpleMovingAverage(const string& name, const string& timeframe,
                               Indicator& source, double period);

 private:
  double Calculate() override;

  double double_period_;
  size_t size_t_period_;

  Indicator& source_;
  double sum_;
  bool can_calculate_;
};