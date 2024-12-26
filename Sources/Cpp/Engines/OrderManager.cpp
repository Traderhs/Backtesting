// 표준 라이브러리
#include <format>
#include <ranges>

// 내부 헤더
#include "Engines/Builder.hpp"
#include "Engines/Engine.hpp"

// 파일 헤더
#include "Engines/OrderManager.hpp"

OrderManager::OrderManager() : current_position_size(0) {}

OrderManager::~OrderManager() = default;

OrderManager& OrderManager::GetOrderManager() {
  if (!instance) {
    lock_guard lock(mutex);
    instance.reset(new OrderManager());
  }
  return *instance;
}

void OrderManager::InitializeOrders() {
  // 트레이딩 심볼들로 초기화
  for (const auto& symbol : bar.GetTradingBarData() | views::keys) {
    ordered_entries[symbol] = {};
    entries[symbol] = {};
    ordered_exits[symbol] = {};
    exits[symbol] = {};
  }
}

void OrderManager::EntryMarket(const string& order_name,
                               const Direction entry_direction,
                               const double order_size,
                               const unsigned char leverage) {
  if (leverage < 1) {
    Logger::LogAndThrowError(
        "지정된 레버리지 " + to_string(leverage) + "은(는) 1보다 작습니다.",
        __FILE__, __LINE__);
  }

  if (!data.capital_updated_current_bar) {
    data.UpdateCapital(*this);  // @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
  }

  const string& entry_symbol = bar.current_symbol;
  const string& trading_tf = bar.GetTradingTimeframe();
  const double order_price = bar.current_close;
  const double entry_price = CalculateSlippagePrice(
      OrderType::MARKET, entry_direction, order_price, leverage);
  const double margin_call_price =
      CalculateMarginCallPrice(entry_direction, entry_price, leverage);
  const double commission =
      CalculateCommission(OrderType::MARKET, entry_price, order_size, leverage);

  // 진입 시간: 현재 바 종료 시간 ~= 다음 바의 시작 시간
  const int64_t entry_time =
      bar.GetBar(trading_tf, bar.GetCurrentIndex(entry_symbol, trading_tf) + 1)
          .open_time;

  OrderBuilder order_builder;
  const order& order = order_builder.SetEntryName(order_name)
                           .SetExitName("")
                           .SetEntryDirection(entry_direction)
                           .SetOrderedEntrySize(order_size)
                           .SetEntrySize(order_size)
                           .SetOrderedExitSize(0)
                           .SetExitSize(0)
                           .SetLeverage(leverage)
                           .SetCommission(commission)
                           .SetOrderType(OrderType::MARKET)
                           .SetOrderedEntryTime(entry_time)
                           .SetOrderedEntryPrice(order_price)
                           .SetEntryTime(entry_time)
                           .SetEntryPrice(entry_price)
                           .SetOrderedExitTime(-1)
                           .SetOrderedExitPrice(-1)
                           .SetExitTime(-1)
                           .SetExitPrice(-1)
                           .SetMarginCallPrice(margin_call_price)
                           .SetMaxProfit(0)
                           .SetMaxLoss(0)
                           .Build();

  // 해당 심볼에서 진입이 없으면 바로 진입
  if (entries[entry_symbol].empty()) {
    OrderEntryMarket(order, entry_symbol, entry_time, order_size, entry_price,
                     commission);
  } else {  // 이미 진입이 있을 때
    bool contain_opposite = false;  // 해당 주문과 반대 방향 주문이 있는지 체크

    for (const auto& entry : entries[entry_symbol]) {
      if (entry.entry_direction != entry_direction) {
        contain_opposite = true;
        break;
      }
    }

    // 반대 방향 진입이 없으면 그대로 진입
    if (!contain_opposite) {
      OrderEntryMarket(order, entry_symbol, entry_time, order_size, entry_price,
                       commission);
    } else {  // 반대 방향 진입이 있으면 모두 청산 후 진입
      Exit();
      OrderEntryMarket(order, entry_symbol, entry_time, order_size, entry_price,
                       commission);
    }
  }
}

mutex OrderManager::mutex;
unique_ptr<OrderManager> OrderManager::instance;

BarDataManager& OrderManager::bar = BarDataManager::GetBarDataManager();
DataManager& OrderManager::data = DataManager::GetDataManager();
Logger& OrderManager::logger = Logger::GetLogger();

