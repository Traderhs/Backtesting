#pragma once

// 표준 라이브러리
#include <string>

// 내부 헤더
#include "Engines/BaseBarHandler.hpp"
#include "Engines/BinanceFetcher.hpp"
#include "Engines/Export.hpp"
#include "Engines/Logger.hpp"
#include "Engines/Strategy.hpp"
#include "Engines/StrategyLoader.hpp"

// 네임 스페이스
using namespace backtesting;
using namespace fetcher;

namespace backtesting::main {

class BACKTESTING_API Backtesting {
 public:
  Backtesting() = delete;
  ~Backtesting() = delete;

  /// 서버 모드로 설정하는 함수
  static void SetServerMode(bool server_mode);

  /// 백테스팅을 실행하는 함수
  static void RunBacktesting();

  /// 서버용 메인 실행
  static void RunServer();

  /// 서버용 단일 백테스팅 실행
  static void RunSingleBacktesting(const string& json_str);

  /// 엔진에 설정값을 추가하는 함수.
  ///
  /// 반환받은 객체를 통해 설정 함수를 호출하면 됨.
  static Config& SetConfig();

  /**
   * API 환경변수 이름을 설정하는 함수
   *
   * @param api_key_env_var API 키를 저장한 환경변수 이름
   * @param api_secret_env_var API 시크릿을 저장한 환경변수 이름
   */
  static void SetApiEnvVars(const string& api_key_env_var,
                            const string& api_secret_env_var);

  /**
   * 시장 데이터 경로를 설정하는 함수
   *
   * @param market_data_directory 설정할 시장 데이터 폴더
   */
  static void SetMarketDataDirectory(const string& market_data_directory);

  /**
   * 지정된 심볼과 시간 프레임에 대해 연속 선물 klines 데이터를
   * Fetch 후 Parquet 형식으로 저장하는 함수
   *
   * @param symbol 연속 선물 캔들스틱 데이터를 가져올
   *               거래 쌍 심볼(예: "BTCUSDT")
   * @param timeframe 연속 선물 캔들스틱 데이터의 타임프레임(예: "1m", "1h")
   */
  static void FetchContinuousKlines(const string& symbol,
                                    const string& timeframe);

  /**
   * 주어진 심볼과 시간 프레임에 대한 연속 선물 캔들스틱 데이터를
   * 업데이트하는 함수
   *
   * @param symbol 업데이트 할 연속 선물 캔들스틱 데이터의
   *               거래 쌍 심볼(예: "BTCUSDT")
   * @param timeframe 캔들스틱 데이터의 타임프레임(예: "1m", "1h")
   */
  static void UpdateContinuousKlines(const string& symbol,
                                     const string& timeframe);

  /**
   * 지정된 심볼과 시간 프레임에 대해 마크 가격 캔들스틱 데이터를
   * Fetch 후 Parquet 형식으로 저장하는 함수.
   *
   * @param symbol 마크 가격 캔들스틱 데이터를 가져올
   *               거래 쌍 심볼(예: "BTCUSDT")
   * @param timeframe 마크 가격 캔들스틱 데이터의 타임프레임(예: "1m", "1h")
   */
  static void FetchMarkPriceKlines(const string& symbol,
                                   const string& timeframe);

  /**
   * 주어진 심볼과 시간 프레임에 대한 마크 가격 캔들스틱 데이터를
   * 업데이트하는 함수
   *
   * @param symbol 업데이트 할 마크 가격 캔들스틱 데이터의
   *               거래 쌍 심볼(예: "BTCUSDT")
   * @param timeframe 마크 가격 캔들스틱 데이터의 타임프레임(예: "1m", "1h")
   */
  static void UpdateMarkPriceKlines(const string& symbol,
                                    const string& timeframe);
  /**
   * 지정된 심볼에 대해 펀딩 비율 데이터를 Fetch 후 json 형식으로 저장하는 함수
   *
   * @param symbol 펀딩 비율 데이터를 가져올
   *               거래 쌍 심볼(예: "BTCUSDT")
   */
  static void FetchFundingRates(const string& symbol);

  /**
   * 주어진 심볼에 대한 펀딩 비율 데이터를 업데이트하는 함수
   *
   * @param symbol 업데이트 할 펀딩 비율 데이터의
   *               거래 쌍 심볼(예: "BTCUSDT")
   */
  static void UpdateFundingRates(const string& symbol);

  /// 바이낸스 선물 거래소 정보를 Fetch하고 저장하는 함수
  static void FetchExchangeInfo(const string& exchange_info_path);

  /// 바이낸스 레버리지 구간을 Fecth하고 저장하는 함수
  static void FetchLeverageBracket(const string& leverage_bracket_path);

  /// 주어진 데이터 폴더에서 각 심볼들의 폴더를 찾아 Parquet 데이터를 읽고
  /// 지정된 바 데이터 유형으로 처리하여 바 핸들러에 추가하는 함수
  ///
  /// ※ 바 유형별로 해당 경로를 만족해야 함 ※\n
  /// 트레이딩(돋보기, 참조): 디렉토리/심볼 이름/타임프레임/타임프레임.parquet\n
  /// 마크 가격: 디렉토리/심볼 이름/타임프레임.parquet
  ///
  /// @param symbol_names 바 데이터로 추가할 심볼 이름들
  /// @param timeframe 추가할 데이터의 타임프레임
  /// @param klines_directory Parquet 파일들이 위치한 데이터 폴더
  /// @param bar_data_type 추가할 바 데이터 유형
  /// @param open_time_column Open Time 컬럼 인덱스
  /// @param open_column Open 컬럼 인덱스
  /// @param high_column High 컬럼 인덱스
  /// @param low_column Low 컬럼 인덱스
  /// @param close_column Close 컬럼 인덱스
  /// @param volume_column Volume 컬럼 인덱스
  /// @param close_time_column Close Time 컬럼 인덱스
  static void AddBarData(const vector<string>& symbol_names,
                         const string& timeframe,
                         const string& klines_directory,
                         BarDataType bar_data_type, int open_time_column = 0,
                         int open_column = 1, int high_column = 2,
                         int low_column = 3, int close_column = 4,
                         int volume_column = 5, int close_time_column = 6);

  /// 거래소 정보를 엔진에 추가하는 함수
  static void AddExchangeInfo(const string& exchange_info_path);

  /// 레버리지 구간을 엔진에 추가하는 함수
  static void AddLeverageBracket(const string& leverage_bracket_path);

  /**
   * 펀딩 비율 데이터를 엔진에 추가하는 함수
   *
   *  파일들이 '펀딩 비율 디렉토리/심볼 이름.json' 경로로 존재해야 함
   * @param symbol_names 펀딩 비율 데이터를 추가할 심볼 이름들
   * @param funding_rates_directory 펀딩 비율 JSON 파일들이 위치한 디렉토리 경로
   */
  static void AddFundingRates(const vector<string>& symbol_names,
                              const string& funding_rates_directory);

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
  static void ValidateSettings();

  static shared_ptr<BarHandler>& bar_;
  static shared_ptr<Logger>& logger_;

  // 서버 모드 플래그
  static bool server_mode_;

  // DLL 로더 저장소
  static vector<shared_ptr<StrategyLoader>> dll_loaders_;

  static string market_data_directory_;
  static string api_key_env_var_;
  static string api_secret_env_var_;

  /// 엔진 코어를 초기화하는 함수
  static void ResetCores();
};

}  // namespace backtesting::main
