// 표준 라이브러리
#include <algorithm>
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
#include "Engines/Backtesting.hpp"
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

BACKTESTING_API chrono::steady_clock::time_point
    Engine::backtesting_start_time_ = chrono::high_resolution_clock::now();

BACKTESTING_API mutex Engine::mutex_;
BACKTESTING_API shared_ptr<Engine> Engine::instance_;

shared_ptr<Engine>& Engine::GetEngine() {
  lock_guard lock(mutex_);  // 다중 스레드에서 안전하게 접근하기 위해 mutex 사용

  // 인스턴스가 생성됐는지 확인
  if (!instance_) {
    instance_ = shared_ptr<Engine>(new Engine(), Deleter());
  }

  return instance_;
}

void Engine::Backtesting() {
  LogSeparator(true);
  Initialize();

  LogSeparator(true);
  logger_->Log(INFO_L, std::format("백테스팅을 시작합니다."), __FILE__,
               __LINE__, true);

  try {
    BacktestingMain();
  } catch ([[maybe_unused]] const Bankruptcy& e) {
    SetBankruptcy();

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
  analyzer_->SaveBackBoard();

  LogSeparator(true);
  logger_->Log(INFO_L, "백테스팅이 완료되었습니다.", __FILE__, __LINE__, true);

  logger_->Log(
      INFO_L,
      "소요 시간: " + FormatTimeDiff(duration_cast<chrono::milliseconds>(
                                         chrono::high_resolution_clock::now() -
                                         backtesting_start_time_)
                                         .count()),
      __FILE__, __LINE__, true);

  // 백테스팅 로그 저장
  // 소요 시간까지 확실히 로그로 저장시키기 위해 마지막에 저장하며,
  // 어색함 방지를 위해 저장 완료 로그를 발생시키지 않음
  analyzer_->SaveBacktestingLog();

  // BackBoard.exe 자동 실행
  if (const string backboard_exe_path =
          analyzer_->GetMainDirectory() + "/BackBoard.exe";
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

bool Engine::IsTradingEnded(const int symbol_idx) const {
  return trading_ended_[symbol_idx];
}

void Engine::ResetEngine() {
  lock_guard lock(mutex_);

  ResetBaseEngine();

  backtesting_start_time_ = chrono::high_resolution_clock::now();

  if (instance_) {
    instance_.reset();
    instance_ = shared_ptr<Engine>(new Engine(), Deleter());
  }
}

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
    // 이후 에러 로그에서 서버 모드는 메세지가 달라야하지만,
    // 앞선 검증을 통해 null 가능성은 없으므로 패스

    if (config_ == nullptr) {
      throw runtime_error(
          "엔진 설정이 추가되지 않았습니다. "
          "Backtesting::SetConfig 함수를 호출해 주세요.");
    }

    const auto& project_directory = Config::GetProjectDirectory();
    const auto& opt_backtest_period = config_->GetBacktestPeriod();
    const auto& opt_use_bar_magnifier = config_->GetUseBarMagnifier();
    const auto initial_balance = config_->GetInitialBalance();
    const auto taker_fee_percentage = config_->GetTakerFeePercentage();
    const auto maker_fee_percentage = config_->GetMakerFeePercentage();
    const auto& opt_check_market_max_qty = config_->GetCheckMarketMaxQty();
    const auto& opt_check_market_min_qty = config_->GetCheckMarketMinQty();
    const auto& opt_check_limit_max_qty = config_->GetCheckLimitMaxQty();
    const auto& opt_check_limit_min_qty = config_->GetCheckLimitMinQty();
    const auto& opt_check_min_notional_value =
        config_->GetCheckMinNotionalValue();
    const auto& slippage = config_->GetSlippage();

    // 각 항목에 대해 초기화되지 않았을 경우 예외를 던짐
    if (project_directory.empty()) {
      throw runtime_error(
          "프로젝트 폴더가 초기화되지 않았습니다. "
          "Backtesting::SetConfig().SetProjectDirectory 함수를 호출해 주세요.");
    }

    if (!filesystem::exists(project_directory)) {
      throw runtime_error(format("프로젝트 폴더 [{}]이(가) 존재하지 않습니다.",
                                 project_directory));
    }

    if (!opt_backtest_period) {
      throw runtime_error(
          "백테스팅 기간이 설정되지 않았습니다. "
          "Backtesting::SetConfig().SetBacktestPeriod 함수를 호출해 주세요.");
    }

    if (!opt_use_bar_magnifier) {
      throw runtime_error(
          "바 돋보기 사용 여부가 초기화되지 않았습니다. "
          "Backtesting::SetConfig().SetUseBarMagnifier 함수를 호출해 주세요.");
    }

    if (isnan(initial_balance)) {
      throw runtime_error(
          "초기 자금이 초기화되지 않았습니다. "
          "Backtesting::SetConfig().SetInitialBalance 함수를 호출해 주세요.");
    }

    if (isnan(taker_fee_percentage)) {
      throw runtime_error(
          "테이커 수수료 퍼센트가 초기화되지 않았습니다. "
          "Backtesting::SetConfig().SetTakerFeePercentage 함수를 호출해 "
          "주세요.");
    }

    if (isnan(maker_fee_percentage)) {
      throw runtime_error(
          "메이커 수수료 퍼센트가 초기화되지 않았습니다. "
          "Backtesting::SetConfig().SetMakerFeePercentage 함수를 호출해 "
          "주세요.");
    }

    if (slippage == nullptr) {
      throw runtime_error(
          "슬리피지가 초기화되지 않았습니다. "
          "Backtesting::SetConfig().SetSlippage 함수를 호출해 주세요.");
    }

    if (IsLessOrEqual(initial_balance, 0.0)) {
      throw runtime_error(format("지정된 초기 자금 [{}]는 0보다 커야 합니다.",
                                 FormatDollar(initial_balance, true)));
    }

    if (IsGreater(taker_fee_percentage, 100.0) ||
        IsLess(taker_fee_percentage, 0.0)) {
      throw runtime_error(
          format("지정된 테이커 수수료 퍼센트 [{}%]는 100% 초과 혹은 "
                 "0% 미만으로 설정할 수 없습니다.",
                 taker_fee_percentage));
    }

    if (IsGreater(maker_fee_percentage, 100.0) ||
        IsLess(maker_fee_percentage, 0.0)) {
      throw runtime_error(
          format("지정된 메이커 수수료 퍼센트 [{}%]는 100% 초과 혹은 "
                 "0% 미만으로 설정할 수 없습니다.",
                 maker_fee_percentage));
    }

    if (const auto& error_msg = slippage->ValidateTakerSlippage()) {
      throw runtime_error(*error_msg);
    }

    if (const auto& error_msg = slippage->ValidateMakerSlippage()) {
      throw runtime_error(*error_msg);
    }

    if (!opt_check_market_max_qty) {
      throw runtime_error(
          "시장가 최대 수량 검사 여부가 초기화되지 않았습니다. "
          "Backtesting::SetConfig().SetCheckMarketMaxQty 함수를 호출해 "
          "주세요.");
    }

    if (!opt_check_market_min_qty) {
      throw runtime_error(
          "시장가 최소 수량 검사 여부가 초기화되지 않았습니다. "
          "Backtesting::SetConfig().SetCheckMarketMinQty 함수를 호출해 "
          "주세요.");
    }

    if (!opt_check_limit_max_qty) {
      throw runtime_error(
          "지정가 최대 수량 검사 여부가 초기화되지 않았습니다. "
          "Backtesting::SetConfig().SetCheckLimitMaxQty 함수를 호출해 주세요.");
    }

    if (!opt_check_limit_min_qty) {
      throw runtime_error(
          "지정가 최소 수량 검사 여부가 초기화되지 않았습니다. "
          "Backtesting::SetConfig().SetCheckLimitMinQty 함수를 호출해 주세요.");
    }

    if (!opt_check_min_notional_value) {
      throw runtime_error(
          "최소 명목 가치 검사 여부가 초기화되지 않았습니다. "
          "Backtesting::SetConfig().SetCheckMinNotionalValue 함수를 호출해 "
          "주세요.");
    }

    logger_->Log(INFO_L, "엔진 설정 유효성 검증이 완료되었습니다.", __FILE__,
                 __LINE__, true);
  } catch (const std::exception& e) {
    logger_->Log(ERROR_L, "엔진 설정 유효성 검증이 실패했습니다.", __FILE__,
                 __LINE__, true);

    throw runtime_error(e.what());
  }
}

void Engine::IsValidBarData() {
  try {
    const auto& trading_bar_data = bar_->GetBarData(TRADING, "");
    const auto trading_num_symbols = trading_bar_data->GetNumSymbols();

    // 1.1. 트레이딩 바 데이터가 비었는지 검증
    if (!trading_num_symbols)
      throw runtime_error(
          "트레이딩 바 데이터가 추가되지 않았습니다. "
          "Backtesting::AddBarData 함수를 호출해 주세요.");

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

        if (Backtesting::IsServerMode()) {
          throw runtime_error(
              "이 검사를 비활성화하고 싶다면 심볼 간 트레이딩 바 데이터 중복 "
              "검사 체크 박스를 해제해 주세요.");
        }

        throw runtime_error(
            "이 검사를 비활성화하고 싶다면 "
            "Backtesting::SetConfig().DisableSameBarDataCheck 함수를 "
            "호출해 주세요.");
      }
    }

    // =========================================================================
    const auto use_bar_magnifier = *config_->GetUseBarMagnifier();
    const auto& magnifier_bar_data = bar_->GetBarData(MAGNIFIER, "");
    const auto magnifier_num_symbols = magnifier_bar_data->GetNumSymbols();

    if (use_bar_magnifier) {
      /* 2.1. 트레이딩 바 데이터의 심볼 개수와 돋보기 바 데이터의
              심볼 개수가 같은지 검증 */
      if (trading_num_symbols != magnifier_num_symbols) {
        throw runtime_error(
            format("돋보기 기능 사용 시 트레이딩 바 데이터에 추가된 "
                   "심볼 개수({}개)와 돋보기 바 데이터에 추가된 심볼 "
                   "개수({}개)는 동일해야 합니다.",
                   trading_num_symbols, magnifier_num_symbols));
      }

      /* 2.2. 트레이딩 바 데이터의 심볼들이 돋보기 바 데이터에 존재하고
              순서가 같은지 검증 */
      for (int symbol_idx = 0; symbol_idx < trading_num_symbols; ++symbol_idx) {
        if (const auto& symbol_name =
                trading_bar_data->GetSafeSymbolName(symbol_idx);
            symbol_name != magnifier_bar_data->GetSafeSymbolName(symbol_idx)) {
          throw runtime_error(format(
              "돋보기 바 데이터에 [{}]이(가) 존재하지 않거나 "
              "트레이딩 바 데이터에 추가된 심볼 순서와 일치하지 않습니다.",
              symbol_name));
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

          if (Backtesting::IsServerMode()) {
            throw runtime_error(
                "이 검사를 비활성화하고 싶다면 심볼 간 돋보기 바 데이터 중복 "
                "검사 체크 박스를 해제해 주세요.");
          }

          throw runtime_error(
              "이 검사를 비활성화하고 싶다면 "
              "Backtesting::SetConfig().DisableSameBarDataCheck 함수를 "
              "호출해 주세요.");
        }
      }
    } else {
      // 2.4. 돋보기 기능을 사용하지 않는데 돋보기 바로 추가되었는지 확인
      if (magnifier_num_symbols != 0) {
        throw runtime_error(
            "돋보기 기능을 사용하지 않으면 돋보기 바 데이터를 추가할 수 "
            "없습니다.");
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
        throw runtime_error(format(
            "트레이딩 바 데이터에 추가된 심볼 개수({}개)와 참조 바 "
            "데이터 [{}]에 추가된 심볼 개수({}개)는 동일해야 합니다.",
            trading_num_symbols, reference_timeframe, reference_num_symbols));
      }

      /* 3.2. 트레이딩 바 데이터의 심볼들이 참조 바 데이터에 존재하고
              순서가 같은지 검증 */
      for (int symbol_idx = 0; symbol_idx < trading_num_symbols; ++symbol_idx) {
        if (const auto& symbol_name =
                trading_bar_data->GetSafeSymbolName(symbol_idx);
            symbol_name != reference_bar_data->GetSafeSymbolName(symbol_idx)) {
          throw runtime_error(format(
              "참조 바 데이터에 [{} {}]이(가) 존재하지 않거나 "
              "트레이딩 바 데이터에 추가된 심볼 순서와 일치하지 않습니다.",
              symbol_name, reference_timeframe));
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

          if (Backtesting::IsServerMode()) {
            throw runtime_error(
                "이 검사를 비활성화하고 싶다면 심볼 간 참조 바 데이터 중복 "
                "검사 체크 박스를 해제해 주세요.");
          }

          throw runtime_error(
              "이 검사를 비활성화하고 싶다면 "
              "Backtesting::SetConfig().DisableSameBarDataCheck 함수를 "
              "호출해 주세요.");
        }
      }
    }

    // =========================================================================
    const auto& mark_price_bar_data = bar_->GetBarData(MARK_PRICE, "");
    const auto mark_price_num_symbols = mark_price_bar_data->GetNumSymbols();

    // 돋보기 기능 사용 시 마크 가격 바 데이터는 돋보기 바 데이터와 비교
    // 그렇지 않으면 트레이딩 바 데이터와 비교
    const auto target_bar_data =
        use_bar_magnifier ? magnifier_bar_data : trading_bar_data;
    const auto target_num_symbols = target_bar_data->GetNumSymbols();

    /* 4.1. 타겟 바 데이터의 타임프레임과 마크 가격 바 데이터의
            타임프레임이 같은지 검증 */
    const auto& target_timeframe = target_bar_data->GetTimeframe();
    if (const auto& mark_price_timeframe = mark_price_bar_data->GetTimeframe();
        target_timeframe != mark_price_timeframe) {
      throw runtime_error(
          format("{} 바 데이터의 타임프레임 [{}]와(과) 마크 가격 바 데이터의 "
                 "타임프레임 [{}]은(는) 동일해야 합니다.",
                 use_bar_magnifier ? "돋보기 기능 사용 시 돋보기" : "트레이딩",
                 target_timeframe, mark_price_timeframe));
    }

    /* 4.2. 타겟 바 데이터의 심볼 개수와 마크 가격 바 데이터의
            심볼 개수가 같은지 검증 */
    if (target_num_symbols != mark_price_num_symbols) {
      throw runtime_error(format(
          "{} 바 데이터에 추가된 심볼 개수({}개)와 마크 가격 바 데이터에 "
          "추가된 심볼 개수({}개)는 동일해야 합니다.",
          use_bar_magnifier ? "돋보기 기능 사용 시 돋보기" : "트레이딩",
          target_num_symbols, mark_price_num_symbols));
    }

    /* 4.3. 타겟 바 데이터의 심볼들이 마크 가격 바 데이터에 존재하고
            순서가 같은지 검증 */
    for (int symbol_idx = 0; symbol_idx < target_num_symbols; ++symbol_idx) {
      if (const auto& symbol_name =
              target_bar_data->GetSafeSymbolName(symbol_idx);
          symbol_name != mark_price_bar_data->GetSafeSymbolName(symbol_idx)) {
        throw runtime_error(
            format("마크 가격 바 데이터에 [{}]이(가) 존재하지 않거나 "
                   "{} 바 데이터에 추가된 심볼 순서와 일치하지 않습니다.",
                   symbol_name, use_bar_magnifier ? "돋보기" : "트레이딩"));
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

        if (Backtesting::IsServerMode()) {
          throw runtime_error(
              "이 검사를 비활성화하고 싶다면 심볼 간 마크 가격 바 데이터 중복 "
              "검사 체크 박스를 해제해 주세요.");
        }

        throw runtime_error(
            "이 검사를 비활성화하고 싶다면 "
            "Backtesting::SetConfig().DisableSameBarDataCheck 함수를 "
            "호출해 주세요.");
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
                    mark_price_bar_data->GetSafeSymbolName(
                        mark_price_symbol_idx),
                    use_bar_magnifier ? "돋보기" : "트레이딩",
                    target_bar_data->GetSafeSymbolName(target_symbol_idx)),
                __FILE__, __LINE__, true);

            if (Backtesting::IsServerMode()) {
              throw runtime_error(
                  "이 검사를 비활성화하고 싶다면 마크 가격 바 데이터와 목표 바 "
                  "데이터 중복 검사 체크 박스를 해제해 주세요.");
            }

            throw runtime_error(
                "이 검사를 비활성화하고 싶다면 "
                "Backtesting::SetConfig().DisableSameBarDataWithTargetCheck "
                "함수를 호출해 주세요.");
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
      throw runtime_error(
          "펀딩비의 정확한 정산을 위해 돋보기 기능 미사용 시 트레이딩 바 "
          "데이터의 타임프레임이 1h 이하여야 하며, 돋보기 기능 사용 시 돋보기 "
          "바 데이터의 타임프레임이 1h 이하여야 합니다.");
    }

    logger_->Log(INFO_L, "바 데이터 유효성 검증이 완료되었습니다.", __FILE__,
                 __LINE__, true);
  } catch (const std::exception& e) {
    logger_->Log(ERROR_L, "바 데이터 유효성 검증이 실패했습니다.", __FILE__,
                 __LINE__, true);

    throw runtime_error(e.what());
  }
}

