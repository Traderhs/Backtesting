//
//  © 2024 Traderhs. All rights reserved.
//
//  □
//  ==============================================================================================
//  □
// || ||
// || ||
// ||    ██████╗  █████╗  ██████╗██╗
// ██╗████████╗███████╗███████╗████████╗██╗███╗   ██╗ ██████╗     ||
// ||    ██╔══██╗██╔══██╗██╔════╝██║
// ██╔╝╚══██╔══╝██╔════╝██╔════╝╚══██╔══╝██║████╗  ██║██╔════╝     ||
// ||    ██████╔╝███████║██║     █████╔╝    ██║   █████╗  ███████╗   ██║
// ██║██╔██╗ ██║██║  ███╗    ||
// ||    ██╔══██╗██╔══██║██║     ██╔═██╗    ██║   ██╔══╝  ╚════██║   ██║
// ██║██║╚██╗██║██║   ██║    ||
// ||    ██████╔╝██║  ██║╚██████╗██║  ██╗   ██║   ███████╗███████║   ██║ ██║██║
// ╚████║╚██████╔╝    ||
// ||    ╚═════╝ ╚═╝  ╚═╝ ╚═════╝╚═╝  ╚═╝   ╚═╝   ╚══════╝╚══════╝   ╚═╝ ╚═╝╚═╝
// ╚═══╝ ╚═════╝     ||
// || ||
// || ||
//  □
//  ==============================================================================================
//  □
//
//
//                       ● 다중 자산에 대한 고속, 정밀 백테스팅을 지원하는
//                       프로그램 ●
//
//                                  ◆ 다중 자산 포트폴리오 백테스팅 ◆
//                                 ◆ 벡터화 내부 구조로 고속 백테스팅 ◆
//                                      ◆ 그래프 시각화 분석 ◆
//                                        ◆ 성과 통계 분석 ◆
//                         ◆ 워크 포워드, 몬테카를로 시뮬레이션 등 고급 통계
//                         분석 ◆

// 파일 헤더
#include "Backtesting.hpp"

// 내부 헤더
#include "Engines/BaseBarHandler.hpp"
#include "Engines/BinanceFetcher.hpp"
#include "Engines/Config.hpp"
#include "Engines/Engine.hpp"
#include "Engines/Logger.hpp"
#include "Strategies/TestStrategy.hpp"

shared_ptr<Engine>& Backtesting::engine_ = Engine::GetEngine();
shared_ptr<Logger>& Backtesting::logger_ = Logger::GetLogger();
shared_ptr<BinanceFetcher> Backtesting::fetcher_ =
    make_shared<BinanceFetcher>();

int main() {
  Engine::AddBarData(
      "BTCUSDT", "D:/Programming/Backtesting/Data/Klines/BTCUSDT/1h.parquet",
      TRADING);

  Engine::AddBarData(
    "ETHUSDT", "D:/Programming/Backtesting/Data/Klines/ETHUSDT/1h.parquet",
    TRADING);
  Engine::AddBarData(
    "BTCUSDT", "D:/Programming/Backtesting/Data/Klines/BTCUSDT/1d.parquet",
    REFERENCE);

  Engine::AddBarData(
    "ETHUSDT", "D:/Programming/Backtesting/Data/Klines/ETHUSDT/1d.parquet",
    REFERENCE);
/*
  Engine::AddBarData(
    "XRPUSDT", "D:/Programming/Backtesting/Data/Klines/XRPUSDT/1h.parquet",
   TRADING);

  Engine::AddBarData(
      "APTUSDT", "D:/Programming/Backtesting/Data/Klines/APTUSDT/1h.parquet",
      TRADING);*/

  Engine::AddBarData(
      "BTCUSDT", "D:/Programming/Backtesting/Data/Klines/BTCUSDT/1m.parquet",
      BarType::MAGNIFIER);

  auto strategy = TestStrategy("Test Strategy");
  Backtesting::engine_->AddStrategy(strategy);

  Config config;
  config.SetRootDirectory("D:/Programming/Backtesting")
      .SetInitialBalance(10000)
      .SetCommissionType(CommissionType::COMMISSION_PERCENTAGE)
      .SetMarketCommission(0.04)
      .SetLimitCommission(0.03)
      .SetSlippageType(SlippageType::SLIPPAGE_PERCENTAGE)
      .SetMarketSlippage(0.1)
      .SetLimitSlippage(0);

  Backtesting::engine_->SetConfig(config);
  Backtesting::engine_->Backtesting(false);
}