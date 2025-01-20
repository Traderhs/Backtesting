#pragma once

#include "Engines/Strategy.hpp"

class TestStrategy final : public Strategy {
 public:
  explicit TestStrategy(const string& name);
  ~TestStrategy() override;

  void Initialize() override;

  void Execute() override;

 private:
  // ReSharper disable once CppInconsistentNaming
  SimpleMovingAverage sma1;
  // ReSharper disable once CppInconsistentNaming
  SimpleMovingAverage sma2;
};