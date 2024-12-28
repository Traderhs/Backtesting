#pragma once

// 내부 헤더
#include "Engines/OrderManager.hpp"

// 네임 스페이스
using namespace std;

class OrderBuilder {
 public:
  // 각 필드를 설정하는 메서드
  OrderBuilder& SetEntryName(const string& entry_name);
  OrderBuilder& SetExitName(const string& exit_name);
  OrderBuilder& SetEntryDirection(OrderManager::Direction entry_direction);
  OrderBuilder& SetOrderedEntrySize(double ordered_entry_size);
  OrderBuilder& SetEntrySize(double entry_size);
  OrderBuilder& SetOrderedExitSize(double ordered_exit_size);
  OrderBuilder& SetExitSize(double exit_size);
  OrderBuilder& SetLeverage(unsigned char leverage);
  OrderBuilder& SetCommission(double commission);
  OrderBuilder& SetOrderType(OrderManager::OrderType order_type);
  OrderBuilder& SetOrderedEntryTime(int64_t ordered_entry_time);
  OrderBuilder& SetOrderedEntryPrice(double ordered_entry_price);
  OrderBuilder& SetEntryTime(int64_t entry_time);
  OrderBuilder& SetEntryPrice(double entry_price);
  OrderBuilder& SetOrderedExitTime(int64_t ordered_exit_time);
  OrderBuilder& SetOrderedExitPrice(double ordered_exit_price);
  OrderBuilder& SetExitTime(int64_t exit_time);
  OrderBuilder& SetExitPrice(double exit_price);
  OrderBuilder& SetMarginCallPrice(double margin_call_price);
  OrderBuilder& SetMaxProfit(double max_profit);
  OrderBuilder& SetMaxLoss(double max_loss);

  // 빌드 메서드
  OrderManager::order Build();

private:
  OrderManager::order result {};
};