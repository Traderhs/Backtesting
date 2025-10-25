#pragma once

// 표준 라이브러리
#include <filesystem>
#include <string>

// 내부 헤더
#include "Engines/BarHandler.hpp"
#include "Engines/BaseBarHandler.hpp"
#include "Engines/BinanceFetcher.hpp"
#include "Engines/Config.hpp"
#include "Engines/Engine.hpp"
#include "Engines/Logger.hpp"
#include "Engines/Slippage.hpp"
#include "Strategies/TestStrategy.hpp"

// 네임 스페이스
using namespace backtesting;
using namespace fetcher;

namespace backtesting {

class Backtesting {
 public:
  Backtesting() = delete;
  ~Backtesting() = delete;

  /// 백테스팅을 실행하는 함수
  static void Run() {
    try {
      Engine::GetEngine()->Backtesting();
    } catch (...) {
      Logger::LogAndThrowError("백테스팅 진행 중 오류가 발생했습니다.",
                               __FILE__, __LINE__);
    }
  }

  /// 엔진에 설정값을 추가하는 함수.
  ///
  /// 반환받은 객체를 통해 설정 함수를 호출하면 됨.
  static Config& SetConfig() { return Config::SetConfig(); }

  /**
   * API 환경변수 이름을 설정하는 함수
   *
   * @param api_key_env_var API 키를 저장한 환경변수 이름
   * @param api_secret_env_var API 시크릿을 저장한 환경변수 이름
   */
  static void SetApiEnvVars(const string& api_key_env_var,
                            const string& api_secret_env_var) {
    api_key_env_var_ = api_key_env_var;
    api_secret_env_var_ = api_secret_env_var;
  }

  /**
   * 시장 데이터 경로를 설정하는 함수
   *
   * @param market_data_directory 설정할 시장 데이터 폴더
   */
  static void SetMarketDataDirectory(const string& market_data_directory) {
    if (!filesystem::exists(market_data_directory)) {
      Logger::LogAndThrowError(
          format("지정된 시장 데이터 폴더 [{}]이(가) 존재하지 않습니다: ",
                 market_data_directory),
          __FILE__, __LINE__);
    }
    market_data_directory_ = market_data_directory;
  }

  /**
   * 지정된 심볼과 시간 프레임에 대해 연속 선물 klines 데이터를
   * Fetch 후 Parquet 형식으로 저장하는 함수
   *
   * @param symbol 연속 선물 캔들스틱 데이터를 가져올
   *               거래 쌍 심볼(예: "BTCUSDT")
   * @param timeframe 연속 선물 캔들스틱 데이터의 타임프레임(예: "1m", "1h")
   */
  static void FetchContinuousKlines(const string& symbol,
                                    const string& timeframe) {
    ValidateSettings();
    BinanceFetcher(api_key_env_var_, api_secret_env_var_,
                   market_data_directory_)
        .FetchContinuousKlines(symbol, timeframe);
  }

  /**
   * 주어진 심볼과 시간 프레임에 대한 연속 선물 캔들스틱 데이터를
   * 업데이트하는 함수
   *
   * @param symbol 업데이트 할 연속 선물 캔들스틱 데이터의
   *               거래 쌍 심볼(예: "BTCUSDT")
   * @param timeframe 캔들스틱 데이터의 타임프레임(예: "1m", "1h")
   */
  static void UpdateContinuousKlines(const string& symbol,
                                     const string& timeframe) {
    ValidateSettings();
    BinanceFetcher(api_key_env_var_, api_secret_env_var_,
                   market_data_directory_)
        .UpdateContinuousKlines(symbol, timeframe);
  }

  /**
   * 지정된 심볼과 시간 프레임에 대해 마크 가격 캔들스틱 데이터를
   * Fetch 후 Parquet 형식으로 저장하는 함수.
   *
   * @param symbol 마크 가격 캔들스틱 데이터를 가져올
   *               거래 쌍 심볼(예: "BTCUSDT")
   * @param timeframe 마크 가격 캔들스틱 데이터의 타임프레임(예: "1m", "1h")
   */
  static void FetchMarkPriceKlines(const string& symbol,
                                   const string& timeframe) {
    ValidateSettings();
    BinanceFetcher(api_key_env_var_, api_secret_env_var_,
                   market_data_directory_)
        .FetchMarkPriceKlines(symbol, timeframe);
  }

