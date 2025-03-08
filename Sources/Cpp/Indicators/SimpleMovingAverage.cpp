// 표준 라이브러리
#include <cmath>

// 파일 헤더
#include "Indicators/SimpleMovingAverage.hpp"

SimpleMovingAverage::SimpleMovingAverage(const string& name,
                                         const string& timeframe,
                                         Indicator& source, const double period)
    : Indicator(name, timeframe), source_(source) {
  // 타입 안정성과 속도를 위해 미리 변환
  double_period_ = period;
  size_period_ = static_cast<size_t>(period);
}

void SimpleMovingAverage::Initialize() {
  count_ = 0;
  sum_ = 0;
  can_calculate_ = false;
}

Numeric<double> SimpleMovingAverage::Calculate() {
  // 가장 최근 데이터를 추가
  sum_ += source_[0];

  if (!can_calculate_) {
    if (count_++ < size_period_ - 1) {
      // 아직 계산할 수 있는 데이터 부족
      return nan("");
    }

    // 처음으로 계산 가능해졌을 때 단순히 합을 지표 기간으로 나눠서 계산
    can_calculate_ = true;
    return sum_ / double_period_;
  }

  // 첫 계산 외에는 가장 오래된 데이터를 제거
  sum_ -= source_[size_period_];
  return sum_ / double_period_;
}