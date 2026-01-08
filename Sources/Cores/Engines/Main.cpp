// 내부 헤더
#include "Engines/Backtesting.hpp"
#include "Strategies/DiceSystem.hpp"

// 네임 스페이스
using namespace std;
using namespace backtesting;

void RunLocal() {
  // 거래소 설정
  Backtesting::SetMarketDataDirectory("D:/Programming/Backtesting/Data");
  Backtesting::SetApiEnvVars("BINANCE_API_KEY", "BINANCE_API_SECRET");

  const auto& exchange_info_path =
      "D:/Programming/Backtesting/Data/exchange_info.json";
  const auto& leverage_bracket_path =
      "D:/Programming/Backtesting/Data/leverage_bracket.json";

  Backtesting::FetchExchangeInfo(exchange_info_path);
  Backtesting::FetchLeverageBracket(leverage_bracket_path);

  Backtesting::AddExchangeInfo(exchange_info_path);
  Backtesting::AddLeverageBracket(leverage_bracket_path);

  // 심볼 설정
  const vector<string>& symbol_names = {
      "BTCUSDT",  "APTUSDT", "ETHUSDT",  "BNBUSDT", "SOLUSDT",
      "DOGEUSDT", "ADAUSDT", "AVAXUSDT", "DOTUSDT", "XRPUSDT"};

  // 바 데이터 설정
  Backtesting::AddBarData(symbol_names, "1h",
                          "D:/Programming/Backtesting/Data/Continuous Klines",
                          TRADING);

  Backtesting::AddBarData(symbol_names, "1m",
                          "D:/Programming/Backtesting/Data/Continuous Klines",
                          MAGNIFIER);

  Backtesting::AddBarData(symbol_names, "1d",
                          "D:/Programming/Backtesting/Data/Continuous Klines",
                          REFERENCE);

  Backtesting::AddBarData(symbol_names, "1m",
                          "D:/Programming/Backtesting/Data/Mark Price Klines",
                          MARK_PRICE);

  Backtesting::AddFundingRates(symbol_names,
                               "D:/Programming/Backtesting/Data/Funding Rates");

  // 엔진 설정
  Backtesting::SetConfig()
      .SetProjectDirectory("D:/Programming/Backtesting")
      .SetBacktestPeriod()
      .SetUseBarMagnifier(true)
      .SetInitialBalance(10000)
      .SetTakerFeePercentage(0.045)
      .SetMakerFeePercentage(0.018)
      .SetSlippage(MarketImpactSlippage(2))
      .SetCheckMarketMaxQty(false)
      .SetCheckMarketMinQty(false)
      .SetCheckLimitMaxQty(false)
      .SetCheckLimitMinQty(false)
      .SetCheckMinNotionalValue(true);

  // 전략 설정
  Backtesting::AddStrategy<DiceSystem>("Dice System");

  Backtesting::RunBacktesting();
}

int main(const int argc, char** argv) {
  bool server_mode = false;

  // 서버 모드 플래그 확인
  for (int i = 1; i < argc; i++) {
    if (string(argv[i]) == "--server") {
      server_mode = true;

      break;
    }
  }

  Backtesting::SetServerMode(server_mode);

  // 서버 모드에 따라 실행
  if (server_mode) {
    Backtesting::RunServer();
  } else {
    RunLocal();
  }
}
