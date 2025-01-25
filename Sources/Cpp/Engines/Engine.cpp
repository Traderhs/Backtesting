// 표준 라이브러리
#include <cmath>
#include <format>
#include <set>
#include <utility>
#include <variant>

// 파일 헤더
#include "Engines/Engine.hpp"

// 내부 헤더
#include "Engines/BarData.hpp"
#include "Engines/BarHandler.hpp"
#include "Engines/BaseBarHandler.hpp"
#include "Engines/DataUtils.hpp"
#include "Engines/OrderHandler.hpp"
#include "Engines/Strategy.hpp"
#include "Engines/TimeUtils.hpp"

// 네임 스페이스
using namespace data_utils;
using namespace time_utils;
using namespace std;

Engine::Engine()
    : begin_open_time_(INT64_MAX),
      end_open_time_(0),
      current_open_time_(0),
      unrealized_pnl_updated_(false) {}

void Engine::Deleter::operator()(const Engine* p) const {
  // @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ 저장할 거 저장하기
  delete p;
}

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
  // 유효성 검증
  IsValidBarData(use_bar_magnifier);
  IsValidConfig();
  IsValidDateRange(start, end, format);
  IsValidStrategies();

  // 엔진 초기화
  InitializeEngine();

  const string& is_debug = debug_mode_ ? "디버그 모드로 " : "";
  logger_->Log(LogLevel::INFO_L,
               std::format("백테스팅을 {}시작합니다.", is_debug), __FILE__,
               __LINE__);

  // 변수 로딩
  const auto& trading_bar = bar_->GetBarData(BarType::TRADING);
  const auto& magnifier_bar = bar_->GetBarData(BarType::MAGNIFIER);
  const auto& reference_bar = bar_->GetAllReferenceBarData();

  while (true) {
    // ======================================
    // 트레이딩 바 데이터의 인덱스를 하나씩 진행
    // ======================================

    // 현재 바 인덱스에서 트레이딩을 진행하는지 상태를 업데이트
    const auto& activated_symbols = UpdateTradingStatus();

    // 트레이딩을 진행하는 심볼이 없다면 백테스팅 끝
    if (activated_symbols.empty()) {
      break;
    }

    /* ! 트레이딩을 진행하는 심볼은 트레이딩 바 인덱스 일괄 업데이트 !
       진입 시 미실현 손익을 업데이트하는데, 이때 모든 심볼의
       현재 바의 시가를 참조하기 때문에 일괄 업데이트가 필요 */
    for (const int symbol_idx : activated_symbols) {
      bar_->IncreaseBarIndex(BarType::TRADING, "NONE", symbol_idx);
    }
    unrealized_pnl_updated_ = false;

    // 현재 바 인덱스의 트레이딩을 진행

    // 한 바 이동 시 unrealized_pnl_updated_ : false

    // current_position_size(baseorder), current symbol 등의 업데이트

    // 돋보기 바 아직 없거나 끝났으면 기본 트레이딩바로 작동

    // 전략 실행이랑 참조 바 전체 인덱스 증가 중 뭐가 먼저??

    // 전략 실행 시 지표 nan이면 어떻게 진행? -> 지표 값 참조 시
    // IndicatorInvalidValue throw 후 전략 실행에서 catch 하고 다음 봉으로 진행
    // IndicatorOutOfRange도 받기 (전략에서는 catch 해야됨, 전략 시작 시점에서
    // [20] 참조 이런거 있을 수도 있으므로
    // Bankruptcy도 받기 market 주문에서 파산 가능
    // OrderFailed도 받기 -> e.what() ㄱㄱ

    // 마진콜 검색, max_profit_/max_loss_ 방향에 따라 high low 때 업데이트
    // 대기중인 진입/청산 체크 (지정가, 터치, 트레일링)

    // 트레이딩 인덱스 증가 @@@@@@@@@@@@@@@@@@
  }

  // 백테스팅 종료 체크

  // 체결, 대기 주문 처리

  // 저장할 거 저장 지표 값, 성과 등

  // @@@@@@@@ 완성 후 모듈화 ㄱㄱ
}

