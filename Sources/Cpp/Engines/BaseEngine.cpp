// 표준 라이브러리
#include <format>

// 파일 헤더
#include "Engines\BaseEngine.hpp"

// 내부 헤더
#include "Engines\BarHandler.hpp"
#include "Engines\Strategy.hpp"

BaseEngine::BaseEngine(const Config& config)
    : config_(config),
      wallet_balance_(config.GetInitialBalance()),
      available_balance_(config.GetInitialBalance()),
      max_wallet_balance_(config.GetInitialBalance()),
      min_wallet_balance_(config.GetInitialBalance()),
      drawdown_(0),
      max_drawdown_(0),
      liquidations_(0) {}
BaseEngine::~BaseEngine() = default;

shared_ptr<BarHandler>& BaseEngine::bar_ = BarHandler::GetBarHandler();
shared_ptr<Logger>& BaseEngine::logger_ = Logger::GetLogger();

void BaseEngine::AddBarData(const string& symbol_name, const string& file_path,
                            const BarType bar_type,
                            const vector<int>& columns) {
  bar_->AddBarData(symbol_name, file_path, bar_type, columns);
}

void BaseEngine::AddStrategy(const shared_ptr<Strategy>& strategy) {
  strategies_.push_back(strategy);
}

void BaseEngine::IncreaseWalletBalance(const double increase_balance) {
  if (increase_balance <= 0) {
    logger_->Log(
        LogLevel::WARNING_L,
        format("현재 자금 증가를 위해 주어진 값 {}은(는) 0보다 커야합니다.",
               increase_balance),
        __FILE__, __LINE__);
    return;
  }

  // Wallet이 증가하면 주문 가능 자금도 영향을 받기 때문에 동시 처리
  wallet_balance_ += increase_balance;
  available_balance_ += increase_balance;
}

void BaseEngine::DecreaseWalletBalance(const double decrease_balance) {
  if (decrease_balance <= 0 || decrease_balance > wallet_balance_) {
    logger_->Log(
        LogLevel::WARNING_L,
        format("현재 자금 감소를 위해 주어진 값 {}은(는) 양수로 지정되어야 "
               "하며, 현재 자금 {}을(를) 초과할 수 없습니다.",
               decrease_balance, wallet_balance_),
        __FILE__, __LINE__);
  }

  // Wallet이 감소하면 주문 가능 자금도 영향을 받기 때문에 동시 처리
  wallet_balance_ -= decrease_balance;
  available_balance_ -= decrease_balance;
}

void BaseEngine::IncreaseAvailableBalance(const double increase_balance) {
  if (increase_balance <= 0) {
    logger_->Log(
        LogLevel::WARNING_L,
        format(
            "주문 가능 자금 증가를 위해 주어진 값 {}은(는) 0보다 커야합니다.",
            increase_balance),
        __FILE__, __LINE__);
    return;
  }

  available_balance_ += increase_balance;
}

/// 주문 가능 자금을 감소시키는 함수
void BaseEngine::DecreaseAvailableBalance(double decrease_balance) {
  if (decrease_balance <= 0 || decrease_balance > available_balance_) {
    logger_->Log(
        LogLevel::WARNING_L,
        format(
            "주문 가능 자금 감소를 위해 주어진 값 {}은(는) 양수로 지정되어야 "
            "하며, 주문 가능 자금 {}을(를) 초과할 수 없습니다.",
            decrease_balance, available_balance_),
        __FILE__, __LINE__);
  }

  available_balance_ -= decrease_balance;
}

Config BaseEngine::GetConfig() const { return config_; }

double BaseEngine::GetCurrentBalance() const { return wallet_balance_; }

double BaseEngine::GetAvailableBalance() const { return available_balance_; }

double BaseEngine::GetTickSize(const int symbol_idx) const {
  if (const auto size = tick_size_.size();
      symbol_idx < 0 || size <= symbol_idx) {
    Logger::LogAndThrowError(format("심볼 인덱스가 0보다 작거나 최대값 {}를 "
                                    "초과했습니다. | 지정된 인덱스: {}",
                                    size - 1, symbol_idx),
                             __FILE__, __LINE__);
  }

  return tick_size_[symbol_idx];
}
