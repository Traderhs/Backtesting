//
//  © 2024 Traderhs. All rights reserved.
//
//  ● 다중 심볼에 대한 고속, 정밀 백테스팅을 지원하는 프로그램 ●
//
//  ◆ 다중 자산 포트폴리오 백테스팅 ◆
//  ◆ 벡터화 내부 구조로 고속 백테스팅 ◆
//  ◆ 그래프 시각화 분석 ◆
//  ◆ 성과 통계 분석 ◆
//  ◆ 워크 포워드, 몬테카를로 시뮬레이션 등 고급 통계 분석 ◆
//
//  ============================================================================
//   ● 기본 작동 ●
//
//   ◆ MSVC만 지원
//   ◆ 시간은 GMT 기준
//   ◆ 바 데이터 형식은 Parquet만 지원하며,
//     Open Time, Open, High, Low, Close, Volume, Close Time 열이 존재해야 함.
//   ◆ 타임프레임 길이는 돋보기 < 트레이딩 <= 참조 바 데이터의
//     부등호를 따르는 배수 관계
//   ◆ 마크 가격 바 데이터 타임프레임은 돋보기 기능을
//     사용 시 돋보기 바 데이터 타임프레임과 동일해야 하며,
//     미사용 시 트레이딩 바 데이터 타임프레임과 동일해야 함.
//   ◆ 각 바의 움직임은 봉 가정을 따름 → GetPriceQueue 함수 참조
//   ◆ 각 진입은 격리로 작동하며 단방향으로만 동시 진입 가능
//   ◆ 진입 및 청산 시 지정하는 포지션 크기는 레버리지 미포함 전체 크기
//   ◆ 초기 마진 = 체결 가격 * 체결 크기 / 레버리지 + 진입 심볼의 미실현 손실
//   ◆ 레버리지는 해당 심볼에 체결된 포지션이 없을 때만 변경 가능
//     → AdjustLeverage 함수 참조
//   ◆ 진입 및 청산 세부 작동 구조는 OrderHandler 헤더 파일 참조
//   ◆ 미실현 손익 계산, 강제 청산 확인은 Mark Price를 사용하지만,
//     데이터가 누락된 경우 시장 가격을 사용함
//   ◆ 커스텀 전략 및 지표 생성 방법은 Strategy, Indicator 헤더 파일 참조
//   ◆ 한 백테스팅에는 한 전략만 추가 가능하며, 백보드에서 전략 합성 기능 제공
//     → 각 전략마다 독립 계좌로 작동 가정
//  ============================================================================

// 표준 라이브러리
#include <format>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// 외부 라이브러리
#include <nlohmann/json.hpp>

// 파일 헤더
#include "Backtesting.hpp"

// 내부 헤더
#include "Strategies/DiceSystem.hpp"

