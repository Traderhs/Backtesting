// 표준 라이브러리
#include <array>
#include <cmath>
#include <filesystem>
#include <format>
#include <ranges>
#include <set>
#include <utility>

// 파일 헤더
#include "Engines/Engine.hpp"

// 내부 헤더
#include "Engines/Analyzer.hpp"
#include "Engines/BarData.hpp"
#include "Engines/BarHandler.hpp"
#include "Engines/BaseBarHandler.hpp"
#include "Engines/DataUtils.hpp"
#include "Engines/Exception.hpp"
#include "Engines/OrderHandler.hpp"
#include "Engines/Strategy.hpp"
#include "Engines/TimeUtils.hpp"

// 네임 스페이스
using namespace data_utils;
using namespace time_utils;
using namespace std;
using enum BarType;
using enum PriceType;
using enum LogLevel;

Engine::Engine()
    : trading_bar_num_symbols_(0),
      trading_bar_add_time_(0),
      use_bar_magnifier_(false),
      current_strategy_type_(StrategyType::ON_CLOSE),
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

void Engine::Backtesting(const bool use_bar_magnifier, const string& start,
                         const string& end, const string& format) {
  PrintSeparator();
  Initialize(use_bar_magnifier, start, end, format);

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

  // 설정, 지표값 등 저장 필요@@@@@@@@@@@@@@@@@@@@@@@@@

  PrintSeparator();
  logger_->Log(INFO_L, "백테스팅이 완료되었습니다.", __FILE__, __LINE__);
}

double Engine::UpdateAvailableBalance() {
  unrealized_pnl_ = 0;

  // 전략별 미실현 손익을 합산
  for (const auto& strategy : strategies_) {
    unrealized_pnl_ += strategy->GetOrderHandler()->GetUnrealizedPnl();
  }

  // 사용 가능 자금 업데이트
  available_balance_ = wallet_balance_ + unrealized_pnl_ - used_margin_;

  return available_balance_;
}

void Engine::SetCurrentStrategyType(const StrategyType strategy_type) {
  current_strategy_type_ = strategy_type;
}

size_t Engine::GetMaxDecimalPlace(const int symbol_idx) const {
  return max_decimal_places_[symbol_idx];
}

string Engine::GetCurrentStrategyName() const { return current_strategy_name_; }
StrategyType Engine::GetCurrentStrategyType() const {
  return current_strategy_type_;
}
int64_t Engine::GetCurrentOpenTime() const { return current_open_time_; }

void Engine::Initialize(const bool use_bar_magnifier, const string& start,
                        const string& end, const string& format) {
  // 유효성 검증
  IsValidBarData(use_bar_magnifier);
  IsValidDateRange(start, end, format);
  IsValidConfig();
  IsValidStrategies();
  IsValidIndicators();

  // 폴더 생성
  CreateDirectories();

  // 초기화
  PrintSeparator();
  InitializeEngine(use_bar_magnifier);
  InitializeStrategies();
  InitializeIndicators();
}

