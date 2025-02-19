#pragma once

// 표준 라이브러리
#include <string>

// 네임 스페이스
using namespace std;

/// 하나의 청산된 주문 정보를 저장하는 빌더 클래스
class Trade final {
 public:
  Trade();
  ~Trade();

  Trade& SetTradeNumber(int trade_number);
  Trade& SetStrategyName(const string& strategy_name);
  Trade& SetSymbolName(const string& symbol_name);
  Trade& SetEntryName(const string& entry_name);
  Trade& SetExitName(const string& exit_name);
  Trade& SetEntryDirection(const string& entry_direction);
  Trade& SetEntryTime(const string& entry_time);
  Trade& SetExitTime(const string& exit_time);
  Trade& SetHoldingTime(const string& holding_time);
  Trade& SetEntrySize(double entry_size);
  Trade& SetExitSize(double exit_size);
  Trade& SetEntryPrice(double entry_price);
  Trade& SetExitPrice(double exit_price);
  Trade& SetLeverage(int leverage);
  Trade& SetEntryCommission(double entry_commission);
  Trade& SetExitCommission(double exit_commission);
  Trade& SetProfitLoss(double profit_loss);
  Trade& SetProfitLossPer(double profit_loss_per);
  Trade& SetWalletBalance(double wallet_balance);
  Trade& SetMaxWalletBalance(double max_wallet_balance);
  Trade& SetDrawdown(double drawdown);
  Trade& SetMaxDrawdown(double max_drawdown);
  Trade& SetSymbolCount(int symbol_count);

  [[nodiscard]] int GetTradeNumber() const;
  [[nodiscard]] string GetStrategyName() const;
  [[nodiscard]] string GetSymbolName() const;
  [[nodiscard]] string GetEntryName() const;
  [[nodiscard]] string GetExitName() const;
  [[nodiscard]] string GetEntryDirection() const;
  [[nodiscard]] string GetEntryTime() const;
  [[nodiscard]] string GetExitTime() const;
  [[nodiscard]] string GetHoldingTime() const;
  [[nodiscard]] double GetEntrySize() const;
  [[nodiscard]] double GetExitSize() const;
  [[nodiscard]] double GetEntryPrice() const;
  [[nodiscard]] double GetExitPrice() const;
  [[nodiscard]] int GetLeverage() const;
  [[nodiscard]] double GetEntryCommission() const;
  [[nodiscard]] double GetExitCommission() const;
  [[nodiscard]] double GetProfitLoss() const;
  [[nodiscard]] double GetProfitLossPer() const;
  [[nodiscard]] double GetWalletBalance() const;
  [[nodiscard]] double GetMaxWalletBalance() const;
  [[nodiscard]] double GetDrawdown() const;
  [[nodiscard]] double GetMaxDrawdown() const;
  [[nodiscard]] int GetSymbolCount() const;

 private:
  // 내부 변수
  int trade_number_;           // 거래 번호
  string strategy_name_;       // 전략 이름
  string symbol_name_;         // 심볼 이름
  string entry_name_;          // 진입 이름
  string exit_name_;           // 청산 이름
  string entry_direction_;     // 진입 방향
  string entry_time_;          // 진입 시간
  string exit_time_;           // 청산 시간
  string holding_time_;        // 보유 시간
  double entry_size_;          // 진입 수량
  double exit_size_;           // 청산 수량
  double entry_price_;         // 진입 가격
  double exit_price_;          // 청산 가격
  int leverage_;               // 레버리지
  double entry_commission_;    // 진입 수수료
  double exit_commission_;     // 청산 수수료
  double profit_loss_;         // 손익
  double profit_loss_per_;     // 손익률
  double wallet_balance_;      // 현재 자금
  double max_wallet_balance_;  // 최대 자금
  double drawdown_;            // 드로우다운
  double max_drawdown_;        // 최고 드로우다운
  int symbol_count_;           // 보유 심볼 수
};