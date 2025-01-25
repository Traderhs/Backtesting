#pragma once

// 표준 라이브러리
#include <unordered_map>

// 내부 헤더
#include "Engines/BaseOrderHandler.hpp"

/// 주문, 포지션 등과 관련된 세부적인 작업을 처리하는 클래스.
/// 전략 이름에 따라 멀티톤으로 작동.
class OrderHandler final : public BaseOrderHandler {
 public:
  // 멀티톤 특성 유지
  OrderHandler(const OrderHandler&) = delete;             // 복사 생성자 삭제
  OrderHandler& operator=(const OrderHandler&) = delete;  // 대입 연산자 삭제

  /// OrderHandler의 멀티톤 인스턴스를 반환하는 함수
  static shared_ptr<OrderHandler>& GetOrderHandler(const string& name);

  /**
   * 트레이딩 중인 심볼에서 지정된 가격을 기준으로 진입 대기 주문들이
   * 체결됐는지 확인하고 실행하는 함수.
   */
  void CheckPendingEntries(const vector<double>& prices, bool is_open);

  /**
   * 현재 사용 중인 심볼에서 지정된 가격들을 기준으로 청산 대기 주문들이
   * 체결됐는지 확인하고 실행하는 함수.
   *
   * @param prices [Open, High/Low, Close] 순으로 지정
   * @param open_time MIT, LIT 터치 시 주문 시간을 설정해야하므로 필요
   */
  void CheckPendingExits(const double* prices, int64_t open_time);

  // ===========================================================================
  /*
   * Strategy 구현부에서 사용하는 함수들
   *
   * ! 주의 사항 !
   * 1. 하나의 진입 이름으로는 하나의 진입만 체결할 수 있음.
   * 2. 동일한 진입 이름으로 주문 시 대기 진입 주문은 취소 후 재주문,
   *    체결된 진입이 있을 경우 시장가 체결은 거부됨.
   * 3. 체결된 하나의 진입 이름에 대해서 여러 청산 대기 주문을 가질 수 있음.
   * 4. 동일한 청산 이름으로 주문 시 대기 청산 주문은 취소 후 재주문.
   * 5. 하나의 청산 주문이 진입 수량을 부분 청산 시, 같은 진입 이름을 목표로
   *    하는 대기 청산 주문들은 별도 처리 없이 먼저 체결되는 순서대로 진입 체결
   *    잔량만 청산됨.
   * 6. 총 청산 체결 수량이 진입 체결 수량보다 크다면 내부적으로 청산 주문
   *    타입과 관계 없이 진입 체결 수량보다 크지 않게 조정됨.
   * 7. 청산 체결 수량이 진입 체결 수량과 같아지면 같은 진입 이름을 목표로 하는
   *    대기 청산 주문들은 모두 취소됨.
   */
  // ===========================================================================

  /// 시장가 진입 주문을 위해 사용하는 함수
  ///
  /// @param entry_name 진입 이름
  /// @param entry_direction 진입 방향
  /// @param entry_size 진입 수량
  /// @param leverage 레버리지
  void MarketEntry(const string& entry_name, Direction entry_direction,
                   double entry_size, unsigned char leverage);

  /// 지정가 진입 주문을 위해 사용하는 함수
  ///
  /// @param entry_name 진입 이름
  /// @param entry_direction 진입 방향
  /// @param entry_size 진입 수량
  /// @param leverage 레버리지
  /// @param order_price 지정가 진입 주문 가격
  void LimitEntry(const string& entry_name, Direction entry_direction,
                  double entry_size, unsigned char leverage,
                  double order_price);

  /// Market if Touched 진입 주문을 위해 사용하는 함수.
  /// touch_price에 닿으면 시장가 진입 주문 접수.
  ///
  /// @param entry_name 진입 이름
  /// @param entry_direction 진입 방향
  /// @param entry_size 진입 수량
  /// @param leverage 레버리지
  /// @param touch_price 시장가 주문을 접수할 시점의 가격
  void MitEntry(const string& entry_name, Direction entry_direction,
                double entry_size, unsigned char leverage, double touch_price);