void Engine::IsValidBarData(const bool use_bar_magnifier) const {
  const auto& trading_bar = bar_->GetBarData(TRADING);
  const auto trading_num_symbols = trading_bar.GetNumSymbols();

  // 1.1. 트레이딩 바 데이터가 비었는지 검증
  if (!trading_num_symbols)
    Logger::LogAndThrowError("트레이딩 바 데이터가 비어있습니다.", __FILE__,
                             __LINE__);

  // 1.2. 트레이딩 바 데이터의 중복 가능성 검증
  // set은 중복 불가능하므로 set에 추가한 open 개수가 심볼 개수와 다르다면
  // 중복 추가 가능성 높음
  if (check_duplicate_data_) {
    set<double> trading_bar_open;
    for (int symbol_idx = 0; symbol_idx < trading_num_symbols; symbol_idx++) {
      trading_bar_open.insert(trading_bar.GetBar(symbol_idx, 0).open);
    }

    if (trading_bar_open.size() != trading_num_symbols) {
      logger_->Log(
          ERROR_L,
          "트레이딩 바 데이터에 중복된 데이터가 다른 심볼로 추가되었을 "
          "가능성이 있습니다.",
          __FILE__, __LINE__);
      Logger::LogAndThrowError(
          "중복된 데이터가 없는 것이 확실하다면 NoDuplicateDataCheck 함수를 "
          "호출해 주세요.",
          __FILE__, __LINE__);
    }
  }

  if (use_bar_magnifier) {
    const auto& magnifier_bar = bar_->GetBarData(MAGNIFIER);
    const auto magnifier_num_symbols = magnifier_bar.GetNumSymbols();

    /* 2.1. 트레이딩 바 데이터의 심볼 개수와 돋보기 바 데이터의
            심볼 개수가 같은지 검증 */
    if (magnifier_num_symbols != trading_num_symbols) {
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
      if (const auto& symbol_name = trading_bar.GetSymbolName(symbol_idx);
          symbol_name != magnifier_bar.GetSymbolName(symbol_idx)) {
        Logger::LogAndThrowError(
            format("돋보기 바 데이터에 {}이(가) 존재하지 않거나 "
                   "트레이딩 바에 추가된 심볼 순서와 일치하지 않습니다.",
                   symbol_name),
            __FILE__, __LINE__);
      }
    }

    // 2.3. 돋보기 바 데이터의 중복 가능성 검증
    // set은 중복 불가능하므로 set에 추가한 open 개수가 심볼 개수와 다르다면
    // 중복 추가 가능성 높음
    if (check_duplicate_data_) {
      set<double> magnifier_open;
      for (int symbol_idx = 0; symbol_idx < magnifier_num_symbols;
           symbol_idx++) {
        magnifier_open.insert(magnifier_bar.GetBar(symbol_idx, 0).open);
      }

      if (magnifier_open.size() != magnifier_num_symbols) {
        logger_->Log(
            ERROR_L,
            "돋보기 바 데이터에 중복된 데이터가 다른 심볼로 추가되었을 "
            "가능성이 있습니다.",
            __FILE__, __LINE__);
        Logger::LogAndThrowError(
            "중복된 데이터가 없는 것이 확실하다면 NoDuplicateDataCheck "
            "함수를 호출해 주세요.",
            __FILE__, __LINE__);
      }
    }
  }

  for (const auto& [reference_timeframe, reference_bar] :
       bar_->GetAllReferenceBarData()) {
    const auto reference_num_symbols = reference_bar.GetNumSymbols();

    /* ※ 참조 바 데이터의 심볼 개수와 순서 검증은 추후 트레이딩 바 심볼 외
          다른 데이터(경제 지표 등)의 참조가 필요할 때 삭제 */

    /* 3.1. 트레이딩 바 데이터의 심볼 개수와 참조 바 데이터의
            심볼 개수가 같은지 검증 */
    if (reference_num_symbols != trading_num_symbols) {
      Logger::LogAndThrowError(
          format("트레이딩 바 데이터에 추가된 심볼 개수({}개)와 참조 바 "
                 "데이터 {}에 추가된 심볼 개수({}개)는 동일해야 합니다.",
                 trading_num_symbols, reference_timeframe,
                 reference_num_symbols),
          __FILE__, __LINE__);
    }

    /* 3.2. 트레이딩 바 데이터의 심볼들이 참조 바 데이터에 존재하고
            순서가 같은지 검증 */
    for (int symbol_idx = 0; symbol_idx < trading_num_symbols; ++symbol_idx) {
      if (const auto& symbol_name = trading_bar.GetSymbolName(symbol_idx);
          symbol_name != reference_bar.GetSymbolName(symbol_idx)) {
        Logger::LogAndThrowError(
            format("참조 바 데이터 {}에 {}이(가) 존재하지 않거나 "
                   "트레이딩 바에 추가된 심볼 순서와 일치하지 않습니다.",
                   reference_timeframe, symbol_name),
            __FILE__, __LINE__);
      }
    }

    // 3.3. 참조 바 데이터의 중복 가능성 검증
    // set은 중복 불가능하므로 set에 추가한 open 개수가 심볼 개수와 다르다면
    // 중복 추가 가능성 높음
    if (check_duplicate_data_) {
      set<double> reference_open;
      for (int symbol_idx = 0; symbol_idx < reference_num_symbols;
           symbol_idx++) {
        reference_open.insert(reference_bar.GetBar(symbol_idx, 0).open);
      }

      if (reference_open.size() != reference_num_symbols) {
        logger_->Log(
            ERROR_L,
            format("참조 바 데이터 {}에 중복된 데이터가 다른 심볼로 추가되었을 "
                   "가능성이 있습니다.",
                   reference_timeframe),
            __FILE__, __LINE__);
        Logger::LogAndThrowError(
            "중복된 데이터가 없는 것이 확실하다면 NoDuplicateDataCheck "
            "함수를 호출해 주세요.",
            __FILE__, __LINE__);
      }
    }
  }

  logger_->Log(INFO_L, "바 데이터 유효성 검증이 완료되었습니다.", __FILE__,
               __LINE__);
}