void Engine::UpdateUnrealizedPnl() {
  if (!unrealized_pnl_updated_) {
    double unrealized_pnl = 0;

    // 전략별 미실현 손익을 합산
    for (const auto& strategy : strategies_) {
      unrealized_pnl += strategy->GetOrderHandler()->GetUnrealizedPnl();
    }

    // 진입 가능 자금 추가/감소
    if (unrealized_pnl > 0) {
      IncreaseAvailableBalance(unrealized_pnl);
    } else if (unrealized_pnl < 0) {
      DecreaseAvailableBalance(abs(unrealized_pnl));
    }

    unrealized_pnl_updated_ = true;
  }
}

int64_t Engine::GetCurrentOpenTime() const { return current_open_time_; }

void Engine::IsValidBarData(const bool use_bar_magnifier) {
  const auto& trading_bar = bar_->GetBarData(BarType::TRADING);
  const auto trading_num_symbols = trading_bar.GetNumSymbols();

  // 1.1. 트레이딩 바 데이터가 비었는지 검증
  if (!trading_num_symbols)
    Logger::LogAndThrowError("트레이딩 바 데이터가 비어있습니다.", __FILE__,
                             __LINE__);

  // 1.2. 트레이딩 바 데이터의 중복 가능성 검증
  set<double> trading_bar_open;
  for (int i = 0; i < trading_num_symbols; i++) {
    trading_bar_open.insert(trading_bar.GetOpen(i, 0));
  }

  if (trading_bar_open.size() != trading_num_symbols) {
    logger_->Log(LogLevel::WARNING_L,
                 "트레이딩 바 데이터에 중복된 데이터가 다른 심볼로 추가되었을 "
                 "가능성이 있습니다.",
                 __FILE__, __LINE__);
  }

  if (use_bar_magnifier) {
    const auto& magnifier_bar = bar_->GetBarData(BarType::MAGNIFIER);
    const auto magnifier_num_symbols = magnifier_bar.GetNumSymbols();

    /* 2.1. 트레이딩 바 데이터의 심볼 개수와 돋보기 바 데이터의
            심볼 개수가 같은지 검증 */
    if (magnifier_num_symbols != trading_num_symbols) {
      Logger::LogAndThrowError(
          format("트레이딩 바 데이터에 추가된 심볼 개수({}개)와 돋보기 바 "
                 "데이터에 추가된 심볼 개수({}개)는 동일해야 합니다.",
                 trading_num_symbols, magnifier_num_symbols),
          __FILE__, __LINE__);
    }

    /* 2.2. 트레이딩 바 데이터의 심볼들이 돋보기 바 데이터에 존재하고
            순서가 같은지 검증 */
    for (int i = 0; i < trading_num_symbols; ++i) {
      if (const auto& symbol_name = trading_bar.GetSymbolName(i);
          symbol_name != magnifier_bar.GetSymbolName(i)) {
        Logger::LogAndThrowError(
            format("돋보기 바 데이터에 {}이(가) 존재하지 않거나 "
                   "트레이딩 바에 추가된 심볼 순서와 일치하지 않습니다.",
                   symbol_name),
            __FILE__, __LINE__);
      }
    }

    // 2.3. 돋보기 바 데이터의 중복 가능성 검증
    set<double> magnifier_open;
    for (int i = 0; i < magnifier_num_symbols; i++) {
      magnifier_open.insert(magnifier_bar.GetOpen(i, 0));
    }

    if (magnifier_open.size() != magnifier_num_symbols) {
      logger_->Log(LogLevel::WARNING_L,
                   "돋보기 바 데이터에 중복된 데이터가 다른 심볼로 추가되었을 "
                   "가능성이 있습니다.",
                   __FILE__, __LINE__);
    }
  }

  for (const auto& [reference_timeframe, reference_bar] :
       bar_->GetAllReferenceBarData()) {
    const auto reference_num_symbols = reference_bar.GetNumSymbols();

    /* ※ 참조 바 데이터의 심볼 개수와 순서 검증은 추후 트레이딩 바 심볼 외
          다른 데이터의 참조가 필요할 때 삭제 */

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
    for (int i = 0; i < trading_num_symbols; ++i) {
      if (const auto& symbol_name = trading_bar.GetSymbolName(i);
          symbol_name != reference_bar.GetSymbolName(i)) {
        Logger::LogAndThrowError(
            format("참조 바 데이터 {}에 {}이(가) 존재하지 않거나 "
                   "트레이딩 바에 추가된 심볼 순서와 일치하지 않습니다.",
                   reference_timeframe, symbol_name),
            __FILE__, __LINE__);
      }
    }

    // 3.3. 참조 바 데이터의 중복 가능성 검증
    set<double> reference_open;
    for (int i = 0; i < reference_num_symbols; i++) {
      reference_open.insert(reference_bar.GetOpen(i, 0));
    }

    if (reference_open.size() != reference_num_symbols) {
      logger_->Log(
          LogLevel::WARNING_L,
          format("참조 바 데이터 {}에 중복된 데이터가 다른 심볼로 추가되었을 "
                 "가능성이 있습니다.",
                 reference_timeframe),
          __FILE__, __LINE__);
    }
  }

  logger_->Log(LogLevel::INFO_L, "바 데이터 유효성 검증이 완료되었습니다.",
               __FILE__, __LINE__);
}

