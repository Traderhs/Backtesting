// 파일 헤더
#include "Indicators/Volume.hpp"

Volume::Volume(const string& name, const string& timeframe)
    : Indicator(name, timeframe) {
  CalculateAll();
}

double Volume::Calculate() {
  return bar.GetPrice(this->GetTimeframe(), 0,
                      BarDataManager::PriceType::VOLUME);
}