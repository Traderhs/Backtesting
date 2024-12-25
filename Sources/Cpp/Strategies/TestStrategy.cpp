// 내부 헤더
#include "Indicators/Indicators.hpp"

// 파일 헤더
#include "Strategies/TestStrategy.hpp"

TestStrategy::TestStrategy(const string& name)
    : Strategy(name), close("close", "1d"),
      sma1("sma1", "1d", close, 20),
      sma2("sma2", "1d", close, 5) {}

void TestStrategy::Initialize() {}

void TestStrategy::Execute(){
  if (order.position_size == 0 && close[0] > sma1[0]) {
    //entry()
  } else if (order.position_size == 0 && close[0] < sma1[0]) {
    //entry()
  }

  // position_size는 엔진에서 심볼 바뀔 때 자동으로 업데이트 하게 하자 protected로 하고
  if (order.position_size > 0 && close[0] < sma2[0]) {

  }

  if (order.position_size < 0 && close[0] > sma2[0]) {

  }
}

