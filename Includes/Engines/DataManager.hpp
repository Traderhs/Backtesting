#pragma once

// 표준 라이브러리
#include <mutex>
#include <unordered_map>

// 내부 헤더
#include "Logger.hpp"
#include "OrderManager.hpp"

// 네임 스페이스
using namespace std;

/**
 * 수수료, 슬리피지 등 백테스팅 데이터를 관리하는 클래스
 */
class DataManager final {
 public:
  /// 수수료 타입을 지정하는 열거형 클래스
  enum class CommissionType { COMMISSION_PERCENTAGE, COMMISSION_POINT };

  /// 슬리피지 타입을 지정하는 열거형 클래스
  enum class SlippageType { SLIPPAGE_PERCENTAGE, SLIPPAGE_POINT };

  /// 진입 시 손익을 반영하기 위한 자금 업데이트 여부의 플래그
  bool capital_updated_current_bar;

  /// DataManager의 싱글톤 인스턴스를 반환하는 함수
  static DataManager& GetDataManager();

  /// 초기 자금을 설정하는 함수
  void SetInitialCapital(double initial_capital);

  /// 수수료와 수수료 타입을 설정하는 함수.
  /// 퍼센트로 지정 (5% => 5)
  void SetCommissionAndType(double market_commission, double limit_commission,
                            CommissionType commission_type);

  /// 슬리피지와 슬리피지 타입을 설정하는 함수.
  /// 퍼센트로 지정 (5% => 5)
  void SetSlippageAndType(double market_slippage, double limit_slippage,
                          SlippageType slippage_type);

  /// 현재 자금을 설정하는 함수
  void SetCurrentCapital(double current_capital);

  /// 주문 가능 자금을 설정하는 함수
  void SetAvailableCapital(double available_capital);

  /// 초기 자금을 반환하는 함수
  [[nodiscard]] double GetInitialCapital() const;

  /// 수수료 설정 값을 반환하는 함수: <시장가, 지정가>
  [[nodiscard]] pair<double, double> GetCommission() const;

  /// 수수료 타입을 반환하는 함수
  [[nodiscard]] CommissionType GetCommissionType() const;

  /// 슬리피지 설정 값을 반환하는 함수: <시장가, 지정가>
  [[nodiscard]] pair<double, double> GetSlippage() const;

  /// 슬리피지 타입을 반환하는 함수
  [[nodiscard]] SlippageType GetSlippageType() const;

  /// 현재 자금을 반환하는 함수
  [[nodiscard]] double GetCurrentCapital() const;

  /// 주문 가능 자금을 반환하는 함수
  [[nodiscard]] double GetAvailableCapital() const;

  /// 설정된 최소 틱 단위를 반환하는 함수
  [[nodiscard]] double GetTickSize(const string& symbol) const;

 private:
  // 싱글톤 인스턴스 관리
  static mutex mutex;
  static unique_ptr<DataManager> instance;

  static Logger& logger;

  // 자금 관련 사전 설정 항목
  double initial_capital;          // 초기 자금
  double market_commission;        // 시장가 수수료
  double limit_commission;         // 지정가 수수료
  CommissionType commission_type;  // 수수료 타입: Percentage or point
  double market_slippage;          // 시장가 슬리피지
  double limit_slippage;           // 지정가 슬리피지
  SlippageType slippage_type;      // 슬리피지 타입: Percentage or Point

  // 자금 관련 중도 설정 항목
  double current_capital;          // 현재 자금
  double available_capital;        // 주문 가능 자금
  double max_capital;              // 최고 자금
  double drawdown;                 // 현재 드로우다운
  double max_drawdown;             // 최고 드로우다운
  int margin_call_number;          // 마진콜 횟수

  // 가격 정보
  unordered_map<string, double> tick_size;     // 최소 틱 단위: <심볼, 틱 단위>

   DataManager();
   ~DataManager();
};