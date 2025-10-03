// 파일 헤더
#include "Engines/SymbolInfo.hpp"

namespace backtesting::order {

SymbolInfo::SymbolInfo()
    : price_step_(NAN),
      price_precision_(0),
      qty_step_(NAN),
      qty_precision_(0),
      limit_max_qty_(NAN),
      limit_min_qty_(NAN),
      market_max_qty_(NAN),
      market_min_qty_(NAN),
      min_notional_value_(NAN),
      liquidation_fee_rate_(NAN) {}
SymbolInfo::~SymbolInfo() = default;

SymbolInfo& SymbolInfo::SetExchangeInfoPath(const string& exchange_info_path) {
  exchange_info_path_ = exchange_info_path;
  return *this;
}

SymbolInfo& SymbolInfo::SetPriceStep(const double price_step) {
  price_step_ = price_step;
  return *this;
}

SymbolInfo& SymbolInfo::SetPricePrecision(const int price_precision) {
  price_precision_ = price_precision;
  return *this;
}

SymbolInfo& SymbolInfo::SetQtyStep(const double qty_step) {
  qty_step_ = qty_step;
  return *this;
}

SymbolInfo& SymbolInfo::SetQtyPrecision(const int qty_precision) {
  qty_precision_ = qty_precision;
  return *this;
}

SymbolInfo& SymbolInfo::SetLimitMaxQty(const double default_max_qty) {
  limit_max_qty_ = default_max_qty;
  return *this;
}

SymbolInfo& SymbolInfo::SetLimitMinQty(const double default_min_qty) {
  limit_min_qty_ = default_min_qty;
  return *this;
}

SymbolInfo& SymbolInfo::SetMarketMaxQty(const double market_max_qty) {
  market_max_qty_ = market_max_qty;
  return *this;
}

SymbolInfo& SymbolInfo::SetMarketMinQty(const double market_min_qty) {
  market_min_qty_ = market_min_qty;
  return *this;
}

SymbolInfo& SymbolInfo::SetMinNotionalValue(const double min_notional) {
  min_notional_value_ = min_notional;
  return *this;
}

SymbolInfo& SymbolInfo::SetLiquidationFeeRate(const double liquidation_fee) {
  liquidation_fee_rate_ = liquidation_fee;
  return *this;
}

SymbolInfo& SymbolInfo::SetLeverageBracketPath(
    const string& leverage_bracket_path) {
  leverage_bracket_path_ = leverage_bracket_path;
  return *this;
}

SymbolInfo& SymbolInfo::SetLeverageBracket(
    const vector<LeverageBracket>& leverage_bracket) {
  leverage_brackets_ = leverage_bracket;
  return *this;
}

SymbolInfo& SymbolInfo::SetFundingRatesPath(const string& funding_rates_path) {
  funding_rates_path_ = funding_rates_path;
  return *this;
}

SymbolInfo& SymbolInfo::SetFundingRates(
    const vector<FundingInfo>& funding_rates) {
  funding_rates_ = funding_rates;
  return *this;
}

string SymbolInfo::GetExchangeInfoPath() const { return exchange_info_path_; }
double SymbolInfo::GetPriceStep() const { return price_step_; }
int SymbolInfo::GetPricePrecision() const { return price_precision_; }
double SymbolInfo::GetQtyStep() const { return qty_step_; }
size_t SymbolInfo::GetQtyPrecision() const { return qty_precision_; }
double SymbolInfo::GetLimitMaxQty() const { return limit_max_qty_; }
double SymbolInfo::GetLimitMinQty() const { return limit_min_qty_; }
double SymbolInfo::GetMarketMaxQty() const { return market_max_qty_; }
double SymbolInfo::GetMarketMinQty() const { return market_min_qty_; }
double SymbolInfo::GetMinNotionalValue() const { return min_notional_value_; }
double SymbolInfo::GetLiquidationFeeRate() const {
  return liquidation_fee_rate_;
}

string SymbolInfo::GetLeverageBracketPath() const {
  return leverage_bracket_path_;
}
vector<LeverageBracket>& SymbolInfo::GetLeverageBracket() {
  return leverage_brackets_;
}

string SymbolInfo::GetFundingRatesPath() const { return funding_rates_path_; }
vector<FundingInfo>& SymbolInfo::GetFundingRates() { return funding_rates_; }

}  // namespace backtesting::order