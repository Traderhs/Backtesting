#pragma once

// 내부 헤더
#include "Engines/Indicator.hpp"

class Low final : public Indicator {
 public:
  explicit Low(const string& name, const string& timeframe);

  double Calculate() override;
};