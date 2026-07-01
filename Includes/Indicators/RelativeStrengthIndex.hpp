#pragma once

// 내부 헤더
#include "Engines/Indicator.hpp"

/// 상대 강도 지수 (RSI)
class BACKTESTING_API RelativeStrengthIndex final : public Indicator {
 public:
  explicit RelativeStrengthIndex(const string& name, const string& timeframe,
                                 const Plot& plot, Indicator& source,
                                 double period);

 private:
  Indicator& source_;
  size_t sizet_period_;
  double double_period_;

  double prev_source_;
  bool has_prev_source_;

  size_t count_;
  double sum_up_;
  double sum_down_;
  double avg_up_;
  double avg_down_;
  bool can_calculate_;

  double CalculateRsi() const;

  void Initialize() override;
  Numeric<double> Calculate() override;
};
