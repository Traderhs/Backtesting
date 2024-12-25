// 표준 라이브러리
#include <ctime>
#include <format>
#include <iomanip>
#include <sstream>

// 내부 헤더
#include "Engines/Engine.hpp"

// 파일 헤더
#include "Engines/TimeUtils.hpp"

string TimeUtils::GetCurrentLocalDatetime() {
  const auto& now = system_clock::now();
  const time_t now_time_t = system_clock::to_time_t(now);

  // Local 시간으로 변환
  tm local_time{};
  localtime_s(&local_time, &now_time_t);

  // Datetime으로 변환
  ostringstream ss;
  ss << put_time(&local_time, "%Y-%m-%d %H:%M:%S");
  return ss.str();
}

string TimeUtils::UTCTimestampToUtcDatetime(const int64_t timestamp_ms) {
  // timestamp ms를 time_t로 변환
  const auto timestamp_s = seconds(timestamp_ms / 1000);
  const system_clock::time_point tp(timestamp_s);
  const time_t timestamp_time_t = system_clock::to_time_t(tp);

  // UTC 시간으로 변환
  tm utc_time{};
  gmtime_s(&utc_time, &timestamp_time_t);

  // Datetime으로 변환
  ostringstream ss;
  ss << put_time(&utc_time, "%Y-%m-%d %H:%M:%S");
  return ss.str();
}

int64_t TimeUtils::UTCDatetimeToUTCTimestamp(const string& datetime,
                                             const string& format) {
  tm tm = {};
  istringstream ss(datetime);

  // 문자열을 tm으로 파싱
  ss >> get_time(&tm, format.c_str());
  if (ss.fail())
    Logger::LogAndThrowError("Datetime 문자열을 파싱하는 데 실패했습니다.",
                             __FILE__, __LINE__);

  // tm 구조체를 UTC 타임스탬프로 변환
  // _mkgmtime은 윈도우 전용
  tm.tm_isdst = -1;  // Daylight Saving Time 무시
  const int64_t utc_timestamp = _mkgmtime(&tm);

  return utc_timestamp * 1000;
}

string TimeUtils::FormatTimeframe(const int64_t timeframe_ms) {
  const vector MONTHS = {
      28 * DAY,  // 2월
      29 * DAY,  // 2월
      30 * DAY,  // 4,6,9,11월
      31 * DAY   // 1,3,5,7,8,10,12월
  };

  // 월 단위 체크
  for (const int64_t month : MONTHS) {
    if (timeframe_ms % month == 0) {
      return to_string(timeframe_ms / month) + "M";
    }
  }

  if (timeframe_ms % WEEK == 0) {
    return to_string(timeframe_ms / WEEK) + "w";
  }
  if (timeframe_ms % DAY == 0) {
    return to_string(timeframe_ms / DAY) + "d";
  }
  if (timeframe_ms % HOUR == 0) {
    return to_string(timeframe_ms / HOUR) + "h";
  }
  if (timeframe_ms % MINUTE == 0) {
    return to_string(timeframe_ms / MINUTE) + "m";
  }
  if (timeframe_ms % SECOND == 0) {
    return to_string(timeframe_ms / SECOND) + "s";
  }

  return to_string(timeframe_ms) + "ms";
}

int64_t TimeUtils::ParseTimeframe(const string& timeframe_str) {
  size_t pos = 0;
  while (pos < timeframe_str.size() && isdigit(timeframe_str[pos])) {
    ++pos;
  }

  if (pos == 0) {
    Logger::LogAndThrowError(
        "타임프레임 포맷이 잘못되었습니다.: " + timeframe_str, __FILE__,
        __LINE__);
  }

  // str의 숫자 부분 찾기
  const int64_t value = stoll(timeframe_str.substr(0, pos));

  if (const string unit = timeframe_str.substr(pos); unit == "ms") {
    return value;
  } else {
    if (unit == "s") {
      return value * SECOND;
    }
    if (unit == "m") {
      return value * MINUTE;
    }
    if (unit == "h") {
      return value * HOUR;
    }
    if (unit == "d") {
      return value * DAY;
    }
    if (unit == "w") {
      return value * WEEK;
    }
    if (unit == "M") {
      return value * MONTH;
    }

    Logger::LogAndThrowError("잘못된 유닛이 지정되었습니다.: " + unit,__FILE__,__LINE__);
    return -1;
  }
}