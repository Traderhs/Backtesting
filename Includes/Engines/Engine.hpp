#pragma once

// 표준 라이브러리
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

// 내부 헤더
#include "Engines/BaseEngine.hpp"

// 전방 선언
namespace backtesting::bar {
class BarData;
enum class BarDataType;
struct Bar;
}  // namespace backtesting::bar

namespace backtesting::strategy {
class Strategy;
}

namespace backtesting::order {
struct FillInfo;
enum class Direction;
class SymbolInfo;
class OrderHandler;
}  // namespace backtesting::order

// 네임 스페이스
using namespace std;
namespace backtesting {
using namespace bar;
using namespace strategy;
using namespace order;
}  // namespace backtesting

namespace backtesting::engine {

// 가격 타입을 지정하는 열거형 클래스
enum class PriceType { OPEN, HIGH, LOW, CLOSE };
using enum PriceType;

// 각 가격의 정보를 담고 있는 구조체
struct PriceData {
  double price;          // 가격
  PriceType price_type;  // OHLC
  int symbol_idx;        // 심볼 인덱스
};

// 전략 타입을 지정하는 열거형 클래스
enum class StrategyType {
  ON_CLOSE,
  AFTER_ENTRY,
  AFTER_EXIT,  // 진입과 청산 동시 체결 시 AFTER_ENTRY보다 우선적으로 실행됨
};
using enum StrategyType;

/**
 * 백테스팅 프로세스를 진행하는 클래스
 */
class Engine final : public BaseEngine {
  // 전략, TimeDiff 등 접근용
  friend class Analyzer;

  // ExecuteStrategy 접근용
  friend class OrderHandler;

 public:
  // 싱글톤 특성 유지
  Engine(const Engine&) = delete;             // 복사 생성자 삭제
  Engine& operator=(const Engine&) = delete;  // 대입 연산자 삭제

  /// Engine의 싱글톤 인스턴스를 반환하는 함수
  static shared_ptr<Engine>& GetEngine();

  /// 백테스팅을 실행하는 함수
  void Backtesting();

  /// 현재 사용 중인 전략의 실행 타입을 설정하는 함수
  void SetCurrentStrategyType(StrategyType strategy_type);

  /// 현재 사용 중인 전략의 실행 타입을 반환하는 함수
  [[nodiscard]] StrategyType GetCurrentStrategyType() const;

  /// 현재 진행 중인 Open Time을 반환하는 함수
  [[nodiscard]] int64_t GetCurrentOpenTime() const;

  /// 현재 진행 중인 Close Time을 반환하는 함수
  [[nodiscard]] int64_t GetCurrentCloseTime() const;

  /// 모든 심볼의 트레이딩이 끝났는지 여부를 반환하는 함수
  [[nodiscard]] bool IsAllTradingEnded() const;

  /// 특정 심볼의 트레이딩이 끝났는지 여부를 반환하는 함수
  [[nodiscard]] bool IsTradingEnded(int symbol_idx) const;

 private:
  // 싱글톤 인스턴스 관리
  explicit Engine();
  class Deleter {
   public:
    void operator()(const Engine* p) const;
  };

  // 백테스팅 시작 시간
  static chrono::steady_clock::time_point backtesting_start_time_;

  static mutex mutex_;
  static shared_ptr<Engine> instance_;

  bool use_bar_magnifier_;  // 바 돋보기 기능을 사용하는지 결정하는 플래그

  // ===========================================================================
  shared_ptr<BarData> trading_bar_data_;    // 트레이딩 바 데이터
  shared_ptr<BarData> magnifier_bar_data_;  // 돋보기 바 데이터
  unordered_map<string, shared_ptr<BarData>>
      reference_bar_data_;  // 참조 바 데이터: [타임프레임, 바 데이터]
  shared_ptr<BarData> mark_price_bar_data_;  // 마크 가격 바 데이터

  // ===========================================================================
  vector<size_t>* trading_indices_;     // 각 심볼의 트레이딩 바 인덱스
  vector<size_t>* magnifier_indices_;   // 각 심볼의 돋보기 바 인덱스
  vector<size_t>* mark_price_indices_;  // 각 심볼의 마크 가격 바 인덱스

  // ===========================================================================
  vector<size_t> funding_rates_indices_;     // 각 심볼의 펀딩 비율 인덱스
  vector<double> next_funding_rates_;        // 다음 펀딩 비율
  vector<int64_t> next_funding_times_;       // 다음 펀딩 시간
  vector<double> next_funding_mark_prices_;  // 다음 펀딩 시 사용하는 마크 가격

  // ProcessOhlc 함수에서 현재 가격 타입이 HIGH 또는 LOW일 경우 CLOSE에서 방향을
  // 계산할 수 있게 하기 위한 가격과 가격 타입 캐시
  vector<double> price_cache_;
  vector<PriceType> price_type_cache_;

  // ===========================================================================
  StrategyType current_strategy_type_;      // 현재 사용 중인 전략 실행 타입
  shared_ptr<OrderHandler> order_handler_;  // 주문 핸들러