  /**
   * 주어진 심볼과 시간 프레임에 대한 마크 가격 캔들스틱 데이터를
   * 업데이트하는 함수
   *
   * @param symbol 업데이트 할 마크 가격 캔들스틱 데이터의
   *               거래 쌍 심볼(예: "BTCUSDT")
   * @param timeframe 마크 가격 캔들스틱 데이터의 타임프레임(예: "1m", "1h")
   */
  static void UpdateMarkPriceKlines(const string& symbol,
                                    const string& timeframe) {
    ValidateSettings();
    BinanceFetcher(api_key_env_var_, api_secret_env_var_,
                   market_data_directory_)
        .UpdateMarkPriceKlines(symbol, timeframe);
  }
  /**
   * 지정된 심볼에 대해 펀딩 비율 데이터를 Fetch 후 json 형식으로 저장하는 함수
   *
   * @param symbol 펀딩 비율 데이터를 가져올
   *               거래 쌍 심볼(예: "BTCUSDT")
   */
  static void FetchFundingRates(const string& symbol) {
    ValidateSettings();
    BinanceFetcher(api_key_env_var_, api_secret_env_var_,
                   market_data_directory_)
        .FetchFundingRates(symbol);
  }

  /**
   * 주어진 심볼에 대한 펀딩 비율 데이터를 업데이트하는 함수
   *
   * @param symbol 업데이트 할 펀딩 비율 데이터의
   *               거래 쌍 심볼(예: "BTCUSDT")
   */
  static void UpdateFundingRates(const string& symbol) {
    ValidateSettings();
    BinanceFetcher(api_key_env_var_, api_secret_env_var_,
                   market_data_directory_)
        .UpdateFundingRates(symbol);
  }

  /// 바이낸스 선물 거래소 정보를 Fetch하고 저장하는 함수
  static void FetchExchangeInfo() {
    ValidateSettings();
    BinanceFetcher(api_key_env_var_, api_secret_env_var_,
                   market_data_directory_)
        .FetchExchangeInfo();
  }

  /// 바이낸스 레버리지 구간을 Fecth하고 저장하는 함수
  static void FetchLeverageBracket() {
    ValidateSettings();
    BinanceFetcher(api_key_env_var_, api_secret_env_var_,
                   market_data_directory_)
        .FetchLeverageBracket();
  }

  /// 주어진 파일 경로에서 Parquet 데이터를 읽고
  /// 지정된 바 타입으로 처리하여 바 핸들러에 추가하는 함수
  ///
  /// @param symbol_name 바 데이터로 추가할 심볼 이름
  /// @param file_path Parquet 파일의 경로
  /// @param bar_type 추가할 데이터의 바 타입
  /// @param open_time_column Open Time 컬럼 인덱스
  /// @param open_column Open 컬럼 인덱스
  /// @param high_column High 컬럼 인덱스
  /// @param low_column Low 컬럼 인덱스
  /// @param close_column Close 컬럼 인덱스
  /// @param volume_column Volume 컬럼 인덱스
  /// @param close_time_column Close Time 컬럼 인덱스
  static void AddBarData(const string& symbol_name, const string& file_path,
                         const BarType bar_type, const int open_time_column = 0,
                         const int open_column = 1, const int high_column = 2,
                         const int low_column = 3, const int close_column = 4,
                         const int volume_column = 5,
                         const int close_time_column = 6) {
    BarHandler::GetBarHandler()->AddBarData(
        symbol_name, file_path, bar_type, open_time_column, open_column,
        high_column, low_column, close_column, volume_column,
        close_time_column);
  }

