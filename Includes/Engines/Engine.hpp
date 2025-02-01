#pragma once

// 표준 라이브러리
#include <string>
#include <vector>

// 전방 선언
class BarData;

// 내부 헤더
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

  // @@@@@@@@@ 봉 바뀔 때 false 로직 추가 필요
  //
  //  // @@@@@@@@@ 매개변수 등의 전체 설정 저장은 백테스팅별로 나눠서 txt 파일에
  // 설정값 적도록 하자

  /** @@@@@@@ 문서 수정 필
   * 백테스팅을 실행하는 함수입니다.
   *
   * @param start: 백테스팅 바 데이터의 시작 시간
   * @param end: 백테스팅 바 데이터의 끝 시간
   */  // @@@@@@@@@ 뭔가 초기화 안 된 데이터 있으면 오류 띄우게 처음에 Initialize로 검사
  void Backtesting(bool use_bar_magnifier = true, const string& start = "",
                   const string& end = "",
                   const string& format = "%Y-%m-%d %H:%M:%S");
  // @@@@@@@@@@@ 트레이딩바 종가에서는 전략 돌리고 돋보기는 매 바에서 대기 주문
  // 체크만 하면 될 듯

  /// 현재 바에 사용 가능 자금을 업데이트 하지 않았다면 전략별 미실현 손익과
  /// 사용한 마진에 따라 사용 가능 자금을 업데이트하고 반환하는 함수
  double GetAvailableBalance();

  /// 현재 사용 중인 바 데이터 현재 인덱스의 Open Time을 반환하는 함수
  [[nodiscard]] inline int64_t GetCurrentOpenTime() const;

 private:
  // 싱글톤 인스턴스 관리
  explicit Engine();
  class Deleter {
   public:
    void operator()(const Engine* p) const;
  };

  static mutex mutex_;
  static shared_ptr<Engine> instance_;

  // 트레이딩 시간 정보
  int64_t begin_open_time_;    // 전체 바 데이터의 가장 처음 Open Time
  int64_t end_open_time_;      // 전체 바 데이터의 가장 마지막 Open Time
  int64_t current_open_time_;  // 현재 사용 중인 바 데이터 인덱스의 Open Time

  // 트레이딩 진행 여부
  vector<bool> trading_began_;  // 심볼별로 트레이딩이 진행 중인지 결정
  vector<bool> trading_ended_;  // 심볼별로 트레이딩이 끝났는지 결정

  /// 현재 바에서 미실현 손익과 사용한 마진에 따라 사용 가능 자금이
  /// 업데이트 됐는지를 결정하는 플래그
  bool available_balance_updated_;

  // ===========================================================================
  /// 바 데이터의 유효성을 검증하는 함수
  static void IsValidBarData(bool use_bar_magnifier);

  /// 엔진 설정의 유효성을 검증하는 함수
  void IsValidConfig() const;

  /// Start, End의 시간 범위가 바 데이터 시간 범위 내인지 유효성을 검증하는 함수
  void IsValidDateRange(const string& start, const string& end,
                        const string& format);

  /// 엔진에 추가된 전략의 유효성을 검증하는 함수
  void IsValidStrategies() const;

  /// 백테스팅 전 엔진의 변수들을 초기화하는 함수
  void InitializeEngine();

  /// 모든 심볼에서 현재 트레이딩 바 인덱스에서 트레이딩을 진행하는지 확인하고
  /// 상태를 업데이트하며 트레이딩을 진행하는 심볼들의 벡터를 반환하는 함수
  vector<int> UpdateTradingStatus();

  /// 트레이딩 중인 심볼의 현재 바 인덱스에서 OHLC 가격을 기준으로
  /// 마진콜과 대기 중인 주문의 체결을 확인하는 함수
  void ProcessOhlc(const vector<int>& activated_symbols,
                   const vector<size_t>& activated_bar_indices);

  /// 주어진 바 데이터에서 각 심볼의 현재 인덱스에 해당하는 시가, 고가/저가, 종가를 순서대로 확인하여
  /// 마진콜 및 대기 주문 체결 여부를 검사할 수 있도록 정보를 구조체 형태로 저장한 벡터를 반환하는 함수.
  [[nodiscard]] static vector<PriceData> GetPriceQueue(const BarData& bar_data,
                                                       const vector<int>& activated_symbols,
                                                       const vector<size_t>& activated_bar_indices);
};
