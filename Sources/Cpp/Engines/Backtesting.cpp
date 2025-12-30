// =============================================================================
// Copyright (c) 2024-2026 Traderhs
//
// software is provided for personal, educational, and non-commercial use only.
//
// Commercial use, including but not limited to selling, licensing, or using
// this software as part of a paid product or service, is strictly prohibited
// without prior written permission from the copyright holder.
//
// You may modify and distribute this software for non-commercial purposes,
// provided that this copyright notice is included.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND.
//
// For commercial licensing inquiries, please contact: dice000908@gmail.com

// =============================================================================
//  ● Backtesting - 바이낸스 선물 구조를 반영한 다중 심볼 백테스팅 엔진 ●
//
//  ◆ 다중 심볼 포트폴리오 백테스팅 ◆
//  ◆ 벡터화 내부 구조로 고속 백테스팅 ◆
//  ◆ 펀딩, 강제 청산, 레버리지 등 반영 ◆
//  ◆ 그래프 시각화 분석 ◆
//  ◆ 성과 통계 분석 ◆
//  ◆ 워크 포워드, 몬테카를로 시뮬레이션 등 고급 통계 분석 ◆
//
// =============================================================================
//   ● 기본 작동 ●
//
//   ◆ Windows/MSVC만 지원
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
//
// =============================================================================

// 표준 라이브러리
#include <chrono>
#include <format>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// 외부 라이브러리
#include <nlohmann/json.hpp>

// 파일 헤더
#include "Engines/Backtesting.hpp"

// 내부 헤더
#include "Engines/Exception.hpp"
#include "Engines/TimeUtils.hpp"
#include "Strategies/DiceSystem.hpp"

// 네임스페이스
using namespace backtesting::main;

