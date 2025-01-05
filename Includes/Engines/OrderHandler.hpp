#pragma once

// 표준 라이브러리
#include <unordered_map>

// 내부 헤더
#include "Engines/BaseOrderHandler.hpp"

/// 주문, 포지션 등과 관련된 세부적인 작업을 처리하는 클래스
class OrderHandler final : public BaseOrderHandler {
 public:
  // 멀티톤 특성 유지
  OrderHandler(const OrderHandler&) = delete;             // 복사 생성자 삭제
  OrderHandler& operator=(const OrderHandler&) = delete;  // 대입 연산자 삭제

  /// OrderHandler의 멀티톤 인스턴스를 반환하는 함수
  static OrderHandler& GetOrderHandler(const string& name);

  // ===========================================================================
  /*
   * Strategy 구현부에서 사용하는 함수들
   *
   * ! 주의 사항 !
   * 1. 하나의 진입 이름으로는 하나의 진입만 체결할 수 있음.
   * 2. 같은 이름의 진입 주문을 호출 시, 대기 진입 주문은 수정, 체결은 거부됨.
   * 3. 체결된 하나의 진입 이름에 대해서 여러 청산 대기 주문을 가질 수 있음.
   * 4. 하나의 청산 주문이 부분 혹은 전체 체결 시 같은 진입 이름을 목표로 한
   *    청산 주문들은 모두 취소됨.
   * 5. 이는 포지션 크기의 초과 청산을 방지하기 위함.
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
  /// @param target_entry 청산할 진입 이름
  /// @param exit_size 청산 수량
  void MarketExit(const string& exit_name, const string& target_entry,
                  double exit_size);

  /// 지정가 청산 주문을 위해 사용하는 함수
  ///
  /// @param exit_name 청산 이름
  /// @param target_entry 청산할 진입 이름
  /// @param exit_size 청산 수량
  /// @param order_price 지정가 청산 주문 가격
  void LimitExit(const string& exit_name, const string& target_entry,
                 double exit_size, double order_price);

  /// Market if Touched 청산 주문을 위해 사용하는 함수.
  /// touch_price에 닿으면 시장가 청산 주문 접수.
  ///
  /// @param exit_name 청산 이름
  /// @param target_entry 청산할 진입 이름
  /// @param exit_size 청산 수량
  /// @param touch_price 시장가 주문을 접수할 시점의 가격
  void MitExit(const string& exit_name, const string& target_entry,
               double exit_size, double touch_price);

  /// Limit if Touched 청산 주문을 위해 사용하는 함수.
  /// touch_price에 닿으면 order_price에 지정가 청산 주문 접수.
  ///
  /// @param exit_name 청산 이름
  /// @param target_entry 청산할 진입 이름
  /// @param exit_size 청산 수량
  /// @param touch_price 지정가 주문을 접수할 시점의 가격
  /// @param order_price 지정가 청산 주문 가격
  void LitExit(const string& exit_name, const string& target_entry,
               double exit_size, double touch_price, double order_price);

  /// 트레일링 청산 주문을 위해 사용하는 함수.
  /// touch_price에 닿으면 청산 방향에 따라 최고저가를 추적하며, 최고저가 대비
  /// trail_point가 움직이면 시장가 청산 주문 접수.
  ///
  /// @param exit_name 청산 이름
  /// @param target_entry 청산할 진입 이름
  /// @param exit_size 청산 수량
  /// @param touch_price 최고저가 추적을 시작할 시점의 가격.
  ///                    0으로 지정할 시 바로 추적을 시작
  /// @param trail_point 최고저가로부터 어느정도 움직였을 때 청산할지 결정하는
  /// 포인트
  void TrailingExit(const string& exit_name, const string& target_entry,
                    double exit_size, double touch_price, double trail_point);

  // ===========================================================================
  /// 주문 취소를 위해 사용하는 함수
  void Cancel(int symbol_idx, const string& order_name);
  // Pending 진입, 청산 두 개 돌리면 될 듯
  // 주문 타입에 따라 Limit은 돈 복구하고 이런거 있어야 함
  // 그리고 진입은 모르겠고 청산은 정보 초기화하고 filled_entries로 돌려놔야함

 private:
  // 멀티톤 인스턴스 관리
  explicit OrderHandler();
  ~OrderHandler();

  static mutex mutex_;
  static unordered_map<string, unique_ptr<OrderHandler>> instances_;

  // ===========================================================================
  /// 시장가 진입 주문을 실행하는 함수
  void ExecuteMarketEntry(const shared_ptr<Order>& order, int symbol_idx);

  /// 지정가 진입 주문을 실행하는 함수
  void ExecuteLimitEntry(const shared_ptr<Order>& order, int symbol_idx);

  /// MIT 진입 주문을 실행하는 함수
  void ExecuteMitEntry(const shared_ptr<Order>& order, int symbol_idx);

  /// LIT 진입 주문을 실행하는 함수
  void ExecuteLitEntry(const shared_ptr<Order>& order, int symbol_idx);

  /// 트레일링 진입 주문을 실행하는 함수
  void ExecuteTrailingEntry(const shared_ptr<Order>& order, int symbol_idx);

  // ===========================================================================
  /// 시장가 청산 주문을 실행하는 함수
  void ExecuteMarketExit(const string& exit_name, const string& target_entry,
                         double exit_size);

  /// 지정가 청산 주문을 실행하는 함수
  void ExecuteLimitExit(const string& exit_name, const string& target_entry,
                        double exit_size, double order_price);

  /// MIT 청산 주문을 실행하는 함수
  void ExecuteMitExit(const string& exit_name, const string& target_entry,
                      double exit_size, double touch_price);

  /// LIT 청산 주문을 실행하는 함수
  void ExecuteLitExit(const string& exit_name, const string& target_entry,
                      double exit_size, double touch_price, double order_price);

  /// 트레일링 청산 주문을 실행하는 함수
  void ExecuteTrailingExit(const string& exit_name, const string& target_entry,
                           double exit_size, double touch_price, double trail_point);

  // ===========================================================================
  // ExecutePendingEntries, ExecutePendingExits 함수 필요

  // ===========================================================================
  /// 청산 시 자금, 통계 관련 처리를 하는 함수
  void ExecuteExit(const shared_ptr<Order>& exit);

  /// 진입 가능 자금이 필요 자금보다 많은지 확인하는 함수
  static bool HasEnoughBalance(double available_balance, double needed_balance, int symbol_idx, int64_t order_time);

  /// 청산 주문 포지션 사이즈가 진입 포지션 사이즈를 넘지 않도록 조정하여 반환하는 함수
  static double CalculateAdjustedExitSize(double exit_size, const shared_ptr<Order>& exit_order);

  /// 진입 체결 시 진입 이름이 유효한지 확인하는 함수
  void IsValidEntryName(const string& entry_name, int symbol_idx) const;

  /// 지정가 주문 가격이 유효한 가격인지 확인하는 함수
  static void IsValidLimitOrderPrice(double limit_price, double base_price, Direction direction,
                                     int symbol_idx, int64_t order_time);

  /// 트레일링 진입/청산의 터치 가격이 유효한지 확인하는 함수.
  /// 트레일링 진입/청산의 터치 가격은 0으로 지정될 수 있기 때문에 별개 함수로 처리
  static void IsValidTrailingTouchPrice(double touch_price, int symbol_idx);

  /// 트레일링 포인트가 유효한지 확인하는 함수
  static void IsValidTrailPoint(double trail_point, int symbol_idx);

  /// 진입명이 존재하지 않음을 알리는 경고 로그
  static void NotExistEntryName(const string& entry_name);
};