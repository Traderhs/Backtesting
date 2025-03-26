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

  Config& SetRootDirectory(const string& root_directory);
  Config& SetUseBarMagnifier(bool use_bar_magnifier);
  Config& SetInitialBalance(double initial_balance);
  Config& SetTakerFeePercentage(double taker_fee_percentage);
  Config& SetMakerFeePercentage(double maker_fee_percentage);
  Config& SetTakerSlippagePercentage(double taker_slippage_percentage);
  Config& SetMakerSlippagePercentage(double maker_slippage_percentage);
  Config& DisableSameBarDataCheck(BarType bar_type);
  Config& DisableSameBarDataWithTargetCheck();

  [[nodiscard]] string GetRootDirectory() const;
  [[nodiscard]] bool GetUseBarMagnifier() const;
  [[nodiscard]] double GetInitialBalance() const;
  [[nodiscard]] double GetTakerFeePercentage() const;
  [[nodiscard]] double GetMakerFeePercentage() const;
  [[nodiscard]] double GetTakerSlippagePercentage() const;
  [[nodiscard]] double GetMakerSlippagePercentage() const;
  [[nodiscard]] vector<bool> GetCheckSameBarData() const;
  [[nodiscard]] bool GetCheckSameBarDataWithTarget() const;

  [[nodiscard]] bool UseBarMagnifierHasValue() const;

 private:
  static shared_ptr<Logger>& logger_;

  // 설정값 생성 시 SetConfig 함수 사용을 강제하기 위한 목적
  // 생성 카운터
  static size_t creation_counter_;
  // 전 생성 카운터
  static size_t pre_creation_counter_;

  /// 루트 폴더
  string root_directory_;

  /// 바 돋보기 사용 여부
  optional<bool> use_bar_magnifier_;

  /// 초기 자금
  double initial_balance_;

  /// 테이커(시장가) 수수료 퍼센트
  ///
  /// 백분율로 지정 시 100 곱한 값 (5%면 5로 지정)
  double taker_fee_percentage_;

  /// 메이커(지정가) 수수료 퍼센트
  ///
  /// 백분율로 지정 시 100 곱한 값 (5%면 5로 지정)
  double maker_fee_percentage_;

  /// 시장가 슬리피지 퍼센트
  ///
  /// 백분율로 지정 시 100 곱한 값 (5%면 5로 지정)
  double taker_slippage_percentage_;

  /// 지정가 슬리피지 퍼센트
  ///
  /// 백분율로 지정 시 100 곱한 값 (5%면 5로 지정)
  double maker_slippage_percentage_;

  /// 심볼 간 동일한 바 데이터 검사를 하는지 여부를 결정하는 플래그.
  ///
  /// 바 타입마다 분리하여 작동.
  vector<bool> check_same_bar_data_;

  /// 마크 가격에서 목표 바 데이터와의 동일한 바 데이터 검사를 하는지 여부를
  /// 결정하는 플래그.
  bool check_same_bar_data_with_target_;
};

}  // namespace backtesting::engine
