// 표준 라이브러리
#include <cmath>
#include <format>
#include <utility>
#include <variant>

// 파일 헤더
#include "Engines\Engine.hpp"

// 내부 헤더
#include "Engines\BarData.hpp"
#include "Engines\BarHandler.hpp"
#include "Engines\BaseBarHandler.hpp"
#include "Engines\DataUtils.hpp"
#include "Engines\OrderHandler.hpp"
#include "Engines\Strategy.hpp"
#include "Engines\TimeUtils.hpp"

// 네임 스페이스
using namespace data_utils;
using namespace time_utils;
using namespace std;

Engine::Engine(const Config& config)
    : BaseEngine(config),
      unrealized_pnl_updated_(false),
      begin_open_time_(INT64_MAX),
      end_open_time_(0),
      current_open_time_(-1),
      current_close_time_(-1) {}

void Engine::Deleter::operator()(Engine* p) const {
  // @@@@@@@@@@@ 저장할 거 저장하기
  delete p;
}

mutex Engine::mutex_;
shared_ptr<Engine> Engine::instance_;

shared_ptr<Engine>& Engine::GetEngine(const Config& config) {
  lock_guard lock(mutex_);  // 다중 스레드에서 안전하게 접근하기 위해 mutex 사용

  // 인스턴스가 생성됐는지 확인
  if (!instance_) {
    // 인스턴스가 생성되지 않았으면 생성 후 저장
    if (isnan(config.GetInitialBalance()) ||
        config.GetCommissionType() == CommissionType::COMMISSION_NONE ||
        isnan(config.GetCommission().first) ||
        isnan(config.GetCommission().second) ||
        config.GetSlippageType() == SlippageType::SLIPPAGE_NONE ||
        isnan(config.GetSlippage().first) ||
        isnan(config.GetSlippage().second)) {
      Logger::LogAndThrowError(
          "엔진 인스턴스 첫 생성 시 설정값을 모두 초기화해야 합니다.", __FILE__,
          __LINE__);
    }

    instance_ = shared_ptr<Engine>(new Engine(config), Deleter());
  }

  return instance_;
}

void Engine::Backtesting(const bool use_bar_magnifier, const string& start,
                         const string& end, const string& format) {
  // 유효성 검증
  IsValidBarData(use_bar_magnifier);
  IsValidDateRange(start, end, format);
  // @@@@@@@ (지표랑 전략 추가) 검증 추가 / sub bar 관련도 추가
 /*
  // 사전 엔진 초기화
  PreInitializeEngine();

  // 전략에서 지표 추가 -> 지표 계산 (current symbol, sub index 이런거 바꿔가며
  // 계산) -> 지표 추가 완료 -> 전략 추가

  // 지표 계산@@@@@@@@@@@@@@@@@@
  // trading_testing_timeframe보다 낮으면 오류
  // 타임프레임이 높으면 빈 곳 채우면서 계산
  // 각 봉에서 전략 실행 전 지표 valid인지 플래그 체크 후 false면 지표 체크 후
  // nan 아니면 true로 바꾸고 전략 진행

  // 엔진 초기화
  InitializeEngine();

  const string& is_debug = debug_mode_ ? "디버그 모드로 " : "";
  logger_->Log(Logger::INFO_L,
               std::format("백테스팅을 {}시작합니다.", is_debug), __FILE__,
               __LINE__);

  while (true) {
    // 심볼의 트레이딩 시작과 끝 검증
    const auto& trading_activated = CheckTradingStatus();

    for (const auto& [symbol, bar_data] : trading_activated) {
      current_symbol = symbol;  // 현재 심볼 업데이트
    }
    // Order();
    // Calculate();
    //
    // 인덱스 증가
    // @@@@@@@@@@@ 인덱스들은 바 하나 이동 시 처음에 일괄 업데이트할 것 =>
    // 미실현 손익 계산 시 필요

    // 한 바 이동 시 unrealized_pnl_updated_ : false

    // current_position_size(baseorder), current symbol 등의 업데이트

    // 마진콜 검색, max_profit_/max_loss_ 방향에 따라 high low 때 업데이트
    // 대기중인 진입/청산 체크 (지정가, 터치, 트레일링)
    // 터치 체크법 -> 방금 전 확인한 가격이 터치보다 밑이고 지금 가격이 위이거나
    // (같아도 됨?) 방금 전 확인한 가격이 터치보다 위고 지금 가격이 아래이거나
    // (같아도 됨?)
    // 암튼 터치 체크 전 먼저 예전에 터치 됐나부터 체크해야함

    // 트레일링 조건 체크 시: extreme 설정 됐나부터 확인 -> 안됐으면 touch 확인
    // touch 0으로 하면 바로 트레일링하게 만들기 위함

    // 종가 전략 실행 이후 @@@@@@@@@@
    // SetCurrentBarType 다시 필요@@@@ 지표 계산시 변경

    // 백테스팅 종료 체크

    // 저장할 거 저장 지표 값, 성과 등
  }*/
}

void Engine::UpdateUnrealizedPnl() {
  double pnl = 0;

  // 전략별 미실현 손익을 합산
  for (const auto& strategy : strategies_) {
    pnl += strategy->GetOrderHandler()->GetUnrealizedPnl();
  }

  // 진입 가능 자금에 합산
  IncreaseAvailableBalance(pnl);

  unrealized_pnl_updated_ = true;
}

