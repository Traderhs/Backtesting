#pragma once

// 표준 라이브러리
#include <deque>
#include <memory>
#include <vector>

// 내부 헤더
#include "Engines/BarHandler.hpp"  // 이제 BarHandler에서 여기 include 하면 에러 발생
#include "Engines/Config.hpp"
#include "Engines/DataUtils.hpp"
#include "Engines/Order.hpp"
#include "Engines/Slippage.hpp"
#include "Engines/SymbolInfo.hpp"

// 전방 선언
namespace backtesting::analyzer {
class Analyzer;
}  // namespace backtesting::analyzer

namespace backtesting::bar {
struct Bar;
class BarHandler;
}  // namespace backtesting::bar

namespace backtesting::engine {
class Config;
class Engine;
enum class PriceType;
}  // namespace backtesting::engine

namespace backtesting::logger {
class Logger;
enum class LogLevel;
}  // namespace backtesting::logger

namespace backtesting::order {
class Slippage;
class SymbolInfo;
class Order;
enum class Direction;
enum class OrderType;
struct LeverageBracket;
}  // namespace backtesting::order

// 네임 스페이스
using namespace std;
namespace backtesting {
using namespace analyzer;
using namespace bar;
using namespace engine;
using namespace logger;
using namespace utils;
}  // namespace backtesting

namespace backtesting::order {

// 주문 시그널을 나타나는 열거형 클래스
enum class OrderSignal { LIQUIDATION, EXIT, ENTRY };

// 진입, 청산, 강제 청산해야 하는 주문의 정보를 담은 구조체
struct FillInfo {
  shared_ptr<Order> order;   // 주문 객체
  OrderSignal order_signal;  // 진입 or 청산 or 강제 청산을 지칭
  double fill_price;         // 슬리피지를 미반영한 체결 가격
};

// 어느 대기 주문에서 취소할 지 결정하는 열거형 클래스
// TOTAL은 진입 대기 주문 및 청산 대기 주문 모두에서 취소함
enum class CancelType { TOTAL, ENTRY, EXIT };

/// 주문, 포지션 등과 관련된 기본적인 작업을 처리하는 클래스
class BaseOrderHandler {
  // 기타 함수 접근용
  friend class Engine;

 public:
  // ===========================================================================
  // 전략에서 사용하는 함수들
  // ===========================================================================
  /// 대기 주문 취소를 위해 사용하는 함수.
  ///
  /// @param order_name 취소할 주문의 이름
  /// @param cancel_type 진입 주문, 청산 주문, 전체 주문 중 어디서
  ///                    취소를 할 것인지 명시
  /// @param cancellation_reason 취소 사유
  void Cancel(const string& order_name, CancelType cancel_type,
              const string& cancellation_reason);

  /// 현재 심볼 마지막 진입으로부터 몇 개의 트레이딩 바가 지났는지 계산하여
  /// 반환하는 함수
  ///
  /// 1. 진입이 아직 없었던 심볼은 NaN이 반환됨
  /// 2. AFTER 전략에서 돋보기 바로 참조해도 트레이딩 바 인덱스로
  ///    참조되므로 주의
  [[nodiscard]] double BarsSinceEntry() const;

  /// 현재 심볼 마지막 청산으로부터 몇 개의 트레이딩 바가 지났는지 계산하여
  /// 반환하는 함수
  ///
  /// 1. 청산이 아직 없었던 심볼은 NaN이 반환됨
  /// 2. AFTER 전략에서 돋보기 바로 참조해도 트레이딩 바 인덱스로
  ///    참조되므로 주의
  [[nodiscard]] double BarsSinceExit() const;

  /// 현재 심볼의 마지막 진입 가격을 반환하는 함수
  [[nodiscard]] double LastEntryPrice() const;

  /// 현재 심볼의 마지막 청산 가격을 반환하는 함수
  [[nodiscard]] double LastExitPrice() const;

  /// 현재 심볼의 포지션 사이즈를 단순 반환하는 함수.\n\n
  /// 전략 실횅 시점에 무조건 값을 업데이트하기 때문에 전략 내에서는 이 함수로
  /// 값을 사용하면 됨.\n\n
  /// 양수면 매수 진입, 음수면 매도 진입.
  [[nodiscard]] __forceinline double GetCurrentPositionSize() const {
    return current_position_size_;
  }

