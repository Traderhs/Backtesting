#pragma once

// 표준 라이브러리
#include <format>  // format 추가
#include <memory>
#include <vector>

// 내부 헤더
#include "Engines/Analyzer.hpp"
#include "Engines/BarData.hpp"  // 커스텀 지표에서 사용 편의성을 위해 직접 포함
#include "Engines/BarHandler.hpp"  // 커스텀 지표에서 사용 편의성을 위해 직접 포함
#include "Engines/Config.hpp"
#include "Engines/DataUtils.hpp"
#include "Engines/Engine.hpp"
#include "Engines/Logger.hpp"
#include "Engines/Numeric.hpp"
#include "Engines/Plot.hpp"

// 전방 선언
namespace backtesting::analyzer {
class Analyzer;
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
using namespace utils;
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
 *    Calculate → 각 바마다 값을 계산하여 반환\n
 *
 * 2. 상속받은 지표 생성자에는 [지표 이름, 타임프레임, Plot 객체]를
 *    동일한 순서로 반드시 포함해야 함\n
 *
 * 3. 커스텀 지표 내에서 다른 지표를 사용하기 위해서는, 생성자에서
 *    Indicator& 타입의 인수를 받아 [] 연산자로 참조하여 사용하면 됨.\n
 *    ※ 주의: 인수로 넣을 다른 지표는 커스텀 지표보다 먼저 정의되어야 함.
 *
 * 4. 커스텀 지표를 계산할 때, 커스텀 지표의 타임프레임과 다른 타임프레임의
 *    지표는 사용 불가능\n
 *
 * 5. 헤더 파일 및 소스 파일은 자동으로 탐색하여 저장. 파일명과 클래스명이
 *    동일하고, 지정된 경로에 존재할 때만 소스 파일 탐지.
 *    (루트 폴더/Includes/Indicators/클래스명.hpp 그리고
 *     루트 폴더/Sources/cpp/Indicators/클래스명.cpp)\n
 */
class Indicator {
  // 생성자 및 IncreaseCreationCounter 함수 접근용
  friend class Strategy;

  // Plot 유효성 검사 시 plot_ 접근용
  friend class Engine;

  // 지표 및 설정 저장 시 output_ 및 plot_ 접근용
  friend class Analyzer;

 public:
  // 지표 반환 시 참조 타입으로 받는 것을 강요하기 위하여
  // 복사 생성자, 할당 연산자 삭제
  Indicator(const Indicator&) = delete;
  Indicator& operator=(const Indicator&) = delete;

  /// 지표의 계산된 값을 반환하는 연산자 오버로딩.\n\n
  /// 사용법: 지표 클래스 객체[n개 바 전 인덱스]
  [[nodiscard]] Numeric<double> operator[](size_t index);

  /// 모든 심볼의 모든 바에 해당되는 지표 값을 계산하는 함수
  void CalculateIndicator();

  /// 지표의 타임프레임을 설정하는 함수
  void SetTimeframe(const string& timeframe);

  /// 트레이딩 바 데이터의 타임프레임보다 큰 타임프레임의 지표인지 설정하는 함수
  /// 지표 참조 시 빠른 방법 판단을 위하여 설정
  void SetHigherTimeframeIndicator();

  /// 해당 지표의 소스 파일 경로를 설정하는 함수
  void SetSourcePath(const string& source_path);

  /// 해당 지표의 헤더 파일 경로를 설정하는 함수
  void SetHeaderPath(const string& header_path);

  /// 해당 지표의 이름을 반환하는 함수
  [[nodiscard]] string GetName() const;

  /// 해당 지표의 클래스 이름을 반환하는 함수
  [[nodiscard]] string GetClassName() const;

  /// 해당 지표의 타임프레임을 반환하는 함수
  [[nodiscard]] string GetTimeframe() const;

  /// 해당 지표의 소스 파일 경로를 반환하는 함수
  string GetSourcePath();

  /// 해당 지표의 헤더 파일 경로를 반환하는 함수
  string GetHeaderPath();

  /// 지표 클래스 이름이 저장된 목록에 이미 있는지 확인하는 함수
  static bool IsIndicatorClassSaved(const string& class_name);

  /// 지표 클래스 이름을 저장된 목록에 추가하는 함수
  static void AddSavedIndicatorClass(const string& class_name);