  // ===========================================================================
  int64_t begin_open_time_;     // 전체 바 데이터의 가장 처음 Open Time
  int64_t end_close_time_;      // 전체 바 데이터의 가장 마지막 Close Time
  int64_t current_open_time_;   // 현재 진행 중인 Open Time
  int64_t current_close_time_;  // 현재 진행 중인 Close Time

  // ===========================================================================
  int64_t next_month_boundary_;  // 다음 월 경계 시간 (콘솔 로그 여부 결정)

  // ===========================================================================
  vector<bool> trading_began_;  // 심볼별로 트레이딩이 진행 중인지 결정
  vector<bool> trading_ended_;  // 심볼별로 트레이딩이 끝났는지 결정
  bool all_trading_ended_;  // 모든 심볼의 트레이딩이 끝났는지 결정하는 플래그

  // 현재 트레이딩 바 시간에서 트레이딩을 진행하는 심볼 인덱스
  vector<int> activated_symbol_indices_;

  // 심볼 이름들
  vector<string> symbol_names_;

  /// 백테스팅의 메인 로직 시작 전 엔진의 유효성 검사와 초기화를 하는 함수
  void Initialize();

  /// 엔진 설정의 유효성을 검증하는 함수
  static void IsValidConfig();

  /// 바 데이터의 유효성을 검증하는 함수
  static void IsValidBarData();

  /// Start, End의 시간 범위가 바 데이터 시간 범위 내인지 유효성을 검증하는 함수
  void IsValidDateRange();

  /// 펀딩 비율, 거래소 및 레버리지 정보의 유효성을 검증하는 함수
  static void IsValidSymbolInfo();

  /// 엔진에 추가된 전략의 유효성을 검증하는 함수
  void IsValidStrategy();

  /// 전략에서 사용하는 지표의 유효성을 검증하는 함수
  void IsValidIndicators();

  /// 엔진의 변수들을 초기화하는 함수
  void InitializeEngine();

  /// 거래소 정보에 따라 심볼 정보를 초기화하는 함수
  void InitializeSymbolInfo();

  /// 전략을 초기화하는 함수
  void InitializeStrategy();

  /// 전략에서 사용하는 지표들을 계산하고 저장하는 함수
  void InitializeIndicators() const;

  /// 백테스팅의 메인 로직을 실행하는 함수
  void BacktestingMain();

  /**
   * 모든 심볼에 대하여 현재 트레이딩 바 시간에서 트레이딩을 진행하는지
   * 확인하고 상태를 업데이트하는 함수.
   *
   * 돋보기 기능을 사용하지 않는다면 무조건 트레이딩 바에서, 돋보기 기능을
   * 사용한다면 무조건 돋보기 바에서 백테스팅이 진행됨.
   */
  void UpdateTradingStatus();

  /// 트레이딩이 끝난 심볼의 상태 변화와 체결된 진입 주문의
  /// 전량 청산을 하는 함수
  void ExecuteTradingEnd(int symbol_idx, const string& bar_data_type_str);

  // 트레이딩이 끝나지 않은 심볼의 종료를 위하여 상태 변화와 체결된 진입 주문의
  // 전량 청산을 하는 함수
  void ExecuteAllTradingEnd();

  /// 각 심볼의 펀딩 시간과 현재 시간을 비교하여 펀딩 시간이 됐다면 펀딩을
  /// 실행하는 함수
  void CheckFundingTime();

  /// 주어진 바 데이터 유형과 심볼들의 현재 바 인덱스에서 OHLC 가격을 기준으로
  /// 강제 청산 및 대기 중인 주문의 체결을 확인하는 함수
  void ProcessOhlc(BarDataType bar_data_type,
                   const vector<int>& symbol_indices);

  /// 주어진 바 데이터 유형과 심볼들의 현재 바 인덱스에서 마크 가격과
  /// 시장 가격의 시가, 고가/저가, 종가를 순서대로 확인하여
  /// 강제 청산 및 대기 중인 주문의 체결을 확인할 수 있도록
  /// 정보를 구조체 형태로 저장한 벡터를 반환하는 함수.
  [[nodiscard]] pair<vector<PriceData>, vector<PriceData>> GetPriceQueue(
      BarDataType market_bar_data_type,
      const vector<int>& symbol_indices) const;

  /// 전 가격에서 현재 가격으로 올 때의 가격 방향을 계산하는 함수
  [[nodiscard]] Direction CalculatePriceDirection(
      BarDataType bar_data_type, int symbol_idx, double current_price,
      PriceType current_price_type) const;

  /// 전 가격에서 현재 가격으로 올 때의 가격 방향과 체결 우선 순위에 따라
  /// 체결 순서대로 주문들을 정렬하는 함수
  ///
  /// 반환이 없고, 인수로 넣은 주문 벡터가 직접 정렬됨에 주의
  static void SortOrders(vector<FillInfo>& should_fill_orders,
                         Direction price_direction);

  /// 지정된 심볼에서 전략을 실행하는 함수
  void ExecuteStrategy(StrategyType strategy_type, int symbol_idx);

  /// 주문 체결 후 더 이상 추가 진입/청산이 발생하지 않을 때까지 AFTER 전략을
  /// 실행하는 함수
  void ExecuteChainedAfterStrategies(int symbol_idx);
};

}  // namespace backtesting::engine
