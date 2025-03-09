// 표준 라이브러리
#include <format>
#include <fstream>

// 파일 헤더
#include "Engines/BaseAnalyzer.hpp"

// 내부 헤더
#include "Engines/DataUtils.hpp"
#include "Engines/Logger.hpp"
#include "Engines/Trade.hpp"

// 네임 스페이스
namespace backtesting {
using namespace analyzer;
using namespace logger;
using namespace utils;
}  // namespace backtesting

namespace backtesting::analyzer {

BaseAnalyzer::BaseAnalyzer() : trade_num_(1) {}
BaseAnalyzer::~BaseAnalyzer() = default;

shared_ptr<Logger>& BaseAnalyzer::logger_ = Logger::GetLogger();

void BaseAnalyzer::Initialize(const double initial_balance) {
  trading_list_.push_back(Trade()
                              .SetSymbolName("-")
                              .SetStrategyName("-")
                              .SetEntryName("-")
                              .SetExitName("-")
                              .SetEntryTime("-")
                              .SetExitTime("-")
                              .SetHoldingTime("-")
                              .SetWalletBalance(initial_balance)
                              .SetMaxWalletBalance(initial_balance));
}

void BaseAnalyzer::AddTrade(Trade& trade) {
  trade.SetTradeNumber(trade_num_++)
      .SetProfitLossPer(trade.GetProfitLoss() /
                        trading_list_.back().GetWalletBalance() * 100);
  // 손익률은 전 거래의 지갑 자금 대비 손익

  trading_list_.push_back(trade);
}

void BaseAnalyzer::TradingListToCsv(const string& file_path) const {
  ofstream file(file_path);
  if (!file.is_open()) {
    Logger::LogAndThrowError("파일을 열 수 없습니다", __FILE__, __LINE__);
  }

  // CSV 헤더 작성
  file << "거래 번호,전략 이름,심볼 이름,진입 이름,청산 이름,진입 방향,"
          "진입 시간,청산 시간,보유 시간,진입 수량,청산 수량,진입 가격,"
          "청산 가격,레버리지,진입 수수료,청산 수수료,강제 청산 수수료,"
          "손익,손익률,현재 자금,최고 자금,드로우다운,최고 드로우다운,"
          "보유 심볼 수\n";

  // 데이터 작성
  for (const auto& trade : trading_list_) {
    file << "\"" << trade.GetTradeNumber() << "\","
         << "\"" << trade.GetStrategyName() << "\","
         << "\"" << trade.GetSymbolName() << "\","
         << "\"" << trade.GetEntryName() << "\","
         << "\"" << trade.GetExitName() << "\","
         << "\"" << trade.GetEntryDirection() << "\","
         << "\"" << trade.GetEntryTime() << "\","
         << "\"" << trade.GetExitTime() << "\","
         << "\"" << trade.GetHoldingTime() << "\","
         << "\"" << trade.GetEntrySize() << "\","
         << "\"" << trade.GetExitSize() << "\","
         << "\"" << trade.GetEntryPrice() << "\","
         << "\"" << trade.GetExitPrice() << "\","
         << "\"" << trade.GetLeverage() << "\","
         << "\"" << FormatDollar(trade.GetEntryFee()) << "\","
         << "\"" << FormatDollar(trade.GetExitFee()) << "\","
         << "\"" << FormatDollar(trade.GetLiquidationFee()) << "\","
         << "\"" << FormatDollar(trade.GetProfitLoss()) << "\","
         << "\""
         << to_string(RoundToDecimalPlaces(trade.GetProfitLossPer(), 2)) +
                "%\","
         << "\"" << FormatDollar(trade.GetWalletBalance()) << "\","
         << "\"" << FormatDollar(trade.GetMaxWalletBalance()) << "\","
         << "\""
         << to_string(RoundToDecimalPlaces(trade.GetDrawdown(), 2)) + "%\","
         << "\""
         << to_string(RoundToDecimalPlaces(trade.GetMaxDrawdown(), 2)) + "%\","
         << "\"" << trade.GetSymbolCount() << "\"\n";
  }

  file.close();

  logger_->Log(INFO_L,
               format("거래 목록이 {} 경로에 저장되었습니다.", file_path),
               __FILE__, __LINE__);
}

}  // namespace backtesting::analyzer