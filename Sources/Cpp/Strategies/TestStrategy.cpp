// 파일 헤더
#include "Strategies/TestStrategy.hpp"

// 내부 헤더
#include "Engines/Order.hpp"
#include "Engines/OrderHandler.hpp"

TestStrategy::TestStrategy(const string& name)
    : Strategy(name),
      sma1("sma1", trading_timeframe, close, 20),
      sma2("sma2", trading_timeframe, close, 5) {}
TestStrategy::~TestStrategy() = default;

void TestStrategy::Initialize() {}

#include <iostream>
void TestStrategy::ExecuteOnClose() {
  if (order->current_position_size == 0) {
    if (close[0] > sma1[0] && close[1] < sma1[1]) {
      order->LimitEntry("매수 진입", LONG, 0.1, sma1[0], 1);
    }
  }
}

void TestStrategy::ExecuteAfterEntry() {
  order->MarketEntry("매수 진입 2", LONG, 0.1, 1);

  if (order->current_position_size > 0) {
    order->LimitExit("매수 청산", "매수 진입", entry_size, order->LastEntryPrice() * 1.01);
    order->LimitExit("매수 청산 2", "매수 진입 2", entry_size, order->LastEntryPrice() * 1.01);
  }
}

void TestStrategy::ExecuteAfterExit() {
  order->MarketEntry("매수 진입 3", LONG, 0.1, 1);
  order->LimitExit("매수 청산 3", "매수 진입 3", entry_size, order->LastEntryPrice() * 1.01);
}


