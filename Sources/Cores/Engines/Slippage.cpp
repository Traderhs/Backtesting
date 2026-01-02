// 표준 라이브러리
#include <filesystem>
#include <format>

// 파일 헤더
#include "Engines/Slippage.hpp"

// 내부 헤더
#include "Engines/BarData.hpp"
#include "Engines/BarHandler.hpp"
#include "Engines/Config.hpp"
#include "Engines/DataUtils.hpp"
#include "Engines/Engine.hpp"
#include "Engines/Numeric.hpp"
#include "Engines/Order.hpp"
#include "Engines/SymbolInfo.hpp"
#include "Engines/TimeUtils.hpp"

namespace backtesting::order {

using namespace bar;
using namespace engine;
using namespace numeric;
using namespace utils;

vector<SymbolInfo> Slippage::symbol_info_;
shared_ptr<BarHandler>& MarketImpactSlippage::bar_ =
    BarHandler::GetBarHandler();
shared_ptr<Config>& MarketImpactSlippage::config_ = Engine::GetConfig();

void Slippage::SetSymbolInfo(const vector<SymbolInfo>& symbol_info) {
  if (symbol_info_.empty()) {
    symbol_info_ = symbol_info;
  } else [[unlikely]] {
    Logger::LogAndThrowError(
        "심볼 정보가 이미 초기화되어 다시 초기화할 수 없습니다.", __FILE__,
        __LINE__);
  }
}

// =============================================================================
optional<string> PercentageSlippage::ValidateTakerSlippage() const {
  // 테이커 슬리피지율이 NaN이면 유효하지 않음
  if (isnan(taker_slippage_ratio_)) {
    return "테이커 슬리피지 퍼센트는 NaN으로 설정할 수 없습니다.";
  }

  // 테이커 슬리피지율이 0~100% 범위를 벗어나면 유효하지 않음
  if (IsGreater(taker_slippage_ratio_, 100.0) ||
      IsLess(taker_slippage_ratio_, 0.0)) {
    return format(
        "지정된 테이커 슬리피지 퍼센트 [{}%]는 100% 초과 혹은 "
        "0% 미만으로 설정할 수 없습니다.",
        taker_slippage_ratio_ * 100);
  }

  return nullopt;
}

optional<string> PercentageSlippage::ValidateMakerSlippage() const {
  // 메이커 슬리피지율이 NaN이면 유효하지 않음
  if (isnan(maker_slippage_ratio_)) {
    return "메이커 슬리피지 퍼센트는 NaN으로 설정할 수 없습니다.";
  }

  // 메이커 슬리피지율이 0~100% 범위를 벗어나면 유효하지 않음
  if (IsGreater(maker_slippage_ratio_, 100.0) ||
      IsLess(maker_slippage_ratio_, 0.0)) {
    return format(
        "지정된 메이커 슬리피지 퍼센트 [{}%]는 100% 초과 혹은 "
        "0% 미만으로 설정할 수 없습니다.",
        maker_slippage_ratio_ * 100);
  }

  return nullopt;
}

double PercentageSlippage::CalculateSlippagePrice(
    const OrderType order_type, const Direction direction,
    const double order_price,
    const double order_size, /* 퍼센트 슬리피지에서는 사용하지 않는 매개변수지만
                                가상 함수 오버라이드를 위해 존재 */
    const int symbol_idx) const {
  // 주문 타입에 따라 슬리피지율 선택
  const double slippage_ratio =
      order_type == MARKET || order_type == MIT || order_type == TRAILING
          ? taker_slippage_ratio_
          : maker_slippage_ratio_;

  // 슬리피지율이 0이면 원래 가격 반환
  if (IsEqual(slippage_ratio, 0.0)) {
    return order_price;
  }

  // 방향에 따라 슬리피지 적용
  // 매수 진입: 가격이 올라가므로 불리함
  // 매도 진입: 가격이 내려가므로 불리함
  const double slippage_price = direction == LONG
                                    ? order_price * (1.0 + slippage_ratio)
                                    : order_price * (1.0 - slippage_ratio);

  // 가격 단위로 반올림
  return RoundToStep(slippage_price, symbol_info_[symbol_idx].GetPriceStep());
}

// =============================================================================
optional<string> MarketImpactSlippage::ValidateTakerSlippage() const {
  if (impact_coefficient_ < 0.0) {
    return "시장 충격 계수는 음수일 수 없습니다.";
  }

  if (rolling_window_ <= 0) {
    return "롤링 윈도우는 양수여야 합니다.";
  }

  if (tick_floor_bps_ < 0.0) {
    return "틱 플로어는 음수일 수 없습니다.";
  }

  // 스트레스 계수가 NaN이면 유효하지 않음
  if (isnan(stress_multiplier_)) {
    return "슬리피지 스트레스 계수는 NaN으로 설정할 수 없습니다.";
  }

  if (IsLess(stress_multiplier_, 0.0)) {
    return format(
        "지정된 슬리피지 스트레스 계수 [{}배]는 음수로 설정할 수 없습니다.",
        stress_multiplier_);
  }

  return nullopt;
}

optional<string> MarketImpactSlippage::ValidateMakerSlippage() const {
  // 메이커 주문도 동일한 검증
  return ValidateTakerSlippage();
}

void MarketImpactSlippage::Initialize() {
  // 타임프레임 높낮이 측정
  const auto parsed_15_min_tf = ParseTimeframe("15m");
  const auto& trading_bar_data = bar_->GetBarData(TRADING, "");
  const auto& magnifier_bar_data = bar_->GetBarData(MAGNIFIER, "");
  is_trading_low_tf_ =
      ParseTimeframe(trading_bar_data->GetTimeframe()) <= parsed_15_min_tf;

  // 돋보기 기능 미사용 시 Timeframe은 Empty이므로 false로 처리
  // 돋보기 기능 미사용 시 어차피 미사용 변수이므로 false 처리하는 것
  const auto& magnifier_tf = magnifier_bar_data->GetTimeframe();
  is_magnifier_low_tf_ = magnifier_tf.empty()
                             ? false
                             : ParseTimeframe(magnifier_tf) <= parsed_15_min_tf;

  // 심볼별 스프레드 저장 벡터 초기화
  previous_spread_bps_.resize(trading_bar_data->GetNumSymbols(), 0.0);
}

double MarketImpactSlippage::CalculateSlippagePrice(
    const OrderType order_type, const Direction direction,
    const double order_price, const double order_size,
    const int symbol_idx) const {
  // 지정가 주문은 슬리피지 없음
  if (order_type == LIMIT || order_type == LIT) {
    return order_price;
  }

  // 정보 로딩
  const auto& bar_data = bar_->GetBarData(bar_->GetCurrentBarDataType(),
                                          bar_->GetCurrentReferenceTimeframe());
  const auto bar_idx = bar_->GetCurrentBarIndex();
  const auto& bar = bar_data->GetBar(symbol_idx, bar_idx);
  const auto price_step = symbol_info_[symbol_idx].GetPriceStep();

  // 심볼별 틱 사이즈를 틱 플로어로 설정 (bps 변환)
  const double hi = max(bar.high, epsilon_);
  const double lo = max(bar.low, epsilon_);

  // px_ref: 로그-미드레인지 ≒ 효율가 근사 (Abdi & Ranaldo, 2017)
  // exp((lnH+lnL)/2) = sqrt(H*L)
  const double px_ref = max(sqrt(hi * lo), epsilon_);

  // 1 tick in bps
  const double tick_bps = price_step / px_ref * 1e4;

  // 하한 설정: 최소 스프레드 틱수 × tick_bps  vs  아주 작은 bps epsilon
  // 권장 기본: min_full_spread_ticks = 1.0, min_bps_floor = 0.01 bps
  constexpr double min_full_spread_ticks = 1.0;  // 알트가 자주 2틱↑면 1.5~2.0로
  constexpr double min_bps_floor = 0.01;         // 수치적 안정화용 아주 작게

  tick_floor_bps_ = max(tick_bps * min_full_spread_ticks, min_bps_floor);

  // 바 인덱스 유효성 검사 후 데이터 부족 시 폴백
  if (bar_idx < rolling_window_) {
    const double slippage_ratio = tick_floor_bps_ / 2.0 / 10000.0;

    return direction == LONG ? order_price * (1.0 + slippage_ratio)
                             : order_price * (1.0 - slippage_ratio);
  }

  // 1. 스프레드 추정 (bps)
  double spread_raw = EstimateSpreadEdge(symbol_idx, bar_idx, bar_data);
  spread_raw = SanitizeValue(spread_raw, tick_floor_bps_);
  if (spread_raw < tick_floor_bps_) {
    spread_raw = tick_floor_bps_;
  }

  double spread_smoothed = spread_raw;

  // 고빈도 타임프레임(≤15분)에만 EMA 스무딩 적용
  // 학술적 근거: 1m~15m은 마이크로스트럭처 노이즈가 크므로 평활화 필요,
  // 30m 이상은 이미 자연 평활화되어 있어 불필요
  const bool apply_ema_smoothing = bar_->GetCurrentBarDataType() == TRADING
                                       ? is_trading_low_tf_
                                       : is_magnifier_low_tf_;

  if (apply_ema_smoothing && previous_spread_bps_[symbol_idx] > 0.0) {
    // EMA: spread_smoothed = α * spread_current + (1-α) * spread_previous
    spread_smoothed =
        spread_ema_alpha_ * spread_raw +
        (1.0 - spread_ema_alpha_) * previous_spread_bps_[symbol_idx];

    // EMA 이후 최종 바닥 재클램핑
    spread_smoothed = max(spread_smoothed, tick_floor_bps_);
  }

  // 클램핑된 값을 이전값으로 저장
  previous_spread_bps_[symbol_idx] = spread_smoothed;

  // 2. 변동성 추정
  double volatility =
      EstimateVolatilityGarmanKlass(symbol_idx, bar_idx, bar_data);
  volatility = SanitizeValue(volatility, 0.0);

  // 3. 롤링 거래량
  const double rolling_volume =
      CalculateRollingVolume(symbol_idx, bar_idx, bar_data);
  const double safe_volume = max(rolling_volume, epsilon_);

  // 4. 시장 충격 계산: k * σ * (Q/V)^β
  // PR 캡: 극저유동성 구간에서 Q/V 폭주 방지
  double participation_rate = order_size / safe_volume;
  participation_rate = min(participation_rate, participation_rate_cap_);

  const double market_impact_bps = impact_coefficient_ * volatility *
                                   pow(participation_rate, impact_exponent_) *
                                   10000.0;

  // 5. 총 슬리피지 (bps): spread/2 + market_impact
  const double total_slippage_bps =
      spread_smoothed * 0.5 + SanitizeValue(market_impact_bps, 0.0);

  // bps → tick 변환
  constexpr double eps_tick = 1e-12;

  // tick_bps는 위에서 계산한 값 사용: tick_bps = (price_step / px_ref) * 1e4;
  const double slip_ticks_theoretical = total_slippage_bps / tick_bps;

  // 불리한 정수 틱수 and 스트레스 테스트 계수
  const double slip_ticks_adverse =
      ceil(slip_ticks_theoretical - eps_tick) * stress_multiplier_;

  // 6. 최종 체결가 = 주문가 ± (정수 틱수 × tick size)
  double slippage_price;
  if (direction == LONG) {
    slippage_price = order_price + slip_ticks_adverse * price_step;
  } else {
    slippage_price = order_price - slip_ticks_adverse * price_step;
  }

  // 가격 단위로 반올림
  return RoundToStep(slippage_price, price_step);
}

// =============================================================================
// 스프레드 추정 메서드들
// =============================================================================
double MarketImpactSlippage::EstimateSpreadEdge(
    const int symbol_idx, const size_t bar_idx,
    const shared_ptr<BarData>& bar_data) const {
  // === EDGE(Ardia-Guidotti-Kröncke, 2024) 정확 구현 ===
  // - 본 함수는 논문 식 (11)과 (A.18)을 따른다.
  // - X1, X2는 식 (12) 정의 사용, 가중치 w1,w2는 식 (13)–(14) 사용.
  // - 선택빈도 ν_{o=h,l}, ν_{c=h,l}는 표 정의대로 각각 평균[(o=h),(o=l)],
  //   평균[(c=h),(c=l)].
  // - 무체결 바( h_t = l_t = c_{t-1} )는 제외.
  const size_t start_idx = bar_idx - rolling_window_ + 1;

  // (1) 로그가격 전처리 + 극값 선택빈도 집계
  int valid_count = 0;
  int nu_o_h = 0, nu_o_l = 0, nu_c_h = 0, nu_c_l = 0;

  vector<double> log_open_vec, log_high_vec, log_low_vec, log_close_vec,
      log_eta_vec;
  log_open_vec.reserve(rolling_window_);
  log_high_vec.reserve(rolling_window_);
  log_low_vec.reserve(rolling_window_);
  log_close_vec.reserve(rolling_window_);
  log_eta_vec.reserve(rolling_window_);

  double prev_close_log = 0.0;
  bool has_prev = false;

  for (size_t idx = start_idx; idx <= bar_idx; ++idx) {
    constexpr double tol_rel = 1e-8;
    constexpr double tol_abs = 1e-12;

    const auto& bar = bar_data->GetBar(symbol_idx, idx);
    const double open = bar.open, high = bar.high, low = bar.low,
                 close = bar.close;

    // 바들은 전부 유효하다고 가정

    // 무체결 바 필터: H==L==C_{t-1} (허용오차 포함)
    if (has_prev && abs(high - low) < tol_abs) {
      const double c_prev = exp(prev_close_log);

      if (const double tol = tol_abs + tol_rel * c_prev;
          abs(high - c_prev) < tol && abs(low - c_prev) < tol) {
        continue;
      }
    }

    // 로그변환 및 η_t = (h_t + l_t)/2 (로그공간)
    const double log_open = log(open);
    const double log_high = log(high);
    const double log_low = log(low);
    const double log_close = log(close);
    const double log_eta = (log_high + log_low) / 2.0;

    log_open_vec.push_back(log_open);
    log_high_vec.push_back(log_high);
    log_low_vec.push_back(log_low);
    log_close_vec.push_back(log_close);
    log_eta_vec.push_back(log_eta);

    // 선택빈도 집계: O/C가 H/L에 위치한 경우
    const double tol = tol_abs + tol_rel * abs(log_high);
    if (abs(log_open - log_high) < tol) {
      ++nu_o_h;
    } else if (abs(log_open - log_low) < tol) {
      ++nu_o_l;
    }

    if (abs(log_close - log_high) < tol) {
      ++nu_c_h;
    } else if (abs(log_close - log_low) < tol) {
      ++nu_c_l;
    }

    ++valid_count;
    prev_close_log = log_close;
    has_prev = true;
  }

  // 유효 바 수 부족 → CS 폴백
  if (valid_count < rolling_window_ / 2) {
    return EstimateSpreadCorwinSchultz(symbol_idx, bar_idx, bar_data);
  }

  // (2) 선택빈도 ν 계산 (표 정의: 평균값)
  const double nu_o_hl =
      static_cast<double>(nu_o_h + nu_o_l) / (2.0 * valid_count);
  const double nu_c_hl =
      static_cast<double>(nu_c_h + nu_c_l) / (2.0 * valid_count);

  if (nu_o_hl < epsilon_ && nu_c_hl < epsilon_) {
    return tick_floor_bps_;  // 극단 선택이 거의 없으면 틱 플로어
  }

  // (3) X1, X2 시계열 산출 (식 12), 무체결 바 제외했으므로 t>=1부터
  double sum_x1 = 0.0, sum_x2 = 0.0;
  double sum_x1_sq = 0.0, sum_x2_sq = 0.0;
  int x_count = 0;

  for (int t = 1; t < valid_count; ++t) {
    const double eta_t = log_eta_vec[t];
    const double open_t = log_open_vec[t];
    const double close_tm1 = log_close_vec[t - 1];
    const double eta_tm1 = log_eta_vec[t - 1];

    const double x1_t = (eta_t - open_t) * (open_t - close_tm1) +
                        (open_t - close_tm1) * (close_tm1 - eta_tm1);
    const double x2_t = (eta_t - open_t) * (open_t - eta_tm1) +
                        (eta_t - close_tm1) * (close_tm1 - eta_tm1);

    sum_x1 += x1_t;
    sum_x2 += x2_t;
    sum_x1_sq += x1_t * x1_t;
    sum_x2_sq += x2_t * x2_t;
    ++x_count;
  }

  // 폴백
  if (x_count < 2) {
    return EstimateSpreadCorwinSchultz(symbol_idx, bar_idx, bar_data);
  }

  const double mean_x1 = sum_x1 / x_count;
  const double mean_x2 = sum_x2 / x_count;

  // (4) 가중치 w1,w2 (식 13–14: 역분산 정규화)
  const double var_x1 =
      (sum_x1_sq - x_count * mean_x1 * mean_x1) / (x_count - 1.0 + epsilon_);
  const double var_x2 =
      (sum_x2_sq - x_count * mean_x2 * mean_x2) / (x_count - 1.0 + epsilon_);

  double w1, w2;
  if (const double var_sum = var_x1 + var_x2;
      var_sum > epsilon_ && var_x1 > 0.0 && var_x2 > 0.0) {
    w1 = var_x2 / var_sum;  // = (1/σ1^2) / ((1/σ1^2)+(1/σ2^2))
    w2 = var_x1 / var_sum;
  } else {
    w1 = w2 = 0.5;
  }

  // (6) EDGE 본식 (식 11) 시도, 불안정하면 (A.18) 폴백
  const double den = w1 * w2 * (nu_o_hl + nu_c_hl) - 0.5;
  double s_sq;
  if (den > epsilon_) {
    // Eq. (11): S^2 = (w1 E[X1] + w2 E[X2]) / (w1 w2 (ν_o=h,l + ν_c=h,l) - 1/2)
    s_sq = (w1 * mean_x1 + w2 * mean_x2) / den;
  } else {
    // Eq. (A.18): S^2 = -2 (w1 E[X1] + w2 E[X2]) / (1 - k ν_{o,c=h,l}),  k = 4
    // w1 w2
    const double nu_avg = 0.5 * (nu_o_hl + nu_c_hl);  // ν_{o,c=h,l}
    const double hat = 4.0 * w1 * w2;                 // Appendix A.3.3
    const double den_alt = 1.0 - hat * nu_avg;        // (A.18)
    s_sq = abs(den_alt) > epsilon_
               ? -2.0 * (w1 * mean_x1 + w2 * mean_x2) / den_alt
               : 0.0;
  }

  const double s = sqrt(s_sq);      // 상대 스프레드
  double spread_bps = s * 10000.0;  // bps 변환

  spread_bps = max(spread_bps, tick_floor_bps_);  // 틱 플로어
  if (!isfinite(spread_bps)) {
    spread_bps = tick_floor_bps_;
  }
  return spread_bps;
}

double MarketImpactSlippage::EstimateSpreadCorwinSchultz(
    const int symbol_idx, const size_t bar_idx,
    const shared_ptr<BarData>& bar_data) const {
  // Corwin-Schultz (2012) 2바 고저가 비율 기반 스프레드 추정
  // 정확한 공식: β = ln²(H/L), γ = ln²(Hmax/Lmin) 사용
  const auto& previous_bar = bar_data->GetBar(symbol_idx, bar_idx - 1);
  const auto& current_bar = bar_data->GetBar(symbol_idx, bar_idx);

  const double h1 = previous_bar.high;
  const double l1 = previous_bar.low;
  const double h2 = current_bar.high;
  const double l2 = current_bar.low;

  if (h1 <= epsilon_ || l1 <= epsilon_ || h2 <= epsilon_ || l2 <= epsilon_) {
    return tick_floor_bps_;
  }

  // β = [ln²(H₁/L₁) + ln²(H₂/L₂)]
  const double ln_hl1 = log(h1 / l1);
  const double ln_hl2 = log(h2 / l2);
  const double beta =
      ln_hl1 * ln_hl1 + ln_hl2 * ln_hl2;  // 합 버전 (일관성 유지)

  // γ = ln²(Hmax/Lmin)
  const double h_max = max(h1, h2);
  const double l_min = min(l1, l2);
  if (h_max <= epsilon_ || l_min <= epsilon_) {
    return tick_floor_bps_;
  }

  const double ln_hmax_lmin = log(h_max / l_min);
  const double gamma = ln_hmax_lmin * ln_hmax_lmin;

  // α = [√(2β) - √β] / (3-2√2) - √(γ / (3-2√2))
  const double sqrt_2 = sqrt(2.0);
  const double denom = 3.0 - 2.0 * sqrt_2;
  const double alpha =
      (sqrt(2.0 * beta) - sqrt(beta)) / denom - sqrt(gamma / denom);

  if (alpha <= 0.0) {
    return tick_floor_bps_;
  }

  // S = 2(e^α - 1) / (1 + e^α) (상대 스프레드 = (ask-bid)/mid)
  const double exp_alpha = exp(alpha);
  const double spread = 2.0 * (exp_alpha - 1.0) / (1.0 + exp_alpha);
  const double spread_bps = spread * 10000.0;

  return max(SanitizeValue(spread_bps, tick_floor_bps_), tick_floor_bps_);
}

double MarketImpactSlippage::EstimateVolatilityGarmanKlass(
    const int symbol_idx, const size_t bar_idx,
    const shared_ptr<BarData>& bar_data) const {
  // Garman-Klass (1980) 변동성 추정
  double sum_gk = 0.0;
  int valid_count = 0;

  for (size_t idx = bar_idx - rolling_window_ + 1; idx <= bar_idx; ++idx) {
    const auto& bar = bar_data->GetBar(symbol_idx, idx);
    const double open = bar.open;
    const double high = bar.high;
    const double low = bar.low;
    const double close = bar.close;

    if (open <= epsilon_ || high <= epsilon_ || low <= epsilon_ ||
        close <= epsilon_) {
      continue;
    }

    const double hl_ratio = log(high / low);
    const double co_ratio = log(close / open);

    const double gk_term = 0.5 * hl_ratio * hl_ratio -
                           (2.0 * log(2.0) - 1.0) * co_ratio * co_ratio;

    sum_gk += gk_term;
    valid_count++;
  }

  if (valid_count == 0) {
    return 0.0;
  }

  const double variance = sum_gk / valid_count;
  return sqrt(max(variance, 0.0));
}

double MarketImpactSlippage::CalculateRollingVolume(
    const int symbol_idx, const size_t bar_idx,
    const shared_ptr<BarData>& bar_data) const {
  double volume_sum = 0.0;

  for (size_t idx = bar_idx - rolling_window_ + 1; idx <= bar_idx; ++idx) {
    const auto& bar = bar_data->GetBar(symbol_idx, idx);
    volume_sum += bar.volume;
  }

  return volume_sum;
}

}  // namespace backtesting::order
