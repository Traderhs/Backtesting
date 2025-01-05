#pragma once

// 내부 헤더
#include "BarHandler.hpp"
#include "Config.hpp"
#include "Logger.hpp"
#include "Strategy.hpp"

/// 엔진의 기본적인 설정, 초기화를 담당하는 클래스
class BaseEngine {
 public:
  /// 수수료 타입을 지정하는 열거형 클래스
  enum class CommissionType {
    COMMISSION_NONE,
    COMMISSION_PERCENTAGE,
  };

  /// 슬리피지 타입을 지정하는 열거형 클래스
  enum class SlippageType {
    SLIPPAGE_NONE,
    SLIPPAGE_PERCENTAGE,
  };

  /// 현재 자금을 증가시키는 함수
  void IncreaseWalletBalance(double increase_balance);

  /// 현재 자금을 감소시키는 함수 (양수로 지정)
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

  static BarHandler& bar_;
  static Logger& logger_;

  // 임시 위치 @@@@ 엔진에 추가된 전략
  vector<Strategy> strategies_;

 private:
  // 자금 관련 사전 설정 항목
  const Config& config_;

  // 자금 관련 중도 설정 항목

  // 현재 자금 = 초기 자금 ± 실현 손익 ± 펀딩피 - 수수료
  double wallet_balance_;

  // 주문 가능 자금 = 현재 자금 ± 미실현 손익 - 마진 자금(진입 증거금 + 예약 증거금)
  double available_balance_;
  double max_wallet_balance_;   // 최고 자금
  double min_wallet_balance_;   // 최저 자금
  double drawdown_;      // 현재 드로우다운
  double max_drawdown_;  // 최고 드로우다운
  int liquidations_;     // 강제 청산 횟수
  // @@@@@@@@@@@@@@@@ 마진콜 X ->
  // 유지 증거금 계산 메커니즘 도입 -> 진입 금액에 따라 레버리지 제한 -> api로 받아와야함
  // 유지 증거금 -> 강제 청산 가격 계산
  // 강제 청산시 보험 기금 감소됨 -> 이것도 만들기

  // 심볼 정보
  vector<double> tick_size_;  // 심볼의 최소 틱 단위: 심볼 인덱스<틱 단위>
};