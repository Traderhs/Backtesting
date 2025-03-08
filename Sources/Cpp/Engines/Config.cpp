// 표준 라이브러리
#include <cmath>

// 파일 헤더
#include "Engines/Config.hpp"

// 내부 헤더
#include "Engines/Logger.hpp"

Config::Config()
    : initial_balance_(nan("")),
      taker_fee_(nan("")),
      maker_fee_(nan("")),
      taker_slippage_(nan("")),
      maker_slippage_(nan("")),
      check_bar_data_duplication_(4, true),
      check_target_bar_data_duplication_(true) {
  // 증가 카운터는 SetConfig 함수로만 증가하는데 SetConfig 없이 직접 생성자
  // 호출로 전 증가 카운터가 현재 증가 카운터와 같다면 오류 발생
  if (pre_creation_counter_ == creation_counter_) {
    logger_->Log(LogLevel::ERROR_L,
                 "엔진의 사전 설정은 SetConfig 함수의 호출로만 가능합니다.",
                 __FILE__, __LINE__);
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

Config& Config::SetRootDirectory(const string& root_directory) {
  root_directory_ = root_directory;
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

Config& Config::SetTakerFee(const double market_commission) {
  taker_fee_ = market_commission;
  return *this;
}

Config& Config::SetMakerFee(const double limit_commission) {
  maker_fee_ = limit_commission;
  return *this;
}

Config& Config::SetTakerSlippage(const double market_slippage) {
  taker_slippage_ = market_slippage;
  return *this;
}

Config& Config::SetMakerSlippage(const double limit_slippage) {
  maker_slippage_ = limit_slippage;
  return *this;
}

Config& Config::DisableBarDataDuplicationCheck(BarType bar_type) {
  check_bar_data_duplication_[static_cast<size_t>(bar_type)] = false;
  return *this;
}

Config& Config::DisableTargetBarDataDuplicationCheck() {
  check_target_bar_data_duplication_ = false;
  return *this;
}

string Config::GetRootDirectory() const { return root_directory_; }
bool Config::GetUseBarMagnifier() const { return use_bar_magnifier_.value(); }
double Config::GetInitialBalance() const { return initial_balance_; }
double Config::GetTakerFee() const { return taker_fee_; }
double Config::GetMakerFee() const { return maker_fee_; }
double Config::GetTakerSlippage() const { return taker_slippage_; }
double Config::GetMakerSlippage() const { return maker_slippage_; }
vector<bool> Config::GetCheckBarDataDuplication() const {
  return check_bar_data_duplication_;
}
bool Config::GetCheckTargetBarDataDuplication() const {
  return check_target_bar_data_duplication_;
}

bool Config::UseBarMagnifierHasValue() const {
  return use_bar_magnifier_.has_value();
}