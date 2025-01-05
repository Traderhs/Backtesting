#pragma once

// 표준 라이브러리
#include <chrono>
#include <cstdint>
#include <string>

// 네임 스페이스
using namespace std;
using namespace chrono;

/**
 * 시간 핸들링을 위한 유틸리티 네임스페이스
 */
namespace time_utils {

constexpr int64_t kSecond = 1000;
constexpr int64_t kMinute = 60 * kSecond;
constexpr int64_t kHour = 60 * kMinute;
constexpr int64_t kDay = 24 * kHour;
constexpr int64_t kWeek = 7 * kDay;
constexpr int64_t kMonth = 30 * kDay;

/**
 * 현재 시스템의 로컬 시간대를 기준으로 현재 날짜와 시간을 반환하는 함수
 *
 * @return 현재 로컬 날짜와 시간의 문자열
 */
string GetCurrentLocalDatetime();

/**
 * 주어진 타임스탬프(밀리초 기준)를 유닉스 에포크 시간대부터 UTC 날짜-시간
 * 문자열로 변환하여 반환하는 함수
 *
 * @param timestamp_ms 변환할 밀리초 단위의 타임스탬프
 * @return UTC 날짜와 시간의 문자열 표현
 */
string UtcTimestampToUtcDatetime(int64_t timestamp_ms);

/**
 * 주어진 UTC 날짜 및 시간 문자열을 UTC 타임스탬프로 변환하여 반환하는 함수
 *
 * @param datetime 변환할 UTC 날짜 및 시간의 문자열
 * @param format datetime 문자열의 포맷을 지정하는 형식 문자열
 * @return 밀리초 단위의 UTC 타임스탬프
 */
int64_t UtcDatetimeToUtcTimestamp(const string& datetime, const string& format);

/**
 * 주어진 타임프레임(밀리초 기준)을 사람이 읽을 수 있는
 * 시간 단위로 변환하여 반환하는 함수
 *
 * @param timeframe_ms 밀리초 단위로 주어진 타임프레임
 * @return 사람이 읽을 수 있는 시간 단위로 변환된 문자열
 */
string FormatTimeframe(int64_t timeframe_ms);

/// 타임프레임 문자열을 타임스탬프로 변환하여 반환하는 함수
int64_t ParseTimeframe(const string& timeframe_str);
}
