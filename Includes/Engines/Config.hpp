#pragma once

// 표준 라이브러리
#include <memory>
#include <optional>
#include <string>

// 내부 헤더
#include "Engines/BaseEngine.hpp"

// 전방 선언
namespace backtesting {
namespace bar {
enum class BarType;
}

namespace logger {
class Logger;
}
}  // namespace backtesting

// 네임 스페이스
using namespace std;
namespace backtesting {
using namespace engine;
using namespace logger;
}  // namespace backtesting

namespace backtesting::engine {

/// 백테스팅 기간을 지정하는 구조체.\n
/// Start와 End 시간을 지정하지 않으면 캔들 범위 전체로 백테스팅 진행
struct Period {
  Period() = default;
  explicit Period(const string& start_time, const string& end_time,
                  const string& format) {
    this->start_time = start_time;
    this->end_time = end_time;
    this->format = format;
  }

  [[nodiscard]] string GetStartTime() const { return start_time; }
  [[nodiscard]] string GetEndTime() const { return end_time; }
  [[nodiscard]] string GetFormat() const { return format; }

 private:
  string start_time;  // 백테스팅 시작 시간
  string end_time;    // 백테스팅 종료 시간
  string format;      // 시간 문자열 포맷
};

/// 엔진의 사전 설정값을 담당하는 빌더 클래스
class Config final {
 public:
  Config();
  ~Config();

  /// 엔진에 설정값을 추가하는 함수.
  ///
  /// 설정값 추가는 항상 이 함수를 통해야 함.
  static Config& SetConfig() {
    // SetConfig 함수를 통할 때만 생성 카운터 증가
    creation_counter_++;

    if (BaseEngine::config_ == nullptr) {
      const auto& config = std::make_shared<Config>();
      BaseEngine::config_ = config;
    }

    return *BaseEngine::config_;
  }

  // 루트 폴더를 설정하는 함수
  Config& SetRootDirectory(const string& root_directory);

  /// 백테스팅 기간을 설정하는 함수.\n
  /// Start와 End 시간을 지정하지 않으면 캔들 범위 전체로 백테스팅을 진행
  /// @param start_time 트레이딩 바 데이터의 타임프레임을 기준으로,
  ///                   지정된 Start Time 이후의 Open Time부터 백테스팅
  /// @param end_time 트레이딩 바 데이터의 타임프레임을 기준으로,
  ///                 지정된 End Time 이전의 Close Time까지 백테스팅
  /// @param format Start Time과 End Time의 시간 포맷
  Config& SetBacktestPeriod(const string& start_time = "",
                            const string& end_time = "",
                            const string& format = "%Y-%m-%d %H:%M:%S");

  // 바 돋보기 기능을 사용할지 여부를 설정하는 함수
  Config& SetUseBarMagnifier(bool use_bar_magnifier);

  // 초기 자금을 설정하는 함수
  Config& SetInitialBalance(double initial_balance);

  // 테이커(시장가) 수수료율을 설정하는 함수
  // (퍼센트로 지정: 0.05% -> O: 0.05 X: 0.0005)
  Config& SetTakerFeePercentage(double taker_fee_percentage);

  // 메이커(지정가) 수수료율을 설정하는 함수
  // (퍼센트로 지정: 0.05% -> O: 0.05 X: 0.0005)
  Config& SetMakerFeePercentage(double maker_fee_percentage);

  // 테이커(시장가) 슬리피지율을 설정하는 함수
  // (퍼센트로 지정: 0.05% -> O: 0.05 X: 0.0005)
  Config& SetTakerSlippagePercentage(double taker_slippage_percentage);

  // 메이커(지정가) 수수료율을 설정하는 함수
  // (퍼센트로 지정: 0.05% -> O: 0.05 X: 0.0005)
  Config& SetMakerSlippagePercentage(double maker_slippage_percentage);

  // 심볼 간 바 데이터 중복 검사를 비활성화하는 함수
  Config& DisableSameBarDataCheck(BarType bar_type);

  // 마크 가격 바 데이터와 목표 바 데이터의 중복 검사를 비활성화하는 함수
  Config& DisableSameBarDataWithTargetCheck();

  [[nodiscard]] static string GetRootDirectory();
  [[nodiscard]] optional<Period> GetBacktestPeriod() const;
  [[nodiscard]] optional<bool> GetUseBarMagnifier() const;
  [[nodiscard]] double GetInitialBalance() const;
  [[nodiscard]] double GetTakerFeePercentage() const;
  [[nodiscard]] double GetMakerFeePercentage() const;
  [[nodiscard]] double GetTakerSlippagePercentage() const;
  [[nodiscard]] double GetMakerSlippagePercentage() const;
  [[nodiscard]] vector<bool> GetCheckSameBarData() const;
  [[nodiscard]] bool GetCheckSameBarDataWithTarget() const;

 private:
  static shared_ptr<Logger>& logger_;

  // 설정값 생성 시 SetConfig 함수 사용을 강제하기 위한 목적
  // 생성 카운터
  static size_t creation_counter_;
  // 전 생성 카운터
  static size_t pre_creation_counter_;

  /// 루트 폴더
  static string root_directory_;

  /// 백테스팅 기간
  optional<Period> backtest_period_;

  /// 바 돋보기 사용 여부
  optional<bool> use_bar_magnifier_;

  /// 초기 자금
  double initial_balance_;

  /// 테이커(시장가) 수수료율
  ///
  /// 백분율로 지정 시 100 곱한 값 (5%면 5로 지정)
  double taker_fee_percentage_;

  /// 메이커(지정가) 수수료율
  ///
  /// 백분율로 지정 시 100 곱한 값 (5%면 5로 지정)
  double maker_fee_percentage_;

  /// 시장가 슬리피지율
  ///
  /// 백분율로 지정 시 100 곱한 값 (5%면 5로 지정)
  double taker_slippage_percentage_;

  /// 지정가 슬리피지율
  ///
  /// 백분율로 지정 시 100 곱한 값 (5%면 5로 지정)
  double maker_slippage_percentage_;

  /// 심볼 간 중복된 바 데이터 검사를 하는지 여부를 결정하는 플래그.
  ///
  /// 바 타입마다 분리하여 작동.
  vector<bool> check_same_bar_data_;

  /// 마크 가격 바 데이터에서 목표 바 데이터와의 중복된 바 데이터 검사를 하는지
  /// 여부를 결정하는 플래그.
  bool check_same_bar_data_with_target_;
};

}  // namespace backtesting::engine