namespace backtesting::main {

shared_ptr<BarHandler>& Backtesting::bar_ = BarHandler::GetBarHandler();
shared_ptr<Logger>& Backtesting::logger_ = Logger::GetLogger();

string Backtesting::market_data_directory_;
string Backtesting::api_key_env_var_;
string Backtesting::api_secret_env_var_;

// 서버 모드 플래그
static bool server_mode = false;

void Backtesting::RunBacktesting() {
  try {
    Engine::GetEngine()->Backtesting();
  } catch (...) {
    Logger::LogAndThrowError("백테스팅 실행 중 오류가 발생했습니다.", __FILE__,
                             __LINE__);
  }
}

void Backtesting::RunLocal() {
  // TODO 이 함수는 서버에서 실행해달라는 메시지만 로그. 그 외 fetch 코드만 주석

  // 거래소 정책은 계속 변화하므로 매번 저장
  SetMarketDataDirectory("D:/Programming/Backtesting/Data");
  SetApiEnvVars("BINANCE_API_KEY", "BINANCE_API_SECRET");

  FetchExchangeInfo();
  FetchLeverageBracket();

  AddExchangeInfo("D:/Programming/Backtesting/Data/exchange_info.json");
  AddLeverageBracket("D:/Programming/Backtesting/Data/leverage_bracket.json");

  // 엔진 설정
  SetConfig()
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

  // 심볼 설정
  const vector<string>& symbol_names = {
      "BTCUSDT",  "APTUSDT", "ETHUSDT",  "BNBUSDT", "SOLUSDT",
      "DOGEUSDT", "ADAUSDT", "AVAXUSDT", "DOTUSDT", "XRPUSDT"};

  // 바 데이터 설정
  AddBarData(symbol_names, "1h",
             "D:/Programming/Backtesting/Data/Continuous Klines", TRADING);

  AddBarData(symbol_names, "1m",
             "D:/Programming/Backtesting/Data/Continuous Klines", MAGNIFIER);

  AddBarData(symbol_names, "1d",
             "D:/Programming/Backtesting/Data/Continuous Klines", REFERENCE);

  AddBarData(symbol_names, "1m",
             "D:/Programming/Backtesting/Data/Mark Price Klines", MARK_PRICE);

  AddFundingRates(symbol_names,
                  "D:/Programming/Backtesting/Data/Funding Rates");

  // 전략 설정
  AddStrategy<DiceSystem>("Dice System");

  RunBacktesting();
}

void Backtesting::RunServer() {
  // 서버 모드로 진입했음을 알리는 로그를 출력하여 Js가 준비 상태를 감지
  cout << "백테스팅 엔진 준비 완료" << endl;
  string line;

  // stdin 명령 루프
  while (getline(cin, line)) {
    if (line.empty()) {
      continue;
    }

    istringstream iss(line);
    string cmd;
    iss >> cmd;

    try {
      if (cmd == "runSingleBacktesting") {
        // 나머지 라인을 JSON으로 파싱
        string json_str;
        getline(iss, json_str);

        RunSingleBacktesting(json_str);
      } else if (cmd == "shutdown") {
        break;
      } else {
        cout << "알 수 없는 명령어: " << cmd << endl;
      }
    } catch (...) {
      // 무시하고 다음 루프 진행
    }

    Engine::LogSeparator(true);
  }
}

void Backtesting::RunSingleBacktesting(const string& json_str) {
  try {
    if (!server_mode) {
      throw runtime_error(
          "RunSingleBacktesting 함수는 서버 모드에서만 실행 가능합니다.");
    }

    if (json_str.empty()) {
      throw runtime_error("서버 오류: 빈 Json이 C++ 서버로 전달되었습니다.");
    }

    const json& json_config = json::parse(json_str);

    // =======================================================================
    // Config 설정
    // =======================================================================
    const auto& project_directory =
        json_config.at("projectDirectory").get<string>();

    SetConfig()
        .SetProjectDirectory(project_directory)
        .SetUseBarMagnifier(json_config.at("useBarMagnifier").get<bool>());

    // TODO 각종 설정 추가

    // =======================================================================
    // 변수 설정, 데이터 Fetch 및 추가
    // =======================================================================
    const auto& data_directory = project_directory + "/Data";

    SetMarketDataDirectory(data_directory);
    SetApiEnvVars(json_config.at("apiKeyEnvVar").get<string>(),
                  json_config.at("apiSecretEnvVar").get<string>());

    bool updated = false;
    const auto now = chrono::duration_cast<chrono::milliseconds>(
                         chrono::system_clock::now().time_since_epoch())
                         .count();

    const auto& exchange_info_path = data_directory + "/exchange_info.json";
    const auto& leverage_bracket_path =
        data_directory + "/leverage_bracket.json";

    // 거래소 정보 파일이 존재하지 않을 때
    const auto exist_exchange_info = filesystem::exists(exchange_info_path);
    if (!exist_exchange_info) {
      logger_->Log(INFO_L,
                   "거래소 정보 파일이 존재하지 않아 데이터를 요청합니다.",
                   __FILE__, __LINE__, true);

      FetchExchangeInfo();
      updated = true;
    }

    // 레버리지 구간 파일이 존재하지 않을 때
    const auto exist_leverage_bracket =
        filesystem::exists(leverage_bracket_path);
    if (!exist_leverage_bracket) {
      logger_->Log(INFO_L,
                   "레버리지 구간 파일이 존재하지 않아 데이터를 요청합니다.",
                   __FILE__, __LINE__, true);

      FetchLeverageBracket();
      updated = true;
    }

    // 두 파일이 모두 존재할 때
    if (exist_exchange_info && exist_leverage_bracket) {
      bool need_update = false;
      string last_data_update_datetime;

      // "마지막 데이터 업데이트" 항목이 존재하는지 확인
      if (json_config.contains("lastDataUpdates")) {
        // "마지막 데이터 업데이트"가 빈 문자열이 아닌지 확인
        if (last_data_update_datetime =
                json_config["lastDataUpdates"].get<string>();
            !last_data_update_datetime.empty()) {
          const auto last_data_update = UtcDatetimeToUtcTimestamp(
              last_data_update_datetime, "%Y-%m-%d %H:%M:%S");

          // 마지막 데이터 업데이트 시간으로부터 24시간이 지났으면
          // 데이터 업데이트 필요
          if (constexpr int64_t ts_24_hour_ms = 24 * 60 * 60 * 1000;
              now - last_data_update >= ts_24_hour_ms) {
            logger_->Log(
                INFO_L,
                format(
                    "현재 시간 [{}]이 마지막 데이터 업데이트 시간 [{}]으로부터 "
                    "24시간이 경과하여 데이터를 업데이트합니다.",
                    UtcTimestampToUtcDatetime(now), last_data_update_datetime),
                __FILE__, __LINE__, true);

            need_update = true;
          }
        } else {
          logger_->Log(INFO_L,
                       "마지막 데이터 업데이트 시간이 존재하지 않아 데이터를 "
                       "업데이트합니다.",
                       __FILE__, __LINE__, true);

          need_update = true;
        }
      } else {
        // "마지막 데이터 업데이트" 항목 자체가 없는 경우 무조건 업데이트
        logger_->Log(INFO_L,
                     "마지막 데이터 업데이트 항목이 존재하지 않아 데이터를 "
                     "업데이트합니다.",
                     __FILE__, __LINE__, true);

        need_update = true;
      }

      if (need_update) {
        FetchExchangeInfo();
        FetchLeverageBracket();

        updated = true;
      } else {
        logger_->Log(
            INFO_L,
            format("현재 시간 [{}]이 마지막 데이터 업데이트 시간 [{}]으로부터 "
                   "24시간이 경과하지 않아 데이터 업데이트를 건너뜁니다.",
                   UtcTimestampToUtcDatetime(now), last_data_update_datetime),
            __FILE__, __LINE__, true);
      }
    }

    if (updated) {
      const json& payload = UtcTimestampToUtcDatetime(now);

      cout << "업데이트 완료" << payload.dump() << endl;
    }

    // 데이터 추가
    AddExchangeInfo(exchange_info_path);
    AddLeverageBracket(leverage_bracket_path);

    // =========================================================================
    // 바 데이터 추가
    // =========================================================================
    // 서버에서 바 데이터 초기화 및 추가를 요청할 때 사용
    // (초기 추가, 심볼/바 데이터 설정 변경 감지 시 수행)
    static bool is_first_adding = true;

    // 바 데이터를 추가할 때 오류 발생 시 바 데이터를 초기화하기 위한 플래그
    static bool bar_data_adding_error_occurred = false;

    // 서버에서 바 데이터 초기화 및 추가를 요청하거나,
    // 이전 바 데이터 추가 중 오류 발생 시 바 데이터를 초기화하고 다시 추가
    // 초기 추가는 무조건 바 데이터 추가
    if (json_config.at("clearAndAddBarData").get<bool>() ||
        bar_data_adding_error_occurred || is_first_adding) {
      // 초기 추가가 아닐 때만 바 데이터 초기화
      if (!is_first_adding) {
        if (!bar_data_adding_error_occurred) {
          logger_->Log(INFO_L,
                       "심볼 또는 바 데이터 설정 변경이 감지되어 "
                       "바 데이터를 초기화한 후 다시 추가합니다.",
                       __FILE__, __LINE__, true);
        } else {
          logger_->Log(INFO_L,
                       "이전 바 데이터 추가 과정에서 오류가 발생하여 "
                       "바 데이터를 초기화한 후 다시 추가합니다",
                       __FILE__, __LINE__, true);

          bar_data_adding_error_occurred = false;
        }

        bar_->ClearBarData();
      } else {
        // 초기 추가일 때는 바 데이터 미초기화
        // 첫 추가는 이전 추가가 없으므로, 추가 중 오류는 발생하지 않았기 때문에
        // 따로 처리하지 않음
        is_first_adding = false;
      }

      // 심볼 이름들 파싱
      vector<string> symbol_names;

      const auto& symbol_configs = json_config.at("symbolConfigs");
      if (!symbol_configs.is_array()) {
        throw runtime_error(
            "서버 오류: C++ 서버로 전달된 심볼 설정이 배열이 아닙니다.");
      }

      for (const auto& symbol_name : symbol_configs) {
        symbol_names.push_back(symbol_name.get<string>());
      }

      // 바 데이터 파싱
      const auto& bar_data_configs = json_config.at("barDataConfigs");
      if (!bar_data_configs.is_array()) {
        throw runtime_error(
            "서버 오류: C++ 서버로 전달된 바 데이터 설정이 배열이 아닙니다.");
      }

      // 각 바 데이터 유형별로 추가
      try {
        for (const auto& bar_data_config : bar_data_configs) {
          // 타임프레임, 바 데이터 폴더, 바 데이터 유형 파싱
          const auto& timeframe = bar_data_config.at("timeframe").get<string>();
          const auto& klines_directory =
              bar_data_config.at("klinesDirectory").get<string>();
          const auto& bar_data_type_str =
              bar_data_config.at("barDataType").get<string>();

          BarDataType bar_data_type{};
          if (bar_data_type_str == "트레이딩") {
            bar_data_type = TRADING;
          } else if (bar_data_type_str == "돋보기") {
            bar_data_type = MAGNIFIER;
          } else if (bar_data_type_str == "참조") {
            bar_data_type = REFERENCE;
          } else if (bar_data_type_str == "마크 가격") {
            bar_data_type = MARK_PRICE;
          } else {
            throw runtime_error("서버 오류: 잘못된 바 데이터 유형 지정");
          }

          // 바 데이터로 추가
          if (!symbol_names.empty() && !timeframe.empty() &&
              !klines_directory.empty()) {
            AddBarData(symbol_names, timeframe, klines_directory,
                       bar_data_type);
          }
        }
      } catch ([[maybe_unused]] const std::exception& e) {
        // 바 데이터 추가 중 발생한 오류는 다음 실행 때
        // 바 데이터를 강제로 초기화하도록 설정
        bar_data_adding_error_occurred = true;

        throw;
      }
    } else {
      logger_->Log(INFO_L, "바 데이터가 이미 캐시되어 있어 추가하지 않습니다.",
                   __FILE__, __LINE__, true);
    }

    return;  // TODO 테스트용 임시 리턴

    RunBacktesting();
  } catch (const std::exception& e) {
    logger_->Log(ERROR_L, e.what(), __FILE__, __LINE__, true);
    Logger::LogAndThrowError("단일 백테스팅 실행 중 오류가 발생했습니다.",
                             __FILE__, __LINE__);
  }
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
  try {
    ValidateSettings();
    BinanceFetcher(api_key_env_var_, api_secret_env_var_,
                   market_data_directory_)
        .FetchExchangeInfo();
  } catch (const std::exception& e) {
    if (server_mode) {
      throw;
    }

    Logger::LogAndThrowError(e.what(), __FILE__, __LINE__);
  }
}

void Backtesting::FetchLeverageBracket() {
  try {
    ValidateSettings();
    BinanceFetcher(api_key_env_var_, api_secret_env_var_,
                   market_data_directory_)
        .FetchLeverageBracket();
  } catch (const std::exception& e) {
    if (server_mode) {
      throw;
    }

    Logger::LogAndThrowError(e.what(), __FILE__, __LINE__);
  }
}

void Backtesting::AddBarData(const vector<string>& symbol_names,
                             const string& timeframe,
                             const string& klines_directory,
                             const BarDataType bar_data_type,
                             const int open_time_column, const int open_column,
                             const int high_column, const int low_column,
                             const int close_column, const int volume_column,
                             const int close_time_column) {
  if (symbol_names.empty()) {
    return;
  }

  vector<string> file_paths;
  file_paths.reserve(symbol_names.size());

  for (const string& symbol_name : symbol_names) {
    string file_path;

    if (bar_data_type == MARK_PRICE) {
      file_path =
          format("{}/{}/{}.parquet", klines_directory, symbol_name, timeframe);
    } else {
      file_path = format("{}/{}/{}/{}.parquet", klines_directory, symbol_name,
                         timeframe, timeframe);
    }

    file_paths.emplace_back(move(file_path));
  }

  BarHandler::GetBarHandler()->AddBarData(
      symbol_names, file_paths, bar_data_type, open_time_column, open_column,
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
    if (server_mode) {
      throw exception::InvalidValue(
          "시장 데이터 경로가 설정되지 않았습니다. "
          "Backboard 폴더 내 editor.json 및 editor.json.bak 파일 삭제 후 "
          "백보드를 다시 실행해 주세요.");
    }

    throw exception::InvalidValue(
        "시장 데이터 경로가 설정되지 않았습니다. "
        "Backtesting::SetMarketDataDirectory 함수를 호출해 주세요.");
  }

  if (api_key_env_var_.empty() || api_secret_env_var_.empty()) {
    if (server_mode) {
      throw exception::InvalidValue("API 환경 변수가 설정되지 않았습니다.");
    }

    throw exception::InvalidValue(
        "API 환경 변수가 설정되지 않았습니다. "
        "Backtesting::SetApiEnvVars 함수를 호출해 주세요.");
  }
}

}  // namespace backtesting::main

int main(const int argc, char** argv) {
  // 서버 모드 플래그 확인
  for (int i = 1; i < argc; i++) {
    if (string(argv[i]) == "--server") {
      server_mode = true;

      break;
    }
  }

  // 서버 모드에 따라 실행
  if (server_mode) {
    Backtesting::RunServer();
  } else {
    Backtesting::RunLocal();
  }
}
