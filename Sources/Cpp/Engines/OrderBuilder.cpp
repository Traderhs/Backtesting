// 파일
#include "Engines/OrderBuilder.hpp"

OrderBuilder& OrderBuilder::SetEntryName(const string& entry_name) {
  result.entry_name = entry_name;
  return *this;
}

OrderBuilder& OrderBuilder::SetExitName(const string& exit_name) {
  result.exit_name = exit_name;
  return *this;
}

OrderBuilder& OrderBuilder::SetEntryDirection(
    const OrderManager::Direction entry_direction) {
  result.entry_direction = entry_direction;
  return *this;
}

OrderBuilder& OrderBuilder::SetOrderedEntrySize(
    const double ordered_entry_size) {
  result.ordered_entry_size = ordered_entry_size;
  return *this;
}

OrderBuilder& OrderBuilder::SetEntrySize(const double entry_size) {
  result.entry_size = entry_size;
  return *this;
}

OrderBuilder& OrderBuilder::SetOrderedExitSize(const double ordered_exit_size) {
  result.ordered_exit_size = ordered_exit_size;
  return *this;
}

OrderBuilder& OrderBuilder::SetExitSize(const double exit_size) {
  result.exit_size = exit_size;
  return *this;
}

OrderBuilder& OrderBuilder::SetLeverage(const unsigned char leverage) {
  result.leverage = leverage;
  return *this;
}

OrderBuilder& OrderBuilder::SetCommission(const double commission) {
  result.commission = commission;
  return *this;
}

OrderBuilder& OrderBuilder::SetOrderType(
    const OrderManager::OrderType order_type) {
  result.order_type = order_type;
  return *this;
}

OrderBuilder& OrderBuilder::SetOrderedEntryTime(
    const int64_t ordered_entry_time) {
  result.ordered_entry_time = ordered_entry_time;
  return *this;
}

OrderBuilder& OrderBuilder::SetOrderedEntryPrice(
    const double ordered_entry_price) {
  result.ordered_entry_price = ordered_entry_price;
  return *this;
}

OrderBuilder& OrderBuilder::SetEntryTime(const int64_t entry_time) {
  result.entry_time = entry_time;
  return *this;
}

OrderBuilder& OrderBuilder::SetEntryPrice(const double entry_price) {
  result.entry_price = entry_price;
  return *this;
}

OrderBuilder& OrderBuilder::SetOrderedExitTime(
    const int64_t ordered_exit_time) {
  result.ordered_exit_time = ordered_exit_time;
  return *this;
}

OrderBuilder& OrderBuilder::SetOrderedExitPrice(
    const double ordered_exit_price) {
  result.ordered_exit_price = ordered_exit_price;
  return *this;
}

OrderBuilder& OrderBuilder::SetExitTime(const int64_t exit_time) {
  result.exit_time = exit_time;
  return *this;
}

OrderBuilder& OrderBuilder::SetExitPrice(const double exit_price) {
  result.exit_price = exit_price;
  return *this;
}

OrderBuilder& OrderBuilder::SetMarginCallPrice(const double margin_call_price) {
  result.margin_call_price = margin_call_price;
  return *this;
}

OrderBuilder& OrderBuilder::SetMaxProfit(const double max_profit) {
  result.max_profit = max_profit;
  return *this;
}

OrderBuilder& OrderBuilder::SetMaxLoss(const double max_loss) {
  result.max_loss = max_loss;
  return *this;
}

OrderManager::order OrderBuilder::Build() { return result;
}
