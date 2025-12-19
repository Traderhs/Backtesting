#pragma once

// 표준 라이브러리
#include <string>

// 네임 스페이스
using namespace std;

namespace backtesting::analyzer {

/// 하나의 청산된 주문 정보를 저장하는 빌더 클래스
class Trade final {
 public:
  Trade();
  ~Trade();

  // 기본 정보
  Trade& SetTradeNumber(int trade_number);
  Trade& SetSymbolName(const string& symbol_name);
  Trade& SetEntryName(const string& entry_name);
  Trade& SetExitName(const string& exit_name);
  Trade& SetEntryDirection(const string& entry_direction);

  // 시간 정보
  Trade& SetEntryTime(const string& entry_time);
  Trade& SetExitTime(const string& exit_time);
  Trade& SetHoldingTime(const string& holding_time);

  // 진입 정보
  Trade& SetLeverage(int leverage);
  Trade& SetEntryPrice(double entry_price);
  Trade& SetEntrySize(double entry_size);

  // 청산 정보
  Trade& SetExitPrice(double exit_price);
  Trade& SetExitSize(double exit_size);
  Trade& SetLiquidationPrice(double liquidation_price);

  // 자금 정보
  Trade& SetReceivedFundingCount(int received_funding_count);
  Trade& SetReceivedFundingAmount(double received_funding_amount);
  Trade& SetPaidFundingCount(int paid_funding_count);
  Trade& SetPaidFundingAmount(double paid_funding_amount);
  Trade& SetTotalFundingCount(int total_funding_count);
  Trade& SetTotalFundingAmount(double total_funding_amount);
  Trade& SetEntryFee(double entry_fee);
  Trade& SetExitFee(double exit_fee);
  Trade& SetLiquidationFee(double liquidation_fee);
  Trade& SetPnl(double pnl);
  Trade& SetNetPnl(double net_pnl);
  Trade& SetIndividualPnlPer(double individual_pnl_per);
  Trade& SetTotalPnlPer(double total_pnl_per);
  Trade& SetWalletBalance(double wallet_balance);
  Trade& SetMaxWalletBalance(double max_wallet_balance);
  Trade& SetDrawdown(double drawdown);
  Trade& SetMaxDrawdown(double max_drawdown);
  Trade& SetCumPnl(double cum_pnl);
  Trade& SetCumPnlPer(double cum_pnl_per);

  // 기타 정보
  Trade& SetSymbolCount(int symbol_count);

  // ======================================================
  // 기본 정보
  [[nodiscard]] int GetTradeNumber() const;
  [[nodiscard]] string GetSymbolName() const;
  [[nodiscard]] string GetEntryName() const;
  [[nodiscard]] string GetExitName() const;
  [[nodiscard]] string GetEntryDirection() const;

  // 시간 정보
  [[nodiscard]] string GetEntryTime() const;
  [[nodiscard]] string GetExitTime() const;
  [[nodiscard]] string GetHoldingTime() const;

  // 진입 정보
  [[nodiscard]] int GetLeverage() const;
  [[nodiscard]] double GetEntryPrice() const;
  [[nodiscard]] double GetEntrySize() const;

  // 청산 정보
  [[nodiscard]] double GetExitSize() const;
  [[nodiscard]] double GetExitPrice() const;
  [[nodiscard]] double GetLiquidationPrice() const;

  // 자금 정보
  [[nodiscard]] int GetReceivedFundingCount() const;
  [[nodiscard]] double GetReceivedFundingAmount() const;
  [[nodiscard]] int GetPaidFundingCount() const;
  [[nodiscard]] double GetPaidFundingAmount() const;
  [[nodiscard]] int GetTotalFundingCount() const;
  [[nodiscard]] double GetTotalFundingAmount() const;
  [[nodiscard]] double GetEntryFee() const;
  [[nodiscard]] double GetExitFee() const;
  [[nodiscard]] double GetLiquidationFee() const;
  [[nodiscard]] double GetPnl() const;
  [[nodiscard]] double GetNetPnl() const;
  [[nodiscard]] double GetIndividualPnlPer() const;
  [[nodiscard]] double GetTotalPnlPer() const;
  [[nodiscard]] double GetWalletBalance() const;
  [[nodiscard]] double GetMaxWalletBalance() const;
  [[nodiscard]] double GetDrawdown() const;
  [[nodiscard]] double GetMaxDrawdown() const;
  [[nodiscard]] double GetCumPnl() const;
  [[nodiscard]] double GetCumPnlPer() const;

  // 기타 정보
  [[nodiscard]] int GetSymbolCount() const;

 private:
  // 기본 정보
  int trade_number_;        // 거래 번호
  string symbol_name_;      // 심볼 이름
  string entry_name_;       // 진입 이름
  string exit_name_;        // 청산 이름
  string entry_direction_;  // 진입 방향

  // 시간 정보
  string entry_time_;    // 진입 시간
  string exit_time_;     // 청산 시간
  string holding_time_;  // 보유 시간

  // 진입 정보
  int leverage_;        // 레버리지
  double entry_price_;  // 진입 가격
  double entry_size_;   // 진입 수량

  // 청산 정보
  double exit_price_;         // 청산 가격
  double exit_size_;          // 청산 수량
  double liquidation_price_;  // 강제 청산 가격

  // 자금 정보
  int received_funding_count_;      // 펀딩비 수령 횟수
  double received_funding_amount_;  // 펀딩비 수령
  int paid_funding_count_;          // 펀딩비 지불 횟수
  double paid_funding_amount_;      // 펀딩비 지불
  int total_funding_count_;         // 펀딩 횟수
  double total_funding_amount_;     // 펀딩비
  double entry_fee_;                // 진입 수수료
  double exit_fee_;                 // 청산 수수료
  double liquidation_fee_;          // 강제 청산 수수료
  double pnl_;                      // 손익
  double net_pnl_;                  // 순손익
  double individual_pnl_per_;       // 진입 마진 대비 순손익률
  double total_pnl_per_;            // 진입 주문 시점의 지갑 자금 대비 순손익률
  double wallet_balance_;           // 현재 자금
  double max_wallet_balance_;       // 최고 자금
  double drawdown_;                 // 드로우다운
  double max_drawdown_;             // 최고 드로우다운
  double cum_pnl_;                  // 초기 자본금 대비 누적 손익
  double cum_pnl_per_;              // 초기 자본금 대비 누적 손익률

  // 기타 정보
  int symbol_count_;  // 보유 심볼 수
};

}  // namespace backtesting::analyzer