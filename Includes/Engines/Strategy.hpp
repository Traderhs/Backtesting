#pragma once

// 표준 라이브러리
#include <cfloat>
#include <filesystem>
#include <format>
#include <typeinfo>

// 내부 헤더
#include "Engines/DataUtils.hpp"
#include "Engines/Engine.hpp"
#include "Engines/Export.hpp"
#include "Engines/Logger.hpp"
// ReSharper disable once CppUnusedIncludeDirective
#include "Engines/Order.hpp"  // 커스텀 전략에서 사용 편의성을 위해 직접 포함
#include "Engines/OrderHandler.hpp"  // 커스텀 전략에서 사용 편의성을 위해 직접 포함
#include "Engines/Plot.hpp"
#include "Indicators/Indicators.hpp"  // 커스텀 전략에서 사용 편의성을 위해 직접 포함

// 내부 include에서 BACKTESTING_API가 #undef 되어 빈 값으로 변경되었을 수 있고,
// Strategy 클래스 정의에는 dllimport/dllexport 속성이 필요하므로
// 여기서 상태를 복구
#if defined(STRATEGY_BUILD) && !defined(BACKTESTING_EXPORTS)
#undef BACKTESTING_API
#define BACKTESTING_API __declspec(dllimport)
#endif

// 전방 선언
namespace backtesting::engine {
class Engine;
}

namespace backtesting::main {
class Backtesting;
}

// 네임 스페이스
using namespace std;
namespace fs = filesystem;
namespace backtesting {
using namespace engine;
using namespace main;
using namespace order;
using namespace plot;
using namespace utils;
}  // namespace backtesting

namespace backtesting::strategy {

/**
 * 백테스팅 전략을 생성하기 위한 팩토리 클래스
 *
 * ※ 커스텀 전략 생성 시 유의 사항 ※\n
 * 0. 커스텀 전략 클래스가 DLL로 로드될 가능성이 있다면
 *     클래스 선언에 BACKTESTING_API 매크로를 반드시 명시.
 *    이는 런타임에 심볼을 올바르게 노출하기 위하여 필수적
 *
 * 1. Strategy 클래스를 Public 상속 후
 *    Initialize, ExecuteOnClose, ExecuteAfterEntry, ExecuteAfterExit
 *    함수들을 오버라이드해서 제작\n
 *
 *    Initialize → 엔진 초기화 시 최초 1회 실행\n
 *    ExecuteOnCLose → 트레이딩 바 종가마다 모든 심볼에서 실행\n
 *    ExecuteAfterEntry → 진입 체결이 있었다면 해당 심볼에서만 즉시 실행\n
 *    ExecuteAfterExit → 청산 체결이 있었다면 해당 심볼에서만 즉시 실행
 *                       ExecuteAfterEntry보다 우선 순위가 높음\n
 *
 * 2. 헤더 파일 및 소스 파일은 자동으로 탐색하여 저장.
 *    파일명과 클래스명이 동일하고, 지정된 경로에 존재할 때만 소스 파일 탐지.
 *    (프로젝트 폴더/Includes/Strategies/클래스명.hpp 그리고
 *     프로젝트 폴더/Sources/cpp/Strategies/클래스명.cpp)\n
 *
 * 3. 전략에서 사용하는 커스텀 지표는 AddIndicator 템플릿 함수로 추가 가능\n
 *    AddIndicator<커스텀 지표>(이름, 타임프레임, 플롯(선택))
 *
 * 4. 지표의 타임프레임을 트레이딩 바 데이터 타임프레임과 일치시키고 싶으면,
 *    trading_timeframe 변수를 사용하면 됨.\n
 *
 * 5. 플롯은 Area, BaseLine, Histogram, Line 중에서 선택 가능하며,
 *    해당 클래스의 생성자를 참고하여 생성하여 전달하면 됨.
 *    전달하지 않거나 Null 클래스 전달 시 해당 지표의 플롯을 끔.\n
 *
 * 6. 추가한 커스텀 지표를 전략에서 참조하기 위해서는 커스텀 지표 타입의
 *    참조 변수에 저장해야 함.\n
 *    번외로, 가격 참조를 위해 open, high, low, close, volume 지표의 변수가
 *    기본 제공됨.\n
 *
 * 7. 참조 방법은 참조 변수[인덱스]이며, 인덱스 n은 n봉 전 트레이딩 바의 값\n
 *
 * 8. 트레이딩 바 타임프레임보다 지표의 타임프레임이 큰 경우,
 *    지표 바의 Close Time이 트레이딩 바의 Close Time이 동일해진 순간,
 *    다음 Close Time이 동일해지기 전까지 지표의 전 바의 값이 참조됨\n
 *
 * 9. 지표 값은 종가에서 완성되는데 AFTER 전략에서는 완성되지 않은
 *    현재 바의 중간 값을 참조하므로,
 *    AFTER 전략에서는 [0]으로 현재의 값을 참조할 수 없음 ([1] 이상 가능)
 *
 * 10. 부가 기능으로, 진입 잔량을 전량 청산하고 싶으면 left_size 변수를
 *     청산 수량에 사용하면 됨.\n
 */
class BACKTESTING_API Strategy {
  // config.json 저장 시 OHLCV 지표 이름 접근용
  friend class Analyzer;