  /// 주어진 데이터 폴더에서 각 심볼들의 폴더를 찾아 Parquet 데이터를 읽고
  /// 지정된 바 타입으로 처리하여 바 핸들러에 추가하는 함수 (병렬 처리 최적화)
  ///
  /// ※ 바 유형별로 해당 경로를 만족해야 함 ※\n
  /// 트레이딩(돋보기, 참조): 디렉토리/심볼 이름/타임프레임/타임프레임.parquet\n
  /// 마크 가격: 디렉토리/심볼 이름/타임프레임.parquet
  ///
  /// @param symbol_names 바 데이터로 추가할 심볼 이름들
  /// @param timeframe 추가할 데이터의 타임프레임
  /// @param klines_directory Parquet 파일들이 위치한 데이터 폴더
  /// @param bar_type 추가할 데이터의 바 타입
  /// @param open_time_column Open Time 컬럼 인덱스
  /// @param open_column Open 컬럼 인덱스
  /// @param high_column High 컬럼 인덱스
  /// @param low_column Low 컬럼 인덱스
  /// @param close_column Close 컬럼 인덱스
  /// @param volume_column Volume 컬럼 인덱스
  /// @param close_time_column Close Time 컬럼 인덱스
  static void AddBarDataBatch(
      const vector<string>& symbol_names, const string& timeframe,
      const string& klines_directory, const BarType bar_type,
      const int open_time_column = 0, const int open_column = 1,
      const int high_column = 2, const int low_column = 3,
      const int close_column = 4, const int volume_column = 5,
      const int close_time_column = 6) {
    if (symbol_names.empty()) {
      return;
    }

    // 파일 경로들을 미리 계산
    vector<string> file_paths;
    file_paths.reserve(symbol_names.size());

    for (const string& symbol_name : symbol_names) {
      string file_path;
      if (bar_type == MARK_PRICE) {
        file_path = format("{}/{}/{}.parquet", klines_directory, symbol_name,
                           timeframe);
      } else {
        file_path = format("{}/{}/{}/{}.parquet", klines_directory, symbol_name,
                           timeframe, timeframe);
      }
      file_paths.emplace_back(move(file_path));
    }

    // 최적화된 배치 처리 함수 사용
    BarHandler::GetBarHandler()->AddBarDataBatch(
        symbol_names, file_paths, bar_type, open_time_column, open_column,
        high_column, low_column, close_column, volume_column,
        close_time_column);
  }

  /// 거래소 정보를 엔진에 추가하는 함수
  static void AddExchangeInfo(const string& exchange_info_path) {
    Engine::AddExchangeInfo(exchange_info_path);
  }

  /// 레버리지 구간을 엔진에 추가하는 함수
  static void AddLeverageBracket(const string& leverage_bracket_path) {
    Engine::AddLeverageBracket(leverage_bracket_path);
  }

  /**
   * 펀딩 비율 데이터를 엔진에 추가하는 함수
   *
   *  파일들이 '펀딩 비율 디렉토리/심볼 이름.json' 경로로 존재해야 함
   * @param symbol_names 펀딩 비율 데이터를 추가할 심볼 이름들
   * @param funding_rates_directory 펀딩 비율 JSON 파일들이 위치한 디렉토리 경로
   */
  static void AddFundingRates(const vector<string>& symbol_names,
                              const string& funding_rates_directory) {
    Engine::AddFundingRates(symbol_names, funding_rates_directory);
  }

  /// 엔진에 전략을 추가하는 함수.
  ///
  /// 템플릿에 생성한 커스텀 전략을 추가하고 이름을 넣으면 됨.
  template <typename CustomStrategy, typename... Args>
  static void AddStrategy(const string& name, Args&&... args) {
    try {
      Strategy::AddStrategy<CustomStrategy>(name, std::forward<Args>(args)...);
    } catch (...) {
      // 하위에서 이미 상세 로그를 남겼으므로 여기서는 간단하게
      Logger::LogAndThrowError(
          format("[{}] 전략을 엔진에 추가하는 데 실패했습니다.", name),
          __FILE__, __LINE__);
    }
  }

 private:
  /// 설정값들이 올바르게 설정되었는지 검증하는 함수
  static void ValidateSettings() {
    if (market_data_directory_.empty()) {
      Logger::LogAndThrowError(
          "시장 데이터 경로가 설정되지 않았습니다. "
          "Backtesting::SetMarketDataDirectory 함수를 호출해 주세요.",
          __FILE__, __LINE__);
    }

    if (api_key_env_var_.empty() || api_secret_env_var_.empty()) {
      Logger::LogAndThrowError(
          "API 환경변수가 설정되지 않았습니다. "
          "Backtesting::SetApiEnvVars 함수를 호출해 주세요.",
          __FILE__, __LINE__);
    }
  }

  static string market_data_directory_;
  static string api_key_env_var_;
  static string api_secret_env_var_;
};

}  // namespace backtesting