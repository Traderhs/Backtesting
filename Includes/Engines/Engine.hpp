/**
 * © 2024 Traderhs. All rights reserved.
 *
 * ==============================================================================================
 *
 *
 *  ██████╗  █████╗  ██████╗██╗  ██╗████████╗███████╗███████╗████████╗██╗███╗
 * ██╗ ██████╗ ██╔══██╗██╔══██╗██╔════╝██║
 * ██╔╝╚══██╔══╝██╔════╝██╔════╝╚══██╔══╝██║████╗  ██║██╔════╝
 *  ██████╔╝███████║██║     █████╔╝    ██║   █████╗  ███████╗   ██║   ██║██╔██╗
 * ██║██║  ███╗ ██╔══██╗██╔══██║██║     ██╔═██╗    ██║   ██╔══╝  ╚════██║   ██║
 * ██║██║╚██╗██║██║   ██║ ██████╔╝██║  ██║╚██████╗██║  ██╗   ██║
 * ███████╗███████║   ██║   ██║██║ ╚████║╚██████╔╝ ╚═════╝ ╚═╝  ╚═╝ ╚═════╝╚═╝
 * ╚═╝   ╚═╝   ╚══════╝╚══════╝   ╚═╝   ╚═╝╚═╝  ╚═══╝ ╚═════╝
 *
 *
 *  ● 여러 자산에 대한 백테스팅을 지원하는 프로그램 ●
 *
 *  ◆ 다중 자산 포트폴리오 백테스팅
 *  ◆ 바 내부 움직임 추적
 *  ◆ 그래프 시각화 분석
 *  ◆ 성과 통계 분석
 *  ◆ 워크 포워드, 몬테카를로 시뮬레이션 등 고급 통계 분석 지원
 *
 * ==============================================================================================
 */

#pragma once

// 표준 라이브러리
#include <string>
#include <unordered_map>
#include <vector>

// 내부 헤더
#include "BarDataManager.hpp"
#include "DataManager.hpp"
#include "Logger.hpp"
#include "OrderManager.hpp"

// 네임 스페이스
using namespace std;

/**
 * 백테스팅 프로세스를 진행하는 메인 클래스
 */
class Engine final {
 public:
  Engine();
  ~Engine();
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

 private:
  static DataManager& data;    // 데이터 관리용 객체
  static BarDataManager& bar;  // 바 데이터 관리용 객체
  static Logger& logger;       // 로그용 객체
  static OrderManager& order;  // 주문용 객체

  int64_t begin_open_time;     // 전체 바 데이터의 가장 처음 Open Time
  int64_t end_open_time;       // 전체 바 데이터의 가장 마지막 Open Time

  int64_t current_open_time;   // 현재 트레이딩 바의 Open Time
  int64_t current_close_time;  // 현재 트레이딩 바의 Close Time

  unordered_map<string, bool> trading_began;   // 심볼별로 트레이딩을 진행하는 중인지 결정하는 플래그
  unordered_map<string, bool> trading_ended;   // 심볼별로 트레이딩이 끝났는지 결정하는 플래그

  /// BarDataManager의 유효성을 검증하는 함수
  static void IsValidBarData(bool use_bar_magnifier);

  /// DataManager의 설정 유효성을 검증하는 함수
  static void IsValidData();
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
  unordered_map<string, vector<BarDataManager::bar_data>> CheckTradingStatus();
};
