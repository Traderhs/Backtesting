#pragma once

// 표준 라이브러리
#include <cfloat>
#include <format>

// 전방 선언
class BarHandler;
class Engine;
class Logger;

// 내부 헤더
#include "Engines/Order.hpp"
#include "Engines/OrderHandler.hpp"
#include "Indicators/Indicators.hpp"

// 네임 스페이스
using namespace std;
using enum Direction;

/// 백테스팅 전략을 생성하기 위한 팩토리 클래스
class Strategy {
 public:
  // Factory 메서드 추가
  // 전략을 팩토리로 우회하여 생성하고 strategy_에 추가하고 반환하는 함수
  template <typename CustomStrategy, typename... Args>
  static void AddStrategy(const string& name, Args&&... args) {
    // AddStrategy 함수를 통할 때만 생성 카운터 증가
    creation_counter_++;

    strategy_.push_back(
        std::make_shared<CustomStrategy>(name, std::forward<Args>(args)...));

    logger->Log(LogLevel::INFO_L,
                format("[{}] 전략이 엔진에 추가되었습니다.", name), __FILE__,
                __LINE__);
  }

  /// 전략 실행 전 초기화를 통해 값을 미리 계산하기 위한 함수.
  virtual void Initialize() = 0;

  /// 모든 바의 종가에서 전략을 실행하는 함수
  virtual void ExecuteOnClose() = 0;

  /// 특정 심볼의 진입 직후 전략을 실행하는 함수
  virtual void ExecuteAfterEntry() = 0;

  /// 특정 심볼의 청산 직후 전략을 실행하는 함수
  virtual void ExecuteAfterExit() = 0;

  /// 엔진 초기화 시 trading_timeframe을 설정하는 함수
  void SetTradingTimeframe(const string& trading_timeframe);

  /// 생성된 전략들의 벡터를 반환하는 함수
  static vector<shared_ptr<Strategy>>& GetStrategies();

  /// 해당 전략에서 사용하는 지표들을 반환하는 함수
  vector<shared_ptr<Indicator>>& GetIndicators();

  /// 해당 전략의 이름을 반환하는 함수
  [[nodiscard]] string GetName() const;

  /// 해당 전략의 주문 핸들러를 반환하는 함수
  [[nodiscard]] shared_ptr<OrderHandler> GetOrderHandler() const;

 private:  // indicators_를 먼저 초기화 시키기 위하여 protected보다 위에 위치
  // 커스텀 전략 생성 시 AddStrategy 함수 사용을 강제하기 위한 목적
  // 생성 카운터
  static size_t creation_counter_;
  // 전 생성 카운터
  static size_t pre_creation_counter_;

  static vector<shared_ptr<Strategy>> strategy_;  // 생성한 전략들
  vector<shared_ptr<Indicator>> indicators_;  // 해당 전략에서 사용하는 지표들

  string name_;  // 전략의 이름

 protected:
  explicit Strategy(const string& name);
  virtual ~Strategy();

  /// 전략에 지표를 추가하는 함수
  template <typename CustomIndicator, typename... Args>
  CustomIndicator& AddIndicator(const string& name, const string& timeframe,
                                Args&&... args) {
    // AddIndicator 함수를 통할 때만 생성 카운터 증가
    Indicator::IncreaseCreationCounter();

    const auto& indicator = std::make_shared<CustomIndicator>(
        name, timeframe, std::forward<Args>(args)...);
    indicators_.push_back(indicator);

    return *indicator;
  }

  // ReSharper disable once CppInconsistentNaming
  /// 전략 작성 시 사용하는 바 핸들러
  static shared_ptr<BarHandler>& bar;

  // ReSharper disable once CppInconsistentNaming
  /// 전략 작성 시 사용하는 엔진
  static shared_ptr<Engine>& engine;

  // ReSharper disable once CppInconsistentNaming
  /// 전략 작성 시 사용하는 로거
  static shared_ptr<Logger>& logger;

  // ReSharper disable once CppInconsistentNaming
  /// 전략 작성 시 사용하는 주문 핸들러
  shared_ptr<OrderHandler>& order;  // 다형성에 의한 동적 작동하므로 static 제외

  // ReSharper disable once CppInconsistentNaming
  /// 트레이딩 바 타임프레임
  string trading_timeframe;

  /// 전략 작성 편의성용 가격 데이터 지표화
  // ReSharper disable once CppInconsistentNaming
  Open& open;  // 시가 데이터
  // ReSharper disable once CppInconsistentNaming
  High& high;  // 고가 데이터
  // ReSharper disable once CppInconsistentNaming
  Low& low;  // 저가 데이터
  // ReSharper disable once CppInconsistentNaming
  Close& close;  // 종가 데이터
  // ReSharper disable once CppInconsistentNaming
  Volume& volume;  // 거래량 데이터

  // ReSharper disable once CppInconsistentNaming
  /// 커스텀 지표에서 청산 시 진입 잔량의 전량 청산을 위해 사용하는 변수.
  ///
  /// 엔진 내부적으로 청산 수량은 진입 잔량의 최대값으로 변환되기 때문에
  /// double 최대값으로 사용
  const double entry_size = DBL_MAX;
};
