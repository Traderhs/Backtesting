// 표준 라이브러리
#include <cmath>
#include <format>

// 외부 라이브러리
#include "nlohmann/json.hpp"

// 파일 헤더
#include "Engines/BaseEngine.hpp"

// 내부 헤더
#include <Engines/BarData.hpp>

#include "Engines/Analyzer.hpp"
#include "Engines/BarHandler.hpp"
#include "Engines/Config.hpp"
#include "Engines/DataUtils.hpp"
#include "Engines/Exception.hpp"
#include "Engines/Logger.hpp"
#include "Engines/Strategy.hpp"
#include "Engines/TimeUtils.hpp"

// 네임 스페이스
using namespace std;
namespace backtesting {
using namespace config;
using namespace utils;
}  // namespace backtesting

namespace backtesting::engine {

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
                            const BarType bar_type, const int open_time_column,
                            const int open_column, const int high_column,
                            const int low_column, const int close_column,
                            const int volume_column,
                            const int close_time_column) {
  bar_->AddBarData(symbol_name, file_path, bar_type, open_time_column,
                   open_column, high_column, low_column, close_column,
                   volume_column, close_time_column);
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
        ERROR_L,
        format("거래소 정보 파일 [{}]의 Json 형식이 유효하지 않습니다.",
               exchange_info_path),
        __FILE__, __LINE__);
    Logger::LogAndThrowError(e.what(), __FILE__, __LINE__);
  }

  file.close();

  logger_->Log(INFO_L, "거래소 정보가 엔진에 추가되었습니다.", __FILE__,
               __LINE__);
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
        ERROR_L,
        format("레버리지 구간 파일 [{}]의 Json 형식이 유효하지 않습니다.",
               leverage_bracket_path),
        __FILE__, __LINE__);
    Logger::LogAndThrowError(e.what(), __FILE__, __LINE__);
  }

  file.close();

  logger_->Log(INFO_L, "레버리지 구간이 엔진에 추가되었습니다.", __FILE__,
               __LINE__);
}

bool BaseEngine::IsEngineInitialized() const { return engine_initialized_; }

void BaseEngine::IncreaseWalletBalance(const double increase_balance) {
  if (IsLess(increase_balance, 0.0)) {
    logger_->Log(
        ERROR_L,
        format(
            "현재 자금 증가를 위해 주어진 [{}]는 0보다 크거나 같아야 합니다.",
            FormatDollar(increase_balance, true)),
        __FILE__, __LINE__);
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
        __FILE__, __LINE__);
    throw runtime_error("지갑 자금 감소 실패");
  }

  if (IsGreater(decrease_balance, wallet_balance_)) {
    logger_->Log(
        ERROR_L,
        format("지갑 자금 감소를 위해 주어진 [{}]는 지갑 자금 [{}]를 초과할 수 "
               "없습니다.",
               FormatDollar(decrease_balance, true),
               FormatDollar(wallet_balance_, true)),
        __FILE__, __LINE__);
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
        __FILE__, __LINE__);
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
                 __FILE__, __LINE__);
    throw runtime_error("사용한 마진 증가 실패");
  }

  used_margin_ += increase_margin;
}

void BaseEngine::DecreaseUsedMargin(const double decrease_margin) {
  if (IsLessOrEqual(decrease_margin, 0.0)) {
    logger_->Log(
        ERROR_L,
        format("사용한 마진 감소를 위해 주어진 [{}]는 양수로 지정해야 합니다.",
               FormatDollar(decrease_margin, true)),
        __FILE__, __LINE__);
    throw runtime_error("사용한 마진 감소 실패");
  }

  if (IsGreater(decrease_margin, used_margin_)) {
    logger_->Log(
        ERROR_L,
        format("사용한 마진 감소를 위해 주어진 [{}]는 사용한 마진 [{}]를 "
               "초과할 수 없습니다.",
               FormatDollar(decrease_margin, true),
               FormatDollar(used_margin_, true)),
        __FILE__, __LINE__);
    throw runtime_error("사용한 마진 감소 실패");
  }

  used_margin_ -= decrease_margin;
}

void BaseEngine::SetBankruptcy() { is_bankruptcy_ = true; }

void BaseEngine::IncreaseLiquidationCount() { liquidation_count_++; }

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

