// 파일 헤더
#include "Engines/Order.hpp"

// 내부 헤더
#include "Engines/Logger.hpp"

// 네임 스페이스
namespace backtesting {
using namespace logger;
}

namespace backtesting::order {

Order::Order()
    : leverage_(-1),
      entry_margin_(0),
      left_margin_(0),
      liquidation_price_(NAN),
      liquidation_fee_(0),
      received_funding_count_(0),
      received_funding_amount_(0),
      paid_funding_count_(0),
      paid_funding_amount_(0),
      wb_when_entry_order_(0),
      exit_count_(0),

      entry_order_type_(ORDER_NONE),
      entry_direction_(DIRECTION_NONE),
      entry_fee_(NAN),

      entry_touch_price_(NAN),
      entry_touch_direction_(DIRECTION_NONE),
      entry_extreme_price_(NAN),
      entry_trail_point_(NAN),

      entry_order_time_(-1),
      entry_order_price_(NAN),
      entry_order_size_(0),

      entry_filled_time_(-1),
      entry_filled_price_(NAN),
      entry_filled_size_(0),

      exit_order_type_(ORDER_NONE),
      exit_direction_(DIRECTION_NONE),
      exit_fee_(NAN),

      exit_touch_price_(NAN),
      exit_touch_direction_(DIRECTION_NONE),
      exit_extreme_price_(NAN),
      exit_trail_point_(NAN),

      exit_order_time_(-1),
      exit_order_price_(NAN),
      exit_order_size_(0),

      exit_filled_time_(-1),
      exit_filled_price_(NAN),
      exit_filled_size_(0) {}
Order::~Order() = default;

// ===========================================================================
Order& Order::SetLeverage(const int leverage) {
  leverage_ = leverage;
  return *this;
}

Order& Order::SetEntryMargin(const double entry_margin) {
  entry_margin_ = entry_margin;
  return *this;
}

Order& Order::SetLeftMargin(const double left_margin) {
  left_margin_ = left_margin;
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

Order& Order::AddReceivedFundingCount() {
  received_funding_count_++;
  return *this;
}

Order& Order::SetReceivedFundingAmount(const double received_funding_amount) {
  received_funding_amount_ = received_funding_amount;
  return *this;
}

Order& Order::AddPaidFundingCount() {
  paid_funding_count_++;
  return *this;
}

Order& Order::SetPaidFundingAmount(const double paid_funding_amount) {
  paid_funding_amount_ = paid_funding_amount;
  return *this;
}

Order& Order::SetWbWhenEntryOrder(const double wb_when_entry_order) {
  wb_when_entry_order_ = wb_when_entry_order;
  return *this;
}

Order& Order::AddExitCount() {
  exit_count_++;
  return *this;
}

// ===========================================================================
Order& Order::SetEntryName(const string& entry_name) {
  entry_name_ = entry_name;
  return *this;
}

Order& Order::SetEntryOrderType(const OrderType entry_order_type) {
  entry_order_type_ = entry_order_type;
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

Order& Order::SetEntryTouchPrice(const double entry_touch_price) {
  entry_touch_price_ = entry_touch_price;
  return *this;
}

Order& Order::SetEntryTouchDirection(const Direction entry_touch_direction) {
  entry_touch_direction_ = entry_touch_direction;
  return *this;
}

Order& Order::SetEntryExtremePrice(const double entry_extreme_price) {
  entry_extreme_price_ = entry_extreme_price;
  return *this;
}

Order& Order::SetEntryTrailPoint(const double entry_trail_point) {
  entry_trail_point_ = entry_trail_point;
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

Order& Order::SetExitOrderType(const OrderType exit_order_type) {
  exit_order_type_ = exit_order_type;
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

Order& Order::SetExitTouchPrice(const double exit_touch_price) {
  exit_touch_price_ = exit_touch_price;
  return *this;
}

Order& Order::SetExitTouchDirection(const Direction exit_touch_direction) {
  exit_touch_direction_ = exit_touch_direction;
  return *this;
}

Order& Order::SetExitExtremePrice(const double exit_extreme_price) {
  exit_extreme_price_ = exit_extreme_price;
  return *this;
}

Order& Order::SetExitTrailPoint(const double exit_trail_point) {
  exit_trail_point_ = exit_trail_point;
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
  switch (order_type) {
    case MARKET: {
      return "시장가";
    }

    case LIMIT: {
      return "지정가";
    }

    case LIT: {
      return "LIT";
    }

    case MIT: {
      return "MIT";
    }

    case TRAILING: {
      return "트레일링";
    }

    case ORDER_NONE: {
      throw runtime_error(
          "주문 유형을 String으로 변환하는 중 오류가 발생했습니다. 주문 유형은 "
          "ORDER_NONE으로 지정할 수 없습니다.");
    }
  }

  return string();
}

// ===========================================================================
int Order::GetLeverage() const { return leverage_; }
double Order::GetEntryMargin() const { return entry_margin_; }
double Order::GetLeftMargin() const { return left_margin_; }
double Order::GetLiquidationPrice() const { return liquidation_price_; }
double Order::GetLiquidationFee() const { return liquidation_fee_; }
int Order::GetReceivedFundingCount() const { return received_funding_count_; }
double Order::GetReceivedFundingAmount() const {
  return received_funding_amount_;
}
int Order::GetPaidFundingCount() const { return paid_funding_count_; }
double Order::GetPaidFundingAmount() const { return paid_funding_amount_; }
double Order::GetWbWhenEntryOrder() const { return wb_when_entry_order_; }
int Order::GetExitCount() const { return exit_count_; }

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

}  // namespace backtesting::order