  /// Limit if Touched 진입 주문을 위해 사용하는 함수.
  /// touch_price에 닿으면 order_price에 지정가 진입 주문 접수.
  ///
  /// @param entry_name 진입 이름
  /// @param entry_direction 진입 방향
  /// @param entry_size 진입 수량
  /// @param leverage 레버리지
  /// @param touch_price 지정가 주문을 접수할 시점의 가격
  /// @param order_price 지정가 진입 주문 가격
  void LitEntry(const string& entry_name, Direction entry_direction,
                double entry_size, unsigned char leverage, double touch_price,
                double order_price);

  /// 트레일링 진입 주문을 위해 사용하는 함수.
  /// touch_price에 닿으면 진입 방향에 따라 최고저가를 추적하며, 최고저가 대비
  /// trail_point가 움직이면 시장가 진입 주문 접수.
  ///
  /// @param entry_name 진입 이름
  /// @param entry_direction 진입 방향
  /// @param entry_size 진입 수량
  /// @param leverage 레버리지
  /// @param touch_price 최고저가 추적을 시작할 시점의 가격.
  ///                    0으로 지정할 시 바로 추적을 시작
  /// @param trail_point 최고저가로부터 어느정도 움직였을 때 진입할지 결정하는
  /// 포인트
  void TrailingEntry(const string& entry_name, Direction entry_direction,
                     double entry_size, unsigned char leverage,
                     double touch_price, double trail_point);

  // ===========================================================================
  /// 시장가 청산 주문을 위해 사용하는 함수
  ///
  /// @param exit_name 청산 이름
  /// @param target_entry_name 청산할 진입 이름
  /// @param exit_size 청산 수량
  void MarketExit(const string& exit_name, const string& target_entry_name,
                  double exit_size);

  /// 지정가 청산 주문을 위해 사용하는 함수
  ///
  /// @param exit_name 청산 이름
  /// @param target_entry_name 청산할 진입 이름
  /// @param exit_size 청산 수량
  /// @param order_price 지정가 청산 주문 가격
  void LimitExit(const string& exit_name, const string& target_entry_name,
                 double exit_size, double order_price);

  /// Market if Touched 청산 주문을 위해 사용하는 함수.
  /// touch_price에 닿으면 시장가 청산 주문 접수.
  ///
  /// @param exit_name 청산 이름
  /// @param target_entry_name 청산할 진입 이름
  /// @param exit_size 청산 수량
  /// @param touch_price 시장가 주문을 접수할 시점의 가격
  void MitExit(const string& exit_name, const string& target_entry_name,
               double exit_size, double touch_price);

  /// Limit if Touched 청산 주문을 위해 사용하는 함수.
  /// touch_price에 닿으면 order_price에 지정가 청산 주문 접수.
  ///
  /// @param exit_name 청산 이름
  /// @param target_entry_name 청산할 진입 이름
  /// @param exit_size 청산 수량
  /// @param touch_price 지정가 주문을 접수할 시점의 가격
  /// @param order_price 지정가 청산 주문 가격
  void LitExit(const string& exit_name, const string& target_entry_name,
               double exit_size, double touch_price, double order_price);

  /// 트레일링 청산 주문을 위해 사용하는 함수.
  /// touch_price에 닿으면 청산 방향에 따라 최고저가를 추적하며, 최고저가 대비
  /// trail_point가 움직이면 시장가 청산 주문 접수.
  ///
  /// @param exit_name 청산 이름
  /// @param target_entry_name 청산할 진입 이름
  /// @param exit_size 청산 수량
  /// @param touch_price 최고저가 추적을 시작할 시점의 가격.
  ///                    0으로 지정할 시 바로 추적을 시작
  /// @param trail_point 최고저가로부터 어느정도 움직였을 때 청산할지 결정하는
  /// 포인트
  void TrailingExit(const string& exit_name, const string& target_entry_name,
                    double exit_size, double touch_price, double trail_point);