void Engine::IsValidDateRange(const string& start, const string& end,
                              const string& format) {
  const auto& trading_bar = bar_->GetBarData(TRADING);
  for (int symbol_idx = 0; symbol_idx < trading_bar.GetNumSymbols();
       symbol_idx++) {
    // 백테스팅 시작 시 가장 처음의 Open Time 값 구하기
    begin_open_time_ =
        min(begin_open_time_, trading_bar.GetBar(symbol_idx, 0).open_time);

    // 백테스팅 시작 시 가장 끝의 Open Time 값 구하기
    end_open_time_ = max(
        end_open_time_,
        trading_bar.GetBar(symbol_idx, trading_bar.GetNumBars(symbol_idx) - 1)
            .open_time);
  }

  // Start가 지정된 경우 범위 체크
  if (!start.empty()) {
    if (const auto start_time = UtcDatetimeToUtcTimestamp(start, format);
        start_time < begin_open_time_) {
      Logger::LogAndThrowError(
          std::format("지정된 Start 시간 {}은(는) 최소 시간 {}의 "
                      "전으로 지정할 수 없습니다.",
                      start, UtcTimestampToUtcDatetime(begin_open_time_)),
          __FILE__, __LINE__);
    } else {
      begin_open_time_ = start_time;
    }
  }

  // End가 지정된 경우 범위 체크
  if (!end.empty()) {
    if (const auto end_time = UtcDatetimeToUtcTimestamp(end, format);
        end_time > end_open_time_) {
      Logger::LogAndThrowError(
          std::format("지정된 End 시간 {}은(는) 최대 시간 {}의 "
                      "후로 지정할 수 없습니다.",
                      end, UtcTimestampToUtcDatetime(end_open_time_)),
          __FILE__, __LINE__);
    } else {
      end_open_time_ = end_time;
    }
  }

  // Start, End가 둘다 지정된 경우 범위 체크
  if (!start.empty() && !end.empty()) {
    if (UtcDatetimeToUtcTimestamp(start, format) >
        UtcDatetimeToUtcTimestamp(end, format)) {
      Logger::LogAndThrowError(
          std::format("지정된 Start 시간 {}은(는) 지정된 End 시간 {}의 전으로 "
                      "지정할 수 없습니다.",
                      start, end),
          __FILE__, __LINE__);
    }
  }

  logger_->Log(INFO_L, "날짜 유효성 검증이 완료되었습니다.", __FILE__,
               __LINE__);
}

void Engine::IsValidConfig() const {
  const auto& root_directory = config_.GetRootDirectory();
  const auto initial_balance = config_.GetInitialBalance();
  const auto commission_type = config_.GetCommissionType();
  const auto market_commission = config_.GetMarketCommission();
  const auto limit_commission = config_.GetLimitCommission();
  const auto slippage_type = config_.GetSlippageType();
  const auto market_slippage = config_.GetMarketSlippage();
  const auto limit_slippage = config_.GetLimitSlippage();

  if (root_directory.empty() || isnan(initial_balance) ||
      commission_type == CommissionType::COMMISSION_NONE ||
      isnan(market_commission) || isnan(limit_commission) ||
      slippage_type == SlippageType::SLIPPAGE_NONE || isnan(market_slippage) ||
      isnan(limit_slippage)) {
    Logger::LogAndThrowError("엔진 설정값을 모두 초기화해야 합니다.", __FILE__,
                             __LINE__);
  }

  if (!filesystem::exists(root_directory)) {
    Logger::LogAndThrowError(
        format("지정된 루트 폴더 {}은(는) 유효하지 않습니다.", root_directory),
        __FILE__, __LINE__);
  }

  if (IsGreater(market_commission, 100.0) || IsLess(market_commission, 0.0)) {
    Logger::LogAndThrowError(
        format("지정된 시장가 수수료율 {}%는 100% 초과 혹은 "
               "0% 미만으로 설정할 수 없습니다.",
               market_commission),
        __FILE__, __LINE__);
  }

  if (IsGreater(limit_commission, 100.0) || IsLess(limit_commission, 0.0)) {
    Logger::LogAndThrowError(
        format("지정된 지정가 수수료율 {}%는 100% 초과 혹은 "
               "0% 미만으로 설정할 수 없습니다.",
               limit_commission),
        __FILE__, __LINE__);
  }

  if (IsGreater(market_slippage, 100.0) || IsLess(market_slippage, 0.0)) {
    Logger::LogAndThrowError(
        format("지정된 시장가 슬리피지율 {}%는 100% 초과 혹은 "
               "0% 미만으로 설정할 수 없습니다.",
               market_slippage),
        __FILE__, __LINE__);
  }

  if (IsGreater(limit_slippage, 100.0) || IsLess(limit_slippage, 0.0)) {
    Logger::LogAndThrowError(
        format("지정된 지정가 슬리피지율 {}%는 100% 초과 혹은 "
               "0% 미만으로 설정할 수 없습니다.",
               limit_slippage),
        __FILE__, __LINE__);
  }

  logger_->Log(INFO_L, "엔진 설정값 유효성 검증이 완료되었습니다.", __FILE__,
               __LINE__);
}

void Engine::IsValidStrategies() {
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
    Logger::LogAndThrowError("엔진에 전략이 추가되지 않았습니다.", __FILE__,
                             __LINE__);
  }

  logger_->Log(INFO_L, "전략 유효성 검증이 완료되었습니다.", __FILE__,
               __LINE__);
}

