#pragma once

// 표준 라이브러리
#include <utility>

// 내부 헤더
#include "Engines/Strategy.hpp"

class DiceSystem final : public Strategy {
 public:
  explicit DiceSystem(const string& name);
  ~DiceSystem() override;

  void Initialize() override;
  void ExecuteOnClose() override;
  void ExecuteBeforeEntry() override;
  void ExecuteAfterEntry() override;
  void ExecuteBeforeExit() override;
  void ExecuteAfterExit() override;

 private:
  EndOfDay& eod_;
  High& daily_high_;
  Low& daily_low_;

  SwingHigh& swing_high_;
  SwingLow& swing_low_;

  // 심볼 정보
  vector<SymbolInfo> symbol_info_;

  // 캐시된 현재 포지션 수량
  double current_position_size_;

  // 캐시된 지갑 자금
  double wallet_balance_;

  /* 지갑 자금에서의 최대 개별 매매 리스크 비율
   (수수료 등에 의해 초과할 수 있음) */
  double risk_ratio_per_trade_;

  // 심볼당 할당 가능한 지갑 자금 비율
  double balance_ratio_per_symbol_;

  // 롱/숏 스탑 로스 계수
  double long_stop_ratio_;
  double short_stop_ratio_;

  /**
   * 포지션 사이징 로직에 따라 포지션 크기와 레버리지를 계산하는 함수
   *
   * @param stop_loss_points 진입가와 손절가 사이의 가격 포인트 차이
   *                         → 1 Points = 1 USDT로 가정하고 계산
   * @param order_price 진입 주문 가격
   * @param entry_direction 진입 방향
   * @return {포지션크기, 레버리지} 쌍
   */
  pair<double, int> CalculatePositionSizeAndLeverage(double stop_loss_points,
                                                     double order_price,
                                                     Direction entry_direction);
};