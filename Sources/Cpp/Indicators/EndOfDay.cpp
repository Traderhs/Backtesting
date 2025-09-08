// 표준 라이브러리
#include <regex>
#include <sstream>

// 파일 헤더
#include "Indicators/EndOfDay.hpp"

// 내부 헤더
#include "Engines/Logger.hpp"

EndOfDay::EndOfDay(const string& name, const string& timeframe,
                   const Plot& plot, const string& market_close_time)
    : Indicator(name, timeframe, plot),
      market_close_time_(market_close_time),
      close_hour_(0),
      close_minute_(0),
      close_second_(0),
      timeframe_minutes_(0),
      symbol_idx_(0) {
  ValidateAndParseTime(market_close_time);
}

void EndOfDay::Initialize() {
  // 지표가 각 심볼별로 재초기화될 때 호출됨
  reference_bar_ = bar_->GetBarData(REFERENCE, this->GetTimeframe());
  symbol_idx_ = bar_->GetCurrentSymbolIndex();

  // Initialize 때 하는 이유는, timeframe으로 TRADING_TIMEFRAME 문자열을 사용할
  // 수 있기 때문에 엔진 초기화 후 지표 초기화 때 초기화 해야함
  try {
    timeframe_minutes_ =
        static_cast<int>(ParseTimeframe(GetTimeframe()) / kMinute);
  } catch (const exception& e) {
    Logger::LogAndThrowError(e.what(), __FILE__, __LINE__);
  }
}

Numeric<double> EndOfDay::Calculate() {
  // 장 마감 시간인지 확인
  return IsEndOfDay(
             reference_bar_->GetBar(symbol_idx_, bar_->GetCurrentBarIndex())
                 .close_time)
             ? 1.0
             : 0.0;
}

void EndOfDay::ValidateAndParseTime(const string& time_str) {
  // HH:MM:SS 형식 검증 (정규표현식)
  const regex time_pattern(
      R"(^([01]?[0-9]|2[0-3]):([0-5]?[0-9]):([0-5]?[0-9])$)");
  smatch matches;

  if (!regex_match(time_str, matches, time_pattern)) {
    Logger::LogAndThrowError(
        format(
            "EndOfDay 지표의 장 마감 시간 [{}]이(가) HH:MM:SS 형식이 아닙니다. "
            "(예시: \"15:30:00\")",
            time_str),
        __FILE__, __LINE__);
  }

  // 시간, 분, 초 파싱
  close_hour_ = stoi(matches[1].str());
  close_minute_ = stoi(matches[2].str());
  close_second_ = stoi(matches[3].str());

  // 범위 검증 (00:00:00 ~ 23:59:59)
  if (close_hour_ < 0 || close_hour_ > 23) {
    Logger::LogAndThrowError(
        format("EndOfDay 지표의 장 마감 시간의 시간 [{}]이(가) 0~23 범위를 "
               "벗어났습니다.",
               close_hour_),
        __FILE__, __LINE__);
  }

  if (close_minute_ < 0 || close_minute_ > 59) {
    Logger::LogAndThrowError(
        format("EndOfDay 지표의 장 마감 시간의 분 [{}]이(가) 0~59 범위를 "
               "벗어났습니다.",
               close_minute_),
        __FILE__, __LINE__);
  }

  if (close_second_ < 0 || close_second_ > 59) {
    Logger::LogAndThrowError(
        format("EndOfDay 지표의 장 마감 시간의 초 [{}]이(가) 0~59 범위를 "
               "벗어났습니다.",
               close_second_),
        __FILE__, __LINE__);
  }
}

bool EndOfDay::IsEndOfDay(const int64_t bar_close_time) const {
  // 타임스탬프를 날짜시간 문자열로 변환 (YYYY-MM-DD HH:MM:SS 형식)

  // 시간 부분만 추출 (HH:MM:SS 형태)
  if (const string& datetime_str = UtcTimestampToUtcDatetime(bar_close_time);
      datetime_str.length() >= 19) {  // "YYYY-MM-DD HH:MM:SS" 최소 길이
    const string time_part = datetime_str.substr(11, 8);  // "HH:MM:SS" 추출

    // 시간, 분, 초 파싱
    const int bar_hour = stoi(time_part.substr(0, 2));
    const int bar_minute = stoi(time_part.substr(3, 2));
    const int bar_second = stoi(time_part.substr(6, 2));

    // 초 단위로 변환하여 비교
    const int total_close_seconds =
        close_hour_ * 3600 + close_minute_ * 60 + close_second_;
    const int total_bar_close_seconds =
        bar_hour * 3600 + bar_minute * 60 + bar_second;

    // 바의 종료 시간이 지정된 시간 이상인지 확인
    return total_bar_close_seconds >= total_close_seconds;
  }

  return false;  // 파싱 실패 시 false 반환
}