void Engine::IsValidIndicators() {
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

  // 타임프레임 유효성 검사
  for (int strategy_idx = 0; strategy_idx < strategies_.size();
       strategy_idx++) {
    for (const auto& indicator : indicators_[strategy_idx]) {
      const string& timeframe = indicator->GetTimeframe();
      try {
        ParseTimeframe(timeframe);
      } catch ([[maybe_unused]] const exception& e) {
        // 지표에서 trading_timeframe을 변수를 사용하면 아직 초기화 전이기
        // 때문에 TRADING_TIMEFRAME으로 지정되어 있는데 이 경우는 유효한
        // 타임프레임이기 때문에 넘어감
        if (timeframe == "TRADING_TIMEFRAME") {
          continue;
        }

        logger_->Log(ERROR_L,
                     format("[{}] 전략에서 사용하는 [{}] 지표의 타임프레임 "
                            "[{}]이(가) 유효하지 않습니다.",
                            strategies_[strategy_idx]->GetName(),
                            indicator->GetName(), indicator->GetTimeframe()),
                     __FILE__, __LINE__);
      }
      throw;
    }
  }

  logger_->Log(INFO_L, "지표 유효성 검증이 완료되었습니다.", __FILE__,
               __LINE__);
}

void Engine::CreateDirectories() {
  try {
    // 전략 이름들을 이어붙인 이름 + 현재 시간이 이번 백테스팅의 메인 폴더
    string main_directory = config_.GetRootDirectory() + "/Results/";

    for (const auto& strategy : strategies_) {
      main_directory += strategy->GetName() + "_";
    }

    main_directory += GetCurrentLocalDatetime();

    // 시간 구분 문자 제거 및 공백 언더 스코어화
    // (3부터 시작하는 이유는 드라이브 경로의 ':' 제외)
    main_directory.erase(
        std::remove(main_directory.begin() + 3, main_directory.end(), ':'),
        main_directory.end());
    main_directory.erase(
        std::remove(main_directory.begin() + 3, main_directory.end(), '-'),
        main_directory.end());
    replace(main_directory.begin() + 3, main_directory.end(), ' ', '_');

    // 메인 폴더 생성
    filesystem::create_directory(main_directory);
    main_directory_ = main_directory;

    // 지표 저장 폴더 생성
    for (const auto& strategy : strategies_) {
      filesystem::create_directories(main_directory + "/Indicators/" +
                                     strategy->GetName());
    }
  } catch (const exception& e) {
    logger->Log(ERROR_L, e.what(), __FILE__, __LINE__);
    Logger::LogAndThrowError("폴더 생성 중 에러가 발생했습니다.", __FILE__,
                             __LINE__);
  }
}

void Engine::InitializeEngine(const bool use_bar_magnifier) {
  // 자금 설정
  const auto initial_balance = config_.GetInitialBalance();
  wallet_balance_ = initial_balance;
  available_balance_ = initial_balance;
  max_wallet_balance_ = initial_balance;

  // 바 데이터 초기화
  trading_bar_ = make_shared<BarData>(bar_->GetBarData(TRADING));
  magnifier_bar_ = make_shared<BarData>(bar_->GetBarData(MAGNIFIER));
  reference_bar_ = make_shared<unordered_map<string, BarData>>(
      bar_->GetAllReferenceBarData());

  // 바 데이터 정보 초기화
  trading_bar_num_symbols_ = trading_bar_->GetNumSymbols();
  trading_bar_add_time_ = ParseTimeframe(trading_bar_->GetTimeframe());
  trading_bar_timeframe_ = trading_bar_->GetTimeframe();

  // 각 심볼의 최대 소숫점 자리수 초기화
  max_decimal_places_.resize(trading_bar_num_symbols_);
  for (int symbol_idx = 0; symbol_idx < trading_bar_num_symbols_;
       symbol_idx++) {
    max_decimal_places_[symbol_idx] = CountMaxDecimalPlace(symbol_idx);
  }

  // 트레이딩 시간 정보 초기화
  current_open_time_ = begin_open_time_;
  current_close_time_ = begin_open_time_ + trading_bar_add_time_ - 1;

  // 시작 시간까지 트레이딩 바의 인덱스를 이동
  bar_->ProcessBarIndices(TRADING, "NONE", current_close_time_);

  // trading_began_, trading_ended 초기화
  trading_began_.resize(trading_bar_num_symbols_);
  trading_ended_.resize(trading_bar_num_symbols_);

  for (int symbol_idx = 0; symbol_idx < trading_bar_num_symbols_;
       symbol_idx++) {
    bar_->SetCurrentSymbolIndex(symbol_idx);

    // 첫 시작 시간이 begin_open_time과 같다면 바로 시작하는 Symbol
    if (trading_bar_->GetBar(symbol_idx, bar_->GetCurrentBarIndex())
            .open_time == begin_open_time_) {
      trading_began_[symbol_idx] = true;
    } else {
      trading_began_[symbol_idx] = false;
    }

    trading_ended_[symbol_idx] = false;
  }

  // 활성화된 심볼들 초기화
  activated_symbol_indices_.resize(trading_bar_num_symbols_);
  activated_magnifier_symbol_indices_.resize(trading_bar_num_symbols_);
  activated_magnifier_bar_indices_.resize(trading_bar_num_symbols_);
  activated_trading_symbol_indices_.resize(trading_bar_num_symbols_);
  activated_trading_bar_indices_.resize(trading_bar_num_symbols_);

  // 돋보기 기능 사용 여부 결정
  if (use_bar_magnifier) {
    use_bar_magnifier_ = true;
  }

  // 분석기 초기화
  analyzer_->Initialize(initial_balance);

  engine_initialized_ = true;
  logger->Log(INFO_L, "엔진 초기화가 완료되었습니다.", __FILE__, __LINE__);
}