void BaseEngine::PrintSeparator() {
  logger_->LogNoFormat(INFO_L, string(217, '='));
}

string BaseEngine::CreateDirectories() const {
  // 전략 이름들을 이어붙인 이름 + 현재 시간이 이번 백테스팅의 메인 폴더
  string main_directory = config_->GetRootDirectory() + "/Results/";

  try {
    for (const auto& strategy : strategies_) {
      main_directory += strategy->GetName() + "_";
    }

    main_directory += GetCurrentLocalDatetime();

    // 시간 구분 문자 제거 및 공백 언더 스코어화
    // (3부터 시작하는 이유는 드라이브 경로의 ':' 제외)
    main_directory.erase(
        std::remove(main_directory.begin() + 3, main_directory.end(), ':'),
        main_directory.end());
    main_directory.erase(
        std::remove(main_directory.begin() + 3, main_directory.end(), '-'),
        main_directory.end());
    replace(main_directory.begin() + 3, main_directory.end(), ' ', '_');

    // 메인 폴더 생성
    filesystem::create_directory(main_directory);

    // 전략 소스 저장 폴더 생성
    filesystem::create_directories(main_directory + "/Sources");

    // 지표 저장 폴더 생성
    for (const auto& strategy : strategies_) {
      filesystem::create_directories(main_directory + "/Indicators/" +
                                     strategy->GetName());
    }

    // 매매 목록 저장 폴더 생성
    filesystem::create_directories(main_directory + "/Trading Lists");
  } catch (const std::exception& e) {
    logger_->Log(ERROR_L, e.what(), __FILE__, __LINE__);
    Logger::LogAndThrowError("폴더 생성 중 에러가 발생했습니다.", __FILE__,
                             __LINE__);
  }

  return main_directory;
}

