// 표준 라이브러리
#include <array>
#include <cmath>
#include <filesystem>
#include <format>
#include <ranges>
#include <set>
#include <utility>

// 외부 라이브러리
#include "arrow/stl_iterator.h"
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
      trading_indices_(),
      magnifier_indices_(),
      mark_price_indices_(),
      current_strategy_type_(ON_CLOSE),
      begin_open_time_(INT64_MAX),
      end_close_time_(0),
      current_open_time_(0),
      current_close_time_(0),
      next_month_boundary_(0),
      all_trading_ended_(false) {}

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

void Engine::Backtesting() {
  const auto& start = chrono::high_resolution_clock::now();

  LogSeparator(true);
  Initialize();

  LogSeparator(true);
  logger_->Log(INFO_L, std::format("백테스팅을 시작합니다."), __FILE__,
               __LINE__, true);

  try {
    BacktestingMain();
  } catch ([[maybe_unused]] const Bankruptcy& e) {
    logger_->Log(ERROR_L, "파산으로 인해 백테스팅을 종료합니다.", __FILE__,
                 __LINE__, true);
  }

  LogSeparator(true);
  logger_->Log(INFO_L, "백테스팅 결과 저장을 시작합니다.", __FILE__, __LINE__,
               true);

  // 이번 백테스팅 결과 저장 폴더 생성
  analyzer_->CreateDirectories();

  // 지표 데이터 저장
  analyzer_->SaveIndicatorData();

  // 거래 내역 저장
  analyzer_->SaveTradeList();

  // 백테스팅 설정 저장
  analyzer_->SaveConfig();

  // 전략 및 지표의 코드 저장
  analyzer_->SaveSourcesAndHeaders();

  // 백보드 저장
  analyzer_->SaveBackboard();

  LogSeparator(true);
  logger_->Log(INFO_L, "백테스팅이 완료되었습니다.", __FILE__, __LINE__, true);

  const auto& end = chrono::high_resolution_clock::now();
  logger_->Log(
      INFO_L,
      "소요 시간: " +
          FormatTimeDiff(
              duration_cast<chrono::milliseconds>(end - start).count()),
      __FILE__, __LINE__, true);

  // 백테스팅 로그 저장
  // 소요 시간까지 확실히 로그로 저장시키기 위해 마지막에 저장하며,
  // 어색함 방지를 위해 저장 완료 로그를 발생시키지 않음
  analyzer_->SaveBacktestingLog();

  // Backboard.exe 자동 실행
  if (const string backboard_exe_path =
          analyzer_->GetMainDirectory() + "/Backboard.exe";
      filesystem::exists(backboard_exe_path)) {
    system(format(R"(start "" "{}")", backboard_exe_path).c_str());
  }
}

void Engine::SetCurrentStrategyType(const StrategyType strategy_type) {
  current_strategy_type_ = strategy_type;
}

StrategyType Engine::GetCurrentStrategyType() const {
  return current_strategy_type_;
}

int64_t Engine::GetCurrentOpenTime() const { return current_open_time_; }

int64_t Engine::GetCurrentCloseTime() const { return current_close_time_; }

bool Engine::IsAllTradingEnded() const { return all_trading_ended_; }

void Engine::Initialize() {
  // 유효성 검증
  IsValidConfig();
  IsValidBarData();
  IsValidDateRange();
  IsValidSymbolInfo();
  IsValidStrategy();
  IsValidIndicators();

  // 초기화
  LogSeparator(true);
  InitializeEngine();
  InitializeSymbolInfo();
  InitializeStrategy();
  InitializeIndicators();
}

void Engine::IsValidConfig() {
  try {
    if (config_ == nullptr) {
      Logger::LogAndThrowError(
          "엔진에 설정값이 추가되지 않았습니다. "
          "Backtesting::SetConfig 함수를 호출해 주세요.",
          __FILE__, __LINE__);
    }

    const auto& root_directory = Config::GetRootDirectory();
    const auto& backtesting_period = config_->GetBacktestingPeriod();
    const auto& use_bar_magnifier = config_->GetUseBarMagnifier();
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
          "Backtesting::SetConfig().SetRootDirectory 함수를 호출해 주세요.",
          __FILE__, __LINE__);
    }

    if (!filesystem::exists(root_directory)) {
      Logger::LogAndThrowError(
          format("루트 폴더 [{}]이(가) 존재하지 않습니다.", root_directory),
          __FILE__, __LINE__);
    }

    if (!backtesting_period.has_value()) {
      Logger::LogAndThrowError(
          "백테스팅 기간이 설정되지 않았습니다. "
          "Backtesting::SetConfig().SetBacktestingPeriod 함수를 호출해 주세요.",
          __FILE__, __LINE__);
    }

    if (!use_bar_magnifier.has_value()) {
      Logger::LogAndThrowError(
          "바 돋보기 사용 여부가 초기화되지 않았습니다. "
          "Backtesting::SetConfig().SetUseBarMagnifier 함수를 호출해 주세요.",
          __FILE__, __LINE__);
    }

    if (isnan(initial_balance)) {
      Logger::LogAndThrowError(
          "초기 자금이 초기화되지 않았습니다. "
          "Backtesting::SetConfig().SetInitialBalance 함수를 호출해 주세요.",
          __FILE__, __LINE__);
    }

    if (isnan(taker_fee_percentage)) {
      Logger::LogAndThrowError(
          "테이커 수수료 퍼센트가 초기화되지 않았습니다. "
          "Backtesting::SetConfig().SetTakerFeePercentage 함수를 호출해 "
          "주세요.",
          __FILE__, __LINE__);
    }

    if (isnan(maker_fee_percentage)) {
      Logger::LogAndThrowError(
          "메이커 수수료 퍼센트가 초기화되지 않았습니다. "
          "Backtesting::SetConfig().SetMakerFeePercentage 함수를 호출해 "
          "주세요.",
          __FILE__, __LINE__);
    }

    if (isnan(taker_slippage_percentage)) {
      Logger::LogAndThrowError(
          "테이커 슬리피지 퍼센트가 초기화되지 않았습니다. "
          "Backtesting::SetConfig().SetTakerSlippagePercentage 함수를 호출해 "
          "주세요.",
          __FILE__, __LINE__);
    }

    if (isnan(maker_slippage_percentage)) {
      Logger::LogAndThrowError(
          "메이커 슬리피지 퍼센트가 초기화되지 않았습니다. "
          "Backtesting::SetConfig().SetMakerSlippagePercentage 함수를 호출해 "
          "주세요.",
          __FILE__, __LINE__);
    }

    if (!filesystem::exists(root_directory)) {
      Logger::LogAndThrowError(
          format("지정된 루트 폴더 [{}]은(는) 유효하지 않습니다.",
                 root_directory),
          __FILE__, __LINE__);
    }

    if (IsLessOrEqual(initial_balance, 0.0)) {
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
                 __LINE__, true);
  } catch (...) {
    Logger::LogAndThrowError("엔진 설정값 유효성 검증이 실패했습니다.",
                             __FILE__, __LINE__);
  }
}