void Engine::InitializeStrategies() const {
  // 전략별 주문 핸들러 및 전략 초기화
  for (const auto& strategy : strategies_) {
    strategy->GetOrderHandler()->Initialize(trading_bar_num_symbols_);
    strategy->Initialize();
  }

  Strategy::SetTradingTimeframe(trading_bar_timeframe_);

  logger->Log(INFO_L, "전략 초기화가 완료되었습니다.", __FILE__, __LINE__);
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

    // 지표 저장
    for (const auto& indicator : indicators_[strategy_idx]) {
      indicator->SaveIndicator(
          format("{}/Indicators/{}/{} {}.csv", main_directory_, strategy_name,
                 indicator->GetName(), indicator->GetTimeframe()),
          strategy_name);
    }
  }

  logger->Log(INFO_L, "지표 초기화가 완료되었습니다.", __FILE__, __LINE__);
}

void Engine::BacktestingMain() {
  while (true) {
    // 현재 바 인덱스에서 트레이딩을 진행하는지 상태를 업데이트
    UpdateTradingStatus();

    // 트레이딩이 모두 끝났으면 백테스팅 끝
    if (IsBacktestingEnd()) {
      return;
    }

    logger_->Log(
        INFO_L,
        format("진행 시간: {}", UtcTimestampToUtcDatetime(current_open_time_)),
        __FILE__, __LINE__);

    // 현재 바 인덱스의 트레이딩을 진행
    // 돋보기 바에 활성화된 심볼이 있으면 돋보기 바를 이용하여 백테스팅
    const int64_t original_open_time = current_open_time_;
    if (!activated_magnifier_symbol_indices_.empty()) {
      bar_->SetCurrentBarType(MAGNIFIER, "NONE");

      do {
        // 활성화된 돋보기 바 인덱스 증가
        for (const auto& symbol_idx : activated_magnifier_symbol_indices_) {
          activated_magnifier_bar_indices_.push_back(
              bar_->IncreaseBarIndex(MAGNIFIER, "NONE", symbol_idx));
        }

        // 현재 Open Time을 현재 돋보기 바의 Open Time으로 업데이트
        // 돋보기 바로 진행 시 정확한 주문, 체결 시간을 얻기 위함
        current_open_time_ =
            magnifier_bar_
                ->GetBar(activated_magnifier_symbol_indices_[0],
                         activated_magnifier_bar_indices_[0])
                .open_time;

        // 해당 돋보기 바를 진행
        ProcessOhlc(activated_magnifier_symbol_indices_,
                    activated_magnifier_bar_indices_);

        // 바 돋보기의 Close Time이 트레이딩 바의 Close Time과
        // 같아질 때까지 진행
      } while (magnifier_bar_
                   ->GetBar(activated_magnifier_symbol_indices_[0],
                            activated_magnifier_bar_indices_[0])
                   .close_time < current_close_time_);
    }
    current_open_time_ = original_open_time;

    // 트레이딩 바에 활성화된 심볼이 있으면 트레이딩 바를 이용하여 백테스팅
    // 1. 돋보기 바 자체를 미사용 2. 아직 돋보기 바 사용 불가능
    if (!activated_trading_symbol_indices_.empty()) {
      bar_->SetCurrentBarType(TRADING, "NONE");

      const auto& bar_indices = bar_->GetBarIndices(TRADING, "NONE");
      for (const auto& symbol_idx : activated_trading_symbol_indices_) {
        activated_trading_bar_indices_.push_back(bar_indices[symbol_idx]);
      }

      ProcessOhlc(activated_trading_symbol_indices_,
                  activated_trading_bar_indices_);
    }

    // =======================================================================
    // 활성화된 심볼들의 트레이딩 바에서 전략 실행
    for (const auto& strategy : strategies_) {
      const auto& order_handler = strategy->GetOrderHandler();

      for (const auto symbol_idx : activated_symbol_indices_) {
        ExecuteStrategy(strategy, StrategyType::ON_CLOSE, symbol_idx);

        bool just_entered = false;
        bool just_exited = false;
        do {
          // On Close 전략 실행 후 진입이 있었을 경우 After Entry 전략 실행
          if (order_handler->GetJustEntered()) {
            order_handler->InitializeJustEntered();
            ExecuteStrategy(strategy, StrategyType::AFTER_ENTRY, symbol_idx);

            // After Entry 전략 실행 시 추가 진입 혹은 청산 가능성이 있으므로
            // 상태를 다시 업데이트
            just_entered = order_handler->GetJustEntered();
            just_exited = order_handler->GetJustExited();
          }

          // On Close 전략 실행 후 청산이 있었을 경우 After Exit 전략 실행
          if (order_handler->GetJustExited()) {
            order_handler->InitializeJustExited();
            ExecuteStrategy(strategy, StrategyType::AFTER_EXIT, symbol_idx);

            // After Exit 전략 실행 시 추가 진입 혹은 청산 가능성이 있으므로
            // 상태를 다시 업데이트
            just_entered = order_handler->GetJustEntered();
            just_exited = order_handler->GetJustExited();
          }

          // 진입 및 청산 체결이 없는 경우 전략 실행 종료
        } while (just_entered || just_exited);
      }
    }

    // =======================================================================
    // 활성화된 심볼들의 트레이딩 바 인덱스 증가
    for (const auto symbol_idx : activated_symbol_indices_) {
      bar_->IncreaseBarIndex(TRADING, "NONE", symbol_idx);
    }

    /* current_open_time_ 업데이트:
       UpdateTradingStatus에서 트레이딩 시작 검증 시 사용

       current_close_time_ 업데이트:
       UpdateTradingStatus에서 현재 트레이딩 바 Close Time까지 바 돋보기를
       사용할 수 있는지 검증하기 위하여 사용 */
    current_open_time_ += trading_bar_add_time_;
    current_close_time_ += trading_bar_add_time_;
  }
}

