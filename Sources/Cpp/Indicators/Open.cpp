// 파일 헤더
#include "Indicators/Open.hpp"

Open::Open(const string& name, const string& timeframe)
    : Indicator(name, timeframe) {
  CalculateAll();
}

double Open::Calculate() {
  return bar.GetPrice(this->GetTimeframe(), 0,
                      BarDataManager::PriceType::OPEN);
}