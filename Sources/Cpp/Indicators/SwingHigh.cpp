// 파일 헤더
#include "Indicators/SwingHigh.hpp"

// 내부 헤더
#include "Engines/Logger.hpp"

SwingHigh::SwingHigh(const string& name, const string& timeframe,
                     const Plot& plot, const double period)
    : Indicator(name, timeframe, plot),
      symbol_idx_(-1),
      count_(0),
      can_calculate_(false),
      last_swing_high_(NAN) {
  if (period <= 0) {
    Logger::LogAndThrowError(
        format("SwingHigh 지표의 Period [{}]은(는) 0보다 커야 합니다.", period),
        __FILE__, __LINE__);
  }

  period_ = static_cast<size_t>(period);
}

void SwingHigh::Initialize() {
  // 지표가 각 심볼별로 재초기화될 때 호출됨
  reference_bar_ = bar_->GetBarData(REFERENCE, this->GetTimeframe());
  symbol_idx_ = bar_->GetCurrentSymbolIndex();
  count_ = 0;
  can_calculate_ = false;
  last_swing_high_ = NAN;
}

Numeric<double> SwingHigh::Calculate() {
  // 기간만큼 데이터가 누적되지 않은 경우 NaN 반환
  if (!can_calculate_) {
    if (count_++ < period_ * 2) {
      return NAN;
    }

    can_calculate_ = true;
  }

  const size_t current_bar_idx = bar_->GetCurrentBarIndex();
  const size_t check_idx = current_bar_idx - period_;

  bool is_swing_high = true;
  const double center_high =
      reference_bar_->GetBar(symbol_idx_, check_idx).high;

  // 좌측과 우측 len개씩 비교
  for (int i = 1; i <= period_; i++) {
    const int left_idx = check_idx - i;   // high[len - i]
    const int right_idx = check_idx + i;  // high[len + i]

    // high[len + i] <= high[len] and high[len] >= high[len - i]가
    // 유지되지 않으면 Swing High 갱신 실패
    const double left_high = reference_bar_->GetBar(symbol_idx_, left_idx).high;
    const double right_high =
        reference_bar_->GetBar(symbol_idx_, right_idx).high;

    if (right_high > center_high || center_high < left_high) {
      is_swing_high = false;
      break;
    }
  }

  if (is_swing_high) {
    last_swing_high_ = center_high;
  }

  // 갱신되었다면 갱신된 값, 아니라면 전 바의 값이 반환됨
  return last_swing_high_;
}
