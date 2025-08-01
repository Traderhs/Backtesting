#pragma once

// 표준 라이브러리
#include <cstdint>
#include <string>

// 네임 스페이스
using namespace std;

namespace backtesting::order {

/// 방향을 지정하는 열거형 클래스
enum class Direction { DIRECTION_NONE, LONG, SHORT };
using enum Direction;

/// 주문 타입을 지정하는 열거형 클래스
enum class OrderType { ORDER_NONE, MARKET, LIMIT, MIT, LIT, TRAILING };
using enum OrderType;

/// 하나의 주문 정보를 담고 있는 빌더 클래스
class Order {
 public:
  Order();
  ~Order();

  Order& SetLeverage(int leverage);
  Order& SetEntryMargin(double entry_margin);
  Order& SetLeftMargin(double left_margin);
  Order& SetLiquidationPrice(double liquidation_price);
  Order& SetLiquidationFee(double liquidation_fee);
  Order& SetWbWhenEntryOrder(double wb_when_entry_order);
  Order& AddExitCount();

  // ===========================================================================
  Order& SetEntryName(const string& entry_name);
  Order& SetEntryOrderType(OrderType order_type);
  Order& SetEntryDirection(Direction entry_direction);
  Order& SetEntryFee(double entry_fee);

  Order& SetEntryTouchPrice(double touch_price);
  Order& SetEntryTouchDirection(Direction touch_direction);
  Order& SetEntryExtremePrice(double extreme_price);
  Order& SetEntryTrailPoint(double trail_point);

  Order& SetEntryOrderTime(int64_t entry_order_time);
  Order& SetEntryOrderPrice(double entry_order_price);
  Order& SetEntryOrderSize(double entry_order_size);

  Order& SetEntryFilledTime(int64_t entry_filled_time);
  Order& SetEntryFilledPrice(double entry_filled_price);
  Order& SetEntryFilledSize(double entry_filled_size);

  // ===========================================================================
  Order& SetExitName(const string& exit_name);
  Order& SetExitOrderType(OrderType order_type);
  Order& SetExitDirection(Direction exit_direction);
  Order& SetExitFee(double exit_fee);

  Order& SetExitTouchPrice(double touch_price);
  Order& SetExitTouchDirection(Direction touch_direction);
  Order& SetExitExtremePrice(double extreme_price);
  Order& SetExitTrailPoint(double trail_point);

  Order& SetExitOrderTime(int64_t exit_order_time);
  Order& SetExitOrderPrice(double exit_order_price);
  Order& SetExitOrderSize(double exit_order_size);

  Order& SetExitFilledTime(int64_t exit_filled_time);
  Order& SetExitFilledPrice(double exit_filled_price);
  Order& SetExitFilledSize(double exit_filled_size);

  // ===========================================================================
  [[nodiscard]] static string OrderTypeToString(OrderType order_type);

  // ===========================================================================
  [[nodiscard]] int GetLeverage() const;
  [[nodiscard]] double GetEntryMargin() const;
  [[nodiscard]] double GetLeftMargin() const;
  [[nodiscard]] double GetLiquidationPrice() const;
  [[nodiscard]] double GetLiquidationFee() const;
  [[nodiscard]] double GetWbWhenEntryOrder() const;
  [[nodiscard]] int GetExitCount() const;

  // ===========================================================================
  [[nodiscard]] string GetEntryName() const;
  [[nodiscard]] OrderType GetEntryOrderType() const;
  [[nodiscard]] Direction GetEntryDirection() const;
  [[nodiscard]] double GetEntryFee() const;

  [[nodiscard]] double GetEntryTouchPrice() const;
  [[nodiscard]] Direction GetEntryTouchDirection() const;
  [[nodiscard]] double GetEntryExtremePrice() const;
  [[nodiscard]] double GetEntryTrailPoint() const;

  [[nodiscard]] int64_t GetEntryOrderTime() const;
  [[nodiscard]] double GetEntryOrderPrice() const;
  [[nodiscard]] double GetEntryOrderSize() const;