void Engine::IsValidBarData() {
  try {
    const auto trading_bar_data = bar_->GetBarData(TRADING);
    const auto trading_num_symbols = trading_bar_data->GetNumSymbols();

    // 1.1. 트레이딩 바 데이터가 비었는지 검증
    if (!trading_num_symbols)
      Logger::LogAndThrowError(
          "트레이딩 바 데이터가 추가되지 않았습니다. "
          "Backtesting::AddBarData 함수를 호출해 주세요.",
          __FILE__, __LINE__);

    // 1.2. 트레이딩 바 데이터의 중복 가능성 검증
    // set은 중복 불가능하므로 set에 추가한 open 개수가 심볼 개수와 다르다면
    // 중복 추가 가능성 높음
    if (config_->GetCheckSameBarData()[0]) {
      set<double> trading_bar_open;
      for (int symbol_idx = 0; symbol_idx < trading_num_symbols; symbol_idx++) {
        trading_bar_open.insert(trading_bar_data->GetBar(symbol_idx, 0).open);
      }

      if (trading_bar_open.size() != trading_num_symbols) {
        logger_->Log(
            ERROR_L,
            "트레이딩 바 데이터에 중복된 데이터가 다른 심볼로 추가되었을 "
            "가능성이 있습니다.",
            __FILE__, __LINE__, true);
        Logger::LogAndThrowError(
            "이 검사를 비활성화하고 싶다면 "
            "Backtesting::SetConfig().DisableSameBarDataCheck 함수를 "
            "호출해 주세요.",
            __FILE__, __LINE__);
      }
    }

    // =========================================================================
    const auto magnifier_bar_data = bar_->GetBarData(MAGNIFIER);
    const auto magnifier_num_symbols = magnifier_bar_data->GetNumSymbols();

    if (config_->GetUseBarMagnifier().value()) {
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
      if (config_->GetCheckSameBarData()[1]) {
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
              __FILE__, __LINE__, true);
          Logger::LogAndThrowError(
              "이 검사를 비활성화하고 싶다면 "
              "Backtesting::SetConfig().DisableSameBarDataCheck 함수를 "
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
    for (const auto& [reference_timeframe, reference_bar_data] :
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
      if (config_->GetCheckSameBarData()[2]) {
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
                       __FILE__, __LINE__, true);
          Logger::LogAndThrowError(
              "이 검사를 비활성화하고 싶다면 "
              "Backtesting::SetConfig().DisableSameBarDataCheck 함수를 "
              "호출해 주세요.",
              __FILE__, __LINE__);
        }
      }
    }

    // =========================================================================
    const auto mark_price_bar_data = bar_->GetBarData(MARK_PRICE);
    const auto mark_price_num_symbols = mark_price_bar_data->GetNumSymbols();

    // 돋보기 기능 사용 시 마크 가격 바 데이터는 돋보기 바 데이터와 비교
    // 그렇지 않으면 트레이딩 바 데이터와 비교
    const auto use_bar_magnifier = config_->GetUseBarMagnifier().value();
    const auto target_bar_data =
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
    if (config_->GetCheckSameBarData()[3]) {
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
            __FILE__, __LINE__, true);
        Logger::LogAndThrowError(
            "이 검사를 비활성화하고 싶다면 "
            "Backtesting::SetConfig().DisableSameBarDataCheck 함수를 "
            "호출해 주세요.",
            __FILE__, __LINE__);
      }
    }

    // 4.5. 마크 가격 바 데이터와 타켓 바 데이터의 중복 가능성 검증
    if (config_->GetCheckSameBarDataWithTarget()) {
      // 모든 마크 가격 바 데이터 순회
      for (int mark_price_symbol_idx = 0;
           mark_price_symbol_idx < mark_price_num_symbols;
           mark_price_symbol_idx++) {
        const auto mark_max_idx =
            mark_price_bar_data->GetNumBars(mark_price_symbol_idx) - 1;

        // 한 마크 가격 바 데이터에 대해 모든 타켓 바 데이터를 순회
        for (int target_symbol_idx = 0; target_symbol_idx < target_num_symbols;
             target_symbol_idx++) {
          const auto target_max_idx =
              target_bar_data->GetNumBars(target_symbol_idx) - 1;

          // 마지막 바의 모든 가격이 같으면 중복된 데이터일 가능성 존재
          const auto& target_bar =
              target_bar_data->GetBar(target_symbol_idx, target_max_idx);
          const auto& mark_bar =
              mark_price_bar_data->GetBar(mark_price_symbol_idx, mark_max_idx);

          if (IsEqual(target_bar.open, mark_bar.open) &&
              IsEqual(target_bar.high, mark_bar.high) &&
              IsEqual(target_bar.low, mark_bar.low) &&
              IsEqual(target_bar.close, mark_bar.close)) {
            logger_->Log(
                ERROR_L,
                format(
                    "마크 가격 바 데이터의 심볼 [{}]와(과) {} 바 데이터의 심볼 "
                    "[{}]이(가) 중복된 데이터일 가능성이 있습니다.",
                    mark_price_bar_data->GetSymbolName(mark_price_symbol_idx),
                    use_bar_magnifier ? "돋보기" : "트레이딩",
                    target_bar_data->GetSymbolName(target_symbol_idx)),
                __FILE__, __LINE__, true);
            Logger::LogAndThrowError(
                "이 검사를 비활성화하고 싶다면 "
                "Backtesting::SetConfig().DisableSameBarDataWithTargetCheck "
                "함수를 호출해 주세요.",
                __FILE__, __LINE__);
          }
        }
      }
    }

    // 5. 펀딩비 사용 가능 여부 확인
    // 펀딩비 최소 정산 시간 단위가 1h이므로,
    // 돋보기 기능 미사용 시 트레이딩 타임프레임이 1h 이하여야 하며,
    // 돋보기 기능 사용 시 돋보기 타임프레임이 1h 이하여야 함
    const auto parsed_trading_tf =
        ParseTimeframe(trading_bar_data->GetTimeframe());

    if (const auto parsed_1_h = ParseTimeframe("1h");
        (!use_bar_magnifier && parsed_trading_tf > parsed_1_h) ||
        (use_bar_magnifier &&
         ParseTimeframe(magnifier_bar_data->GetTimeframe()) > parsed_1_h)) {
      Logger::LogAndThrowError(
          "펀딩비의 정확한 정산을 위해 돋보기 기능 미사용 시 트레이딩 바 "
          "데이터의 타임프레임이 1h 이하여야 하며, 돋보기 기능 사용 시 돋보기 "
          "바 데이터의 타임프레임이 1h 이하여야 합니다.",
          __FILE__, __LINE__);
    }

    logger_->Log(INFO_L, "바 데이터 유효성 검증이 완료되었습니다.", __FILE__,
                 __LINE__, true);
  } catch (...) {
    Logger::LogAndThrowError("바 데이터 유효성 검증이 실패했습니다.", __FILE__,
                             __LINE__);
  }
}