void Engine::IsValidConfig() const {
  const auto& config = GetConfig();

  if (isnan(config.GetInitialBalance()) ||
      config.GetCommissionType() == CommissionType::COMMISSION_NONE ||
      isnan(config.GetCommission().first) ||
      isnan(config.GetCommission().second) ||
      config.GetSlippageType() == SlippageType::SLIPPAGE_NONE ||
      isnan(config.GetSlippage().first) || isnan(config.GetSlippage().second)) {
    Logger::LogAndThrowError("엔진 설정값을 모두 초기화해야 합니다.", __FILE__,
                             __LINE__);
  }

  if (const auto market_commission = config.GetCommission().first;
      market_commission > 100 || market_commission < 0) {
    Logger::LogAndThrowError(
        format("지정된 시장가 수수료율 {}%는 100% 초과 혹은 "
               "0% 미만으로 설정할 수 없습니다.",
               market_commission),
        __FILE__, __LINE__);
  }

  if (const auto limit_commission = config.GetCommission().second;
      limit_commission > 100 || limit_commission < 0) {
    Logger::LogAndThrowError(
        format("지정된 지정가 수수료율 {}%는 100% 초과 혹은 "
               "0% 미만으로 설정할 수 없습니다.",
               limit_commission),
        __FILE__, __LINE__);
  }

  if (const auto market_slippage = config.GetSlippage().first;
      market_slippage > 100 || market_slippage < 0) {
    Logger::LogAndThrowError(
        format("지정된 시장가 슬리피지율 {}%는 100% 초과 혹은 "
               "0% 미만으로 설정할 수 없습니다.",
               market_slippage),
        __FILE__, __LINE__);
  }

  if (const auto limit_slippage = config.GetSlippage().second;
      limit_slippage > 100 || limit_slippage < 0) {
    Logger::LogAndThrowError(
        format("지정된 지정가 슬리피지율 {}%는 100% 초과 혹은 "
               "0% 미만으로 설정할 수 없습니다.",
               limit_slippage),
        __FILE__, __LINE__);
  }

  logger_->Log(LogLevel::INFO_L, "엔진 설정값 검증이 완료되었습니다.", __FILE__,
               __LINE__);
}

