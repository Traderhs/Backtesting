#pragma once

// 표준 라이브러리
#include <string>

// 내부 헤더
#include "Engines/BaseFetcher.hpp"

// 전방 선언
namespace arrow {
class Array;
}

namespace backtesting::logger {
class Logger;
}

// 네임 스페이스
using namespace std;
using namespace backtesting::logger;

namespace backtesting::fetcher {

/// Binance 바 데이터의 Fetch와 Update를 담당하는 클래스
class BinanceFetcher final : public BaseFetcher {
 public:
  BinanceFetcher() = delete;
  explicit BinanceFetcher(string api_key_env_var, string api_secret_env_var);
  explicit BinanceFetcher(string api_key_env_var, string api_secret_env_var,
                          string market_data_path);

  /**
   * 지정된 심볼과 시간 프레임에 대해 현물 및 연속 선물 klines 데이터를
   * Fetch 후 병합하고 Parquet 형식으로 저장하는 함수
   *
   * @param symbol 연속 선물 캔들스틱 데이터를 가져올
   *               거래 쌍 심볼(예: "BTCUSDT")
   * @param timeframe 연속 선물 캔들스틱 데이터의 타임프레임(예: "1m", "1h")
   */
  void FetchContinuousKlines(const string& symbol,
                             const string& timeframe) const;

  /**
   * 주어진 심볼과 시간 프레임에 대한 연속 선물 캔들스틱 데이터를
   * 업데이트하는 함수
   *
   * @param symbol 업데이트 할 연속 선물 캔들스틱 데이터의
   *               거래 쌍 심볼(예: "BTCUSDT")
   * @param timeframe 캔들스틱 데이터의 타임프레임(예: "1m", "1h")
   */
  void UpdateContinuousKlines(const string& symbol,
                              const string& timeframe) const;

  /**
   * 지정된 심볼과 시간 프레임에 대해 마크 가격 캔들스틱 데이터를
   * Fetch 후 Parquet 형식으로 저장하는 함수.
   *
   * 마크 가격이 존재하지 않는 시절을 선물 또는 현물을 Fetch하여 보정하지 않는
   * 이유는, 마크 가격이 아예 없었던 시절은 시장 가격이 미실현 손익과 강제
   * 청산의 기준이었기 때문.
   *
   * @param symbol 마크 가격 캔들스틱 데이터를 가져올
   *               거래 쌍 심볼(예: "BTCUSDT")
   * @param timeframe 마크 가격 캔들스틱 데이터의 타임프레임(예: "1m", "1h")
   */
  void FetchMarkPriceKlines(const string& symbol,
                            const string& timeframe) const;

  /**
   * 주어진 심볼과 시간 프레임에 대한 마크 가격 캔들스틱 데이터를
   * 업데이트하는 함수
   *
   * @param symbol 업데이트 할 마크 가격 캔들스틱 데이터의
   *               거래 쌍 심볼(예: "BTCUSDT")
   * @param timeframe 마크 가격 캔들스틱 데이터의 타임프레임(예: "1m", "1h")
   */
  void UpdateMarkPriceKlines(const string& symbol,
                             const string& timeframe) const;

  /// 바이낸스 선물 거래소 정보를 Fetch하고 저장하는 함수
  void FetchExchangeInfo() const;

  /// 바이낸스 레버리지 구간을 Fecth하고 저장하는 함수
  void FetchLeverageBracket() const;

 private:
  static shared_ptr<Logger>& logger_;  // 로그용 객체

  static string header_;            // 바이낸스 API 헤더 문자열
  static string futures_endpoint_;  // 선물 엔드 포인트
  static string spot_endpoint_;     // 현물 엔드 포인트

  static string server_time_url_;        // 서버 시간 URL
  static string continuous_klines_url_;  // 연속 선물 Klines URL
  static string spot_klines_url_;        // 현물 Klines URL
  static string mark_price_klines_url_;  // 마크 Klines URL
  static string exchange_info_url_;      // 거래소 정보 URL
  static string leverage_bracket_url_;   // 레버리지 구간 URL

