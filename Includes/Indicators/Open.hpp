#pragma once

// 내부 헤더
#include "Engines/Indicator.hpp"

class Open final : public Indicator {
 public:
  explicit Open(const string& name, const string& timeframe);

  double Calculate() override;
};