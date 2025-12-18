#pragma once

// 표준 라이브러리
#include <mutex>
#include <unordered_set>

// 내부 헤더
#include "BaseOrderHandler.hpp"

// 전방 선언
namespace backtesting::bar {
enum class BarDataType;
}

namespace backtesting::engine {
class Engine;
}

namespace backtesting::order {

/**
 * 주문, 포지션 등과 관련된 세부적인 작업을 처리하는 클래스.
 *
 * ※ 주의 사항 ※
 * 1. 하나의 진입 이름으로는 하나의 진입만 체결할 수 있음.
 *
 * 2. 동일한 진입 이름으로 주문 시 진입 대기 주문은 취소 후 재주문,
 *    체결 시 동일한 진입 이름으로 체결된 진입이 있을 경우 체결이 거부됨.
 *
 * 3. 체결된 하나의 진입 이름에 대해서 여러 청산 대기 주문을 가질 수 있음.
 *
 * 4. 동일한 청산 이름으로 주문 시 청산 대기 주문은 취소 후 재주문.
 *
 * 5. 하나의 청산 주문이 진입 수량을 부분 청산 시, 같은 진입 이름을 목표로
 *    하는 청산 대기 주문들은 별도 처리 없이 먼저 체결되는 순서대로 진입 체결
 *    잔량만 청산됨.
 *
 * 6. 총 청산 체결 수량이 진입 체결 수량보다 크다면 내부적으로 청산 주문
 *    타입과 관계 없이 진입 체결 수량보다 크지 않게 조정됨.
 *
 * 7. 청산 체결 수량이 진입 체결 수량과 같아지면 같은 진입 이름을 목표로 하는
 *    청산 대기 주문들은 모두 취소됨.
 *
 * 8. 진입 시, 해당 심볼에 진입 방향과 반대 방향의 진입이 존재한다면 사용 가능
 *    자금이 충분한지 체크하기 전에 우선적으로 반대 방향의 진입을 리버스 청산함.
 *    → 사용 가능 자금을 최대한 확보하여 진입 가능성을 높이기 위함
 *    → 기존 진입과 반대 방향의 주문이 사용 가능 자금이 부족하여 취소되더라도
 *      기존 진입은 무조건 리버스 청산되므로 주의가 필요
 */
class OrderHandler final : public BaseOrderHandler {
  // 체결 확인, 주문 실행 용도
  friend class Engine;

 public:
  // 싱글톤 특성 유지
  OrderHandler(const OrderHandler&) = delete;             // 복사 생성자 삭제
  OrderHandler& operator=(const OrderHandler&) = delete;  // 대입 연산자 삭제

  /// OrderHandler의 싱글톤 인스턴스를 반환하는 함수
  static shared_ptr<OrderHandler>& GetOrderHandler();

  // ===========================================================================
  // 전략 구현부에서 사용하는 진입 함수들
  // ===========================================================================
  /// 시장가 진입 주문을 위해 사용하는 함수
  ///
  /// @param entry_name 진입 이름
  /// @param entry_direction 진입 방향
  /// @param order_size 진입 수량
  /// @param leverage 레버리지
  /// @return 주문 성공 여부
  bool MarketEntry(const string& entry_name, Direction entry_direction,
                   double order_size, int leverage);

  /// 지정가 진입 주문을 위해 사용하는 함수
  ///
  /// @param entry_name 진입 이름
  /// @param entry_direction 진입 방향
  /// @param order_price 지정가 진입 주문 가격
  /// @param order_size 진입 수량
  /// @param leverage 레버리지
  /// @return 주문 성공 여부
  bool LimitEntry(const string& entry_name, Direction entry_direction,
                  double order_price, double order_size, int leverage);

  /// Market if Touched 진입 주문을 위해 사용하는 함수.
  /// touch_price에 닿으면 시장가 진입 주문 접수.
  ///
  /// @param entry_name 진입 이름
  /// @param entry_direction 진입 방향
  /// @param touch_price 시장가 주문을 접수할 시점의 가격
  /// @param order_size 진입 수량
  /// @param leverage 레버리지
  /// @return 주문 성공 여부
  bool MitEntry(const string& entry_name, Direction entry_direction,
                double touch_price, double order_size, int leverage);

