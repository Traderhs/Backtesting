#pragma once

// 표준 라이브러리
#include <string>

// 내부 헤더
#include "Engines\BaseFetcher.hpp"
#include "Engines\Logger.hpp"

// 네임 스페이스
using namespace std;
using namespace arrow;

/// Binance 바 데이터의 Fetch와 Update를 담당하는 클래스
class BinanceFetcher final : public BaseFetcher {
 public:
  explicit BinanceFetcher();
  explicit BinanceFetcher(string market_data_path);

  /**
   * 지정된 심볼과 시간 프레임에 대해 Binance 현물 및 선물 klines 데이터를 Fetch
   * 후 병합하고 Parquet 형식으로 저장하는 함수
   *
   * @param symbol klines 데이터를 가져올 거래 쌍 심볼(예: "BTCUSDT")
   * @param timeframe klines 데이터의 타임프레임(예: "1m", "1h")
   */
  void FetchAndSaveBarData(const string& symbol, const string& timeframe) const;

  /**
   * 주어진 심볼과 시간 프레임에 대한 Spot-Futures klines 데이터를
   * 업데이트하는 함수
   *
   * @param symbol 업데이트 할 klines 데이터의 거래 쌍 심볼(예: "BTCUSDT")
   * @param timeframe klines 데이터의 타임프레임(예: "1m", "1h")
   */
  void UpdateBarData(const string& symbol, const string& timeframe) const;

 private:
  static shared_ptr<Logger>& logger_;  // 로그용 객체

  string market_data_path_;   // Market Data 폴더 경로
  string klines_path_;        // Klines 폴더 경로
  string funding_rate_path_;  // Funding Rate 폴더 경로

  string futures_klines_url_ =
      "https://fapi.binance.com/fapi/v1/continuousKlines";
  string spot_klines_url_ = "https://api.binance.com/api/v3/klines";

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
  static vector<shared_ptr<Array>> GetArraysAddedKlines(
      const vector<json>& klines);

  /**
   * 주어진 klines 데이터를 변환하여 더 쉽게 다룰 수 있는 형식으로 변환하는
   * 함수. 현물 데이터와 선물 데이터의 데이터 조정 시 double 형식의 가격
   * 데이터가 필요하므로 조정
   *
   * @param klines 변환할 klines 데이터를 포함하는 deque
   * @param drop_latest 변환 과정에서 최신 데이터를 포함하지 않을지를
   *                    결정하는 플래그. true일 경우 최신 데이터 행을 제거
   * @return 변환된 klines 데이터를 포함하는 deque
   */
  static vector<json> TransformKlines(const vector<json>& klines,
                                     bool drop_latest);

  /**
   * 주어진 현물 klines와 선물 klines 데이터를 병합하여 조정된 klines 데이터를 반환하는 함수.
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
};
