// 표준 라이브러리
#include <cmath>

// 파일 헤더
#include "Engines/Order.hpp"

Order::Order()
    : leverage_(-1),
      margin_call_price_(nan("")),
      max_profit_(nan("")),
      max_loss_(nan("")),
      entry_order_type_(BaseOrderHandler::OrderType::NONE),
      entry_direction_(BaseOrderHandler::Direction::NONE),
      entry_commission_(nan("")),
      entry_touch_price_(nan("")),
      entry_extreme_price_(nan("")),
      entry_trail_point_(nan("")),
      entry_order_time_(-1),
      entry_order_size_(0),
      entry_order_price_(nan("")),
      entry_filled_time_(-1),
      entry_filled_size_(0),
      entry_filled_price_(nan("")),
      exit_order_type_(BaseOrderHandler::OrderType::NONE),
      exit_direction_(BaseOrderHandler::Direction::NONE),
      exit_commission_(nan("")),
      exit_touch_price_(nan("")),
      exit_extreme_price_(nan("")),
      exit_trail_point_(nan("")),
      exit_order_time_(-1),
      exit_order_size_(0),
      exit_order_price_(nan("")),
      exit_filled_time_(-1),
      exit_filled_size_(0),
      exit_filled_price_(nan("")) {}
Order::~Order() = default;

Order& Order::SetLeverage(const unsigned char leverage) {
  leverage_ = leverage;
  return *this;
}

Order& Order::SetMarginCallPrice(const double margin_call_price) {
  margin_call_price_ = margin_call_price;
  return *this;
}

Order& Order::SetMaxProfit(const double max_profit) {
  max_profit_ = max_profit;
  return *this;
}

Order& Order::SetMaxLoss(const double max_loss) {
  max_loss_ = max_loss;
  return *this;
}

Order& Order::SetEntryName(const string& entry_name) {
  entry_name_ = entry_name;
  return *this;
}

Order& Order::SetEntryOrderType(const BaseOrderHandler::OrderType order_type) {
  entry_order_type_ = order_type;
  return *this;
}

Order& Order::SetEntryDirection(const BaseOrderHandler::Direction direction) {
  entry_direction_ = direction;
  return *this;
}

Order& Order::SetEntryCommission(const double commission) {
  entry_commission_ = commission;
  return *this;
}

Order& Order::SetEntryTouchPrice(const double touch_price) {
  entry_touch_price_ = touch_price;
  return *this;
}

Order& Order::SetEntryExtremePrice(const double extreme_price) {
  entry_extreme_price_ = extreme_price;
  return *this;
}

Order& Order::SetEntryTrailPoint(const double trail_point) {
  entry_trail_point_ = trail_point;
  return *this;
}

Order& Order::SetEntryOrderTime(const int64_t entry_order_time) {
  entry_order_time_ = entry_order_time;
  return *this;
}

Order& Order::SetEntryOrderSize(const double entry_order_size) {
  entry_order_size_ = entry_order_size;
  return *this;
}

Order& Order::SetEntryOrderPrice(const double entry_order_price) {
  entry_order_price_ = entry_order_price;
  return *this;
}

Order& Order::SetEntryFilledTime(const int64_t entry_filled_time) {
  entry_filled_time_ = entry_filled_time;
  return *this;
}

Order& Order::SetEntryFilledSize(const double entry_filled_size) {
  entry_filled_size_ = entry_filled_size;
  return *this;
}

Order& Order::SetEntryFilledPrice(const double entry_filled_price) {
  entry_filled_price_ = entry_filled_price;
  return *this;
}

Order& Order::SetExitName(const string& exit_name) {
  exit_name_ = exit_name;
  return *this;
}

Order& Order::SetExitOrderType(const BaseOrderHandler::OrderType order_type) {
  exit_order_type_ = order_type;
  return *this;
}

Order& Order::SetExitDirection(const BaseOrderHandler::Direction direction) {
  exit_direction_ = direction;
  return *this;
}

Order& Order::SetExitCommission(const double commission) {
  exit_commission_ = commission;
  return *this;
}

Order& Order::SetExitTouchPrice(const double touch_price) {
  exit_touch_price_ = touch_price;
  return *this;
}

Order& Order::SetExitExtremePrice(const double extreme_price) {
  exit_extreme_price_ = extreme_price;
  return *this;
}

Order& Order::SetExitTrailPoint(const double trail_point) {
  exit_trail_point_ = trail_point;
  return *this;
}

Order& Order::SetExitOrderTime(const int64_t exit_order_time) {
  exit_order_time_ = exit_order_time;
  return *this;
}

Order& Order::SetExitOrderSize(const double exit_order_size) {
  exit_order_size_ = exit_order_size;
  return *this;
}

Order& Order::SetExitOrderPrice(const double exit_order_price) {
  exit_order_price_ = exit_order_price;
  return *this;
}

Order& Order::SetExitFilledTime(const int64_t exit_filled_time) {
  exit_filled_time_ = exit_filled_time;
  return *this;
}

Order& Order::SetExitFilledSize(const double exit_filled_size) {
  exit_filled_size_ = exit_filled_size;
  return *this;
}

Order& Order::SetExitFilledPrice(const double exit_filled_price) {
  exit_filled_price_ = exit_filled_price;
  return *this;
}

unsigned char Order::GetLeverage() const { return leverage_; }
double Order::GetMarginCallPrice() const { return margin_call_price_; }
double Order::GetMaxProfit() const { return max_profit_; }
double Order::GetMaxLoss() const { return max_loss_; }
string Order::GetEntryName() const { return entry_name_; }
BaseOrderHandler::OrderType Order::GetEntryOrderType() const {
  return entry_order_type_;
}
BaseOrderHandler::Direction Order::GetEntryDirection() const {
  return entry_direction_;
}
double Order::GetEntryCommission() const { return entry_commission_; }
double Order::GetEntryTouchPrice() const { return entry_touch_price_; }
double Order::GetEntryExtremePrice() const { return entry_extreme_price_; }
double Order::GetEntryTrailPoint() const { return entry_trail_point_; }
int64_t Order::GetEntryOrderTime() const { return entry_order_time_; }
double Order::GetEntryOrderSize() const { return entry_order_size_; }
double Order::GetEntryOrderPrice() const { return entry_order_price_; }
int64_t Order::GetEntryFilledTime() const { return entry_filled_time_; }
double Order::GetEntryFilledSize() const { return entry_filled_size_; }
double Order::GetEntryFilledPrice() const { return entry_filled_price_; }
string Order::GetExitName() const { return exit_name_; }
BaseOrderHandler::OrderType Order::GetExitOrderType() const {
  return exit_order_type_;
}
BaseOrderHandler::Direction Order::GetExitDirection() const {
  return exit_direction_;
}
double Order::GetExitCommission() const { return exit_commission_; }
double Order::GetExitTouchPrice() const { return exit_touch_price_; }
double Order::GetExitExtremePrice() const { return exit_extreme_price_; }
double Order::GetExitTrailPoint() const { return exit_trail_point_; }
int64_t Order::GetExitOrderTime() const { return exit_order_time_; }
double Order::GetExitOrderSize() const { return exit_order_size_; }
double Order::GetExitOrderPrice() const { return exit_order_price_; }
int64_t Order::GetExitFilledTime() const { return exit_filled_time_; }
double Order::GetExitFilledSize() const { return exit_filled_size_; }
double Order::GetExitFilledPrice() const { return exit_filled_price_; }
