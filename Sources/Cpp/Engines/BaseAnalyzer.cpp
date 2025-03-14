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
  trade_list_.push_back(Trade()
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

void BaseAnalyzer::AddTrade(Trade& new_trade, const int exit_count) {
  if (exit_count == 1) {
    // 전량 청산 거래거나 첫 분할 청산 거래인 경우 거래 번호 추가
    trade_list_.push_back(new_trade.SetTradeNumber(trade_num_++));
  } else {
    // 두 번째 분할 청산 거래부터는 첫 분할 청산 거래의 진입 거래 번호가
    // 거래 번호가 됨
    int trade_num = 0;
    for (int idx = trade_list_.size() - 1; idx >= 0; --idx) {
      if (const auto& trade = trade_list_[idx];
          trade.GetEntryName() == new_trade.GetEntryName() &&
          IsEqual(trade.GetEntryPrice(), new_trade.GetEntryPrice())) {
        trade_num = trade.GetTradeNumber();
        break;
      }
    }

    trade_list_.push_back(new_trade.SetTradeNumber(trade_num));
  }
}

void BaseAnalyzer::SaveTradeList(const string& file_path) const {
  ofstream file(file_path);
  if (!file.is_open()) {
    Logger::LogAndThrowError("파일을 열 수 없습니다", __FILE__, __LINE__);
  }

  // BOM 추가 (한글 인코딩 깨짐 방지)
  file << "\xEF\xBB\xBF";

  // CSV 헤더 작성
  file << "거래 번호,전략 이름,심볼 이름,진입 이름,청산 이름,진입 방향,"
          "진입 시간,청산 시간,보유 시간,레버리지,진입 가격,진입 수량,"
          "청산 가격,청산 수량,강제 청산 가격,진입 수수료,청산 수수료,"
          "강제 청산 수수료,손익,순손익,개별 손익률,전체 손익률,현재 자금,"
          "최고 자금,드로우다운,최고 드로우다운,누적 손익,누적 손익률,"
          "보유 심볼 수\n";

  // 데이터 작성
  for (const auto& trade : trade_list_) {
    file << "\"" << trade.GetTradeNumber() << "\","
         << "\"" << trade.GetStrategyName() << "\","
         << "\"" << trade.GetSymbolName() << "\","
         << "\"" << trade.GetEntryName() << "\","
         << "\"" << trade.GetExitName() << "\","
         << "\"" << trade.GetEntryDirection() << "\","
         << "\"" << trade.GetEntryTime() << "\","
         << "\"" << trade.GetExitTime() << "\","
         << "\"" << trade.GetHoldingTime() << "\","
         << "\"" << trade.GetLeverage() << "\","
         << "\"" << trade.GetEntryPrice() << "\","
         << "\"" << trade.GetEntrySize() << "\","
         << "\"" << trade.GetExitPrice() << "\","
         << "\"" << trade.GetExitSize() << "\","
         << "\"" << trade.GetLiquidationPrice() << "\","
         << "\"" << FormatDollar(trade.GetEntryFee(), false) << "\","
         << "\"" << FormatDollar(trade.GetExitFee(), false) << "\","
         << "\"" << FormatDollar(trade.GetLiquidationFee(), false) << "\","
         << "\"" << FormatDollar(trade.GetPnl(), false) << "\","
         << "\"" << FormatDollar(trade.GetPnlNet(), false) << "\","
         << "\""
         << to_string(RoundToDecimalPlaces(trade.GetIndividualPnlPer(), 2)) +
                "%\","
         << "\""
         << to_string(RoundToDecimalPlaces(trade.GetTotalPnlPer(), 2)) + "%\","
         << "\"" << FormatDollar(trade.GetWalletBalance(), false) << "\","
         << "\"" << FormatDollar(trade.GetMaxWalletBalance(), false) << "\","
         << "\""
         << to_string(RoundToDecimalPlaces(trade.GetDrawdown(), 2)) + "%\","
         << "\""
         << to_string(RoundToDecimalPlaces(trade.GetMaxDrawdown(), 2)) + "%\","
         << "\"" << FormatDollar(trade.GetCumPnl(), false) << "\","
         << "\""
         << to_string(RoundToDecimalPlaces(trade.GetCumPnlPer(), 2)) + "%\","
         << "\"" << trade.GetSymbolCount() << "\"\n";
  }

  file.close();
}

}  // namespace backtesting::analyzer