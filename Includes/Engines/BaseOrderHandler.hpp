#pragma once

// 표준 라이브러리
#include <deque>
#include <memory>
#include <vector>

#include "Config.hpp"

// 전방 선언
namespace backtesting::analyzer {
class Analyzer;
class TechnicalAnalyzer;
}  // namespace backtesting::analyzer

namespace backtesting::bar {
class Bar;
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
}  // namespace backtesting

namespace backtesting::order {

/// 주문, 포지션 등과 관련된 기본적인 작업을 처리하는 클래스
class BaseOrderHandler {
 public:
  // ReSharper disable once CppInconsistentNaming
  /// 현재 심볼의 포지션 사이즈. 양수면 매수 진입, 음수면 매도 진입.
  double current_position_size;

  /// 엔진 설정을 불러오고 주문들과 기타 설정을 초기화하는 함수
  void Initialize(int num_symbols);

  /// 심볼 정보를 초기화하는 함수
  static void SetSymbolInfo(const vector<SymbolInfo>& symbol_info);

  /// 현재 심볼의 포지션 사이즈 합계를 업데이트하는 함수
  void UpdateCurrentPositionSize();

  /// 현재 심볼과 바에서 진입이 이루어졌는지를 결정하는 플래그를 초기화하는 함수
  void InitializeJustEntered();

  /// 현재 심볼과 바에서 청산이 이루어졌는지를 결정하는 플래그를 초기화하는 함수
  void InitializeJustExited();

  /// 지정된 심볼 마크 가격의 지정된 가격 타입을 기준으로 계산한 미실현 손실의
  /// 절댓값의 합계를 반환하는 함수.
  ///
  /// 마크 가격이 현재 Close Time과 일치하지 않는다면 트레이딩 바 가격을 사용
  [[nodiscard]] double GetUnrealizedLoss(int symbol_idx,
                                         PriceType price_type) const;

  /// 현재 심볼과 바에서 진입이 이루어졌는지 여부를 반환하는 함수
  [[nodiscard]] bool GetJustEntered() const;

  /// 현재 심볼과 바에서 청산이 이루어졌는지 여부를 반환하는 함수
  [[nodiscard]] bool GetJustExited() const;
  // ===========================================================================
  // 전략에서 사용하는 함수들

  /// 대기 주문 취소를 위해 사용하는 함수.
  ///
  /// order_name이 진입 대기 주문과 청산 대기 주문에 동시에 존재하면 모두 취소.
  void Cancel(const string& order_name);

  /// 현재 심볼의 레버리지를 변경하는 함수
  ///
  /// 현재 심볼에 체결된 주문이 없는 경우에만 변경 가능
  void AdjustLeverage(int leverage);

  /// 현재 심볼 마지막 진입으로부터 몇 개의 트레이딩 바가 지났는지 계산하여
  /// 반환하는 함수
  ///
  /// 진입이 아직 없었던 심볼은 NaN이 반환됨
  [[nodiscard]] double BarsSinceEntry() const;

  /// 현재 심볼 마지막 청산으로부터 몇 개의 트레이딩 바가 지났는지 계산하여
  /// 반환하는 함수
  ///
  /// 청산이 아직 없었던 심볼은 NaN이 반환됨
  [[nodiscard]] double BarsSinceExit() const;

  /// 현재 심볼의 마지막 진입 가격을 반환하는 함수
  [[nodiscard]] double LastEntryPrice() const;

  /// 현재 심볼의 마지막 청산 가격을 반환하는 함수
  [[nodiscard]] double LastExitPrice() const;

 protected:
  BaseOrderHandler();
  ~BaseOrderHandler();

  static shared_ptr<Analyzer>& analyzer_;
  static shared_ptr<BarHandler>& bar_;
  static shared_ptr<Engine>& engine_;
  static shared_ptr<Logger>& logger_;
  static shared_ptr<TechnicalAnalyzer>& ta_;

  // 엔진 설정
  static shared_ptr<Config> config_;

  // 심볼 정보
  static vector<SymbolInfo> symbol_info_;

  // 진입 및 청산 주문: 심볼 인덱스<주문>
  vector<deque<shared_ptr<Order>>> pending_entries_;  // 대기 중인 진입 주문
  vector<deque<shared_ptr<Order>>> filled_entries_;   // 체결된 진입 주문
  vector<deque<shared_ptr<Order>>> pending_exits_;    // 대기 중인 청산 주문

  /// 현재 심볼과 바에서 진입 혹은 청산이 이루어졌는지를 결정하는 플래그
  bool just_entered_;
  bool just_exited_;

  /// 각 심볼별 마지막 진입과 청산의 바 인덱스
  vector<size_t> last_entry_bar_indices_;
  vector<size_t> last_exit_bar_indices_;

  /// 각 심볼별 마지막 진입과 청산의 가격
  vector<double> last_entry_prices_;
  vector<double> last_exit_prices_;

  /// 리버스 청산을 진행할 때 시장가 최대 주문 수량 검사를 피하기 위한 플래그
  bool is_reverse_exit_;

