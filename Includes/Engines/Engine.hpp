#pragma once

// 표준 라이브러리
#include <string>
#include <vector>

// 내부 헤더
#include "Engines\BaseEngine.hpp"

// 네임 스페이스
using namespace std;

/**
 * 백테스팅 프로세스를 진행하는 클래스
 */
class Engine final : public BaseEngine {
 public:
  // 싱글톤 특성 유지
  Engine(const Engine&) = delete;             // 복사 생성자 삭제
  Engine& operator=(const Engine&) = delete;  // 대입 연산자 삭제

  /// Engine의 싱글톤 인스턴스를 반환하는 함수
  static shared_ptr<Engine>& GetEngine(const Config& config = Config());

  // @@@@@@@@@@@ 펀딩피 fetch하고 추가/감소되는 매커니즘 필요 (사용/미사용
  // 플래그 넣자)

  /// 현재 바에서 진입 손익에 따라 자금이 업데이트 됐는지 결정하는 플래그
  bool unrealized_pnl_updated_;
  // @@@@@@@@@ 봉 바뀔 때 false 로직 추가 필요

  // @@@@@@@@@ 매개변수 등의 전체 설정 저장은 백테스팅별로 나눠서 txt 파일에
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

  /// 전략별 미실현 손익에 따라 주문 가능 자금을 업데이트하는 함수
  void UpdateUnrealizedPnl();

 private:
  // 싱글톤 인스턴스 관리
  explicit Engine(const Config& config);
  class Deleter {
  public:
    void operator()(Engine* p) const;
  };

  static mutex mutex_;
  static shared_ptr<Engine> instance_;

  // 트레이딩 정보
  int64_t begin_open_time_;  // 전체 바 데이터의 가장 처음 Open Time
  int64_t end_open_time_;    // 전체 바 데이터의 가장 마지막 Open Time

  int64_t current_open_time_;   // 현재 트레이딩 바의 Open Time
  int64_t current_close_time_;  // 현재 트레이딩 바의 Close Time

  vector<bool> trading_began_;  // 심볼별로 트레이딩이 진행 중인지 결정
  vector<bool> trading_ended_;  // 심볼별로 트레이딩이 끝났는지 결정

  /// 바 데이터의 유효성을 검증하는 함수
  static void IsValidBarData(bool use_bar_magnifier);


  // @@@@@@@@@@@@@@@@@@@@@@ Order, Strategy, Indicator 유효성 검사 만들기
  /// Start, End의 시간 범위가 바 데이터 시간 범위 내인지 유효성을 검증하는 함수
  void IsValidDateRange(const string& start, const string& end, const string& format);

  /// 엔진의 초기화 작업 전 미리 초기화 필요한 것들을 초기화하는 함수
  void PreInitializeEngine();

  /// 엔진의 백테스팅 전 트레이딩 변수들을 초기화하는 함수
  void InitializeEngine();

  /// sub_index를 추가된 심볼과 타임프레임에 맞게 초기화하는 함수
  void InitializeSubIndex();

  /// 각 심볼의 트레이딩 바 데이터에 대해 백테스팅이 시작됐는지 끝났는지 검사하고,
  /// 트레이딩 중인 심볼과 데이터의 map을 반환하는 함수
  //unordered_map<string, vector<BarDataManager::bar_data>> CheckTradingStatus();
};
