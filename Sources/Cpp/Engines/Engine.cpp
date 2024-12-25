// 표준 라이브러리
#include <cmath>
#include <format>
#include <utility>
#include <variant>

// 내부 헤더
#include "Engines/DataUtils.hpp"
#include "Engines/TimeUtils.hpp"

// 파일 헤더
#include "Engines/Engine.hpp"

// 네임 스페이스
using namespace DataUtils;
using namespace TimeUtils;
using namespace std;

// 데이터 접근 연산자 오버로딩
/*
double operator[](size_t idx) const {
    if (idx < high.size()) {
        return high[idx];  // high 배열에 접근
    }
    return -1.0f;  // 인덱스가 유효하지 않을 경우
}*/

Engine::Engine()
    : begin_open_time(INT64_MAX),
      end_open_time(0),
      current_open_time(-1),
      current_close_time(-1) {}

Engine::~Engine() = default;  // @@@@@@@@@@@ 저장할 거 저장하기

void Engine::Backtesting(const bool use_bar_magnifier, const string& start,
                         const string& end, const string& format) {
  // 유효성 검증
  IsValidBarData(use_bar_magnifier);
  IsValidDateRange(start, end, format);
  IsValidSettings();
  // @@@@@@@ (지표랑 전략) 검증 추가 / sub bar 관련도 추가

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

  logger.Log(Logger::INFO_L, "백테스팅을 시작합니다.", __FILE__, __LINE__);

  while (true) {
    // 심볼의 트레이딩 시작과 끝 검증
    const auto& trading_activated = CheckTradingStatus();

    for (const auto& [symbol, bar_data] : trading_activated) {
      current_symbol = symbol;
    }
    // Order();
    // Calculate();
    //
    // 인덱스 증가

    // Current Open Time Update

    // 백테스팅 종료 체크

    // 저장할 거 저장 지표 값, 성과 등
  }
}

DataManager& Engine::data = DataManager::GetDataManager();
BarDataManager& Engine::bar = BarDataManager::GetBarDataManager();
Logger& Engine::logger = Logger::GetLogger();
OrderManager& Engine::order = OrderManager::GetOrderManager();

void Engine::IsValidBarData(const bool use_bar_magnifier) {
  // Trading Bar Data가 비었는지 체크
  if (bar.GetTradingBarData().empty())
    Logger::LogAndThrowError("트레이딩 바 데이터가 비어있습니다.", __FILE__,
                             __LINE__);

  for (const auto& symbol : bar.GetTradingBarData() | views::keys) {
    // Trading Bar Data가 Magnifier에 존재하는지 체크
    if (use_bar_magnifier && !bar.GetMagnifierBarData().contains(symbol)) {
      Logger::LogAndThrowError(
          symbol + "이(가) 돋보기 바 데이터에 존재하지 않습니다.", __FILE__,
          __LINE__);
    }
  }

  logger.Log(Logger::INFO_L, "데이터 유효성 검증이 완료되었습니다.", __FILE__,
             __LINE__);
}

void Engine::IsValidDateRange(const string& start, const string& end,
                              const string& format) {
  for (const auto& bar_data : bar.GetTradingBarData() | views::values) {
    // 백테스팅 시작 시 가장 처음의 Open Time 값 구하기
    begin_open_time = min(begin_open_time, bar_data.begin()->open_time);

    // 백테스팅 시작 시 가장 끝의 Open Time 값 구하기
    end_open_time = max(end_open_time, prev(bar_data.end())->open_time);
  }

  // Start가 지정된 경우 범위 체크
  if (!start.empty()) {
    if (const int64_t start_time = UTCDatetimeToUTCTimestamp(start, format);
        start_time < begin_open_time) {
      Logger::LogAndThrowError(
          std::format(
              "지정된 Start 시간이 데이터 범위 밖입니다. | 최소 시간: {} | "
              "지정된 Start 시간: {}",
              UTCTimestampToUtcDatetime(begin_open_time),
              UTCTimestampToUtcDatetime(start_time)),
          __FILE__, __LINE__);
    } else {
      begin_open_time = start_time;
    }
  }

  // End가 지정된 경우 범위 체크
  if (!end.empty()) {
    if (const int64_t end_time = UTCDatetimeToUTCTimestamp(end, format);
        end_time > end_open_time) {
      Logger::LogAndThrowError(
          std::format(
              "지정된 End 시간이 데이터 범위 밖입니다. | 최대 시간: {} | "
              "지정된 End 시간: {}",
              UTCTimestampToUtcDatetime(end_open_time),
              UTCTimestampToUtcDatetime(end_time)),
          __FILE__, __LINE__);
    } else {
      end_open_time = end_time;
    }
  }

  logger.Log(Logger::INFO_L, "날짜 유효성 검증이 완료되었습니다.", __FILE__,
             __LINE__);
}

void Engine::IsValidSettings() {
  if (data.GetInitialCapital() == -1)
    Logger::LogAndThrowError("초기 자금이 설정되지 않았습니다.", __FILE__,
                             __LINE__);

  if (data.GetCommission() == -1)
    Logger::LogAndThrowError("수수료가 설정되지 않았습니다.", __FILE__,
                             __LINE__);

  if (data.GetSlippage() == -1)
    Logger::LogAndThrowError("슬리피지가 설정되지 않았습니다.", __FILE__,
                             __LINE__);

  logger.Log(Logger::INFO_L, "트레이딩 변수 유효성 검증이 완료되었습니다.",
             __FILE__, __LINE__);
}

void Engine::PreInitializeEngine() {
  // 지표 계산을 위해 sub_index를 미리 초기화
  InitializeSubIndex();

  logger.Log(Logger::INFO_L, "엔진 사전 초기화가 완료되었습니다.", __FILE__,
             __LINE__);
}

void Engine::InitializeEngine() {
  // 백테스팅 중 설정하는 트레이딩 변수 초기화
  current_open_time = begin_open_time;

  for (const auto& [symbol, bar_data] : bar.GetTradingBarData()) {
    trading_index.emplace(symbol, 0);
    magnifier_index.emplace(symbol, 0);

    // sub_index 초기화
    InitializeSubIndex();

    // 첫 시작 시간이 begin_open_time과 같다면 바로 시작하는 Symbol
    if (bar_data.begin()->open_time == begin_open_time) {
      trading_began.emplace(symbol, true);
    } else {
      trading_began.emplace(symbol, false);
    }

    trading_ended.emplace(symbol, false);
  }


  current_capital = initial_capital;
  max_capital = initial_capital;
  drawdown = 0.0f;
  maximum_drawdown = 0.0f;

  // @@@@@@@@@@@@@ 돋보기랑 서브 start 인덱스 찾기 ㄱㄱㄱ 시작 시간 넘어가면
  // 에러 발생요

  // order들 symbol들로 초기화

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
    if (trading_began[symbol]) {
      // 활성화된 트레이딩 심볼로 추가
      trading_activated.emplace(symbol, move(bar_data));

      // 현재 인덱스의 Open Time이 마지막 Open Time이라면
      if (local_current_open_time == end_open_time) {
        trading_began[symbol] = false;
        trading_ended[symbol] = true;
      }
    } else {  // 트레이딩이 끝나지 않았고, 시작되지 않은 심볼
      if (!trading_ended[symbol] &&
          local_current_open_time == current_open_time) {
        // 활성화된 트레이딩 심볼로 추가
        trading_activated.emplace(symbol, move(bar_data));
        trading_began[symbol] = true;
      }
    }
  }

  return trading_activated;
}
