#pragma once

// 내부 헤더
#include "Engines/BaseBarHandler.hpp"
#include "Engines/BinanceFetcher.hpp"
#include "Engines/Config.hpp"
#include "Engines/Engine.hpp"
#include "Engines/Logger.hpp"
#include "Strategies/TestStrategy.hpp"

// 네임 스페이스
using namespace backtesting;
using namespace config;
using namespace fetcher;

namespace backtesting {

class Backtesting {
 public:
  /**
   * 백테스팅을 실행하는 함수
   *
   * @param start_time: 백테스팅 시작 시간
   * @param end_time: 백테스팅 끝 시간
   * @param format start_time, end_time의 시간 포맷
   */
  static void Run(const string& start_time = "", const string& end_time = "",
                  const string& format = "%Y-%m-%d %H:%M:%S") {
    Engine::GetEngine()->Backtesting(start_time, end_time, format);
  }

  /**
   * 지정된 로그 레벨과 파일 및 라인 정보를 사용하여 메시지를 기록하는 함수
   *
   * @param log_level 로그 메시지의 레벨
   * @param message 기록할 로그 메시지
   * @param file 로그가 생성된 파일의 이름. __FILE__로 지정
   * @param line 로그 명령문이 발생한 파일의 라인 번호. __LINE__으로 지정
   */
  static void Log(const LogLevel& log_level, const string& message,
                  const string& file, const int line) {
    Logger::GetLogger()->Log(log_level, message, file, line);
  }

  /**
   * 지정된 심볼과 시간 프레임에 대해 현물 및 연속 선물 klines 데이터를
   * Fetch 후 병합하고 Parquet 형식으로 저장하는 함수
   *
   * @param symbol 연속 선물 캔들스틱 데이터를 가져올
   *               거래 쌍 심볼(예: "BTCUSDT")
   * @param timeframe 연속 선물 캔들스틱 데이터의 타임프레임(예: "1m", "1h")
   */
  static void FetchContinuousKlines(const string& symbol,
                                    const string& timeframe) {
    make_shared<BinanceFetcher>("BINANCE_API_KEY", "BINANCE_API_SECRET")
        ->FetchContinuousKlines(symbol, timeframe);
  }

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
  static void FetchMarkPriceKlines(const string& symbol,
                                   const string& timeframe) {
    make_shared<BinanceFetcher>("BINANCE_API_KEY", "BINANCE_API_SECRET")
        ->FetchMarkPriceKlines(symbol, timeframe);
  }

  /// 바이낸스 선물 거래소 정보를 Fetch하고 저장하는 함수
  static void FetchExchangeInfo() {
    make_shared<BinanceFetcher>("BINANCE_API_KEY", "BINANCE_API_SECRET")
        ->FetchExchangeInfo();
  }

  /// 바이낸스 레버리지 구간을 Fecth하고 저장하는 함수
  static void FetchLeverageBracket() {
    make_shared<BinanceFetcher>("BINANCE_API_KEY", "BINANCE_API_SECRET")
        ->FetchLeverageBracket();
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
    make_shared<BinanceFetcher>("BINANCE_API_KEY", "BINANCE_API_SECRET")
        ->UpdateContinuousKlines(symbol, timeframe);
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
    make_shared<BinanceFetcher>("BINANCE_API_KEY", "BINANCE_API_SECRET")
        ->UpdateMarkPriceKlines(symbol, timeframe);
  }

  /// 주어진 파일 경로에서 Parquet 데이터를 읽고
  /// 지정된 바 타입으로 처리하여 바 핸들러에 추가하는 함수
  ///
  /// @param symbol_name 심볼 이름
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
    Engine::AddBarData(symbol_name, file_path, bar_type, open_time_column,
                       open_column, high_column, low_column, close_column,
                       volume_column, close_time_column);
  }

  /// 거래소 정보를 엔진에 추가하는 함수
  static void AddExchangeInfo(const string& exchange_info_path) {
    Engine::AddExchangeInfo(exchange_info_path);
  }

  /// 레버리지 구간을 엔진에 추가하는 함수
  static void AddLeverageBracket(const string& leverage_bracket_path) {
    Engine::AddLeverageBracket(leverage_bracket_path);
  }

  /// 엔진에 전략을 추가하는 함수.
  ///
  /// 템플릿에 생성한 커스텀 전략을 추가하고 이름과 인수를 넣으면 됨.
  template <typename CustomStrategy, typename... Args>
  static void AddStrategy(const string& name, Args&&... args) {
    Strategy::AddStrategy<CustomStrategy>(name, std::forward<Args>(args)...);
  }

  /// 엔진에 설정값을 추가하는 함수.
  ///
  /// 반환받은 객체를 통해 설정 함수를 호출하면 됨.
  static Config& SetConfig() { return Config::SetConfig(); }
};

}  // namespace backtesting