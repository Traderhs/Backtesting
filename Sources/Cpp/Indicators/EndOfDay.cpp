// 표준 라이브러리
#include <regex>

// 파일 헤더
#include "Indicators/EndOfDay.hpp"

// 내부 헤더
#include "Engines/Logger.hpp"
#include "Engines/TimeUtils.hpp"

EndOfDay::EndOfDay(const string& name, const string& timeframe,
                   const Plot& plot, const string& market_close_time)
    : Indicator(name, timeframe, plot),
      market_close_time_(market_close_time),
      close_seconds_of_day_(0),
      timeframe_minutes_(0),
      symbol_idx_(0) {
  ValidateAndParseTime(market_close_time);
}

void EndOfDay::Initialize() {
  // Initialize 때 파싱하는 이유는, timeframe으로 TRADING_TIMEFRAME 문자열을
  // 사용할 수 있기 때문에 엔진 초기화 후 지표 초기화 때 하는 것
  try {
    timeframe_minutes_ =
        static_cast<int>(ParseTimeframe(GetTimeframe()) / kMinute);
  } catch (const exception& e) {
    Logger::LogAndThrowError(e.what(), __FILE__, __LINE__);
  }

  reference_bar_ = bar_->GetBarData(REFERENCE, this->GetTimeframe());
  symbol_idx_ = bar_->GetCurrentSymbolIndex();
}

Numeric<double> EndOfDay::Calculate() {
  // 타임스탬프를 직접 계산하여 초 단위로 비교 (문자열 변환 없이)
  const int64_t close_time_ms =
      reference_bar_->GetBar(symbol_idx_, bar_->GetCurrentBarIndex())
          .close_time;

  // 밀리초를 초로 변환
  const int64_t close_time_sec = close_time_ms / 1000;

  // UTC 기준으로 하루 중 몇 초인지 계산 (86400초 = 24시간)
  const int bar_seconds_of_day = static_cast<int>(close_time_sec % 86400);

  // 바의 종료 시간이 지정된 시간 이상인지 확인
  return bar_seconds_of_day >= close_seconds_of_day_ ? 1.0 : 0.0;
}

void EndOfDay::ValidateAndParseTime(const string& time_str) {
  // HH:MM:SS 형식 검증
  const regex time_pattern(R"(^([01][0-9]|2[0-3]):([0-5][0-9]):([0-5][0-9])$)");
  smatch matches;

  if (!regex_match(time_str, matches, time_pattern)) {
    Logger::LogAndThrowError(
        format(
            "EndOfDay 지표의 장 마감 시간 [{}]이(가) HH:MM:SS 형식이 아닙니다.",
            time_str),
        __FILE__, __LINE__);
  }

  // 시간, 분, 초 파싱
  const int close_hour = stoi(matches[1].str());
  const int close_minute = stoi(matches[2].str());
  const int close_second = stoi(matches[3].str());

  // 범위 검증 (00:00:00 ~ 23:59:59)
  if (close_hour < 0 || close_hour > 23) {
    Logger::LogAndThrowError(
        format("EndOfDay 지표의 장 마감 시간의 시간 [{}]이(가) 0~23 범위를 "
               "벗어났습니다.",
               close_hour),
        __FILE__, __LINE__);
  }

  if (close_minute < 0 || close_minute > 59) {
    Logger::LogAndThrowError(
        format("EndOfDay 지표의 장 마감 시간의 분 [{}]이(가) 0~59 범위를 "
               "벗어났습니다.",
               close_minute),
        __FILE__, __LINE__);
  }

  if (close_second < 0 || close_second > 59) {
    Logger::LogAndThrowError(
        format("EndOfDay 지표의 장 마감 시간의 초 [{}]이(가) 0~59 범위를 "
               "벗어났습니다.",
               close_second),
        __FILE__, __LINE__);
  }

  // 하루 기준 초 단위로 사전 계산 (한번만 계산)
  close_seconds_of_day_ = close_hour * 3600 + close_minute * 60 + close_second;
}
