#pragma once

// 내부 헤더
#include "Engines/Strategy.hpp"

class TestStrategy2 final : public Strategy {
 public:
  explicit TestStrategy2(const string& name);
  ~TestStrategy2() override;

  void Initialize() override;
  void ExecuteOnClose() override;
  void ExecuteAfterEntry() override;
  void ExecuteAfterExit() override;

 private:
  Close& daily_close_;

  // ReSharper disable once CppInconsistentNaming
  ExponentialMovingAverage& sma1;
  // ReSharper disable once CppInconsistentNaming
  ExponentialMovingAverage& sma2;

  SwingHigh& highest_;
  SwingLow& lowest_;

  StandardDeviation& std_;
};