// 표준 라이브러리
#include <cmath>

// 파일 헤더
#include "Indicators/ConstantValue.hpp"

// 내부 헤더
#include "Engines/Logger.hpp"

ConstantValue::ConstantValue(const string& name, const string& timeframe,
                             const Plot& plot, const double value)
    : Indicator(name, timeframe, plot), value_(value) {
  if (!isfinite(value)) {
    throw runtime_error(format(
        "ConstantValue 지표의 Value [{}]은(는) 유한해야 합니다.", value));
  }
}

void ConstantValue::Initialize() {}

Numeric<double> ConstantValue::Calculate() { return value_; }
