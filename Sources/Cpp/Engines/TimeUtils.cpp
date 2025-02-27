// 표준 라이브러리
#include <chrono>
#include <ctime>
#include <format>
#include <iomanip>
#include <sstream>
#include <vector>

// 파일 헤더
#include "Engines/TimeUtils.hpp"

// 네임 스페이스
using namespace chrono;

namespace time_utils {
string GetCurrentLocalDatetime() {
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

string UtcTimestampToUtcDatetime(const int64_t timestamp_ms) {
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

int64_t UtcDatetimeToUtcTimestamp(const string& datetime,
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

string FormatTimeframe(const int64_t timeframe_ms) {
  const vector months = {
      28 * kDay,  // 2월
      29 * kDay,  // 2월
      30 * kDay,  // 4,6,9,11월
      31 * kDay   // 1,3,5,7,8,10,12월
  };

  // 월 단위 체크
  for (const int64_t month : months) {
    if (timeframe_ms % month == 0) {
      return to_string(timeframe_ms / month) + "M";
    }
  }

  if (timeframe_ms % kWeek == 0) {
    return to_string(timeframe_ms / kWeek) + "w";
  }
  if (timeframe_ms % kDay == 0) {
    return to_string(timeframe_ms / kDay) + "d";
  }
  if (timeframe_ms % kHour == 0) {
    return to_string(timeframe_ms / kHour) + "h";
  }
  if (timeframe_ms % kMinute == 0) {
    return to_string(timeframe_ms / kMinute) + "m";
  }
  if (timeframe_ms % kSecond == 0) {
    return to_string(timeframe_ms / kSecond) + "s";
  }

  return to_string(timeframe_ms) + "ms";
}

int64_t ParseTimeframe(const string& timeframe_str) {
  // 타임프레임의 숫자 부분의 끝 좌표 찾기
  size_t pos = 0;
  while (pos < timeframe_str.size() && isdigit(timeframe_str[pos])) {
    ++pos;
  }

  if (pos == 0) {
    throw runtime_error(format(
        "잘못된 타임프레임 포맷 {}이(가) 지정되었습니다.", timeframe_str));
  }

  // str의 숫자 부분 찾기
  const int64_t value = stoll(timeframe_str.substr(0, pos));

  if (const string unit = timeframe_str.substr(pos); unit == "ms") {
    return value;
  } else {
    if (unit == "s") {
      return value * kSecond;
    }
    if (unit == "m") {
      return value * kMinute;
    }
    if (unit == "h") {
      return value * kHour;
    }
    if (unit == "d") {
      return value * kDay;
    }
    if (unit == "w") {
      return value * kWeek;
    }
    if (unit == "M") {
      return value * kMonth;
    }

    throw runtime_error(
        format("잘못된 타임프레임 유닛 {}이(가) 지정되었습니다.", unit));
  }
}

string FormatTimeDiff(const int64_t diff_ms) {
  // 같은 봉에서의 거래일 경우
  if (diff_ms == 0) {
    return "동일봉 거래";
  }

  // 1초 미만
  if (diff_ms < kSecond) {
    return to_string(diff_ms) + "밀리초";
  }

  if (diff_ms >= kYear)
    return to_string(diff_ms / kYear) + "년 " +
           to_string((diff_ms % kYear) / kMonth) + "개월";
  if (diff_ms >= kMonth)
    return to_string(diff_ms / kMonth) + "개월 " +
           to_string((diff_ms % kMonth) / kWeek) + "주";
  if (diff_ms >= kWeek)
    return to_string(diff_ms / kWeek) + "주 " +
           to_string((diff_ms % kWeek) / kDay) + "일";
  if (diff_ms >= kDay)
    return to_string(diff_ms / kDay) + "일 " +
           to_string((diff_ms % kDay) / kHour) + "시간";
  if (diff_ms >= kHour)
    return to_string(diff_ms / kHour) + "시간 " +
           to_string((diff_ms % kHour) / kMinute) + "분";
  if (diff_ms >= kMinute)
    return to_string(diff_ms / kMinute) + "분 " +
           to_string((diff_ms % kMinute) / kSecond) + "초";
  return to_string(diff_ms / kSecond) + "초";
}
}  // namespace time_utils