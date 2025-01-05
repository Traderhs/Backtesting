#pragma once

// 내부 헤더
#include "Engines/BaseEngine.hpp"

/// 엔진의 사전 설정값을 담당하는 빌더 클래스
class Config final {
 public:
  Config();
  ~Config();

  Config& SetInitialBalance(double initial_balance);
  Config& SetCommissionType(BaseEngine::CommissionType commission_type);
  Config& SetCommission(const pair<double, double>& commission);
  Config& SetSlippageType(BaseEngine::SlippageType slippage_type);
  Config& SetSlippage(const pair<double, double>& slippage);

  Config& Build();

  [[nodiscard]] double GetInitialBalance() const;
  [[nodiscard]] BaseEngine::CommissionType GetCommissionType() const;
  [[nodiscard]] pair<double, double> GetCommission() const;
  [[nodiscard]] BaseEngine::SlippageType GetSlippageType() const;
  [[nodiscard]] pair<double, double> GetSlippage() const;

 private:
  /// 초기 자금
  double initial_balance_;

  /// 수수료 타입
  BaseEngine::CommissionType commission_type_;

  /// <시장가 수수료, 지정가 수수료>: 퍼센트로 지정 시 100 곱한 값 (5% -> 5)
  pair<double, double> commission_;

  /// 슬리피지 타입
  BaseEngine::SlippageType slippage_type_;

  /// <시장가 슬리피지, 지정가 슬리피지>: 퍼센트로 지정 시 100 곱한 값 (5% -> 5)
  pair<double, double> slippage_;
};
