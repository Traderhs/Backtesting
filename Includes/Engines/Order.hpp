#pragma once

// 표준 라이브러리
#include <string>

// 내부 헤더
#include "Engines/BaseOrderHandler.hpp"

/// 하나의 주문 정보를 담고 있는 빌더 클래스
class Order {
 public:
  Order();
  ~Order();

  Order& SetLeverage(unsigned char leverage);
  Order& SetMarginCallPrice(double margin_call_price);
  Order& SetMaxProfit(double max_profit);
  Order& SetMaxLoss(double max_loss);

  Order& SetEntryName(const string& entry_name);
  Order& SetEntryOrderType(BaseOrderHandler::OrderType order_type);
  Order& SetEntryDirection(BaseOrderHandler::Direction direction);
  Order& SetEntryCommission(double commission);

  Order& SetEntryTouchPrice(double touch_price);
  Order& SetEntryExtremePrice(double extreme_price);
  Order& SetEntryTrailPoint(double trail_point);

  Order& SetEntryOrderTime(int64_t entry_order_time);
  Order& SetEntryOrderSize(double entry_order_size);
  Order& SetEntryOrderPrice(double entry_order_price);

  Order& SetEntryFilledTime(int64_t entry_filled_time);
  Order& SetEntryFilledSize(double entry_filled_size);
  Order& SetEntryFilledPrice(double entry_filled_price);

  Order& SetExitName(const string& exit_name);
  Order& SetExitOrderType(BaseOrderHandler::OrderType order_type);
  Order& SetExitDirection(BaseOrderHandler::Direction direction);
  Order& SetExitCommission(double commission);

  Order& SetExitTouchPrice(double touch_price);
  Order& SetExitExtremePrice(double extreme_price);
  Order& SetExitTrailPoint(double trail_point);

  Order& SetExitOrderTime(int64_t exit_order_time);
  Order& SetExitOrderSize(double exit_order_size);
  Order& SetExitOrderPrice(double exit_order_price);

  Order& SetExitFilledTime(int64_t exit_filled_time);
  Order& SetExitFilledSize(double exit_filled_size);
  Order& SetExitFilledPrice(double exit_filled_price);

  [[nodiscard]] unsigned char GetLeverage() const;
  [[nodiscard]] double GetMarginCallPrice() const;
  [[nodiscard]] double GetMaxProfit() const;
  [[nodiscard]] double GetMaxLoss() const;

  [[nodiscard]] string GetEntryName() const;
  [[nodiscard]] BaseOrderHandler::OrderType GetEntryOrderType() const;
  [[nodiscard]] BaseOrderHandler::Direction GetEntryDirection() const;
  [[nodiscard]] double GetEntryCommission() const;

  [[nodiscard]] double GetEntryTouchPrice() const;
  [[nodiscard]] double GetEntryExtremePrice() const;
  [[nodiscard]] double GetEntryTrailPoint() const;

  [[nodiscard]] int64_t GetEntryOrderTime() const;
  [[nodiscard]] double GetEntryOrderSize() const;
  [[nodiscard]] double GetEntryOrderPrice() const;

  [[nodiscard]] int64_t GetEntryFilledTime() const;
  [[nodiscard]] double GetEntryFilledSize() const;
  [[nodiscard]] double GetEntryFilledPrice() const;

  [[nodiscard]] string GetExitName() const;
  [[nodiscard]] BaseOrderHandler::OrderType GetExitOrderType() const;
  [[nodiscard]] BaseOrderHandler::Direction GetExitDirection() const;
  [[nodiscard]] double GetExitCommission() const;

  [[nodiscard]] double GetExitTouchPrice() const;
  [[nodiscard]] double GetExitExtremePrice() const;
  [[nodiscard]] double GetExitTrailPoint() const;

  [[nodiscard]] int64_t GetExitOrderTime() const;
  [[nodiscard]] double GetExitOrderSize() const;
  [[nodiscard]] double GetExitOrderPrice() const;

  [[nodiscard]] int64_t GetExitFilledTime() const;
  [[nodiscard]] double GetExitFilledSize() const;
  [[nodiscard]] double GetExitFilledPrice() const;

 private:
  // 통합 변수
  unsigned char leverage_;    // 레버리지 배수
  double margin_call_price_;  // 마진콜 가격  // 수정 필요 @@@@@@@@@@@@@@@@@@@
  double max_profit_;         // 최대 수익
  double max_loss_;           // 최대 손실

  // 진입 변수
  string entry_name_;                             // 진입 주문 이름
  BaseOrderHandler::OrderType entry_order_type_;  // 진입 주문 타입
  BaseOrderHandler::Direction entry_direction_;   // 진입 방향
  double entry_commission_;                       // 진입 수수료 금액

  // MIT, LIT, Trailing 진입 대기 변수
  double entry_touch_price_;    // 진입 주문을 실행할 터치 가격
  double entry_extreme_price_;  // Trailing: 진입 주문 이후 최고저가 값
  double entry_trail_point_;    /* Trailing: 최고저가에서 어느정도 움직였을 때
                                             진입할지 결정                   */

  // 시장가, 지정가 진입 주문 시 업데이트
  int64_t entry_order_time_;  // 진입 주문 시간
  double entry_order_size_;   // 진입 주문 수량
  double entry_order_price_;  // 진입 주문 가격

  // 시장가, 지정가 진입 체결 시 업데이트
  int64_t entry_filled_time_;  // 진입 체결 시간
  double entry_filled_size_;   // 진입 체결 수량
  double entry_filled_price_;  // 진입 체결 가격

  // 청산 변수
  string exit_name_;                             // 청산 주문 이름
  BaseOrderHandler::OrderType exit_order_type_;  // 청산 주문 타입
  BaseOrderHandler::Direction exit_direction_;   // 청산 방향
  double exit_commission_;                       // 청산 수수료 금액

  // MIT, LIT, Trailing 청산 대기 변수
  double exit_touch_price_;    // 청산 주문을 실행할 터치 가격
  double exit_extreme_price_;  // Trailing: 청산 주문 이후 최고저가 값
  double exit_trail_point_;    /* Trailing: 최고저가에서 어느정도 움직였을 때
                                            청산할지 결정                   */

  // 시장가, 지정가 청산 주문 시 업데이트
  int64_t exit_order_time_;   // 청산 주문 시간
  double exit_order_size_;    // 청산 주문 수량
  double exit_order_price_;   // 청산 주문 가격

  // 시장가, 지정가 청산 체결 시 업데이트
  int64_t exit_filled_time_;  // 청산 체결 시간
  double exit_filled_size_;   // 청산 체결 수량
  double exit_filled_price_;  // 청산 체결 가격
};