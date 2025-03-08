// 표준 라이브러리
#include <cmath>

// 파일 헤더
#include "Engines/SymbolInfo.hpp"

SymbolInfo::SymbolInfo()
    : tick_size_(nan("")),
      limit_max_qty_(nan("")),
      limit_min_qty_(nan("")),
      market_max_qty_(nan("")),
      market_min_qty_(nan("")),
      qty_step_(nan("")),
      min_notional_(nan("")),
      liquidation_fee_(nan("")) {}
SymbolInfo::~SymbolInfo() = default;

SymbolInfo& SymbolInfo::SetTickSize(const double tick_size) {
  tick_size_ = tick_size;
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

SymbolInfo& SymbolInfo::SetQtyStep(const double qty_step) {
  qty_step_ = qty_step;
  return *this;
}

SymbolInfo& SymbolInfo::SetMinNotional(const double min_notional) {
  min_notional_ = min_notional;
  return *this;
}

SymbolInfo& SymbolInfo::SetLiquidationFee(const double liquidation_fee) {
  liquidation_fee_ = liquidation_fee;
  return *this;
}

SymbolInfo& SymbolInfo::SetLeverageBracket(
    const vector<LeverageBracket>& leverage_bracket) {
  leverage_brackets_ = leverage_bracket;
  return *this;
}

double SymbolInfo::GetTickSize() const { return tick_size_; }
double SymbolInfo::GetLimitMaxQty() const { return limit_max_qty_; }
double SymbolInfo::GetLimitMinQty() const { return limit_min_qty_; }
double SymbolInfo::GetMarketMaxQty() const { return market_max_qty_; }
double SymbolInfo::GetMarketMinQty() const { return market_min_qty_; }
double SymbolInfo::GetQtyStep() const { return qty_step_; }
double SymbolInfo::GetMinNotional() const { return min_notional_; }
double SymbolInfo::GetLiquidationFee() const { return liquidation_fee_; }
vector<LeverageBracket>& SymbolInfo::GetLeverageBracket() {
  return leverage_brackets_;
}
