#pragma once

// 내부 헤더
#include "Engines/Indicator.hpp"

class Close final : public Indicator {
public:
  explicit Close(const string& name, const string& timeframe);

  double Calculate() override;
};