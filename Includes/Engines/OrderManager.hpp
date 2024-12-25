#pragma once

// 표준 라이브러리
#include <mutex>
#include <unordered_map>
#include <vector>

// 네임 스페이스
using namespace std;

/**
 * 주문, 포지션 등과 관련된 작업을 처리하는 클래스
 */
class OrderManager final {
 public:
  // 포지션 방향을 지정하는 열거형 클래스
  enum class Direction { LONG, SHORT };

  // 주문 방법을 지정하는 열거형 클래스
  enum class OrderType { MARKET, LIMIT, MIT, LIT, TRAILING };

  // 하나의 주문 정보를 담고있는 구조체
  struct order {
    string entry_name;           // 진입 주문 이름
    string exit_name;            // 청산 주문 이름
    Direction entry_direction;   // 진입 방향
    double order_size;           // 주문 수량
    unsigned char leverage;      // 레버리지 배수
    OrderType order_type;        // 주문 타입
    int64_t ordered_entry_time;  // 진입 주문 시간
    double ordered_entry_price;  // 진입 주문 가격
    int64_t entry_time;          // 진입 시간
    double entry_price;          // 진입 가격
    int64_t ordered_exit_time;   // 청산 주문 시간
    double ordered_exit_price;   // 청산 주문 가격
    int64_t exit_time;           // 청산 시간
    double exit_price;           // 청산 가격
  };

  // 현재 사용 중인 심볼의 포지션 사이즈 // @@@@@@@@@@@@@@@ 바뀌는 거 추가
  int position_size;

  // OrderManager의 싱글톤 인스턴스를 반환하는 함수
  static OrderManager& GetOrderManager();

  // 진입 주문을 위해 사용하는 함수

  // @@@@@@@@@ 심볼 필요함? 바꿔서 이제 노필요아닌가
  void entry(const string& symbol, const string& order_name,  // MARKET
             Direction entry_direction, double order_size,
             unsigned char leverage, OrderType order_type);

  void entry(const string& symbol, const string& order_name,  // LIMIT, MIT, LIT
             Direction entry_direction, double order_size,
             unsigned char leverage, OrderType order_type, double order_price);

  void entry(const string& symbol, const string& order_name,  // TRAILING
             Direction entry_direction, double order_size,
             unsigned char leverage, OrderType order_type,
             double trail_start_price, double trail_price);

  // 포지션 청산 주문을 위해 사용하는 함수
  void exit();

  // 주문 취소를 위해 사용하는 함수
  void cancel(const string& order_name);

private:
  OrderManager();
  ~OrderManager();

  // 싱글톤 인스턴스 관리
  static mutex mutex;
  static unique_ptr<OrderManager> instance;

  // 포지션 <심볼, 벡터<주문>>
  unordered_map<string, vector<order>> ordered_entries;  // 진입 주문
  unordered_map<string, vector<order>> entries;          // 진입 완료
  unordered_map<string, vector<order>> ordered_exits;    // 청산 주문
  unordered_map<string, vector<order>> exits;            // 청산 완료
};
