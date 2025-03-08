// 파일 헤더
#include "Strategies/TestStrategy.hpp"

// 내부 헤더
#include "Engines/Order.hpp"

// 네임 스페이스
enum class Direction;

TestStrategy::TestStrategy(const string& name)
    : Strategy(name),
      sma1(AddIndicator<SimpleMovingAverage>("sma1", trading_timeframe, close,
                                             20)),
      sma2(AddIndicator<SimpleMovingAverage>("sma2", trading_timeframe, close,
                                             5)) {}
TestStrategy::~TestStrategy() = default;

void TestStrategy::Initialize() {}

void TestStrategy::ExecuteOnClose() {
  if (order->current_position_size == 0) {
    if (close[0] > sma1[0] && close[1] < sma1[1]) {
      order->SetLeverage(10);
      order->LimitEntry("매수 진입", Direction::LONG, 0.1, sma1[0]);
    }
  }
}

void TestStrategy::ExecuteAfterEntry() {
  order->SetLeverage(100);
  order->MarketEntry("매수 진입 2", Direction::LONG, 10);
  order->LimitExit("매수 청산 2", "매수 진입 2", entry_size,
                   order->LastEntryPrice() * 1.01);
}

void TestStrategy::ExecuteAfterExit() {
  order->MarketEntry("매수 진입 3", Direction::LONG, 10);
  order->LimitExit("매수 청산 3", "매수 진입 3", entry_size,
                   order->LastEntryPrice() * 1.01);
}