  /// Limit if Touched 진입 주문을 위해 사용하는 함수.
  /// touch_price에 닿으면 order_price에 지정가 진입 주문 접수.
  ///
  /// @param entry_name 진입 이름
  /// @param entry_direction 진입 방향
  /// @param touch_price 지정가 주문을 접수할 시점의 가격
  /// @param order_price 지정가 진입 주문 가격
  /// @param order_size 진입 수량
  /// @param leverage 레버리지
  /// @return 주문 성공 여부
  bool LitEntry(const string& entry_name, Direction entry_direction,
                double touch_price, double order_price, double order_size,
                int leverage);

  /// 트레일링 진입 주문을 위해 사용하는 함수.
  /// touch_price에 닿으면 진입 방향에 따라 최고저가를 추적하며, 최고저가 대비
  /// trail_point가 움직이면 시장가 진입 주문 접수.
  ///
  /// @param entry_name 진입 이름
  /// @param entry_direction 진입 방향
  /// @param touch_price 최고저가 추적을 시작할 시점의 가격.
  ///                    0으로 지정할 시 바로 추적을 시작
  /// @param trail_point 최고저가로부터 어느정도 움직였을 때 진입할지 결정하는
  ///                    포인트
  /// @param order_size 진입 수량
  /// @param leverage 레버리지
  /// @return 주문 성공 여부
  bool TrailingEntry(const string& entry_name, Direction entry_direction,
                     double touch_price, double trail_point, double order_size,
                     int leverage);

  // ===========================================================================
  // 전략 구현부에서 사용하는 청산 함수들
  // 반환 값은 항상 주문 성공 여부
  // ===========================================================================
  /// 시장가 청산 주문을 위해 사용하는 함수.
  ///
  /// @param exit_name 청산 이름
  /// @param target_name 청산할 진입 이름
  /// @param order_size 청산 수량
  /// @return 주문 성공 여부
  bool MarketExit(const string& exit_name, const string& target_name,
                  double order_size);

  /// 지정가 청산 주문을 위해 사용하는 함수.
  ///
  /// @param exit_name 청산 이름
  /// @param target_name 청산할 진입 이름
  /// @param order_price 지정가 청산 주문 가격
  /// @param order_size 청산 수량
  /// @return 주문 성공 여부
  bool LimitExit(const string& exit_name, const string& target_name,
                 double order_price, double order_size);

  /// Market if Touched 청산 주문을 위해 사용하는 함수.
  /// touch_price에 닿으면 시장가 청산 주문 접수.
  ///
  /// @param exit_name 청산 이름
  /// @param target_name 청산할 진입 이름
  /// @param touch_price 시장가 주문을 접수할 시점의 가격
  /// @param order_size 청산 수량
  /// @return 주문 성공 여부
  bool MitExit(const string& exit_name, const string& target_name,
               double touch_price, double order_size);

  /// Limit if Touched 청산 주문을 위해 사용하는 함수.
  /// touch_price에 닿으면 order_price에 지정가 청산 주문 접수.
  ///
  /// @param exit_name 청산 이름
  /// @param target_name 청산할 진입 이름
  /// @param touch_price 지정가 주문을 접수할 시점의 가격
  /// @param order_price 지정가 청산 주문 가격
  /// @param order_size 청산 수량
  /// @return 주문 성공 여부
  bool LitExit(const string& exit_name, const string& target_name,
               double touch_price, double order_price, double order_size);

  /// 트레일링 청산 주문을 위해 사용하는 함수.
  /// touch_price에 닿으면 청산 방향에 따라 최고저가를 추적하며, 최고저가 대비
  /// trail_point가 움직이면 시장가 청산 주문 접수.
  ///
  /// @param exit_name 청산 이름
  /// @param target_name 청산할 진입 이름
  /// @param touch_price 최고저가 추적을 시작할 시점의 가격.
  ///                    0으로 지정할 시 바로 추적을 시작
  /// @param trail_point 최고저가로부터 어느정도 움직였을 때 청산할지
  ///                    결정하는 포인트
  /// @param order_size 청산 수량
  /// @return 주문 성공 여부
  bool TrailingExit(const string& exit_name, const string& target_name,
                    double touch_price, double trail_point, double order_size);

  // ===========================================================================
  /// 현재 사용 중인 심볼 인덱스의 모든 진입 및 청산 대기 주문을 취소하는 함수
  void CancelAll(const string& cancellation_reason);

  /// 현재 사용 중인 심볼 인덱스의 모든 체결된 진입을 시장가로 다음 바 시가에서
  /// 청산하는 함수\n\n
  /// AFTER ENTRY, AFTER EXIT 전략에서 호출하더라도
  /// 다음 바 시가에서 청산됨
  void CloseAll();