  [[nodiscard]] int64_t GetEntryFilledTime() const;
  [[nodiscard]] double GetEntryFilledPrice() const;
  [[nodiscard]] double GetEntryFilledSize() const;

  // ===========================================================================
  [[nodiscard]] string GetExitName() const;
  [[nodiscard]] OrderType GetExitOrderType() const;
  [[nodiscard]] Direction GetExitDirection() const;
  [[nodiscard]] double GetExitFee() const;

  [[nodiscard]] double GetExitTouchPrice() const;
  [[nodiscard]] Direction GetExitTouchDirection() const;
  [[nodiscard]] double GetExitExtremePrice() const;
  [[nodiscard]] double GetExitTrailPoint() const;

  [[nodiscard]] int64_t GetExitOrderTime() const;
  [[nodiscard]] double GetExitOrderPrice() const;
  [[nodiscard]] double GetExitOrderSize() const;

  [[nodiscard]] int64_t GetExitFilledTime() const;
  [[nodiscard]] double GetExitFilledPrice() const;
  [[nodiscard]] double GetExitFilledSize() const;

 private:
  // 통합 변수
  int leverage_;                // 레버리지 배수
  double entry_margin_;         // 최초 진입 마진
  double left_margin_;          // 청산 후 잔여 마진
  double liquidation_price_;    // 강제 청산 가격
  double liquidation_fee_;      // 강제 청산 수수료
  double wb_when_entry_order_;  // 진입 주문 시점의 지갑 자금
  int exit_count_;              // 원본 진입의 청산 횟수

  // ===========================================================================
  // 진입 변수
  string entry_name_;           // 진입 주문 이름
  OrderType entry_order_type_;  // 진입 주문 타입
  Direction entry_direction_;   // 진입 방향
  double entry_fee_;            // 진입 수수료 금액

  // MIT, LIT, Trailing 진입 대기 변수
  double entry_touch_price_;         // 진입 주문을 실행할 터치 가격
  Direction entry_touch_direction_;  // 진입 주문을 위해 터치해야 하는 방향
  double entry_extreme_price_;       // 진입 주문 이후 최고저가 값
  double entry_trail_point_;         /* 최고저가에서 어느정도 움직였을 때
                                        진입할지 결정하는 값 */

  // 시장가, 지정가 진입 주문 시 업데이트
  int64_t entry_order_time_;  // 진입 주문 시간
  double entry_order_price_;  // 진입 주문 가격
  double entry_order_size_;   // 진입 주문 수량

  // 시장가, 지정가 진입 체결 시 업데이트
  int64_t entry_filled_time_;  // 진입 체결 시간
  double entry_filled_price_;  // 진입 체결 가격
  double entry_filled_size_;   // 진입 체결 수량

  // ===========================================================================
  // 청산 변수
  string exit_name_;           // 청산 주문 이름
  OrderType exit_order_type_;  // 청산 주문 타입
  Direction exit_direction_;   // 청산 방향
  double exit_fee_;            // 청산 수수료 금액

  // MIT, LIT, Trailing 청산 대기 변수
  double exit_touch_price_;         // 청산 주문을 실행할 터치 가격
  Direction exit_touch_direction_;  // 청산 주문을 위해 터치해야 하는 방향
  double exit_extreme_price_;       // 청산 주문 이후 최고저가 값
  double exit_trail_point_;         /* 최고저가에서 어느정도 움직였을 때
                                       청산할지 결정 */

  // 시장가, 지정가 청산 주문 시 업데이트
  int64_t exit_order_time_;  // 청산 주문 시간
  double exit_order_price_;  // 청산 주문 가격
  double exit_order_size_;   // 청산 주문 수량

  // 시장가, 지정가 청산 체결 시 업데이트
  int64_t exit_filled_time_;  // 청산 체결 시간
  double exit_filled_price_;  // 청산 체결 가격
  double exit_filled_size_;   // 청산 체결 수량
                              // (원본 진입의 경우 총 청산 수량이 누적됨)
};

}  // namespace backtesting::order