size_t Engine::CountMaxDecimalPlace(const int symbol_idx) const {
  size_t decimal_place = 0;

  const auto num_bars = trading_bar_->GetNumBars(symbol_idx);
  for (int bar_idx = 0; bar_idx < 10; bar_idx++) {
    try {
      // 앞에서 10개 바를 체크
      const auto& front_bar = trading_bar_->SafeGetBar(symbol_idx, bar_idx);
      decimal_place = max(decimal_place, CountDecimalPlaces(front_bar.open));
      decimal_place = max(decimal_place, CountDecimalPlaces(front_bar.high));
      decimal_place = max(decimal_place, CountDecimalPlaces(front_bar.low));
      decimal_place = max(decimal_place, CountDecimalPlaces(front_bar.close));

      // 뒤에서 10개 바를 체크
      const auto& back_bar =
          trading_bar_->SafeGetBar(symbol_idx, num_bars - 1 - bar_idx);
      decimal_place = max(decimal_place, CountDecimalPlaces(back_bar.open));
      decimal_place = max(decimal_place, CountDecimalPlaces(back_bar.high));
      decimal_place = max(decimal_place, CountDecimalPlaces(back_bar.low));
      decimal_place = max(decimal_place, CountDecimalPlaces(back_bar.close));
    } catch ([[maybe_unused]] const IndexOutOfRange& e) {
      continue;
    }
  }

  return decimal_place;
}

void Engine::UpdateTradingStatus() {
  // 활성화된 벡터 초기화
  ClearActivatedVectors();

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
        trading_bar_->IsValidIndex(symbol_idx, bar_idx);
      } catch ([[maybe_unused]] const IndexOutOfRange& e) {
        // 해당 심볼의 데이터의 끝까지 진행했다면 해당 심볼은 트레이딩 종료
        ExecuteTradingEnd(symbol_idx);
        continue;
      }

      // 트레이딩 바 데이터 결손이 있으면 트레이딩 불가
      if (current_open_time_ !=
          trading_bar_->GetBar(symbol_idx, bar_idx).open_time) {
        continue;
      }

      // 진행할 바가 남아있고 트레이딩 바 데이터 결손이 없으면 트레이딩 진행
      DetermineActivation(symbol_idx);
    } else {
      // 트레이딩을 시작하지 않은 심볼은 이번 바에서 시작했는지 검사
      if (const auto current_open_time =
              trading_bar_->GetBar(symbol_idx, bar_idx).open_time;
          current_open_time == current_open_time_) {
        trading_began_[symbol_idx] = true;

        DetermineActivation(symbol_idx);
      }
    }
  }
}

void Engine::ClearActivatedVectors() {
  activated_symbol_indices_.clear();
  activated_trading_symbol_indices_.clear();
  activated_trading_bar_indices_.clear();
  activated_magnifier_symbol_indices_.clear();
  activated_magnifier_bar_indices_.clear();
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
  }

  logger_->Log(
      ORDER_L,
      format(
          "[{}]의 트레이딩 바 데이터가 끝나 해당 심볼의 백테스팅을 종료합니다.",
          trading_bar_->GetSymbolName(symbol_idx)),
      __FILE__, __LINE__);
}

bool Engine::IsBacktestingEnd() const {
  // 한 심볼이라도 끝나지 않았으면 끝나지 않음
  return ranges::all_of(trading_ended_,
                        [](const bool is_end) { return is_end; });
}