void BaseEngine::SaveConfig(const string& file_path) const {
  ordered_json config;

  const auto& trading_bar = bar_->GetBarData(TRADING);
  const auto& magnifier_bar = bar_->GetBarData(MAGNIFIER);
  const auto& reference_bar = bar_->GetAllReferenceBarData();
  const auto& mark_price_bar = bar_->GetBarData(MARK_PRICE);

  // 심볼 값 배열에 각 심볼의 객체를 push
  auto& symbol_json = config["심볼"];
  for (int symbol_idx = 0; symbol_idx < trading_bar->GetNumSymbols();
       symbol_idx++) {
    // 각 심볼의 배열
    ordered_json local_symbol_json;
    local_symbol_json["심볼명"] = trading_bar->GetSymbolName(symbol_idx);

    // 트레이딩 바 정보 저장
    const auto trading_num_bars = trading_bar->GetNumBars(symbol_idx);
    local_symbol_json["트레이딩 바"]["기간"]["시작"] =
        UtcTimestampToUtcDatetime(trading_bar->GetBar(symbol_idx, 0).open_time);
    local_symbol_json["트레이딩 바"]["기간"]["끝"] = UtcTimestampToUtcDatetime(
        trading_bar->GetBar(symbol_idx, trading_num_bars - 1).close_time);
    local_symbol_json["트레이딩 바"]["타임프레임"] =
        trading_bar->GetTimeframe();
    local_symbol_json["트레이딩 바"]["바 개수"] = trading_num_bars;

    // 돋보기 기능 사용 시 돋보기 바 정보 저장
    if (config_->GetUseBarMagnifier()) {
      const auto magnifier_num_bars = magnifier_bar->GetNumBars(symbol_idx);
      local_symbol_json["돋보기 바"]["기간"]["시작"] =
          UtcTimestampToUtcDatetime(
              magnifier_bar->GetBar(symbol_idx, 0).open_time);
      local_symbol_json["돋보기 바"]["기간"]["끝"] = UtcTimestampToUtcDatetime(
          magnifier_bar->GetBar(symbol_idx, magnifier_num_bars - 1).close_time);
      local_symbol_json["돋보기 바"]["타임프레임"] =
          magnifier_bar->GetTimeframe();
      local_symbol_json["돋보기 바"]["바 개수"] = magnifier_num_bars;
    } else {
      local_symbol_json["돋보기 바"] = json::array();
    }

    // 각 참조 바 정보 저장
    for (const auto& [timeframe, bar_data] : reference_bar) {
      ordered_json local_reference_bar_json;

      const auto reference_num_bars = bar_data->GetNumBars(symbol_idx);
      local_reference_bar_json["기간"]["시작"] =
          UtcTimestampToUtcDatetime(bar_data->GetBar(symbol_idx, 0).open_time);
      local_reference_bar_json["기간"]["끝"] = UtcTimestampToUtcDatetime(
          bar_data->GetBar(symbol_idx, reference_num_bars - 1).close_time);
      local_reference_bar_json["타임프레임"] = timeframe;
      local_reference_bar_json["바 개수"] = reference_num_bars;

      local_symbol_json["참조 바"].push_back(local_reference_bar_json);
    }

    // 마크 가격 바 정보 저장
    const auto mark_price_num_bars = mark_price_bar->GetNumBars(symbol_idx);
    local_symbol_json["마크 가격 바"]["기간"]["시작"] =
        UtcTimestampToUtcDatetime(
            mark_price_bar->GetBar(symbol_idx, 0).open_time);
    local_symbol_json["마크 가격 바"]["기간"]["끝"] = UtcTimestampToUtcDatetime(
        mark_price_bar->GetBar(symbol_idx, mark_price_num_bars - 1).close_time);
    local_symbol_json["마크 가격 바"]["타임프레임"] =
        mark_price_bar->GetTimeframe();
    local_symbol_json["마크 가격 바"]["바 개수"] = mark_price_num_bars;

    // 한 심볼의 정보저장
    symbol_json.push_back(local_symbol_json);
  }

  // 전략 값 배열에 각 전략의 객체를 push
  auto& strategy_json = config["전략"];
  for (const auto& strategy : strategies_) {
    ordered_json local_strategy_json;

    local_strategy_json["전략명"] = strategy->GetName();

    // 해당 전략에서 사용하는 지표들을 저장
    for (const auto& indicator : strategy->GetIndicators()) {
      ordered_json local_indicator_json;
      local_indicator_json["지표명"] = indicator->GetName();
      local_indicator_json["타임프레임"] = indicator->GetTimeframe();

      local_strategy_json["지표"].push_back(local_indicator_json);
    }

    strategy_json.push_back(local_strategy_json);
  }

  // 설정 값 배열에 각 설정의 객체를 push
  auto& config_json = config["설정"];
  config_json["루트 폴더"] = config_->GetRootDirectory();
  config_json["바 돋보기"] =
      config_->GetUseBarMagnifier() ? "활성화" : "비활성화";
  config_json["초기 자금"] = FormatDollar(config_->GetInitialBalance(), true);

  ostringstream taker_fee_percentage, maker_fee_percentage,
      taker_slippage_percentage, maker_slippage_percentage;
  taker_fee_percentage << fixed << setprecision(4)
                       << config_->GetTakerFeePercentage();
  maker_fee_percentage << fixed << setprecision(4)
                       << config_->GetMakerFeePercentage();
  taker_slippage_percentage << fixed << setprecision(4)
                            << config_->GetTakerSlippagePercentage();
  maker_slippage_percentage << fixed << setprecision(4)
                            << config_->GetMakerSlippagePercentage();

  config_json["테이커 수수료 퍼센트"] = taker_fee_percentage.str() + "%";
  config_json["메이커 수수료 퍼센트"] = maker_fee_percentage.str() + "%";
  config_json["테이커 슬리피지 퍼센트"] = taker_slippage_percentage.str() + "%";
  config_json["메이커 슬리피지 퍼센트"] = maker_slippage_percentage.str() + "%";

  string bar_type_str[4] = {"트레이딩 바", "돋보기 바", "참조 바",
                            "마크 가격 바"};
  for (int i = 0; i < 4; i++) {
    config_json["심볼 간 동일한 바 데이터 검사"][bar_type_str[i]] =
        config_->GetCheckSameBarData()[i] ? "활성화" : "비활성화";
  }

  config_json["마크 가격과 동일한 목표 바 데이터 검사"] =
      config_->GetCheckSameTargetBarData() ? "활성화" : "비활성화";

  // 파일로 저장
  ofstream config_file(file_path);
  config_file << setw(4) << config << endl;
  config_file.close();
}

}  // namespace backtesting::engine