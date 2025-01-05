// 파일 헤더
#include "Indicators/Low.hpp"

Low::Low(const string& name, const string& timeframe)
    : Indicator(name, timeframe) {
  CalculateAll();
}

double Low::Calculate() {
  return bar_.GetBar(this->GetTimeframe(), 0).low;
}