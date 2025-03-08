// 표준 라이브러리
#include <cmath>
#include <format>
#include <iostream>

// 외부 라이브러리
#include "nlohmann/json.hpp"

// 파일 헤더
#include "Engines/BaseEngine.hpp"

// 내부 헤더
#include "Engines/Analyzer.hpp"
#include "Engines/BarHandler.hpp"
#include "Engines/Config.hpp"
#include "Engines/DataUtils.hpp"
#include "Engines/Exception.hpp"

// 네임 스페이스
using namespace std;
using namespace data_utils;

BaseEngine::BaseEngine()
    : engine_initialized_(false),
      wallet_balance_(nan("")),
      available_balance_(nan("")),
      unrealized_pnl_(0),
      used_margin_(0),
      is_bankruptcy_(false),
      max_wallet_balance_(nan("")),
      drawdown_(0),
      max_drawdown_(0),
      liquidation_count_(0) {}
BaseEngine::~BaseEngine() = default;

shared_ptr<Analyzer>& BaseEngine::analyzer_ = Analyzer::GetAnalyzer();
shared_ptr<BarHandler>& BaseEngine::bar_ = BarHandler::GetBarHandler();
shared_ptr<Logger>& BaseEngine::logger_ = Logger::GetLogger();
json BaseEngine::exchange_info_;
json BaseEngine::leverage_bracket_;
shared_ptr<Config> BaseEngine::config_;

void BaseEngine::AddBarData(const string& symbol_name, const string& file_path,
                            const BarType bar_type,
                            const vector<int>& columns) {
  bar_->AddBarData(symbol_name, file_path, bar_type, columns);
}

void BaseEngine::AddExchangeInfo(const string& exchange_info_path) {
  ifstream file(exchange_info_path);
  if (!file.is_open()) {
    Logger::LogAndThrowError(
        format("거래소 정보 파일 [{}]이(가) 유효하지 않습니다.",
               exchange_info_path),
        __FILE__, __LINE__);
  }

  if (file.peek() == ifstream::traits_type::eof()) {
    Logger::LogAndThrowError(
        format("거래소 정보 파일 [{}]이(가) 비어있습니다.", exchange_info_path),
        __FILE__, __LINE__);
  }

  try {
    exchange_info_ = json::parse(file);
  } catch (const json::parse_error& e) {
    // JSON 파싱 오류 처리
    logger_->Log(
        LogLevel::ERROR_L,
        format("거래소 정보 파일 [{}]의 Json 형식이 유효하지 않습니다.",
               exchange_info_path),
        __FILE__, __LINE__);
    Logger::LogAndThrowError(e.what(), __FILE__, __LINE__);
  }

  file.close();

  logger_->Log(LogLevel::INFO_L, "거래소 정보가 엔진에 추가되었습니다.",
               __FILE__, __LINE__);
}

void BaseEngine::AddLeverageBracket(const string& leverage_bracket_path) {
  ifstream file(leverage_bracket_path);
  if (!file.is_open()) {
    Logger::LogAndThrowError(
        format("레버리지 구간 파일 [{}]이(가) 유효하지 않습니다.",
               leverage_bracket_path),
        __FILE__, __LINE__);
  }

  if (file.peek() == ifstream::traits_type::eof()) {
    Logger::LogAndThrowError(
        format("레버리지 구간 파일 [{}]이(가) 비어있습니다.",
               leverage_bracket_path),
        __FILE__, __LINE__);
  }

  try {
    leverage_bracket_ = json::parse(file);
  } catch (const json::parse_error& e) {
    // JSON 파싱 오류 처리
    logger_->Log(
        LogLevel::ERROR_L,
        format("레버리지 구간 파일 [{}]의 Json 형식이 유효하지 않습니다.",
               leverage_bracket_path),
        __FILE__, __LINE__);
    Logger::LogAndThrowError(e.what(), __FILE__, __LINE__);
  }

  file.close();

  logger_->Log(LogLevel::INFO_L, "레버리지 구간이 엔진에 추가되었습니다.",
               __FILE__, __LINE__);
}

