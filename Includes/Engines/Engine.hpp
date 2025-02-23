#pragma once

// 표준 라이브러리
#include <string>
#include <unordered_map>
#include <vector>

// 전방 선언
class Strategy;

// 내부 헤더
#include "Engines/BarData.hpp"
#include "Engines/BaseEngine.hpp"

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

  // @@@@@@@@@@@ 펀딩피 fetch하고 추가/감소되는 매커니즘 필요 (사용/미사용
  // 플래그 넣자)
  //
  //  // @@@@@@@@@ 매개변수 등의 전체 설정 저장은 백테스팅별로 나눠서 txt 파일에
  // 설정값 적도록 하자 (전략 코드도 함께)

  /**
   * 백테스팅을 실행하는 함수
   *
   * @param use_bar_magnifier 트레이딩 바 내부의 세부 움직임을 추적하는 돋보기
   *                          기능을 사용할지 결정
   * @param start: 백테스팅 바 데이터의 시작 시간
   * @param end: 백테스팅 바 데이터의 끝 시간
   * @param format start, end의 시간 포맷
   */
  void Backtesting(bool use_bar_magnifier = true, const string& start = "",
                   const string& end = "",
                   const string& format = "%Y-%m-%d %H:%M:%S");

  /// 현재 바에 사용 가능 자금을 업데이트 하지 않았다면 전략별 미실현 손익과
  /// 사용한 마진에 따라 사용 가능 자금을 업데이트하고 반환하는 함수
  double UpdateAvailableBalance();

  /// 해당되는 심볼 인덱스의 최대 소숫점 자리수를 반환하는 함수
  [[nodiscard]] size_t GetMaxDecimalPlace(int symbol_idx) const;

  /// 현재 사용 중인 전략의 이름을 반환하는 함수
  [[nodiscard]] string GetCurrentStrategyName() const;

  /// 현재 사용 중인 전략의 실행 타입을 반환하는 함수
  [[nodiscard]] string GetCurrentStrategyType() const;

  /// 현재 사용 중인 바 데이터 현재 인덱스의 Open Time을 반환하는 함수
  [[nodiscard]] int64_t GetCurrentOpenTime() const;

 private:
  // 싱글톤 인스턴스 관리
  explicit Engine();
  class Deleter {
   public:
    void operator()(const Engine* p) const;
  };

  static mutex mutex_;
  static shared_ptr<Engine> instance_;

  // 바 데이터
  shared_ptr<BarData> trading_bar_;
  shared_ptr<BarData> magnifier_bar_;
  shared_ptr<unordered_map<string, BarData>>
      reference_bar_;  // [타임프레임, 바 데이터]

  // ===========================================================================
  // 바 데이터 정보
  int trading_bar_num_symbols_;  /// 트레이딩 바 심볼 개수
  int64_t
      trading_bar_add_time_;  /// 트레이딩 바 인덱스 하나 증가 시 증가하는 시간
  string trading_bar_timeframe_;  /// 트레이딩 바 타임프레임

  /// 각 심볼의 최대 소숫점 자릿수
  /// 진입, 청산 시 정확한 가격에서 주문, 체결을 보장하기 위함
  vector<size_t> max_decimal_places_;

  /// 백테스팅에서 바 돋보기 기능을 사용하는지 결정하는 플래그
  bool use_bar_magnifier_;

  /// 현재 바에서 미실현 손익과 사용한 마진에 따라 사용 가능 자금이
  /// 업데이트 됐는지를 결정하는 플래그
  bool available_balance_updated_;

  // ===========================================================================
  // 전략 정보
  string current_strategy_name_;  /// 현재 사용 중인 전략 이름
  string current_strategy_type_;  /// 현재 사용 중인 전략 실행 타입

  // ===========================================================================
  // 트레이딩 시간 정보
  int64_t begin_open_time_;     /// 전체 바 데이터의 가장 처음 Open Time
  int64_t end_open_time_;       /// 전체 바 데이터의 가장 마지막 Open Time
  int64_t current_open_time_;   /// 현재 사용 중인 바 인덱스의 Open Time
  int64_t current_close_time_;  /// 현재 사용 중인 바 인덱스의 Close Time

  // ===========================================================================
  // 트레이딩 진행 여부
  vector<bool> trading_began_;  /// 심볼별로 트레이딩이 진행 중인지 결정
  vector<bool> trading_ended_;  /// 심볼별로 트레이딩이 끝났는지 결정

  // 현재 바에서 트레이딩을 진행하는 심볼들
  vector<int> activated_symbol_indices_;
  vector<int> activated_magnifier_symbol_indices_;
  vector<size_t> activated_magnifier_bar_indices_;
  vector<int> activated_trading_symbol_indices_;
  vector<size_t> activated_trading_bar_indices_;

  /// 백테스팅의 메인 로직 시작 전 엔진의 유효성 검사와 초기화를 하는 함수
  void Initialize(bool use_bar_magnifier, const string& start,
                  const string& end, const string& format);

  /// 바 데이터의 유효성을 검증하는 함수
  static void IsValidBarData(bool use_bar_magnifier);

  /// Start, End의 시간 범위가 바 데이터 시간 범위 내인지 유효성을 검증하는 함수
  void IsValidDateRange(const string& start, const string& end,
                        const string& format);

  /// 엔진에 추가된 전략의 유효성을 검증하는 함수
  void IsValidStrategies() const;

  /// 엔진 설정의 유효성을 검증하는 함수
  void IsValidConfig() const;

  /// 저장에 필요한 폴더들을 생성하는 함수
  void CreateDirectories();

  /// 엔진의 변수들을 초기화하는 함수
  void InitializeEngine(bool use_bar_magnifier);

  /// 전략에서 사용하는 지표들을 계산하고 저장하는 함수
  void InitializeIndicators() const;

  /// 백테스팅의 메인 로직을 실행하는 함수
  void BacktestingMain();

  /// 해당되는 심볼 인덱스의 트레이딩 바 데이터에서
  /// 최대 소숫점 자리수를 구하여 반환하는 함수
  [[nodiscard]] size_t CountMaxDecimalPlace(int symbol_idx) const;

  /**
   * 모든 심볼에 대하여 현재 트레이딩 바 인덱스에서 트레이딩을 진행하는지
   * 확인하고 상태를 업데이트하는 함수.
   *
   * 트레이딩을 진행하는 심볼들은 트레이딩 바 혹은 돋보기 바를 사용 가능하다면
   * 돋보기 바의 심볼 인덱스 벡터에 추가됨.
   */
  void UpdateTradingStatus();

  /// 활성화된 트레이딩 바 및 돋보기 바의 심볼 인덱스 벡터와 바 인덱스 벡터를
  /// 초기화하는 함수
  void ClearActivatedVectors();

  /// 백테스팅이 끝났는지 검증하는 함수
  [[nodiscard]] bool IsBacktestingEnd() const;

  /// UpdateTradingStatus 함수에서 활성화된 심볼 인덱스에 추가할 때,
  /// 참조 바가 사용 가능한지 확인하고, 사용 가능하면 조건에 따라 트레이딩
  /// 바인지 돋보기 바인지 결정하고 추가하는 함수
  void DetermineActivation(int symbol_idx, size_t bar_idx);

  /// 트레이딩 중인 심볼의 현재 바 인덱스에서 OHLC 가격을 기준으로
  /// 마진콜과 대기 중인 주문의 체결을 확인하는 함수
  void ProcessOhlc(const vector<int>& activated_symbols,
                   const vector<size_t>& activated_bar_indices);

  /// 주어진 바 데이터에서 각 심볼의 현재 인덱스에 해당하는 시가, 고가/저가,
  /// 종가를 순서대로 확인하여 마진콜 및 대기 주문 체결 여부를 검사할 수 있도록
  /// 정보를 구조체 형태로 저장한 벡터들을 array로 반환하는 함수.
  static vector<PriceData> GetPriceQueue(
      const BarData& bar_data, const vector<int>& activated_symbols,
      const vector<size_t>& activated_bar_indices);

  /// 지정된 전략과 심볼에서 전략을 실행하는 함수
  void ExecuteStrategy(const shared_ptr<Strategy>& strategy,
                       const string& strategy_type, int symbol_index);
};