  /// 전략 이름과 심볼 이름으로 포맷된 로그를 발생시키는 함수
  static void LogFormattedInfo(LogLevel log_level,
                               const string& formatted_message,
                               const char* file, int line);

  // 지정된 심볼의 설정된 레버리지를 반환하는 함수
  [[nodiscard]] int GetLeverage(int symbol_idx) const;

  /// 주문 정보에 따라 슬리피지를 반영한 체결 가격을 반환하는 함수.
  [[nodiscard]] static double CalculateSlippagePrice(OrderType order_type,
                                                     Direction direction,
                                                     double order_price);

  /// 주문 정보에 따라 수수료 금액을 계산하여 반환하는 함수
  [[nodiscard]] static double CalculateTradingFee(OrderType order_type,
                                                  double filled_price,
                                                  double filled_size);

  /// 주문 정보에 따라 강제 청산 가격을 계산하여 반환하는 함수
  [[nodiscard]] static double CalculateLiquidationPrice(
      Direction entry_direction, double order_price, double position_size,
      double entry_margin);

  /// 지정된 심볼과 명목 가치에 해당되는 레버리지 구간을 찾아 반환하는 함수
  [[nodiscard]] static LeverageBracket GetLeverageBracket(int symbol_idx,
                                                          double order_price,
                                                          double position_size);

  /// 진입 마진을 계산하여 반환하는 함수
  ///
  /// price_type은 미실현 손실을 계산하는 가격 기준을 지정
  [[nodiscard]] double CalculateMargin(double price, double entry_size,
                                       PriceType price_type) const;

  /// 진입 정보에 따라 PnL을 계산하는 함수
  [[nodiscard]] static double CalculatePnl(Direction entry_direction,
                                           double base_price,
                                           double entry_price,
                                           double position_size);

  // 방향이 유효한 값인지 확인하는 함수
  static void IsValidDirection(Direction direction);

  // 가격이 유효한 값인지 확인하는 함수
  static void IsValidPrice(double price);

  // 포지션 크기가 유효한 값인지 확인하는 함수
  void IsValidPositionSize(double position_size, OrderType order_type) const;

  // 명목 가치(가격 * 포지션 크기)가 최소 기준을 통과하여
  // 유효한 값인지 확인하는 함수
  static void IsValidNotionalValue(double order_price, double position_size);

  // 레버리지가 현재 브라켓의 최대 레버리지 이하인지 확인하는 함수
  void IsValidLeverage(double order_price, double position_size) const;

  /// 진입 체결 시 진입 이름이 유효한지 확인하는 함수
  void IsValidEntryName(const string& entry_name) const;

  /// 지정가 주문 가격이 유효한 가격인지 확인하는 함수
  static void IsValidLimitOrderPrice(double limit_price, double base_price,
                                     Direction direction);

  /// 트레일링 진입/청산의 터치 가격이 유효한지 확인하는 함수.
  /// 트레일링 진입/청산의 터치 가격은 0으로 지정될 수 있기 때문에 별개 함수로
  /// 처리.
  static void IsValidTrailingTouchPrice(double touch_price);

  /// 트레일링 포인트가 유효한지 확인하는 함수
  static void IsValidTrailPoint(double trail_point);

  /// 지정가 주문에서 현재 가격이 진입 방향에 따라 주문 가격보다 낮아졌거나
  /// 커졌는지 확인하는 함수.
  ///
  /// 매수 진입의 경우, 가격이 주문 가격과 같거나 낮아지면 조건 만족.
  ///
  /// 매도 진입의 경우, 가격이 주문 가격과 같거나 높아지면 조건 만족.
  [[nodiscard]] static bool IsLimitPriceSatisfied(Direction order_direction,
                                                  double price,
                                                  double order_price);

  /// 현재 가격이 터치 방향에 따라 터치 가격보다 커졌거나 작아졌는지 확인하는
  /// 함수.
  ///
  /// 터치 방향이 매수인 경우, 터치 가격과 같거나 커지면 조건 만족.
  ///
  /// 터치 방향이 매도인 경우, 터치 가격과 같거나 작아지면 조건 만족.
  [[nodiscard]] static bool IsPriceTouched(Direction touch_direction,
                                           double price, double touch_price);

  /// 자금이 필요 자금보다 많은지 확인하는 함수
  static void HasEnoughBalance(double balance, double needed_balance,
                               const string& balance_type_msg,
                               const string& purpose_msg);

  /// 지정된 심볼 마지막 진입의 트레이딩 바 인덱스를 업데이트하는 함수
  void UpdateLastEntryBarIndex(int symbol_idx);

  /// 지정된 심볼 마지막 청산의 트레이딩 바 인덱스를 업데이트하는 함수
  void UpdateLastExitBarIndex(int symbol_idx);

 private:
  /// 심볼별 현재 레버리지
  vector<int> leverages_;

  /// 진입 주문 취소 시 자금 관련 처리를 하는 함수
  static void ExecuteCancelEntry(const shared_ptr<Order>& cancel_order);
};

}  // namespace backtesting::order
