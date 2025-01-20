// 표준 라이브러리
#include <format>

// 파일 헤더
#include "Engines/BaseEngine.hpp"

// 내부 헤더
#include "Engines/BarHandler.hpp"
#include "Engines/Strategy.hpp"

BaseEngine::BaseEngine()
    : debug_mode_(false),
      wallet_balance_(-1),
      available_balance_(-1),
      max_wallet_balance_(-1),
      min_wallet_balance_(-1),
      drawdown_(0),
      max_drawdown_(0),
      liquidations_(0) {}
BaseEngine::~BaseEngine() = default;

shared_ptr<BarHandler>& BaseEngine::bar_ = BarHandler::GetBarHandler();
shared_ptr<Logger>& BaseEngine::logger_ = Logger::GetLogger();

void BaseEngine::SetDebugMode() { debug_mode_ = true; }

void BaseEngine::AddBarData(const string& symbol_name, const string& file_path,
                            const BarType bar_type,
                            const vector<int>& columns) {
  bar_->AddBarData(symbol_name, file_path, bar_type, columns);
}

void BaseEngine::AddStrategy(const shared_ptr<Strategy>& strategy) {
  strategies_.push_back(strategy);

  logger_->Log(LogLevel::INFO_L,
               strategy->GetName() + " 전략이 추가되었습니다.", __FILE__,
               __LINE__);
}

void BaseEngine::SetConfig(const Config& config) {
  config_ = config;

  const auto initial_balance = config.GetInitialBalance();
  wallet_balance_ = initial_balance;
  available_balance_ = initial_balance;
  max_wallet_balance_ = initial_balance;
  min_wallet_balance_ = initial_balance;
}

bool BaseEngine::IncreaseWalletBalance(const double increase_balance) {
  if (increase_balance <= 0) {
    logger_->Log(
        LogLevel::ERROR_L,
        format("현재 자금 증가를 위해 주어진 값 {}은(는) 0보다 커야합니다.",
               increase_balance),
        __FILE__, __LINE__);
    return false;
  }

  // Wallet이 증가하면 주문 가능 자금도 영향을 받기 때문에 동시 처리
  wallet_balance_ += increase_balance;
  available_balance_ += increase_balance;
  return true;
}

bool BaseEngine::DecreaseWalletBalance(const double decrease_balance) {
  if (decrease_balance <= 0 || decrease_balance > wallet_balance_) {
    logger_->Log(
        LogLevel::ERROR_L,
        format("현재 자금 감소를 위해 주어진 값 {}은(는) 양수로 지정되어야 "
               "하며, 현재 자금 {}을(를) 초과할 수 없습니다.",
               decrease_balance, wallet_balance_),
        __FILE__, __LINE__);
    return false;
  }

  // Wallet이 감소하면 주문 가능 자금도 영향을 받기 때문에 동시 처리
  wallet_balance_ -= decrease_balance;
  available_balance_ -= decrease_balance;
  return true;
}

bool BaseEngine::IncreaseAvailableBalance(const double increase_balance) {
  if (increase_balance <= 0) {
    logger_->Log(
        LogLevel::ERROR_L,
        format(
            "주문 가능 자금 증가를 위해 주어진 값 {}은(는) 0보다 커야합니다.",
            increase_balance),
        __FILE__, __LINE__);
    return false;
  }

  available_balance_ += increase_balance;
  return true;
}

bool BaseEngine::DecreaseAvailableBalance(double decrease_balance) {
  if (decrease_balance <= 0 || decrease_balance > available_balance_) {
    logger_->Log(
        LogLevel::ERROR_L,
        format(
            "주문 가능 자금 감소를 위해 주어진 값 {}은(는) 양수로 지정되어야 "
            "하며, 주문 가능 자금 {}을(를) 초과할 수 없습니다.",
            decrease_balance, available_balance_),
        __FILE__, __LINE__);
    return false;
  }

  available_balance_ -= decrease_balance;
  return true;
}

void BaseEngine::SetBankruptcy() {
  is_bankruptcy_ = true;
}

Config BaseEngine::GetConfig() const { return config_; }

double BaseEngine::GetCurrentBalance() const { return wallet_balance_; }

double BaseEngine::GetAvailableBalance() const { return available_balance_; }
