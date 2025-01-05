#pragma once

// 표준 라이브러리
#include <deque>
#include <vector>

// 내부 헤더
#include "Engines/BarHandler.hpp"
#include "Engines/Engine.hpp"
#include "Engines/Logger.hpp"
#include "Engines/Order.hpp"

// 네임 스페이스
using namespace std;

/**
 * 주문, 포지션 등과 관련된 기본적인 작업을 처리하는 클래스
 */
class BaseOrderHandler {
 public:
  /// 포지션 방향을 지정하는 열거형 클래스
  enum class Direction { NONE, LONG, SHORT };

  /// 주문 타입을 지정하는 열거형 클래스
  enum class OrderType { NONE, MARKET, LIMIT, MIT, LIT, TRAILING };

  // ReSharper disable once CppInconsistentNaming
  /// 현재 사용 중인 심볼의 포지션 사이즈: 전략 구현부에서 사용
  int current_position_size;  // @@@@@@@@@@@@@@@ 종가 전략 평가 시 심볼 바뀔 때
                              // 바뀌는 거 추가

  /// 주문 정보를 가지고 있는 변수를 심볼의 개수 크기로 초기화하는 함수
  void InitializeOrders(int num_symbols);

  /// 현재 미실현 손익의 합계를 반환하는 함수.
  /// 업데이트 시점의 모든 심볼의 가격을 알기 어려우므로, 시가 시점의 평가
  /// 손익으로 타협하여 계산
  [[nodiscard]] double GetUnrealizedPnl() const;

  // 트레일링 주문 방향에 따라 초기 최고저가를 찾아 반환하는 함수
  static double GetInitialExtremePrice(Direction direction);

 protected:
  BaseOrderHandler();
  ~BaseOrderHandler();

  static BarHandler& bar_;
  static Engine& engine_;
  static Logger& logger_;
  Config& config_;

  // 주문 정보를 가지고 있는 변수: 심볼 인덱스<주문>
  vector<deque<shared_ptr<Order>>> pending_entries_;  // 대기 중인 진입 주문
  vector<deque<shared_ptr<Order>>> filled_entries_;   // 체결된 진입 주문
  vector<deque<shared_ptr<Order>>> pending_exits_;    // 대기 중인 청산 주문
  vector<shared_ptr<Order>> filled_exits_;            // 체결된 청산 주문

  /// 주문 타입에 따라 슬리피지를 반영한 체결 가격을 반환하는 함수.
  [[nodiscard]] double CalculateSlippagePrice(
      double order_price, OrderType order_type, Direction direction,
      const shared_ptr<Order>& order) const;

  /// 주문 타입에 따라 수수료 금액을 계산하여 반환하는 함수
  [[nodiscard]] double CalculateCommission(
      double filled_price, OrderType order_type, double filled_position_size,
      const shared_ptr<Order>& order) const;

  /// 마진콜 가격을 계산하여 반환하는 함수
  [[nodiscard]] static double CalculateMarginCallPrice(const shared_ptr<Order>& order);

  // 가격이 유효한 값인지 확인하는 함수
  static void IsValidPrice(double price);

  // 포지션 크기가 유효한 값인지 확인하는 함수
  static void IsValidPositionSize(double position_size);

  // 레버리지가 유효한 값인지 확인하는 함수
  static void IsValidLeverage(unsigned char leverage);
};