void Engine::DetermineActivation(const int symbol_idx) {
  // 트레이딩 전 바의 Close Time
  // 타임스탬프이므로 current_open_time - 1 == previous_close_time
  const auto prev_close_time = current_open_time_ - 1;

  /* 참조 바를 사용 가능한지 검증
     1. 트레이딩 바의 타임프레임과 참조 바의 타임프레임이 같으면 바 인덱스를
        무조건 일치
     2. 참조 바의 타임프레임이 더 크다면 트레이딩 바의 Close Time이
        참조 바의 Close Time을 지난 다음 바부터 해당 참조 바 인덱스를 참조 가능
     2.1. 트레이딩 전 바 Close Time보다 참조 바의 Close Time이 작으면
          트레이딩 전 바 Close Time까지 최대한 증가 후 사용 가능.
     2.2. 트레이딩 전 바 Close Time과 참조 바의 Close Time이 같으면
          사용 가능
     2.3. 트레이딩 전 바 Close Time보다 참조 바의 Close Time이 같거나 크면
          아직 사용 불가능하므로 트레이딩 불가                         */
  for (const auto& [timeframe, reference_bar] : *reference_bar_) {
    bar_->SetCurrentBarType(REFERENCE, timeframe);

    if (timeframe == trading_bar_timeframe_) {
      bar_->ProcessBarIndex(symbol_idx, REFERENCE, timeframe,
                            current_close_time_);
    } else {
      if (reference_bar.GetBar(symbol_idx, bar_->GetCurrentBarIndex())
              .close_time <= prev_close_time) {
        bar_->ProcessBarIndex(symbol_idx, REFERENCE, timeframe,
                              prev_close_time);
      } else {
        // 트레이딩 전 바 Close Time보다 참조 바의 Close Time이 크면
        // 참조 불가능하므로 트레이딩 불가
        // 하지만 트레이딩 바는 인덱스를 맞추어 흘러가야 하므로 인덱스 증가
        // * 원래 트레이딩 바 인덱스는 활성화된 심볼에서만 증가함
        bar_->IncreaseBarIndex(TRADING, "NONE", symbol_idx);
        return;
      }
    }
  }

  if (use_bar_magnifier_) {
    bar_->SetCurrentBarType(MAGNIFIER, "NONE");

    int64_t magnifier_close_time =
        magnifier_bar_->GetBar(symbol_idx, bar_->GetCurrentBarIndex())
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
        if (!bar_->ProcessBarIndex(symbol_idx, MAGNIFIER, "NONE",
                                   prev_close_time)) {
          // 돋보기 바의 최대 인덱스에 도달하여 트레이딩 전 바 Close
          // Time까지 이동시키지 못하면 돋보기 사용 불가
          can_use_magnifier = false;
        }
      }

      if (can_use_magnifier) {
        // 트레이딩 전 바 Close Time과 같은
        // Close Time을 갖는 돋보기 바 인덱스
        const auto updated_bar_idx = bar_->GetCurrentBarIndex();

        // 트레이딩 바의 Close Time까지 돋보기 바의 인덱스가 유효한지 확인
        try {
          size_t added_index = 1;

          do {
            magnifier_close_time =
                magnifier_bar_
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

void Engine::ProcessOhlc(const vector<int>& activated_symbols,
                         const vector<size_t>& activated_bar_indices) {
  // 체크할 순서대로 가격 데이터를 저장한 벡터 로딩
  const auto& price_queue =
      GetPriceQueue(bar_->GetBarData(bar_->GetCurrentBarType(), "NONE"),
                    activated_symbols, activated_bar_indices);

  // 정해진 순서대로 마진콜과 대기 주문을 확인
  for (const auto& price_data : price_queue) {
    const auto& [price, price_type, symbol_idx] = price_data;

    // 가격 유효성 검사
    if (IsLessOrEqual(price, 0.0) || isnan(price)) {
      logger_->Log(WARNING_L,
                   format("현재가 {}이(가) 유효하지 않으므로 마진콜과 대기 "
                          "주문의 체결을 확인할 수 없습니다.",
                          price),
                   __FILE__, __LINE__);
      continue;
    }

    bar_->SetCurrentSymbolIndex(symbol_idx);

    for (const auto& strategy : strategies_) {
      // 진입 및 청산 시 전략 이름을 설정해야하므로 미리 설정
      current_strategy_name_ = strategy->GetName();

      // 각 전략의 OrderHandler를 순회하며 대기 주문 체크
      const auto& order_handler = strategy->GetOrderHandler();

      // @@@@@@@@@ 마진콜 체크 : 체결 진입 주문 체크,
      // 마진콜 당하면 부분 청산 잔여 수량만 삭제하기
      // + 해당 진입 이름을 목표로 하는 청산 대기 주문도 삭제

      // 진입 및 청산 대기 주문의 체결 확인
      bool just_entered = false;
      bool just_exited = false;
      do {
        // 진입 대기 주문의 체결 확인 후 체결이 존재할 시 After Entry 전략 실행
        order_handler->CheckPendingEntries(price, price_type, symbol_idx);
        if (order_handler->GetJustEntered()) {
          order_handler->InitializeJustEntered();
          ExecuteStrategy(strategy, StrategyType::AFTER_ENTRY, symbol_idx);

          // After Entry 전략 실행 시 추가 진입 혹은 청산 가능성이 있으므로
          // 상태를 다시 업데이트
          just_entered = order_handler->GetJustEntered();
          just_exited = order_handler->GetJustExited();
        }

        // 청산 대기 주문의 체결 확인 후 체결이 존재할 시 After Exit 전략 실행
        order_handler->CheckPendingExits(price, price_type, symbol_idx);
        if (order_handler->GetJustExited()) {
          order_handler->InitializeJustExited();
          ExecuteStrategy(strategy, StrategyType::AFTER_EXIT, symbol_idx);

          // After Exit 전략 실행 시 추가 진입 혹은 청산 가능성이 있으므로
          // 상태를 다시 업데이트
          just_entered = order_handler->GetJustEntered();
          just_exited = order_handler->GetJustExited();
        }

        // 진입 및 청산 체결이 없는 경우 체결 확인 및 전략 실행 종료
      } while (just_entered || just_exited);
    }
  }
}

vector<PriceData> Engine::GetPriceQueue(
    const BarData& bar_data, const vector<int>& activated_symbols,
    const vector<size_t>& activated_bar_indices) {
  vector<PriceData> open_queue;
  vector<PriceData> high_low_queue1;
  vector<PriceData> high_low_queue2;
  vector<PriceData> close_queue;

  // 미리 메모리 할당
  const auto num_activated_symbols = activated_symbols.size();
  open_queue.reserve(num_activated_symbols);
  high_low_queue1.reserve(num_activated_symbols);
  high_low_queue2.reserve(num_activated_symbols);
  close_queue.reserve(num_activated_symbols);

  // 활성화된 심볼 순회
  PriceData price_data{};
  for (int activated_symbol_idx = 0;
       activated_symbol_idx < num_activated_symbols; activated_symbol_idx++) {
    // 해당 심볼의 가격 데이터 로딩
    const int symbol_idx = activated_symbols[activated_symbol_idx];
    const auto& bar = bar_data.GetBar(
        symbol_idx, activated_bar_indices[activated_symbol_idx]);
    const double open = bar.open;
    const double high = bar.high;
    const double low = bar.low;

    // 구조체 공통 필드 설정
    price_data.symbol_index = symbol_idx;

    // 시가 데이터 추가
    price_data.price = open;
    price_data.price_type = OPEN;
    open_queue.push_back(price_data);

    // 고저가 데이터 추가
    // 시가 대비 고가의 폭이 저가의 폭보다 크다면
    // 시가 -> 저가 -> 고가 -> 종가로 움직임 가정.
    // 시가 대비 저가의 폭이 고가의 폭보다 크다면
    // 시가 -> 고가 -> 저가 -> 종가로 움직임 가정.
    if (IsGreaterOrEqual(high - open, open - low)) {
      price_data.price = low;
      price_data.price_type = LOW;
      high_low_queue1.push_back(price_data);

      price_data.price = high;
      price_data.price_type = HIGH;
      high_low_queue2.push_back(price_data);
    } else {
      price_data.price = high;
      price_data.price_type = HIGH;
      high_low_queue1.push_back(price_data);

      price_data.price = low;
      price_data.price_type = LOW;
      high_low_queue2.push_back(price_data);
    }

    // 종가 데이터 추가
    price_data.price = bar.close;
    price_data.price_type = CLOSE;
    close_queue.push_back(price_data);
  }

  vector<PriceData> result;
  result.reserve(num_activated_symbols * 4);

  // 모든 벡터의 데이터를 result에 이동
  result.insert(result.end(), make_move_iterator(open_queue.begin()),
                make_move_iterator(open_queue.end()));

  result.insert(result.end(), make_move_iterator(high_low_queue1.begin()),
                make_move_iterator(high_low_queue1.end()));

  result.insert(result.end(), make_move_iterator(high_low_queue2.begin()),
                make_move_iterator(high_low_queue2.end()));

  result.insert(result.end(), make_move_iterator(close_queue.begin()),
                make_move_iterator(close_queue.end()));

  return result;
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

    if (strategy_type == StrategyType::ON_CLOSE) {
      strategy->ExecuteOnClose();
    } else if (strategy_type == StrategyType::AFTER_ENTRY) {
      strategy->ExecuteAfterEntry();
    } else if (strategy_type == StrategyType::AFTER_EXIT) {
      strategy->ExecuteAfterExit();
    }
  } catch ([[maybe_unused]] const Bankruptcy& e) {
    SetBankruptcy();
    throw;
  }

  // 원본 설정을 복원
  bar_->SetCurrentBarType(original_bar_type, "NONE");
}
