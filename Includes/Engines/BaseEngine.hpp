#pragma once

// 표준 라이브러리
#include <string>
#include <vector>

// 전방 선언
class BarHandler;
class Strategy;
enum class BarType;

// 내부 헤더
#include "Engines\Config.hpp"
#include "Engines\Logger.hpp"

// 네임 스페이스
using namespace std;

/// 엔진의 기본적인 설정, 초기화를 담당하는 클래스
class BaseEngine {
 public:
  bool debug_mode_ = true;  // 디버그 로그가 기록되는 모드

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

  void AddStrategy(const shared_ptr<Strategy>& strategy);

  /// 현재 자금을 증가시키는 함수.
  /// 현재 자금 증가 시 진입 가능 자금도 영향을 받으므로 같이 증가.
  void IncreaseWalletBalance(double increase_balance);

  /// 현재 자금을 감소시키는 함수 (양수로 지정).
  /// 현재 자금 감소 시 진입 가능 자금도 영향을 받으므로 같이 감소.
  void DecreaseWalletBalance(double decrease_balance);

  /// 주문 가능 자금을 증가시키는 함수
  void IncreaseAvailableBalance(double increase_balance);

  /// 주문 가능 자금을 감소시키는 함수 (양수로 지정)
  void DecreaseAvailableBalance(double decrease_balance);

  /// 엔진 설정을 반환하는 함수
  [[nodiscard]] Config GetConfig() const;

  /// 현재 자금을 반환하는 함수
  [[nodiscard]] double GetCurrentBalance() const;

  /// 주문 가능 자금을 반환하는 함수
  [[nodiscard]] double GetAvailableBalance() const;

  /// 설정된 심볼의 최소 틱 단위를 반환하는 함수
  [[nodiscard]] double GetTickSize(int symbol_idx) const;

 protected:
  explicit BaseEngine(const Config& config);
  ~BaseEngine();

  static shared_ptr<BarHandler>& bar_;
  static shared_ptr<Logger>& logger_;

  /// 엔진에 추가된 전략
  vector<shared_ptr<Strategy>> strategies_;

 private:
  /// 자금 관련 사전 설정 항목
  Config config_;

  /// 자금 관련 중도 설정 항목

  /// 현재 자금 = 초기 자금 ± 실현 손익 ± 펀딩피 - 수수료
  double wallet_balance_;

  /// 주문 가능 자금 = 현재 자금 ± 미실현 손익 - 마진 자금(진입 증거금 + 예약 증거금)
  double available_balance_;

  double max_wallet_balance_;  /// 최고 자금
  double min_wallet_balance_;  /// 최저 자금
  double drawdown_;            /// 현재 드로우다운
  double max_drawdown_;        /// 최고 드로우다운
  int liquidations_;           /// 강제 청산 횟수
  // @@@@@@@@@@@@@@@@ 마진콜 X ->
  // 유지 증거금 계산 메커니즘 도입 -> 진입 금액에 따라 진입 레버리지 제한 -> api로 받아와야함
  // 유지 증거금 -> 강제 청산 가격 계산
  // 강제 청산시 보험 기금 감소됨 -> 이것도 만들기

  // 심볼 정보
  vector<double> tick_size_;  // 심볼의 최소 틱 단위: 심볼 인덱스<틱 단위>
};