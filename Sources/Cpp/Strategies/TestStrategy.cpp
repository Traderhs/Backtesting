// 내부 헤더
#include "Engines/BarHandler.hpp"

// 파일 헤더
#include "Strategies/TestStrategy.hpp"

TestStrategy::TestStrategy(const string& name)
    : Strategy(name),
      sma1(AddIndicator<SimpleMovingAverage>(
          "sma1", "1h", Line(Rgba::orange, 2, SOLID, SIMPLE, false, 0, true),
          close, 20)),
      sma2(AddIndicator<SimpleMovingAverage>(
          "sma2", "1h", Line(Rgba::red, 2, SOLID, SIMPLE, false, 0, true),
          close, 5)) {}
TestStrategy::~TestStrategy() = default;

void TestStrategy::Initialize() {}

void TestStrategy::ExecuteOnClose() {
  double order_size;
  if (bar->GetCurrentSymbolIndex() == 0) {
    order_size = 0.05;
  } else {
    order_size = 500;
  }

  const auto position_size = order->GetCurrentPositionSize();

  if (position_size == 0) {
    if (close[0] > sma1[0] && close[1] < sma1[1]) {
      order->MarketEntry("이평선 매수", Direction::LONG, order_size, 10);
      return;
    }

    if (close[0] < sma1[0] && close[1] > sma1[1]) {
      order->MarketEntry("이평선 매도", Direction::SHORT, order_size, 10);
      return;
    }
  } else if (position_size > 0 && order->BarsSinceEntry() == 1) {
    order->MarketExit("이평선 매수 타임컷", "이평선 매수", left_size);
  } else if (position_size < 0 && order->BarsSinceEntry() == 1) {
    order->MarketExit("이평선 매도 타임컷", "이평선 매도", left_size);
  }
}

void TestStrategy::ExecuteAfterEntry() {
  if (order->GetCurrentPositionSize() > 0) {
    order->MitExit("이평선 매수 청산", "이평선 매수",
                   order->LastEntryPrice() * 1.01, left_size);
    order->MitExit("이평선 매수 손절", "이평선 매수",
                   order->LastEntryPrice() * 0.95, left_size);
  } else {
    order->MitExit("이평선 매도 청산", "이평선 매도",
                   order->LastEntryPrice() * 0.99, left_size);
    order->MitExit("이평선 매도 손절", "이평선 매도",
                   order->LastEntryPrice() * 1.05, left_size);
  }
}

void TestStrategy::ExecuteAfterExit() {}

void TestStrategy::ExecuteBeforeEntry() {}

void TestStrategy::ExecuteBeforeExit() {}