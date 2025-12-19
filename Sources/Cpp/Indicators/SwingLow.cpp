// 파일 헤더
#include "Indicators/SwingLow.hpp"

// 내부 헤더
#include "Engines/Logger.hpp"

SwingLow::SwingLow(const string& name, const string& timeframe,
                   const Plot& plot, const double period)
    : Indicator(name, timeframe, plot),
      symbol_idx_(-1),
      count_(0),
      can_calculate_(false),
      last_swing_low_(NAN) {
  if (period <= 0) {
    Logger::LogAndThrowError(
        format("SwingLow 지표의 Period [{}]은(는) 0보다 커야 합니다.", period),
        __FILE__, __LINE__);
  }

  period_ = static_cast<size_t>(period);
}

void SwingLow::Initialize() {
  // 지표가 각 심볼별로 재초기화될 때 호출됨
  reference_bar_ = bar_->GetBarData(REFERENCE, this->GetTimeframe());
  symbol_idx_ = bar_->GetCurrentSymbolIndex();
  count_ = 0;
  can_calculate_ = false;
  last_swing_low_ = NAN;
}

Numeric<double> SwingLow::Calculate() {
  // 기간만큼 데이터가 누적되지 않은 경우 NaN 반환
  if (!can_calculate_) {
    if (count_++ < period_ * 2) {
      return NAN;
    }

    can_calculate_ = true;
  }

  const size_t current_bar_idx = bar_->GetCurrentBarIndex();
  const size_t check_idx = current_bar_idx - period_;

  bool is_swing_low = true;
  const double center_low = reference_bar_->GetBar(symbol_idx_, check_idx).low;

  // 좌측과 우측 len개씩 비교
  for (size_t i = 1; i <= period_; i++) {
    const auto left_idx = check_idx - i;   // low[len - i]
    const auto right_idx = check_idx + i;  // low[len + i]

    // low[len + i] >= low[len] and low[len] <= low[len - i]가
    // 유지되지 않으면 Swing Low 갱신 실패
    const double left_low = reference_bar_->GetBar(symbol_idx_, left_idx).low;
    // ReSharper disable once CppTooWideScopeInitStatement
    const double right_low = reference_bar_->GetBar(symbol_idx_, right_idx).low;

    if (right_low < center_low || center_low > left_low) {
      is_swing_low = false;
      break;
    }
  }

  if (is_swing_low) {
    last_swing_low_ = center_low;
  }

  // 갱신되었다면 갱신된 값, 아니라면 전 바의 값이 반환됨
  return last_swing_low_;
}
