#pragma once

// 표준 라이브러리
#include <cfloat>
#include <format>
#include <regex>

// 내부 헤더
#include "Engines/Logger.hpp"
// ReSharper disable once CppUnusedIncludeDirective
#include "Engines/Order.hpp"  // 커스텀 전략에서 사용 편의성을 위해 직접 포함
#include "Engines/OrderHandler.hpp"  // 커스텀 전략에서 사용 편의성을 위해 직접 포함
#include "Engines/Plot.hpp"
#include "Indicators/Indicators.hpp"

// 네임 스페이스
using namespace std;
namespace backtesting {
using namespace order;
using namespace plot;
}  // namespace backtesting

namespace backtesting::strategy {

/**
 * 백테스팅 전략을 생성하기 위한 팩토리 클래스
 *
 * ※ 커스텀 전략 생성 시 유의 사항 ※\n
 * 1. Strategy 클래스를 Public 상속 후
 *    Initialize, ExecuteOnClose, ExecuteAfterEntry, ExecuteAfterExit
 *    함수들을 오버라이드해서 제작\n
 *
 *    Initialize → 엔진 초기화 시 최초 1회 실행\n
 *    ExecuteOnCLose → 트레이딩 바 종가마다 모든 심볼에서 실행\n
 *    ExecuteAfterEntry → 진입 체결이 있었다면 해당 심볼에서만 즉시 실행\n
 *    ExecuteAfterExit → 청산 체결이 있었다면 해당 심볼에서만 즉시 실행\n
 *
 * 2. Strategy 생성자에는 전략 이름과 커스텀 전략의 소스 코드를 저장하기 위하여
 *    __FILE__ 매크로 값을 반드시 포함해야 함\n
 *
 * 3. 커스텀 지표는 AddIndicator 템플릿 함수로 추가 가능\n
 *    AddIndicator<커스텀 지표>(플롯(선택), 이름, 타임프레임)
 *
 * 4. 타임프레임을 트레이딩 바 데이터 타임프레임과 일치시키고 싶으면,
 *    trading_timeframe 변수를 사용하면 됨.\n
 *
 * 5. 플롯은 Area, BaseLine, Histogram, Line 중에서 선택 가능하며,
 *    해당 클래스의 생성자를 참고하여 생성하여 전달하면 됨.
 *    전달하지 않거나 NullPlot 전달 시 해당 지표의 플롯을 끔.\n
 *
 * 6. 추가한 커스텀 지표를 전략에서 참조하기 위해서는 커스텀 지표 타입의
 *    참조 변수에 저장해야 함.\n
 *    번외로, 가격 참조를 위해 open, high, low, close, volume 지표가
 *    기본 제공됨.\n
 *
 * 7. 참조 방법은 참조 변수[인덱스]이며, 인덱스 n은 n봉 전 트레이딩 바의 값\n
 *
 * 8. 트레이딩 바 타임프레임보다 지표의 타임프레임이 큰 경우,
 *    지표 바의 Open Time이 트레이딩 바의 Open Time이 동일해진 순간,
 *    다음 Open Time이 동일해지기 전까지 지표의 전 바의 값이 참조됨\n
 *
 * 9. 부가 기능으로, 진입 잔량을 전량 청산하고 싶으면 left_size 변수를
 *    청산 수량에 사용하면 됨.\n
 */
class Strategy {
 public:
  // Factory 메서드 추가
  // 전략을 팩토리로 우회하여 생성하고 strategy_에 추가하고 반환하는 함수
  template <typename CustomStrategy, typename... Args>
  static void AddStrategy(const string& name, Args&&... args) {
    // AddStrategy 함수를 통할 때만 생성 카운터 증가
    creation_counter_++;

    strategies_.push_back(
        std::make_shared<CustomStrategy>(name, std::forward<Args>(args)...));

    logger->Log(INFO_L, format("[{}] 전략이 엔진에 추가되었습니다.", name),
                __FILE__, __LINE__);
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
  static void SetTradingTimeframe(const string& trading_tf);

  /// 생성된 전략들의 벡터를 반환하는 함수
  [[nodiscard]] static vector<shared_ptr<Strategy>>& GetStrategies();

  /// 해당 전략에서 사용하는 지표들을 반환하는 함수
  [[nodiscard]] vector<shared_ptr<Indicator>>& GetIndicators();

  /// 해당 전략의 이름을 반환하는 함수
  [[nodiscard]] string GetName() const;

  /// 해당 전략의 소스 파일 경로를 반환하는 함수
  string GetSourcePath();

  /// 해당 전략의 주문 핸들러를 반환하는 함수
  [[nodiscard]] shared_ptr<OrderHandler> GetOrderHandler() const;

 private:  // indicators_를 먼저 초기화 시키기 위하여 protected보다 위에 위치
  // 커스텀 전략 생성 시 AddStrategy 함수 사용을 강제하기 위한 목적
  // 생성 카운터
  static size_t creation_counter_;
  // 전 생성 카운터
  static size_t pre_creation_counter_;

  static vector<shared_ptr<Strategy>> strategies_;  // 생성한 전략들
  vector<shared_ptr<Indicator>> indicators_;  // 해당 전략에서 사용하는 지표들

  string name_;             /// 전략의 이름
  string child_file_path_;  /// 커스텀 전략의 파일 경로
                            /// → 백테스팅 종료 후 소스 코드 저장 목적

 protected:
  /// 전략 생성자
  ///
  /// @param name 전략의 이름
  /// @param child_file_path 커스텀 전략의 파일 경로 (__FILE__ 매크로로 전달)
  explicit Strategy(const string& name, const string& child_file_path);
  virtual ~Strategy();

  // ※ AddIndicator 함수의 존재 의의는,
  //    전략에서 사용하는 지표들을 구별해야 하기 위함

  /// 전략에 지표를 추가하는 함수
  ///
  ///
  /// @param name 커스텀 전략에 추가할 템플릿 지표의 이름
  /// @param timeframe 커스텀 전략에 추가할 템플릿 지표의 타임프레임
  /// @param plot 플롯 정보를 담은 Area | Baseline | Histogram | Line
  ///             클래스 객체. NullPlot으로 설정되면 플롯하지 않음.
  /// @param args 템플릿 지표의 추가적인 매개변수
  template <typename CustomIndicator, typename... Args>
  CustomIndicator& AddIndicator(const string& name, const string& timeframe,
                                const Plot& plot = NullPlot(), Args&&... args) {
    // AddIndicator 함수를 통할 때만 생성 카운터 증가
    Indicator::IncreaseCreationCounter();

    const auto& indicator = std::make_shared<CustomIndicator>(
        name, timeframe, plot, std::forward<Args>(args)...);

    indicators_.push_back(indicator);

    return *indicator;
  }

  // ===========================================================================
  // 전략에서 사용 가능한 핸들러 및 변수들

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
  static string trading_timeframe;

  /// 전략 작성 편의성용 가격 데이터 지표화
  /// 가격 데이터는 플롯 설정과 관련없이 하나의 캔들로 플롯됨
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
  const double left_size = DBL_MAX;
};

}  // namespace backtesting::strategy
using namespace strategy;