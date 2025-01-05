// 표준 라이브러리
#include <chrono>
#include <ctime>

// 내부 헤더
#include "Engines/Logger.hpp"
#include "Engines/TimeUtils.hpp"

// 파일 헤더
#include "BinanceDataManagerTest.hpp"

using namespace chrono;
using namespace time_utils;

void BinanceDataManagerTest::SetUp() {
  test = BinanceBarDataManager("../Tests/Klines");
  url = "https://fapi.binance.com/fapi/v1/continuousKlines";
  forward_params = {
      {"pair", "BTCUSDT"},
      {"contractType", "PERPETUAL"},
      {"startTime", "1731628800000"},  // 2024년 11월 15일 00:00:00 +00:00
      {"interval", "1M"},
      {"limit", "1"}};

  backward_params = {
      {"pair", "BTCUSDT"},
      {"contractType", "PERPETUAL"},
      {"endTime", "1596240000000"},  // 2020년 8월 1일 00:00:00 +00:00
      {"interval", "1M"},
      {"limit", "1"}};
}

void BinanceDataManagerTest::TearDown() {
  // Klines 파일이 존재하면 삭제
  for (const std::string klinesDirectory = "../../Tests/Klines";
       const auto& entry : directory_iterator(klinesDirectory)) {
    if (is_regular_file(entry.status())) {
      remove(entry.path());
    }
  }
}

TEST_F(BinanceDataManagerTest, FetchBinanceKlinesDataTest) {
  for (int i = 0; i < 13; i++) {
    // 앞으로 fetch했을 때 총 개월
    const auto fetched_months =
        FetchKlines(url, forward_params, true).get().size();

    // startTime을 tm 구조체로 변환
    const milliseconds ms(stoll(forward_params["startTime"]));
    const auto tp = system_clock::time_point(ms);
    const time_t start_time = system_clock::to_time_t(tp);
    tm tm_start_time{};
    gmtime_s(&tm_start_time, &start_time);

    // 현재 UTC 시간을 tm 구조체로 변환
    const time_t now = system_clock::to_time_t(system_clock::now());
    tm tm_now{};
    gmtime_s(&tm_now, &now);

    // startTime부터 현재 시간까지 월 차이를 계산
    int test_months = (tm_now.tm_year - tm_start_time.tm_year) * 12 +
                       (tm_now.tm_mon - tm_start_time.tm_mon);

    // 1일에 봉 하나가 추가되므로 month에 1을 추가함
    if (tm_start_time.tm_mday == 1) {
      test_months++;
    }

    logger.Log(
        Logger::DEBUG_L,
        format("[Start Time: {}] [Now Time: {}] | "
               "[Fetched Months: {}] [Test Months: {}]",
               UtcTimestampToUtcDatetime(stoll(forward_params["startTime"])),
               UtcTimestampToUtcDatetime(now * 1000), to_string(fetched_months),
               to_string(test_months)),
        __FILE__, __LINE__);

    // Fetch 해온 월의 수가 StartTime으로부터 현재 날짜까지의 월의 수와 같은지 테스트
    EXPECT_EQ(fetched_months, test_months);

    forward_params["startTime"] = to_string(stoll(forward_params["startTime"]) -
                                            2678400000);  // 1개월씩 감소
  }

  // ===========================================================================
  // 뒤로 fetch했을 때 총 개월
  const int backward_months = static_cast<int>(
      FetchKlines(url, backward_params, false).get().size());

  // 상장일로부터 2020년 8월 1일까지 12개월
  EXPECT_EQ(backward_months, 12);
}
