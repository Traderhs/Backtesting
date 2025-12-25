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

namespace backtesting::utils {

// Thread-local 시간 캐시 (1초 단위로 캐싱)
thread_local char time_cache[32];
thread_local int64_t last_cached_second = 0;

int64_t GetCurrentLocalTimestamp() { return time(nullptr) * 1000; }

int64_t GetCurrentUtcTimestamp() {
  const auto now = system_clock::now();
  return std::chrono::duration_cast<milliseconds>(now.time_since_epoch())
      .count();
}

// 최적화된 현재 시간 포맷팅 함수 (로그용)
size_t FormatCurrentTimeFast(char* buffer) {
  const auto now = system_clock::now();
  const time_t now_time_t = system_clock::to_time_t(now);

  // 1초 단위로 캐싱
  if (now_time_t == last_cached_second) {
    // 캐시된 시간 복사
    char* p = buffer;
    const char* cached = time_cache;
    while (*cached) *p++ = *cached++;
    return p - buffer;
  }

  // 새로운 시간 포맷팅
  tm local_time{};
  localtime_s(&local_time, &now_time_t);

  char* p = buffer;

  // "YYYY-MM-DD HH:MM:SS" 형식으로 수동 포맷팅
  // 연도
  const int year = local_time.tm_year + 1900;
  *p++ = static_cast<char>('0' + year / 1000);
  *p++ = static_cast<char>('0' + (year / 100) % 10);
  *p++ = static_cast<char>('0' + (year / 10) % 10);
  *p++ = static_cast<char>('0' + year % 10);

  *p++ = '-';

  // 월
  const int month = local_time.tm_mon + 1;
  *p++ = static_cast<char>('0' + month / 10);
  *p++ = static_cast<char>('0' + month % 10);

  *p++ = '-';

  // 일
  const int day = local_time.tm_mday;
  *p++ = static_cast<char>('0' + day / 10);
  *p++ = static_cast<char>('0' + day % 10);

  *p++ = ' ';

  // 시
  const int hour = local_time.tm_hour;
  *p++ = static_cast<char>('0' + hour / 10);
  *p++ = static_cast<char>('0' + hour % 10);

  *p++ = ':';

  // 분
  const int min = local_time.tm_min;
  *p++ = static_cast<char>('0' + min / 10);
  *p++ = static_cast<char>('0' + min % 10);

  *p++ = ':';

  // 초
  const int sec = local_time.tm_sec;
  *p++ = static_cast<char>('0' + sec / 10);
  *p++ = static_cast<char>('0' + sec % 10);

  *p = '\0';

  // 캐시 업데이트
  char* cache_p = time_cache;
  const char* temp_p = buffer;
  while (*temp_p) *cache_p++ = *temp_p++;
  *cache_p = '\0';
  last_cached_second = now_time_t;

  return p - buffer;
}

string GetCurrentLocalDatetime() {
  char buffer[32];
  const size_t len = FormatCurrentTimeFast(buffer);
  return {buffer, len};
}

string UtcTimestampToUtcDatetime(const int64_t timestamp_ms) {
  if (timestamp_ms < 0) {
    return "";
  }

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
  if (ss.fail()) {
    Logger::LogAndThrowError("Datetime 문자열을 파싱하는 데 실패했습니다.",
                             __FILE__, __LINE__);
  }

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
    Logger::LogAndThrowError(
        format("잘못된 타임프레임 포맷 [{}]이(가) 지정되었습니다.",
               timeframe_str),
        __FILE__, __LINE__);
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

    Logger::LogAndThrowError(
        format("잘못된 타임프레임 유닛 [{}]이(가) 지정되었습니다.", unit),
        __FILE__, __LINE__);
  }

  return -1;
}

string FormatTimeDiff(const int64_t diff_ms) {
  // 보유 시간을 계산하고, 같은 봉에서의 거래일 경우
  if (diff_ms == 0) {
    return "동일봉 거래";
  }

  // 1초 미만
  if (diff_ms < kSecond) {
    return to_string(static_cast<double>(diff_ms) / 1000.0) + "초";
  }

  vector<pair<int64_t, string>> units = {
      {kYear, "년"},   {kMonth, "개월"}, {kWeek, "주"},  {kDay, "일"},
      {kHour, "시간"}, {kMinute, "분"},  {kSecond, "초"}};

  vector<string> result_units;
  int64_t remainder = diff_ms;

  // 첫 번째 단위 찾기
  for (const auto& [unit_value, unit_name] : units) {
    if (remainder >= unit_value) {
      const int64_t count = remainder / unit_value;
      result_units.push_back(to_string(count) + unit_name);
      remainder %= unit_value;
      break;
    }
  }

  // 2번째 단위 찾기 (0이 아닌 경우만, 최대한 높은 단위)
  for (const auto& [unit_value, unit_name] : units) {
    if (remainder >= unit_value) {
      if (const int64_t count = remainder / unit_value; count > 0) {
        result_units.push_back(to_string(count) + unit_name);
        break;
      }
    }
  }

  // 결과 조합
  if (result_units.empty()) {
    return to_string(diff_ms / kSecond) + "초";
  }

  string result = result_units[0];
  for (size_t i = 1; i < result_units.size(); ++i) {
    result += " " + result_units[i];
  }

  return result;
}

bool IsTimestampMs(const int64_t timestamp) {
  // 현재 시간을 초, 밀리초 단위로 가져옴
  const int64_t current_s = system_clock::now().time_since_epoch() / seconds(1);
  const int64_t current_ms =
      system_clock::now().time_since_epoch() / milliseconds(1);

  // 밀리초 단위 값이 현재 밀리초 값과 더 가까우면 ms 단위
  return abs(timestamp - current_ms) < abs(timestamp - current_s);
}

int64_t CalculateNextMonthBoundary(const int64_t timestamp_ms) {
  const auto time_point = system_clock::from_time_t(timestamp_ms / 1000);
  const auto time_t_val = system_clock::to_time_t(time_point);

  tm tm = {};
  gmtime_s(&tm, &time_t_val);

  // 다음 달 1일 00:00:00으로 설정
  tm.tm_mday = 1;
  tm.tm_hour = 0;
  tm.tm_min = 0;
  tm.tm_sec = 0;

  // 월 증가 처리
  tm.tm_mon++;
  if (tm.tm_mon == 12) {
    tm.tm_mon = 0;
    tm.tm_year++;
  }

  // UTC 기준 time_t로 변환
  return _mkgmtime(&tm) * 1000;
}

}  // namespace backtesting::utils
