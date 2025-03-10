// 표준 라이브러리
#include <array>
#include <cmath>
#include <filesystem>
#include <format>
#include <ranges>
#include <set>
#include <utility>

// 외부 라이브러리
#include "nlohmann/json.hpp"

// 파일 헤더
#include "Engines/Engine.hpp"

// 내부 헤더
#include "Engines/Analyzer.hpp"
#include "Engines/BarData.hpp"
#include "Engines/BarHandler.hpp"
#include "Engines/BaseBarHandler.hpp"
#include "Engines/Config.hpp"
#include "Engines/DataUtils.hpp"
#include "Engines/Exception.hpp"
#include "Engines/OrderHandler.hpp"
#include "Engines/Strategy.hpp"
#include "Engines/SymbolInfo.hpp"
#include "Engines/TimeUtils.hpp"

// 네임 스페이스
namespace backtesting {
using namespace exception;
using namespace utils;
}  // namespace backtesting

namespace backtesting::engine {

Engine::Engine()
    : use_bar_magnifier_(false),
      trading_bar_num_symbols_(0),
      trading_bar_time_diff_(0),
      magnifier_bar_time_diff_(0),
      current_strategy_type_(ON_CLOSE),
      begin_open_time_(INT64_MAX),
      end_open_time_(0),
      current_open_time_(0),
      current_close_time_(0) {}

void Engine::Deleter::operator()(const Engine* p) const { delete p; }

mutex Engine::mutex_;
shared_ptr<Engine> Engine::instance_;

shared_ptr<Engine>& Engine::GetEngine() {
  lock_guard lock(mutex_);  // 다중 스레드에서 안전하게 접근하기 위해 mutex 사용

  // 인스턴스가 생성됐는지 확인
  if (!instance_) {
    instance_ = shared_ptr<Engine>(new Engine(), Deleter());
  }

  return instance_;
}

void Engine::Backtesting(const string& start_time, const string& end_time,
                         const string& format) {
  const auto& start = chrono::high_resolution_clock::now();

  PrintSeparator();
  Initialize(start_time, end_time, format);

  PrintSeparator();
  logger_->Log(INFO_L, std::format("백테스팅을 시작합니다."), __FILE__,
               __LINE__);

  PrintSeparator();
  try {
    BacktestingMain();
  } catch ([[maybe_unused]] const Bankruptcy& e) {
    logger_->Log(ERROR_L, "파산으로 인해 백테스팅을 종료합니다.", __FILE__,
                 __LINE__);
  }

  PrintSeparator();
  logger_->Log(INFO_L, "백테스팅이 완료되었습니다.", __FILE__, __LINE__);

  const auto& end = chrono::high_resolution_clock::now();
  logger_->Log(
      INFO_L,
      "소요 시간: " +
          FormatTimeDiff(
              duration_cast<chrono::milliseconds>(end - start).count()),
      __FILE__, __LINE__);

  // 폴더 생성
  const string& main_directory = CreateDirectories();

  // 로그 저장
  logger_->SaveBacktestingLog(main_directory + "/backtesting.log");

  // 설정 저장
  SaveConfig(main_directory + "/config.json");

  // 전략 코드 저장

  // 지표 저장
  for (int strategy_idx = 0; strategy_idx < strategies_.size();
       strategy_idx++) {
    for (const auto& indicator : indicators_[strategy_idx]) {
      indicator->SaveIndicator(
          std::format("{}/Indicators/{}/{} {}.csv", main_directory,
                      strategies_[strategy_idx]->GetName(),
                      indicator->GetName(), indicator->GetTimeframe()));
    }
  }

  // 매매 목록 저장
  analyzer_->SaveTradingList(main_directory +
                             "/Trading Lists/trading list.csv");
}

double Engine::UpdateAvailableBalance() {
  available_balance_ = wallet_balance_ - used_margin_;

  return available_balance_;
}

void Engine::SetCurrentStrategyType(const StrategyType strategy_type) {
  current_strategy_type_ = strategy_type;
}
string Engine::GetCurrentStrategyName() const { return current_strategy_name_; }
StrategyType Engine::GetCurrentStrategyType() const {
  return current_strategy_type_;
}
int64_t Engine::GetCurrentOpenTime() const { return current_open_time_; }
int64_t Engine::GetCurrentCloseTime() const { return current_close_time_; }

void Engine::Initialize(const string& start_time, const string& end_time,
                        const string& format) {
  // 유효성 검증
  IsValidConfig();
  IsValidSymbolInfo();
  IsValidBarData();
  IsValidDateRange(start_time, end_time, format);
  IsValidStrategies();
  IsValidIndicators();

  // 초기화
  PrintSeparator();
  InitializeEngine();
  InitializeSymbolInfo();
  InitializeStrategies();
  InitializeIndicators();
}

void Engine::IsValidConfig() {
  try {
    if (config_ == nullptr) {
      Logger::LogAndThrowError(
          "엔진에 설정값이 추가되지 않았습니다. "
          "Config::SetConfig 함수를 호출해 주세요.",
          __FILE__, __LINE__);
    }

    const auto& root_directory = config_->GetRootDirectory();
    const auto initial_balance = config_->GetInitialBalance();
    const auto taker_fee_percentage = config_->GetTakerFeePercentage();
    const auto maker_fee_percentage = config_->GetMakerFeePercentage();
    const auto taker_slippage_percentage =
        config_->GetTakerSlippagePercentage();
    const auto maker_slippage_percentage =
        config_->GetMakerSlippagePercentage();

    // 각 항목에 대해 초기화되지 않았을 경우 예외를 던짐
    if (root_directory.empty()) {
      Logger::LogAndThrowError(
          "루트 폴더가 초기화되지 않았습니다. "
          "SetRootDirectory 함수를 호출해 주세요.",
          __FILE__, __LINE__);
    }

    if (!config_->UseBarMagnifierHasValue()) {
      Logger::LogAndThrowError(
          "바 돋보기 사용 여부가 초기화되지 않았습니다. "
          "SetUseBarMagnifier 함수를 호출해 주세요.",
          __FILE__, __LINE__);
    }

    if (isnan(initial_balance)) {
      Logger::LogAndThrowError(
          "초기 자금이 초기화되지 않았습니다. "
          "SetInitialBalance 함수를 호출해 주세요.",
          __FILE__, __LINE__);
    }

    if (isnan(taker_fee_percentage)) {
      Logger::LogAndThrowError(
          "테이커 수수료 퍼센트가 초기화되지 않았습니다. "
          "SetTakerFeePercentage 함수를 호출해 주세요.",
          __FILE__, __LINE__);
    }

    if (isnan(maker_fee_percentage)) {
      Logger::LogAndThrowError(
          "메이커 수수료 퍼센트가 초기화되지 않았습니다. "
          "SetMakerFeePercentage 함수를 호출해 주세요.",
          __FILE__, __LINE__);
    }

    if (isnan(taker_slippage_percentage)) {
      Logger::LogAndThrowError(
          "테이커 슬리피지 퍼센트가 초기화되지 않았습니다. "
          "SetTakerSlippagePercentage 함수를 호출해 주세요.",
          __FILE__, __LINE__);
    }

    if (isnan(maker_slippage_percentage)) {
      Logger::LogAndThrowError(
          "메이커 슬리피지 퍼센트가 초기화되지 않았습니다. "
          "SetMakerSlippagePercentage 함수를 호출해 주세요.",
          __FILE__, __LINE__);
    }

    if (!filesystem::exists(root_directory)) {
      Logger::LogAndThrowError(
          format("지정된 루트 폴더 [{}]은(는) 유효하지 않습니다.",
                 root_directory),
          __FILE__, __LINE__);
    }

    if (initial_balance <= 0) {
      Logger::LogAndThrowError(
          format("지정된 초기 자금 [{}]는 0보다 커야 합니다.",
                 FormatDollar(initial_balance, true)),
          __FILE__, __LINE__);
    }

    if (IsGreater(taker_fee_percentage, 100.0) ||
        IsLess(taker_fee_percentage, 0.0)) {
      Logger::LogAndThrowError(
          format("지정된 테이커 수수료 퍼센트 [{}%]는 100% 초과 혹은 "
                 "0% 미만으로 설정할 수 없습니다.",
                 taker_fee_percentage),
          __FILE__, __LINE__);
    }

    if (IsGreater(maker_fee_percentage, 100.0) ||
        IsLess(maker_fee_percentage, 0.0)) {
      Logger::LogAndThrowError(
          format("지정된 메이커 수수료 퍼센트 [{}%]는 100% 초과 혹은 "
                 "0% 미만으로 설정할 수 없습니다.",
                 maker_fee_percentage),
          __FILE__, __LINE__);
    }

    if (IsGreater(taker_slippage_percentage, 100.0) ||
        IsLess(taker_slippage_percentage, 0.0)) {
      Logger::LogAndThrowError(
          format("지정된 테이커 슬리피지 퍼센트 [{}%]는 100% 초과 혹은 "
                 "0% 미만으로 설정할 수 없습니다.",
                 taker_slippage_percentage),
          __FILE__, __LINE__);
    }

    if (IsGreater(maker_slippage_percentage, 100.0) ||
        IsLess(maker_slippage_percentage, 0.0)) {
      Logger::LogAndThrowError(
          format("지정된 메이커 슬리피지 퍼센트 [{}%]는 100% 초과 혹은 "
                 "0% 미만으로 설정할 수 없습니다.",
                 maker_slippage_percentage),
          __FILE__, __LINE__);
    }

    logger_->Log(INFO_L, "엔진 설정값 유효성 검증이 완료되었습니다.", __FILE__,
                 __LINE__);
  } catch (...) {
    Logger::LogAndThrowError("엔진 설정값 유효성 검증이 실패했습니다.",
                             __FILE__, __LINE__);
  }
}

void Engine::IsValidSymbolInfo() {
  try {
    if (exchange_info_.empty()) {
      Logger::LogAndThrowError(
          "엔진에 거래소 정보가 추가되지 않았습니다. "
          "Engine::AddExchangeInfo 함수를 호출해 주세요.",
          __FILE__, __LINE__);
    }

    if (leverage_bracket_.empty()) {
      Logger::LogAndThrowError(
          "엔진에 레버리지 구간이 추가되지 않았습니다. "
          "Engine::AddLeverageBracket 함수를 호출해 주세요.",
          __FILE__, __LINE__);
    }

    logger_->Log(INFO_L, "심볼 정보 유효성 검증이 완료되었습니다.", __FILE__,
                 __LINE__);
  } catch (...) {
    Logger::LogAndThrowError("심볼 정보 유효성 검증이 실패했습니다.", __FILE__,
                             __LINE__);
  }
}

void Engine::IsValidBarData() {
  try {
    const auto& trading_bar_data = bar_->GetBarData(TRADING);
    const auto trading_num_symbols = trading_bar_data->GetNumSymbols();

    // 1.1. 트레이딩 바 데이터가 비었는지 검증
    if (!trading_num_symbols)
      Logger::LogAndThrowError("트레이딩 바 데이터가 추가되지 않았습니다.",
                               __FILE__, __LINE__);

    // 1.2. 트레이딩 바 데이터의 중복 가능성 검증
    // set은 중복 불가능하므로 set에 추가한 open 개수가 심볼 개수와 다르다면
    // 중복 추가 가능성 높음
    if (config_->GetCheckBarDataDuplication()[0]) {
      set<double> trading_bar_open;
      for (int symbol_idx = 0; symbol_idx < trading_num_symbols; symbol_idx++) {
        trading_bar_open.insert(trading_bar_data->GetBar(symbol_idx, 0).open);
      }

      if (trading_bar_open.size() != trading_num_symbols) {
        logger_->Log(
            ERROR_L,
            "트레이딩 바 데이터에 중복된 데이터가 다른 심볼로 추가되었을 "
            "가능성이 있습니다.",
            __FILE__, __LINE__);
        Logger::LogAndThrowError(
            "중복된 데이터가 없는 것이 확실하다면 "
            "Config::SetConfig().DisableBarDataDuplicationCheck 함수를 "
            "호출해 주세요.",
            __FILE__, __LINE__);
      }
    }

    // =========================================================================
    const auto& magnifier_bar_data = bar_->GetBarData(MAGNIFIER);
    const auto magnifier_num_symbols = magnifier_bar_data->GetNumSymbols();

    if (config_->GetUseBarMagnifier()) {
      /* 2.1. 트레이딩 바 데이터의 심볼 개수와 돋보기 바 데이터의
              심볼 개수가 같은지 검증 */
      if (trading_num_symbols != magnifier_num_symbols) {
        Logger::LogAndThrowError(
            format("돋보기 기능 사용 시 트레이딩 바 데이터에 추가된 "
                   "심볼 개수({}개)와 돋보기 바 데이터에 추가된 심볼 "
                   "개수({}개)는 동일해야 합니다.",
                   trading_num_symbols, magnifier_num_symbols),
            __FILE__, __LINE__);
      }

      /* 2.2. 트레이딩 바 데이터의 심볼들이 돋보기 바 데이터에 존재하고
              순서가 같은지 검증 */
      for (int symbol_idx = 0; symbol_idx < trading_num_symbols; ++symbol_idx) {
        if (const auto& symbol_name =
                trading_bar_data->GetSymbolName(symbol_idx);
            symbol_name != magnifier_bar_data->GetSymbolName(symbol_idx)) {
          Logger::LogAndThrowError(
              format(
                  "돋보기 바 데이터에 [{}]이(가) 존재하지 않거나 "
                  "트레이딩 바 데이터에 추가된 심볼 순서와 일치하지 않습니다.",
                  symbol_name),
              __FILE__, __LINE__);
        }
      }

      // 2.3. 돋보기 바 데이터의 중복 가능성 검증
      // set은 중복 불가능하므로 set에 추가한 open 개수가 심볼 개수와 다르다면
      // 중복 추가 가능성 높음
      if (config_->GetCheckBarDataDuplication()[1]) {
        set<double> magnifier_open;
        for (int symbol_idx = 0; symbol_idx < magnifier_num_symbols;
             symbol_idx++) {
          magnifier_open.insert(magnifier_bar_data->GetBar(symbol_idx, 0).open);
        }

        if (magnifier_open.size() != magnifier_num_symbols) {
          logger_->Log(
              ERROR_L,
              "돋보기 바 데이터에 중복된 데이터가 다른 심볼로 추가되었을 "
              "가능성이 있습니다.",
              __FILE__, __LINE__);
          Logger::LogAndThrowError(
              "중복된 데이터가 없는 것이 확실하다면 "
              "Config::SetConfig().DisableBarDataDuplicationCheck 함수를 "
              "호출해 주세요.",
              __FILE__, __LINE__);
        }
      }
    } else {
      // 2.4. 돋보기 기능을 사용하지 않는데 돋보기 바로 추가되었는지 확인
      if (magnifier_num_symbols != 0) {
        Logger::LogAndThrowError(
            "돋보기 기능을 사용하지 않으면 돋보기 바 데이터를 추가할 수 "
            "없습니다.",
            __FILE__, __LINE__);
      }
    }

    // =========================================================================
    for (auto& [reference_timeframe, reference_bar_data] :
         bar_->GetAllReferenceBarData()) {
      const auto reference_num_symbols = reference_bar_data->GetNumSymbols();

      /* ※ 참조 바 데이터의 심볼 개수와 순서 검증은 추후 트레이딩 바 심볼 외
            다른 데이터(경제 지표 등)의 참조가 필요할 때 삭제 */

      /* 3.1. 트레이딩 바 데이터의 심볼 개수와 참조 바 데이터의
              심볼 개수가 같은지 검증 */
      if (trading_num_symbols != reference_num_symbols) {
        Logger::LogAndThrowError(
            format("트레이딩 바 데이터에 추가된 심볼 개수({}개)와 참조 바 "
                   "데이터 [{}]에 추가된 심볼 개수({}개)는 동일해야 합니다.",
                   trading_num_symbols, reference_timeframe,
                   reference_num_symbols),
            __FILE__, __LINE__);
      }

      /* 3.2. 트레이딩 바 데이터의 심볼들이 참조 바 데이터에 존재하고
              순서가 같은지 검증 */
      for (int symbol_idx = 0; symbol_idx < trading_num_symbols; ++symbol_idx) {
        if (const auto& symbol_name =
                trading_bar_data->GetSymbolName(symbol_idx);
            symbol_name != reference_bar_data->GetSymbolName(symbol_idx)) {
          Logger::LogAndThrowError(
              format(
                  "참조 바 데이터에 [{} {}]이(가) 존재하지 않거나 "
                  "트레이딩 바 데이터에 추가된 심볼 순서와 일치하지 않습니다.",
                  symbol_name, reference_timeframe),
              __FILE__, __LINE__);
        }
      }

      // 3.3. 참조 바 데이터의 중복 가능성 검증
      // set은 중복 불가능하므로 set에 추가한 open 개수가 심볼 개수와 다르다면
      // 중복 추가 가능성 높음
      if (config_->GetCheckBarDataDuplication()[2]) {
        set<double> reference_open;
        for (int symbol_idx = 0; symbol_idx < reference_num_symbols;
             symbol_idx++) {
          reference_open.insert(reference_bar_data->GetBar(symbol_idx, 0).open);
        }

        if (reference_open.size() != reference_num_symbols) {
          logger_->Log(ERROR_L,
                       format("참조 바 데이터 [{}]에 중복된 데이터가 다른 "
                              "심볼로 추가되었을 가능성이 있습니다.",
                              reference_timeframe),
                       __FILE__, __LINE__);
          Logger::LogAndThrowError(
              "중복된 데이터가 없는 것이 확실하다면 "
              "Config::SetConfig().DisableBarDataDuplicationCheck 함수를 "
              "호출해 주세요.",
              __FILE__, __LINE__);
        }
      }
    }

    // =========================================================================
    const auto& mark_price_bar_data = bar_->GetBarData(MARK_PRICE);
    const auto mark_price_num_symbols = mark_price_bar_data->GetNumSymbols();

    // 돋보기 기능 사용 시 마크 가격 바 데이터는 돋보기 바 데이터와 비교
    // 그렇지 않으면 트레이딩 바 데이터와 비교
    const auto use_bar_magnifier = config_->GetUseBarMagnifier();
    const auto& target_bar_data =
        use_bar_magnifier ? magnifier_bar_data : trading_bar_data;
    const auto target_num_symbols = target_bar_data->GetNumSymbols();

    /* 4.1. 타겟 바 데이터의 타임프레임과 마크 가격 바 데이터의
            타임프레임이 같은지 검증 */
    const auto& target_timeframe = target_bar_data->GetTimeframe();
    if (const auto& mark_price_timeframe = mark_price_bar_data->GetTimeframe();
        target_timeframe != mark_price_timeframe) {
      Logger::LogAndThrowError(
          format("{} 바 데이터의 타임프레임 [{}]와(과) 마크 가격 바 데이터의 "
                 "타임프레임 [{}]은(는) 동일해야 합니다.",
                 use_bar_magnifier ? "돋보기 기능 사용 시 돋보기" : "트레이딩",
                 target_timeframe, mark_price_timeframe),
          __FILE__, __LINE__);
    }

    /* 4.2. 타겟 바 데이터의 심볼 개수와 마크 가격 바 데이터의
            심볼 개수가 같은지 검증 */
    if (target_num_symbols != mark_price_num_symbols) {
      Logger::LogAndThrowError(
          format(
              "{} 바 데이터에 추가된 심볼 개수({}개)와 마크 가격 바 데이터에 "
              "추가된 심볼 개수({}개)는 동일해야 합니다.",
              use_bar_magnifier ? "돋보기 기능 사용 시 돋보기" : "트레이딩",
              target_num_symbols, mark_price_num_symbols),
          __FILE__, __LINE__);
    }

    /* 4.3. 타겟 바 데이터의 심볼들이 마크 가격 바 데이터에 존재하고
            순서가 같은지 검증 */
    for (int symbol_idx = 0; symbol_idx < target_num_symbols; ++symbol_idx) {
      if (const auto& symbol_name = target_bar_data->GetSymbolName(symbol_idx);
          symbol_name != mark_price_bar_data->GetSymbolName(symbol_idx)) {
        Logger::LogAndThrowError(
            format("마크 가격 바 데이터에 [{}]이(가) 존재하지 않거나 "
                   "{} 바 데이터에 추가된 심볼 순서와 일치하지 않습니다.",
                   symbol_name, use_bar_magnifier ? "돋보기" : "트레이딩"),
            __FILE__, __LINE__);
      }
    }

    // 4.4. 마크 가격 바 데이터의 중복 가능성 검증
    // set은 중복 불가능하므로 set에 추가한 open 개수가 심볼 개수와 다르다면
    // 중복 추가 가능성 높음
    if (config_->GetCheckBarDataDuplication()[3]) {
      set<double> mark_price_open;
      for (int symbol_idx = 0; symbol_idx < mark_price_num_symbols;
           symbol_idx++) {
        mark_price_open.insert(mark_price_bar_data->GetBar(symbol_idx, 0).open);
      }

      if (mark_price_open.size() != mark_price_num_symbols) {
        logger_->Log(
            ERROR_L,
            "마크 가격 바 데이터에 중복된 데이터가 다른 심볼로 추가되었을 "
            "가능성이 있습니다.",
            __FILE__, __LINE__);
        Logger::LogAndThrowError(
            "중복된 데이터가 없는 것이 확실하다면 "
            "Config::SetConfig().DisableBarDataDuplicationCheck 함수를 "
            "호출해 주세요.",
            __FILE__, __LINE__);
      }
    }

    // 4.5. 마크 가격 바 데이터와 타켓 바 데이터의 중복 가능성 검증
    if (config_->GetCheckTargetBarDataDuplication()) {
      for (int symbol_idx = 0; symbol_idx < mark_price_num_symbols;
           symbol_idx++) {
        const auto target_max_idx = target_bar_data->GetNumBars(symbol_idx) - 1;
        const auto mark_price_max_idx =
            mark_price_bar_data->GetNumBars(symbol_idx) - 1;

        if (target_bar_data->GetBar(symbol_idx, target_max_idx).open ==
            mark_price_bar_data->GetBar(symbol_idx, mark_price_max_idx).open) {
          logger_->Log(
              ERROR_L,
              format(
                  "마크 가격 바 데이터와 {} 바 데이터가 [{}] 심볼에서 동일한 "
                  "데이터일 가능성이 있습니다.",
                  use_bar_magnifier ? "돋보기" : "트레이딩",
                  mark_price_bar_data->GetSymbolName(symbol_idx)),
              __FILE__, __LINE__);
          Logger::LogAndThrowError(
              "중복된 데이터가 없는 것이 확실하다면 "
              "Config::SetConfig().DisableTargetBarDataDuplicationCheck 함수를 "
              "호출해 주세요.",
              __FILE__, __LINE__);
        }
      }
    }

    logger_->Log(INFO_L, "바 데이터 유효성 검증이 완료되었습니다.", __FILE__,
                 __LINE__);
  } catch (...) {
    Logger::LogAndThrowError("바 데이터 유효성 검증이 실패했습니다.", __FILE__,
                             __LINE__);
  }
}

void Engine::IsValidDateRange(const string& start_time, const string& end_time,
                              const string& format) {
  try {
    const auto& trading_bar = bar_->GetBarData(TRADING);
    for (int symbol_idx = 0; symbol_idx < trading_bar->GetNumSymbols();
         symbol_idx++) {
      // 백테스팅 시작 시 가장 처음의 Open Time 값 구하기
      begin_open_time_ =
          min(begin_open_time_, trading_bar->GetBar(symbol_idx, 0).open_time);

      // 백테스팅 시작 시 가장 끝의 Open Time 값 구하기
      end_open_time_ =
          max(end_open_time_,
              trading_bar
                  ->GetBar(symbol_idx, trading_bar->GetNumBars(symbol_idx) - 1)
                  .open_time);
    }

    // Start가 지정된 경우 범위 체크
    if (!start_time.empty()) {
      if (const auto start_time_ts =
              UtcDatetimeToUtcTimestamp(start_time, format);
          start_time_ts < begin_open_time_) {
        Logger::LogAndThrowError(
            std::format("지정된 Start 시간 {}은(는) 최소 시간 {}의 "
                        "전으로 지정할 수 없습니다.",
                        start_time,
                        UtcTimestampToUtcDatetime(begin_open_time_)),
            __FILE__, __LINE__);
      } else {
        begin_open_time_ = start_time_ts;
      }
    }

    // End가 지정된 경우 범위 체크
    if (!end_time.empty()) {
      if (const auto end_time_ts = UtcDatetimeToUtcTimestamp(end_time, format);
          end_time_ts > end_open_time_) {
        Logger::LogAndThrowError(
            std::format("지정된 End 시간 {}은(는) 최대 시간 {}의 "
                        "후로 지정할 수 없습니다.",
                        end_time, UtcTimestampToUtcDatetime(end_open_time_)),
            __FILE__, __LINE__);
      } else {
        end_open_time_ = end_time_ts;
      }
    }

    // Start, End가 둘다 지정된 경우 범위 체크
    if (!start_time.empty() && !end_time.empty()) {
      if (UtcDatetimeToUtcTimestamp(start_time, format) >
          UtcDatetimeToUtcTimestamp(end_time, format)) {
        Logger::LogAndThrowError(
            std::format(
                "지정된 Start 시간 {}은(는) 지정된 End 시간 {}의 전으로 "
                "지정할 수 없습니다.",
                start_time, end_time),
            __FILE__, __LINE__);
      }
    }

    logger_->Log(INFO_L, "시간 범위 유효성 검증이 완료되었습니다.", __FILE__,
                 __LINE__);
  } catch (...) {
    Logger::LogAndThrowError("시간 범위 유효성 검증이 실패했습니다.", __FILE__,
                             __LINE__);
  }
}

void Engine::IsValidStrategies() {
  try {
    // 전략들을 로딩
    for (const auto& strategy : Strategy::GetStrategies()) {
      strategies_.push_back(strategy);
    }

    // 중복 이름 검사
    set<string> names;
    string duplicate_name;
    for (const auto& strategy : strategies_) {
      if (string name = strategy->GetName(); !names.insert(name).second) {
        duplicate_name = name;
        break;
      }
    }

    if (!duplicate_name.empty()) {
      Logger::LogAndThrowError(
          format("전략은 동일한 이름 [{}]을(를) 가질 수 없습니다.",
                 duplicate_name),
          __FILE__, __LINE__);
    }

    // 전략 개수 검사
    if (strategies_.empty()) {
      Logger::LogAndThrowError(
          "엔진에 전략이 추가되지 않았습니다. "
          "Strategy::AddStrategy 함수를 호출해 주세요.",
          __FILE__, __LINE__);
    }

    logger_->Log(INFO_L, "전략 유효성 검증이 완료되었습니다.", __FILE__,
                 __LINE__);
  } catch (...) {
    Logger::LogAndThrowError("전략 유효성 검증이 실패했습니다.", __FILE__,
                             __LINE__);
  }
}

void Engine::IsValidIndicators() {
  try {
    // 지표들을 로딩
    indicators_.resize(strategies_.size());
    for (int strategy_idx = 0; strategy_idx < strategies_.size();
         strategy_idx++) {
      set<string> names;
      string strategy_name;
      string duplicate_name;
      for (const auto& indicator : strategies_[strategy_idx]->GetIndicators()) {
        // 각 전략 내에서 지표들은 같은 이름을 가질 수 없도록 검사
        if (string name = indicator->GetName(); !names.insert(name).second) {
          strategy_name = strategies_[strategy_idx]->GetName();
          duplicate_name = name;
          break;
        }

        // 동일한 이름이 없다면 지표 벡터에 추가
        indicators_[strategy_idx].push_back(indicator);
      }

      if (!duplicate_name.empty()) {
        Logger::LogAndThrowError(format("[{}] 전략 내에서 동일한 이름의 지표 "
                                        "[{}]을(를) 가질 수 없습니다.",
                                        strategy_name, duplicate_name),
                                 __FILE__, __LINE__);
      }
    }

    // 지표 타임프레임 유효성 검사
    for (int strategy_idx = 0; strategy_idx < strategies_.size();
         strategy_idx++) {
      for (const auto& indicator : indicators_[strategy_idx]) {
        const string& timeframe = indicator->GetTimeframe();
        try {
          ParseTimeframe(timeframe);
        } catch ([[maybe_unused]] const std::exception& e) {
          // 지표에서 trading_timeframe을 변수를 사용하면 아직 초기화 전이기
          // 때문에 TRADING_TIMEFRAME으로 지정되어 있는데 이 경우는 유효한
          // 타임프레임이기 때문에 넘어감
          if (timeframe == "TRADING_TIMEFRAME") {
            continue;
          }

          Logger::LogAndThrowError(
              format("[{}] 전략에서 사용하는 [{}] 지표의 타임프레임 "
                     "[{}]이(가) 유효하지 않습니다.",
                     strategies_[strategy_idx]->GetName(), indicator->GetName(),
                     indicator->GetTimeframe()),
              __FILE__, __LINE__);
        }
      }
    }

    logger_->Log(INFO_L, "지표 유효성 검증이 완료되었습니다.", __FILE__,
                 __LINE__);
  } catch (...) {
    Logger::LogAndThrowError("지표 유효성 검증이 실패했습니다.", __FILE__,
                             __LINE__);
  }
}

void Engine::InitializeEngine() {
  // 자금 설정
  const auto initial_balance = config_->GetInitialBalance();
  wallet_balance_ = initial_balance;
  available_balance_ = initial_balance;
  max_wallet_balance_ = initial_balance;

  // 돋보기 기능 사용 여부 결정
  use_bar_magnifier_ = config_->GetUseBarMagnifier();

  // 바 데이터 초기화
  trading_bar_data_ = bar_->GetBarData(TRADING);
  if (use_bar_magnifier_) {
    magnifier_bar_data_ = bar_->GetBarData(MAGNIFIER);
  }
  reference_bar_data_ = bar_->GetAllReferenceBarData();
  mark_price_bar_data_ = bar_->GetBarData(MARK_PRICE);

  // 바 데이터 정보 초기화
  trading_bar_num_symbols_ = trading_bar_data_->GetNumSymbols();
  trading_bar_timeframe_ = trading_bar_data_->GetTimeframe();
  trading_bar_time_diff_ = ParseTimeframe(trading_bar_data_->GetTimeframe());
  if (use_bar_magnifier_) {
    magnifier_bar_time_diff_ =
        ParseTimeframe(magnifier_bar_data_->GetTimeframe());
  }

  // 심볼 정보 크기 초기화
  symbol_info_.resize(trading_bar_num_symbols_);

  // 트레이딩 시간 정보 초기화
  current_open_time_ = begin_open_time_;
  current_close_time_ = begin_open_time_ + trading_bar_time_diff_ - 1;

  // 시작 시간까지 트레이딩 바 인덱스 및 마크 가격 바 인덱스를 이동
  bar_->ProcessBarIndices(TRADING, "NONE", current_close_time_);
  if (!use_bar_magnifier_) {
    bar_->ProcessBarIndices(MARK_PRICE, "NONE", current_close_time_);
  }

  // trading_began_, trading_ended 초기화
  trading_began_.resize(trading_bar_num_symbols_);
  trading_ended_.resize(trading_bar_num_symbols_);

  for (int symbol_idx = 0; symbol_idx < trading_bar_num_symbols_;
       symbol_idx++) {
    bar_->SetCurrentSymbolIndex(symbol_idx);

    // 첫 시작 시간이 begin_open_time과 같다면 바로 시작하는 Symbol
    if (trading_bar_data_->GetBar(symbol_idx, bar_->GetCurrentBarIndex())
            .open_time == begin_open_time_) {
      trading_began_[symbol_idx] = true;
    } else {
      trading_began_[symbol_idx] = false;
    }

    trading_ended_[symbol_idx] = false;
  }

  // 활성화된 심볼들 초기화
  activated_symbol_indices_.resize(trading_bar_num_symbols_);
  if (use_bar_magnifier_) {
    activated_magnifier_symbol_indices_.resize(trading_bar_num_symbols_);
  }
  activated_trading_symbol_indices_.resize(trading_bar_num_symbols_);

  // 분석기 초기화
  analyzer_->Initialize(initial_balance);

  engine_initialized_ = true;
  logger_->Log(INFO_L, "엔진 초기화가 완료되었습니다.", __FILE__, __LINE__);
}

void Engine::InitializeSymbolInfo() {
  // 모든 심볼 순회
  for (int symbol_idx = 0; symbol_idx < trading_bar_num_symbols_;
       symbol_idx++) {
    const string& symbol_name = trading_bar_data_->GetSymbolName(symbol_idx);
    SymbolInfo symbol_info;

    // 거래소 정보 순회
    try {
      bool symbol_found = false;

      // 거래소 정보의 symbols 배열 내 모든 심볼들을 순회
      for (const auto& symbol : exchange_info_.at("symbols")) {
        if (symbol.at("symbol") == symbol_name &&
            symbol.at("contractType") == "PERPETUAL") {
          // 해당 심볼이 존재한다면 필요한 정보들로 초기화
          int filter_count = 0;  // filter 배열에서 초기화한 횟수
          for (const auto& filters = symbol.at("filters");
               const auto& filter : filters) {
            if (filter.at("filterType") == "PRICE_FILTER") {
              symbol_info.SetTickSize(GetDoubleFromJson(filter, "tickSize"));
              filter_count += 1;
              continue;
            }

            if (filter.at("filterType") == "LOT_SIZE") {
              symbol_info.SetLimitMaxQty(GetDoubleFromJson(filter, "maxQty"))
                  .SetLimitMinQty(GetDoubleFromJson(filter, "minQty"));
              filter_count += 2;
              continue;
            }

            if (filter.at("filterType") == "MARKET_LOT_SIZE") {
              symbol_info.SetMarketMaxQty(GetDoubleFromJson(filter, "maxQty"))
                  .SetMarketMinQty(GetDoubleFromJson(filter, "minQty"))
                  .SetQtyStep(GetDoubleFromJson(filter, "stepSize"));
              filter_count += 3;
              continue;
            }

            if (filter.at("filterType") == "MIN_NOTIONAL") {
              symbol_info.SetMinNotional(GetDoubleFromJson(filter, "notional"));
              filter_count += 1;
              continue;
            }
          }

          if (filter_count != 7) {
            Logger::LogAndThrowError(
                "filters의 심볼 정보 중 일부가 존재하지 않습니다.", __FILE__,
                __LINE__);
          }

          symbol_info.SetLiquidationFee(
              GetDoubleFromJson(symbol, "liquidationFee"));

          symbol_found = true;
          break;
        }
      }

      if (!symbol_found) {
        throw invalid_argument(
            format("거래소 정보에 [symbol: {}] && [contractType: PERPETUAL]인 "
                   "객체가 존재하지 않습니다.",
                   symbol_name));
      }
    } catch (const std::exception& e) {
      logger_->Log(ERROR_L, e.what(), __FILE__, __LINE__);
      Logger::LogAndThrowError(
          format(
              "[{}] 심볼에서 거래소 정보를 초기화하는 중 오류가 발생했습니다.",
              symbol_name),
          __FILE__, __LINE__);
    }

    // 레버리지 구간 순회
    try {
      bool symbol_found = false;

      // 레버리지 구간의 symbols 배열 내 모든 심볼들을 순회
      for (const auto& symbol : leverage_bracket_) {
        if (symbol.at("symbol") == symbol_name) {
          vector<LeverageBracket> leverage_brackets;

          // 해당 심볼이 존재한다면 구간을 순회하며 추가
          for (const auto& bracket : symbol.at("brackets")) {
            LeverageBracket leverage_bracket{};
            leverage_bracket.min_notional_value =
                bracket.at("notionalFloor").get<double>();
            leverage_bracket.max_notional_value =
                bracket.at("notionalCap").get<double>();
            leverage_bracket.max_leverage =
                bracket.at("initialLeverage").get<int>();
            leverage_bracket.maintenance_margin_rate =
                bracket.at("maintMarginRatio").get<double>();
            leverage_bracket.maintenance_amount =
                bracket.at("cum").get<double>();

            leverage_brackets.push_back(leverage_bracket);
          }

          symbol_info.SetLeverageBracket(leverage_brackets);

          symbol_found = true;
          break;
        }
      }

      if (!symbol_found) {
        throw invalid_argument(
            format("레버리지 구간에 [symbol: {}]인 객체가 존재하지 않습니다.",
                   symbol_name));
      }
    } catch (const std::exception& e) {
      logger_->Log(ERROR_L, e.what(), __FILE__, __LINE__);
      Logger::LogAndThrowError(
          format("[{}] 레버리지 구간을 초기화하는 중 오류가 발생했습니다.",
                 symbol_name),
          __FILE__, __LINE__);
    }

    symbol_info_[symbol_idx] = symbol_info;
  }

  BaseOrderHandler::SetSymbolInfo(symbol_info_);
  logger_->Log(INFO_L, "심볼 정보 초기화가 완료되었습니다.", __FILE__,
               __LINE__);
}

void Engine::InitializeStrategies() const {
  // 전략별 주문 핸들러 및 전략 초기화
  for (const auto& strategy : strategies_) {
    strategy->GetOrderHandler()->Initialize(trading_bar_num_symbols_);
    strategy->Initialize();
  }

  Strategy::SetTradingTimeframe(trading_bar_timeframe_);

  logger_->Log(INFO_L, "전략 초기화가 완료되었습니다.", __FILE__, __LINE__);
}

void Engine::InitializeIndicators() const {
  // 전략에서 trading_timeframe을 사용하여 타임프레임이 공란이면
  // 트레이딩 바의 타임프레임을 사용
  for (int strategy_idx = 0; strategy_idx < strategies_.size();
       strategy_idx++) {
    const string& strategy_name = strategies_[strategy_idx]->GetName();

    for (const auto& indicator : indicators_[strategy_idx]) {
      if (indicator->GetTimeframe() == "TRADING_TIMEFRAME") {
        indicator->SetTimeframe(trading_bar_timeframe_);
      }

      // 지표 계산
      indicator->CalculateIndicator(strategy_name);
    }
  }

  logger_->Log(INFO_L, "지표 초기화가 완료되었습니다.", __FILE__, __LINE__);
}

void Engine::BacktestingMain() {
  while (true) {
    // 현재 바 인덱스에서 트레이딩을 진행하는지 상태를 업데이트
    UpdateTradingStatus();

    // 트레이딩이 모두 끝났으면 백테스팅 끝
    if (ranges::all_of(trading_ended_,
                       [](const bool is_end) { return is_end; })) {
      return;
    }

    logger_->Log(
        INFO_L,
        format("진행 시간: {}", UtcTimestampToUtcDatetime(current_open_time_)),
        __FILE__, __LINE__);

    // 돋보기 바에 활성화된 심볼이 있으면 돋보기 바를 이용하여 체결 확인
    if (!activated_magnifier_symbol_indices_.empty()) {
      const auto original_open_time = current_open_time_;
      const auto original_close_time = current_close_time_;
      bar_->SetCurrentBarType(MAGNIFIER, "NONE");

      // UpdateTradingStatus에서 일치시킨 시간은 전 트레이딩 바의 Close Time
      current_open_time_ = original_open_time - magnifier_bar_time_diff_;
      current_close_time_ = original_open_time - 1;

      while (true) {
        // 현재 Open Time과 Close Time을 돋보기 바 하나만큼 증가
        // Open Time은 돋보기 바로 진행 시 정확한 주문, 체결 시간을 얻기 위함
        // Close Time은 마크 가격 바 인덱스를 일치시키기 위함
        current_open_time_ += magnifier_bar_time_diff_;
        current_close_time_ += magnifier_bar_time_diff_;

        for (const auto& symbol_idx : activated_magnifier_symbol_indices_) {
          // 활성화된 돋보기 바 인덱스 증가. UpdateTradingStatus에서
          // 트레이딩 전 바의 Close Time으로 일치시키므로 하나 증가하고 시작
          bar_->IncreaseBarIndex(MAGNIFIER, "NONE", symbol_idx);

          // 마크 가격 바 인덱스를 현재 돋보기 바 Close Time으로 일치
          bar_->ProcessBarIndex(MARK_PRICE, "NONE", symbol_idx,
                                current_close_time_);
        }

        // 해당 돋보기 바를 진행
        ProcessOhlc(MAGNIFIER, activated_magnifier_symbol_indices_);

        // 돋보기 바의 Close Time이 트레이딩 바의 Close Time과 같아지면 종료
        if (current_close_time_ == original_close_time) {
          break;
        }
      }

      // 원상 복구
      current_open_time_ = original_open_time;
      current_close_time_ = original_close_time;
    }

    // 트레이딩 바에 활성화된 심볼이 있으면 트레이딩 바를 이용하여 체결 확인
    // 1. 돋보기 기능 자체를 미사용
    // 2. 돋보기 기능을 사용하지만 데이터 누락으로 사용 불가능
    if (!activated_trading_symbol_indices_.empty()) {
      bar_->SetCurrentBarType(TRADING, "NONE");

      // 돋보기 기능을 사용하면 마크 가격 바 데이터의 타임프레임은 돋보기 바와
      // 같기 때문에 기능을 사용하지 않을 때만 일치.
      // 돋보기 기능을 사용하지만 데이터 누락으로 사용 불가능할 때는
      // GetPriceQueue 함수 내에서 시장 가격을 사용하도록 조치되어 있음
      if (!use_bar_magnifier_) {
        for (const auto& symbol_idx : activated_trading_symbol_indices_) {
          // 마크 가격 바 인덱스를 현재 트레이딩 바 Close Time으로 일치
          bar_->ProcessBarIndex(MARK_PRICE, "NONE", symbol_idx,
                                current_close_time_);
        }
      }

      ProcessOhlc(TRADING, activated_trading_symbol_indices_);
    }

    // =======================================================================
    // 활성화된 심볼들의 트레이딩 바에서 전략 실행
    // 순서: 한 심볼 내에서 모든 전략 실행 후 다음 심볼로 이동
    // ProcessOhlc 함수 내에서의 순서와 일치
    for (const auto symbol_idx : activated_symbol_indices_) {
      for (const auto& strategy : strategies_) {
        const auto& order_handler = strategy->GetOrderHandler();

        ExecuteStrategy(strategy, ON_CLOSE, symbol_idx);

        // On Close 전략 실행 후 진입/청산이 있었을 경우
        // After Entry, After Exit 전략 실행
        // After Entry, After Exit 전략 실행 후 추가 진입 혹은 추가 청산
        // 가능성이 있으므로 추가 진입 및 청산이 없을 때까지 전략 실행
        bool just_entered = order_handler->GetJustEntered();
        bool just_exited = order_handler->GetJustExited();
        do {
          if (just_entered) {
            order_handler->InitializeJustEntered();
            ExecuteStrategy(strategy, AFTER_ENTRY, symbol_idx);
          }

          if (just_exited) {
            order_handler->InitializeJustExited();
            ExecuteStrategy(strategy, AFTER_EXIT, symbol_idx);
          }

          // After Entry, After Exit 전략 실행 시 추가 진입 혹은 추가 청산
          // 가능성이 있으므로 상태를 다시 업데이트

          // 진입 및 청산 체결이 없는 경우 체결 확인 및 전략 실행 종료
        } while (((just_entered = order_handler->GetJustEntered())) ||
                 ((just_exited = order_handler->GetJustExited())));
      }
    }

    // =======================================================================
    // 활성화된 심볼들의 트레이딩 바 인덱스 증가
    for (const auto symbol_idx : activated_symbol_indices_) {
      bar_->IncreaseBarIndex(TRADING, "NONE", symbol_idx);
    }

    // current_open_time_ -> UpdateTradingStatus에서 트레이딩 시작 검증 시 사용
    // current_close_time_
    //     -> UpdateTradingStatus에서 현재 트레이딩 바 Close Time까지 바
    //        돋보기를 사용할 수 있는지 검증하기 위하여 사용
    current_open_time_ += trading_bar_time_diff_;
    current_close_time_ += trading_bar_time_diff_;
  }
}

void Engine::UpdateTradingStatus() {
  // 활성화된 벡터 초기화
  activated_symbol_indices_.clear();
  activated_trading_symbol_indices_.clear();
  activated_magnifier_symbol_indices_.clear();

  // 트레이딩 바 전체 심볼 순회
  for (int symbol_idx = 0; symbol_idx < trading_bar_num_symbols_;
       symbol_idx++) {
    // 사용 중인 바 정보 업데이트
    bar_->SetCurrentBarType(TRADING, "NONE");
    bar_->SetCurrentSymbolIndex(symbol_idx);

    const auto bar_idx = bar_->GetCurrentBarIndex();

    if (trading_began_[symbol_idx]) {
      // 트레이딩을 시작했지만 끝난 심볼은 업데이트할 것이 없음
      if (trading_ended_[symbol_idx]) {
        continue;
      }

      // 트레이딩을 시작했지만 끝나지 않은 심볼은 이번 바에서 끝났는지 검사
      try {
        trading_bar_data_->IsValidIndex(symbol_idx, bar_idx);
      } catch ([[maybe_unused]] const IndexOutOfRange& e) {
        // 해당 심볼의 데이터의 끝까지 진행했다면 해당 심볼은 트레이딩 종료
        ExecuteTradingEnd(symbol_idx);
        continue;
      }

      // 트레이딩 바 데이터 결손이 있으면 트레이딩 불가
      if (current_open_time_ !=
          trading_bar_data_->GetBar(symbol_idx, bar_idx).open_time) {
        continue;
      }

      // 진행할 바가 남아있고 트레이딩 바 데이터 결손이 없으면 트레이딩 진행
      DetermineActivation(symbol_idx);
    } else {
      // 트레이딩을 시작하지 않은 심볼은 이번 바에서 시작했는지 검사
      if (const auto current_open_time =
              trading_bar_data_->GetBar(symbol_idx, bar_idx).open_time;
          current_open_time == current_open_time_) {
        trading_began_[symbol_idx] = true;

        DetermineActivation(symbol_idx);
      }
    }
  }
}

void Engine::ExecuteTradingEnd(const int symbol_idx) {
  trading_ended_[symbol_idx] = true;

  // 진입 및 청산 대기 주문을 취소하고 체결된 진입 주문 잔량을 종가에 청산
  // 메인 루프 전 트레이딩 바 인덱스를 하나 증가시켰으므로 하나 감소시켜야
  // 마지막 바를 가리킴
  bar_->SetCurrentBarType(TRADING, "NONE");
  bar_->SetCurrentBarIndex(bar_->GetCurrentBarIndex() - 1);

  for (const auto& strategy : strategies_) {
    current_strategy_name_ = strategy->GetName();

    const auto& order_handler = strategy->GetOrderHandler();
    order_handler->CancelAll();
    order_handler->CloseAll();

    // 바가 끝난 전량 청산은 Just Exited로 판단하지 않음
    order_handler->InitializeJustExited();
  }

  logger_->Log(ORDER_L,
               format("[{}] 심볼의 트레이딩 바 데이터가 끝나 해당 심볼의 "
                      "백테스팅을 종료합니다.",
                      trading_bar_data_->GetSymbolName(symbol_idx)),
               __FILE__, __LINE__);
}

void Engine::DetermineActivation(const int symbol_idx) {
  // 트레이딩 전 바의 Close Time
  // 타임스탬프이므로 current_open_time - 1 == previous_close_time
  const auto prev_close_time = current_open_time_ - 1;

  /* 참조 바를 사용 가능한지 검증
     1. 트레이딩 바의 타임프레임과 참조 바의 타임프레임이 같으면 바 인덱스를
        무조건 일치
     2. 참조 바의 타임프레임이 더 크다면 트레이딩 바의 Close Time이
        참조 바의 Close Time을 지난 다음 바부터
        해당 참조 바 인덱스를 참조 가능
     2.1. 트레이딩 전 바 Close Time보다 참조 바의 Close Time이 작으면
          트레이딩 전 바 Close Time까지 최대한 증가 후 사용 가능.
     2.2. 트레이딩 전 바 Close Time과 참조 바의 Close Time이 같으면
          사용 가능
     2.3. 트레이딩 전 바 Close Time보다 참조 바의 Close Time이 같거나 크면
          아직 사용 불가능하므로 트레이딩 불가                         */
  for (const auto& [timeframe, reference_bar] : reference_bar_data_) {
    bar_->SetCurrentBarType(REFERENCE, timeframe);

    if (timeframe == trading_bar_timeframe_) {
      bar_->ProcessBarIndex(REFERENCE, timeframe, symbol_idx,
                            current_close_time_);
    } else {
      if (reference_bar->GetBar(symbol_idx, bar_->GetCurrentBarIndex())
              .close_time <= prev_close_time) {
        bar_->ProcessBarIndex(REFERENCE, timeframe, symbol_idx,
                              prev_close_time);
      } else {
        // 트레이딩 전 바 Close Time보다 참조 바의 Close Time이 크면
        // 참조 불가능하므로 트레이딩 불가
        //
        // 하지만 트레이딩 바는 인덱스를 맞추어 흘러가야
        // Open Time, Close Time이 동기화 되므로 인덱스 증가
        // * 원래 트레이딩 바 인덱스는 활성화된 심볼에서만 증가함
        bar_->IncreaseBarIndex(TRADING, "NONE", symbol_idx);
        return;
      }
    }
  }

  if (use_bar_magnifier_) {
    bar_->SetCurrentBarType(MAGNIFIER, "NONE");

    int64_t magnifier_close_time =
        magnifier_bar_data_->GetBar(symbol_idx, bar_->GetCurrentBarIndex())
            .close_time;

    /* 돋보기 바를 사용 가능한지 검증
       1. 트레이딩 전 바 Close Time보다 돋보기 바의 Close Time이 작으면
          트레이딩 전 바 Close Time으로 일치시킨 후 사용 가능
       2. 트레이딩 전 바 Close Time과 돋보기 바의 Close Time이 같으면
          사용 가능
       3. 트레이딩 전 바 Close Time보다 돋보기 바의 Close Time이 크면
          아직 사용 불가능하므로 트레이딩 바를 사용하여 진행             */
    if (magnifier_close_time <= prev_close_time) {
      bool can_use_magnifier = true;

      if (magnifier_close_time < prev_close_time) {
        if (!bar_->ProcessBarIndex(MAGNIFIER, "NONE", symbol_idx,
                                   prev_close_time)) {
          // 돋보기 바의 최대 인덱스에 도달하여 트레이딩 전 바 Close
          // Time까지 이동시키지 못하면 돋보기 사용 불가
          can_use_magnifier = false;
        }
      }

      if (can_use_magnifier) {
        // 트레이딩 전 바 Close Time과 같은 Close Time을 갖는 돋보기 바 인덱스
        const auto updated_bar_idx = bar_->GetCurrentBarIndex();

        // 트레이딩 바의 Close Time까지 돋보기 바의 인덱스가 유효한지 확인
        try {
          size_t added_index = 1;

          do {
            magnifier_close_time =
                magnifier_bar_data_
                    ->SafeGetBar(symbol_idx, updated_bar_idx + added_index++)
                    .close_time;
          } while (magnifier_close_time < current_close_time_);
        } catch ([[maybe_unused]] const IndexOutOfRange& e) {
          // 돋보기 바 데이터가 현재 트레이딩 바의 Close Time까지 유효하지
          // 않으면 돋보기 사용 불가
          can_use_magnifier = false;
        }
      }

      if (can_use_magnifier) {
        activated_symbol_indices_.push_back(symbol_idx);
        activated_magnifier_symbol_indices_.push_back(symbol_idx);
        return;
      }
    }
  }

  // 돋보기 바를 사용하지 않거나 사용할 수 없으면
  // 트레이딩 바를 사용하여 진행
  activated_symbol_indices_.push_back(symbol_idx);
  activated_trading_symbol_indices_.push_back(symbol_idx);
}

void Engine::ProcessOhlc(const BarType bar_type,
                         const vector<int>& symbol_indices) {
  // 매커니즘에 따라 확인할 순서대로 가격을 정렬한 벡터 얻기
  const auto [mark_price_queue, market_price_queue] =
      move(GetPriceQueue(bar_type, symbol_indices));

  // 순서: 한 가격 내에서 모든 전략 실행 후 다음 심볼 및 가격으로 넘어감
  // 한 전략에서 모든 가격 체크 후 다음 가격 체크하면 논리상 시간을 한 번
  // 거슬러 올라가는 것이므로 옳지 않음
  for (int queue_idx = 0; queue_idx < mark_price_queue.size(); queue_idx++) {
    const auto [mark_price, mark_price_type, mark_price_symbol_idx] =
        mark_price_queue[queue_idx];

    const auto [market_price, market_price_type, market_price_symbol_idx] =
        market_price_queue[queue_idx];

    // 마크 가격과 시장 가격에서 하나의 큐 인덱스의 심볼 인덱스는 동일
    bar_->SetCurrentSymbolIndex(mark_price_symbol_idx);

    for (const auto& strategy : strategies_) {
      // 로깅 및 진입과 청산 시 전략 이름을 설정해야하므로 미리 설정
      current_strategy_name_ = strategy->GetName();

      // 각 전략의 OrderHandler를 순회하며 강제 청산과 대기 주문의 체결을 확인
      const auto& order_handler = strategy->GetOrderHandler();

      // 정해진 순서대로 강제 청산을 확인
      // 강제 청산의 경우 After Exit 전략이 실행됨
      order_handler->CheckLiquidation(mark_price, mark_price_type,
                                      mark_price_symbol_idx, bar_type);

      // 정해진 순서대로 진입 및 청산 대기 주문의 체결을 확인
      do {
        // Check가 루프에 포함되는 이유는,
        // After Entry, After Exit 전략에서의 주문도 체결될 수도 있기 때문

        // 진입 대기 주문의 체결 확인 후 체결이 존재할 시 After Entry 전략 실행
        order_handler->CheckPendingEntries(market_price, market_price_type,
                                           market_price_symbol_idx);

        if (order_handler->GetJustEntered()) {
          order_handler->InitializeJustEntered();
          ExecuteStrategy(strategy, AFTER_ENTRY, market_price_symbol_idx);
        }

        // 청산 대기 주문의 체결 확인 후 체결이 존재할 시 After Exit 전략 실행
        order_handler->CheckPendingExits(market_price, market_price_type,
                                         market_price_symbol_idx);

        if (order_handler->GetJustExited()) {
          order_handler->InitializeJustExited();
          ExecuteStrategy(strategy, AFTER_EXIT, market_price_symbol_idx);
        }

        // 진입 및 청산 체결이 없는 경우 체결 확인 및 전략 실행 종료
      } while (order_handler->GetJustEntered() ||
               order_handler->GetJustExited());
    }
  }
}

pair<vector<PriceData>, vector<PriceData>> Engine::GetPriceQueue(
    const BarType market_bar_type, const vector<int>& symbol_indices) {
  // 마지막 저장은 open -> high_low1 -> high_low2 -> close 순서
  vector<PriceData> mark_open_queue;
  vector<PriceData> mark_high_low_queue1;
  vector<PriceData> mark_high_low_queue2;
  vector<PriceData> mark_close_queue;

  vector<PriceData> market_open_queue;
  vector<PriceData> market_high_low_queue1;
  vector<PriceData> market_high_low_queue2;
  vector<PriceData> market_close_queue;

  // 미리 메모리 할당
  const auto num_activated_symbols = symbol_indices.size();
  mark_open_queue.reserve(num_activated_symbols);
  mark_high_low_queue1.reserve(num_activated_symbols);
  mark_high_low_queue2.reserve(num_activated_symbols);
  mark_close_queue.reserve(num_activated_symbols);

  market_open_queue.reserve(num_activated_symbols);
  market_high_low_queue1.reserve(num_activated_symbols);
  market_high_low_queue2.reserve(num_activated_symbols);
  market_close_queue.reserve(num_activated_symbols);

  PriceData mark_price_data{};
  PriceData market_price_data{};
  const auto mark_bar_data = bar_->GetBarData(MARK_PRICE, "NONE");
  const auto market_bar_data = bar_->GetBarData(market_bar_type, "NONE");

  // 활성화된 심볼 순회
  for (const int symbol_idx : symbol_indices) {
    // 해당 심볼의 가격 데이터 로딩
    bar_->SetCurrentSymbolIndex(symbol_idx);

    bar_->SetCurrentBarType(MARK_PRICE, "NONE");
    auto mark_bar =
        mark_bar_data->GetBar(symbol_idx, bar_->GetCurrentBarIndex());

    bar_->SetCurrentBarType(market_bar_type, "NONE");
    const auto& market_bar =
        market_bar_data->GetBar(symbol_idx, bar_->GetCurrentBarIndex());

    // Mark Price의 경우, 데이터 누락으로 Market Open Time과 일치하지 않으면
    // 시장 가격 사용
    if (mark_bar.open_time != market_bar.open_time) {
      mark_bar = market_bar;
    }

    const double mark_open = mark_bar.open;
    const double mark_high = mark_bar.high;
    const double mark_low = mark_bar.low;

    const double market_open = market_bar.open;
    const double market_high = market_bar.high;
    const double market_low = market_bar.low;

    // 구조체 공통 필드 설정
    mark_price_data.symbol_index = symbol_idx;
    market_price_data.symbol_index = symbol_idx;

    // 시가 데이터 추가
    mark_price_data.price = mark_open;
    mark_price_data.price_type = OPEN;
    mark_open_queue.push_back(mark_price_data);

    market_price_data.price = market_open;
    market_price_data.price_type = OPEN;
    market_open_queue.push_back(market_price_data);

    // 고저가 데이터 추가
    // 마크 가격 기준, 시가 대비 고가의 폭이 저가의 폭보다 크다면
    // 시가 -> 저가 -> 고가 -> 종가로 움직임 가정.
    // 마크 가격 기준, 시가 대비 저가의 폭이 고가의 폭보다 크다면
    // 시가 -> 고가 -> 저가 -> 종가로 움직임 가정.
    if (IsGreaterOrEqual(mark_high - mark_open, mark_open - mark_low)) {
      mark_price_data.price = mark_low;
      mark_price_data.price_type = LOW;
      mark_high_low_queue1.push_back(mark_price_data);

      mark_price_data.price = mark_high;
      mark_price_data.price_type = HIGH;
      mark_high_low_queue2.push_back(mark_price_data);
    } else {
      mark_price_data.price = mark_high;
      mark_price_data.price_type = HIGH;
      mark_high_low_queue1.push_back(mark_price_data);

      mark_price_data.price = mark_low;
      mark_price_data.price_type = LOW;
      mark_high_low_queue2.push_back(mark_price_data);
    }

    // 시장 가격 기준, 시가 대비 고가의 폭이 저가의 폭보다 크다면
    // 시가 -> 저가 -> 고가 -> 종가로 움직임 가정.
    // 시장 가격 기준, 시가 대비 저가의 폭이 고가의 폭보다 크다면
    // 시가 -> 고가 -> 저가 -> 종가로 움직임 가정.
    if (IsGreaterOrEqual(market_high - market_open, market_open - market_low)) {
      market_price_data.price = market_low;
      market_price_data.price_type = LOW;
      market_high_low_queue1.push_back(market_price_data);

      market_price_data.price = market_high;
      market_price_data.price_type = HIGH;
      market_high_low_queue2.push_back(market_price_data);
    } else {
      market_price_data.price = market_high;
      market_price_data.price_type = HIGH;
      market_high_low_queue1.push_back(market_price_data);

      market_price_data.price = mark_low;
      market_price_data.price_type = LOW;
      market_high_low_queue2.push_back(market_price_data);
    }

    // 종가 데이터 추가
    mark_price_data.price = mark_bar.close;
    mark_price_data.price_type = CLOSE;
    mark_close_queue.push_back(mark_price_data);

    market_price_data.price = market_bar.close;
    market_price_data.price_type = CLOSE;
    market_close_queue.push_back(market_price_data);
  }

  vector<PriceData> mark_queue;
  vector<PriceData> market_queue;
  // 총 크기는 심볼 개수 * OHLC(4개)
  mark_queue.reserve(num_activated_symbols * 4);
  market_queue.reserve(num_activated_symbols * 4);

  // 모든 벡터의 데이터를 큐에 이동
  mark_queue.insert(mark_queue.end(),
                    make_move_iterator(mark_open_queue.begin()),
                    make_move_iterator(mark_open_queue.end()));
  mark_queue.insert(mark_queue.end(),
                    make_move_iterator(mark_high_low_queue1.begin()),
                    make_move_iterator(mark_high_low_queue1.end()));
  mark_queue.insert(mark_queue.end(),
                    make_move_iterator(mark_high_low_queue2.begin()),
                    make_move_iterator(mark_high_low_queue2.end()));
  mark_queue.insert(mark_queue.end(),
                    make_move_iterator(mark_close_queue.begin()),
                    make_move_iterator(mark_close_queue.end()));

  market_queue.insert(market_queue.end(),
                      make_move_iterator(market_open_queue.begin()),
                      make_move_iterator(market_open_queue.end()));
  market_queue.insert(market_queue.end(),
                      make_move_iterator(market_high_low_queue1.begin()),
                      make_move_iterator(market_high_low_queue1.end()));
  market_queue.insert(market_queue.end(),
                      make_move_iterator(market_high_low_queue2.begin()),
                      make_move_iterator(market_high_low_queue2.end()));
  market_queue.insert(market_queue.end(),
                      make_move_iterator(market_close_queue.begin()),
                      make_move_iterator(market_close_queue.end()));

  return {mark_queue, market_queue};
}

void Engine::ExecuteStrategy(const shared_ptr<Strategy>& strategy,
                             const StrategyType strategy_type,
                             const int symbol_index) {
  // 원본 설정을 저장
  const auto original_bar_type = bar_->GetCurrentBarType();

  // 트레이딩 바의 지정된 심볼에서 전략 실행
  bar_->SetCurrentBarType(TRADING, "NONE");
  bar_->SetCurrentSymbolIndex(symbol_index);

  // 진입 및 청산 시 전략 이름을 설정해야하므로 미리 설정
  current_strategy_name_ = strategy->GetName();

  // 현재 심볼의 포지션 사이즈 업데이트
  strategy->GetOrderHandler()->UpdateCurrentPositionSize();

  try {
    current_strategy_type_ = strategy_type;

    if (strategy_type == ON_CLOSE) {
      strategy->ExecuteOnClose();
    } else if (strategy_type == AFTER_ENTRY) {
      strategy->ExecuteAfterEntry();
    } else if (strategy_type == AFTER_EXIT) {
      strategy->ExecuteAfterExit();
    }
  } catch ([[maybe_unused]] const Bankruptcy& e) {
    SetBankruptcy();
    throw;
  }

  // 원본 설정을 복원
  bar_->SetCurrentBarType(original_bar_type, "NONE");
}

}  // namespace backtesting::engine