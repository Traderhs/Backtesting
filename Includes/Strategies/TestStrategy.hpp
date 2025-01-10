#pragma once

#include "Engines\Strategy.hpp"

class TestStrategy final : public Strategy {
 public:
  explicit TestStrategy(const string& name);

  void Execute() override;

  void Initialize() override;

 private:
  SimpleMovingAverage sma1_;
  SimpleMovingAverage sma2_;
};