  /// 지정된 심볼 마크 가격의 지정된 가격 타입을 기준으로 계산한 미실현 손실의
  /// 절댓값의 합계를 반환하는 함수.
  ///
  /// 마크 가격이 현재 진행 중인 Close Time과 일치하지 않는다면 전략을 실행한 바
  /// 타입의 가격을 사용
  [[nodiscard]] double GetUnrealizedLoss(int symbol_idx,
                                         PriceType price_type) const;

  /// 현재 심볼과 바에서 진입이 이루어졌는지 여부를 반환하는 함수
  [[nodiscard]] __forceinline bool IsJustEntered() const {
    return just_entered_;
  }

  /// 현재 심볼과 바에서 청산이 이루어졌는지 여부를 반환하는 함수
  [[nodiscard]] __forceinline bool IsJustExited() const { return just_exited_; }

  /// 심볼 이름으로 포맷된 로그를 발생시키는 함수
  __forceinline void LogFormattedInfo(const LogLevel log_level,
                                      const string& formatted_message,
                                      const char* file, const int line) {
    logger_->Log(log_level,
                 format("[{}] {}", symbol_names_[bar_->GetCurrentSymbolIndex()],
                        formatted_message),
                 file, line, false);
  }

  /// 진입 마진을 계산하여 반환하는 함수
  ///
  /// price_type은 미실현 손실을 계산하는 가격 기준을 지정
  [[nodiscard]] double CalculateMargin(double price, double entry_size,
                                       PriceType price_type,
                                       int symbol_idx) const;

  /// 주문 정보에 따라 강제 청산 가격을 계산하여 반환하는 함수
  [[nodiscard]] static double CalculateLiquidationPrice(
      Direction entry_direction, double order_price, double position_size,
      double margin, int symbol_idx);

 protected:
  BaseOrderHandler();
  ~BaseOrderHandler();

  static shared_ptr<Analyzer>& analyzer_;
  static shared_ptr<BarHandler>& bar_;
  static shared_ptr<Config>& config_;
  static shared_ptr<Engine>& engine_;
  static shared_ptr<Logger>& logger_;

  // 엔진 설정들
  double initial_balance_;         // 초기 자금
  shared_ptr<Slippage> slippage_;  // 슬리피지 계산 방법
  double taker_fee_percentage_;    // 테이커 수수료율
  double maker_fee_percentage_;    // 메이커 수수료율
  bool check_limit_max_qty_;       // 지정가 최대 수량 검사 여부
  bool check_limit_min_qty_;       // 지정가 최소 수량 검사 여부
  bool check_market_max_qty_;      // 시장가 최대 수량 검사 여부
  bool check_market_min_qty_;      // 시장가 최소 수량 검사 여부
  bool check_min_notional_value_;  // 최소 명목 가치 검사 여부

  // 심볼 정보
  static vector<SymbolInfo> symbol_info_;

  // 심볼 이름들
  vector<string> symbol_names_;

  // 진입 및 청산 주문: 심볼 인덱스<주문>
  vector<deque<shared_ptr<Order>>> pending_entries_;  // 대기 중인 진입 주문
  vector<deque<shared_ptr<Order>>> filled_entries_;   // 체결된 진입 주문
  vector<deque<shared_ptr<Order>>> pending_exits_;    // 대기 중인 청산 주문

  // 체결해야 하는 주문 목록 (강제 청산 + 청산 + 진입)
  vector<FillInfo> should_fill_orders_;

  /// 현재 심볼의 포지션 사이즈. 양수면 매수 진입, 음수면 매도 진입.
  double current_position_size_;

  /// 현재 심볼과 바에서 진입 혹은 청산이 이루어졌는지를 결정하는 플래그
  bool just_entered_;
  bool just_exited_;

  /// 각 심볼별 마지막 진입과 청산의 바 인덱스
  vector<size_t> last_entry_bar_indices_;
  vector<size_t> last_exit_bar_indices_;