  /// 지표 소스 코드 경로 자동 감지 함수
  template <typename CustomIndicator>
  void AutoDetectSourcePaths() {
    // 루트 폴더 가져오기
    const string& root_dir = Config::GetRootDirectory();

    // 루트 폴더가 설정되지 않았는지 확인
    if (root_dir.empty()) {
      Logger::LogAndThrowError(
          format("[{}] 지표의 소스 파일 경로를 자동 감지하기 위해서는 "
                 "먼저 엔진 설정에서 루트 폴더를 지정해야 합니다.",
                 name_),
          __FILE__, __LINE__);
    }

    // typeid에서 클래스 이름 추출
    const string& type_name = typeid(CustomIndicator).name();
    string class_name = ExtractClassName(type_name);

    // 클래스 이름 저장
    class_name_ = class_name;

    // 소스 파일 경로 후보들
    const vector<string>& source_candidates = {
        format("{}/Sources/cpp/Indicators/{}.cpp", root_dir, class_name),
        format("{}/Sources/cpp/Indicators/{}.cpp", root_dir, name_)};

    // 헤더 파일 경로 후보들
    const vector<string>& header_candidates = {
        format("{}/Includes/Indicators/{}.hpp", root_dir, class_name),
        format("{}/Includes/Indicators/{}.hpp", root_dir, name_)};

    // 소스 파일 찾기
    for (const auto& path : source_candidates) {
      if (SetFilePath(path, true)) {
        break;
      }
    }

    // 헤더 파일 찾기
    for (const auto& path : header_candidates) {
      if (SetFilePath(path, false)) {
        break;
      }
    }
  }

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

  /// 저장된 지표들의 클래스 이름 목록 (중복 저장 방지용)
  static vector<string> saved_indicator_classes_;

  string name_;                             // 지표의 이름
  string timeframe_;                        // 지표의 타임프레임
  string class_name_;                       // 지표의 클래스 이름
  vector<double> input_;                    // 지표의 파라미터
  vector<vector<Numeric<double>>> output_;  // 지표의 계산된 값: 심볼<값>
  bool is_calculated_;                 // 지표가 계산되었는지 확인하는 플래그
  vector<size_t> reference_num_bars_;  /// 지표의 타임프레임에 해당되는
                                       /// 참조 바 데이터의 심볼별 바 개수

  string cpp_file_path_;     /// 커스텀 지표의 소스 파일 경로
                             /// → 백테스팅 종료 후 소스 코드 저장 목적
  string header_file_path_;  /// 커스텀 지표의 헤더 파일 경로
                             /// → 백테스팅 종료 후 소스 코드 저장 목적

  // 지표가 현재 계산 중인지 확인하는 플래그.
  // 지표 계산 시 사용하는 다른 지표가 계산하는 지표와 다른 타임프레임을 가질 수
  // 없게 검사할 때 사용
  static bool is_calculating_;           // 현재 지표 계산 중인지 여부
  static string calculating_name_;       // 계산 중인 지표의 이름
  static string calculating_timeframe_;  // 계산 중인 지표의 타임프레임
  bool
      is_higher_timeframe_indicator_;  /// 트레이딩 바의 타임프레임보다 큰
                                       /// 타임프레임의 지표인지 확인하는 플래그

  // 성능 최적화를 위한 캐시 변수들
  mutable shared_ptr<BarData> trading_bar_data_;  // 트레이딩 바 데이터
  mutable shared_ptr<BarData>
      reference_bar_data_;              // 현재 지표 타임프레임의 참조 바 데이터
  mutable size_t cached_symbol_idx_{};  // 캐시된 심볼 인덱스
  mutable size_t cached_trading_bar_idx_{};  // 캐시된 트레이딩 바 인덱스
  mutable size_t cached_target_bar_idx_{};   // 캐시된 대상 바 인덱스
  mutable size_t cached_ref_bar_idx_{};      // 캐시된 참조 바 인덱스

  // 플롯 정보
  string plot_type_;       // 플롯 클래스명
  shared_ptr<Plot> plot_;  // 플롯 정보

  /// 지표 생성 카운터를 증가시키는 함수
  static void IncreaseCreationCounter();

  /// 파일이 존재하는지 확인하고 존재하면 경로로 설정하는 함수
  bool SetFilePath(const string& path, bool is_cpp);
};

}  // namespace backtesting::indicator
using namespace backtesting::indicator;
