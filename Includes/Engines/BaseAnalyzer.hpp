#pragma once

// 표준 라이브러리
#include <memory>
#include <string>
#include <vector>

// 전방 선언
class Engine;
class Trade;
class Order;
class Logger;

// 네임 스페이스
using namespace std;

/// 기본적인 거래 통계를 생성하는 분석기의 기초 클래스
class BaseAnalyzer {
 public:
  /// 분석기를 초기화하는 함수
  void Initialize(double initial_balance);

  /// 거래 목록에 거래를 추가하는 함수
  void AddTrade(Trade& trade);

  /// 거래 목록을 csv 파일로 저장하는 함수
  void TradingListToCsv(const string& file_path) const;

 protected:
  BaseAnalyzer();
  ~BaseAnalyzer();

  static shared_ptr<Logger>& logger_;

 private:
  /// 거래 목록
  vector<Trade> trading_list_;

  /// 현재 거래 번호
  int trade_num_;
};