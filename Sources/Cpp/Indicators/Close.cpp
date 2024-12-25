// 파일 헤더
#include "Indicators/Close.hpp"

Close::Close(const string& name, const string& timeframe)
    : Indicator(name, timeframe) {
  CalculateAll();
}

double Close::Calculate() {
  return bar.GetPrice(this->GetTimeframe(), 0,
                      BarDataManager::PriceType::CLOSE);
}