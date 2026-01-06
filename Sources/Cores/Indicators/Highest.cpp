// 파일 헤더
#include "Indicators/Highest.hpp"

// 내부 헤더
#include "Engines/Logger.hpp"

Highest::Highest(const string& name, const string& timeframe, const Plot& plot,
                 Indicator& source, const double period)
    : Indicator(name, timeframe, plot),
      source_(source),
      sizet_period_(static_cast<size_t>(period)),
      double_period_(period),
      count_(0),
      can_calculate_(false),
      current_idx_(0) {
  if (period <= 0) {
    Logger::LogAndThrowError(
        format("Highest 지표의 Period [{}]은(는) 0보다 커야 합니다.", period),
        __FILE__, __LINE__);
  }
}

void Highest::Initialize() {
  // 지표가 각 심볼별로 재초기화될 때 호출됨
  count_ = 0;
  can_calculate_ = false;
  dq_.clear();
  current_idx_ = 0;
}

Numeric<double> Highest::Calculate() {
  double value = source_[0];

  // 윈도우이 채워질 때까지 인덱스 증가 및 데크에 추가
  // deque는 값이 감소하는 순서로 유지 (앞에는 최대값)
  while (!dq_.empty() && dq_.back().first <= value) {
    dq_.pop_back();
  }

  dq_.emplace_back(value, current_idx_);

  if (!can_calculate_) {
    if (count_++ < sizet_period_ - 1) {
      current_idx_++;
      return NAN;
    }

    can_calculate_ = true;
  }

  // 윈도우에서 벗어난 요소 제거
  const size_t window_start_idx = current_idx_ + 1 - sizet_period_;
  while (!dq_.empty() && dq_.front().second < window_start_idx) {
    dq_.pop_front();
  }

  current_idx_++;
  return dq_.front().first;
}