void Engine::IsValidDateRange() {
  try {
    const auto& trading_bar_data = bar_->GetBarData(TRADING, "");
    for (int symbol_idx = 0; symbol_idx < trading_bar_data->GetNumSymbols();
         symbol_idx++) {
      // 바 데이터 중 가장 처음의 Open Time 값 구하기
      begin_open_time_ = min(begin_open_time_,
                             trading_bar_data->GetBar(symbol_idx, 0).open_time);

      // 바 데이터 중 가장 끝의 Close Time 값 구하기
      end_close_time_ = max(
          end_close_time_,
          trading_bar_data
              ->GetBar(symbol_idx, trading_bar_data->GetNumBars(symbol_idx) - 1)
              .close_time);
    }

    // 백테스팅 기간 받아오기
    const auto backtest_period = *config_->GetBacktestPeriod();
    const auto& start_time = backtest_period.GetStartTime();
    const auto& end_time = backtest_period.GetEndTime();
    const auto& format = backtest_period.GetFormat();

    // Start가 지정된 경우 범위 체크
    if (!start_time.empty()) {
      if (const auto start_time_ts =
              UtcDatetimeToUtcTimestamp(start_time, format);
          start_time_ts < begin_open_time_) {
        throw runtime_error(std::format(
            "지정된 백테스팅 시작 시간 [{}]은(는) 바 데이터 최소 "
            "시간 [{}]의 전으로 지정할 수 없습니다.",
            start_time, UtcTimestampToUtcDatetime(begin_open_time_)));
      } else {
        begin_open_time_ = start_time_ts;
      }
    }

    // End가 지정된 경우 범위 체크
    if (!end_time.empty()) {
      if (const auto end_time_ts = UtcDatetimeToUtcTimestamp(end_time, format);
          end_time_ts > end_close_time_) {
        throw runtime_error(
            std::format("지정된 백테스팅 종료 시간 [{}]은(는) 바 데이터 최대 "
                        "시간 [{}]의 후로 지정할 수 없습니다.",
                        end_time, UtcTimestampToUtcDatetime(end_close_time_)));
      } else {
        end_close_time_ = end_time_ts;
      }
    }

    // Start, End가 둘 다 지정된 경우 범위 체크
    if (!start_time.empty() && !end_time.empty()) {
      if (UtcDatetimeToUtcTimestamp(start_time, format) >
          UtcDatetimeToUtcTimestamp(end_time, format)) {
        throw runtime_error(
            std::format("지정된 백테스팅 시작 시간 [{}]은(는) 지정된 백테스팅 "
                        "종료 시간 [{}]의 후로 지정할 수 없습니다.",
                        start_time, end_time));
      }
    }

    logger_->Log(INFO_L, "시간 범위 유효성 검증이 완료되었습니다.", __FILE__,
                 __LINE__, true);
  } catch (const std::exception& e) {
    logger_->Log(ERROR_L, "시간 범위 유효성 검증이 실패했습니다.", __FILE__,
                 __LINE__, true);

    throw runtime_error(e.what());
  }
}

