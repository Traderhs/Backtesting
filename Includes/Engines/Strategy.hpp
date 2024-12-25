#pragma once

// 내부 헤더
#include "Engine.hpp"
#include "OrderManager.hpp"

class Strategy {
 public:
  explicit Strategy(string name);
  virtual ~Strategy();

  // 전략 실행 전 초기화를 통해 값을 미리 계산하기 위한 함수
  virtual void Initialize() = 0;

  // 매 봉마다 전략을 실행하는 함수
  virtual void Execute() = 0;

 protected:
  static OrderManager& order;

 private:
  string name;
};
