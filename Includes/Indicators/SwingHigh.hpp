#pragma once

// 내부 헤더
#include "Engines/Indicator.hpp"

/// Swing Point High
class SwingHigh final : public Indicator {
 public:
  explicit SwingHigh(const string& name, const string& timeframe,
                     const Plot& plot, double period);

 private:
  shared_ptr<BarData> reference_bar_;
  int symbol_idx_;

  size_t period_;
  size_t count_;
  bool can_calculate_;
  double last_swing_high_;

  void Initialize() override;
  Numeric<double> Calculate() override;
};
