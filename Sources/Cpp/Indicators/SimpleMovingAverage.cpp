// 파일 헤더
#include "Indicators/SimpleMovingAverage.hpp"

SimpleMovingAverage::SimpleMovingAverage(const string& name,
                                         const string& timeframe,
                                         Indicator& source, const double period)
    : Indicator(name, timeframe), source(source) {
  SetInput({period});

  this->process = 0;
  this->sum = 0;

  CalculateAll();
}

double SimpleMovingAverage::Calculate() {
  const auto period = GetInput()[0];

  // 가장 최근 데이터를 추가
  sum += source[0];

  if (process < period - 1) {
    // 아직 계산할 수 있는 데이터 부족
    process++;
    return nan("");
  }

  // 가장 오래된 데이터를 제거
  sum -= source[static_cast<size_t>(period)];
  return sum / period;
}
