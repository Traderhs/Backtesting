#pragma once

// 표준 라이브러리
#include <string>
#include <utility>

// 네임 스페이스
using namespace std;

/// 수수료 타입을 지정하는 열거형 클래스
enum class CommissionType {
  COMMISSION_NONE,
  COMMISSION_PERCENTAGE,
};

/// 슬리피지 타입을 지정하는 열거형 클래스
enum class SlippageType {
  SLIPPAGE_NONE,
  SLIPPAGE_PERCENTAGE,
};

/// 엔진의 사전 설정값을 담당하는 빌더 클래스
class Config final {
 public:
  Config();
  ~Config();

  Config& SetRootDirectory(const string& root_directory);
  Config& SetInitialBalance(double initial_balance);
  Config& SetCommissionType(CommissionType commission_type);
  Config& SetMarketCommission(double market_commission);
  Config& SetLimitCommission(double limit_commission);
  Config& SetSlippageType(SlippageType slippage_type);
  Config& SetMarketSlippage(double market_slippage);
  Config& SetLimitSlippage(double limit_slippage);

  [[nodiscard]] string GetRootDirectory() const;
  [[nodiscard]] double GetInitialBalance() const;
  [[nodiscard]] CommissionType GetCommissionType() const;
  [[nodiscard]] double GetMarketCommission() const;
  [[nodiscard]] double GetLimitCommission() const;
  [[nodiscard]] SlippageType GetSlippageType() const;
  [[nodiscard]] double GetMarketSlippage() const;
  [[nodiscard]] double GetLimitSlippage() const;

 private:
  /// 루트 폴더
  string root_directory_;

  /// 초기 자금
  double initial_balance_;

  /// 수수료 타입
  CommissionType commission_type_;

  /// 시장가 수수료: 퍼센트로 지정 시 100 곱한 값 (5% -> 5)
  double market_commission_;

  /// 지정가 수수료: 퍼센트로 지정 시 100 곱한 값 (5% -> 5)
  double limit_commission_;

  /// 슬리피지 타입
  SlippageType slippage_type_;

  /// 시장가 슬리피지: 퍼센트로 지정 시 100 곱한 값 (5% -> 5)
  double market_slippage_;

  /// 지정가 슬리피지: 퍼센트로 지정 시 100 곱한 값 (5% -> 5)
  double limit_slippage_;
};
