#pragma once

// 내부 헤더
#include "Engines/OrderHandler.hpp"

/**
 * 백테스팅 전략을 생성하기 위한 가상 클래스
 */
class Strategy {
 public:
  explicit Strategy(string name);
  virtual ~Strategy();

  /// 전략 실행 전 초기화를 통해 값을 미리 계산하기 위한 함수
  virtual void Initialize() = 0;

  /// 매 봉마다 전략을 실행하는 함수
  virtual void Execute() = 0;

  /// 해당 전략의 주문 핸들러를 반환하는 함수
  [[nodiscard]] OrderHandler& GetOrderHandler() const;

 protected:
  static OrderHandler& order_;  // 주문 핸들러

 private:
  string name_;  // 전략 이름
};
