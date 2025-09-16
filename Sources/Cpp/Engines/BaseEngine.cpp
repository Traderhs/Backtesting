// 표준 라이브러리
#include <format>

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
#include "Engines/Logger.hpp"
#include "Engines/SymbolInfo.hpp"

// 네임 스페이스
using namespace std;
using namespace backtesting::utils;

namespace backtesting::engine {

BaseEngine::BaseEngine()
    : engine_initialized_(false),
      trading_bar_num_symbols_(0),
      trading_bar_time_diff_(0),
      magnifier_bar_time_diff_(0),
      wallet_balance_(NAN),
      used_margin_(0),
      available_balance_(NAN),
      is_bankruptcy_(false),
      max_wallet_balance_(NAN),
      drawdown_(0),
      max_drawdown_(0) {}
BaseEngine::~BaseEngine() = default;

shared_ptr<Analyzer>& BaseEngine::analyzer_ = Analyzer::GetAnalyzer();
shared_ptr<BarHandler>& BaseEngine::bar_ = BarHandler::GetBarHandler();
shared_ptr<Logger>& BaseEngine::logger_ = Logger::GetLogger();
vector<json> BaseEngine::funding_rates_;
vector<string> BaseEngine::funding_rates_paths_;
json BaseEngine::exchange_info_;
string BaseEngine::exchange_info_path_;
json BaseEngine::leverage_bracket_;
string BaseEngine::leverage_bracket_path_;
shared_ptr<Config> BaseEngine::config_;

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
    exchange_info_path_ = exchange_info_path;
  } catch (const json::parse_error& e) {
    // JSON 파싱 오류 처리
    logger_->Log(
        ERROR_L,
        format("거래소 정보 파일 [{}]의 Json 형식이 유효하지 않습니다.",
               exchange_info_path),
        __FILE__, __LINE__, true);

    Logger::LogAndThrowError(e.what(), __FILE__, __LINE__);
  }

  file.close();

  logger_->Log(INFO_L, "거래소 정보가 엔진에 추가되었습니다.", __FILE__,
               __LINE__, true);
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
    leverage_bracket_path_ = leverage_bracket_path;
  } catch (const json::parse_error& e) {
    // JSON 파싱 오류 처리
    logger_->Log(
        ERROR_L,
        format("레버리지 구간 파일 [{}]의 Json 형식이 유효하지 않습니다.",
               leverage_bracket_path),
        __FILE__, __LINE__, true);
    Logger::LogAndThrowError(e.what(), __FILE__, __LINE__);
  }

  file.close();

  logger_->Log(INFO_L, "레버리지 구간이 엔진에 추가되었습니다.", __FILE__,
               __LINE__, true);
}

void BaseEngine::AddFundingRates(const vector<string>& symbol_names,
                                 const string& funding_rates_directory) {
  if (!filesystem::exists(funding_rates_directory)) {
    Logger::LogAndThrowError(
        format("펀딩 비율 폴더 [{}]이(가) 유효하지 않습니다.",
               funding_rates_directory),
        __FILE__, __LINE__);
  }

  for (const auto& symbol_name : symbol_names) {
    const auto& funding_rate_path =
        format("{}/{}.json", funding_rates_directory, symbol_name);

    ifstream file(funding_rate_path);
    if (!file.is_open()) {
      Logger::LogAndThrowError(
          format("펀딩 비율 파일 [{}]이(가) 유효하지 않습니다.",
                 funding_rate_path),
          __FILE__, __LINE__);
    }

    if (file.peek() == ifstream::traits_type::eof()) {
      Logger::LogAndThrowError(
          format("펀딩 비율 파일 [{}]이(가) 비어있습니다.", funding_rate_path),
          __FILE__, __LINE__);
    }

    try {
      funding_rates_.push_back(json::parse(file));
      funding_rates_paths_.push_back(funding_rate_path);
    } catch (const json::parse_error& e) {
      // JSON 파싱 오류 처리
      logger_->Log(
          ERROR_L,
          format("펀딩 비율 파일 [{}]의 Json 형식이 유효하지 않습니다.",
                 funding_rate_path),
          __FILE__, __LINE__, true);

      Logger::LogAndThrowError(e.what(), __FILE__, __LINE__);
    }

    file.close();
  }

  logger_->Log(INFO_L, "펀딩 비율이 엔진에 추가되었습니다.", __FILE__, __LINE__,
               true);
}

bool BaseEngine::IsEngineInitialized() const { return engine_initialized_; }

void BaseEngine::IncreaseWalletBalance(const double increase_balance) {
  if (IsLess(increase_balance, 0.0)) {
    logger_->Log(
        ERROR_L,
        format(
            "현재 자금 증가를 위해 주어진 [{}]는 0보다 크거나 같아야 합니다.",
            FormatDollar(increase_balance, true)),
        __FILE__, __LINE__, true);
    throw runtime_error("지갑 자금 증가 실패");
  }

  wallet_balance_ += increase_balance;
}

