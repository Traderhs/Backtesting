#pragma once

// 표준 라이브러리
#include <memory>
#include <vector>

// 내부 헤더
#include "Engines/BaseAnalyzer.hpp"
#include "Engines/Numeric.hpp"

// 전방 선언
namespace backtesting::analyzer {
enum class PlotStyle;
class Analyzer;
}  // namespace backtesting::analyzer

namespace backtesting::bar {
class BarData;
class BarHandler;
}  // namespace backtesting::bar

namespace backtesting::engine {
class Engine;
}

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
using namespace analyzer;
using namespace bar;
using namespace engine;
using namespace numeric;
using namespace logger;
using namespace numeric;
}  // namespace backtesting

namespace backtesting::indicator {

/// 전략 구현 시 사용하는 지표를 생성하기 위한 추상 클래스
class Indicator {
 public:
  // 플롯 정보 참조용
  friend class BaseAnalyzer;

  // 지표 반환 시 참조 타입으로 받는 것을 강요하기 위하여
  // 복사 생성자, 할당 연산자 삭제
  Indicator(const Indicator&) = delete;
  Indicator& operator=(const Indicator&) = delete;

  /// 지표의 계산된 값을 반환하는 연산자 오버로딩.
  /// 사용법: 지표 클래스 객체[n개 바 전 인덱스]
  [[nodiscard]] Numeric<double> operator[](size_t index);

  /// 모든 심볼의 모든 바에 해당되는 지표 값을 계산하는 함수
  void CalculateIndicator(const string& strategy_name);

  /// 계산된 지표값을 지정된 경로의 심볼 폴더에 csv 파일로 저장하는 함수
  ///
  /// 전략별로 저장된 지표가 다르고, 심볼별로 지표의 Open Time 누락 정도가
  /// 다르며, 지표별로 타임프레임이 다르므로 모두 따로 나누어저장
  void SaveIndicator(const string& indicators_strategy_path) const;

  /// 지표의 타임프레임을 설정하는 함수
  void SetTimeframe(const string& timeframe);

  /// 지표의 이름을 반환하는 함수
  [[nodiscard]] string GetName() const;

  /// 지표의 타임프레임을 반환하는 함수
  [[nodiscard]] string GetTimeframe() const;

 protected:
  Indicator(const string& name, const string& timeframe, bool overlay,
            PlotStyle plot_style, const Color& color, int line_width);
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
  // 커스텀 지표 생성 시 Strategy 클래스의 AddIndicator 함수 사용을
  // 강제하기 위한 목적
  // 생성 카운터
  static size_t creation_counter_;
  // 전 생성 카운터
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
  static bool is_calculating_;
  static string calculating_name_;       /// 계산 중인 지표의 이름
  static string calculating_timeframe_;  /// 계산 중인 지표의 타임프레임

  // 플롯 정보
  bool overlay_;              // 차트 위에 덮어씌울지 여부
  PlotStyle plot_style_;      // 플롯 스타일
  Color color_;               // 색
  unsigned char line_width_;  // 굵기

  /// 지표 생성 카운터를 증가시키는 함수
  static void IncreaseCreationCounter();
  friend class strategy::Strategy;
};

}  // namespace backtesting::indicator
using namespace backtesting::indicator;
