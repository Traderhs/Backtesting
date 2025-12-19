#pragma once

// 표준 라이브러리
#include <deque>

// 내부 헤더
#include "Engines/Indicator.hpp"

/// 주어진 기간 내 최저값
class Lowest final : public Indicator {
 public:
  explicit Lowest(const string& name, const string& timeframe, const Plot& plot,
                  Indicator& source, double period);

 private:
  Indicator& source_;
  size_t sizet_period_;
  double double_period_;

  size_t count_;
  bool can_calculate_;

  deque<pair<double, size_t>> dq_;
  size_t current_idx_;

  void Initialize() override;
  Numeric<double> Calculate() override;
};
