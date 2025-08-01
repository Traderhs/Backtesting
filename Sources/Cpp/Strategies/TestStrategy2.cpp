// 파일 헤더
#include "Strategies/TestStrategy2.hpp"

#include "Engines/BarHandler.hpp"

TestStrategy2::TestStrategy2(const string& name)
    : Strategy(name),
daily_close_(AddIndicator<Close>("Daily Close","1d", Line(Rgba::white, 2, SOLID, SIMPLE, false, 0, true))),
      sma1(AddIndicator<SimpleMovingAverage>(
          "sma1", "1d",
          Line(Rgba::orange, 2, SOLID, SIMPLE, false, 0, true), daily_close_, 20)),
      sma2(AddIndicator<SimpleMovingAverage>(
          "sma2", "1d",
          Line(Rgba::red, 2, SOLID, SIMPLE, false, 0, true), daily_close_, 5)) {}
TestStrategy2::~TestStrategy2() = default;

void TestStrategy2::Initialize() {}

void TestStrategy2::ExecuteOnClose() {
  // @TODO 여러 번 사용하는 [] 참조는 캐시할 것

  logger->Log(WARNING_L, "daily close: " + to_string(daily_close_[0]), __FILE__, __LINE__, false);
  logger->Log(WARNING_L, "daily sma 5: " + to_string(sma2[0]), __FILE__, __LINE__, false);

  double order_size = 1;
  if (close[0] < 100) {
    order_size = static_cast<int>(100 / close[0]);
  }

  if (order->current_position_size == 0) {
    if (close[0] > sma1[0] && close[1] < sma1[1]) {
      order->MarketEntry("이평선 매수", Direction::LONG, order_size);
      return;
    }

    if (close[0] < sma1[0] && close[1] > sma1[1]) {
      order->MarketEntry("이평선 매도", Direction::SHORT, order_size);
      return;
    }
  }

  if (order->current_position_size > 0 && close[0] < sma2[0]) {
    order->MarketExit("이평선 매수 청산", "이평선 매수", left_size);
    return;
  }

  if (order->current_position_size < 0 && close[0] > sma2[0]) {
    order->MarketExit("이평선 매도 청산", "이평선 매도", left_size);
    return;
  }
}

void TestStrategy2::ExecuteAfterEntry() {}

void TestStrategy2::ExecuteAfterExit() {}