  /// 체결된 진입 주문에서 Entry Name과 같은 이름의 진입 주문이 있는지 여부를
  /// 반환하는 함수
  [[nodiscard]] bool HasFilledEntryOrder(const string& entry_name) const;

  /// 체결된 진입 주문에서 Entry Names 중에서 같은 이름의 진입 주문이
  /// 몇 개 존재하는지 찾아 개수를 반환하는 함수
  [[nodiscard]] size_t CountEntryOrder(
      const unordered_set<string>& entry_names) const;

 private:
  // 싱글톤 인스턴스 관리
  OrderHandler();
  class Deleter {
   public:
    void operator()(const OrderHandler* p) const;
  };

  static mutex mutex_;
  static shared_ptr<OrderHandler> instance_;

  // ===========================================================================
  // 체결 확인 및 실행 함수
  // ===========================================================================
  /// 트레이딩 중인 심볼에서 지정된 가격에서 강제 청산이 체결됐는지 확인 후
  /// 체결해야 하는 주문 벡터에 추가하는 함수
  ///
  /// 고저가를 확인할 때 강제 청산되었으면 강제 청산 가격은 청산 가격과 마크
  /// 가격의 차이를 시장 가격에서 조정하므로 실제 가격과 다를 수 있음에 주의
  void CheckLiquidation(BarDataType market_bar_data_type, int symbol_idx, double price,
                        PriceType price_type);

  /// 트레이딩 중인 심볼에서 지정된 가격을 기준으로 청산 대기 주문들이
  /// 체결됐는지 확인 후 해당 주문 정보들을 체결해야 하는 주문 벡터에
  /// 추가하는 함수
  void CheckPendingExits(int symbol_idx, double price, PriceType price_type);

  /// 트레이딩 중인 심볼에서 지정된 가격을 기준으로 진입 대기 주문들이
  /// 체결됐는지 확인 후 해당 주문 정보들을 체결해야 하는 주문 벡터에
  /// 추가하는 함수
  void CheckPendingEntries(int symbol_idx, double price, PriceType price_type);

  /// 지정된 주문을 시그널과 주문 타입에 적합하게 체결하는 함수
  void FillOrder(const FillInfo& order_info, int symbol_idx,
                 PriceType price_type);

  /// 체결된 진입 주문에서 펀딩비를 정산하는 함수
  void ExecuteFunding(double funding_rate, const string& funding_time,
                      double funding_price, int symbol_idx);

  // ===========================================================================
  /// 강제 청산 가격 달성 시 자금 관련 처리 후 청산시키는 함수
  ///
  /// 청산 확인은 Mark Price이지만 청산 가격은 실제 시장 가격 기준이므로
  /// order_price는 시장 가격으로 지정
  void FillLiquidation(const shared_ptr<Order>& filled_entry,
                       const string& exit_name, int symbol_idx,
                       double fill_price);

  /// 시장가 진입 시 자금 관련 처리 후 체결 주문에 추가하는 함수
  ///
  /// @return 체결 성공 여부
  bool FillMarketEntry(const shared_ptr<Order>& market_entry, int symbol_idx,
                       PriceType price_type);

  /**
   * 현재 사용 중인 심볼에서 지정된 방향과 반대 방향의 진입 체결 주문이 있으면
   * 모두 청산하는 함수
   *
   * 진입 주문 가격에서 청산을 해야 그 가격에서 동시에 진입 체결이 가능하므로
   * 진입 주문 가격의 지정이 필요
   */
  void ExitOppositeFilledEntries(Direction target_entry_direction,
                                 double entry_order_price, int symbol_idx);

  /// 청산 시 자금, 통계 관련 처리를 하는 함수
  void ExecuteExit(const shared_ptr<Order>& exit_order, int symbol_idx);

  // ===========================================================================
  // 진입 주문 체결 확인 및 체결 함수
  // ===========================================================================
  /// 지정가 진입 대기 주문의 체결을 확인하고 해당 주문 정보를 반환하는 함수
  [[nodiscard]] static optional<FillInfo> CheckPendingLimitEntry(
      const shared_ptr<Order>& limit_entry, double price, PriceType price_type);

  /// MIT 진입 대기 주문의 체결을 확인하고 해당 주문 정보를 반환하는 함수
  [[nodiscard]] static optional<FillInfo> CheckPendingMitEntry(
      const shared_ptr<Order>& mit_entry, double price, PriceType price_type);