  // ResetStrategy 접근용
  friend class Backtesting;

 public:
  // 전략을 팩토리로 우회하여 생성하고 strategy_에 추가하고 반환하는 함수
  template <typename CustomStrategy, typename... Args>
  static void AddStrategy(const string& name, Args&&... args) {
    if (strategy_ != nullptr) {
      Logger::LogAndThrowError(
          "한 백테스팅은 한 개의 전략만 사용할 수 있습니다.", __FILE__,
          __LINE__);
    }

    used_creation_function_ = true;

    try {
      strategy_ =
          std::make_shared<CustomStrategy>(name, std::forward<Args>(args)...);
    } catch (const std::exception& e) {
      // 지표 관련 오류면 이미 로깅 됐으므로 간단하게,
      // 전략 생성자의 다른 오류면 상세하게
      if (const string& error_msg = e.what();
          error_msg.find("지표 생성자에서 오류가 발생했습니다.") !=
          string::npos) {
        Logger::LogAndThrowError(
            format("[{}] 전략 생성자에서 오류가 발생했습니다.", name), __FILE__,
            __LINE__);
      } else {
        Logger::LogAndThrowError(
            format("[{}] 전략 생성자에서 오류가 발생했습니다.: {}", name,
                   error_msg),
            __FILE__, __LINE__);
      }
    } catch (...) {
      Logger::LogAndThrowError(
          format("[{}] 전략 생성자에서 알 수 없는 오류가 발생했습니다.", name),
          __FILE__, __LINE__);
    }

    // 전략의 파일 경로 자동 설정
    strategy_->AutoDetectSourcePaths<CustomStrategy>();

    logger->Log(INFO_L, format("[{}] 전략이 엔진에 추가되었습니다.", name),
                __FILE__, __LINE__, true);
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

  /// 엔진에 추가된 전략을 반환하는 함수
  [[nodiscard]] static shared_ptr<Strategy>& GetStrategy();

  /// 전략에서 사용하는 지표들을 반환하는 함수
  [[nodiscard]] vector<shared_ptr<Indicator>>& GetIndicators();

  /// 전략의 이름을 반환하는 함수
  [[nodiscard]] string GetStrategyName() const;

  /// 전략의 클래스 이름을 반환하는 함수
  [[nodiscard]] string GetStrategyClassName() const;

  /// 전략의 주문 핸들러를 반환하는 함수
  [[nodiscard]] shared_ptr<OrderHandler> GetOrderHandler() const;

  /// 전략의 소스 파일 경로를 반환하는 함수
  string GetSourcePath();

  /// 전략의 헤더 파일 경로를 반환하는 함수
  string GetHeaderPath();

 private:  // indicators_를 먼저 초기화 시키기 위하여 protected보다 위에 위치
  static shared_ptr<Strategy> strategy_;      // 엔진에 추가된 전략
  vector<shared_ptr<Indicator>> indicators_;  // 해당 전략에서 사용하는 지표들

  string name_;         /// 전략의 이름
  string class_name_;   /// 전략의 클래스 이름
  string header_path_;  /// 커스텀 전략의 헤더 파일 경로
                        /// → 백테스팅 종료 후 소스 코드 저장 목적
  string source_path_;  /// 커스텀 전략의 소스 파일 경로
                        /// → 백테스팅 종료 후 소스 코드 저장 목적

  // 전략을 추가하기 위해 AddStrategy 함수를 거쳤는지 검증하기 위한 플래그
  static bool used_creation_function_;

  /// 전략 소스 코드 경로 자동 감지 함수
  template <typename CustomStrategy>
  void AutoDetectSourcePaths() {
    // 프로젝트 폴더 가져오기
    const string& project_directory = Config::GetProjectDirectory();

    // 프로젝트 폴더가 설정되지 않았는지 확인
    if (project_directory.empty()) {
      Logger::LogAndThrowError(
          format("[{}] 전략의 소스 파일 경로를 자동 감지하기 위해서는 "
                 "먼저 엔진 설정에서 프로젝트 폴더를 지정해야 합니다.",
                 name_),
          __FILE__, __LINE__);
    }

    // typeid에서 클래스 이름 추출
    const string& type_name = typeid(CustomStrategy).name();
    string class_name = ExtractClassName(type_name);

    // 클래스 이름 저장
    class_name_ = class_name;

    // 헤더 파일 경로 후보들
    vector<string> header_paths;

    bool appointed_header_path = false;
    const auto& strategy_header_path = Config::GetStrategyHeaderPath();

    // 설정된 전략 헤더 파일 경로가 있으면 그것만 사용
    if (!strategy_header_path.empty()) {
      appointed_header_path = true;

      header_paths.push_back(strategy_header_path);
    } else if (const auto& configured_dirs = Config::GetStrategyHeaderDirs();
               !configured_dirs.empty()) {
      // 설정된 헤더 폴더가 있다면 해당 폴더들의 하위 경로들을 후보 경로로 설정
      for (const auto& configured_dir : configured_dirs) {
        try {
          if (fs::exists(configured_dir)) {
            for (const auto& entry :
                 fs::recursive_directory_iterator(configured_dir)) {
              if (entry.is_regular_file()) {
                if (const auto& path = entry.path();
                    path.extension() == ".hpp") {
                  if (const auto& stem = path.stem().string();
                      stem == class_name || stem == name_) {
                    header_paths.push_back(path.string());
                  }
                }
              }
            }
          }
        } catch (...) {
          // 접근 권한이 없는 폴더 등은 무시
        }
      }
    } else {
      // 설정된 헤더 경로 및 폴더가 없다면 기본 헤더 경로 사용
      header_paths = {
          format("{}/Includes/Strategies/{}.hpp", project_directory,
                 class_name),
          format("{}/Includes/Strategies/{}.hpp", project_directory, name_)};
    }

    // 후보 경로에서 헤더 파일 탐색
    bool header_found = false;
    for (const auto& header_path : header_paths) {
      if (fs::exists(header_path)) {
        header_found = true;
        header_path_ = header_path;

        break;
      }
    }

    if (!header_found) {
      logger->Log(
          ERROR_L,
          format("[{}] 전략의 헤더 파일 경로 감지가 실패했습니다.", name_),
          __FILE__, __LINE__, true);

      string target_path;
      if (appointed_header_path) {
        target_path = strategy_header_path;
      } else {
        target_path = format("{}/Includes/Strategies/{}.hpp", project_directory,
                             class_name_);
      }

      Logger::LogAndThrowError(
          format("전략의 클래스명과 헤더 파일명은 동일해야 하며, "
                 "[{}] 경로에 존재해야 합니다.",
                 target_path),
          __FILE__, __LINE__);
    }

    // 소스 파일 경로 후보들
    vector<string> source_paths;

    bool appointed_source_path = false;
    const auto& strategy_source_path = Config::GetStrategySourcePath();

    // 설정된 전략 소스 파일 경로가 있으면 그것만 사용
    if (!strategy_source_path.empty()) {
      appointed_source_path = true;

      source_paths.push_back(strategy_source_path);
    } else if (const auto& configured_dirs = Config::GetStrategySourceDirs();
               !configured_dirs.empty()) {
      // 설정된 소스 폴더가 있다면 해당 폴더들의 하위 경로들을 후보 경로로 설정
      for (const auto& configured_dir : configured_dirs) {
        try {
          if (fs::exists(configured_dir)) {
            for (const auto& entry :
                 fs::recursive_directory_iterator(configured_dir)) {
              if (entry.is_regular_file()) {
                if (const auto& path = entry.path();
                    path.extension() == ".cpp") {
                  if (const auto& stem = path.stem().string();
                      stem == class_name || stem == name_) {
                    source_paths.push_back(path.string());
                  }
                }
              }
            }
          }
        } catch (...) {
          // 접근 권한이 없는 폴더 등은 무시
        }
      }
    } else {
      // 설정된 소스 경로 및 폴더가 없다면 기본 소스 경로 사용
      source_paths = {format("{}/Sources/Cores/Strategies/{}.cpp",
                             project_directory, class_name),
                      format("{}/Sources/Cores/Strategies/{}.cpp",
                             project_directory, name_)};
    }

    // 후보 경로에서 소스 파일 탐색
    bool source_found = false;
    for (const auto& source_path : source_paths) {
      if (fs::exists(source_path)) {
        source_found = true;
        source_path_ = source_path;

        break;
      }
    }

    if (!source_found) {
      logger->Log(
          ERROR_L,
          format("[{}] 전략의 소스 파일 경로 감지가 실패했습니다.", name_),
          __FILE__, __LINE__, true);

      string target_path;
      if (appointed_source_path) {
        target_path = strategy_source_path;
      } else {
        target_path = format("{}/Sources/Cores/Strategies/{}.cpp",
                             project_directory, class_name_);
      }

      Logger::LogAndThrowError(
          format("전략의 클래스명과 소스 파일명은 동일해야 하며, "
                 "[{}] 경로에 존재해야 합니다.",
                 target_path, class_name_),
          __FILE__, __LINE__);
    }
  }

 protected:
  /// 전략 생성자
  ///
  /// @param name 전략의 이름
  explicit Strategy(const string& name);
  virtual ~Strategy();

  // ※ AddIndicator 함수의 존재 의의는,
  //    전략에서 사용하는 지표들을 구별해야 하기 위함

  /// 전략에 지표를 추가하는 함수
  ///
  ///
  /// @param name 커스텀 전략에 추가할 템플릿 지표의 이름
  /// @param timeframe 커스텀 전략에 추가할 템플릿 지표의 타임프레임
  /// @param plot 플롯 정보를 담은 Area | Baseline | Histogram | Line
  ///             클래스 객체. Null로 설정되면 플롯하지 않음.
  /// @param args 템플릿 지표의 추가적인 매개변수
  template <typename CustomIndicator, typename... Args>
  CustomIndicator& AddIndicator(const string& name, const string& timeframe,
                                const Plot& plot = Null(), Args&&... args) {
    // AddIndicator 함수를 통할 때만 생성 카운터 증가
    Indicator::IncreaseCreationCounter();

    shared_ptr<CustomIndicator> indicator;
    try {
      indicator = std::make_shared<CustomIndicator>(
          name, timeframe, plot, std::forward<Args>(args)...);
    } catch (const std::exception& e) {
      Logger::LogAndThrowError(
          format("[{}] 지표 생성자에서 오류가 발생했습니다.: {}", name,
                 e.what()),
          __FILE__, __LINE__);
    } catch (...) {
      Logger::LogAndThrowError(
          format("[{}] 지표 생성자에서 알 수 없는 오류가 발생했습니다.", name),
          __FILE__, __LINE__);
    }

    // 지표의 파일 경로 자동 설정
    indicator->template AutoDetectSourcePaths<CustomIndicator>();

    // 같은 클래스 이름의 지표가 저장되지 않았을 경우에만 저장된 지표에 추가
    if (const string& class_name = indicator->GetIndicatorClassName();
        !Indicator::IsIndicatorClassSaved(class_name)) {
      Indicator::AddSavedIndicatorClass(class_name);
    }

    indicators_.push_back(indicator);

    // 지표 추가 로그는, TRADING_TIMEFRAME을 사용할 수도 있으므로
    // 발생시키지 않음

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

  /* 전략 작성 편의성용 가격 데이터 지표화.
     가격 데이터는 플롯 설정과 관련없이 하나의 캔들로 플롯됨 */
  // ReSharper disable once CppInconsistentNaming
  Open& open;  // 트레이딩 바 데이터의 시가 데이터
  // ReSharper disable once CppInconsistentNaming
  High& high;  // 트레이딩 바 데이터의 고가 데이터
  // ReSharper disable once CppInconsistentNaming
  Low& low;  // 트레이딩 바 데이터의 저가 데이터
  // ReSharper disable once CppInconsistentNaming
  Close& close;  // 트레이딩 바 데이터의 종가 데이터
  // ReSharper disable once CppInconsistentNaming
  Volume& volume;  // 트레이딩 바 데이터의 거래량 데이터

  // ReSharper disable once CppInconsistentNaming
  /// 커스텀 전략에서 청산 시 진입 잔량의 전량 청산을 위해 사용하는 변수.
  ///
  /// 엔진 내부적으로 청산 수량은 진입 잔량의 최대값으로 변환되기 때문에
  /// double 최대값으로 사용
  const double left_size = DBL_MAX;

 private:
  /// Strategy를 초기화하는 함수
  static void ResetStrategy();
};

}  // namespace backtesting::strategy
using namespace strategy;

// 이 헤더를 include한 후 선언되는 커스텀 전략 클래스는 일반 클래스로 정의되도록
// 매크로를 빈 값으로 재정의
// (베이스 클래스인 Strategy는 이미 dllimport로 정의되었으므로 영향 없음)
#if defined(STRATEGY_BUILD) && !defined(BACKTESTING_EXPORTS)
#undef BACKTESTING_API
#define BACKTESTING_API
#endif
