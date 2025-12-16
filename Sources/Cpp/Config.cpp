// 파일 헤더
#include "Config.hpp"

// 내부 헤더
#include "Logger.hpp"
#include "Slippage.hpp"

namespace backtesting::engine {

Config::Config()
    : initial_balance_(NAN),
      taker_fee_percentage_(NAN),
      maker_fee_percentage_(NAN),
      check_same_bar_data_(4, true),
      check_same_bar_data_with_target_(true) {
  // 증가 카운터는 SetConfig 함수로만 증가하는데 SetConfig 없이 직접 생성자
  // 호출로 전 증가 카운터가 현재 증가 카운터와 같다면 오류 발생
  if (pre_creation_counter_ == creation_counter_) {
    logger_->Log(ERROR_L,
                 "엔진의 사전 설정은 SetConfig 함수의 호출로만 가능합니다.",
                 __FILE__, __LINE__, true);
    Logger::LogAndThrowError("엔진을 사전 설정하는 중 에러가 발생했습니다.",
                             __FILE__, __LINE__);
  }

  // 정상적으로 AddStrategy 함수를 통했다면 전 증가 가운터에 현재 카운터를 대입
  pre_creation_counter_ = creation_counter_;
}
Config::~Config() = default;

shared_ptr<Logger>& Config::logger_ = Logger::GetLogger();
size_t Config::creation_counter_;
size_t Config::pre_creation_counter_;
string Config::project_directory_;

Config& Config::SetProjectDirectory(const string& project_directory) {
  project_directory_ = project_directory;

  // 프로젝트 폴더 설정 시 로그 폴더도 함께 설정
  Logger::SetLogDirectory(project_directory + "/Logs");

  return *this;
}

Config& Config::SetBacktestPeriod(const string& start_time,
                                  const string& end_time,
                                  const string& format) {
  backtest_period_ = Period{start_time, end_time, format};
  return *this;
}

Config& Config::SetUseBarMagnifier(const bool use_bar_magnifier) {
  use_bar_magnifier_ = use_bar_magnifier;
  return *this;
}

Config& Config::SetInitialBalance(const double initial_balance) {
  initial_balance_ = initial_balance;
  return *this;
}

Config& Config::SetTakerFeePercentage(const double taker_fee_percentage) {
  taker_fee_percentage_ = taker_fee_percentage;
  return *this;
}

Config& Config::SetMakerFeePercentage(const double maker_fee_percentage) {
  maker_fee_percentage_ = maker_fee_percentage;
  return *this;
}

Config& Config::SetCheckLimitMaxQty(bool check_limit_max_qty) {
  check_limit_max_qty_ = check_limit_max_qty;
  return *this;
}

Config& Config::SetCheckLimitMinQty(bool check_limit_min_qty) {
  check_limit_min_qty_ = check_limit_min_qty;
  return *this;
}

Config& Config::SetCheckMarketMaxQty(bool check_market_max_qty) {
  check_market_max_qty_ = check_market_max_qty;
  return *this;
}

Config& Config::SetCheckMarketMinQty(bool check_market_min_qty) {
  check_market_min_qty_ = check_market_min_qty;
  return *this;
}

Config& Config::SetCheckMinNotionalValue(bool check_min_notional_value) {
  check_min_notional_value_ = check_min_notional_value;
  return *this;
}

Config& Config::DisableSameBarDataCheck(BarType bar_type) {
  check_same_bar_data_[static_cast<size_t>(bar_type)] = false;
  return *this;
}

Config& Config::DisableSameBarDataWithTargetCheck() {
  check_same_bar_data_with_target_ = false;
  return *this;
}

string Config::GetProjectDirectory() { return project_directory_; }
optional<Period> Config::GetBacktestPeriod() const { return backtest_period_; }
optional<bool> Config::GetUseBarMagnifier() const { return use_bar_magnifier_; }
double Config::GetInitialBalance() const { return initial_balance_; }
double Config::GetTakerFeePercentage() const { return taker_fee_percentage_; }
double Config::GetMakerFeePercentage() const { return maker_fee_percentage_; }
shared_ptr<Slippage> Config::GetSlippage() const { return slippage_; }
optional<bool> Config::GetCheckLimitMaxQty() const {
  return check_limit_max_qty_;
}
optional<bool> Config::GetCheckLimitMinQty() const {
  return check_limit_min_qty_;
}
optional<bool> Config::GetCheckMarketMaxQty() const {
  return check_market_max_qty_;
}
optional<bool> Config::GetCheckMarketMinQty() const {
  return check_market_min_qty_;
}
optional<bool> Config::GetCheckMinNotionalValue() const {
  return check_min_notional_value_;
}
vector<bool> Config::GetCheckSameBarData() const {
  return check_same_bar_data_;
}
bool Config::GetCheckSameBarDataWithTarget() const {
  return check_same_bar_data_with_target_;
}

}  // namespace backtesting::engine