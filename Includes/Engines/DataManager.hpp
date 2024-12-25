#pragma once

// 표준 라이브러리
#include <mutex>

// 네임 스페이스
using namespace std;

/**
 * 수수료, 슬리피지 등 백테스팅 데이터를 관리하는 클래스
 */
class DataManager final {
 public:
  // 수수료 타입을 지정하는 열거형 클래스
  enum class CommissionType { COMMISSION_PERCENTAGE, COMMISSION_POINT };

  // 슬리피지 타입을 지정하는 열거형 클래스
  enum class SlippageType { SLIPPAGE_PERCENTAGE, SLIPPAGE_POINT };

  // DataManager의 싱글톤 인스턴스를 반환하는 함수
  static DataManager& GetDataManager();

  // 초기 자금을 설정하는 함수
  void SetInitialCapital(double initial_capital);

  // 수수료와 수수료 타입을 설정하는 함수
  void SetCommissionAndType(double commission, CommissionType commission_type);

  // 슬리피지와 슬리피지 타입을 설정하는 함수
  void SetSlippageAndType(double slippage, SlippageType slippage_type);

  // 초기 자금을 반환하는 함수
  [[nodiscard]] double GetInitialCapital() const;

  // 초기 자금을 반환하는 함수
  [[nodiscard]] double GetCommission() const;

  // 초기 자금을 반환하는 함수
  [[nodiscard]] double GetSlippage() const;

 private:
  // 싱글톤 인스턴스 관리
  static mutex mutex;
  static unique_ptr<DataManager> instance;

  // 자금 관련 항목
  double initial_capital;          // 초기 자금
  double commission;               // 수수료
  CommissionType commission_type;  // 수수료 타입: Percentage or point
  double slippage;                 // 슬리피지
  SlippageType slippage_type;      // 슬리피지 타입: Percentage or Point
  double current_capital;          // 현재 자금
  double max_capital;              // 최고 자금
  double drawdown;                 // 현재 드로우다운
  double maximum_drawdown;         // 최고 드로우다운

   DataManager();
   ~DataManager();
};