
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
//   ◆ 한 백테스팅에는 한 전략만 추가 가능하며, BackBoard에서 전략 합성 기능
//   제공
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
#include "Engines/StrategyLoader.hpp"
#include "Engines/TimeUtils.hpp"

// 네임스페이스
using namespace backtesting::main;

namespace backtesting::main {

BACKTESTING_API shared_ptr<BarHandler>& Backtesting::bar_ =
    BarHandler::GetBarHandler();
BACKTESTING_API shared_ptr<Logger>& Backtesting::logger_ = Logger::GetLogger();

BACKTESTING_API bool Backtesting::server_mode_ = false;
BACKTESTING_API vector<shared_ptr<StrategyLoader>> Backtesting::dll_loaders_;
BACKTESTING_API string Backtesting::market_data_directory_;
BACKTESTING_API string Backtesting::api_key_env_var_;
BACKTESTING_API string Backtesting::api_secret_env_var_;

void Backtesting::SetServerMode(const bool server_mode) {
  server_mode_ = server_mode;
}

bool Backtesting::IsServerMode() { return server_mode_; }

void Backtesting::RunBacktesting() {
  try {
    Engine::GetEngine()->Backtesting();
  } catch (const std::exception& e) {
    logger_->Log(ERROR_L, "백테스팅 실행 중 오류가 발생했습니다.", __FILE__,
                 __LINE__, true);

    throw runtime_error(e.what());
  }
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
      } else if (cmd == "fetchOrUpdateBarData") {
        string json_str;
        getline(iss, json_str);

        FetchOrUpdateBarData(json_str);
      } else if (cmd == "shutdown") {
        break;
      } else {
        cout << "알 수 없는 명령어: " << cmd << endl;
      }
    } catch (...) {
      // 무시하고 다음 루프 진행
    }
  }
}

