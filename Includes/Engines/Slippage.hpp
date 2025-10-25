#pragma once

// 표준 라이브러리
#include <memory>
#include <optional>
#include <string>

// 전방 선언
namespace backtesting::order {
enum class Direction;
enum class OrderType;
}  // namespace backtesting::order

// 네임 스페이스
using namespace std;

namespace backtesting::order {

/// 슬리피지 계산을 담당하는 추상 기본 클래스
class Slippage {
 public:
  virtual ~Slippage() = default;

  /// 슬리피지 객체를 복제하는 순수 가상 함수
  [[nodiscard]] virtual unique_ptr<Slippage> clone() const = 0;

  /// 슬리피지를 적용한 체결 가격을 계산하는 순수 가상 함수
  ///
  /// @param order_type 주문 타입 (시장가/지정가)
  /// @param direction 진입/청산 방향
  /// @param order_price 원래 주문 가격
  /// @param price_step 심볼의 가격 단위 (반올림용)
  /// @return 슬리피지가 적용된 체결 가격
  [[nodiscard]] virtual double CalculateSlippagePrice(
      OrderType order_type, Direction direction, double order_price,
      double price_step) const = 0;

  /// 테이커 슬리피지가 유효한지 검증하는 순수 가상 함수
  ///
  /// @return 유효하면 nullopt, 유효하지 않으면 에러 메시지
  [[nodiscard]] virtual optional<string> ValidateTakerSlippage() const = 0;

  /// 메이커 슬리피지가 유효한지 검증하는 순수 가상 함수
  ///
  /// @return 유효하면 nullopt, 유효하지 않으면 에러 메시지
  [[nodiscard]] virtual optional<string> ValidateMakerSlippage() const = 0;
};

/// 퍼센트 기반 슬리피지 계산 클래스
class PercentageSlippage final : public Slippage {
 public:
  /// 생성자 - 테이커 및 메이커 슬리피지 퍼센트를 반드시 지정해야 함
  ///
  /// @param taker_slippage_percentage 테이커(시장가) 슬리피지율 (%)
  /// @param maker_slippage_percentage 메이커(지정가) 슬리피지율 (%)
  explicit PercentageSlippage(const double taker_slippage_percentage,
                              const double maker_slippage_percentage)
      : taker_slippage_percentage_(taker_slippage_percentage),
        maker_slippage_percentage_(maker_slippage_percentage) {}

  // 기본 생성자 삭제
  PercentageSlippage() = delete;

  ~PercentageSlippage() override = default;

  [[nodiscard]] unique_ptr<Slippage> clone() const override {
    return make_unique<PercentageSlippage>(taker_slippage_percentage_,
                                           maker_slippage_percentage_);
  }

  [[nodiscard]] double CalculateSlippagePrice(OrderType order_type,
                                              Direction direction,
                                              double order_price,
                                              double price_step) const override;

  [[nodiscard]] optional<string> ValidateTakerSlippage() const override;
  [[nodiscard]] optional<string> ValidateMakerSlippage() const override;

  [[nodiscard]] double GetTakerSlippagePercentage() const {
    return taker_slippage_percentage_;
  }
  [[nodiscard]] double GetMakerSlippagePercentage() const {
    return maker_slippage_percentage_;
  }

 private:
  /// 테이커(시장가) 슬리피지율
  ///
  /// 백분율로 지정 시 100 곱한 값 (5%면 5로 지정)
  double taker_slippage_percentage_;

  /// 메이커(지정가) 슬리피지율
  ///
  /// 백분율로 지정 시 100 곱한 값 (5%면 5로 지정)
  double maker_slippage_percentage_;
};

}  // namespace backtesting::order
