#pragma once

// 내부 헤더
#include "Engines/Indicator.hpp"

class Volume final : public Indicator {
public:
  explicit Volume(const string& name, const string& timeframe);

  double Calculate() override;
};