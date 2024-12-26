#pragma once

// 표준 라이브러리
#include <mutex>
#include <unordered_map>
#include <vector>

// 내부 헤더
#include "BarDataManager.hpp"
#include "DataManager.hpp"
#include "Logger.hpp"

// 네임 스페이스
using namespace std;

/**
 * 주문, 포지션 등과 관련된 작업을 처리하는 클래스
 */
class OrderManager final {
 public:
  /// 포지션 방향을 지정하는 열거형 클래스
  enum class Direction { LONG, SHORT };

  /// 주문 방법을 지정하는 열거형 클래스
  enum class OrderType { MARKET, LIMIT, MIT, LIT, TRAILING };

  /// 하나의 주문 정보를 담고있는 구조체
  struct order {
    string entry_name;           // 진입 주문 이름
    string exit_name;            // 청산 주문 이름
    Direction entry_direction;   // 진입 방향
    double ordered_entry_size;   // 진입 주문 수량
    double entry_size;           // 진입 체결 수량
    double ordered_exit_size;    // 청산 주문 수량
    double exit_size;            // 청산 체결 수량
    unsigned char leverage;      // 레버리지 배수
    double commission;           // 수수료 금액
    OrderType order_type;        // 주문 타입
    int64_t ordered_entry_time;  // 진입 주문 시간
    double ordered_entry_price;  // 진입 주문 가격
    int64_t entry_time;          // 진입 시간
    double entry_price;          // 진입 가격
    int64_t ordered_exit_time;   // 청산 주문 시간
    double ordered_exit_price;   // 청산 주문 가격
    int64_t exit_time;           // 청산 시간
    double exit_price;           // 청산 가격
    double margin_call_price;    // 마진콜 가격
    double max_profit;           // 최대 수익:
    double max_loss;  // 최대 손실: @@@@@@@@@ 이 두 항목은 트레이딩(돋보기) 바
                      // 진행시 업데이트
  };

  /// 거래 목록에서 하나의 거래 정보를 담고있는 구조체
  struct trade {
    int trade_number;           // 거래 번호
    string entry_name;          // 진입 주문 이름
    string exit_name;           // 청산 주문 이름
    string symbol;              // 심볼명
    Direction entry_direction;  // 진입 방향
    double trade_size;          // 거래 수량
    unsigned char leverage;     // 레버리지
    double commission;          // 수수료 금액
    double slippage;            // 슬리피지 금액
    int64_t entry_time;         // 진입 시간
    int64_t exit_time;          // 청산 시간
    int64_t holding_time;       // 보유 시간
    double entry_price;         // 진입 가격
    double exit_price;          // 청산 가격
    double profit_loss;         // 손익
    double profit_loss_per;     // 손익률
    double max_profit;          // 거래 중 최대 수익
    double max_loss;            // 거래 중 최대 손실
    double current_capital;     // 현재 자금
    double max_capital;         // 최대 자금
    double drawdown;            // 드로우다운
    double max_drawdown;        // 최고 드로우다운
    unsigned char entries;      // 보유 심볼 수
  };

  /// 현재 사용 중인 심볼의 포지션 사이즈
  int current_position_size;  // @@@@@@@@@@@@@@@ 바뀌는 거 추가

  /// OrderManager의 싱글톤 인스턴스를 반환하는 함수
  static OrderManager& GetOrderManager();

  /// ordered_entries, entries, ordered_exits, exits을 초기화하는 함수
  void InitializeOrders();

  /// 진입 주문을 위해 사용하는 함수
  void EntryMarket(const string& order_name, Direction entry_direction,
                   double order_size, unsigned char leverage);

  void EntryLimit(const string& order_name, Direction entry_direction,
                  double order_size, unsigned char leverage,
                  double order_price);

  void EntryMit(const string& order_name, Direction entry_direction,
                double order_size, unsigned char leverage, double touch_price,
                double order_price);

  void EntryLit(const string& order_name, Direction entry_direction,
                double order_size, unsigned char leverage, double touch_price,
                double order_price);

  void EntryTrailing(const string& order_name, Direction entry_direction,
                     double order_size, unsigned char leverage,
                     double trail_start_price, double trail_price);

  /// 포지션 청산 주문을 위해 사용하는 함수
  void Exit();  // 진입 주문 이름도 받기  // 진입 체결 수량 == 청산 체결
                // 수량일때만 exits로 이동요

  /// 주문 취소를 위해 사용하는 함수
  void Cancel(const string& order_name);

 private:
  OrderManager();
  ~OrderManager();

  // 싱글톤 인스턴스 관리
  static mutex mutex;
  static unique_ptr<OrderManager> instance;

  static BarDataManager& bar;
  static DataManager& data;
  static Logger& logger;

  // 주문들 <심볼, 벡터<주문>>
  unordered_map<string, vector<order>> ordered_entries;  // 진입 주문
  unordered_map<string, vector<order>> entries;          // 진입 완료 주문
  unordered_map<string, vector<order>> ordered_exits;    // 청산 주문
  unordered_map<string, vector<order>> exits;            // 청산 완료 주문

  /// 거래 목록
  vector<trade> trading_list;

  /// 시장가 진입 주문을 진행하는 함수
  void OrderEntryMarket(const order& order,
                        const string& entry_symbol,
                        int64_t entry_time,
                        double order_size,
                        double entry_price,
                        double commission);

  /// 손익에 따라 현재 자금 및 진입 가능 자금을 업데이트하는 함수
  void UpdateCapital();

  /// 주문 타입에 따라 슬리피지를 계산한 진입/청산 가격을 반환하는 함수
  /// @param order_type MARKET 혹은 LIMIT으로만 지정 가능
  /// @param direction 주문 방향
  /// @param price 주문 가격
  /// @param leverage 레버리지
  static double CalculateSlippagePrice(
    OrderType order_type, Direction direction, double price, unsigned char leverage);

  /// 주문 타입에 따라 수수료 금액을 계산하여 반환하는 함수
  static double CalculateCommission(OrderType order_type, double price, double position_size, unsigned char leverage);

  /// 마진콜 가격을 계산하여 반환하는 함수
  static double CalculateMarginCallPrice(
    Direction direction, double price, unsigned char leverage);

  /// 최소 틱 크기로 가격을 반올림하여 반환하는 함수
  static double RoundToTickSize(double price, double tick_size);
};