  /// LIT 진입 대기 주문의 체결을 확인하고 해당 주문 정보를 반환하는 함수
  [[nodiscard]] optional<FillInfo> CheckPendingLitEntry(
      const shared_ptr<Order>& lit_entry, int& order_idx, int symbol_idx,
      double price, PriceType price_type);

  /// 트레일링 진입 대기 주문의 체결을 확인하고 해당 주문 정보를 반환하는 함수
  [[nodiscard]] static optional<FillInfo> CheckPendingTrailingEntry(
      const shared_ptr<Order>& trailing_entry, double price,
      PriceType price_type);

  /// 현재 사용 중인 심볼에서 지정된 MIT/트레일링 진입 대기 주문을
  /// 시장가로 체결하는 함수. 자금 관련 처리를 하고 체결 주문으로 이동시킴.
  void FillPendingMarketEntry(const shared_ptr<Order>& market_entry,
                              int symbol_idx, double fill_price,
                              PriceType price_type);

  /// 현재 사용 중인 심볼에서 지정된 지정가/LIT 진입 대기 주문을 지정가로
  /// 체결하는 함수. 자금 관련 처리를 하고 체결 주문으로 이동시킴.
  void FillPendingLimitEntry(const shared_ptr<Order>& limit_entry,
                             int symbol_idx, double fill_price,
                             PriceType price_type);

  /// 현재 사용 중인 심볼에서 LIT 진입 대기 주문이 터치되었을 때 지정가로
  /// 주문하는 함수.
  ///
  /// @return 주문 성공 여부
  bool OrderPendingLitEntry(const shared_ptr<Order>& lit_entry, int& order_idx,
                            int symbol_idx, PriceType price_type);

  // ===========================================================================
  // 청산 주문 체결 확인 및 체결 함수
  // ===========================================================================
  /// 지정가 청산 대기 주문의 체결을 확인하고 해당 주문 정보를 반환하는 함수
  [[nodiscard]] static optional<FillInfo> CheckPendingLimitExit(
      const shared_ptr<Order>& limit_exit, double price, PriceType price_type);

  /// MIT 청산 대기 주문의 체결을 확인하고 해당 주문 정보를 반환하는 함수
  [[nodiscard]] static optional<FillInfo> CheckPendingMitExit(
      const shared_ptr<Order>& mit_exit, double price, PriceType price_type);

  /// LIT 청산 대기 주문의 체결을 확인하고 해당 주문 정보를 반환하는 함수
  [[nodiscard]] optional<FillInfo> CheckPendingLitExit(
      const shared_ptr<Order>& lit_exit, int symbol_idx, double price,
      PriceType price_type);

  /// 트레일링 청산 대기 주문의 체결을 확인하고 해당 주문 정보를 반환하는 함수
  [[nodiscard]] static optional<FillInfo> CheckPendingTrailingExit(
      const shared_ptr<Order>& trailing_exit, double price,
      PriceType price_type);

  /// 현재 사용 중인 심볼에서 지정된 청산 대기 주문을 시장가 혹은 지정가로
  /// 체결하는 함수. 자금 관련 처리를 하고 체결 주문으로 이동시킴.
  void FillPendingExitOrder(
      const shared_ptr<Order>& exit_order,
      const pair<shared_ptr<Order>, int>& target_entry_order, int symbol_idx,
      double fill_price);

  // ===========================================================================
  /// 체결된 진입 주문에서 Entry Name과 같은 이름의 진입 주문을 찾아
  /// 주문 인덱스와 함께 반환하는 함수
  [[nodiscard]] optional<pair<shared_ptr<Order>, int>> FindFilledEntryOrder(
      const string& entry_name, int symbol_idx) const;

  // 지정된 주문 시그널에서 해당 대기 주문이 존재하는지 여부를 반환하는 함수
  // (LIQUIDATION은 오류)
  [[nodiscard]] bool ExistsPendingOrder(const shared_ptr<Order>& pending_order,
                                        OrderSignal order_signal,
                                        int symbol_idx) const;

  /// 청산 주문 크기와 이미 체결된 청산 크기의 합이 진입 체결 크기를 넘지 않도록
  /// 조정하여 반환하는 함수
  [[nodiscard]] static double GetAdjustedExitSize(
      double exit_size, const shared_ptr<Order>& entry_order);

  /// 분석기에 청산된 거래를 추가하는 함수
  void AddTrade(const shared_ptr<Order>& exit_order, double realized_pnl,
                int symbol_idx) const;
};

}  // namespace backtesting::order