#pragma once

// 표준 라이브러리
#include <memory>
#include <optional>
#include <string>
#include <vector>

// 전방 선언
namespace backtesting::bar {
class BarData;
class BarHandler;
enum class BarDataType;
struct Bar;
}  // namespace backtesting::bar

namespace backtesting::engine {
class Config;
}

namespace backtesting::order {
class SymbolInfo;
enum class Direction;
enum class OrderType;
}  // namespace backtesting::order

// 네임 스페이스
using namespace std;
namespace backtesting {
using namespace bar;
using namespace engine;
}  // namespace backtesting

namespace backtesting::order {

/// 슬리피지 계산을 담당하는 추상 클래스
class Slippage {
 public:
  virtual ~Slippage() = default;

  // 심볼 정보
  static vector<SymbolInfo> symbol_info_;

  /// 슬리피지 객체를 복제하는 순수 가상 함수
  [[nodiscard]] virtual unique_ptr<Slippage> Clone() const = 0;

  /// 슬리피지 객체를 초기화하는 순수 가상 함수
  virtual void Initialize() = 0;

  /// 슬리피지를 적용한 체결 가격을 계산하는 순수 가상 함수
  ///
  /// @param order_type 주문 타입 (시장가/지정가)
  /// @param direction 진입/청산 방향
  /// @param order_price 원래 주문 가격
  /// @param order_size
  /// @param symbol_idx
  /// @return 슬리피지가 적용된 체결 가격
  [[nodiscard]] virtual double CalculateSlippagePrice(OrderType order_type,
                                                      Direction direction,
                                                      double order_price,
                                                      double order_size,
                                                      int symbol_idx) const = 0;

  /// 테이커 슬리피지가 유효한지 검증하는 순수 가상 함수
  ///
  /// @return 유효하면 nullopt, 유효하지 않으면 에러 메시지
  [[nodiscard]] virtual optional<string> ValidateTakerSlippage() const = 0;

  /// 메이커 슬리피지가 유효한지 검증하는 순수 가상 함수
  ///
  /// @return 유효하면 nullopt, 유효하지 않으면 에러 메시지
  [[nodiscard]] virtual optional<string> ValidateMakerSlippage() const = 0;

  /// 심볼 정보를 초기화하는 함수
  static void SetSymbolInfo(const vector<SymbolInfo>& symbol_info);
};

/// 퍼센트 기반 슬리피지 계산 클래스
class PercentageSlippage final : public Slippage {
 public:
  /// 테이커 및 메이커 슬리피지를 고정 퍼센트로 계산하는 클래스
  ///
  /// 백분율로 지정 시 100 곱한 값으로 지정 (5%면 5로 지정)
  /// @param taker_slippage_percentage 테이커(시장가) 슬리피지 퍼센트 (%)
  /// @param maker_slippage_percentage 메이커(지정가) 슬리피지 퍼센트 (%)
  explicit PercentageSlippage(const double taker_slippage_percentage,
                              const double maker_slippage_percentage)
      : taker_slippage_ratio_(taker_slippage_percentage / 100),
        maker_slippage_ratio_(maker_slippage_percentage / 100) {}

  // 기본 생성자 삭제
  PercentageSlippage() = delete;

  ~PercentageSlippage() override = default;

  [[nodiscard]] unique_ptr<Slippage> Clone() const override {
    return make_unique<PercentageSlippage>(*this);
  }

  [[nodiscard]] optional<string> ValidateTakerSlippage() const override;
  [[nodiscard]] optional<string> ValidateMakerSlippage() const override;

  // 할 작업 없음
  void Initialize() override {}

  [[nodiscard]] double CalculateSlippagePrice(OrderType order_type,
                                              Direction direction,
                                              double order_price,
                                              double order_size,
                                              int symbol_idx) const override;

  [[nodiscard]] __forceinline double GetTakerSlippagePercentage() const {
    return taker_slippage_ratio_ * 100;
  }

  [[nodiscard]] __forceinline double GetMakerSlippagePercentage() const {
    return maker_slippage_ratio_ * 100;
  }

 private:
  /// 테이커(시장가) 슬리피지율
  double taker_slippage_ratio_;

