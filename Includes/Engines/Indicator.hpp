#pragma once

// 표준 라이브러리
#include <memory>
#include <vector>

// 내부 헤더
#include "Engines/BaseAnalyzer.hpp"
#include "Engines/BaseEngine.hpp"
#include "Engines/Numeric.hpp"
#include "Engines/Plot.hpp"

// 전방 선언
namespace backtesting::analyzer {
enum class PlotStyle;
class BaseAnalyzer;
}  // namespace backtesting::analyzer

namespace backtesting::bar {
class BarData;
class BarHandler;
}  // namespace backtesting::bar

namespace backtesting::engine {
class Engine;
}  // namespace backtesting::engine

namespace backtesting::strategy {
class Strategy;
}

namespace backtesting::logger {
class Logger;
}

// 네임 스페이스
using namespace std;
using namespace backtesting;  // 커스텀 지표에서 필요
namespace backtesting {
using namespace bar;
using namespace engine;
using namespace numeric;
using namespace logger;
using namespace numeric;
using namespace plot;
}  // namespace backtesting

namespace backtesting::indicator {

/**
 * 전략 구현 시 사용하는 커스텀 지표를 생성하기 위한 추상 클래스
 *
 * ※ 커스텀 지표 생성 시 유의 사항 ※\n
 * 1. Indicator 클래스를 Public 상속 후 Initialize, Calculate 함수들을
 *    오버라이드해서 제작\n
 *
 *    Initialize → 지표 계산 시 최초 1회 실행\n
 *    Calculate → 각 바마다 값을 계산하여 반환 \n
 *
 * 2. 상속받은 지표 생성자에는 [지표 이름, 타임프레임, 플롯 타입 문자열,
 *    Plot 객체]를 순서 동일하게 반드시 포함해야 함\n
 *
 * 3. 커스텀 지표 내에서 다른 지표를 사용하기 위해서는, 생성자에서
 *    Indicator& 타입의 인수를 받아 [] 연산자로 참조하여 사용하면 됨.\n
 *    ※ 주의: 인수로 넣을 다른 지표는 커스텀 지표보다 먼저 정의되어야 함.
 *
 * 4. 커스텀 지표의 타임프레임과 다른 타임프레임의 지표는 사용 불가능\n
 */
class Indicator {
  // friend는 using 영향 받지 않으므로 모든 네임 스페이스를 작성
 
  // 생성자 및 IncreaseCreationCounter 함수 접근용
  friend class backtesting::strategy::Strategy;

  // Plot 유효성 검사 시 plot_ 접근용
  friend class backtesting::engine::Engine;

  // 차트 작성 시 output_ 및 plot_ 접근용
  friend class backtesting::analyzer::BaseAnalyzer;

 public:
  // 지표 반환 시 참조 타입으로 받는 것을 강요하기 위하여
  // 복사 생성자, 할당 연산자 삭제
  Indicator(const Indicator&) = delete;
  Indicator& operator=(const Indicator&) = delete;

  /// 지표의 계산된 값을 반환하는 연산자 오버로딩.
  /// 사용법: 지표 클래스 객체[n개 바 전 인덱스]
  [[nodiscard]] Numeric<double> operator[](size_t index);

  /// 모든 심볼의 모든 바에 해당되는 지표 값을 계산하는 함수
  void CalculateIndicator(const string& strategy_name);

  /// 지표의 타임프레임을 설정하는 함수
  void SetTimeframe(const string& timeframe);

  /// 지표의 이름을 반환하는 함수
  [[nodiscard]] string GetName() const;

  /// 지표의 타임프레임을 반환하는 함수
  [[nodiscard]] string GetTimeframe() const;

 protected:
  Indicator(const string& name, const string& timeframe, const Plot& plot);
  virtual ~Indicator();

  static shared_ptr<Analyzer>& analyzer_;
  static shared_ptr<BarHandler>& bar_;
  static shared_ptr<Engine>& engine_;
  static shared_ptr<Logger>& logger_;

  /// 커스텀 지표들의 멤버 변수들을 초기화하는 함수.
  /// 심볼별 지표 계산 시 각 심볼에서 멤버 변수들을 초기 상태로 복구하여
  /// 초기부터 계산할 수 있게 해야 함.
  virtual void Initialize() = 0;

  /// 각 바에서 지표를 계산하는 함수. 메인 로직을 작성.
  virtual Numeric<double> Calculate() = 0;

 private:
  // 카운터는 커스텀 지표 생성 시 Strategy 클래스의 AddIndicator 함수 사용을
  // 강제하기 위한 목적
  /// 생성 카운터
  static size_t creation_counter_;
  /// 전 생성 카운터
  static size_t pre_creation_counter_;

  string name_;                             // 지표의 이름
  string timeframe_;                        // 지표의 타임프레임
  vector<double> input_;                    // 지표의 파라미터
  vector<vector<Numeric<double>>> output_;  // 지표의 계산된 값: 심볼<값>
  bool is_calculated_;                 // 지표가 계산되었는지 확인하는 플래그
  vector<size_t> reference_num_bars_;  // 지표의 타임프레임에 해당되는
                                       // 참조 바 데이터의 심볼별 바 개수

  // 지표가 현재 계산 중인지 확인하는 플래그.
  // 지표 계산 시 사용하는 다른 지표가 계산하는 지표와 다른 타임프레임을 가질 수
  // 없게 검사할 때 사용
  static bool is_calculating_;           // 현재 지표 계산 중인지 여부
  static string calculating_name_;       // 계산 중인 지표의 이름
  static string calculating_timeframe_;  // 계산 중인 지표의 타임프레임

  // 플롯 정보
  string plot_type_;       // 플롯 클래스명
  shared_ptr<Plot> plot_;  // 플롯 정보

  /// 지표 생성 카운터를 증가시키는 함수
  static void IncreaseCreationCounter();
};

}  // namespace backtesting::indicator
using namespace backtesting::indicator;
