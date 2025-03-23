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
//  ● 다중 자산에 대한 고속, 정밀 백테스팅을 지원하는 프로그램 ●
//
//  ◆ 다중 자산 포트폴리오 백테스팅 ◆
//  ◆ 벡터화 내부 구조로 고속 백테스팅 ◆
//  ◆ 그래프 시각화 분석 ◆
//  ◆ 성과 통계 분석 ◆
//  ◆ 워크 포워드, 몬테카를로 시뮬레이션 등 고급 통계 분석 ◆
//
//  ==============================================================================================
//   ● 기본 작동 ●
//
//   ◆ 시간은 GMT 기준
//   ◆ 돋보기 < 트레이딩 <= 참조 바 데이터의 타임프레임은
//     부등호를 따르며, 배수 관계
//   ◆ 마크 가격 바 데이터 타임프레임은 돋보기 기능
//     사용 시 돋보기 바 데이터 타임프레임와 동일,
//     미사용 시 트레이딩 바 데이터 타임프레임과 동일
//   ◆ 각 바의 움직임은 봉 가정을 따름 → GetPriceQueue 함수 참조
//   ◆ 각 진입은 격리로 작동하며 단일 방향으로만 동시 진입 가능
//   ◆ 진입 및 청산 시 지정하는 포지션 크기는 레버리지 미포함 전체 크기
//   ◆ 초기 마진은 체결 가격 * 체결 크기 / 레버리지 + 진입 심볼의 미실현 손실
//   ◆ 레버리지는 해당 심볼에 체결된 포지션이 없을 때만 변경 가능
//     → AdjustLeverage 함수 참조
//   ◆ 진입 및 청산 세부 작동 구조는 OrderHandler 헤더 파일 참조
//   ◆ 미실현 손익 계산, 강제 청산 확인은 Mark Price를 사용하지만,
//     데이터가 누락된 경우 시장 가격을 사용함
//   ◆ 커스텀 전략 및 지표 생성 방법은 Strategy, Indicator 헤더 파일 참조
//  ==============================================================================================

// 파일 헤더
#include "Backtesting.hpp"

int main() {
  // 거래소 정책은 계속 변화하므로 매번 저장
  Backtesting::FetchExchangeInfo();
  Backtesting::FetchLeverageBracket();

  Backtesting::AddBarData(
      "BTCUSDT",
      "D:/Programming/Backtesting/Data/Continuous Klines/BTCUSDT/1h.parquet",
      TRADING);

  Backtesting::AddBarData(
      "XRPUSDT",
      "D:/Programming/Backtesting/Data/Continuous Klines/XRPUSDT/1h.parquet",
      TRADING);

  Backtesting::AddBarData(
      "APTUSDT",
      "D:/Programming/Backtesting/Data/Continuous Klines/APTUSDT/1h.parquet",
      TRADING);

  Backtesting::AddBarData(
      "BTCUSDT",
      "D:/Programming/Backtesting/Data/Mark Price Klines/BTCUSDT/1h.parquet",
      MARK_PRICE);

  Backtesting::AddBarData(
      "XRPUSDT",
      "D:/Programming/Backtesting/Data/Mark Price Klines/XRPUSDT/1h.parquet",
      MARK_PRICE);

  Backtesting::AddBarData(
      "APTUSDT",
      "D:/Programming/Backtesting/Data/Mark Price Klines/APTUSDT/1h.parquet",
      MARK_PRICE);

  Backtesting::AddExchangeInfo(
      "D:/Programming/Backtesting/Data/exchange_info.json");
  Backtesting::AddLeverageBracket(
      "D:/Programming/Backtesting/Data/leverage_bracket.json");

  Backtesting::AddStrategy<TestStrategy>("Test Strategy");

  Backtesting::SetConfig()
      .SetRootDirectory("D:/Programming/Backtesting")
      .SetUseBarMagnifier(false)
      .SetInitialBalance(10000)
      .SetTakerFeePercentage(0.045)
      .SetMakerFeePercentage(0.018)
      .SetTakerSlippagePercentage(0.1)
      .SetMakerSlippagePercentage(0);

  Backtesting::Run();
}