void Engine::IsValidSymbolInfo() {
  const auto& trading_bar_data = bar_->GetBarData(TRADING, "");
  const auto trading_num_symbols = trading_bar_data->GetNumSymbols();

  try {
    if (exchange_info_.empty()) {
      throw runtime_error(
          "엔진에 거래소 정보가 추가되지 않았습니다. "
          "Backtesting::AddExchangeInfo 함수를 호출해 주세요.");
    }

    if (leverage_bracket_.empty()) {
      throw runtime_error(
          "엔진에 레버리지 구간이 추가되지 않았습니다. "
          "Backtesting::AddLeverageBracket 함수를 호출해 주세요.");
    }

    if (funding_rates_.empty()) {
      throw runtime_error(
          "엔진에 펀딩 비율이 추가되지 않았습니다. "
          "Backtesting::AddFundingRates 함수를 호출해 주세요.");
    }

    if (const auto funding_rates_num_symbols = funding_rates_.size();
        funding_rates_num_symbols != trading_num_symbols) {
      throw runtime_error(
          format("트레이딩 바 데이터에 추가된 심볼 개수({}개)와 펀딩 비율에 "
                 "추가된 심볼 개수({}개)는 동일해야 합니다.",
                 trading_num_symbols, funding_rates_num_symbols));
    }

    for (int symbol_idx = 0; symbol_idx < trading_num_symbols; ++symbol_idx) {
      if (const auto& symbol_name =
              trading_bar_data->GetSafeSymbolName(symbol_idx);
          symbol_name !=
          funding_rates_[symbol_idx][0]["symbol"].get<string>()) {
        throw runtime_error(
            format("펀딩 비율에 [{}]이(가) 존재하지 않거나 "
                   "트레이딩 바 데이터에 추가된 심볼 순서와 일치하지 않습니다.",
                   symbol_name));
      }
    }

    logger_->Log(INFO_L, "심볼 정보 유효성 검증이 완료되었습니다.", __FILE__,
                 __LINE__, true);
  } catch (const std::exception& e) {
    logger_->Log(ERROR_L, "심볼 정보 유효성 검증이 실패했습니다.", __FILE__,
                 __LINE__, true);

    throw runtime_error(e.what());
  }
}