void Backtesting::RunSingleBacktesting(const string& json_str) {
  try {
    if (!server_mode_) {
      throw runtime_error(
          "RunSingleBacktesting 함수는 서버 모드에서만 실행 가능합니다.");
    }

    if (json_str.empty()) {
      throw runtime_error("서버 오류: 빈 Json이 C++ 서버로 전달되었습니다.");
    }

    const json& json_config = json::parse(json_str);

    // 백테스팅 시작 시간 설정
    const int64_t start_ms =
        json_config.at("backtestingStartTime").get<int64_t>();

    const auto now_system = chrono::time_point_cast<chrono::milliseconds>(
        chrono::system_clock::now());

    const auto system_tp =
        chrono::time_point<chrono::system_clock, chrono::milliseconds>(
            chrono::milliseconds(start_ms));

    auto delta = now_system - system_tp;

    // 미래 시간 방어
    if (delta < chrono::milliseconds(0)) {
      delta = chrono::milliseconds(0);
    }

    Engine::backtesting_start_time_ =
        chrono::steady_clock::now() -
        chrono::duration_cast<chrono::steady_clock::duration>(delta);

    // =======================================================================
    // 거래소 설정
    // =======================================================================
    const auto& project_directory =
        json_config.at("projectDirectory").get<string>();
    const auto& exchange_info_path =
        json_config.at("exchangeInfoPath").get<string>();
    const auto& leverage_bracket_path =
        json_config.at("leverageBracketPath").get<string>();

    SetMarketDataDirectory(project_directory + "/Data");
    SetApiEnvVars(json_config.at("apiKeyEnvVar").get<string>(),
                  json_config.at("apiSecretEnvVar").get<string>());

    bool updated = false;

    // 거래소 정보 파일이 존재하지 않을 때
    const auto exist_exchange_info = filesystem::exists(exchange_info_path);
    if (!exist_exchange_info) {
      logger_->Log(INFO_L,
                   "거래소 정보 파일이 존재하지 않아 데이터를 요청합니다.",
                   __FILE__, __LINE__, true);

      FetchExchangeInfo(exchange_info_path);
      updated = true;
    }

    // 레버리지 구간 파일이 존재하지 않을 때
    const auto exist_leverage_bracket =
        filesystem::exists(leverage_bracket_path);
    if (!exist_leverage_bracket) {
      logger_->Log(INFO_L,
                   "레버리지 구간 파일이 존재하지 않아 데이터를 요청합니다.",
                   __FILE__, __LINE__, true);

      FetchLeverageBracket(leverage_bracket_path);
      updated = true;
    }

    const auto now = chrono::duration_cast<chrono::milliseconds>(
                         chrono::system_clock::now().time_since_epoch())
                         .count();

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
          const auto last_data_update = LocalDatetimeToUtcTimestamp(
              last_data_update_datetime, "%Y-%m-%d %H:%M:%S");

          // 마지막 데이터 업데이트 시간으로부터 24시간이 지났으면
          // 데이터 업데이트 필요
          if (constexpr int64_t ts_24_hour_ms = 24 * 60 * 60 * 1000;
              now - last_data_update >= ts_24_hour_ms) {
            logger_->Log(
                INFO_L,
                format(
                    "현재 시간 [{}]이 마지막 데이터 업데이트 시간 [{}]으로부터 "
                    "24시간이 경과하여 거래소 정보 및 레버리지 구간 데이터를 "
                    "업데이트합니다.",
                    UtcTimestampToLocalDatetime(now),
                    last_data_update_datetime),
                __FILE__, __LINE__, true);

            need_update = true;
          }
        } else {
          logger_->Log(INFO_L,
                       "마지막 데이터 업데이트 시간이 존재하지 않아 "
                       "거래소 정보 및 레버리지 구간 데이터를 업데이트합니다.",
                       __FILE__, __LINE__, true);

          need_update = true;
        }
      } else {
        // "마지막 데이터 업데이트" 항목 자체가 없는 경우 무조건 업데이트
        logger_->Log(INFO_L,
                     "마지막 데이터 업데이트 항목이 존재하지 않아 "
                     "거래소 정보 및 레버리지 구간 데이터를 업데이트합니다.",
                     __FILE__, __LINE__, true);

        need_update = true;
      }

      if (need_update) {
        FetchExchangeInfo(exchange_info_path);
        FetchLeverageBracket(leverage_bracket_path);

        updated = true;
      } else {
        logger_->Log(
            INFO_L,
            format("현재 시간 [{}]이 마지막 데이터 업데이트 시간 [{}]으로부터 "
                   "24시간이 경과하지 않아 거래소 정보 및 레버리지 구간 데이터 "
                   "업데이트를 건너뜁니다.",
                   UtcTimestampToLocalDatetime(now), last_data_update_datetime),
            __FILE__, __LINE__, true);
      }
    }

    if (updated) {
      const json& payload = UtcTimestampToLocalDatetime(now);

      cout << "업데이트 완료" << payload.dump() << endl;
    }

    // 데이터 추가
    AddExchangeInfo(exchange_info_path);
    AddLeverageBracket(leverage_bracket_path);

    // =========================================================================
    // 바 데이터 설정
    // =========================================================================
    // 서버에서 바 데이터 초기화 및 추가를 요청할 때 사용
    // (초기 추가, 심볼/바 데이터 설정 변경 감지 시 수행)
    static bool is_first_adding = true;

    // 바 데이터를 추가할 때 오류 발생 시 바 데이터를 초기화하기 위한 플래그
    static bool bar_data_adding_error_occurred = false;

    // 심볼 이름들 파싱
    vector<string> symbol_names;

    // symbolConfigs는 Js 서버에서 더 다양한 정보를 가지고 있지만,
    // CPP 서버로는 심볼 이름만 전달됨
    const auto& symbol_configs = json_config.at("symbolConfigs");
    if (!symbol_configs.is_array()) {
      throw runtime_error(
          "서버 오류: C++ 서버로 전달된 심볼 설정이 배열이 아닙니다.");
    }

    for (const auto& symbol_name : symbol_configs) {
      symbol_names.push_back(symbol_name.get<string>());
    }

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

        bar_->ResetBarHandler();
      } else {
        // 초기 추가일 때는 바 데이터 미초기화
        // 첫 추가는 이전 추가가 없으므로, 추가 중 오류는 발생하지 않았기 때문에
        // 따로 처리하지 않음
        is_first_adding = false;
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
      } catch (const std::exception& e) {
        // 바 데이터 추가 중 발생한 오류는 다음 실행 때
        // 바 데이터를 강제로 초기화하도록 설정
        bar_data_adding_error_occurred = true;

        throw runtime_error(e.what());
      }
    } else {
      logger_->Log(INFO_L, "바 데이터가 이미 캐시되어 있어 추가하지 않습니다.",
                   __FILE__, __LINE__, true);
    }

    // =======================================================================
    // 펀딩 비율 설정
    // =======================================================================
    AddFundingRates(symbol_names,
                    json_config.at("fundingRatesDirectory").get<string>());

    // =======================================================================
    // 엔진 설정
    // =======================================================================
    // "처음부터" 사용 시 무조건 처음부터 사용
    const auto& backtest_period_start =
        json_config.at("useBacktestPeriodStart").get<bool>()
            ? ""
            : json_config.at("backtestPeriodStart").get<string>();

    // "끝까지" 사용 시 무조건 끝까지 사용
    const auto& backtest_period_end =
        json_config.at("useBacktestPeriodEnd").get<bool>()
            ? ""
            : json_config.at("backtestPeriodEnd").get<string>();

    auto& config_builder =
        SetConfig()
            .SetProjectDirectory(project_directory)
            .SetBacktestPeriod(
                backtest_period_start, backtest_period_end,
                json_config.at("backtestPeriodFormat").get<string>())
            .SetUseBarMagnifier(json_config.at("useBarMagnifier").get<bool>())
            .SetInitialBalance(json_config.at("initialBalance").get<double>())
            .SetTakerFeePercentage(
                json_config.at("takerFeePercentage").get<double>())
            .SetMakerFeePercentage(
                json_config.at("makerFeePercentage").get<double>());

    // 슬리피지 타입에 따라 설정
    if (const auto& slippage_model =
            json_config.at("slippageModel").get<string>();
        slippage_model == "PercentageSlippage") {
      config_builder.SetSlippage(PercentageSlippage(
          json_config.at("slippageTakerPercentage").get<double>(),
          json_config.at("slippageMakerPercentage").get<double>()));
    } else if (slippage_model == "MarketImpactSlippage") {
      config_builder.SetSlippage(MarketImpactSlippage(
          json_config.at("slippageStressMultiplier").get<double>()));
    } else {
      throw runtime_error("서버 오류: 잘못된 슬리피지 타입 지정");
    }

    config_builder
        .SetCheckMarketMaxQty(json_config.at("checkMarketMaxQty").get<bool>())
        .SetCheckMarketMinQty(json_config.at("checkMarketMinQty").get<bool>())
        .SetCheckLimitMaxQty(json_config.at("checkLimitMaxQty").get<bool>())
        .SetCheckLimitMinQty(json_config.at("checkLimitMinQty").get<bool>())
        .SetCheckMinNotionalValue(
            json_config.at("checkMinNotionalValue").get<bool>());

    // 바 데이터 중복 검사 설정
    if (!json_config.at("checkSameBarDataWithTarget").get<bool>()) {
      config_builder.DisableSameBarDataWithTargetCheck();
    }

    if (!json_config.at("checkSameBarDataTrading").get<bool>()) {
      config_builder.DisableSameBarDataCheck(TRADING);
    }

    if (!json_config.at("checkSameBarDataMagnifier").get<bool>()) {
      config_builder.DisableSameBarDataCheck(MAGNIFIER);
    }

    if (!json_config.at("checkSameBarDataReference").get<bool>()) {
      config_builder.DisableSameBarDataCheck(REFERENCE);
    }

    if (!json_config.at("checkSameBarDataMarkPrice").get<bool>()) {
      config_builder.DisableSameBarDataCheck(MARK_PRICE);
    }

    // =======================================================================
    // 전략 설정
    // =======================================================================
    const auto& strategy_config = json_config.at("strategyConfig");

    // 전략 헤더 및 소스 폴더 설정
    vector<string> strategy_header_dirs;
    for (const auto& strategy_header_dir :
         strategy_config.at("strategyHeaderDirs")) {
      strategy_header_dirs.push_back(strategy_header_dir.get<string>());
    }

    config_builder.SetStrategyHeaderDirs(strategy_header_dirs);

    vector<string> strategy_source_dirs;
    for (const auto& strategy_source_dir :
         strategy_config.at("strategySourceDirs")) {
      strategy_source_dirs.push_back(strategy_source_dir.get<string>());
    }

    config_builder.SetStrategySourceDirs(strategy_source_dirs);

    // 지표 헤더 및 소스 폴더 설정
    vector<string> indicator_header_dirs;
    for (const auto& indicator_header_dir :
         strategy_config.at("indicatorHeaderDirs")) {
      indicator_header_dirs.push_back(indicator_header_dir.get<string>());
    }

    config_builder.SetIndicatorHeaderDirs(indicator_header_dirs);

    vector<string> indicator_source_dirs;
    for (const auto& indicator_source_dir :
         strategy_config.at("indicatorSourceDirs")) {
      indicator_source_dirs.push_back(indicator_source_dir.get<string>());
    }

    config_builder.SetIndicatorSourceDirs(indicator_source_dirs);

    // 전략 헤더 및 소스 파일 경로 설정
    config_builder.SetStrategyHeaderPath(
        strategy_config.at("strategyHeaderPath").get<string>());

    config_builder.SetStrategySourcePath(
        strategy_config.at("strategySourcePath").get<string>());

    // 새 로더 생성
    const auto loader = make_shared<StrategyLoader>();
    string error;

    // DLL 로드
    if (!loader->Load(strategy_config.at("dllPath").get<string>(), error)) {
      throw runtime_error(error);
    }

    // DLL에서 AddStrategy 호출
    if (!loader->AddStrategyFromDll(strategy_config.at("name").get<string>(),
                                    error)) {
      throw runtime_error(error);
    }

    // DLL 언로드 방지를 위해 로더 저장
    dll_loaders_.push_back(loader);

    // 백테스팅 실행
    RunBacktesting();

    // 엔진 코어 초기화
    ResetCores();
  } catch (const std::exception& e) {
    logger_->Log(ERROR_L, "단일 백테스팅 실행 중 오류가 발생했습니다.",
                 __FILE__, __LINE__, true);

    // 상세 오류 원인 로그
    logger_->Log(ERROR_L, e.what(), __FILE__, __LINE__, true);

    // 엔진 코어 초기화
    ResetCores();

    throw;
  }
}