void Engine::IsValidDateRange() {
  try {
    const auto& trading_bar = bar_->GetBarData(TRADING);
    for (int symbol_idx = 0; symbol_idx < trading_bar->GetNumSymbols();
         symbol_idx++) {
      // 바 데이터 중 가장 처음의 Open Time 값 구하기
      begin_open_time_ =
          min(begin_open_time_, trading_bar->GetBar(symbol_idx, 0).open_time);

      // 바 데이터 중 가장 끝의 Close Time 값 구하기
      end_close_time_ =
          max(end_close_time_,
              trading_bar
                  ->GetBar(symbol_idx, trading_bar->GetNumBars(symbol_idx) - 1)
                  .close_time);
    }

    // 백테스팅 기간 받아오기
    const auto backtesting_period = config_->GetBacktestingPeriod().value();
    const auto& start_time = backtesting_period.GetStartTime();
    const auto& end_time = backtesting_period.GetEndTime();
    const auto& format = backtesting_period.GetFormat();

    // Start가 지정된 경우 범위 체크
    if (!start_time.empty()) {
      if (const auto start_time_ts =
              UtcDatetimeToUtcTimestamp(start_time, format);
          start_time_ts < begin_open_time_) {
        Logger::LogAndThrowError(
            std::format("지정된 Start 시간 [{}]은(는) 최소 시간 [{}]의 "
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
          end_time_ts > end_close_time_) {
        Logger::LogAndThrowError(
            std::format("지정된 End 시간 [{}]은(는) 최대 시간 [{}]의 "
                        "후로 지정할 수 없습니다.",
                        end_time, UtcTimestampToUtcDatetime(end_close_time_)),
            __FILE__, __LINE__);
      } else {
        end_close_time_ = end_time_ts;
      }
    }

    // Start, End가 둘 다 지정된 경우 범위 체크
    if (!start_time.empty() && !end_time.empty()) {
      if (UtcDatetimeToUtcTimestamp(start_time, format) >
          UtcDatetimeToUtcTimestamp(end_time, format)) {
        Logger::LogAndThrowError(
            std::format(
                "지정된 시작 시간 [{}]은(는) 지정된 종료 시간 [{}]의 후로 "
                "지정할 수 없습니다.",
                start_time, end_time),
            __FILE__, __LINE__);
      }
    }

    logger_->Log(INFO_L, "시간 범위 유효성 검증이 완료되었습니다.", __FILE__,
                 __LINE__, true);
  } catch (...) {
    Logger::LogAndThrowError("시간 범위 유효성 검증이 실패했습니다.", __FILE__,
                             __LINE__);
  }
}

void Engine::IsValidSymbolInfo() {
  const auto trading_bar_data = bar_->GetBarData(TRADING);
  const auto trading_num_symbols = trading_bar_data->GetNumSymbols();

  try {
    if (funding_rates_.empty()) {
      Logger::LogAndThrowError(
          "엔진에 펀딩 비율이 추가되지 않았습니다. "
          "Backtesting::AddFundingRates 함수를 호출해 주세요.",
          __FILE__, __LINE__);
    }

    if (const auto funding_rates_num_symbols = funding_rates_.size();
        funding_rates_num_symbols != trading_num_symbols) {
      Logger::LogAndThrowError(
          format("트레이딩 바 데이터에 추가된 심볼 개수({}개)와 펀딩 비율에 "
                 "추가된 심볼 개수({}개)는 동일해야 합니다.",
                 trading_num_symbols, funding_rates_num_symbols),
          __FILE__, __LINE__);
    }

    for (int symbol_idx = 0; symbol_idx < trading_num_symbols; ++symbol_idx) {
      if (const auto& symbol_name = trading_bar_data->GetSymbolName(symbol_idx);
          symbol_name !=
          funding_rates_[symbol_idx][0]["symbol"].get<string>()) {
        Logger::LogAndThrowError(
            format("펀딩 비율에 [{}]이(가) 존재하지 않거나 "
                   "트레이딩 바 데이터에 추가된 심볼 순서와 일치하지 않습니다.",
                   symbol_name),
            __FILE__, __LINE__);
      }
    }

    if (exchange_info_.empty()) {
      Logger::LogAndThrowError(
          "엔진에 거래소 정보가 추가되지 않았습니다. "
          "Backtesting::AddExchangeInfo 함수를 호출해 주세요.",
          __FILE__, __LINE__);
    }

    if (leverage_bracket_.empty()) {
      Logger::LogAndThrowError(
          "엔진에 레버리지 구간이 추가되지 않았습니다. "
          "Backtesting::AddLeverageBracket 함수를 호출해 주세요.",
          __FILE__, __LINE__);
    }

    logger_->Log(INFO_L, "심볼 정보 유효성 검증이 완료되었습니다.", __FILE__,
                 __LINE__, true);
  } catch (...) {
    Logger::LogAndThrowError("심볼 정보 유효성 검증이 실패했습니다.", __FILE__,
                             __LINE__);
  }
}

void Engine::IsValidStrategy() {
  try {
    // 전략을 로딩
    if (strategy_ == nullptr) {
      if (const auto& strategy = Strategy::GetStrategy(); strategy != nullptr) {
        strategy_ = strategy;
      } else {
        Logger::LogAndThrowError(
            "엔진에 전략이 추가되지 않았습니다. "
            "Backtesting::AddStrategy 함수를 호출해 주세요.",
            __FILE__, __LINE__);
      }
    } else {
      Logger::LogAndThrowError(
          "한 백테스팅은 한 개의 전략만 사용할 수 있습니다.", __FILE__,
          __LINE__);
    }

    logger_->Log(INFO_L, "전략 유효성 검증이 완료되었습니다.", __FILE__,
                 __LINE__, true);
  } catch (...) {
    Logger::LogAndThrowError("전략 유효성 검증이 실패했습니다.", __FILE__,
                             __LINE__);
  }
}

void Engine::IsValidIndicators() {
  try {
    const auto& strategy_name = strategy_->GetName();

    // 지표들을 로딩
    set<string> names;
    string duplicate_name;
    for (const auto& indicator : strategy_->GetIndicators()) {
      // 각 전략 내에서 지표들은 같은 이름을 가질 수 없도록 검사
      // 로그 및 성과 분석 시 같은 이름은 분석이 힘들어지므로 원칙적 금지
      if (string name = indicator->GetName(); !names.insert(name).second) {
        duplicate_name = name;
        break;
      }

      // 동일한 이름이 없다면 지표 벡터에 추가
      indicators_.push_back(indicator);
    }

    if (!duplicate_name.empty()) {
      Logger::LogAndThrowError(format("[{}] 전략 내에서 동일한 이름의 지표 "
                                      "[{}]을(를) 가질 수 없습니다.",
                                      strategy_name, duplicate_name),
                               __FILE__, __LINE__);
    }

    for (const auto& indicator : indicators_) {
      const auto& indicator_name = indicator->GetName();

      // 지표 타임프레임 유효성 검사
      const string& timeframe = indicator->GetTimeframe();
      try {
        ParseTimeframe(timeframe);
      } catch ([[maybe_unused]] const std::exception& e) {
        // 지표에서 trading_timeframe을 변수를 사용하면 아직 초기화 전이기
        // 때문에 TRADING_TIMEFRAME으로 지정되어 있는데 이 경우는 유효한
        // 타임프레임이기 때문에 넘어감
        if (timeframe != "TRADING_TIMEFRAME") {
          Logger::LogAndThrowError(
              format("[{}] 전략에서 사용하는 [{}] 지표의 타임프레임 "
                     "[{}]이(가) 유효하지 않습니다.",
                     strategy_name, indicator_name, timeframe),
              __FILE__, __LINE__);
        }
      }

      // 지표 플롯 타입 검사
      if (const auto& plot_type = indicator->plot_type_;
          plot_type != "Area" && plot_type != "Baseline" &&
          plot_type != "Histogram" && plot_type != "Line" &&
          plot_type != "Null") {
        Logger::LogAndThrowError(
            format("[{}] 전략에서 사용하는 [{}] 지표의 플롯 타입 "
                   "[{}]이(가) 유효하지 않습니다. "
                   "(가능한 타입: Area, Baseline, Histogram, Line, Null)",
                   strategy_name, indicator_name, plot_type),
            __FILE__, __LINE__);
      }
    }

    logger_->Log(INFO_L, "지표 유효성 검증이 완료되었습니다.", __FILE__,
                 __LINE__, true);
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
  use_bar_magnifier_ = config_->GetUseBarMagnifier().value();

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
  for (const auto& timeframe : reference_bar_data_ | views::keys) {
    reference_bar_time_diff_[timeframe] = ParseTimeframe(timeframe);
  }

  // 바 인덱스 참조
  trading_indices_ = &bar_->GetBarIndices(TRADING);
  magnifier_indices_ = &bar_->GetBarIndices(MAGNIFIER);
  mark_price_indices_ = &bar_->GetBarIndices(MARK_PRICE);

  // 심볼 정보 크기 초기화
  symbol_info_.resize(trading_bar_num_symbols_);

  // 트레이딩 시간 정보 초기화
  current_open_time_ = begin_open_time_;
  current_close_time_ = begin_open_time_ + trading_bar_time_diff_ - 1;

  // 월 경계 초기화 (백테스팅 시작 시간 기준)
  next_month_boundary_ = CalculateNextMonthBoundary(current_open_time_);

  // 시작 시간까지 트레이딩 바 인덱스 및 마크 가격 바 인덱스를 이동
  bar_->ProcessBarIndices(TRADING, "", current_close_time_);
  if (!use_bar_magnifier_) {
    bar_->ProcessBarIndices(MARK_PRICE, "", current_close_time_);
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

  // 분석기 초기화
  analyzer_->Initialize(begin_open_time_, end_close_time_, initial_balance);

  engine_initialized_ = true;
  logger_->Log(INFO_L, "엔진 초기화가 완료되었습니다.", __FILE__, __LINE__,
               true);
}

void Engine::InitializeSymbolInfo() {
  // 모든 심볼 순회
  for (int symbol_idx = 0; symbol_idx < trading_bar_num_symbols_;
       symbol_idx++) {
    SymbolInfo symbol_info;
    const string& symbol_name = trading_bar_data_->GetSymbolName(symbol_idx);

    // 거래소 정보 초기화
    try {
      bool symbol_found = false;

      // 데이터 경로 설정
      symbol_info.SetExchangeInfoPath(exchange_info_path_);

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
              symbol_info.SetMinNotionalValue(
                  GetDoubleFromJson(filter, "notional"));
              filter_count += 1;
              continue;
            }
          }

          if (filter_count != 7) {
            Logger::LogAndThrowError(
                "filters의 심볼 정보 중 일부가 존재하지 않습니다.", __FILE__,
                __LINE__);
          }

          symbol_info.SetLiquidationFeeRate(
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
      logger_->Log(ERROR_L, e.what(), __FILE__, __LINE__, true);
      Logger::LogAndThrowError(
          format(
              "[{}] 심볼에서 거래소 정보를 초기화하는 중 오류가 발생했습니다.",
              symbol_name),
          __FILE__, __LINE__);
    }

    // 레버리지 구간 초기화
    try {
      bool symbol_found = false;

      // 데이터 경로 설정
      symbol_info.SetLeverageBracketPath(leverage_bracket_path_);

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
      logger_->Log(ERROR_L, e.what(), __FILE__, __LINE__, true);
      Logger::LogAndThrowError(
          format("[{}] 레버리지 구간을 초기화하는 중 오류가 발생했습니다.",
                 symbol_name),
          __FILE__, __LINE__);
    }

    // 펀딩 비율 인덱스 및 캐시 초기화
    funding_rates_indices_.resize(trading_bar_num_symbols_, 0);
    next_funding_rates_.resize(trading_bar_num_symbols_);
    next_funding_times_.resize(trading_bar_num_symbols_);
    next_funding_mark_prices_.resize(trading_bar_num_symbols_);

    // 펀딩 비율 초기화
    try {
      // 데이터 경로 설정
      symbol_info.SetFundingRatesPath(funding_rates_paths_[symbol_idx]);

      const auto& funding_rates = funding_rates_[symbol_idx];
      const auto funding_rates_size = funding_rates.size();
      vector<FundingInfo> funding_rates_vector(funding_rates_size);

      // 펀딩 비율 JSON을 순회하며 필요한 정보만 벡터에 추가
      for (int idx = 0; idx < funding_rates_size; idx++) {
        const auto& funding_rate = funding_rates[idx];
        const auto& mark_price = funding_rate.at("markPrice").get<string>();

        // 마크 가격이 빈 문자열로도 존재하기 때문에 조건 분기
        funding_rates_vector[idx] = {
            stod(funding_rate.at("fundingRate").get<string>()),
            funding_rate.at("fundingTime").get<int64_t>(),
            mark_price.empty() ? NAN : stod(mark_price)};
      }

      // 첫 펀딩 비율 및 시간 캐시
      bool funding_rate_exist = false;

      for (size_t idx = 0; idx < funding_rates_vector.size(); ++idx) {
        // 백테스팅 기간 중의 첫 데이터 포인트를 찾으면 캐시 후 루프 종료
        if (const auto& [funding_rate, funding_time, mark_price] =
                funding_rates_vector[idx];
            funding_time >= begin_open_time_ &&
            funding_time <= end_close_time_) {
          funding_rates_indices_[symbol_idx] = idx;
          next_funding_rates_[symbol_idx] = funding_rate;
          next_funding_times_[symbol_idx] = funding_time;
          next_funding_mark_prices_[symbol_idx] = mark_price;

          funding_rate_exist = true;
          break;
        }
      }

      if (!funding_rate_exist) {
        Logger::LogAndThrowError(
            format("백테스팅 기간 [{} - {}]에 해당되는 [{}] 펀딩 비율 데이터가 "
                   "존재하지 않습니다.",
                   UtcTimestampToUtcDatetime(begin_open_time_),
                   UtcTimestampToUtcDatetime(end_close_time_),
                   trading_bar_data_->GetSymbolName(symbol_idx)),
            __FILE__, __LINE__);
      }

      symbol_info.SetFundingRates(funding_rates_vector);
    } catch (const std::exception& e) {
      logger_->Log(ERROR_L, e.what(), __FILE__, __LINE__, true);
      Logger::LogAndThrowError(
          format("[{}] 심볼에서 펀딩 비율을 초기화하는 중 오류가 발생했습니다.",
                 symbol_name),
          __FILE__, __LINE__);
    }

    symbol_info_[symbol_idx] = symbol_info;
  }

  Analyzer::SetSymbolInfo(symbol_info_);
  BaseOrderHandler::SetSymbolInfo(symbol_info_);

  logger_->Log(INFO_L, "심볼 정보 초기화가 완료되었습니다.", __FILE__, __LINE__,
               true);
}

void Engine::InitializeStrategy() {
  // 주문 핸들러 및 전략 초기화
  order_handler_ = strategy_->GetOrderHandler();
  order_handler_->Initialize(trading_bar_num_symbols_);
  strategy_->Initialize();

  Strategy::SetTradingTimeframe(trading_bar_timeframe_);

  logger_->Log(INFO_L, "전략 초기화가 완료되었습니다.", __FILE__, __LINE__,
               true);
}

void Engine::InitializeIndicators() const {
  // 전략에서 trading_timeframe을 사용하여 타임프레임이 공란이면
  // 트레이딩 바의 타임프레임을 사용
  for (const auto& indicator : indicators_) {
    if (indicator->GetTimeframe() == "TRADING_TIMEFRAME") {
      indicator->SetTimeframe(trading_bar_timeframe_);
    }

    if (ParseTimeframe(indicator->GetTimeframe()) >
        ParseTimeframe(trading_bar_timeframe_)) {
      indicator->SetHigherTimeframeIndicator();
    }

    // 지표 계산
    indicator->CalculateIndicator();
  }

  logger_->Log(INFO_L, "지표 초기화가 완료되었습니다.", __FILE__, __LINE__,
               true);
}

void Engine::BacktestingMain() {
  while (true) {
    // =========================================================================
    // [진행 시간 로그]
    // =========================================================================
    LogSeparator(false);

    // 월이 바뀌는지 체크
    bool log_to_console = false;
    if (current_open_time_ >= next_month_boundary_) {
      log_to_console = true;
      next_month_boundary_ = CalculateNextMonthBoundary(current_open_time_);
    }

    // 월이 바뀔 때만 콘솔에 로그 출력
    logger_->Log(
        INFO_L,
        format("진행 시간: {}", UtcTimestampToUtcDatetime(current_open_time_)),
        __FILE__, __LINE__, log_to_console);

    // =========================================================================
    // [심볼별 트레이딩 진행 여부 및 진행 방법 결정]
    // =========================================================================
    // 현재 바 인덱스에서 트레이딩을 진행하는지 상태를 업데이트
    UpdateTradingStatus();

    // 전체 트레이딩 종료 확인
    // end_close_time이 마지막 트레이딩 바 Close Time과 같으면
    // UpdateTradingStatus 함수 이후 자연스럽게 백테스팅이 종료되므로
    // UpdateTradingStatus 함수 후에 전체 트레이딩 종료를 확인
    if (current_close_time_ > end_close_time_) {
      ExecuteAllTradingEnd();
    }

    // 트레이딩이 모두 끝났으면 백테스팅 종료
    if (ranges::all_of(trading_ended_,
                       [](const bool is_end) { return is_end; })) {
      if (!all_trading_ended_) {
        all_trading_ended_ = true;
      }

      return;
    }

    // =========================================================================
    // [펀딩비 정산 및 OHLC를 진행하며 대기 주문 체결 확인]
    // =========================================================================
    // 돋보기 바 기능 사용 시 돋보기 바 시간 진행
    if (use_bar_magnifier_) {
      const auto original_open_time = current_open_time_;
      const auto original_close_time = current_close_time_;
      bar_->SetCurrentBarType(MAGNIFIER, "");

      // 루프 시작하자마자 시간을 증가시키므로 전 돋보기 바로 시간을 설정
      current_open_time_ = original_open_time - magnifier_bar_time_diff_;
      current_close_time_ = original_open_time - 1;

      vector<int> activated_magnifier_symbol_indices;
      do {
        // 현재 Open Time과 Close Time을 돋보기 바 하나만큼 증가
        // Open Time은 돋보기 바로 진행 시 정확한 주문, 체결 시간을 얻기 위함
        // Close Time은 마크 가격 바 인덱스를 일치시키기 위함
        current_open_time_ += magnifier_bar_time_diff_;
        current_close_time_ += magnifier_bar_time_diff_;

        for (const auto& symbol_idx : activated_symbol_indices_) {
          bar_->SetCurrentSymbolIndex(symbol_idx);
          bar_->ProcessBarIndex(MAGNIFIER, "", symbol_idx, current_close_time_);
          const auto moved_bar_idx = bar_->GetCurrentBarIndex();
          const auto moved_close_time =
              magnifier_bar_data_->GetBar(symbol_idx, moved_bar_idx).close_time;

          if (moved_close_time == current_close_time_) {
            // 정상적으로 Close Time이 일치된 바만 체결 확인
            activated_magnifier_symbol_indices.push_back(symbol_idx);
          } else {
            string magnifier_next_open_time;

            // 다음 바의 Open Time 찾기
            if (moved_bar_idx + 1 !=
                magnifier_bar_data_->GetNumBars(symbol_idx)) {
              // 현재 바가 마지막 바가 아닌 경우 직접 Open Time 가져오기
              magnifier_next_open_time = UtcTimestampToUtcDatetime(
                  magnifier_bar_data_->GetBar(symbol_idx, moved_bar_idx + 1)
                      .open_time);
            } else {
              // 마지막 바인 경우는 종료 명시
              magnifier_next_open_time = "데이터 종료";
            }

            logger_->Log(
                WARNING_L,
                format("[{}] 심볼의 [{}] 돋보기 바가 누락되어 체결 확인을 "
                       "건너뜁니다. (돋보기 바 다음 시간: [{}])",
                       trading_bar_data_->GetSymbolName(symbol_idx),
                       UtcTimestampToUtcDatetime(current_open_time_),
                       magnifier_next_open_time),
                __FILE__, __LINE__, false);
          }

          // 마크 가격 바 인덱스를 현재 돋보기 바 Close Time으로 일치.
          // 펀딩비 데이터에 마크 가격이 누락되었을 경우 시장 마크 가격을
          // 가져와야 하기 때문에 모든 트레이딩 활성화 심볼에 대해 일치
          bar_->ProcessBarIndex(MARK_PRICE, "", symbol_idx,
                                current_close_time_);
        }

        // 돋보기 기능 사용 시 돋보기 바 진행 도중 펀딩비 정산
        CheckFundingTime();

        // 해당 돋보기 바를 진행
        ProcessOhlc(MAGNIFIER, activated_magnifier_symbol_indices);

        // 다음 시간 진행을 위해 활성화 된 심볼 초기화
        activated_magnifier_symbol_indices.clear();

        // 돋보기 바의 Close Time이 트레이딩 바의 Close Time과 같아지면
        // 돋보기 바 시간 진행 종료
      } while (current_close_time_ != original_close_time);

      // 돋보기 바 진행이 끝났다면 시간을 원상 복구
      current_open_time_ = original_open_time;
      current_close_time_ = original_close_time;
    } else {
      // 돋보기 기능 미사용 시 트레이딩 바를 이용하여 체결 확인
      bar_->SetCurrentBarType(TRADING, "");

      for (const auto& symbol_idx : activated_symbol_indices_) {
        // 마크 가격 바 인덱스를 현재 트레이딩 바 Close Time으로 일치
        bar_->ProcessBarIndex(MARK_PRICE, "", symbol_idx, current_close_time_);
      }

      // 돋보기 기능 미사용 시 트레이딩 바 진행 도중 펀딩비 정산
      CheckFundingTime();

      // 해당 트레이딩 바를 진행
      ProcessOhlc(TRADING, activated_symbol_indices_);
    }

    // =========================================================================
    // [종가에서 전략 실행]
    // =========================================================================
    // 활성화된 심볼들의 트레이딩 바에서 전략 실행
    for (const auto symbol_idx : activated_symbol_indices_) {
      ExecuteStrategy(strategy_, ON_CLOSE, symbol_idx);

      // On Close 전략 실행 후 진입/청산이 있었을 경우
      // After Entry, After Exit 전략 실행
      // After Entry, After Exit 전략 실행 후 추가 진입 혹은 추가 청산
      // 가능성이 있으므로 추가 진입 및 청산이 없을 때까지 전략 실행
      bool just_entered = order_handler_->IsJustEntered();
      bool just_exited = order_handler_->IsJustExited();

      do {
        if (just_entered) {
          order_handler_->InitializeJustEntered();
          ExecuteStrategy(strategy_, AFTER_ENTRY, symbol_idx);
        }

        if (just_exited) {
          order_handler_->InitializeJustExited();
          ExecuteStrategy(strategy_, AFTER_EXIT, symbol_idx);
        }

        // After Entry, After Exit 전략 실행 시 추가 진입 혹은 추가 청산
        // 가능성이 있으므로 상태를 다시 업데이트

        // 진입 및 청산 체결이 없는 경우 체결 확인 및 전략 실행 종료
      } while (((just_entered = order_handler_->IsJustEntered())) ||
               ((just_exited = order_handler_->IsJustExited())));
    }

    // =========================================================================
    // [인덱스 및 시간 증가]
    // =========================================================================
    // 활성화된 심볼들의 트레이딩 바 인덱스 증가
    for (const auto symbol_idx : activated_symbol_indices_) {
      bar_->IncreaseBarIndex(TRADING, "", symbol_idx);
    }

    // current_open_time_ -> UpdateTradingStatus에서 트레이딩 시작 검증 시 사용
    // current_close_time_
    // -> 1. UpdateTradingStatus에서 현재 트레이딩 바 Close Time까지
    //       바 돋보기를 사용할 수 있는지 검증하기 위하여 사용
    //    2. 백테스팅 종료 시간 확인을 위해 사용
    current_open_time_ += trading_bar_time_diff_;
    current_close_time_ += trading_bar_time_diff_;
  }
}

void Engine::UpdateTradingStatus() {
  // 활성화된 심볼 벡터 초기화
  activated_symbol_indices_.clear();

  for (int symbol_idx = 0; symbol_idx < trading_bar_num_symbols_;
       symbol_idx++) {
    bar_->SetCurrentSymbolIndex(symbol_idx);

    // =========================================================================
    // 트레이딩 바가 사용 가능한지 검증
    // =========================================================================
    // 사용 중인 바 정보 업데이트
    bar_->SetCurrentBarType(TRADING, "");
    const auto bar_idx = bar_->GetCurrentBarIndex();

    if (trading_began_[symbol_idx]) {
      // 트레이딩을 시작했지만 끝난 심볼은 트레이딩 불가
      if (trading_ended_[symbol_idx]) {
        continue;
      }

      // 트레이딩을 시작했지만 끝나지 않은 심볼은 이번 바에서 끝났는지 검사
      // (전 바(마지막 바)까지 진행하고 현재 바(다음 바) Ohlc 시작 전에 종료)
      if (bar_idx == trading_bar_data_->GetNumBars(symbol_idx)) {
        // 해당 심볼의 데이터의 끝까지 진행했다면 해당 심볼은 트레이딩 종료
        ExecuteTradingEnd(symbol_idx, "트레이딩");
        continue;
      }

      // 트레이딩 바 데이터 결손이 있으면 트레이딩 불가
      if (const auto trading_bar_open_time =
              trading_bar_data_->GetBar(symbol_idx, bar_idx).open_time;
          trading_bar_open_time != current_open_time_) {
        logger_->Log(
            WARNING_L,
            format("[{}] 심볼의 [{}] 트레이딩 바가 누락되어 이번 시간의 "
                   "트레이딩을 건너뜁니다. (트레이딩 바 다음 시간: [{}])",
                   trading_bar_data_->GetSymbolName(symbol_idx),
                   UtcTimestampToUtcDatetime(current_open_time_),
                   UtcTimestampToUtcDatetime(trading_bar_open_time)),
            __FILE__, __LINE__, true);

        // 결손 시 현재 시간보다 큰 시간을 가리키므로 인덱스는 증가시키지 않음
        continue;
      }

      // 진행할 바가 남아있고 트레이딩 바 데이터 결손이 없으면 트레이딩 바 유효
    } else {
      // 트레이딩을 시작하지 않은 심볼은 이번 바에서 시작했는지 검사
      if (const auto trading_bar_open_time =
              trading_bar_data_->GetBar(symbol_idx, bar_idx).open_time;
          trading_bar_open_time == current_open_time_) {
        // Open Time이 같다면 트레이딩 바 유효
        trading_began_[symbol_idx] = true;
      } else {
        continue;
      }
    }

    // =========================================================================
    /* 참조 바가 사용 가능한지 검증
     1. 트레이딩 바의 타임프레임과 참조 바의 타임프레임이 같으면 바 인덱스를
        무조건 일치한 후 사용 가능
     2. 참조 바의 타임프레임이 더 크다면, 트레이딩 바의 Close Time이
        참조 바의 Close Time과 같아질 때부터 해당 참조 바의 인덱스를 참조 가능
     2.1. 트레이딩 바의 Close Time보다 참조 바의 Close Time이 작으면
          트레이딩 바의 Close Time까지 최대한 증가 후 사용 가능
     2.2. 트레이딩 바의 Close Time과 참조 바의 Close Time이
          같으면 사용 가능
     2.3. 트레이딩 바의 Close Time보다 참조 바의 Close Time이
          크면 아직 사용 불가능하므로 트레이딩 불가 */
    // =========================================================================
    bool can_use_reference = true;

    for (const auto& [timeframe, bar_data] : reference_bar_data_) {
      bar_->SetCurrentBarType(REFERENCE, timeframe);

      if (timeframe == trading_bar_timeframe_) {
        // 참조 바의 타임프레임이 트레이딩 바의 타임프레임과 같을 때
        bar_->ProcessBarIndex(REFERENCE, timeframe, symbol_idx,
                              current_close_time_);
      } else {
        // 참조 바의 타임프레임이 트레이딩 바의 타임프레임보다 클 때
        bar_->ProcessBarIndex(REFERENCE, timeframe, symbol_idx,
                              current_close_time_);
        const auto moved_bar_idx = bar_->GetCurrentBarIndex();
        const auto moved_close_time =
            bar_data->GetBar(symbol_idx, moved_bar_idx).close_time;

        // ※ 1. 하나의 참조 바라도 데이터가 아직 시작되지 않았으면 트레이딩 불가
        if (moved_close_time > current_close_time_) {
          /* 참조 바 데이터가 사용 불가능하면 트레이딩은 불가능하지만,
             트레이딩 바 인덱스는 맞추어 흘러가야 Open Time, Close Time이 동기화
             되므로 인덱스 증가
             ※ 원래 트레이딩 바 인덱스는 활성화된 심볼에서만 증가함 */
          logger_->Log(
              WARNING_L,
              format("[{} {}] 참조 바 데이터가 아직 시작되지 않아 해당 심볼의 "
                     "트레이딩을 진행할 수 없습니다. (참조 바가 시작되는 기준 "
                     "Close Time: [{}])",
                     trading_bar_data_->GetSymbolName(symbol_idx), timeframe,
                     UtcTimestampToUtcDatetime(moved_close_time)),
              __FILE__, __LINE__, false);

          bar_->IncreaseBarIndex(TRADING, "", symbol_idx);
          can_use_reference = false;
          break;
        }

        // ※ 2. 하나의 참조 바라도 마지막 참조 바 인덱스 시간부터 참조 바
        //       타임프레임만큼 시간이 지나면 더이상 참조 바 사용이 불가해지므로
        //       해당 심볼의 트레이딩 종료
        if (moved_bar_idx == bar_data->GetNumBars(symbol_idx) - 1 &&
            current_close_time_ ==
                moved_close_time + reference_bar_time_diff_[timeframe]) {
          ExecuteTradingEnd(symbol_idx, "참조");
          can_use_reference = false;
          break;
        }
      }
    }

    if (!can_use_reference) {
      continue;
    }

    // =========================================================================
    /* 돋보기 바를 사용 가능한지 검증
      1. 트레이딩 전 바 Close Time보다 돋보기 바의 Close Time이 작으면
         트레이딩 전 바 Close Time까지 최대한 증가 후 사용 가능
      2. 트레이딩 전 바 Close Time과 돋보기 바의 Close Time이 같으면
         사용 가능
      3. 트레이딩 전 바 Close Time보다 돋보기 바의 Close Time이 크면
         아직 사용 불가능하므로 트레이딩 불가 */
    // =========================================================================
    if (use_bar_magnifier_) {
      bar_->SetCurrentBarType(MAGNIFIER, "");

      bar_->ProcessBarIndex(MAGNIFIER, "", symbol_idx, current_open_time_ - 1);
      const auto moved_bar_idx = bar_->GetCurrentBarIndex();
      const auto moved_close_time =
          magnifier_bar_data_->GetBar(symbol_idx, moved_bar_idx).close_time;

      // ※ 1. 돋보기 바의 데이터가 아직 시작되지 않았으면 트레이딩 불가
      // 전 바의 Close Time으로 일치시키므로 >= 조건 사용 (데이터 누락)
      if (moved_close_time >= current_close_time_) {
        /* 돋보기 바 데이터가 사용 불가능하면 트레이딩은 불가능하지만,
           트레이딩 바 인덱스는 맞추어 흘러가야 Open Time, Close Time이 동기화
           되므로 인덱스 증가
           ※ 원래 트레이딩 바 인덱스는 활성화된 심볼에서만 증가함 */
        logger_->Log(
            WARNING_L,
            format("[{}] 돋보기 바 데이터가 아직 시작되지 않아 해당 심볼의 "
                   "트레이딩을 진행할 수 없습니다. (돋보기 바 시작 시간: [{}])",
                   trading_bar_data_->GetSymbolName(symbol_idx),
                   UtcTimestampToUtcDatetime(
                       magnifier_bar_data_->GetBar(symbol_idx, moved_bar_idx)
                           .open_time)),
            __FILE__, __LINE__, true);

        bar_->IncreaseBarIndex(TRADING, "", symbol_idx);
        continue;
      }

      // ※ 2. 마지막 바까지 이동 시 해당 심볼의 트레이딩 종료
      if (moved_bar_idx == magnifier_bar_data_->GetNumBars(symbol_idx) - 1) {
        ExecuteTradingEnd(symbol_idx, "돋보기");
        continue;
      }
    }

    // 조건에 걸리지 않은 심볼은 트레이딩 진행
    activated_symbol_indices_.push_back(symbol_idx);
  }
}

void Engine::ExecuteTradingEnd(const int symbol_idx,
                               const string& bar_type_str) {
  trading_ended_[symbol_idx] = true;

  // 진입 및 청산 대기 주문을 취소하고 체결된 진입 주문 잔량을 종가에 청산
  // 메인 루프 전 트레이딩 바 인덱스를 하나 증가시켰으므로 하나 감소시켜야
  // 마지막 바를 가리킴
  bar_->SetCurrentBarType(TRADING, "");
  bar_->SetCurrentBarIndex(bar_->GetCurrentBarIndex() - 1);

  order_handler_->CancelAll();
  order_handler_->CloseAll();

  // 바가 끝난 전량 청산은 Just Exited로 판단하지 않음
  order_handler_->InitializeJustExited();

  logger_->Log(
      INFO_L,
      format("[{}] 심볼의 {} 바 데이터가 끝나 해당 심볼의 "
             "백테스팅을 종료합니다.",
             trading_bar_data_->GetSymbolName(symbol_idx), bar_type_str),
      __FILE__, __LINE__, true);
}

void Engine::ExecuteAllTradingEnd() {
  all_trading_ended_ = true;

  // 트레이딩이 끝나지 않은 심볼만 처리
  for (int symbol_idx = 0; symbol_idx < trading_bar_num_symbols_;
       symbol_idx++) {
    if (!trading_ended_[symbol_idx]) {
      bar_->SetCurrentSymbolIndex(symbol_idx);
      trading_ended_[symbol_idx] = true;

      // 진입 및 청산 대기 주문을 취소하고 체결된 진입 주문 잔량을 종가 청산
      // 메인 루프 전 트레이딩 바 인덱스를 하나 증가시켰으므로 하나
      // 감소시켜야 마지막 바를 가리킴
      bar_->SetCurrentBarType(TRADING, "");
      bar_->SetCurrentBarIndex(bar_->GetCurrentBarIndex() - 1);

      order_handler_->CancelAll();
      order_handler_->CloseAll();

      // 트레이딩이 모두 종료된 전량 청산은 Just Exited로 판단하지 않음
      order_handler_->InitializeJustExited();

      logger_->Log(INFO_L,
                   format("지정된 종료 시간에 의해 [{}] 심볼의 "
                          "백테스팅을 종료합니다.",
                          trading_bar_data_->GetSymbolName(symbol_idx)),
                   __FILE__, __LINE__, true);
    }
  }
}

void Engine::CheckFundingTime() {
  auto update_next_funding_info = [&](const int symbol_idx) {
    const auto& funding_rates = symbol_info_[symbol_idx].GetFundingRates();

    // 다음 펀딩 비율 인덱스가 데이터 범위를 벗어나는지 체크
    if (const auto next_idx = ++funding_rates_indices_[symbol_idx];
        next_idx < funding_rates.size()) {
      const auto& [funding_rate, funding_time, mark_price] =
          funding_rates[next_idx];

      next_funding_rates_[symbol_idx] = funding_rate;
      next_funding_times_[symbol_idx] = funding_time;
      next_funding_mark_prices_[symbol_idx] = mark_price;
    } else {
      logger_->Log(WARNING_L,
                   format("[{}] 펀딩 비율 데이터가 종료되었으므로 해당 심볼의 "
                          "펀딩비는 더 이상 정산되지 않습니다.",
                          trading_bar_data_->GetSymbolName(symbol_idx)),
                   __FILE__, __LINE__);

      next_funding_rates_[symbol_idx] = -1;
      next_funding_times_[symbol_idx] = INT64_MAX;
      next_funding_mark_prices_[symbol_idx] = -1;
    }
  };

  for (const auto symbol_idx : activated_symbol_indices_) {
    bar_->SetCurrentSymbolIndex(symbol_idx);

    // 펀딩 시간이 되면 펀딩
    // 만약 펀딩 비율 데이터가 종료되었으면 펀딩비는 없음
    // -> 종료 시 MAX 값으로 설정되므로 자동으로 조건 미통과
    if (const auto funding_time = next_funding_times_[symbol_idx];
        current_open_time_ >= funding_time) {
      const double next_funding_price = next_funding_mark_prices_[symbol_idx];
      double funding_price;

      // 펀딩의 기준 가격 찾기
      if (!isnan(next_funding_price)) {
        // 1. 펀딩 비율 데이터의 기본 데이터가 존재하면 그대로 사용
        funding_price = next_funding_price;
      } else if (const auto& current_mark_price_bar =
                     mark_price_bar_data_->GetBar(
                         symbol_idx, (*mark_price_indices_)[symbol_idx]);
                 current_close_time_ == current_mark_price_bar.close_time) {
        // 2. 마크 가격의 Close Time이 현재 진행 시간의 Close Time과 같다면
        //    마크 가격의 Open 가격을 사용
        funding_price = current_mark_price_bar.open;
      } else if (const auto& current_market_bar =
                     bar_->GetBarData(bar_->GetCurrentBarType())
                         ->GetBar(symbol_idx, bar_->GetCurrentBarIndex());
                 current_close_time_ == current_market_bar.close_time) {
        // 3. 시장 가격의 Close Time이 현재 진행 시간의 Close Time과 같다면
        //    시장 가격의 Open 가격을 사용
        funding_price = current_market_bar.open;
      } else {
        // 4. 모든 일치하는 데이터가 없다면 펀딩비 정산 불가
        logger_->Log(
            WARNING_L,
            format("[{}] 펀딩 비율 데이터에 마크 가격이 존재하지 않으며, 현재 "
                   "진행 시간 [{}]과 일치하는 마크 가격 바와 시장 가격 바가 "
                   "존재하지 않으므로 펀딩비를 정산할 수 없습니다.",
                   UtcTimestampToUtcDatetime(funding_time),
                   UtcTimestampToUtcDatetime(current_open_time_)),
            __FILE__, __LINE__, true);

        // 다음 펀딩 정보 업데이트
        update_next_funding_info(symbol_idx);
        continue;
      }

      // 펀딩 가격이 정상적으로 존재한다면 펀딩비 정산
      order_handler_->ExecuteFunding(next_funding_rates_[symbol_idx],
                                     UtcTimestampToUtcDatetime(funding_time),
                                     funding_price, symbol_idx);

      // 다음 펀딩 정보 업데이트
      update_next_funding_info(symbol_idx);
    }
  }
}

void Engine::ProcessOhlc(const BarType bar_type,
                         const vector<int>& symbol_indices) {
  // 매커니즘에 따라 확인할 순서대로 가격을 정렬한 벡터 얻기
  const auto [mark_price_queue, market_price_queue] =
      move(GetPriceQueue(bar_type, symbol_indices));

  // 순서: 한 가격에서 전략 실행 후 다음 심볼 및 가격으로 넘어감
  // 한 심볼에서 모든 가격 체크 후 다음 가격 체크하면 논리상 시간을 한 번
  // 거슬러 올라가는 것이므로 옳지 않음
  for (int queue_idx = 0; queue_idx < mark_price_queue.size(); queue_idx++) {
    const auto [mark_price, mark_price_type, mark_price_symbol_idx] =
        mark_price_queue[queue_idx];

    const auto [market_price, market_price_type, market_price_symbol_idx] =
        market_price_queue[queue_idx];

    // 마크 가격과 시장 가격에서 하나의 큐 인덱스의 심볼 인덱스는 동일
    bar_->SetCurrentSymbolIndex(mark_price_symbol_idx);

    // 정해진 순서대로 강제 청산을 확인
    // 강제 청산의 경우 After Exit 전략이 실행됨
    order_handler_->CheckLiquidation(mark_price, mark_price_type,
                                     mark_price_symbol_idx, bar_type);

    // 정해진 순서대로 진입 및 청산 대기 주문의 체결을 확인
    do {
      // CheckPending 함수가 루프에 포함되어 여러 번 체결을 확인하는 이유는,
      // After Entry, After Exit 전략에서의 주문도 체결될 수도 있기 때문

      // 진입 대기 주문의 체결 확인 후 체결이 존재할 시 After Entry 전략 실행
      order_handler_->CheckPendingEntries(market_price, market_price_type,
                                          market_price_symbol_idx);

      if (order_handler_->IsJustEntered()) {
        order_handler_->InitializeJustEntered();
        ExecuteStrategy(strategy_, AFTER_ENTRY, market_price_symbol_idx);
      }

      // 청산 대기 주문의 체결 확인 후 체결이 존재할 시 After Exit 전략 실행
      order_handler_->CheckPendingExits(market_price, market_price_type,
                                        market_price_symbol_idx);

      if (order_handler_->IsJustExited()) {
        order_handler_->InitializeJustExited();
        ExecuteStrategy(strategy_, AFTER_EXIT, market_price_symbol_idx);
      }

      // 진입 및 청산 체결이 없는 경우 체결 확인 및 전략 실행 종료
    } while (order_handler_->IsJustEntered() || order_handler_->IsJustExited());
  }
}

pair<vector<PriceData>, vector<PriceData>> Engine::GetPriceQueue(
    const BarType market_bar_type, const vector<int>& symbol_indices) const {
  size_t vector_idx = 0;
  const auto num_symbols = symbol_indices.size();

  // 총 크기를 미리 계산하여 한 번에 할당 (심볼 개수 * OHLC 4개)
  const auto total_size = num_symbols * 4;
  vector<PriceData> mark_queue(total_size);
  vector<PriceData> market_queue(total_size);

#pragma omp parallel for if (num_symbols > 1)
  for (const auto symbol_idx : symbol_indices) {
    const auto& orig_mark_bar = mark_price_bar_data_->GetBar(
        symbol_idx, (*mark_price_indices_)[symbol_idx]);
    const auto& market_bar =
        market_bar_type == TRADING
            ? trading_bar_data_->GetBar(symbol_idx,
                                        (*trading_indices_)[symbol_idx])
            : magnifier_bar_data_->GetBar(symbol_idx,
                                          (*magnifier_indices_)[symbol_idx]);

    // 마크 가격의 Open Time과 시장 가격의 Open Time이 다르다면 시장 가격을
    // 기준으로 강제 청산을 확인
    // (ProcessOhlc 호출 전 Close Time을 일치시키기 때문에 데이터가 존재하다면
    // Open Time도 일치해야 함)
    const auto& mark_bar = orig_mark_bar.open_time != market_bar.open_time
                               ? market_bar
                               : orig_mark_bar;

    // 값 캐싱
    const double mark_open = mark_bar.open;
    const double mark_high = mark_bar.high;
    const double mark_low = mark_bar.low;
    const double mark_close = mark_bar.close;

    const double market_open = market_bar.open;
    const double market_high = market_bar.high;
    const double market_low = market_bar.low;
    const double market_close = market_bar.close;

    // 고저가 순서 결정
    // 시가 대비 고가의 폭이 저가의 폭보다 크면: 시가 → 저가 → 고가 → 종가
    // 시가 대비 저가의 폭이 고가의 폭보다 크면: 시가 → 고가 → 저가 → 종가
    const bool mark_low_first =
        IsGreaterOrEqual(mark_high - mark_open, mark_open - mark_low);
    const bool market_low_first =
        IsGreaterOrEqual(market_high - market_open, market_open - market_low);

    // Open
    mark_queue[vector_idx] = {mark_open, OPEN, symbol_idx};
    market_queue[vector_idx] = {market_open, OPEN, symbol_idx};

    // High/Low1
    mark_queue[num_symbols + vector_idx] =
        mark_low_first ? PriceData{mark_low, LOW, symbol_idx}
                       : PriceData{mark_high, HIGH, symbol_idx};
    market_queue[num_symbols + vector_idx] =
        market_low_first ? PriceData{market_low, LOW, symbol_idx}
                         : PriceData{market_high, HIGH, symbol_idx};

    // High/Low2
    mark_queue[2 * num_symbols + vector_idx] =
        mark_low_first ? PriceData{mark_high, HIGH, symbol_idx}
                       : PriceData{mark_low, LOW, symbol_idx};
    market_queue[2 * num_symbols + vector_idx] =
        market_low_first ? PriceData{market_high, HIGH, symbol_idx}
                         : PriceData{market_low, LOW, symbol_idx};

    // Close
    mark_queue[3 * num_symbols + vector_idx] = {mark_close, CLOSE, symbol_idx};
    market_queue[3 * num_symbols + vector_idx] = {market_close, CLOSE,
                                                  symbol_idx};

    vector_idx++;
  }

  return {move(mark_queue), move(market_queue)};
}

void Engine::ExecuteStrategy(const shared_ptr<Strategy>& strategy,
                             const StrategyType strategy_type,
                             const int symbol_index) {
  // = 원본 설정을 저장 =
  // 종가 전략 실행인 경우 트레이딩 바
  // (ON_CLOSE, AFTER ENTRY, AFTER EXIT)
  //
  // ProcessOhlc에서 전략 실행인 경우 트레이딩 바 혹은 돋보기 바
  // (AFTER ENTRY, AFTER EXIT)
  const auto original_bar_type = bar_->GetCurrentBarType();

  // 트레이딩 바의 지정된 심볼에서 전략 실행
  // 돋보기 바에서 AFTER ENTRY, AFTER EXIT가 실행되더라도 전략은 종가 기준으로
  // 하는 것이 참조 측면에서 올바름
  bar_->SetCurrentBarType(TRADING, "");
  bar_->SetCurrentSymbolIndex(symbol_index);

  // 현재 심볼의 포지션 사이즈 업데이트
  order_handler_->UpdateCurrentPositionSize();

  try {
    current_strategy_type_ = strategy_type;

    switch (strategy_type) {
      case ON_CLOSE: {
        strategy->ExecuteOnClose();
        break;
      }

      case AFTER_ENTRY: {
        strategy->ExecuteAfterEntry();
        break;
      }

      case AFTER_EXIT: {
        strategy->ExecuteAfterExit();
        break;
      }
    }
  } catch ([[maybe_unused]] const Bankruptcy& e) {
    SetBankruptcy();
    throw;
  }

  // 원본 설정을 복원
  bar_->SetCurrentBarType(original_bar_type, "");
}

}  // namespace backtesting::engine