void Engine::IsValidDateRange(const string& start, const string& end,
                              const string& format) {
  const auto& trading_bar = bar_->GetBarData(BarType::TRADING);
  for (int i = 0; i < trading_bar.GetNumSymbols(); i++) {
    // 백테스팅 시작 시 가장 처음의 Open Time 값 구하기
    begin_open_time_ = min(begin_open_time_, trading_bar.GetOpenTime(i, 0));

    // 백테스팅 시작 시 가장 끝의 Open Time 값 구하기
    end_open_time_ = max(end_open_time_, trading_bar.GetOpenTime(
                                             i, trading_bar.GetNumBars(i) - 1));
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

  logger_->Log(LogLevel::INFO_L, "날짜 유효성 검증이 완료되었습니다.", __FILE__,
               __LINE__);
}

void Engine::IsValidStrategies() const {
  if (strategies_.empty()) {
    Logger::LogAndThrowError("엔진에 전략이 추가되지 않았습니다.", __FILE__,
                             __LINE__);
  }

  logger_->Log(LogLevel::INFO_L, "전략 유효성 검증이 완료되었습니다.", __FILE__,
               __LINE__);
}

void Engine::InitializeEngine() {
  const auto& trading_bar = bar_->GetBarData(BarType::TRADING);
  const auto num_symbols = trading_bar.GetNumSymbols();

  // trading_began_, trading_ended 초기화
  trading_began_.resize(num_symbols);
  trading_ended_.resize(num_symbols);

  for (int i = 0; i < num_symbols; i++) {
    // 첫 시작 시간이 begin_open_time과 같다면 바로 시작하는 Symbol
    if (trading_bar.GetOpenTime(i, 0) == begin_open_time_) {
      trading_began_[i] = true;
    } else {
      trading_began_[i] = false;
    }

    trading_ended_[i] = false;
  }

  // 전략별 주문 초기화
  for (const auto& strategy : strategies_) {
    strategy->GetOrderHandler()->InitializeOrders(num_symbols);
  }

  logger->Log(LogLevel::INFO_L, "엔진 초기화가 완료되었습니다.", __FILE__,
              __LINE__);
}

vector<int> Engine::UpdateTradingStatus() {
  // 원본 설정을 저장
  const auto original_bar_type = bar_->GetCurrentBarType();
  const auto original_reference_tf = bar_->GetCurrentReferenceTimeframe();
  const auto original_symbol_idx = bar_->GetCurrentSymbolIndex();

  // 트레이딩 바 로딩
  const auto& trading_bar = bar_->GetBarData(BarType::TRADING);

  // 사용 중인 바 타입 업데이트
  bar_->SetCurrentBarType(BarType::TRADING, "NONE");

  // 이번 바에서 트레이딩을 진행하는 심볼들
  vector<int> activated_symbols;

  // 트레이딩 바 전체 심볼 순회
  for (int i = 0; i < trading_bar.GetNumSymbols(); i++) {
    // 사용 중인 심볼 업데이트
    bar_->SetCurrentSymbolIndex(i);

    if (trading_began_[i]) {
      // 트레이딩을 시작했지만 끝난 심볼은 업데이트할 것이 없음
      if (trading_ended_[i]) {
        continue;
      }

      // 트레이딩을 시작했지만 끝나지 않은 심볼은 이번 바에서 끝났는지 검사
      if (trading_bar.GetOpenTime(i, bar_->GetCurrentBarIndex()) > end_open_time_) {
        trading_ended_[i] = true;
      } else {
        activated_symbols.push_back(i);
      }
    } else {
      // 트레이딩을 시작하지 않은 심볼은 이번 바에서 시작했는지 검사
      if (trading_bar.GetOpenTime(i, bar_->GetCurrentBarIndex()) == begin_open_time_) {
        trading_began_[i] = true;
        activated_symbols.push_back(i);
      }
    }
  }

  // 원본 설정을 복원
  bar_->SetCurrentBarType(original_bar_type, original_reference_tf);
  bar_->SetCurrentSymbolIndex(original_symbol_idx);

  return activated_symbols;
}

void Engine::ProcessOhlc(const vector<int>& activated_symbols, const vector<size_t>& activated_bar_indices) {
  // 바 정보 로딩
  const auto current_bar_type = bar_->GetCurrentBarType();
  const auto& bar = bar_->GetBarData(current_bar_type, "NONE");

  // 현재 Open Time 업데이트
  current_open_time_ = bar.GetOpenTime(activated_symbols[0], activated_bar_indices[0]);

  // @@@@@@@@@ 마진콜 체크 : 체결 진입 주문 체크,
    // 마진콜 당하면 부분 청산 잔여 수량만 삭제하기
    // + 해당 진입 이름을 목표로 하는 청산 대기 주문도 삭제
    // Check Filled Entry MarginCall 이거 이름 잘 배열 ㄱㄱ

  for (const auto& strategy : strategies_) {
    // 각 전략의 OrderHandler를 순회하며 대기 주문 체크
    const auto& order_handler = strategy->GetOrderHandler();

    // 시가의 마진콜 및 체결 확인
    vector<double> open_prices;
    for (int i = 0; i < activated_symbols.size(); i++) {
      open_prices.push_back(bar.GetOpen(activated_symbols[i], activated_bar_indices[i]));
    }

    // @@@@@@마진콜

    order_handler->CheckPendingEntries(open_prices, true);
    order_handler->CheckPendingExits(open_prices, true);
  }

  // 가격 배열 생성
  // 시가 대비 고가의 폭이 저가의 폭보다 크다면 시가 -> 저가 -> 고가 -> 종가로 움직임 가정
  // 시가 대비 저가의 폭이 고가의 폭보다 크다면 시가 -> 고가 -> 저가 -> 종가로 움직임 가정
  const double prices[4] = { open, high - open >= open - low ? low : high,
                             high - open >= open - low ? high : low, close };



    /
  }
}

