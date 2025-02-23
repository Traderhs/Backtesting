#pragma once

// 표준 라이브러리
#include <cfloat>
#include <optional>

// 전방 선언
class BarHandler;

// 내부 헤더
#include "Engines/Order.hpp"
#include "Engines/OrderHandler.hpp"
#include "Indicators/Indicators.hpp"

// 네임 스페이스
using namespace std;
using enum Direction;

/// 백테스팅 전략을 생성하기 위한 가상 클래스
class Strategy {
 public:
  Strategy() = delete;

  /// 전략 실행 전 초기화를 통해 값을 미리 계산하기 위한 함수.
  virtual void Initialize() = 0;

  /// 모든 바의 종가에서 전략을 실행하는 함수
  virtual void ExecuteOnClose() = 0;

  /// 특정 심볼의 진입 직후 전략을 실행하는 함수
  virtual void ExecuteAfterEntry() = 0;

  /// 특정 심볼의 청산 직후 전략을 실행하는 함수
  virtual void ExecuteAfterExit() = 0;

  /// 엔진 초기화 시 trading_timeframe을 초기화하는 함수
  void SetTradingTimeframe(const string& trading_timeframe);

  /// 해당 전략의 이름을 반환하는 함수
  [[nodiscard]] string GetName() const;

  /// 해당 전략의 주문 핸들러를 반환하는 함수
  [[nodiscard]] shared_ptr<OrderHandler> GetOrderHandler() const;

 protected:
  explicit Strategy(string name);
  virtual ~Strategy();

  // ReSharper disable once CppInconsistentNaming
  /// 전략 작성 시 사용하는 주문 핸들러
  shared_ptr<OrderHandler>& order;  // 다형성에 의한 동적 작동하므로 static 제외

  // ReSharper disable once CppInconsistentNaming
  /// 전략 작성 시 사용하는 바 핸들러
  static shared_ptr<BarHandler>& bar;

  // ReSharper disable once CppInconsistentNaming
  /// 트레이딩 바 타임프레임
  optional<string> trading_timeframe;

  // ReSharper disable once CppInconsistentNaming
  /// 커스텀 지표에서 청산 시 진입 잔량의 전량 청산을 위해 사용하는 변수.
  ///
  /// 엔진 내부적으로 청산 수량은 진입 잔량의 최대값으로 변환되기 때문에
  /// double 최대값으로 사용
  const double entry_size = DBL_MAX;

  /// 전략 작성 편의성용 가격 데이터 지표화
  // ReSharper disable once CppInconsistentNaming
  Open open;  // 시가 데이터
  // ReSharper disable once CppInconsistentNaming
  High high;  // 고가 데이터
  // ReSharper disable once CppInconsistentNaming
  Low low;  // 저가 데이터
  // ReSharper disable once CppInconsistentNaming
  Close close;  // 종가 데이터
  // ReSharper disable once CppInconsistentNaming
  Volume volume;  // 거래량 데이터

 private:
  string name_;  // 전략의 이름
};
