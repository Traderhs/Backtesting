#pragma once

// 표준 라이브러리
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

  Config& SetInitialBalance(double initial_balance);
  Config& SetCommissionType(CommissionType commission_type);
  Config& SetCommission(const pair<double, double>& commission);
  Config& SetSlippageType(SlippageType slippage_type);
  Config& SetSlippage(const pair<double, double>& slippage);

  [[nodiscard]] double GetInitialBalance() const;
  [[nodiscard]] CommissionType GetCommissionType() const;
  [[nodiscard]] pair<double, double> GetCommission() const;
  [[nodiscard]] SlippageType GetSlippageType() const;
  [[nodiscard]] pair<double, double> GetSlippage() const;

 private:
  /// 초기 자금
  double initial_balance_;

  /// 수수료 타입
  CommissionType commission_type_;

  /// <시장가 수수료, 지정가 수수료>: 퍼센트로 지정 시 100 곱한 값 (5% -> 5)
  pair<double, double> commission_;

  /// 슬리피지 타입
  SlippageType slippage_type_;

  /// <시장가 슬리피지, 지정가 슬리피지>: 퍼센트로 지정 시 100 곱한 값 (5% -> 5)
  pair<double, double> slippage_;
};