namespace backtesting {

string Backtesting::market_data_directory_;
string Backtesting::api_key_env_var_;
string Backtesting::api_secret_env_var_;

// 서버 모드 플래그
static bool server_mode = false;

void Backtesting::Run() {
  try {
    Engine::GetEngine()->Backtesting();
  } catch (...) {
    Logger::LogAndThrowError("백테스팅 진행 중 오류가 발생했습니다.", __FILE__,
                             __LINE__);
  }
}

void Backtesting::RunSingleBacktesting(const string& json_str) {
  if (!server_mode) {
    Logger::LogAndThrowError(
        "RunSingleBacktesting 함수는 서버 모드에서만 실행 가능합니다.",
        __FILE__, __LINE__);
  }

  static bool bar_data_loaded = false;  // 바 데이터 로드 여부 캐시

  if (!json_str.empty()) {
    if (json config = json::parse(json_str);
        !config.value("skipBarDataLoading", false) || !bar_data_loaded) {
      if (config.contains("barDataConfigs") &&
          config["barDataConfigs"].is_array()) {
        for (const auto& bar_config : config["barDataConfigs"]) {
          vector<string> symbols;
          if (bar_config.contains("symbols") &&
              bar_config["symbols"].is_array()) {
            for (const auto& symbol : bar_config["symbols"]) {
              symbols.push_back(symbol.get<string>());
            }
          }

          string timeframe = bar_config.value("timeframe", "1h");
          string directory_path = bar_config.value("directoryPath", "");
          string type_str = bar_config.value("type", "TRADING");

          const BarType bar_type = type_str == "TRADING"      ? TRADING
                                   : type_str == "MAGNIFIER"  ? MAGNIFIER
                                   : type_str == "REFERENCE"  ? REFERENCE
                                   : type_str == "MARK_PRICE" ? MARK_PRICE
                                                              : TRADING;

          if (!symbols.empty() && !directory_path.empty()) {
            AddBarData(symbols, timeframe, directory_path, bar_type);
          }
        }

        bar_data_loaded = true;
      }

      if (config.contains("useMagnifier")) {
        const bool use_magnifier = config["useMagnifier"].get<bool>();
        SetConfig().SetUseBarMagnifier(use_magnifier);
      }
    }
  }

  Run();
}

Config& Backtesting::SetConfig() { return Config::SetConfig(); }

void Backtesting::SetApiEnvVars(const string& api_key_env_var,
                                const string& api_secret_env_var) {
  api_key_env_var_ = api_key_env_var;
  api_secret_env_var_ = api_secret_env_var;
}

void Backtesting::SetMarketDataDirectory(const string& market_data_directory) {
  if (!filesystem::exists(market_data_directory)) {
    Logger::LogAndThrowError(
        format("지정된 시장 데이터 폴더 [{}]이(가) 존재하지 않습니다: ",
               market_data_directory),
        __FILE__, __LINE__);
  }
  market_data_directory_ = market_data_directory;
}

void Backtesting::FetchContinuousKlines(const string& symbol,
                                        const string& timeframe) {
  ValidateSettings();
  BinanceFetcher(api_key_env_var_, api_secret_env_var_, market_data_directory_)
      .FetchContinuousKlines(symbol, timeframe);
}

void Backtesting::UpdateContinuousKlines(const string& symbol,
                                         const string& timeframe) {
  ValidateSettings();
  BinanceFetcher(api_key_env_var_, api_secret_env_var_, market_data_directory_)
      .UpdateContinuousKlines(symbol, timeframe);
}

void Backtesting::FetchMarkPriceKlines(const string& symbol,
                                       const string& timeframe) {
  ValidateSettings();
  BinanceFetcher(api_key_env_var_, api_secret_env_var_, market_data_directory_)
      .FetchMarkPriceKlines(symbol, timeframe);
}

void Backtesting::UpdateMarkPriceKlines(const string& symbol,
                                        const string& timeframe) {
  ValidateSettings();
  BinanceFetcher(api_key_env_var_, api_secret_env_var_, market_data_directory_)
      .UpdateMarkPriceKlines(symbol, timeframe);
}

void Backtesting::FetchFundingRates(const string& symbol) {
  ValidateSettings();
  BinanceFetcher(api_key_env_var_, api_secret_env_var_, market_data_directory_)
      .FetchFundingRates(symbol);
}

void Backtesting::UpdateFundingRates(const string& symbol) {
  ValidateSettings();
  BinanceFetcher(api_key_env_var_, api_secret_env_var_, market_data_directory_)
      .UpdateFundingRates(symbol);
}

void Backtesting::FetchExchangeInfo() {
  ValidateSettings();
  BinanceFetcher(api_key_env_var_, api_secret_env_var_, market_data_directory_)
      .FetchExchangeInfo();
}

void Backtesting::FetchLeverageBracket() {
  ValidateSettings();
  BinanceFetcher(api_key_env_var_, api_secret_env_var_, market_data_directory_)
      .FetchLeverageBracket();
}

void Backtesting::AddBarData(const vector<string>& symbol_names,
                             const string& timeframe,
                             const string& klines_directory,
                             const BarType bar_type, const int open_time_column,
                             const int open_column, const int high_column,
                             const int low_column, const int close_column,
                             const int volume_column,
                             const int close_time_column) {
  if (symbol_names.empty()) {
    return;
  }

  vector<string> file_paths;
  file_paths.reserve(symbol_names.size());

  for (const string& symbol_name : symbol_names) {
    string file_path;
    if (bar_type == MARK_PRICE) {
      file_path =
          format("{}/{}/{}.parquet", klines_directory, symbol_name, timeframe);
    } else {
      file_path = format("{}/{}/{}/{}.parquet", klines_directory, symbol_name,
                         timeframe, timeframe);
    }
    file_paths.emplace_back(move(file_path));
  }

  BarHandler::GetBarHandler()->AddBarData(
      symbol_names, file_paths, bar_type, open_time_column, open_column,
      high_column, low_column, close_column, volume_column, close_time_column);
}

void Backtesting::AddExchangeInfo(const string& exchange_info_path) {
  Engine::AddExchangeInfo(exchange_info_path);
}

void Backtesting::AddLeverageBracket(const string& leverage_bracket_path) {
  Engine::AddLeverageBracket(leverage_bracket_path);
}

void Backtesting::AddFundingRates(const vector<string>& symbol_names,
                                  const string& funding_rates_directory) {
  Engine::AddFundingRates(symbol_names, funding_rates_directory);
}

void Backtesting::ValidateSettings() {
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

}  // namespace backtesting

int main(const int argc, char** argv) {
  // 서버 모드 플래그 확인
  for (int i = 1; i < argc; ++i) {
    if (string(argv[i]) == "--server") {
      server_mode = true;
      break;
    }
  }

  // 서버 모드가 아니면 즉시 실행
  if (!server_mode) {
    // 거래소 정책은 계속 변화하므로 매번 저장
    Backtesting::SetApiEnvVars("BINANCE_API_KEY", "BINANCE_API_SECRET");
    Backtesting::SetMarketDataDirectory("D:/Programming/Backtesting/Data");
    Backtesting::FetchExchangeInfo();
    Backtesting::FetchLeverageBracket();

    const vector<string>& symbol_list = {
        "BTCUSDT",  "APTUSDT", "ETHUSDT",  "BNBUSDT", "SOLUSDT",
        "DOGEUSDT", "ADAUSDT", "AVAXUSDT", "DOTUSDT", "XRPUSDT"};

    Backtesting::AddBarData(symbol_list, "1h",
                            "D:/Programming/Backtesting/Data/Continuous Klines",
                            TRADING);

    Backtesting::AddBarData(symbol_list, "1m",
                            "D:/Programming/Backtesting/Data/Continuous Klines",
                            MAGNIFIER);

    Backtesting::AddBarData(symbol_list, "1d",
                            "D:/Programming/Backtesting/Data/Continuous Klines",
                            REFERENCE);

    Backtesting::AddBarData(symbol_list, "1m",
                            "D:/Programming/Backtesting/Data/Mark Price Klines",
                            MARK_PRICE);

    Backtesting::AddExchangeInfo(
        "D:/Programming/Backtesting/Data/exchange_info.json");
    Backtesting::AddLeverageBracket(
        "D:/Programming/Backtesting/Data/leverage_bracket.json");

    Backtesting::AddFundingRates(
        symbol_list, "D:/Programming/Backtesting/Data/Funding Rates");

    Backtesting::SetConfig()
        .SetProjectDirectory("D:/Programming/Backtesting")
        .SetBacktestPeriod()
        .SetUseBarMagnifier(true)
        .SetInitialBalance(10000)
        .SetTakerFeePercentage(0.045)
        .SetMakerFeePercentage(0.018)
        .SetSlippage(MarketImpactSlippage(2))
        .SetCheckLimitMaxQty(false)
        .SetCheckLimitMinQty(false)
        .SetCheckMarketMaxQty(false)
        .SetCheckMarketMinQty(false)
        .SetCheckMinNotionalValue(true);

    Backtesting::AddStrategy<DiceSystem>("Dice System");

    Backtesting::Run();
    return 0;
  }

  // 서버 모드: stdin 명령 루프
  string line;

  while (getline(cin, line)) {
    if (line.empty()) continue;

    istringstream iss(line);
    string cmd;
    iss >> cmd;

    try {
      if (cmd == "RunSingleBacktesting") {
        // 나머지 라인을 JSON으로 파싱
        string json_str;
        getline(iss, json_str);
        Backtesting::RunSingleBacktesting(json_str);
      } else if (cmd == "shutdown") {
        break;
      } else {
        cout << "Unknown command: " << cmd << "\n" << flush;
      }
    } catch (const exception& e) {
      cout << "ERROR: " << e.what() << "\n" << flush;
    }
  }
}