  string api_key_env_var_;     // API 키를 저장한 환경 변수 이름
  string api_secret_env_var_;  // API 시크릿을 저장한 환경 변수 이름

  string data_path_;               // Data 폴더 경로
  string continuous_klines_path_;  // 연속 선물 Klines 폴더 경로
  string mark_price_klines_path_;  // 마크 Klines 폴더 경로
  string funding_rates_path_;      // Funding Rate 폴더 경로

  /**
   * Binance API를 사용하여 지정된 URL과 파라미터에 대한
   * klines 데이터를 연속적으로 Fetch하는 함수
   *
   * @param url klines 데이터를 가져올 API의 URL
   * @param params 요청에 사용될 파라미터
   * @param forward 데이터를 가져오는 방향. true이면 데이터를 앞으로
   *                가져오고, false이면 데이터를 뒤로 가져옴
   * @return 가져온 klines 데이터를 나타내는 deque의 비동기 future 객체
   */
  static future<vector<json>> FetchKlines(
      const string& url, const unordered_map<string, string>& params,
      bool forward);

  /**
   * 주어진 기간 문자열을 파일 이름에 적합한 형식으로 변환
   *
   * @param timeframe 원래의 타임프레임 문자열
   * @return 파일 이름용으로 형식화된 타임프레임을 나타내는 문자열
   */
  static string GetFilenameWithTimeframe(const string& timeframe);

  /**
   * 주어진 JSON 형식의 kline 데이터를 빌더로 구성된 벡터에 추가한 뒤
   * Arrow Array로 변환하여 반환
   *
   * @param klines 추가할 klines 데이터를 포함하는 JSON 객체의 deque
   * @return 변환된 Arrow Array Vector
   */
  static vector<shared_ptr<arrow::Array>> GetArraysAddedKlines(
      const vector<json>& klines);

  /**
   * 주어진 klines 데이터를 변환하여 더 쉽게 다룰 수 있는 형식으로 변환하는
   * 함수.
   *
   * 현물 데이터와 선물 데이터의 데이터 조정 시 double 형식의 가격
   * 데이터가 필요하므로 가격 데이터는 double로 변환.
   *
   * @param klines 변환할 klines 데이터를 포함하는 deque
   * @param drop_latest 변환 과정에서 최신 데이터를 포함하지 않을지를
   *                    결정하는 플래그. true일 경우 최신 데이터 행을 제거
   * @return 변환된 klines 데이터를 포함하는 deque
   */
  static vector<json> TransformKlines(const vector<json>& klines,
                                      bool drop_latest);

  /**
   * 주어진 현물 klines와 선물 klines 데이터를 병합하여 조정된 klines 데이터를
   * 반환하는 함수.
   *
   * @param spot_klines 조정할 현물 klines 데이터의 deque
   * @param futures_klines 병합할 선물 klines 데이터의 deque
   * @return 조정된 현물 데이터와 선물 데이터가 병합된 새로운 klines deque
   */
  static vector<json> ConcatKlines(const vector<json>& spot_klines,
                                   const vector<json>& futures_klines);

  /**
   * 주어진 klines 데이터를 Parquet 파일로 변환하고 저장하는 함수
   *
   * @param klines 저장할 klines 데이터를 포함하는 JSON 객체의 deque
   * @param file_path Parquet 파일을 저장할 경로
   */
  static void SaveKlines(const vector<json>& klines, const string& file_path);

  /// 백슬래시를 모두 슬래시로 변환하여 반환하는 함수
  static string ConvertBackslashToSlash(const string& path_string);

  /// 바이낸스 선물 서버 시간을 Fetch하여 반환하는 함수
  static int64_t GetServerTime();
};

}  // namespace backtesting::fetcher