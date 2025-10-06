#pragma once

// 표준 라이브러리
#include <string>

// 내부 헤더
#include "Engines/Indicator.hpp"

/// 해당 봉이 당일 마지막 봉인지 판단하는 지표
class EndOfDay final : public Indicator {
 public:
  /// @param name 지표의 이름
  /// @param timeframe 지표의 타임프레임
  /// @param plot 플롯 정보
  /// @param market_close_time 장 마감 시간 (HH:MM:SS 형식, 예: "15:30:00")
  explicit EndOfDay(const string& name, const string& timeframe,
                    const Plot& plot, const string& market_close_time);

 private:
  string market_close_time_;         // 장 마감 시간 (HH:MM:SS 형식)
  int close_seconds_of_day_;         // 장 마감 시간을 하루 기준 초 단위로 변환
  int timeframe_minutes_;            // 타임프레임을 분 단위로 변환

  shared_ptr<BarData> reference_bar_;  // 참조 바 데이터
  int symbol_idx_;                     // 현재 심볼 인덱스

  void Initialize() override;
  Numeric<double> Calculate() override;

  /// 시간 문자열 형식 검증 및 파싱
  void ValidateAndParseTime(const string& time_str);
};
