#pragma once

// 내부 헤더
#include "Engines/Indicator.hpp"

class High final : public Indicator {
 public:
  explicit High(const string& name, const string& timeframe);

  double Calculate() override;
};