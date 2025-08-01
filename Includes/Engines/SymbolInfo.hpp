#pragma once

// 표준 라이브러리
#include <string>
#include <vector>

// 외부 라이브러리
#include <nlohmann/json.hpp>

// 네임 스페이스
using namespace std;
using namespace nlohmann;

namespace backtesting::order {

/// 하나의 레버리지 브라켓을 나타내는 구조
struct LeverageBracket {
  double min_notional_value;       // 해당 구간의 최소 명목 가치
  double max_notional_value;       // 해당 구간의 최대 명목 가치
  int max_leverage;                // 해당 구간의 최대 레버리지
  double maintenance_margin_rate;  // 해당 구간의 유지 마진율
  double maintenance_amount;       // 해당 구간의 유지 금액
};

// 하나의 펀딩 정보를 나타내는 구조체
struct FundingInfo {
  double funding_rate;   // 펀딩 비율
  int64_t funding_time;  // 펀딩 시간
  double mark_price;     // 펀딩 시 사용하는 마크 가격
};

/// 하나의 심볼의 정보를 포함하는 빌더 클래스
class SymbolInfo final {
 public:
  SymbolInfo();
  ~SymbolInfo();

  SymbolInfo& SetExchangeInfoPath(const string& exchange_info_path);
  SymbolInfo& SetTickSize(double tick_size);
  SymbolInfo& SetLimitMaxQty(double default_max_qty);
  SymbolInfo& SetLimitMinQty(double default_min_qty);
  SymbolInfo& SetMarketMaxQty(double market_max_qty);
  SymbolInfo& SetMarketMinQty(double market_min_qty);
  SymbolInfo& SetQtyStep(double qty_step);
  SymbolInfo& SetMinNotionalValue(double min_notional);
  SymbolInfo& SetLiquidationFeeRate(double liquidation_fee);

  SymbolInfo& SetLeverageBracketPath(const string& leverage_bracket_path);
  SymbolInfo& SetLeverageBracket(
      const vector<LeverageBracket>& leverage_bracket);

  SymbolInfo& SetFundingRatesPath(const string& funding_rates_path);
  SymbolInfo& SetFundingRates(const vector<FundingInfo>& funding_rates);

  // ===========================================================================
  [[nodiscard]] string GetExchangeInfoPath() const;
  [[nodiscard]] double GetTickSize() const;
  [[nodiscard]] double GetLimitMaxQty() const;
  [[nodiscard]] double GetLimitMinQty() const;
  [[nodiscard]] double GetMarketMaxQty() const;
  [[nodiscard]] double GetMarketMinQty() const;
  [[nodiscard]] double GetQtyStep() const;
  [[nodiscard]] double GetMinNotionalValue() const;
  [[nodiscard]] double GetLiquidationFeeRate() const;

  [[nodiscard]] string GetLeverageBracketPath() const;
  [[nodiscard]] vector<LeverageBracket>& GetLeverageBracket();

  [[nodiscard]] string GetFundingRatesPath() const;
  [[nodiscard]] vector<FundingInfo>& GetFundingRates();

 private:
  string exchange_info_path_;    // 거래소 정보 파일 경로
  double tick_size_;             // 틱 사이즈
  double limit_max_qty_;         // 지정가 최대 수량
  double limit_min_qty_;         // 지정가 최소 수량
  double market_max_qty_;        // 시장가 최대 수량
  double market_min_qty_;        // 시장가 최소 수량
  double qty_step_;              // 수량 최소 단위
  double min_notional_value_;    // 최소 명목 가치
  double liquidation_fee_rate_;  // 강제 청산 수수료율

  string leverage_bracket_path_;               // 레버리지 구간 파일 경로
  vector<LeverageBracket> leverage_brackets_;  // 레버리지 구간

  string funding_rates_path_;          // 펀딩 비율 파일 경로
  vector<FundingInfo> funding_rates_;  // 해당 심볼의 펀딩 정보
};

}  // namespace backtesting::order
