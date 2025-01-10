#pragma once

// 표준 라이브러리
#include <string>
#include <vector>

// 네임 스페이스
using namespace std;

class BaseAnalyzer {
 protected:
  BaseAnalyzer();
  ~BaseAnalyzer();

 private:
  /// 거래 목록에서 하나의 거래 정보를 담고있는 구조체
  struct Trade {
    int trade_number;           // 거래 번호
    string entry_name;          // 진입 주문 이름
    string exit_name;           // 청산 주문 이름
    string symbol;              // 심볼명
    string entry_direction;     // 진입 방향
    double trade_size;          // 거래 수량
    unsigned char leverage;     // 레버리지
    double commission;          // 수수료 금액
    double slippage;            // 슬리피지 금액
    string entry_time;         // 진입 시간
    string exit_time;          // 청산 시간
    string holding_time;       // 보유 시간
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

  /// 거래 목록
  vector<Trade> trading_list_; // 구조 수정 필요하면 ㄱㄱ 아직 안봄
   // @@@@@@@@@@@@ 구현: 전체와 각 전략 1개씩 객체 생성 => 전략이 1개면 해당 전략 객체만 생성
   // Analyzer 내부에 여러개 저장할건지, 객체를 여러개 생성할건지는 나중에 정하자. 근데 후자가 나은듯?
};