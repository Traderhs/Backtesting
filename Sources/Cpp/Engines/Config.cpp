// 표준 라이브러리
#include <cmath>

// 파일 헤더
#include "Engines/Config.hpp"

Config::Config()
    : initial_balance_(nan("")),
      commission_type_(CommissionType::COMMISSION_NONE),
      commission_({nan(""), nan("")}),
      slippage_type_(SlippageType::SLIPPAGE_NONE),
      slippage_({nan(""), nan("")}) {}
Config::~Config() = default;

Config& Config::SetInitialBalance(const double initial_balance) {
  initial_balance_ = initial_balance;
  return *this;
}

Config& Config::SetCommissionType(const CommissionType commission_type) {
  commission_type_ = commission_type;
  return *this;
}

Config& Config::SetCommission(const pair<double, double>& commission) {
  commission_ = commission;
  return *this;
}

Config& Config::SetSlippageType(const SlippageType slippage_type) {
  slippage_type_ = slippage_type;
  return *this;
}

Config& Config::SetSlippage(const pair<double, double>& slippage) {
  slippage_ = slippage;
  return *this;
}

double Config::GetInitialBalance() const { return initial_balance_; }
CommissionType Config::GetCommissionType() const { return commission_type_; }
pair<double, double> Config::GetCommission() const { return commission_; }
SlippageType Config::GetSlippageType() const { return slippage_type_; }
pair<double, double> Config::GetSlippage() const { return slippage_; }