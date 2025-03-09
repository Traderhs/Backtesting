#pragma once

// 내부 헤더
#include "Engines/BaseBarHandler.hpp"
#include "Engines/BinanceFetcher.hpp"
#include "Engines/Config.hpp"
#include "Engines/Engine.hpp"
#include "Engines/Logger.hpp"
#include "Strategies/TestStrategy.hpp"

// 네임 스페이스
using namespace backtesting;
using namespace fetcher;

namespace backtesting {
class Backtesting {
 public:
  /**
   * 지정된 심볼과 시간 프레임에 대해 현물 및 연속 선물 klines 데이터를
   * Fetch 후 병합하고 Parquet 형식으로 저장하는 함수
   *
   * @param symbol 연속 선물 캔들스틱 데이터를 가져올
   *               거래 쌍 심볼(예: "BTCUSDT")
   * @param timeframe 연속 선물 캔들스틱 데이터의 타임프레임(예: "1m", "1h")
   */
  void FetchContinuousKlines(const string& symbol, const string& timeframe) {
    fetcher_->FetchContinuousKlines(symbol, timeframe);
  }

 private:
  auto& engine_ = Engine::GetEngine();
  auto& logger_ = Logger::GetLogger();
  auto fetcher_ =
      make_shared<BinanceFetcher>("BINANCE_API_KEY", "BINANCE_API_SECRET");
};
}  // namespace backtesting