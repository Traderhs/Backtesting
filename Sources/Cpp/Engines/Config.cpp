// 표준 라이브러리
#include <cmath>

// 파일 헤더
#include "Engines/Config.hpp"

Config::Config()
    : initial_balance_(nan("")),
      commission_type_(CommissionType::COMMISSION_NONE),
      market_commission_(nan("")),
      limit_commission_(nan("")),
      slippage_type_(SlippageType::SLIPPAGE_NONE),
      market_slippage_(nan("")),
      limit_slippage_(nan("")) {}
Config::~Config() = default;

Config& Config::SetRootDirectory(const string& root_directory) {
  root_directory_ = root_directory;
  return *this;
}

Config& Config::SetInitialBalance(const double initial_balance) {
  initial_balance_ = initial_balance;
  return *this;
}

Config& Config::SetCommissionType(const CommissionType commission_type) {
  commission_type_ = commission_type;
  return *this;
}

Config& Config::SetMarketCommission(const double market_commission) {
  market_commission_ = market_commission;
  return *this;
}

Config& Config::SetLimitCommission(const double limit_commission) {
  limit_commission_ = limit_commission;
  return *this;
}

Config& Config::SetSlippageType(const SlippageType slippage_type) {
  slippage_type_ = slippage_type;
  return *this;
}

Config& Config::SetMarketSlippage(const double market_slippage) {
  market_slippage_ = market_slippage;
  return *this;
}

Config& Config::SetLimitSlippage(const double limit_slippage) {
  limit_slippage_ = limit_slippage;
  return *this;
}

string Config::GetRootDirectory() const { return root_directory_; }
double Config::GetInitialBalance() const { return initial_balance_; }
CommissionType Config::GetCommissionType() const { return commission_type_; }
double Config::GetMarketCommission() const { return market_commission_; }
double Config::GetLimitCommission() const { return limit_commission_; }
SlippageType Config::GetSlippageType() const { return slippage_type_; }
double Config::GetMarketSlippage() const { return market_slippage_; }
double Config::GetLimitSlippage() const { return limit_slippage_; }