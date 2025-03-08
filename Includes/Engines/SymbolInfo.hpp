#pragma once

// 표준 라이브러리
#include <vector>

// 네임 스페이스
using namespace std;

/// 하나의 레버리지 브라켓을 나타내는 구조
struct LeverageBracket {
  double min_notional_value;       // 해당 구간의 최소 명목 가치
  double max_notional_value;       // 해당 구간의 최대 명목 가치
  int max_leverage;                // 해당 구간의 최대 레버리지
  double maintenance_margin_rate;  // 해당 구간의 유지 마진율
  double maintenance_amount;       // 해당 구간의 유지 금액
};

/// 심볼의 거래 규칙을 포함하는 빌더 클래스
class SymbolInfo final {
 public:
  SymbolInfo();
  ~SymbolInfo();

  SymbolInfo& SetTickSize(double tick_size);
  SymbolInfo& SetLimitMaxQty(double default_max_qty);
  SymbolInfo& SetLimitMinQty(double default_min_qty);
  SymbolInfo& SetMarketMaxQty(double market_max_qty);
  SymbolInfo& SetMarketMinQty(double market_min_qty);
  SymbolInfo& SetQtyStep(double qty_step);
  SymbolInfo& SetMinNotional(double min_notional);
  SymbolInfo& SetMaxMultiplier(double max_multiplier);
  SymbolInfo& SetMinMultiplier(double min_multiplier);
  SymbolInfo& SetLiquidationFee(double liquidation_fee);
  SymbolInfo& SetLeverageBracket(
      const vector<LeverageBracket>& leverage_bracket);

  [[nodiscard]] double GetTickSize() const;
  [[nodiscard]] double GetLimitMaxQty() const;
  [[nodiscard]] double GetLimitMinQty() const;
  [[nodiscard]] double GetMarketMaxQty() const;
  [[nodiscard]] double GetMarketMinQty() const;
  [[nodiscard]] double GetQtyStep() const;
  [[nodiscard]] double GetMinNotional() const;
  [[nodiscard]] double GetMaxMultiplier() const;
  [[nodiscard]] double GetMinMultiplier() const;
  [[nodiscard]] double GetLiquidationFee() const;
  [[nodiscard]] vector<LeverageBracket>& GetLeverageBracket();

 private:
  double tick_size_;       // 틱 사이즈                 // 완
  double limit_max_qty_;   // 지정가 최대 수량       // 완
  double limit_min_qty_;   // 지정가 최소 수량        // 완
  double market_max_qty_;  // 시장가 최대 수량        // 완
  double market_min_qty_;  // 시장가 최소 수량        // 완
  double qty_step_;        // 수량 최소 단위          // 완
  double min_notional_;    // 최소 약정 금액           // 완
  double
      max_multiplier_;  // 현재가 대비 최대 주문 가격 비율     마크가격대비
                        // 지정가인듯
                        // https://www.binance.com/en/futures/trading-rules/perpetual
                        // GetCurrentMarkPrice 함수 만들어서 가격 대비 해서 리턴
  double min_multiplier_;   // 현재가 대비 최소 주문 가격 비율
  double liquidation_fee_;  // 강제 청산 수수료         // 완
  vector<LeverageBracket> leverage_brackets_;  // 레버리지 브라켓
};
