#pragma once

// 표준 라이브러리
#include <string>

// 네임 스페이스
using namespace std;

/// 방향을 지정하는 열거형 클래스
enum class Direction { NONE, LONG, SHORT };

/// 주문 타입을 지정하는 열거형 클래스
enum class OrderType { NONE, MARKET, LIMIT, MIT, LIT, TRAILING };

/// 하나의 주문 정보를 담고 있는 빌더 클래스
class Order {
 public:
  Order();
  ~Order();

  Order& SetLeverage(unsigned char leverage);
  Order& SetMarginCallPrice(double margin_call_price);

  // ===========================================================================
  Order& SetEntryName(const string& entry_name);
  Order& SetEntryOrderType(OrderType order_type);
  Order& SetEntryDirection(Direction entry_direction);
  Order& SetEntryCommission(double commission);

  Order& SetEntryTouchPrice(double touch_price);
  Order& SetEntryTouchDirection(Direction touch_direction);
  Order& SetEntryExtremePrice(double extreme_price);
  Order& SetEntryTrailPoint(double trail_point);

  Order& SetEntryOrderTime(int64_t entry_order_time);
  Order& SetEntryOrderSize(double entry_order_size);
  Order& SetEntryOrderPrice(double entry_order_price);

  Order& SetEntryFilledTime(int64_t entry_filled_time);
  Order& SetEntryFilledSize(double entry_filled_size);
  Order& SetEntryFilledPrice(double entry_filled_price);

  // ===========================================================================
  Order& SetExitName(const string& exit_name);
  Order& SetExitOrderType(OrderType order_type);
  Order& SetExitDirection(Direction exit_direction);
  Order& SetExitCommission(double commission);

  Order& SetExitTouchPrice(double touch_price);
  Order& SetExitTouchDirection(Direction touch_direction);
  Order& SetExitExtremePrice(double extreme_price);
  Order& SetExitTrailPoint(double trail_point);

  Order& SetExitOrderTime(int64_t exit_order_time);
  Order& SetExitOrderSize(double exit_order_size);
  Order& SetExitOrderPrice(double exit_order_price);

  Order& SetExitFilledTime(int64_t exit_filled_time);
  Order& SetExitFilledSize(double exit_filled_size);
  Order& SetExitFilledPrice(double exit_filled_price);

  // ===========================================================================
  [[nodiscard]] inline unsigned char GetLeverage() const;
  [[nodiscard]] inline double GetMarginCallPrice() const;

  // ===========================================================================
  [[nodiscard]] inline string GetEntryName() const;
  [[nodiscard]] inline OrderType GetEntryOrderType() const;
  [[nodiscard]] inline Direction GetEntryDirection() const;
  [[nodiscard]] inline double GetEntryCommission() const;

  [[nodiscard]] inline double GetEntryTouchPrice() const;
  [[nodiscard]] inline Direction GetEntryTouchDirection() const;
  [[nodiscard]] inline double GetEntryExtremePrice() const;
  [[nodiscard]] inline double GetEntryTrailPoint() const;

  [[nodiscard]] inline int64_t GetEntryOrderTime() const;
  [[nodiscard]] inline double GetEntryOrderSize() const;
  [[nodiscard]] inline double GetEntryOrderPrice() const;

  [[nodiscard]] inline int64_t GetEntryFilledTime() const;
  [[nodiscard]] inline double GetEntryFilledSize() const;
  [[nodiscard]] inline double GetEntryFilledPrice() const;

  // ===========================================================================
  [[nodiscard]] inline string GetExitName() const;
  [[nodiscard]] inline OrderType GetExitOrderType() const;
  [[nodiscard]] inline Direction GetExitDirection() const;
  [[nodiscard]] inline double GetExitCommission() const;

  [[nodiscard]] inline double GetExitTouchPrice() const;
  [[nodiscard]] inline Direction GetExitTouchDirection() const;
  [[nodiscard]] inline double GetExitExtremePrice() const;
  [[nodiscard]] inline double GetExitTrailPoint() const;

  [[nodiscard]] inline int64_t GetExitOrderTime() const;
  [[nodiscard]] inline double GetExitOrderSize() const;
  [[nodiscard]] inline double GetExitOrderPrice() const;

  [[nodiscard]] inline int64_t GetExitFilledTime() const;
  [[nodiscard]] inline double GetExitFilledSize() const;
  [[nodiscard]] inline double GetExitFilledPrice() const;

 private:
  // 통합 변수
  unsigned char leverage_;    // 레버리지 배수
  double margin_call_price_;  // 마진콜 가격  // 수정 필요 @@@@@@@@@@@@@@@@@@@@@

  // ===========================================================================
  // 진입 변수
  string entry_name_;           // 진입 주문 이름
  OrderType entry_order_type_;  // 진입 주문 타입
  Direction entry_direction_;   // 진입 방향
  double entry_commission_;     // 진입 수수료 금액

  // MIT, LIT, Trailing 진입 대기 변수
  double entry_touch_price_;         // 진입 주문을 실행할 터치 가격
  Direction entry_touch_direction_;  // 진입 주문을 위해 터치해야 하는 방향
  double entry_extreme_price_;       // 진입 주문 이후 최고저가 값
  double entry_trail_point_;         /* 최고저가에서 어느정도 움직였을 때
                                        진입할지 결정하는 값 */

  // 시장가, 지정가 진입 주문 시 업데이트
  int64_t entry_order_time_;  // 진입 주문 시간
  double entry_order_size_;   // 진입 주문 수량
  double entry_order_price_;  // 진입 주문 가격

  // 시장가, 지정가 진입 체결 시 업데이트
  int64_t entry_filled_time_;  // 진입 체결 시간
  double entry_filled_size_;   // 진입 체결 수량
  double entry_filled_price_;  // 진입 체결 가격

  // ===========================================================================
  // 청산 변수
  string exit_name_;           // 청산 주문 이름
  OrderType exit_order_type_;  // 청산 주문 타입
  Direction exit_direction_;   // 청산 방향
  double exit_commission_;     // 청산 수수료 금액

  // MIT, LIT, Trailing 청산 대기 변수
  double exit_touch_price_;         // 청산 주문을 실행할 터치 가격
  Direction exit_touch_direction_;  // 청산 주문을 위해 터치해야 하는 방향
  double exit_extreme_price_;       // 청산 주문 이후 최고저가 값
  double exit_trail_point_;         /* 최고저가에서 어느정도 움직였을 때
                                       청산할지 결정 */

  // 시장가, 지정가 청산 주문 시 업데이트
  int64_t exit_order_time_;   // 청산 주문 시간
  double exit_order_size_;    // 청산 주문 수량
  double exit_order_price_;   // 청산 주문 가격

  // 시장가, 지정가 청산 체결 시 업데이트
  int64_t exit_filled_time_;  // 청산 체결 시간
  double exit_filled_size_;   // 청산 체결 수량
  double exit_filled_price_;  // 청산 체결 가격
};