  // ===========================================================================
  /// 주문 취소를 위해 사용하는 함수.
  /// order_name이 진입 대기 주문과 청산 대기 주문에 동시에 존재하면 모두 취소.
  void Cancel(const string& order_name);

 private:
  // 멀티톤 인스턴스 관리
  OrderHandler();
  class Deleter {
   public:
    void operator()(const OrderHandler* p) const;
  };

  static mutex mutex_;
  static unordered_map<string, shared_ptr<OrderHandler>> instances_;

  // ===========================================================================
  /// 시장가 진입 시 자금 관련 처리 후 체결 주문에 추가하는 함수
  void ExecuteMarketEntry(const shared_ptr<Order>& market_entry);

  /// 지정된 방향과 반대 방향의 진입 체결 주문이 있으면 모두 청산하는 함수
  void ExitOppositeFilledEntries(Direction direction);

  /// 청산 시 자금, 통계 관련 처리를 하는 함수
  void ExecuteExit(const shared_ptr<Order>& exit_order);

  /// 진입 주문 취소 시 자금 관련 처리를 하는 함수
  static void ExecuteCancelEntry(const shared_ptr<Order>& cancel_order);

  // ===========================================================================
  /// LIMIT 진입 대기 주문의 체결을 확인하고 체결시키는 함수
  void CheckPendingLimitEntries(int order_idx, int64_t open_time, double current_price);

  /// MIT 진입 대기 주문의 체결을 확인하고 체결시키는 함수
  void CheckPendingMitEntries(int order_idx, int64_t open_time, double current_price);

  void CheckPendingMitEntries(int order_idx, int64_t open_time, double current_price, bool is_open);

  /// 현재 사용 중인 심볼에서 MIT/트레일링 진입 대기 주문을 체결하는 함수.
  /// 자금 관련 처리를 하고 체결 주문으로 이동시킴.
  void FillPendingMarketEntry(int order_idx, int64_t open_time, double entry_order_price);

  /// 현재 사용 중인 심볼에서 지정가/LIT 진입 대기 주문을 체결하는 함수.
  /// 자금 관련 처리를 하고 체결 주문으로 이동시킴.
  void FillPendingLimitEntry(int order_idx, int64_t open_time, double entry_filled_price);

  /// 현재 사용 중인 심볼에서 LIT 진입 대기 주문이 터치되었을 때 지정가로 주문하는 함수.
  void OrderPendingLitEntry(int order_idx, int64_t open_time);

  // ===========================================================================
  /// 현재 사용 중인 심볼에서 지정가/LIT 청산 대기 주문을 체결하는 함수.
  /// 자금 관련 처리를 하고 체결 주문으로 이동시킴.
  int ExecutePendingLimitExit(int order_idx, int64_t open_time, double exit_order_price);

  // ===========================================================================
  /// 지정가 주문에서 현재 가격이 진입 방향에 따라
  /// 주문 가격보다 낮아졌거나 커졌는지 확인하는 함수
  static inline bool IsLimitPriceSatisfied(Direction direction, double price, double order_price);

  /// 현재 가격이 터치 방향에 따라 터치 가격보다 커졌거나 작아졌는지 확인하는 함수
  static inline bool IsPriceTouched(Direction direction, double price, double touch_price);

  /// 체결된 진입 주문에서 Target Entry Name과 같은 주문을 찾아 주문 인덱스와 함께 반환하는 함수
  [[nodiscard]] pair<shared_ptr<Order>, int> FindMatchingEntryOrder(const string& target_entry_name) const;

  /// 청산 체결 크기가 진입 체결 크기를 넘지 않도록 조정하여 반환하는 함수
  static double GetAdjustedExitFilledSize(double exit_size, const shared_ptr<Order>& exit_order);

  /// 진입 가능 자금이 필요 자금보다 많은지 확인하는 함수
  static void HasEnoughBalance(double available_balance, double needed_balance);
};