  /// 각 심볼별 마지막 진입과 청산의 가격
  vector<double> last_entry_prices_;
  vector<double> last_exit_prices_;

  /// 리버스 청산을 진행할 때 시장가 최대 주문 수량 검사를 피하기 위한 플래그.
  /// 시스템적으로 전량 청산 후 반대 방향 진입을 해야하는데,
  /// 검사로 진입이 막히면 방법이 없으므로 이 방법으로 간략화
  bool is_reverse_exit_;

  /// 리버스 청산을 진행할 때 청산 가격을 지정하기 위한 변수.
  /// MarketExit은 청산 가격의 별도 지정이 불가능하기 때문에 클래스 변수를 사용
  double reverse_exit_price_;

  /// 현재 심볼의 레버리지를 변경하는 함수
  ///
  /// 현재 심볼에 체결된 주문이 없는 경우에만 변경 가능
  ///
  /// 실패 시 에러 문자열이 반환됨
  [[nodiscard]] optional<string> AdjustLeverage(int leverage, int symbol_idx);

  // 지정된 심볼의 설정된 레버리지를 반환하는 함수
  [[nodiscard]] int GetLeverage(int symbol_idx) const;

  /// 주문 정보에 따라 슬리피지를 반영한 체결 가격을 반환하는 함수.
  [[nodiscard]] __forceinline double CalculateSlippagePrice(
      const OrderType order_type, const Direction direction,
      const double order_price, const double order_size,
      const int symbol_idx) const {
    // slippage_ 객체를 통해 슬리피지 가격 계산
    return slippage_->CalculateSlippagePrice(order_type, direction, order_price,
                                             order_size, symbol_idx);
  }

  /// 주문 정보에 따라 수수료 금액을 계산하여 반환하는 함수
  [[nodiscard]] double CalculateTradingFee(OrderType order_type,
                                           double filled_price,
                                           double filled_size) const;

  /// 지정된 심볼과 명목 가치에 해당되는 레버리지 구간을 찾아 반환하는 함수
  [[nodiscard]] static LeverageBracket GetLeverageBracket(int symbol_idx,
                                                          double order_price,
                                                          double position_size);

  /// 진입 정보에 따라 PnL을 계산하는 함수
  [[nodiscard]] static double CalculatePnl(Direction entry_direction,
                                           double base_price,
                                           double entry_price,
                                           double position_size);

  // 방향이 유효한 값인지 확인하는 함수
  [[nodiscard]] __forceinline static optional<string> IsValidDirection(
      const Direction direction) {
    if (direction == DIRECTION_NONE) [[unlikely]] {
      return format("방향 [NONE] 오류 (조건: [LONG] 또는 [SHORT])");
    }

    return nullopt;
  }

  // 가격이 유효한 값인지 확인하는 함수
  [[nodiscard]] __forceinline static optional<string> IsValidPrice(
      const double price, const int symbol_idx) {
    if (IsLessOrEqual(price, 0.0) || isnan(price)) [[unlikely]] {
      return format(
          "가격 [{}] 오류 (조건: 0 초과 및 NaN이 아닌 실수)",
          ToFixedString(price, symbol_info_[symbol_idx].GetPricePrecision()));
    }

    return nullopt;
  }

  // 포지션 크기가 유효한 값인지 확인하는 함수
  [[nodiscard]] optional<string> IsValidPositionSize(double position_size,
                                                     OrderType order_type,
                                                     int symbol_idx) const;

  // 명목 가치(가격 * 포지션 크기)가 최소 기준을 통과하여
  // 유효한 값인지 확인하는 함수
  [[nodiscard]] __forceinline optional<string> IsValidNotionalValue(
      const double order_price, const double position_size,
      const int symbol_idx) const {
    if (check_min_notional_value_) {
      // 명목 가치가 해당 심볼의 최소 명목 가치보다 작으면 오류
      const auto notional = order_price * position_size;
      if (const auto min_notional =
              symbol_info_[symbol_idx].GetMinNotionalValue();
          IsLess(notional, min_notional)) {
        return format(
            "명목 가치 [{}] 부족 (조건: 심볼의 최소 명목 가치 [{}] 이상)",
            FormatDollar(notional, true), FormatDollar(min_notional, true));
      }
    }

    return nullopt;
  }

