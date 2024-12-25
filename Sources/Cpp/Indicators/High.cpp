// 파일 헤더
#include "Indicators/High.hpp"

High::High(const string& name, const string& timeframe)
    : Indicator(name, timeframe) {
  CalculateAll();
}

double High::Calculate() {
  return bar.GetPrice(this->GetTimeframe(), 0,
                      BarDataManager::PriceType::HIGH);
}