#pragma once

// 내부 헤더
#include "Engines/Indicator.hpp"

/// True Range
class BACKTESTING_API TrueRange final : public Indicator {
 public:
  explicit TrueRange(const string& name, const string& timeframe,
                     const Plot& plot);

 private:
  shared_ptr<BarData> reference_bar_;
  int symbol_idx_;
  double prev_close_;
  bool first_bar_;

  void Initialize() override;
  Numeric<double> Calculate() override;
};
