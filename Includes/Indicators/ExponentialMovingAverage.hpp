#pragma once

// 내부 헤더
#include "Engines/Indicator.hpp"

/// 지수 이동평균 (EMA)
class ExponentialMovingAverage final : public Indicator {
 public:
  explicit ExponentialMovingAverage(const string& name, const string& timeframe,
                                    const Plot& plot, Indicator& source,
                                    double period);

 private:
  Indicator& source_;
  double double_period_;  // period를 double로 저장 (계산 편의)
  size_t sizet_period_;   // period를 size_t로 저장 (인덱스 접근)

  // SMA 시드용 상태
  size_t count_;        // 누적된 샘플 수 카운터
  double sum_;          // 최근 period 합 (초기 윈도우 구축에 사용)
  bool can_calculate_;  // period 충족 여부

  // EMA 계산 변수
  double prev_;   // 직전 EMA 값
  double alpha_;  // EMA 가중치

  void Initialize() override;
  Numeric<double> Calculate() override;
};