void Engine::IsValidBarData(const bool use_bar_magnifier) {
  const auto& trading_bar = bar_->GetBarData(BarType::TRADING);
  const auto num_symbols = trading_bar.GetNumSymbols();

  // 트레이딩 바 데이터가 비었는지 체크
  if (!num_symbols)
    Logger::LogAndThrowError("트레이딩 바 데이터가 비어있습니다.", __FILE__,
                             __LINE__);

  for (int i = 0; i < num_symbols; ++i) {
    // 트레이딩 바 데이터의 심볼들이 돋보기 바 데이터에 존재하는지 체크
    if (const auto& symbol_name = trading_bar.GetSymbolName(i);
        use_bar_magnifier &&
        symbol_name != bar_->GetBarData(BarType::MAGNIFIER).GetSymbolName(i)) {
      Logger::LogAndThrowError(
          symbol_name +
              "이(가) 돋보기 바 데이터에 존재하지 않거나 트레이딩 바의 심볼 "
              "순서와 일치하지 않습니다.",
          __FILE__, __LINE__);
    }
  }

  logger_->Log(LogLevel::INFO_L, "바 데이터 유효성 검증이 완료되었습니다.",
               __FILE__, __LINE__);
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
          std::format("지정된 Start 시간 {}이(가) 최소 시간 {}의 밖입니다.",
                      UtcTimestampToUtcDatetime(begin_open_time_),
                      UtcTimestampToUtcDatetime(start_time)),
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
          std::format("지정된 End 시간 {}이(가) 최대 시간 {}의 밖입니다.",
                      UtcTimestampToUtcDatetime(end_open_time_),
                      UtcTimestampToUtcDatetime(end_time)),
          __FILE__, __LINE__);
    } else {
      end_open_time_ = end_time;
    }
  }

  logger_->Log(LogLevel::INFO_L, "날짜 유효성 검증이 완료되었습니다.", __FILE__,
               __LINE__);
}

/*
void Engine::PreInitializeEngine() {
  // 지표 계산을 위해 sub_index를 미리 초기화
  InitializeSubIndex();

  logger.Log(Logger::INFO_L, "엔진 사전 초기화가 완료되었습니다.", __FILE__,
             __LINE__);
}

void Engine::InitializeEngine() {
  // @@@@@@@@@@@@ 순서 재배치 필요

  // 백테스팅 중 설정하는 트레이딩 변수 초기화
  current_open_time_ = begin_open_time_;

  for (const auto& [symbol, bar_data] : bar_.GetTradingBarData()) {
    // reserve 이후 emplace
    trading_index.emplace(symbol, 0);
    magnifier_index.emplace(symbol, 0);

    // sub_index 초기화
    InitializeSubIndex();

    // 첫 시작 시간이 begin_open_time과 같다면 바로 시작하는 Symbol
    if (bar_data.begin()->open_time == begin_open_time_) {
      trading_began_.emplace(symbol, true);
    } else {
      trading_began_.emplace(symbol, false);
    }

    trading_ended_.emplace(symbol, false);
  }

  wallet_balance_ = initial_capital;
  max_wallet_balance_ = initial_capital;
  drawdown_ = 0.0f;
  maximum_drawdown = 0.0f;

  // 트레이딩 바 심볼 개수 == 돋보기 == 참조(모든 tf)인지 확인

  // @@@@@@@@@@@@@ 돋보기랑 서브 start 인덱스 찾기 ㄱㄱㄱ 시작 시간 넘어가면
  // 에러 발생요

  // 틱사이즈 계산 : BaseEngine에 함수 위치

  // 전략 관련
  // 전략 초기화

  order.InitializeOrders();

  logger.Log(Logger::INFO_L, "엔진 초기화가 완료되었습니다.", __FILE__,
             __LINE__);
}

void Engine::InitializeSubIndex() {
  for (const auto& [symbol, info] : sub_bar_data) {
    unordered_map<string, size_t> index;
    for (const auto& timeframe : info | views::keys) {
      index.emplace(timeframe, 0);
    }
    sub_index.emplace(symbol, move(index));
  }
}

unordered_map<string, vector<Engine::bar_data>> Engine::CheckTradingStatus() {
  unordered_map<string, vector<bar_data>> trading_activated;

  for (auto& [symbol, bar_data] : trading_bar_data) {
    const auto local_current_open_time =
        bar_data[trading_index[symbol]].open_time;
    // 이미 트레이딩을 시작한 심볼
    if (trading_began_[symbol]) {
      // 활성화된 트레이딩 심볼로 추가
      trading_activated.emplace(symbol, move(bar_data));

      // 현재 인덱스의 Open Time이 마지막 Open Time이라면
      if (local_current_open_time == end_open_time_) {
        trading_began_[symbol] = false;
        trading_ended_[symbol] = true;
      }
    } else {  // 트레이딩이 끝나지 않았고, 시작되지 않은 심볼
      if (!trading_ended_[symbol] &&
          local_current_open_time == current_open_time_) {
        // 활성화된 트레이딩 심볼로 추가
        trading_activated.emplace(symbol, move(bar_data));
        trading_began_[symbol] = true;
      }
    }
  }

  return trading_activated;
}*/
