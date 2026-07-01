#pragma once

// 내부 헤더
#include "Engines/Indicator.hpp"

/// 고정 기준값을 위한 지표
class BACKTESTING_API ConstantValue final : public Indicator {
 public:
  explicit ConstantValue(const string& name, const string& timeframe,
                         const Plot& plot, double value);

 private:
  double value_;

  void Initialize() override;
  Numeric<double> Calculate() override;
};