  // 지정된 레버리지가 1 이상이고 명목 가치에 해당되는 브라켓의 최대 레버리지
  // 이하인지 확인하는 함수
  [[nodiscard]] __forceinline static optional<string> IsValidLeverage(
      const int leverage, const double order_price, const double position_size,
      const int symbol_idx) {
    if (const auto max_leverage =
            GetLeverageBracket(symbol_idx, order_price, position_size)
                .max_leverage;
        leverage < 1 || leverage > max_leverage) {
      return format(
          "레버리지 [{}x] 조건 미만족 (조건: [1x] 이상 및 명목 가치 [{}] "
          "레버리지 구간의 최대 레버리지 [{}x] 이하)",
          leverage, FormatDollar(order_price * position_size, true),
          max_leverage);
    }

    return nullopt;
  }

  /// 진입 체결 시 진입 이름이 유효한지 확인하는 함수
  [[nodiscard]] __forceinline optional<string> IsValidEntryName(
      const string& entry_name, const int symbol_idx) const {
    /* 같은 이름으로 체결된 Entry Name이 여러 개 존재하면, 청산 시 Target Entry
       지정할 때의 로직이 꼬이기 때문에 하나의 Entry Name은 하나의 진입 체결로
       제한 */
    for (const auto& filled_entry : filled_entries_[symbol_idx]) {
      /* 체결된 진입 주문 중 같은 이름이 하나라도 존재하면
         해당 entry_name으로 진입 불가 */
      if (entry_name == filled_entry->GetEntryName()) {
        return format("중복된 진입 이름 [{}] 동시 체결 불가", entry_name);
      }
    }

    return nullopt;
  }

  /// 청산 주문 시 청산 이름이 유효한지 확인하는 함수
  [[nodiscard]] __forceinline optional<string> IsValidExitName(
      const string& exit_name) const {
    // 강제 청산을 청산 이름으로 사용하면 혼선이 있을 수 있으며,
    // 백보드에서 강제 청산 카운트에서 오류가 생기므로 원칙적 금지
    if (exit_name.find("강제 청산") != string::npos) [[unlikely]] {
      return "청산 이름에 \"강제 청산\" 단어 포함 금지";
    }

    // 리버스는 리버스 청산을 위한 시스템 이름이므로 사용 금지
    if (!is_reverse_exit_) {
      if (exit_name.find("리버스") != string::npos) [[unlikely]] {
        return "청산 이름에 \"리버스\" 단어 포함 금지";
      }
    }

    return nullopt;
  }

  /// 지정가 주문 가격이 유효한 가격인지 확인하는 함수
  [[nodiscard]] __forceinline static optional<string> IsValidLimitOrderPrice(
      const double limit_price, const double base_price,
      const Direction direction, const int symbol_idx) {
    if (direction == LONG && IsGreater(limit_price, base_price)) {
      const auto price_precision = symbol_info_[symbol_idx].GetPricePrecision();
      return format("[{}]에서 지정가 주문 불가 (조건: 기준가 [{}] 이하)",
                    ToFixedString(limit_price, price_precision),
                    ToFixedString(base_price, price_precision));
    }

    if (direction == SHORT && IsLess(limit_price, base_price)) {
      const auto price_precision = symbol_info_[symbol_idx].GetPricePrecision();
      return format("[{}]에서 지정가 주문 불가 (조건: 기준가 [{}] 이상)",
                    ToFixedString(limit_price, price_precision),
                    ToFixedString(base_price, price_precision));
    }

    return nullopt;
  }

  /// 트레일링 진입/청산의 터치 가격이 유효한지 확인하는 함수.
  /// 트레일링 진입/청산의 터치 가격은 0으로 지정될 수 있기 때문에 별개 함수로
  /// 처리
  [[nodiscard]] __forceinline static optional<string> IsValidTrailingTouchPrice(
      const double touch_price, const int symbol_idx) {
    if (IsLess(touch_price, 0.0)) [[unlikely]] {
      return format(
          "트레일링 터치 가격 [{}] 미달 (조건: 0 이상)",
          ToFixedString(touch_price,
                        symbol_info_[symbol_idx].GetPricePrecision()));
    }

    return nullopt;
  }