void Backtesting::FetchOrUpdateBarData(const string& json_str) {
  try {
    if (!server_mode_) {
      throw runtime_error(
          "FetchOrUpdateBarData 함수는 서버 모드에서만 실행 가능합니다.");
    }

    if (json_str.empty()) {
      throw runtime_error("서버 오류: 빈 Json이 C++ 서버로 전달되었습니다.");
    }

    const json& json_config = json::parse(json_str);

    const string& operation = json_config.value("operation", "download");

    SetMarketDataDirectory(json_config.at("projectDirectory").get<string>() +
                           "/Data");
    SetApiEnvVars(json_config.at("apiKeyEnvVar").get<string>(),
                  json_config.at("apiSecretEnvVar").get<string>());

    // 심볼 목록
    vector<string> symbols;
    for (const auto& symbol : json_config.at("symbols")) {
      symbols.push_back(symbol.get<string>());
    }

    // 바 데이터 설정
    vector<pair<string, string>> bar_data_configs;
    for (const auto& bar_data_config : json_config.at("barDataConfigs")) {
      bar_data_configs.emplace_back(
          bar_data_config.at("barDataType").get<string>(),
          bar_data_config.at("timeframe").get<string>());
    }

    // 각 심볼 별로 요청 수행
    for (const auto& symbol : symbols) {
      // 바 데이터 처리
      for (const auto& [type, timeframe] : bar_data_configs) {
        try {
          if (type == "마크 가격") {
            if (operation == "download") {
              FetchMarkPriceKlines(symbol, timeframe);
            } else {
              UpdateMarkPriceKlines(symbol, timeframe);
            }
          } else {
            // 트레이딩/참조/돋보기 바 데이터는 전부 연속 선물 klines
            if (operation == "download") {
              FetchContinuousKlines(symbol, timeframe);
            } else {
              UpdateContinuousKlines(symbol, timeframe);
            }
          }
        } catch (...) {
          if (type == "마크 가격") {
            if (operation == "download") {
              logger_->Log(
                  ERROR_L,
                  format("[{}] 마크 가격 캔들스틱 파일 생성이 실패했습니다.",
                         symbol),
                  __FILE__, __LINE__, true);
            } else {
              logger_->Log(
                  ERROR_L,
                  format(
                      "[{}] 마크 가격 캔들스틱 파일 업데이트가 실패했습니다.",
                      symbol),
                  __FILE__, __LINE__, true);
            }
          } else {
            if (operation == "download") {
              logger_->Log(
                  ERROR_L,
                  format("[{} {}] 연속 선물 캔들스틱 파일 생성이 실패했습니다.",
                         symbol, timeframe),
                  __FILE__, __LINE__, true);
            } else {
              logger_->Log(ERROR_L,
                           format("[{} {}] 연속 선물 캔들스틱 파일 "
                                  "업데이트가 실패했습니다.",
                                  symbol, timeframe),
                           __FILE__, __LINE__, true);
            }
          }
        }
      }

      // 펀딩 비율 처리
      try {
        if (operation == "download") {
          FetchFundingRates(symbol);
        } else {
          UpdateFundingRates(symbol);
        }
      } catch (...) {
        if (operation == "download") {
          logger_->Log(
              ERROR_L,
              format("[{}] 펀딩 비율 파일 생성이 실패했습니다.", symbol),
              __FILE__, __LINE__, true);
        } else {
          logger_->Log(
              ERROR_L,
              format("[{}] 펀딩 비율 파일 업데이트가 실패했습니다.", symbol),
              __FILE__, __LINE__, true);
        }
      }
    }

    // 엔진 코어 초기화
    ResetCores();
  } catch (const std::exception& e) {
    logger_->Log(
        ERROR_L,
        "바 데이터를 다운로드 혹은 업데이트하는 중 오류가 발생했습니다.",
        __FILE__, __LINE__, true);

    // 상세 오류 원인 로그
    logger_->Log(ERROR_L, e.what(), __FILE__, __LINE__, true);

    // 엔진 코어 초기화
    ResetCores();

    throw;
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
    throw runtime_error(
        format("지정된 시장 데이터 폴더 [{}]이(가) 존재하지 않습니다: ",
               market_data_directory));
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

void Backtesting::FetchExchangeInfo(const string& exchange_info_path) {
  ValidateSettings();
  BinanceFetcher::FetchExchangeInfo(exchange_info_path);
}

void Backtesting::FetchLeverageBracket(const string& leverage_bracket_path) {
  ValidateSettings();
  BinanceFetcher(api_key_env_var_, api_secret_env_var_, market_data_directory_)
      .FetchLeverageBracket(leverage_bracket_path);
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
    if (server_mode_) {
      throw exception::InvalidValue(
          "시장 데이터 경로가 설정되지 않았습니다. "
          "BackBoard 폴더 내 editor.json 및 editor.json.bak 파일 삭제 후 "
          "BackBoard를 다시 실행해 주세요.");
    }

    throw exception::InvalidValue(
        "시장 데이터 경로가 설정되지 않았습니다. "
        "Backtesting::SetMarketDataDirectory 함수를 호출해 주세요.");
  }

  if (api_key_env_var_.empty() || api_secret_env_var_.empty()) {
    if (server_mode_) {
      throw exception::InvalidValue("API 환경 변수가 설정되지 않았습니다.");
    }

    throw exception::InvalidValue(
        "API 환경 변수가 설정되지 않았습니다. "
        "Backtesting::SetApiEnvVars 함수를 호출해 주세요.");
  }
}

void Backtesting::ResetCores() {
  Analyzer::ResetAnalyzer();
  BarHandler::GetBarHandler()->ResetBarHandlerState();
  Engine::GetEngine()->ResetEngine();
  Indicator::ResetIndicator();
  Logger::ResetLogger();
  OrderHandler::GetOrderHandler()->ResetOrderHandler();
  Slippage::ResetSlippage();
  Strategy::ResetStrategy();

  // 마지막에 DLL을 언로드해야 위쪽에서 리셋 가능
  dll_loaders_.clear();
}

}  // namespace backtesting::main
