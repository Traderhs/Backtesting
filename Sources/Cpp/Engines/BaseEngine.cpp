// 표준 라이브러리
#include <format>

// 파일 헤더
#include "Engines/BaseEngine.hpp"

// 내부 헤더
#include "Engines/Analyzer.hpp"
#include "Engines/BarHandler.hpp"
#include "Engines/DataUtils.hpp"
#include "Engines/Strategy.hpp"

// 네임 스페이스
using namespace data_utils;

BaseEngine::BaseEngine()
    : wallet_balance_(-1),
      available_balance_(-1),
      unrealized_pnl_(0),
      used_margin_(0),
      is_bankruptcy_(false),
      max_wallet_balance_(-1),
      drawdown_(0),
      max_drawdown_(0),
      liquidations_(0) {}
BaseEngine::~BaseEngine() = default;

shared_ptr<Analyzer>& BaseEngine::analyzer_ = Analyzer::GetAnalyzer();
shared_ptr<BarHandler>& BaseEngine::bar_ = BarHandler::GetBarHandler();
shared_ptr<Logger>& BaseEngine::logger_ = Logger::GetLogger();

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
}

bool BaseEngine::IncreaseWalletBalance(const double increase_balance) {
  if (increase_balance <= 0) {
    logger_->Log(LogLevel::ERROR_L,
                 format("현재 자금 증가를 위해 주어진 {}는 0보다 커야합니다.",
                        FormatDollar(increase_balance)),
                 __FILE__, __LINE__);
    return false;
  }

  wallet_balance_ += increase_balance;
  return true;
}

bool BaseEngine::DecreaseWalletBalance(const double decrease_balance) {
  if (decrease_balance < 0 || decrease_balance > wallet_balance_) {
    logger_->Log(
        LogLevel::ERROR_L,
        format("현재 자금 감소를 위해 주어진 {}는 0과 같거나 커야 하며, "
               "지갑 자금 {}를 초과할 수 없습니다.",
               FormatDollar(decrease_balance), FormatDollar(wallet_balance_)),
        __FILE__, __LINE__);
    return false;
  }

  wallet_balance_ -= decrease_balance;
  return true;
}

void BaseEngine::IncreaseUsedMargin(const double increase_margin) {
  if (increase_margin <= 0) {
    logger_->Log(
        LogLevel::ERROR_L,
        format("사용한 마진 증가를 위해 주어진 {}는 0보다 커야 합니다.",
               FormatDollar(increase_margin)),
        __FILE__, __LINE__);
    return;
  }

  // 사용 가능한 마진의 최대값은 현재 실제 보유한 자금인 지갑 자금과
  // 미실현 손익의 합계
  const double total_balance = wallet_balance_ + unrealized_pnl_;
  if (const double sum_used_margin = used_margin_ + increase_margin;
      sum_used_margin > total_balance) {
    logger_->Log(
        LogLevel::ERROR_L,
        format("사용한 마진 {}와 증가할 마진 {}의 합 {}는 "
               "지갑 자금 {}와 미실현 손익 {}의 합계 {}를 초과할 수 없습니다.",
               FormatDollar(used_margin_), FormatDollar(increase_margin),
               FormatDollar(sum_used_margin), FormatDollar(wallet_balance_),
               FormatDollar(unrealized_pnl_), FormatDollar(total_balance)),
        __FILE__, __LINE__);
    return;
  }

  used_margin_ += increase_margin;
}

void BaseEngine::DecreaseUsedMargin(const double decrease_margin) {
  if (decrease_margin <= 0 || decrease_margin > used_margin_) {
    logger_->Log(
        LogLevel::ERROR_L,
        format("사용한 마진 감소를 위해 주어진 {}는 양수로 지정되어야 하며, "
               "사용한 마진 {}를 초과할 수 없습니다.",
               FormatDollar(decrease_margin), FormatDollar(used_margin_)),
        __FILE__, __LINE__);
    return;
  }

  used_margin_ -= decrease_margin;
}

void BaseEngine::SetBankruptcy() { is_bankruptcy_ = true; }

Config BaseEngine::GetConfig() const { return config_; }

double BaseEngine::GetWalletBalance() const { return wallet_balance_; }

double BaseEngine::GetMaxWalletBalance() const { return max_wallet_balance_; }

double BaseEngine::GetDrawdown() const { return drawdown_; }

double BaseEngine::GetMaxDrawdown() const { return max_drawdown_; }


void BaseEngine::UpdateStatistics() {
  max_wallet_balance_ = max(max_wallet_balance_, wallet_balance_);
  drawdown_ = (1 - wallet_balance_ / max_wallet_balance_) * 100;
  max_drawdown_ = max(max_drawdown_, drawdown_);
}