void BaseEngine::DecreaseWalletBalance(const double decrease_balance) {
  if (IsLess(decrease_balance, 0.0)) {
    logger_->Log(
        ERROR_L,
        format(
            "지갑 자금 감소를 위해 주어진 [{}]는 0보다 크거나 같아야 합니다.",
            FormatDollar(decrease_balance, true)),
        __FILE__, __LINE__, true);
    throw runtime_error("지갑 자금 감소 실패");
  }

  if (IsGreater(decrease_balance, wallet_balance_)) {
    logger_->Log(
        ERROR_L,
        format("지갑 자금 감소를 위해 주어진 [{}]는 지갑 자금 [{}]를 초과할 수 "
               "없습니다.",
               FormatDollar(decrease_balance, true),
               FormatDollar(wallet_balance_, true)),
        __FILE__, __LINE__, true);
    throw exception::Bankruptcy("지갑 자금 감소 실패");
  }

  wallet_balance_ -= decrease_balance;
}

void BaseEngine::IncreaseUsedMargin(const double increase_margin) {
  if (IsLessOrEqual(increase_margin, 0.0)) {
    logger_->Log(
        ERROR_L,
        format("사용한 마진 증가를 위해 주어진 [{}]는 양수로 지정해야 합니다.",
               FormatDollar(increase_margin, true)),
        __FILE__, __LINE__, true);
    throw runtime_error("사용한 마진 증가 실패");
  }

  if (const double sum_used_margin = used_margin_ + increase_margin;
      IsGreater(sum_used_margin, wallet_balance_)) {
    logger_->Log(ERROR_L,
                 format("사용한 마진 [{}]와 증가할 마진 [{}]의 합 [{}]는 "
                        "지갑 자금 [{}]를 초과할 수 없습니다.",
                        FormatDollar(used_margin_, true),
                        FormatDollar(increase_margin, true),
                        FormatDollar(sum_used_margin, true),
                        FormatDollar(wallet_balance_, true)),
                 __FILE__, __LINE__, true);
    throw runtime_error("사용한 마진 증가 실패");
  }

  used_margin_ += increase_margin;
}

void BaseEngine::DecreaseUsedMargin(const double decrease_margin) {
  if (IsLess(decrease_margin, 0.0)) {
    logger_->Log(
        ERROR_L,
        format(
            "사용한 마진 감소를 위해 주어진 [{}]는 음수로 지정할 수 없습니다.",
            FormatDollar(decrease_margin, true)),
        __FILE__, __LINE__, true);
    throw runtime_error("사용한 마진 감소 실패");
  }

  if (IsGreater(decrease_margin, used_margin_)) {
    logger_->Log(
        ERROR_L,
        format("사용한 마진 감소를 위해 주어진 [{}]는 사용한 마진 [{}]를 "
               "초과할 수 없습니다.",
               FormatDollar(decrease_margin, true),
               FormatDollar(used_margin_, true)),
        __FILE__, __LINE__, true);
    throw runtime_error("사용한 마진 감소 실패");
  }

  used_margin_ -= decrease_margin;
}

void BaseEngine::SetBankruptcy() { is_bankruptcy_ = true; }

SymbolInfo BaseEngine::GetSymbolInfo(const int symbol_idx) const {
  if (symbol_idx < 0 || symbol_idx >= trading_bar_num_symbols_) {
    Logger::LogAndThrowError(
        format("심볼 정보를 얻기 위하여 지정된 심볼 인덱스 [{}]이(가) 0 "
               "미만이거나 최대 인덱스 [{}]을(를) 초과했습니다.",
               symbol_idx, trading_bar_num_symbols_ - 1),
        __FILE__, __LINE__);
  }

  return symbol_info_[symbol_idx];
}

shared_ptr<Config>& BaseEngine::GetConfig() { return config_; }

double BaseEngine::GetWalletBalance() const { return wallet_balance_; }

double BaseEngine::GetUsedMargin() const { return used_margin_; }

double BaseEngine::GetAvailableBalance() {
  available_balance_ = wallet_balance_ - used_margin_;

  return available_balance_;
}

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

void BaseEngine::LogBalance() {
  logger_->Log(BALANCE_L,
               format("지갑 자금 [{}] | 사용한 마진 [{}] | 사용 가능 자금 [{}]",
                      FormatDollar(wallet_balance_, true),
                      FormatDollar(used_margin_, true),
                      FormatDollar(GetAvailableBalance(), true)),
               __FILE__, __LINE__);
}

void BaseEngine::LogSeparator(const bool log_to_console) {
  logger_->LogNoFormat(INFO_L, string(217, '='), log_to_console);
}

}  // namespace backtesting::engine