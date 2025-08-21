// 파일 헤더
#include "Strategies/TestStrategy.hpp"

TestStrategy::TestStrategy(const string& name)
    : Strategy(name, __FILE__),
      sma1(AddIndicator<SimpleMovingAverage>(
          "sma1", trading_timeframe,
          Line(Rgba::red, 2, SOLID, SIMPLE, false, 0, true), close, 5)),
      sma2(AddIndicator<SimpleMovingAverage>(
          "sma2", trading_timeframe,
          Line(Rgba::orange, 2, SOLID, SIMPLE, false, 0, true), close, 200)) {}
TestStrategy::~TestStrategy() = default;

void TestStrategy::Initialize() {}

void TestStrategy::ExecuteOnClose() {
  if (order->current_position_size == 0) {
    if (close[0] > sma1[0] && close[1] < sma1[1] && close[0] > sma2[0]) {
      order->AdjustLeverage(100);
      order->MarketEntry("매수 진입", LONG, 1);
    }
  }
}

void TestStrategy::ExecuteAfterEntry() {
  order->LimitExit("매수 청산 1", "매수 진입", order->LastEntryPrice() * 1.025,
                   0.33);
  order->LimitExit("매수 청산 2", "매수 진입", order->LastEntryPrice() * 1.05,
                   0.33);
  order->LimitExit("매수 청산 3", "매수 진입", order->LastEntryPrice() * 1.075,
                   0.34);
}

void TestStrategy::ExecuteAfterExit() {}