  /// 트레일링 포인트가 유효한지 확인하는 함수
  [[nodiscard]] __forceinline static optional<string> IsValidTrailPoint(
      const double trail_point, const int symbol_idx) {
    if (IsLessOrEqual(trail_point, 0.0)) [[unlikely]] {
      return format(
          "트레일링 포인트 [{}] 미달 (조건: 0 초과)",
          ToFixedString(trail_point,
                        symbol_info_[symbol_idx].GetPricePrecision()));
    }

    return nullopt;
  }

  /// 지정가 주문에서 현재 가격이 진입 방향에 따라 주문 가격보다 낮아졌거나
  /// 커졌는지 확인하는 함수.
  ///
  /// 매수 진입의 경우, 가격이 주문 가격과 같거나 낮아지면 조건 만족.
  ///
  /// 매도 진입의 경우, 가격이 주문 가격과 같거나 높아지면 조건 만족.
  [[nodiscard]] __forceinline static bool IsLimitPriceSatisfied(
      const Direction order_direction, const double price,
      const double order_price) {
    return (order_direction == LONG && IsLessOrEqual(price, order_price)) ||
           (order_direction == SHORT && IsGreaterOrEqual(price, order_price));
  }

  /// 현재 가격이 터치 방향에 따라 터치 가격보다 커졌거나 작아졌는지 확인하는
  /// 함수.
  ///
  /// 터치 방향이 매수인 경우, 터치 가격과 같거나 커지면 조건 만족.
  ///
  /// 터치 방향이 매도인 경우, 터치 가격과 같거나 작아지면 조건 만족.
  [[nodiscard]] __forceinline static bool IsPriceTouched(
      const Direction touch_direction, const double price,
      const double touch_price) {
    return (touch_direction == LONG && IsGreaterOrEqual(price, touch_price)) ||
           (touch_direction == SHORT && IsLessOrEqual(price, touch_price));
  }

  /// 자금이 필요 자금보다 많은지 확인하는 함수
  [[nodiscard]] __forceinline static optional<string> HasEnoughBalance(
      const double balance, const double needed_balance,
      const string& balance_type_msg, const string& purpose_msg) {
    if (IsLess(balance, needed_balance)) {
      return format("{} 자금 [{}] 부족 (필요 자금: {} [{}])", balance_type_msg,
                    FormatDollar(balance, true), purpose_msg,
                    FormatDollar(needed_balance, true));
    }

    return nullopt;
  }

  /// 지정된 심볼 마지막 진입의 트레이딩 바 인덱스를 업데이트하는 함수
  void UpdateLastEntryBarIndex(int symbol_idx);

  /// 지정된 심볼 마지막 청산의 트레이딩 바 인덱스를 업데이트하는 함수
  void UpdateLastExitBarIndex(int symbol_idx);

 private:
  // BaseOrderHandler가 초기화 됐는지 결정하는 플래그
  bool is_initialized_;

  /// 심볼별 현재 레버리지
  vector<int> leverages_;

  /// 엔진 설정을 불러오고 주문들과 기타 설정을 초기화하는 함수
  void Initialize(int num_symbols, const vector<string>& symbol_names);

  /// 심볼 정보를 초기화하는 함수
  static void SetSymbolInfo(const vector<SymbolInfo>& symbol_info);

  /// 현재 심볼의 포지션 사이즈 합계를 최신 상태로 업데이트하는 함수
  void UpdateCurrentPositionSize(int symbol_idx);

  /// 현재 심볼과 바에서 진입이 이루어졌는지를 결정하는 플래그를 초기화하는 함수
  void InitializeJustEntered();

  /// 현재 심볼과 바에서 청산이 이루어졌는지를 결정하는 플래그를 초기화하는 함수
  void InitializeJustExited();

  /// 진입 주문 취소 시 자금 관련 처리를 하는 함수
  static void DecreaseUsedMarginOnEntryCancel(
      const shared_ptr<Order>& cancel_order);
};

}  // namespace backtesting::order
