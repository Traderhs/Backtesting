#pragma once

// 표준 라이브러리
#include <string>
#include <vector>

// 전방 선언
class Analyzer;
class BarHandler;
class Strategy;
enum class BarType;

// 내부 헤더
#include "Engines/Config.hpp"
#include "Engines/Logger.hpp"

// 네임 스페이스
using namespace std;

/// 엔진의 기본적인 설정, 초기화를 담당하는 클래스
class BaseEngine {
 public:
  /**
   * 주어진 파일 경로에서 Parquet 데이터를 읽고
   * 지정된 바 타입으로 처리하여 핸들러에 추가하는 함수
   *
   * @param symbol_name 심볼 이름
   * @param file_path Parquet 파일의 경로
   * @param bar_type 추가할 데이터의 바 타입
   * @param columns 파일에서 데이터를 추출할 컬럼의 인덱스를 다음 순서로 지정
   *                [Open Time, Open, High, Low, Close, Volume, Close Time]
   */
  static void AddBarData(const string& symbol_name, const string& file_path,
                         BarType bar_type,
                         const vector<int>& columns = {0, 1, 2, 3, 4, 5, 6});

  /// 엔진에 전략을 추가하는 함수
  void AddStrategy(const shared_ptr<Strategy>& strategy);

  /// 엔진 설정을 세팅하는 함수
  void SetConfig(const Config& config);

  // ==========================================================================
  /// 지갑 자금을 증가시키는 함수.
  bool IncreaseWalletBalance(double increase_balance);

  /// 지갑 자금을 감소시키는 함수 (양수로 지정).
  bool DecreaseWalletBalance(double decrease_balance);

  /// 사용 가능 자금을 감소시키는 함수 (양수로 지정)
  bool DecreaseAvailableBalance(double decrease_balance);

  /// 사용한 마진을 증가시키는 함수
  void IncreaseUsedMargin(double increase_margin);

  /// 사용한 마진을 감소시키는 함수 (양수로 지정)
  void DecreaseUsedMargin(double decrease_margin);

  /// 파산을 당했을 때 설정하는 함수
  void SetBankruptcy();

  /// 엔진 설정을 반환하는 함수
  [[nodiscard]] Config GetConfig() const;

  /// 지갑 자금을 반환하는 함수
  [[nodiscard]] double GetWalletBalance() const;

  /// 최고 지갑 자금을 반환하는 함수
  [[nodiscard]] double GetMaxWalletBalance() const;

  // 현재 드로우다운을 반환하는 함수
  [[nodiscard]] double GetDrawdown() const;

  // 최고 드로우다운을 반환하는 함수
  [[nodiscard]] double GetMaxDrawdown() const;

  /// 자금 관련 통계 항목을 업데이트하는 함수
  void UpdateStatistics();

 protected:
  BaseEngine();
  ~BaseEngine();

  static shared_ptr<Analyzer>& analyzer_;
  static shared_ptr<BarHandler>& bar_;
  static shared_ptr<Logger>& logger_;

  /// 엔진에 추가된 전략
  vector<shared_ptr<Strategy>> strategies_;

  /// 자금 관련 사전 설정 항목
  Config config_;

  // 자금 관련 중도 설정 항목
  /// 지갑 자금 = 초기 자금 ± 실현 손익 ± 펀딩피 - 수수료
  double wallet_balance_;

  /// 사용 가능 자금 = 지갑 자금 ± 미실현 손익 - 사용한 마진
  double available_balance_;

  /// 미실현 손익 = 진입한 포지션의 손익의 합
  double unrealized_pnl_;

  /// 사용한 마진: 진입 증거금 + 예약 증거금
  double used_margin_;

  /// 파산 여부를 나타내는 플래그
  bool is_bankruptcy_;

  // 자금 관련 통계 항목
  double max_wallet_balance_;  /// 최고 자금
  double drawdown_;            /// 현재 드로우다운
  double max_drawdown_;        /// 최고 드로우다운
  int liquidations_;           /// 강제 청산 횟수
  // @@@@@@@@@@@@@@@@ 마진콜 X ->
  // 유지 증거금 계산 메커니즘 도입 -> 진입 금액에 따라 진입 레버리지 제한 -> api로 받아와야함
  // 유지 증거금 -> 강제 청산 가격 계산
  // 강제 청산시 보험 기금 감소됨 -> 이것도 만들기
};