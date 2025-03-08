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
//
//  ==============================================================================================
//                  ● 기본 작동 ●
//                 ◆ 시간은 UTC+0 기준
//                 ◆ 돋보기 -> 트레이딩 -> 참조 바 데이터의 타임프레임은
//                   배수 관계
//                ◆ 각 진입은 격리로 작동하며 단일 방향으로만 진입 가능
//                ◆ 각 바의 움직임은 봉 가정을 따름 -> GetPriceQueue 함수 참조
//                ◆ 진입 및 청산 작동 구조는 OrderHandler 헤더 파일 참조
//                ◆ 미실현 손익 계산, 강제 청산 확인은 Mark Price를 사용하지만,
//                  데이터가 누락된 경우 시장 가격을 사용함
//
//  ==============================================================================================

// 파일 헤더
#include "Backtesting.hpp"

// 내부 헤더
#include "Engines/BaseBarHandler.hpp"
#include "Engines/BinanceFetcher.hpp"
#include "Engines/Config.hpp"
#include "Engines/Engine.hpp"
#include "Engines/Logger.hpp"
#include "Strategies/TestStrategy.hpp"

// 네임 스페이스
using enum BarType;

auto& engine = Engine::GetEngine();
auto& logger = Logger::GetLogger();
auto fetcher =
    make_shared<BinanceFetcher>("BINANCE_API_KEY", "BINANCE_API_SECRET");

int main() {
  // 거래소 정책은 계속 변화하므로 매번 저장
  fetcher->FetchExchangeInfo();
  fetcher->FetchLeverageBracket();

  Engine::AddBarData(
      "BTCUSDT",
      "D:/Programming/Backtesting/Data/Continuous Klines/BTCUSDT/1h.parquet",
      TRADING);

  Engine::AddBarData(
      "XRPUSDT",
      "D:/Programming/Backtesting/Data/Continuous Klines/XRPUSDT/1h.parquet",
      TRADING);

  Engine::AddBarData(
      "BTCUSDT",
      "D:/Programming/Backtesting/Data/Mark Price Klines/BTCUSDT/1h.parquet",
      MARK_PRICE);

  Engine::AddBarData(
      "XRPUSDT",
      "D:/Programming/Backtesting/Data/Mark Price Klines/XRPUSDT/1h.parquet",
      MARK_PRICE);

  Engine::AddExchangeInfo("D:/Programming/Backtesting/Data/exchange_info.json");
  Engine::AddLeverageBracket(
      "D:/Programming/Backtesting/Data/leverage_bracket.json");

  Strategy::AddStrategy<TestStrategy>("Test Strategy");

  Config::SetConfig()
      .SetRootDirectory("D:/Programming/Backtesting")
      .SetUseBarMagnifier(false)
      .SetInitialBalance(10000)
      .SetTakerFee(0.045)
      .SetMakerFee(0.018)
      .SetTakerSlippage(0.1)
      .SetMakerSlippage(0);

  engine->Backtesting();
}