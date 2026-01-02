// 파일 헤더
#include "Engines/Trade.hpp"

namespace backtesting::analyzer {

Trade::Trade()
    : trade_number_(0),
      leverage_(0),
      entry_price_(0),
      entry_size_(0),
      exit_price_(0),
      exit_size_(0),
      liquidation_price_(0),
      received_funding_count_(0),
      received_funding_amount_(0),
      paid_funding_count_(0),
      paid_funding_amount_(0),
      total_funding_count_(0),
      total_funding_amount_(0),
      entry_fee_(0),
      exit_fee_(0),
      liquidation_fee_(0),
      pnl_(0),
      net_pnl_(0),
      individual_pnl_per_(0),
      total_pnl_per_(0),
      wallet_balance_(0),
      max_wallet_balance_(0),
      drawdown_(0),
      max_drawdown_(0),
      cum_pnl_(0),
      cum_pnl_per_(0),
      symbol_count_(0) {}
Trade::~Trade() = default;

Trade& Trade::SetTradeNumber(const int trade_number) {
  trade_number_ = trade_number;
  return *this;
}

Trade& Trade::SetSymbolName(const string& symbol_name) {
  symbol_name_ = symbol_name;
  return *this;
}

Trade& Trade::SetEntryName(const string& entry_name) {
  entry_name_ = entry_name;
  return *this;
}

Trade& Trade::SetExitName(const string& exit_name) {
  exit_name_ = exit_name;
  return *this;
}

Trade& Trade::SetEntryDirection(const string& entry_direction) {
  entry_direction_ = entry_direction;
  return *this;
}

Trade& Trade::SetEntryTime(const string& entry_time) {
  entry_time_ = entry_time;
  return *this;
}

Trade& Trade::SetExitTime(const string& exit_time) {
  exit_time_ = exit_time;
  return *this;
}

Trade& Trade::SetHoldingTime(const string& holding_time) {
  holding_time_ = holding_time;
  return *this;
}

Trade& Trade::SetLeverage(const int leverage) {
  leverage_ = leverage;
  return *this;
}

Trade& Trade::SetEntryPrice(const double entry_price) {
  entry_price_ = entry_price;
  return *this;
}

Trade& Trade::SetEntrySize(const double entry_size) {
  entry_size_ = entry_size;
  return *this;
}

Trade& Trade::SetExitPrice(const double exit_price) {
  exit_price_ = exit_price;
  return *this;
}

Trade& Trade::SetExitSize(const double exit_size) {
  exit_size_ = exit_size;
  return *this;
}

Trade& Trade::SetLiquidationPrice(const double liquidation_price) {
  liquidation_price_ = liquidation_price;
  return *this;
}

Trade& Trade::SetReceivedFundingCount(const int received_funding_count) {
  received_funding_count_ = received_funding_count;
  return *this;
}

Trade& Trade::SetReceivedFundingAmount(const double received_funding_amount) {
  received_funding_amount_ = received_funding_amount;
  return *this;
}

Trade& Trade::SetPaidFundingCount(const int paid_funding_count) {
  paid_funding_count_ = paid_funding_count;
  return *this;
}

Trade& Trade::SetPaidFundingAmount(const double paid_funding_amount) {
  paid_funding_amount_ = paid_funding_amount;
  return *this;
}

Trade& Trade::SetTotalFundingCount(const int total_funding_count) {
  total_funding_count_ = total_funding_count;
  return *this;
}

Trade& Trade::SetTotalFundingAmount(const double total_funding_amount) {
  total_funding_amount_ = total_funding_amount;
  return *this;
}

Trade& Trade::SetEntryFee(const double entry_fee) {
  entry_fee_ = entry_fee;
  return *this;
}

Trade& Trade::SetExitFee(const double exit_fee) {
  exit_fee_ = exit_fee;
  return *this;
}

Trade& Trade::SetLiquidationFee(const double liquidation_fee) {
  liquidation_fee_ = liquidation_fee;
  return *this;
}

Trade& Trade::SetPnl(const double pnl) {
  pnl_ = pnl;
  return *this;
}

Trade& Trade::SetNetPnl(const double net_pnl) {
  net_pnl_ = net_pnl;
  return *this;
}

Trade& Trade::SetIndividualPnlPer(const double individual_pnl_per) {
  individual_pnl_per_ = individual_pnl_per;
  return *this;
}

Trade& Trade::SetTotalPnlPer(const double total_pnl_per) {
  total_pnl_per_ = total_pnl_per;
  return *this;
}

Trade& Trade::SetWalletBalance(const double wallet_balance) {
  wallet_balance_ = wallet_balance;
  return *this;
}

Trade& Trade::SetMaxWalletBalance(const double max_wallet_balance) {
  max_wallet_balance_ = max_wallet_balance;
  return *this;
}

Trade& Trade::SetDrawdown(const double drawdown) {
  drawdown_ = drawdown;
  return *this;
}

Trade& Trade::SetMaxDrawdown(const double max_drawdown) {
  max_drawdown_ = max_drawdown;
  return *this;
}

Trade& Trade::SetSymbolCount(const int symbol_count) {
  symbol_count_ = symbol_count;
  return *this;
}

Trade& Trade::SetCumPnl(const double cum_pnl) {
  cum_pnl_ = cum_pnl;
  return *this;
}

Trade& Trade::SetCumPnlPer(const double cum_pnl_per) {
  cum_pnl_per_ = cum_pnl_per;
  return *this;
}

int Trade::GetTradeNumber() const { return trade_number_; }
string Trade::GetSymbolName() const { return symbol_name_; }
string Trade::GetEntryName() const { return entry_name_; }
string Trade::GetExitName() const { return exit_name_; }
string Trade::GetEntryDirection() const { return entry_direction_; }

string Trade::GetEntryTime() const { return entry_time_; }
string Trade::GetExitTime() const { return exit_time_; }
string Trade::GetHoldingTime() const { return holding_time_; }

int Trade::GetLeverage() const { return leverage_; }
double Trade::GetEntryPrice() const { return entry_price_; }
double Trade::GetEntrySize() const { return entry_size_; }

double Trade::GetExitPrice() const { return exit_price_; }
double Trade::GetExitSize() const { return exit_size_; }
double Trade::GetLiquidationPrice() const { return liquidation_price_; }

int Trade::GetReceivedFundingCount() const { return received_funding_count_; }
double Trade::GetReceivedFundingAmount() const {
  return received_funding_amount_;
}
int Trade::GetPaidFundingCount() const { return paid_funding_count_; }
double Trade::GetPaidFundingAmount() const { return paid_funding_amount_; }
int Trade::GetTotalFundingCount() const { return total_funding_count_; }
double Trade::GetTotalFundingAmount() const { return total_funding_amount_; }
double Trade::GetEntryFee() const { return entry_fee_; }
double Trade::GetExitFee() const { return exit_fee_; }
double Trade::GetLiquidationFee() const { return liquidation_fee_; }
double Trade::GetPnl() const { return pnl_; }
double Trade::GetNetPnl() const { return net_pnl_; }
double Trade::GetIndividualPnlPer() const { return individual_pnl_per_; }
double Trade::GetTotalPnlPer() const { return total_pnl_per_; }
double Trade::GetWalletBalance() const { return wallet_balance_; }
double Trade::GetMaxWalletBalance() const { return max_wallet_balance_; }
double Trade::GetDrawdown() const { return drawdown_; }
double Trade::GetMaxDrawdown() const { return max_drawdown_; }
double Trade::GetCumPnl() const { return cum_pnl_; }
double Trade::GetCumPnlPer() const { return cum_pnl_per_; }

int Trade::GetSymbolCount() const { return symbol_count_; }

}  // namespace backtesting::analyzer
