#pragma once

// 전방 선언
class OrderHandler;
class BarHandler;

// 내부 헤더
#include "Indicators/Indicators.hpp"

// 네임 스페이스
using namespace std;

/// 백테스팅 전략을 생성하기 위한 가상 클래스
class Strategy {
 public:
  /// 전략 실행 전 초기화를 통해 값을 미리 계산하기 위한 함수.
  virtual void Initialize() = 0;

  /// 모든 바의 종가에서 전략을 실행하는 함수. 메인 로직을 작성.
  virtual void Execute() = 0;

  /// 해당 전략의 이름을 반환하는 함수
  [[nodiscard]] string GetName() const;

  /// 해당 전략의 주문 핸들러를 반환하는 함수
  [[nodiscard]] shared_ptr<OrderHandler> GetOrderHandler() const;

 protected:
  explicit Strategy(string name);
  virtual ~Strategy();

  /// 전략 작성 시 사용하는 핸들러
  // ReSharper disable once CppInconsistentNaming
  shared_ptr<OrderHandler>&
      order;  // 주문 핸들러: 다형성에 의한 동적 작동하므로 static 제외

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
