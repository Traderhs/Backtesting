//
//  © 2024 Traderhs. All rights reserved.
//
//  ● 다중 심볼에 대한 고속, 정밀 백테스팅을 지원하는 프로그램 ●
//
//  ◆ 다중 자산 포트폴리오 백테스팅 ◆
//  ◆ 벡터화 내부 구조로 고속 백테스팅 ◆
//  ◆ 그래프 시각화 분석 ◆
//  ◆ 성과 통계 분석 ◆
//  ◆ 워크 포워드, 몬테카를로 시뮬레이션 등 고급 통계 분석 ◆
//
//  ============================================================================
//   ● 기본 작동 ●
//
//   ◆ 시간은 GMT 기준
//   ◆ 바 데이터 형식은 Parquet만 지원하며,
//     Open Time, Open, High, Low, Close, Volume, Close Time 열이 존재해야 함.
//   ◆ 타임프레임 길이는 돋보기 < 트레이딩 <= 참조 바 데이터의
//     부등호를 따르는 배수 관계
//   ◆ 마크 가격 바 데이터 타임프레임은 돋보기 기능을
//     사용 시 돋보기 바 데이터 타임프레임과 동일해야 하며,
//     미사용 시 트레이딩 바 데이터 타임프레임과 동일해야 함.
//   ◆ 각 바의 움직임은 봉 가정을 따름 → GetPriceQueue 함수 참조
//   ◆ 각 진입은 격리로 작동하며 단방향으로만 동시 진입 가능
//   ◆ 진입 및 청산 시 지정하는 포지션 크기는 레버리지 미포함 전체 크기
//   ◆ 초기 마진 = 체결 가격 * 체결 크기 / 레버리지 + 진입 심볼의 미실현 손실
//   ◆ 레버리지는 해당 심볼에 체결된 포지션이 없을 때만 변경 가능
//     → AdjustLeverage 함수 참조
//   ◆ 진입 및 청산 세부 작동 구조는 OrderHandler 헤더 파일 참조
//   ◆ 미실현 손익 계산, 강제 청산 확인은 Mark Price를 사용하지만,
//     데이터가 누락된 경우 시장 가격을 사용함
//   ◆ 커스텀 전략 및 지표 생성 방법은 Strategy, Indicator 헤더 파일 참조
//   ◆ 한 백테스팅에는 한 전략만 추가 가능하며, 백보드에서 전략 합성 기능 제공
//     → 각 전략마다 독립 계좌로 작동 가정
//   @TODO 추후 자동 매매 전략 가동 시, 코드 따라가며 사소해서 안 했던 거 모두
//         하고 (슬리피지 시뮬레이션 등), 엔진 최대한 최적화할 것.
//         또한 백보드에 분석 도구도 많이 추가할 것 (엔진과 연계해서라도)
//  ============================================================================

// 파일 헤더
#include "Backtesting.hpp"

// 내부 헤더
#include "Strategies/DiceSystem.hpp"

string Backtesting::market_data_directory_;
string Backtesting::api_key_env_var_;
string Backtesting::api_secret_env_var_;

int main() {
  // 거래소 정책은 계속 변화하므로 매번 저장
  Backtesting::SetApiEnvVars("BINANCE_API_KEY", "BINANCE_API_SECRET");
  Backtesting::SetMarketDataDirectory("D:/Programming/Backtesting/Data");
  Backtesting::FetchExchangeInfo();
  Backtesting::FetchLeverageBracket();
  //const vector<string>& symbol_list = {"BTCUSDT"};
  

  const vector<string>& symbol_list = {
    "BTCUSDT",  "APTUSDT", "ETHUSDT",  "BNBUSDT",  "SOLUSDT",
    "DOGEUSDT", "ADAUSDT", "AVAXUSDT", "DOTUSDT",  "XRPUSDT"};

  Backtesting::AddBarDataBatch(
      symbol_list, "1h", "D:/Programming/Backtesting/Data/Continuous Klines",
      TRADING);

  Backtesting::AddBarDataBatch(
      symbol_list, "1m", "D:/Programming/Backtesting/Data/Continuous Klines",
      MAGNIFIER);

  Backtesting::AddBarDataBatch(
      symbol_list, "1d", "D:/Programming/Backtesting/Data/Continuous Klines",
      REFERENCE);

  Backtesting::AddBarDataBatch(
      symbol_list, "1m", "D:/Programming/Backtesting/Data/Mark Price Klines",
      MARK_PRICE);

  Backtesting::AddExchangeInfo(
      "D:/Programming/Backtesting/Data/exchange_info.json");
  Backtesting::AddLeverageBracket(
      "D:/Programming/Backtesting/Data/leverage_bracket.json");

  Backtesting::AddFundingRates(symbol_list,
                               "D:/Programming/Backtesting/Data/Funding Rates");

  Backtesting::SetConfig()
      .SetRootDirectory("D:/Programming/Backtesting")
      .SetBacktestPeriod()
      .SetUseBarMagnifier(true)
      .SetInitialBalance(10000)
      .SetTakerFeePercentage(0.045)
      .SetMakerFeePercentage(0.018)
      .SetTakerSlippagePercentage(0.1)
      .SetMakerSlippagePercentage(0);

  Backtesting::AddStrategy<DiceSystem>("Dice System");

  Backtesting::Run();
}