void Engine::IsValidStrategy() {
  try {
    // 전략을 로딩
    if (strategy_ == nullptr) {
      if (const auto& strategy = Strategy::GetStrategy(); strategy != nullptr) {
        strategy_ = strategy;
      } else {
        throw runtime_error(
            "엔진에 전략이 추가되지 않았습니다. "
            "Backtesting::AddStrategy 함수를 호출해 주세요.");
      }
    } else {
      throw runtime_error(
          "전략이 중복 추가되었습니다. 한 백테스팅은 한 개의 전략만 사용할 수 "
          "있습니다.");
    }

    logger_->Log(INFO_L, "전략 유효성 검증이 완료되었습니다.", __FILE__,
                 __LINE__, true);
  } catch (const std::exception& e) {
    logger_->Log(ERROR_L, "전략 유효성 검증이 실패했습니다.", __FILE__,
                 __LINE__, true);

    throw runtime_error(e.what());
  }
}

void Engine::IsValidIndicators() {
  try {
    const auto& strategy_name = strategy_->GetStrategyName();

    // 지표들을 로딩
    set<string> names;
    string duplicate_name;
    for (const auto& indicator : strategy_->GetIndicators()) {
      // 각 전략 내에서 지표들은 같은 이름을 가질 수 없도록 검사
      // 로그 및 성과 분석 시 같은 이름은 분석이 힘들어지므로 원칙적 금지
      if (string name = indicator->GetIndicatorName();
          !names.insert(name).second) {
        duplicate_name = name;
        break;
      }

      // 동일한 이름이 없다면 지표 벡터에 추가
      indicators_.push_back(indicator);
    }

    if (!duplicate_name.empty()) {
      throw runtime_error(format(
          "[{}] 전략 내에서 동일한 이름의 지표 [{}]을(를) 가질 수 없습니다.",
          strategy_name, duplicate_name));
    }

    for (const auto& indicator : indicators_) {
      const auto& indicator_name = indicator->GetIndicatorName();

      // 지표 타임프레임 유효성 검사
      if (const string& timeframe = indicator->GetTimeframe();
          timeframe != "TRADING_TIMEFRAME") {
        try {
          ParseTimeframe(timeframe);
        } catch (const std::exception& e) {
          logger_->Log(ERROR_L,
                       format("[{}] 전략에서 사용하는 [{}] 지표의 타임프레임 "
                              "[{}]이(가) 유효하지 않습니다.",
                              strategy_name, indicator_name, timeframe),
                       __FILE__, __LINE__, true);

          throw runtime_error(e.what());
        }
      }

      // 지표 플롯 타입 검사
      if (const auto& plot_type = indicator->plot_type_;
          plot_type != "Area" && plot_type != "Baseline" &&
          plot_type != "Histogram" && plot_type != "Line" &&
          plot_type != "Null") {
        throw runtime_error(
            format("[{}] 전략에서 사용하는 [{}] 지표의 플롯 타입 "
                   "[{}]이(가) 유효하지 않습니다. "
                   "(가능한 타입: Area, Baseline, Histogram, Line, Null)",
                   strategy_name, indicator_name, plot_type));
      }
    }

    logger_->Log(INFO_L, "지표 유효성 검증이 완료되었습니다.", __FILE__,
                 __LINE__, true);
  } catch (const std::exception& e) {
    logger_->Log(INFO_L, "지표 유효성 검증이 실패했습니다.", __FILE__, __LINE__,
                 true);

    throw runtime_error(e.what());
  }
}

void Engine::InitializeEngine() {
  // 자금 설정
  const auto initial_balance = config_->GetInitialBalance();
  wallet_balance_ = initial_balance;
  available_balance_ = initial_balance;
  max_wallet_balance_ = initial_balance;

  // 돋보기 기능 사용 여부 결정
  use_bar_magnifier_ = *config_->GetUseBarMagnifier();

  // 바 데이터 초기화
  trading_bar_data_ = bar_->GetBarData(TRADING, "");
  if (use_bar_magnifier_) {
    magnifier_bar_data_ = bar_->GetBarData(MAGNIFIER, "");
  }
  reference_bar_data_ = bar_->GetAllReferenceBarData();
  mark_price_bar_data_ = bar_->GetBarData(MARK_PRICE, "");

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
  trading_indices_ = &bar_->GetBarIndices(TRADING, "");
  magnifier_indices_ = &bar_->GetBarIndices(MAGNIFIER, "");
  mark_price_indices_ = &bar_->GetBarIndices(MARK_PRICE, "");

  // 가격 및 가격 타입 캐시 초기화
  price_cache_.resize(trading_bar_num_symbols_);
  price_type_cache_.resize(trading_bar_num_symbols_);

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

  symbol_names_.resize(trading_bar_num_symbols_);
  for (int symbol_idx = 0; symbol_idx < trading_bar_num_symbols_;
       symbol_idx++) {
    symbol_names_[symbol_idx] =
        trading_bar_data_->GetSafeSymbolName(symbol_idx);
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
    const string& symbol_name = symbol_names_[symbol_idx];

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
          int filter_count = 0;  // filter 배열에서 값을 찾은 횟수
          for (const auto& filters = symbol.at("filters");
               const auto& filter : filters) {
            if (filter.at("filterType") == "PRICE_FILTER") {
              const auto price_step = GetDoubleFromJson(filter, "tickSize");

              symbol_info.SetPriceStep(price_step);
              symbol_info.SetPricePrecision(CountDecimalPlaces(price_step));
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
              const auto qty_step = GetDoubleFromJson(filter, "stepSize");

              symbol_info.SetMarketMaxQty(GetDoubleFromJson(filter, "maxQty"))
                  .SetMarketMinQty(GetDoubleFromJson(filter, "minQty"))
                  .SetQtyStep(qty_step)
                  .SetQtyPrecision(CountDecimalPlaces(qty_step));
              filter_count += 3;
              continue;
            }

            if (filter.at("filterType") == "MIN_NOTIONAL") {
              symbol_info.SetMinNotionalValue(
                  GetDoubleFromJson(filter, "notional"));
              filter_count += 1;
            }
          }

          if (filter_count != 7) {
            throw invalid_argument(
                format("[{}] filters의 심볼 정보 중 일부가 존재하지 않습니다.",
                       symbol_name));
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
      logger_->Log(
          ERROR_L,
          format("[{}] 거래소 정보를 초기화하는 중 오류가 발생했습니다.",
                 symbol_name),
          __FILE__, __LINE__, true);

      throw runtime_error(e.what());
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
      logger_->Log(
          ERROR_L,
          format("[{}] 레버리지 구간을 초기화하는 중 오류가 발생했습니다.",
                 symbol_name),
          __FILE__, __LINE__, true);

      throw runtime_error(e.what());
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
            funding_rate.at("fundingRate").get<double>(),
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
        logger_->Log(
            WARN_L,
            format("[{}] 백테스팅 기간 [{} - {}]에 해당되는 펀딩 비율 데이터가 "
                   "존재하지 않으므로 해당 심볼의 펀딩비는 정산되지 않습니다.",
                   symbol_name, UtcTimestampToUtcDatetime(begin_open_time_),
                   UtcTimestampToUtcDatetime(end_close_time_)),
            __FILE__, __LINE__, true);

        funding_rates_indices_[symbol_idx] = SIZE_MAX;
        next_funding_rates_[symbol_idx] = NAN;
        next_funding_times_[symbol_idx] = INT64_MAX;
        next_funding_mark_prices_[symbol_idx] = NAN;
      }

      symbol_info.SetFundingRates(funding_rates_vector);
    } catch (const std::exception& e) {
      logger_->Log(ERROR_L,
                   format("[{}] 펀딩 비율을 초기화하는 중 오류가 발생했습니다.",
                          symbol_name),
                   __FILE__, __LINE__, true);

      throw runtime_error(e.what());
    }

    symbol_info_[symbol_idx] = symbol_info;
  }

  // 심볼 정보 복사
  Analyzer::SetSymbolInfo(symbol_info_);
  BaseOrderHandler::SetSymbolInfo(symbol_info_);
  Slippage::SetSymbolInfo(symbol_info_);

  logger_->Log(INFO_L, "심볼 정보 초기화가 완료되었습니다.", __FILE__, __LINE__,
               true);
}

