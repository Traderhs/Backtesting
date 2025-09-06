// 파일 헤더
#include "Indicators/Lowest.hpp"

Lowest::Lowest(const string& name, const string& timeframe, const Plot& plot,
               Indicator& source, const double period)
    : Indicator(name, timeframe, plot),
      source_(source),
      count_(0),
      can_calculate_(false),
      current_idx_(0) {
  double_period_ = period;
  sizet_period_ = static_cast<size_t>(period);
}

void Lowest::Initialize() {
  // 지표가 각 심볼별로 재초기화될 때 호출됨
  count_ = 0;
  can_calculate_ = false;
  dq_.clear();
  current_idx_ = 0;
}

Numeric<double> Lowest::Calculate() {
  double value = source_[0];

  // deque는 값이 증가하는 순서로 유지 (앞에는 최솟값)
  while (!dq_.empty() && dq_.back().first >= value) {
    dq_.pop_back();
  }

  dq_.emplace_back(value, current_idx_);

  if (!can_calculate_) {
    if (count_++ < sizet_period_ - 1) {
      current_idx_++;
      return nan("");
    }

    can_calculate_ = true;
  }

  const size_t window_start_idx = current_idx_ + 1 - sizet_period_;
  while (!dq_.empty() && dq_.front().second < window_start_idx) {
    dq_.pop_front();
  }

  current_idx_++;
  return dq_.front().first;
}
