#pragma once

// 표준 라이브러리
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

// 내부 헤더
#include "Engines/BarData.hpp"
#include "Engines/BaseEngine.hpp"

// 전방 선언
struct Bar;
class Strategy;
class SymbolInfo;

// 네임 스페이스
using namespace std;

// 가격 타입을 지정하는 열거형 클래스
enum class PriceType { OPEN = 0, HIGH, LOW, CLOSE };

// 각 가격의 정보를 담고 있는 구조체
struct PriceData {
  double price;
  PriceType price_type;
  int symbol_index;
};

// 전략 타입을 지정하는 열거형 클래스
enum class StrategyType { ON_CLOSE, AFTER_ENTRY, AFTER_EXIT };

/**
 * 백테스팅 프로세스를 진행하는 클래스
 */
class Engine final : public BaseEngine {
 public:
  // 싱글톤 특성 유지
  Engine(const Engine&) = delete;             // 복사 생성자 삭제
  Engine& operator=(const Engine&) = delete;  // 대입 연산자 삭제

  /// Engine의 싱글톤 인스턴스를 반환하는 함수
  static shared_ptr<Engine>& GetEngine();

  // @@@@@@@@@@@ 펀딩피 fetch하고 추가/감소되는 매커니즘 필요
  //
  //  // @@@@@@@@@ 매개변수 등의 전체 설정 저장은 백테스팅별로 나눠서 txt 파일에
  // 설정값 적도록 하자 (전략 코드도 함께)

  /**
   * 백테스팅을 실행하는 함수
   *
   * @param start_time: 백테스팅 시작 시간
   * @param end_time: 백테스팅 끝 시간
   * @param format start_time, end_time의 시간 포맷
   */
  void Backtesting(const string& start_time = "", const string& end_time = "",
                   const string& format = "%Y-%m-%d %H:%M:%S");

  /// 사용 가능 자금을 업데이트하고 반환하는 함수
  double UpdateAvailableBalance();

  /// 현재 사용 중인 전략의 실행 타입을 설정하는 함수
  void SetCurrentStrategyType(StrategyType strategy_type);

  /// 현재 사용 중인 전략의 이름을 반환하는 함수
  [[nodiscard]] string GetCurrentStrategyName() const;

  /// 현재 사용 중인 전략의 실행 타입을 반환하는 함수
  [[nodiscard]] StrategyType GetCurrentStrategyType() const;

  /// 현재 사용 중인 바 데이터 현재 인덱스의 Open Time을 반환하는 함수
  [[nodiscard]] int64_t GetCurrentOpenTime() const;

  /// 현재 사용 중인 바 데이터 현재 인덱스의 Close Time을 반환하는 함수
  [[nodiscard]] int64_t GetCurrentCloseTime() const;

 private:
  // 싱글톤 인스턴스 관리
  explicit Engine();
  class Deleter {
   public:
    void operator()(const Engine* p) const;
  };

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
  int trading_bar_num_symbols_;      // 트레이딩 바 심볼 개수
  string trading_bar_timeframe_;     // 트레이딩 바 타임프레임
  int64_t trading_bar_time_diff_;    // 트레이딩 바 사이의 타임스탬프 차이
  int64_t magnifier_bar_time_diff_;  // 돋보기 바 사이의 타임스탬프 차이

  // ===========================================================================
  vector<SymbolInfo> symbol_info_;  // 심볼 정보

  // ===========================================================================
  string current_strategy_name_;        // 현재 사용 중인 전략 이름
  StrategyType current_strategy_type_;  // 현재 사용 중인 전략 실행 타입

  // ===========================================================================
  int64_t begin_open_time_;     // 전체 바 데이터의 가장 처음 Open Time
  int64_t end_open_time_;       // 전체 바 데이터의 가장 마지막 Open Time
  int64_t current_open_time_;   // 현재 사용 중인 바 인덱스의 Open Time
  int64_t current_close_time_;  // 현재 사용 중인 바 인덱스의 Close Time