bool BaseEngine::IsEngineInitialized() const { return engine_initialized_; }

void BaseEngine::IncreaseWalletBalance(const double increase_balance) {
  if (IsLess(increase_balance, 0.0)) {
    logger_->Log(
        LogLevel::ERROR_L,
        format(
            "현재 자금 증가를 위해 주어진 [{}]는 0보다 크거나 같아야 합니다.",
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
        format(
            "지갑 자금 감소를 위해 주어진 [{}]는 0보다 크거나 같아야 합니다.",
            FormatDollar(decrease_balance)),
        __FILE__, __LINE__);
    throw runtime_error("지갑 자금 감소 실패");
  }

  if (IsGreater(decrease_balance, wallet_balance_)) {
    logger_->Log(
        LogLevel::ERROR_L,
        format("지갑 자금 감소를 위해 주어진 [{}]는 지갑 자금 [{}]를 초과할 수 "
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
        format("사용한 마진 증가를 위해 주어진 [{}]는 양수로 지정해야 합니다.",
               FormatDollar(increase_margin)),
        __FILE__, __LINE__);
    throw runtime_error("사용한 마진 증가 실패");
  }

  if (const double sum_used_margin = used_margin_ + increase_margin;
      IsGreater(sum_used_margin, wallet_balance_)) {
    logger_->Log(
        LogLevel::ERROR_L,
        format("사용한 마진 [{}]와 증가할 마진 [{}]의 합 [{}]는 "
               "지갑 자금 [{}]를 초과할 수 없습니다.",
               FormatDollar(used_margin_), FormatDollar(increase_margin),
               FormatDollar(sum_used_margin), FormatDollar(wallet_balance_)),
        __FILE__, __LINE__);
    throw runtime_error("사용한 마진 증가 실패");
  }

  used_margin_ += increase_margin;
}

void BaseEngine::DecreaseUsedMargin(const double decrease_margin) {
  if (IsLessOrEqual(decrease_margin, 0.0)) {
    logger_->Log(
        LogLevel::ERROR_L,
        format("사용한 마진 감소를 위해 주어진 [{}]는 양수로 지정해야 합니다.",
               FormatDollar(decrease_margin)),
        __FILE__, __LINE__);
    throw runtime_error("사용한 마진 감소 실패");
  }

  if (IsGreater(decrease_margin, used_margin_)) {
    logger_->Log(
        LogLevel::ERROR_L,
        format("사용한 마진 감소를 위해 주어진 [{}]는 사용한 마진 [{}]를 "
               "초과할 수 없습니다.",
               FormatDollar(decrease_margin), FormatDollar(used_margin_)),
        __FILE__, __LINE__);
    throw runtime_error("사용한 마진 감소 실패");
  }

  used_margin_ -= decrease_margin;
}

void BaseEngine::SetBankruptcy() { is_bankruptcy_ = true; }

void BaseEngine::IncreaseLiquidationCount() { liquidation_count_++; }

string BaseEngine::GetMainDirectory() const { return main_directory_; }

shared_ptr<Config> BaseEngine::GetConfig() { return config_; }

double BaseEngine::GetWalletBalance() const { return wallet_balance_; }

double BaseEngine::GetMaxWalletBalance() const { return max_wallet_balance_; }

double BaseEngine::GetDrawdown() const { return drawdown_; }

double BaseEngine::GetMaxDrawdown() const { return max_drawdown_; }

void BaseEngine::UpdateStatistics() {
  max_wallet_balance_ = IsGreater(wallet_balance_, max_wallet_balance_)
                            ? wallet_balance_
                            : max_wallet_balance_;
  drawdown_ = (1 - wallet_balance_ / max_wallet_balance_) * 100;
  max_drawdown_ =
      IsGreater(drawdown_, max_drawdown_) ? drawdown_ : max_drawdown_;
}

void BaseEngine::PrintSeparator() { cout << string(217, '=') << endl; }