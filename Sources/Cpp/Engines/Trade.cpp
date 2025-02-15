// 파일 헤더
#include "Engines/Trade.hpp"

Trade::Trade()
    : trade_number_(0),
      entry_size_(0.0),
      exit_size_(0.0),
      entry_price_(0.0),
      exit_price_(0.0),
      leverage_(0),
      entry_commission_(0.0),
      exit_commission_(0.0),
      profit_loss_(0.0),
      profit_loss_per_(0.0),
      wallet_balance_(0.0),
      max_wallet_balance_(0.0),
      drawdown_(0.0),
      max_drawdown_(0.0),
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

Trade& Trade::SetEntrySize(const double entry_size) {
  entry_size_ = entry_size;
  return *this;
}

Trade& Trade::SetExitSize(const double exit_size) {
  exit_size_ = exit_size;
  return *this;
}

Trade& Trade::SetEntryPrice(const double entry_price) {
  entry_price_ = entry_price;
  return *this;
}

Trade& Trade::SetExitPrice(const double exit_price) {
  exit_price_ = exit_price;
  return *this;
}

Trade& Trade::SetLeverage(const int leverage) {
  leverage_ = leverage;
  return *this;
}

Trade& Trade::SetEntryCommission(const double entry_commission) {
  entry_commission_ = entry_commission;
  return *this;
}

Trade& Trade::SetExitCommission(const double exit_commission) {
  exit_commission_ = exit_commission;
  return *this;
}

Trade& Trade::SetProfitLoss(const double profit_loss) {
  profit_loss_ = profit_loss;
  return *this;
}

Trade& Trade::SetProfitLossPer(const double profit_loss_per) {
  profit_loss_per_ = profit_loss_per;
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

int Trade::GetTradeNumber() const { return trade_number_; }
string Trade::GetSymbolName() const { return symbol_name_; }
string Trade::GetEntryName() const { return entry_name_; }
string Trade::GetExitName() const { return exit_name_; }
string Trade::GetEntryDirection() const { return entry_direction_; }
string Trade::GetEntryTime() const { return entry_time_; }
string Trade::GetExitTime() const { return exit_time_; }
string Trade::GetHoldingTime() const { return holding_time_; }
double Trade::GetEntrySize() const { return entry_size_; }
double Trade::GetExitSize() const { return exit_size_; }
double Trade::GetEntryPrice() const { return entry_price_; }
double Trade::GetExitPrice() const { return exit_price_; }
int Trade::GetLeverage() const { return leverage_; }
double Trade::GetEntryCommission() const { return entry_commission_; }
double Trade::GetExitCommission() const { return exit_commission_; }
double Trade::GetProfitLoss() const { return profit_loss_; }
double Trade::GetProfitLossPer() const { return profit_loss_per_; }
double Trade::GetWalletBalance() const { return wallet_balance_; }
double Trade::GetMaxWalletBalance() const { return max_wallet_balance_; }
double Trade::GetDrawdown() const { return drawdown_; }
double Trade::GetMaxDrawdown() const { return max_drawdown_; }
int Trade::GetSymbolCount() const { return symbol_count_; }