void OrderManager::OrderEntryMarket(const order& order,
                                    const string& entry_symbol,
                                    const int64_t entry_time,
                                    const double order_size,
                                    const double entry_price,
                                    const double commission) {
  // 1포인트 == 1달러 가정 계산
  const double available_capital = data.GetAvailableCapital();
  const double needed_capital = order_size * entry_price + commission;

  if (available_capital < needed_capital) {
    logger.Log(Logger::WARNING_L,
               format("진입 가능한 자금이 부족합니다. | 주문 가능 자금: {} | "
                      "필요 자금: {}",
                      entry_symbol, entry_time, available_capital,
                      needed_capital + commission),
               __FILE__, __LINE__);
    return;
  }

  // 현재 자금에서 수수료 감소
  data.SetCurrentCapital(data.GetCurrentCapital() - commission);

  // 주문 가능 금액 감소
  data.SetAvailableCapital(available_capital - needed_capital);

  // 진입
  entries[entry_symbol].push_back(order);
}

void OrderManager::UpdateCapital() {
  if (data.capital_updated_current_bar) {
    logger.Log(Logger::WARNING_L,
               "해당 바에서 이미 자금이 업데이트 되었습니다.", __FILE__,
               __LINE__);
    return;
  }

  double profit_loss = 0;

  // entries의 손익 순회
  for (const auto& orders : entries | views::values) {
    for (const auto& order : orders) {
      //@@@@@@@@@@@@@@@@
    }
  }
}


double OrderManager::CalculateSlippagePrice(const OrderType order_type,
                                            const Direction direction,
                                            const double price,
                                            const unsigned char leverage) {
  double slippage;
  const double tick_size = data.GetTickSize(bar.current_symbol);

  // MARKET, LIMIT에 따라 슬리피지가 달라짐
  if (order_type == OrderType::MARKET) {
    // 슬리피지 포인트 계산
    if (data.GetSlippageType() ==
        DataManager::SlippageType::SLIPPAGE_PERCENTAGE) {
      slippage = price * data.GetSlippage().first / 100 * leverage;
    } else {
      slippage = data.GetSlippage().first;
    }

    // 방향에 따라 덧셈과 뺄셈이 달라짐
    if (direction == Direction::LONG)
      return RoundToTickSize(price + slippage, tick_size);

    if (direction == Direction::SHORT)
      return RoundToTickSize(price - slippage, tick_size);

  } else if (order_type == OrderType::LIMIT) {
    // 슬리피지 포인트 계산
    if (data.GetSlippageType() ==
        DataManager::SlippageType::SLIPPAGE_PERCENTAGE) {
      slippage = price * data.GetSlippage().second / 100 * leverage;
    } else {
      slippage = data.GetSlippage().second;
    }

    // 방향에 따라 덧셈과 뺄셈이 달라짐
    if (direction == Direction::LONG)
      return RoundToTickSize(price + slippage, tick_size);

    if (direction == Direction::SHORT)
      return RoundToTickSize(price - slippage, tick_size);
  }

  Logger::LogAndThrowError(
      "잘못된 order_type이 지정되었습니다. | MARKET or LIMIT", __FILE__,
      __LINE__);
  return nan("");
}

double OrderManager::CalculateCommission(const OrderType order_type,
                                         const double price,
                                         const double position_size,
                                         const unsigned char leverage) {
  // MARKET, LIMIT에 따라 수수료가 달라짐
  if (order_type == OrderType::MARKET) {
    if (data.GetCommissionType() ==
        DataManager::CommissionType::COMMISSION_PERCENTAGE) {
      return price * position_size * leverage *
             (data.GetCommission().first / 100);
    }

    return data.GetCommission().first;
  }

  if (order_type == OrderType::LIMIT) {
    if (data.GetCommissionType() ==
        DataManager::CommissionType::COMMISSION_PERCENTAGE) {
      return price * position_size * leverage *
             (data.GetCommission().second / 100);
    }

    return data.GetCommission().second;
  }

  return nan("");
}

double OrderManager::CalculateMarginCallPrice(const Direction direction,
                                              const double price,
                                              const unsigned char leverage) {
  const double margin_call_percentage = 100 / static_cast<double>(leverage);
  double margin_call_price = 0;

  if (direction == Direction::LONG) {
    margin_call_price = (1 - margin_call_percentage / 100) * price;
  } else if (direction == Direction::SHORT) {
    margin_call_price = (1 + margin_call_percentage / 100) * price;
  }

  return RoundToTickSize(margin_call_price,
                         data.GetTickSize(bar.current_symbol));
}

double OrderManager::RoundToTickSize(const double price, const double tick_size) {
  if (tick_size <= 0) {
    Logger::LogAndThrowError("틱 사이즈는 0보다 커야합니다. | 지정된 틱 사이즈: " + to_string(tick_size), __FILE__, __LINE__);
  }

  return round(price / tick_size) * tick_size;
}