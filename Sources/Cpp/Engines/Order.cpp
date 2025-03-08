// 표준 라이브러리
#include <cmath>
#include <stdexcept>

// 파일 헤더
#include "Engines/Order.hpp"

Order::Order()
    : margin_(0),
      leverage_(-1),
      liquidation_price_(nan("")),
      liquidation_fee_(nan("")),

      entry_order_type_(OrderType::NONE),
      entry_direction_(Direction::NONE),
      entry_fee_(nan("")),

      entry_touch_price_(nan("")),
      entry_touch_direction_(Direction::NONE),
      entry_extreme_price_(nan("")),
      entry_trail_point_(nan("")),

      entry_order_time_(-1),
      entry_order_price_(nan("")),
      entry_order_size_(0),

      entry_filled_time_(-1),
      entry_filled_price_(nan("")),
      entry_filled_size_(0),

      exit_order_type_(OrderType::NONE),
      exit_direction_(Direction::NONE),
      exit_fee_(nan("")),

      exit_touch_price_(nan("")),
      exit_touch_direction_(Direction::NONE),
      exit_extreme_price_(nan("")),
      exit_trail_point_(nan("")),

      exit_order_time_(-1),
      exit_order_price_(nan("")),
      exit_order_size_(0),

      exit_filled_time_(-1),
      exit_filled_price_(nan("")),
      exit_filled_size_(0) {}
Order::~Order() = default;

// ===========================================================================
Order& Order::SetMargin(const double margin) {
  margin_ = margin;
  return *this;
}

Order& Order::SetLeverage(const int leverage) {
  leverage_ = leverage;
  return *this;
}

Order& Order::SetLiquidationPrice(const double liquidation_price) {
  liquidation_price_ = liquidation_price;
  return *this;
}

Order& Order::SetLiquidationFee(const double liquidation_fee) {
  liquidation_fee_ = liquidation_fee;
  return *this;
}

// ===========================================================================
Order& Order::SetEntryName(const string& entry_name) {
  entry_name_ = entry_name;
  return *this;
}

Order& Order::SetEntryOrderType(const OrderType order_type) {
  entry_order_type_ = order_type;
  return *this;
}

Order& Order::SetEntryDirection(const Direction entry_direction) {
  entry_direction_ = entry_direction;
  return *this;
}

Order& Order::SetEntryFee(const double entry_fee) {
  entry_fee_ = entry_fee;
  return *this;
}

Order& Order::SetEntryTouchPrice(const double touch_price) {
  entry_touch_price_ = touch_price;
  return *this;
}

Order& Order::SetEntryTouchDirection(const Direction touch_direction) {
  exit_touch_direction_ = touch_direction;
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

Order& Order::SetEntryOrderPrice(const double entry_order_price) {
  entry_order_price_ = entry_order_price;
  return *this;
}

Order& Order::SetEntryOrderSize(const double entry_order_size) {
  entry_order_size_ = entry_order_size;
  return *this;
}

Order& Order::SetEntryFilledTime(const int64_t entry_filled_time) {
  entry_filled_time_ = entry_filled_time;
  return *this;
}

Order& Order::SetEntryFilledPrice(const double entry_filled_price) {
  entry_filled_price_ = entry_filled_price;
  return *this;
}

Order& Order::SetEntryFilledSize(const double entry_filled_size) {
  entry_filled_size_ = entry_filled_size;
  return *this;
}

// ===========================================================================
Order& Order::SetExitName(const string& exit_name) {
  exit_name_ = exit_name;
  return *this;
}

Order& Order::SetExitOrderType(const OrderType order_type) {
  exit_order_type_ = order_type;
  return *this;
}

Order& Order::SetExitDirection(const Direction exit_direction) {
  exit_direction_ = exit_direction;
  return *this;
}

Order& Order::SetExitFee(const double exit_fee) {
  exit_fee_ = exit_fee;
  return *this;
}

Order& Order::SetExitTouchPrice(const double touch_price) {
  exit_touch_price_ = touch_price;
  return *this;
}

Order& Order::SetExitTouchDirection(const Direction touch_direction) {
  exit_touch_direction_ = touch_direction;
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

Order& Order::SetExitOrderPrice(const double exit_order_price) {
  exit_order_price_ = exit_order_price;
  return *this;
}

Order& Order::SetExitOrderSize(const double exit_order_size) {
  exit_order_size_ = exit_order_size;
  return *this;
}

Order& Order::SetExitFilledTime(const int64_t exit_filled_time) {
  exit_filled_time_ = exit_filled_time;
  return *this;
}

Order& Order::SetExitFilledPrice(const double exit_filled_price) {
  exit_filled_price_ = exit_filled_price;
  return *this;
}

Order& Order::SetExitFilledSize(const double exit_filled_size) {
  exit_filled_size_ = exit_filled_size;
  return *this;
}

// ===========================================================================
string Order::OrderTypeToString(const OrderType order_type) {
  if (order_type == OrderType::MARKET) {
    return "시장가";
  }
  if (order_type == OrderType::LIMIT) {
    return "지정가";
  }
  if (order_type == OrderType::MIT) {
    return "MIT";
  }
  if (order_type == OrderType::LIT) {
    return "LIT";
  }
  if (order_type == OrderType::TRAILING) {
    return "트레일링";
  }

  throw runtime_error("");
}

// ===========================================================================
double Order::GetMargin() const { return margin_; }
int Order::GetLeverage() const { return leverage_; }
double Order::GetLiquidationPrice() const { return liquidation_price_; }
double Order::GetLiquidationFee() const { return liquidation_fee_; }

// ===========================================================================
string Order::GetEntryName() const { return entry_name_; }
OrderType Order::GetEntryOrderType() const { return entry_order_type_; }
Direction Order::GetEntryDirection() const { return entry_direction_; }
double Order::GetEntryFee() const { return entry_fee_; }

double Order::GetEntryTouchPrice() const { return entry_touch_price_; }
Direction Order::GetEntryTouchDirection() const {
  return entry_touch_direction_;
}
double Order::GetEntryExtremePrice() const { return entry_extreme_price_; }
double Order::GetEntryTrailPoint() const { return entry_trail_point_; }

int64_t Order::GetEntryOrderTime() const { return entry_order_time_; }
double Order::GetEntryOrderPrice() const { return entry_order_price_; }
double Order::GetEntryOrderSize() const { return entry_order_size_; }

int64_t Order::GetEntryFilledTime() const { return entry_filled_time_; }
double Order::GetEntryFilledPrice() const { return entry_filled_price_; }
double Order::GetEntryFilledSize() const { return entry_filled_size_; }

// ===========================================================================
string Order::GetExitName() const { return exit_name_; }
OrderType Order::GetExitOrderType() const { return exit_order_type_; }
Direction Order::GetExitDirection() const { return exit_direction_; }
double Order::GetExitFee() const { return exit_fee_; }

double Order::GetExitTouchPrice() const { return exit_touch_price_; }
Direction Order::GetExitTouchDirection() const { return exit_touch_direction_; }
double Order::GetExitExtremePrice() const { return exit_extreme_price_; }
double Order::GetExitTrailPoint() const { return exit_trail_point_; }

int64_t Order::GetExitOrderTime() const { return exit_order_time_; }
double Order::GetExitOrderPrice() const { return exit_order_price_; }
double Order::GetExitOrderSize() const { return exit_order_size_; }

int64_t Order::GetExitFilledTime() const { return exit_filled_time_; }
double Order::GetExitFilledPrice() const { return exit_filled_price_; }
double Order::GetExitFilledSize() const { return exit_filled_size_; }
