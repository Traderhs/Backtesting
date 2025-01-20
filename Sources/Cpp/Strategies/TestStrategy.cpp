// 파일 헤더
#include "Strategies/TestStrategy.hpp"

// 내부 헤더
#include "Engines/Order.hpp"
#include "Engines/OrderHandler.hpp"

TestStrategy::TestStrategy(const string& name)
    : Strategy(name),
      sma1("sma1", "1d", close, 20),
      sma2("sma2", "1d", close, 5) {}
TestStrategy::~TestStrategy() = default;

void TestStrategy::Initialize() {}

void TestStrategy::Execute() {
  if (order->current_position_size == 0 && close[0] > sma1[0]) {
    order->MarketEntry("매수 진입", Direction::LONG, 1, 1);
  } else if (order->current_position_size == 0 && close[0] < sma1[0]) {
    order->MarketEntry("매도 진입", Direction::SHORT, 1, 1);
  }

  if (order->current_position_size > 0 && close[0] < sma2[0]) {
    order->MarketExit("매수 청산", "매수 진입", 1);
  }

  if (order->current_position_size < 0 && close[0] > sma2[0]) {
    order->MarketExit("매도 청산", "매도 진입", 1);
  }
}

