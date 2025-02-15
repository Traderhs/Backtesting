#pragma once

// 표준 라이브러리
#include <deque>
#include <vector>

// 전방 선언
class Analyzer;
class BarHandler;
class Engine;

// 내부 헤더
#include "Engines/Config.hpp"
#include "Engines/Logger.hpp"
#include "Engines/Order.hpp"

// 네임 스페이스
using namespace std;

/// 주문, 포지션 등과 관련된 기본적인 작업을 처리하는 클래스
class BaseOrderHandler {
 public:
  // ReSharper disable once CppInconsistentNaming
  /// 현재 심볼의 포지션 사이즈. 양수면 매수 진입, 음수면 매도 진입.
  double current_position_size;

  /// 엔진 설정을 불러오고 주문들을 심볼의 개수 크기로 초기화하는 함수
  void InitializeOrderHandler(int num_symbols);

  /// 현재 미실현 손익의 합계를 반환하는 함수.
  /// 업데이트 시점의 모든 심볼의 가격을 알기 어려우므로, 시가 시점의 평가
  /// 손익으로 타협하여 계산
  [[nodiscard]] double GetUnrealizedPnl() const;

  /// 현재 심볼의 포지션 사이즈 합계를 업데이트하는 함수
  void UpdateCurrentPositionSize();

 protected:
  BaseOrderHandler();
  ~BaseOrderHandler();

  static shared_ptr<Analyzer>& analyzer_;
  static shared_ptr<BarHandler>& bar_;
  static shared_ptr<Engine>& engine_;
  static shared_ptr<Logger>& logger_;
  Config config_;

  // 진입 및 청산 주문: 심볼 인덱스<주문>
  vector<deque<shared_ptr<Order>>> pending_entries_;  // 대기 중인 진입 주문
  vector<deque<shared_ptr<Order>>> filled_entries_;   // 체결된 진입 주문
  vector<deque<shared_ptr<Order>>> pending_exits_;    // 대기 중인 청산 주문

  /// 주문 정보에 따라 슬리피지를 반영한 체결 가격을 반환하는 함수.
  [[nodiscard]] double CalculateSlippagePrice(double order_price,
                                              OrderType order_type,
                                              Direction direction,
                                              int leverage) const;

  /// 주문 정보에 따라 수수료 금액을 계산하여 반환하는 함수
  [[nodiscard]] double CalculateCommission(double filled_price,
                                           OrderType order_type,
                                           double filled_position_size,
                                           int leverage) const;

  /// 주문 정보에 따라 마진콜 가격을 계산하여 반환하는 함수
  [[nodiscard]] static double CalculateMarginCallPrice(
      double entry_filled_price, Direction entry_direction, int leverage);

  /// 진입 정보에 따라 PnL을 계산하는 함수
  static double CalculatePnl(double base_price, Direction entry_direction,
                             double entry_price, double position_size,
                             int leverage);

  // 방향이 유효한 값인지 확인하는 함수
  static void IsValidDirection(Direction direction);

  // 가격이 유효한 값인지 확인하는 함수
  static void IsValidPrice(double price);

  // 포지션 크기가 유효한 값인지 확인하는 함수
  static void IsValidPositionSize(double position_size);

  // 레버리지가 유효한 값인지 확인하는 함수
  static void IsValidLeverage(int leverage);

  /// 진입 체결 시 진입 이름이 유효한지 확인하는 함수
  void IsValidEntryName(const string& entry_name) const;

  /// 지정가 주문 가격이 유효한 가격인지 확인하는 함수
  static void IsValidLimitOrderPrice(double limit_price, double base_price,
                                     Direction direction);

  /// 트레일링 진입/청산의 터치 가격이 유효한지 확인하는 함수.
  /// 트레일링 진입/청산의 터치 가격은 0으로 지정될 수 있기 때문에 별개 함수로
  /// 처리.
  static void IsValidTrailingTouchPrice(double touch_price);

  /// 트레일링 포인트가 유효한지 확인하는 함수
  static void IsValidTrailPoint(double trail_point);

  /// 디버그 모드에서 심볼 이름, 현재 Open Time으로 포맷된 로그를 발생시키는
  /// 함수.
  static void LogFormattedInfo(LogLevel log_level,
                               const string& formatted_message,
                               const char* file, int line);
};
