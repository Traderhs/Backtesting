#pragma once

// 내부 헤더
#include "Engines/Indicator.hpp"

class SimpleMovingAverage final : public Indicator {
 public:
  explicit SimpleMovingAverage(const string& name, const string& timeframe,
                               Indicator& source, double period);

  double Calculate() override;

 private:
  Indicator& source;
  int process;
  double sum;
};