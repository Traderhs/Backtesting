// 내부 헤더
#include "Engines/Engine.hpp"

// 파일 헤더
#include "Engines/OrderManager.hpp"

OrderManager::OrderManager() {
  position_size = 0;
  entries["Total"] = {};
  exits["Total"] = {};
}

OrderManager::~OrderManager() = default;

OrderManager& OrderManager::GetOrderManager() {
  if (!instance) {
    lock_guard lock(mutex);
    instance.reset(new OrderManager());
  }
  return *instance;
}

void OrderManager::entry(const string& symbol, const string& order_name,
                  Direction entry_direction, double order_size,
                  unsigned char leverage, OrderType order_type) {
  for (int i = 0; i < ordered_entries[symbol].size(); i++) {
    if (ordered_entries[symbol][i].ㅁ == order_name) {
    }
  }
}

mutex OrderManager::mutex;
unique_ptr<OrderManager> OrderManager::instance;