  /// 메이커(지정가) 슬리피지율
  double maker_slippage_ratio_;
};

/// OHLCV 기반 시장 충격 슬리피지 계산 클래스
///
/// 실제 시장 데이터(OHLCV)를 기반으로 슬리피지를 추정
/// 1. Effective Spread (실효 스프레드): OHLC 데이터만으로 호가 스프레드 추정
/// 2. Market Impact (시장 충격): 주문 크기가 거래량 대비 클 때의 가격 영향
/// 3. Volatility (변동성): 가격 변동성이 클수록 슬리피지 증가
///
/// 학술적 근거:
/// - EDGE (Ardia-Guidotti-Kröncke, 2024+):
///    고빈도 데이터에서 강건한 스프레드 추정
/// - Garman-Klass (1980): OHLC 변동성 추정
///
/// 시장 충격 모델: slippage_bps = spread/2 + k*σ*(Q/V)^β
/// - β = 0.5 (제곱근 시장 충격, 학계 표준)
/// - Q = 주문 크기
/// - V = 롤링 윈도우 거래량
/// - σ = OHLC 기반 변동성
/// - k = 시장 충격 계수
///
/// 모든 타임프레임 지원: 1분봉부터 일봉, 주봉까지 동일하게 작동
class MarketImpactSlippage final : public Slippage {
 public:
  /// @param stress_multiplier 스트레스 테스트 용도 슬리피지 틱 계수
  ///
  /// 자동 설정값:
  /// - EDGE 스프레드 추정 (모든 타임프레임에서 강건)
  /// - Garman-Klass 변동성 (효율적이고 정확)
  /// - 시장 충격 계수 0.1 (표준값)
  /// - 롤링 윈도우 10 (범용적)
  /// - 틱 플로어 1 bps (기본 최소값)
  /// - 수수료는 Config 설정 사용 (테이커/메이커 모두 양수)
  /// - PR 캡 0.3 (극저유동성 폭주 방지)
  /// - 스프레드 EMA 스무딩 알파 0.3 (15분 이하 고빈도 데이터만)
  explicit MarketImpactSlippage(const double stress_multiplier)
      : impact_coefficient_(0.1),
        rolling_window_(10),
        tick_floor_bps_(1.0),
        impact_exponent_(0.5),
        participation_rate_cap_(0.3),  // PR 캡 (Q/V 폭주 방지)
        spread_ema_alpha_(0.3),        // 스프레드 EMA 스무딩 알파
        epsilon_(1e-10),
        stress_multiplier_(stress_multiplier),
        is_trading_low_tf_(false),
        is_magnifier_low_tf_(false) {}
  ~MarketImpactSlippage() override = default;

  [[nodiscard]] unique_ptr<Slippage> Clone() const override {
    return make_unique<MarketImpactSlippage>(*this);
  }

  [[nodiscard]] optional<string> ValidateTakerSlippage() const override;
  [[nodiscard]] optional<string> ValidateMakerSlippage() const override;

  void Initialize() override;

  [[nodiscard]] double CalculateSlippagePrice(OrderType order_type,
                                              Direction direction,
                                              double order_price,
                                              double order_size,
                                              int symbol_idx) const override;

  [[nodiscard]] __forceinline double GetStressMultiplier() const {
    return stress_multiplier_;
  }

 private:
  double impact_coefficient_;      // k 계수
  size_t rolling_window_;          // 롤링 윈도우 크기
  mutable double tick_floor_bps_;  // 상대적 심볼 틱 플로어 크기 (bps)
  double impact_exponent_;         // β (기본 0.5)
  double participation_rate_cap_;  // PR 캡 (Q/V 폭주 방지, 기본 0.3)
  double spread_ema_alpha_;        // 스프레드 EMA 스무딩 알파 (기본 0.3)
  double epsilon_;                 // 0으로 나누기 방지
  double stress_multiplier_;       // 스트레스 테스트 용도 슬리피지 틱 계수

  // 각 타임프레임이 15분보다 이하인지 여부
  bool is_trading_low_tf_;
  bool is_magnifier_low_tf_;

  static shared_ptr<BarHandler>& bar_;
  static shared_ptr<Config>& config_;

  // 심볼별 이전 스프레드 (EMA용)
  mutable vector<double> previous_spread_bps_;

  /// EDGE 스프레드 추정 (Ardia-Guidotti-Kröncke)
  [[nodiscard]] double EstimateSpreadEdge(
      int symbol_idx, size_t bar_idx,
      const shared_ptr<BarData>& bar_data) const;

  /// Corwin-Schultz 2바 스프레드 추정: 폴백 용도
  [[nodiscard]] double EstimateSpreadCorwinSchultz(
      int symbol_idx, size_t bar_idx,
      const shared_ptr<BarData>& bar_data) const;

  /// Garman-Klass 변동성
  [[nodiscard]] double EstimateVolatilityGarmanKlass(
      int symbol_idx, size_t bar_idx,
      const shared_ptr<BarData>& bar_data) const;

  /// 롤링 윈도우 거래량 합계
  [[nodiscard]] double CalculateRollingVolume(
      int symbol_idx, size_t bar_idx,
      const shared_ptr<BarData>& bar_data) const;

  /// NaN/Inf 체크 및 대체값 반환
  [[nodiscard]] __forceinline static double SanitizeValue(
      const double value, const double fallback_value = 0.0) {
    if (isnan(value) || isinf(value)) {
      return fallback_value;
    }

    return value;
  }
};

}  // namespace backtesting::order
