// 표준 라이브러리
#include <cmath>
#include <format>

// 파일 헤더
#include "Engines/BaseEngine.hpp"

// 내부 헤더
#include "Engines/Analyzer.hpp"
#include "Engines/BarHandler.hpp"
#include "Engines/DataUtils.hpp"
#include "Engines/Exception.hpp"
#include "Engines/Strategy.hpp"

// 네임 스페이스
using namespace data_utils;

BaseEngine::BaseEngine()
    : wallet_balance_(nan("")),
      available_balance_(nan("")),
      unrealized_pnl_(0),
      used_margin_(0),
      is_bankruptcy_(false),
      max_wallet_balance_(nan("")),
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
}

void BaseEngine::IncreaseWalletBalance(const double increase_balance) {
  if (IsLess(increase_balance, 0.0)) {
    logger_->Log(
        LogLevel::ERROR_L,
        format("현재 자금 증가를 위해 주어진 {}는 0보다 크거나 같아야 합니다.",
               FormatDollar(increase_balance)),
        __FILE__, __LINE__);
    throw runtime_error("지갑 자금 증가 실패");
  }

  wallet_balance_ += increase_balance;
}

void BaseEngine::DecreaseWalletBalance(const double decrease_balance) {
  if (IsLess(decrease_balance, 0.0)) {
    logger_->Log(
        LogLevel::ERROR_L,
        format("현재 자금 감소를 위해 주어진 {}는 0보다 크거나 같아야 합니다.",
               FormatDollar(decrease_balance)),
        __FILE__, __LINE__);
    throw runtime_error("지갑 자금 감소 실패");
  }

  if (IsGreater(decrease_balance, wallet_balance_)) {
    logger_->Log(
        LogLevel::ERROR_L,
        format("현재 자금 감소를 위해 주어진 {}는 지갑 자금 {}를 초과할 수 "
               "없습니다.",
               FormatDollar(decrease_balance), FormatDollar(wallet_balance_)),
        __FILE__, __LINE__);
    throw Bankruptcy("지갑 자금 감소 실패");
  }

  wallet_balance_ -= decrease_balance;
}

void BaseEngine::IncreaseUsedMargin(const double increase_margin) {
  if (IsLessOrEqual(increase_margin, 0.0)) {
    logger_->Log(
        LogLevel::ERROR_L,
        format("사용한 마진 증가를 위해 주어진 {}는 양수로 지정해야 합니다.",
               FormatDollar(increase_margin)),
        __FILE__, __LINE__);
    throw runtime_error("사용한 마진 증가 실패");
  }

  // 사용 가능한 마진의 최대값은 현재 실제 보유한 자금인 지갑 자금과
  // 미실현 손익의 합계
  const double total_balance = wallet_balance_ + unrealized_pnl_;
  if (const double sum_used_margin = used_margin_ + increase_margin;
      IsGreater(sum_used_margin, total_balance)) {
    logger_->Log(
        LogLevel::ERROR_L,
        format("사용한 마진 {}와 증가할 마진 {}의 합 {}는 "
               "지갑 자금 {}와 미실현 손익 {}의 합계 {}를 초과할 수 없습니다.",
               FormatDollar(used_margin_), FormatDollar(increase_margin),
               FormatDollar(sum_used_margin), FormatDollar(wallet_balance_),
               FormatDollar(unrealized_pnl_), FormatDollar(total_balance)),
        __FILE__, __LINE__);
    throw runtime_error("사용한 마진 증가 실패");
  }

  used_margin_ += increase_margin;
}

void BaseEngine::DecreaseUsedMargin(const double decrease_margin) {
  if (IsLessOrEqual(decrease_margin, 0.0)) {
    logger_->Log(
        LogLevel::ERROR_L,
        format("사용한 마진 감소를 위해 주어진 {}는 양수로 지정해야 합니다.",
               FormatDollar(decrease_margin)),
        __FILE__, __LINE__);
    throw runtime_error("사용한 마진 감소 실패");
  }

  if (IsGreater(decrease_margin, used_margin_)) {
    logger_->Log(
        LogLevel::ERROR_L,
        format("사용한 마진 감소를 위해 주어진 {}는 사용한 마진 {}를 초과할 수 "
               "없습니다.",
               FormatDollar(decrease_margin), FormatDollar(used_margin_)),
        __FILE__, __LINE__);
    throw runtime_error("사용한 마진 감소 실패");
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