  // ===========================================================================
  vector<bool> trading_began_;  // 심볼별로 트레이딩이 진행 중인지 결정
  vector<bool> trading_ended_;  // 심볼별로 트레이딩이 끝났는지 결정

  // 현재 바에서 트레이딩을 진행하는 심볼 인덱스
  vector<int> activated_symbol_indices_;
  vector<int> activated_magnifier_symbol_indices_;
  vector<int> activated_trading_symbol_indices_;

  /// 백테스팅의 메인 로직 시작 전 엔진의 유효성 검사와 초기화를 하는 함수
  void Initialize(const string& start_time, const string& end_time,
                  const string& format);

  /// 엔진 설정의 유효성을 검증하는 함수
  static void IsValidConfig();

  /// 거래소 및 레버리지 정보의 유효성을 검증하는 함수
  static void IsValidSymbolInfo();

  /// 바 데이터의 유효성을 검증하는 함수
  static void IsValidBarData();

  /// Start, End의 시간 범위가 바 데이터 시간 범위 내인지 유효성을 검증하는 함수
  void IsValidDateRange(const string& start_time, const string& end_time,
                        const string& format);

  /// 엔진에 추가된 전략의 유효성을 검증하는 함수
  void IsValidStrategies();

  /// 전략에서 사용하는 지표의 유효성을 검증하는 함수
  void IsValidIndicators();

  /// 저장에 필요한 폴더들을 생성하는 함수
  void CreateDirectories();

  /// 엔진의 변수들을 초기화하는 함수
  void InitializeEngine();

  /// 거래소 정보에 따라 심볼 정보를 초기화하는 함수
  void InitializeSymbolInfo();

  /// 주어진 Json에서 주어진 키를 찾고 Double로 반환하는 함수
  [[nodiscard]] static double GetDoubleFromJson(const json& data,
                                                const string& key);

  /// 전략들을 초기화하는 함수
  void InitializeStrategies() const;

  /// 전략에서 사용하는 지표들을 계산하고 저장하는 함수
  void InitializeIndicators() const;

  /// 백테스팅의 메인 로직을 실행하는 함수
  void BacktestingMain();

  /**
   * 모든 심볼에 대하여 현재 트레이딩 바 인덱스에서 트레이딩을 진행하는지
   * 확인하고 상태를 업데이트하는 함수.
   *
   * 트레이딩을 진행하는 심볼들은 트레이딩 바 혹은 돋보기 바를 사용 가능하다면
   * 돋보기 바의 심볼 인덱스 벡터에 추가됨.
   */
  void UpdateTradingStatus();

  /// 트레이딩이 끝난 심볼의 상태 변화와 체결 진입 청산을 하는 함수
  void ExecuteTradingEnd(int symbol_idx);

  /// UpdateTradingStatus 함수에서 활성화된 심볼 인덱스에 추가할 때,
  /// 참조 바가 사용 가능한지 확인하고, 사용 가능하면 조건에 따라 트레이딩
  /// 바인지 돋보기 바인지 결정하고 추가하는 함수
  void DetermineActivation(int symbol_idx);

  /// 주어진 바 타입과 심볼들의 현재 바 인덱스에서 OHLC 가격을 기준으로
  /// 강제 청산 및 대기 중인 주문의 체결을 확인하는 함수
  void ProcessOhlc(BarType bar_type, const vector<int>& symbol_indices);

  /// 주어진 바 타입과 심볼들의 현재 바 인덱스에서 마크 가격과 시장 가격의 시가,
  /// 고가/저가, 종가를 순서대로 확인하여 강제 청산 및 대기 중인 주문의 체결을
  /// 확인할 수 있도록 정보를 구조체 형태로 저장한 벡터를 반환하는 함수.
  [[nodiscard]] static pair<vector<PriceData>, vector<PriceData>> GetPriceQueue(
      BarType market_bar_type, const vector<int>& symbol_indices);

  /// 지정된 전략과 심볼에서 전략을 실행하는 함수
  void ExecuteStrategy(const shared_ptr<Strategy>& strategy,
                       StrategyType strategy_type, int symbol_index);
};