void Engine::InitializeStrategy() {
  // 주문 핸들러 및 전략 초기화
  Strategy::SetTradingTimeframe(trading_bar_timeframe_);

  order_handler_ = strategy_->GetOrderHandler();
  order_handler_->Initialize(trading_bar_num_symbols_, symbol_names_);
  order_handler_->slippage_->Initialize();
  strategy_->Initialize();

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
      // 종료 플래그 설정
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
      bar_->SetCurrentBarDataType(MAGNIFIER, "");

      // do-while 루프 시작하자마자 시간을 증가시키므로
      // 전 돋보기 바로 시간을 설정
      current_open_time_ = original_open_time - magnifier_bar_time_diff_;
      current_close_time_ = original_open_time - 1;

      vector<int> activated_magnifier_symbol_indices;
      vector<int> symbols_to_remove;

      do {
        // 현재 Open Time과 Close Time을 돋보기 바 하나만큼 증가
        // Open Time은 돋보기 바로 진행 시 정확한 주문, 체결 시간을 얻기 위함
        // Close Time은 마크 가격 바 인덱스를 일치시키기 위함
        current_open_time_ += magnifier_bar_time_diff_;
        current_close_time_ += magnifier_bar_time_diff_;

        for (const auto symbol_idx : activated_symbol_indices_) {
          bar_->SetCurrentSymbolIndex(symbol_idx);
          bar_->ProcessBarIndex(MAGNIFIER, "", symbol_idx, current_close_time_);
          const auto moved_bar_idx = bar_->GetCurrentBarIndex();
          const auto moved_close_time =
              magnifier_bar_data_->GetBar(symbol_idx, moved_bar_idx).close_time;

          if (moved_close_time == current_close_time_) [[likely]] {
            // 정상적으로 Close Time이 일치된 바만 체결 확인
            activated_magnifier_symbol_indices.push_back(symbol_idx);
          } else [[unlikely]] {
            // 체결 확인이 가능한 다음 바의 Open Time 찾기
            if (moved_bar_idx < magnifier_bar_data_->GetNumBars(symbol_idx) - 1)
                [[likely]] {
              // 현재 바가 마지막 바가 아닌 경우 직접 Open Time 가져오기
              const string& magnifier_next_open_time =
                  UtcTimestampToUtcDatetime(
                      magnifier_bar_data_->GetBar(symbol_idx, moved_bar_idx + 1)
                          .open_time);

              logger_->Log(
                  WARN_L,
                  format("[{}] 심볼의 [{}] 돋보기 바가 누락되어 체결 확인을 "
                         "건너뜁니다. (돋보기 바 다음 시간: [{}])",
                         symbol_names_[symbol_idx],
                         UtcTimestampToUtcDatetime(current_open_time_),
                         magnifier_next_open_time),
                  __FILE__, __LINE__, false);

              // 마크 가격 바 인덱스를 현재 돋보기 바 Close Time으로 일치.
              // 펀딩비 데이터에 마크 가격이 누락되었을 경우 시장 마크 가격을
              // 가져와야 하기 때문에 모든 트레이딩 활성화 심볼에 대해 일치
              bar_->ProcessBarIndex(MARK_PRICE, "", symbol_idx,
                                    current_close_time_);
            } else [[unlikely]] {
              // 현재 바가 마지막 바인 경우는 종료
              ExecuteTradingEnd(symbol_idx, "돋보기");

              symbols_to_remove.push_back(symbol_idx);
            }
          }
        }

        // 돋보기 바 데이터 종료로 삭제해야 하는 심볼 제거
        // 진입 주문이 있으면 전 트레이딩 바 종가에서 청산되기 때문에,
        // 시가에서 정산되는 펀딩비 방지를 위해 펀딩비 정산 앞에서 처리
        if (!symbols_to_remove.empty()) [[unlikely]] {
          erase_if(activated_symbol_indices_, [&](const int symbol_idx) {
            return ranges::find(symbols_to_remove, symbol_idx) !=
                   symbols_to_remove.end();
          });

          // 삭제해야하는 심볼 벡터 초기화
          symbols_to_remove.clear();
        }

        // 돋보기 기능 사용 시 돋보기 바 하나마다 펀딩비 확인 후 정산
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
      bar_->SetCurrentBarDataType(TRADING, "");

      for (const auto symbol_idx : activated_symbol_indices_) {
        // 마크 가격 바 인덱스를 현재 트레이딩 바 Close Time으로 일치
        bar_->ProcessBarIndex(MARK_PRICE, "", symbol_idx, current_close_time_);
      }

      // 돋보기 기능 미사용 시 트레이딩 바 하나마다 펀딩비 확인 후 정산
      CheckFundingTime();

      // 해당 트레이딩 바를 진행
      ProcessOhlc(TRADING, activated_symbol_indices_);
    }

    // =========================================================================
    // [종가에서 전략 실행]
    // =========================================================================
    // 활성화된 심볼들의 트레이딩 바에서 전략 실행
    for (const auto symbol_idx : activated_symbol_indices_) {
      ExecuteStrategy(ON_CLOSE, symbol_idx);

      // On Close 전략 실행 후 더 이상 추가 진입/청산이 발생하지 않을 때까지
      // After 전략 실행
      ExecuteChainedAfterStrategies(symbol_idx);
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
    bar_->SetCurrentBarDataType(TRADING, "");
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
            WARN_L,
            format("[{}] 심볼의 [{}] 트레이딩 바가 누락되어 이번 시간의 "
                   "트레이딩을 건너뜁니다. (트레이딩 바 다음 시간: [{}])",
                   symbol_names_[symbol_idx],
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
      bar_->SetCurrentBarDataType(REFERENCE, timeframe);

      if (timeframe == trading_bar_timeframe_) {
        // 참조 바의 타임프레임이 트레이딩 바의 타임프레임과 같을 때
        bar_->ProcessBarIndex(REFERENCE, timeframe, symbol_idx,
                              current_close_time_);

        // 타임프레임이 같으면 같은 데이터이므로,
        // Close Time이 같아졌는지 유효성 검증은 미진행
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
              WARN_L,
              format("[{} {}] 참조 바 데이터가 아직 시작되지 않아 해당 심볼의 "
                     "트레이딩을 진행할 수 없습니다. (참조 바가 시작되는 기준 "
                     "Close Time: [{}])",
                     symbol_names_[symbol_idx], timeframe,
                     UtcTimestampToUtcDatetime(moved_close_time)),
              __FILE__, __LINE__, false);

          bar_->IncreaseBarIndex(TRADING, "", symbol_idx);
          can_use_reference = false;
          break;
        }

        // ※ 2. 하나의 참조 바라도 마지막 참조 바 인덱스 시간부터 참조 바
        //       타임프레임만큼 시간이 지나면 더 이상 참조 바 사용이
        //       불가해지므로 해당 심볼의 트레이딩 종료 (원래 참조 바
        //       타임프레임만큼 시간이 지나면 인덱스가 업데이트 되어야하므로
        //       그때부터 사용 불가해지는 것)
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
      bar_->SetCurrentBarDataType(MAGNIFIER, "");

      bar_->ProcessBarIndex(MAGNIFIER, "", symbol_idx, current_open_time_ - 1);
      const auto moved_bar_idx = bar_->GetCurrentBarIndex();
      const auto moved_close_time =
          magnifier_bar_data_->GetBar(symbol_idx, moved_bar_idx).close_time;

      // ※ 1. 돋보기 바의 데이터가 아직 시작되지 않았으면 트레이딩 불가
      // 한 트레이딩 바 중간부터 돋보기 바가 시작해도 진행 로직에는
      // 문제가 없으므로, 다음 트레이딩 바의 돋보기 시작 시간인
      // current_close_time_부터 문제 발생 (메인 로직 참고)
      if (moved_close_time >= current_close_time_) {
        /* 돋보기 바 데이터가 사용 불가능하면 트레이딩은 불가능하지만,
           트레이딩 바 인덱스는 맞추어 흘러가야 Open Time, Close Time이 동기화
           되므로 인덱스 증가
           ※ 원래 트레이딩 바 인덱스는 활성화된 심볼에서만 증가함 */
        logger_->Log(
            WARN_L,
            format("[{}] 돋보기 바 데이터가 아직 시작되지 않아 해당 심볼의 "
                   "트레이딩을 진행할 수 없습니다. (돋보기 바 시작 시간: [{}])",
                   symbol_names_[symbol_idx],
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
                               const string& bar_data_type_str) {
  // 상태 저장
  const auto original_bar_data_type = bar_->GetCurrentBarDataType();
  const auto& original_reference_timeframe =
      bar_->GetCurrentReferenceTimeframe();

  trading_ended_[symbol_idx] = true;

  // 진입 및 청산 대기 주문을 취소하고,
  // 체결된 진입 주문 잔량을 트레이딩 바 전 종가에 청산
  //
  // 메인 루프 전 트레이딩 바 인덱스를 하나 증가시켰으므로 하나 감소시켜야
  // 마지막 바를 가리킴
  //
  // 단순화를 위해 이 함수의 실행 타이밍, 바 데이터 타입과 관계없이 전 종가 청산
  bar_->SetCurrentBarDataType(TRADING, "");
  bar_->SetCurrentBarIndex(bar_->GetCurrentBarIndex() - 1);

  order_handler_->CancelAll(bar_data_type_str + " 바 데이터 종료");
  order_handler_->CloseAll();

  // 바가 끝난 전량 청산은 Just Exited로 판단하지 않음
  order_handler_->InitializeJustExited();

  logger_->Log(INFO_L,
               format("[{}] 심볼의 {} 바 데이터가 끝나 해당 심볼의 "
                      "백테스팅을 종료합니다.",
                      symbol_names_[symbol_idx], bar_data_type_str),
               __FILE__, __LINE__, true);

  // 상태 복원
  bar_->SetCurrentBarDataType(original_bar_data_type,
                              original_reference_timeframe);
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
      bar_->SetCurrentBarDataType(TRADING, "");
      bar_->SetCurrentBarIndex(bar_->GetCurrentBarIndex() - 1);

      order_handler_->CancelAll("백테스팅 종료 시간");
      order_handler_->CloseAll();

      // 트레이딩이 모두 종료된 전량 청산은 Just Exited로 판단하지 않음
      order_handler_->InitializeJustExited();

      logger_->Log(INFO_L,
                   format("백테스팅 종료 시간에 의해 [{}] 심볼의 "
                          "백테스팅을 종료합니다.",
                          symbol_names_[symbol_idx]),
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
      logger_->Log(WARN_L,
                   format("[{}] 펀딩 비율 데이터가 종료되었으므로 해당 심볼의 "
                          "펀딩비는 더 이상 정산되지 않습니다.",
                          symbol_names_[symbol_idx]),
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
    // → 데이터 종료 시 펀딩 시간이 MAX 값으로 설정되므로 자동으로 조건 미통과
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
                     bar_->GetBarData(bar_->GetCurrentBarDataType(),
                                      bar_->GetCurrentReferenceTimeframe())
                         ->GetBar(symbol_idx, bar_->GetCurrentBarIndex());
                 current_close_time_ == current_market_bar.close_time) {
        // 3. 시장 가격의 Close Time이 현재 진행 시간의 Close Time과 같다면
        //    시장 가격의 Open 가격을 사용
        funding_price = current_market_bar.open;
      } else [[unlikely]] {
        // 4. 모든 일치하는 데이터가 없다면 펀딩비 정산 불가
        order_handler_->LogFormattedInfo(
            WARN_L,
            format("펀딩 시간 [{}] 데이터에 마크 가격이 존재하지 않으며, "
                   "현재 진행 시간 [{}]과 일치하는 마크 가격 바와 시장 가격 "
                   "바가 존재하지 않으므로 펀딩비를 정산할 수 없습니다.",
                   UtcTimestampToUtcDatetime(funding_time),
                   UtcTimestampToUtcDatetime(current_open_time_)),
            __FILE__, __LINE__);

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

void Engine::ProcessOhlc(const BarDataType bar_data_type,
                         const vector<int>& symbol_indices) {
  auto& should_fill_orders = order_handler_->should_fill_orders_;

  // 매커니즘에 따라 확인할 순서대로 가격을 정렬한 벡터 얻기
  const auto&& [mark_price_queue, market_price_queue] =
      GetPriceQueue(bar_data_type, symbol_indices);

  // 순서: 한 가격에서 가격 확인 후 다음 심볼 및 가격으로 넘어감
  // 한 심볼에서 모든 가격 체크 후 다음 가격 체크하면 논리상 시간을 한 번
  // 거슬러 올라가는 것이므로 옳지 않음
  for (int queue_idx = 0; queue_idx < market_price_queue.size(); queue_idx++) {
    const auto& [mark_price, mark_price_type, mark_price_symbol_idx] =
        mark_price_queue[queue_idx];

    const auto& [market_price, market_price_type, market_price_symbol_idx] =
        market_price_queue[queue_idx];

    // 마크 가격과 시장 가격에서 하나의 큐 인덱스의 심볼 인덱스는 동일
    bar_->SetCurrentSymbolIndex(market_price_symbol_idx);

    // 현재 가격 타입이 HIGH 또는 LOW일 경우 CLOSE에서 방향을 계산할 수
    // 있게 하기 위하여 가격과 가격 타입을 캐시
    //
    // 강제 청산의 경우에도, 실제 체결은 시장 가격이므로 시장 가격을
    // 기준으로 캐시하면 됨
    if (market_price_type == HIGH || market_price_type == LOW) {
      price_cache_[market_price_symbol_idx] = market_price;
      price_type_cache_[market_price_symbol_idx] = market_price_type;
    }

    // 체결/강제 청산 해야하는 주문 추가
    order_handler_->CheckLiquidation(bar_data_type, mark_price_symbol_idx,
                                     mark_price, mark_price_type);
    order_handler_->CheckPendingEntries(market_price_symbol_idx, market_price,
                                        market_price_type);
    order_handler_->CheckPendingExits(market_price_symbol_idx, market_price,
                                      market_price_type);

    // 체결/강제 청산 주문이 없는 경우 즉시 다음 가격 확인
    if (should_fill_orders.empty()) {
      continue;
    }

    // 전 가격에서 현재 가격으로 올 때의 방향과 체결 우선 순위에 따라
    // 체결 순서대로 정렬
    // -> 체결은 시장 가격 기준이므로 Market 변수 사용
    SortOrders(should_fill_orders,
               CalculatePriceDirection(bar_data_type, market_price_symbol_idx,
                                       market_price, market_price_type));

    // 정렬된 순서에 따라 주문 체결
    for (const auto& should_fill_order : should_fill_orders) {
      order_handler_->FillOrder(should_fill_order, market_price_symbol_idx,
                                market_price_type);

      // 주문 체결 후 더 이상 진입/청산이 발생하지 않을 때까지 AFTER 전략 실행
      ExecuteChainedAfterStrategies(market_price_symbol_idx);
    }

    // 다음 루프를 위해 벡터 클리어
    should_fill_orders.clear();
  }
}

pair<vector<PriceData>, vector<PriceData>> Engine::GetPriceQueue(
    const BarDataType market_bar_data_type,
    const vector<int>& symbol_indices) const {
  const auto num_symbols = symbol_indices.size();

  // 총 크기를 미리 계산하여 한 번에 할당 (심볼 개수 * OHLC 4개)
  const size_t total_size = num_symbols * 4;
  vector<PriceData> mark_queue(total_size);
  vector<PriceData> market_queue(total_size);

#pragma omp parallel for if (num_symbols > 1)
  for (size_t symbol_order = 0; symbol_order < num_symbols; symbol_order++) {
    // 실제 심볼 인덱스 (1,5,6 등 활성화된 심볼의 인덱스)
    const auto symbol_idx = symbol_indices[symbol_order];

    // 각 바 데이터의 현재 바를 참조
    const auto& original_mark_bar = mark_price_bar_data_->GetBar(
        symbol_idx, (*mark_price_indices_)[symbol_idx]);
    const auto& market_bar =
        market_bar_data_type == TRADING
            ? trading_bar_data_->GetBar(symbol_idx,
                                        (*trading_indices_)[symbol_idx])
            : magnifier_bar_data_->GetBar(symbol_idx,
                                          (*magnifier_indices_)[symbol_idx]);

    // 마크 가격의 Open Time과 시장 가격의 Open Time이 다르다면 시장 가격을
    // 기준으로 강제 청산을 확인
    // (ProcessOhlc 호출 전 Close Time을 일치시키기 때문에 데이터가 존재하다면
    // Open Time도 일치해야 함)
    const auto& mark_bar = original_mark_bar.open_time != market_bar.open_time
                               ? market_bar
                               : original_mark_bar;

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
    mark_queue[symbol_order] = {mark_open, OPEN, symbol_idx};
    market_queue[symbol_order] = {market_open, OPEN, symbol_idx};

    // High/Low1
    mark_queue[num_symbols + symbol_order] =
        mark_low_first ? PriceData{mark_low, LOW, symbol_idx}
                       : PriceData{mark_high, HIGH, symbol_idx};
    market_queue[num_symbols + symbol_order] =
        market_low_first ? PriceData{market_low, LOW, symbol_idx}
                         : PriceData{market_high, HIGH, symbol_idx};

    // High/Low2
    mark_queue[2 * num_symbols + symbol_order] =
        mark_low_first ? PriceData{mark_high, HIGH, symbol_idx}
                       : PriceData{mark_low, LOW, symbol_idx};
    market_queue[2 * num_symbols + symbol_order] =
        market_low_first ? PriceData{market_high, HIGH, symbol_idx}
                         : PriceData{market_low, LOW, symbol_idx};

    // Close
    mark_queue[3 * num_symbols + symbol_order] = {mark_close, CLOSE,
                                                  symbol_idx};
    market_queue[3 * num_symbols + symbol_order] = {market_close, CLOSE,
                                                    symbol_idx};
  }

  return {move(mark_queue), move(market_queue)};
}

Direction Engine::CalculatePriceDirection(
    const BarDataType bar_data_type, const int symbol_idx,
    const double current_price, const PriceType current_price_type) const {
  switch (current_price_type) {
    case OPEN: {
      if (const auto current_bar_idx = bar_->GetCurrentBarIndex();
          current_bar_idx != 0) {
        // 시가의 경우, 전 바의 종가로부터 갭 방향을 탐색
        // 강제 청산의 경우에도, 실제 체결은 시장 가격이므로 시장 가격을
        // 기준으로 방향을 결정하면 됨
        const auto previous_close =
            bar_->GetBarData(bar_data_type,
                             bar_->GetCurrentReferenceTimeframe())
                ->GetBar(symbol_idx, current_bar_idx - 1)
                .close;

        if (IsGreater(current_price, previous_close)) {
          return LONG;
        }

        if (IsLess(current_price, previous_close)) {
          return SHORT;
        }
      }

      // 바 인덱스가 0이여서 전 바가 없거나, 전 바를 참조할 수 없거나,
      // 현재 시가와 전 바의 종가가 같으면 방향 없음
      // (정렬 시 상승으로 가정 → 애초에 이 케이스면 이 가격에서 체결 확률 없음)
      return DIRECTION_NONE;
    }

    case HIGH: {
      // Case 1. Open -> HIGH: 상승
      // Case 2. LOW -> HIGH: 상승
      // 단, 전 가격(Open, Low)과 High가 같다면 방향은 없지만 상승으로 가정
      // 즉, 항상 상승
      return LONG;
    }

    case LOW: {
      // Case 1. OPEN -> LOW: 하락
      // Case 2. HIGH -> LOW: 하락
      // 단, 전 가격(Open, High)과 Low가 같다면 방향은 없지만 하락으로 가정
      // 즉, 항상 하락
      return SHORT;
    }

    case CLOSE: {
      const auto price_cache = price_cache_[symbol_idx];
      const auto price_type_cache = price_type_cache_[symbol_idx];

      if (isnan(price_cache)) [[unlikely]] {
        logger_->Log(ERROR_L, "가격 방향 계산 중 엔진 오류가 발생했습니다.",
                     __FILE__, __LINE__, true);

        throw runtime_error(
            "가격 캐시가 NaN이므로 CLOSE로 오는 가격 방향을 계산할 수 "
            "없습니다.");
      }

      // 전 High/Low와 종가가 같은 경우
      if (IsEqual(price_cache, current_price)) {
        // 전 가격 타입이 HIGH였다면 상승으로 가정
        if (price_type_cache == HIGH) {
          return LONG;
        }

        // 전 가격 타입이 LOW였다면 하락으로 가정
        if (price_type_cache == LOW) {
          return SHORT;
        }
      }

      // 전 가격 타입이 HIGH인 경우 하락 (HIGH -> CLOSE)
      if (price_type_cache == HIGH) {
        return SHORT;
      }

      // 전 가격 타입이 LOW인 경우 상승 (LOW -> CLOSE)
      if (price_type_cache == LOW) {
        return LONG;
      }
    }

    [[unlikely]] default: {
      logger_->Log(ERROR_L, "가격 방향 계산 중 엔진 오류가 발생했습니다.",
                   __FILE__, __LINE__, true);

      throw runtime_error("알 수 없는 가격 유형이 지정되었습니다.");
    }
  }
}

void Engine::SortOrders(vector<FillInfo>& should_fill_orders,
                        const Direction price_direction) {
  auto get_signal_priority = [](const OrderSignal signal) {
    switch (signal) {
      case OrderSignal::LIQUIDATION:
        return 1;  // 최고 우선순위
      case OrderSignal::EXIT:
        return 2;
      case OrderSignal::ENTRY:
        return 3;
      default:
        return 4;
    }
  };

  // 정렬 기준에 따라 정렬
  ranges::stable_sort(should_fill_orders,
                      [price_direction, get_signal_priority](
                          const FillInfo& a, const FillInfo& b) {
                        // fill_price 기본 정렬
                        if (IsDiff(a.fill_price, b.fill_price)) {
                          switch (price_direction) {
                            case LONG:
                              [[fallthrough]];
                            case DIRECTION_NONE: {
                              // LONG: 낮은 가격 -> 높은 가격 순
                              // DIRECTION_NONE: 낮은 가격 -> 높은 가격 순
                              // 가정
                              return IsLess(a.fill_price, b.fill_price);
                            }

                            case SHORT: {
                              // SHORT: 높은 가격 -> 낮은 가격 순
                              return IsGreater(a.fill_price, b.fill_price);
                            }

                            default:
                              return IsLess(a.fill_price, b.fill_price);
                          }
                        }

                        // fill_price가 같은 경우,
                        // 주문 시그널 우선 순위에 따라 정렬
                        if (a.order_signal != b.order_signal) {
                          // 우선순위
                          // 1) 강제 청산(LIQUIDATION)
                          // 2) 청산(EXIT)
                          // 3) 진입(ENTRY)
                          return get_signal_priority(a.order_signal) <
                                 get_signal_priority(b.order_signal);
                        }

                        // fill_price와 order_signal이 같은 경우,
                        // stable_sort이기 때문에 기존 should_fill_orders의
                        // 순서대로 유지됨
                        // → 기존 Pending/Filled Vector에서 먼저 주문/체결된
                        //   것이 먼저 체결됨
                        return false;
                      });
}

void Engine::ExecuteStrategy(const StrategyType strategy_type,
                             const int symbol_idx) {
  // 종가 전략 실행인 경우 원본 바 데이터 유형은 트레이딩 바
  //
  // ProcessOhlc에서 전략 실행인 경우 원본 바 데이터 유형은
  // 트레이딩 바 혹은 돋보기 바

  // 상태 저장
  const auto original_bar_data_type = bar_->GetCurrentBarDataType();
  const auto& original_reference_timeframe =
      bar_->GetCurrentReferenceTimeframe();

  // 트레이딩 바의 지정된 심볼에서 전략 실행
  // 돋보기 바에서 AFTER EXIT, AFTER ENTRY 전략이 실행되더라도
  // 전략은 트레이딩 바의 종가 기준으로 하는 것이 지표 참조 측면에서 올바름
  //
  // 예를 들어, 1시간 TRADING, 1분 MAGNIFIER라고 했을 때, high[0]했는데
  // 1분의 high 값을 얻으면 안 되므로 TRADING으로 설정하는 것
  // 단, 이러한 설정 때문에 미래 값 참조를 방지하기 위하여 AFTER 전략에서는
  // [0]으로 참조할 수 없음
  bar_->SetCurrentBarDataType(TRADING, "");
  bar_->SetCurrentSymbolIndex(symbol_idx);

  // 현재 심볼의 포지션 사이즈 업데이트
  order_handler_->UpdateCurrentPositionSize(symbol_idx);

  current_strategy_type_ = strategy_type;

  switch (strategy_type) {
    case ON_CLOSE: {
      strategy_->ExecuteOnClose();
      break;
    }

    case AFTER_EXIT: {
      strategy_->ExecuteAfterExit();
      break;
    }

    case AFTER_ENTRY: {
      strategy_->ExecuteAfterEntry();
      break;
    }
  }

  // 상태 복원
  bar_->SetCurrentBarDataType(original_bar_data_type,
                              original_reference_timeframe);
}

void Engine::ExecuteChainedAfterStrategies(const int symbol_idx) {
  bool just_exited = false;
  bool just_entered = false;

  // 더 이상 추가 진입/청산이 발생하지 않을 때까지 AFTER 전략 실행
  while (((just_exited = order_handler_->IsJustExited())) ||
         ((just_entered = order_handler_->IsJustEntered()))) {
    // 1. 청산이 존재했다면 After Exit 전략 실행 (강제 청산 포함)
    if (just_exited) {
      order_handler_->InitializeJustExited();
      ExecuteStrategy(AFTER_EXIT, symbol_idx);
    }

    // 2. 진입이 존재했다면 After Entry 전략 실행
    if (just_entered) {
      order_handler_->InitializeJustEntered();
      ExecuteStrategy(AFTER_ENTRY, symbol_idx);
    }
  }
}

}  // namespace backtesting::engine
