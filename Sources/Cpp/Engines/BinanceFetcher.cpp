// 표준 라이브러리
#include <chrono>
#include <deque>
#include <filesystem>
#include <format>
#include <future>
#include <string>
#include <utility>

// 외부 라이브러리
#include "arrow/array/builder_primitive.h"
#include "arrow/table.h"
#include "nlohmann/json.hpp"

// 파일 헤더
#include "Engines/BinanceFetcher.hpp"

// 내부 헤더
#include "Engines/DataUtils.hpp"
#include "Engines/Engine.hpp"
#include "Engines/Logger.hpp"
#include "Engines/TimeUtils.hpp"

// 네임 스페이스
namespace backtesting {
using namespace utils;
}  // namespace backtesting

namespace backtesting::fetcher {

BinanceFetcher::BinanceFetcher(string api_key_env_var,
                               string api_secret_env_var) {
  api_key_env_var_ = move(api_key_env_var);
  api_secret_env_var_ = move(api_secret_env_var);

  market_data_directory_ = filesystem::absolute("../../Data").string();
  continuous_klines_directory_ = market_data_directory_ + "/Continuous Klines";
  mark_price_klines_directory_ = market_data_directory_ + "/Mark Price Klines";
  funding_rates_directory_ = market_data_directory_ + "/Funding Rates";

  if (!filesystem::exists(market_data_directory_)) {
    filesystem::create_directory(market_data_directory_);
  }

  if (!filesystem::exists(continuous_klines_directory_)) {
    filesystem::create_directory(continuous_klines_directory_);
  }

  if (!filesystem::exists(mark_price_klines_directory_)) {
    filesystem::create_directory(mark_price_klines_directory_);
  }

  if (!filesystem::exists(funding_rates_directory_)) {
    filesystem::create_directory(funding_rates_directory_);
  }
}

BinanceFetcher::BinanceFetcher(string api_key_env_var,
                               string api_secret_env_var,
                               string market_data_directory) {
  api_key_env_var_ = move(api_key_env_var);
  api_secret_env_var_ = move(api_secret_env_var);

  market_data_directory_ = move(market_data_directory);
  continuous_klines_directory_ = market_data_directory_ + "/Continuous Klines";
  mark_price_klines_directory_ = market_data_directory_ + "/Mark Price Klines";
  funding_rates_directory_ = market_data_directory_ + "/Funding Rates";

  if (!filesystem::exists(market_data_directory_)) {
    filesystem::create_directory(market_data_directory_);
  }

  if (!filesystem::exists(continuous_klines_directory_)) {
    filesystem::create_directory(continuous_klines_directory_);
  }

  if (!filesystem::exists(mark_price_klines_directory_)) {
    filesystem::create_directory(mark_price_klines_directory_);
  }

  if (!filesystem::exists(funding_rates_directory_)) {
    filesystem::create_directory(funding_rates_directory_);
  }
}

shared_ptr<Logger>& BinanceFetcher::logger_ = Logger::GetLogger();

string BinanceFetcher::header_ = "X-MBX-APIKEY: ";
string BinanceFetcher::futures_endpoint_ = "https://fapi.binance.com";

string BinanceFetcher::server_time_url_ = futures_endpoint_ + "/fapi/v1/time";
string BinanceFetcher::continuous_klines_url_ =
    futures_endpoint_ + "/fapi/v1/continuousKlines";
string BinanceFetcher::mark_price_klines_url_ =
    futures_endpoint_ + "/fapi/v1/markPriceKlines";
string BinanceFetcher::funding_rates_url_ =
    futures_endpoint_ + "/fapi/v1/fundingRate";
string BinanceFetcher::exchange_info_url_ =
    futures_endpoint_ + "/fapi/v1/exchangeInfo";
string BinanceFetcher::leverage_bracket_url_ =
    futures_endpoint_ + "/fapi/v1/leverageBracket";

void BinanceFetcher::FetchContinuousKlines(const string& symbol,
                                           const string& timeframe) const {
  const auto& start = chrono::high_resolution_clock::now();

  // 타임프레임 유효성 검사
  ParseTimeframe(timeframe);

  // 저장 경로 생성
  const string& timeframe_filename = GetFilenameWithTimeframe(timeframe);
  const string& save_directory =
      continuous_klines_directory_ + "/" + symbol + "/" + timeframe_filename;
  const string& file_path =
      save_directory + "/" + timeframe_filename + ".parquet";
  filesystem::create_directories(save_directory);

  if (filesystem::exists(file_path)) {
    logger_->Log(
        WARN_L,
        format("[{} {}] 연속 선물 캔들스틱 파일이 [{}] 경로에 이미 존재합니다.",
               symbol, timeframe_filename, ConvertBackslashToSlash(file_path)),
        __FILE__, __LINE__, true);
    Engine::LogSeparator(true);
    return;
  }

  logger_->Log(INFO_L,
               format("[{} {}] 연속 선물 캔들스틱 파일 생성을 시작합니다.",
                      symbol, timeframe_filename),
               __FILE__, __LINE__, true);

  const unordered_map<string, string>& params = {{"pair", symbol},
                                                 {"contractType", "PERPETUAL"},
                                                 {"interval", timeframe},
                                                 {"startTime", "0"},
                                                 {"limit", "1000"}};

  const auto& klines = FetchKlines(continuous_klines_url_, params, true).get();

  // 캔들스틱 데이터 변환
  const auto& transformed_klines = TransformKlines(klines, true);

  // 선물 데이터가 비어있으면 처리 중단
  if (transformed_klines.empty()) {
    logger_->Log(ERROR_L, "선물 데이터가 비어있습니다. 처리를 중단합니다.",
                 __FILE__, __LINE__, true);
    Engine::LogSeparator(true);
    return;
  }

  // 저장
  SaveKlines(transformed_klines, save_directory,
             timeframe_filename + ".parquet", true, true);

  logger_->Log(
      INFO_L,
      format("[{} - {}] 기간의 [{} {}] 연속 선물 캔들스틱 파일이 [{}] 경로에 "
             "저장되었습니다.",
             UtcTimestampToUtcDatetime(transformed_klines.front().at(0)),
             UtcTimestampToUtcDatetime(transformed_klines.back().at(6)), symbol,
             timeframe_filename, ConvertBackslashToSlash(file_path)),
      __FILE__, __LINE__, true);

  const auto& end = chrono::high_resolution_clock::now();
  logger_->Log(
      INFO_L,
      "소요 시간: " +
          FormatTimeDiff(
              duration_cast<chrono::milliseconds>(end - start).count()),
      __FILE__, __LINE__, true);
  Engine::LogSeparator(true);
}

void BinanceFetcher::UpdateContinuousKlines(const string& symbol,
                                            const string& timeframe) const {
  const auto& start = chrono::high_resolution_clock::now();

  // 타임프레임 유효성 검사
  ParseTimeframe(timeframe);

  const auto& filename_timeframe = GetFilenameWithTimeframe(timeframe);
  const auto& directory_path =
      continuous_klines_directory_ + "/" + symbol + "/" + filename_timeframe;
  const auto& file_path =
      directory_path + "/" + filename_timeframe + ".parquet";

  if (!filesystem::exists(file_path)) {
    logger_->Log(WARN_L,
                 format("[{} {}] 연속 선물 캔들스틱 파일이 존재하지 않아 "
                        "업데이트할 수 없습니다.",
                        symbol, filename_timeframe),
                 __FILE__, __LINE__, true);
    Engine::LogSeparator(true);
    return;
  }

  // Parquet 파일 읽기
  shared_ptr<arrow::Table> klines_file;
  try {
    klines_file = ReadParquet(file_path);
  } catch (const exception& e) {
    Logger::LogAndThrowError(e.what(), __FILE__, __LINE__);
  }

  logger_->Log(
      INFO_L,
      format("[{} - {}] 기간의 [{} {}] 연속 선물 캔들스틱 파일이 존재합니다. "
             "최신 연속 선물 캔들스틱 데이터 업데이트를 시작합니다.",
             UtcTimestampToUtcDatetime(
                 any_cast<int64_t>(GetCellValue(klines_file, "Open Time", 0))),
             UtcTimestampToUtcDatetime(any_cast<int64_t>(GetCellValue(
                 klines_file, "Close Time", klines_file->num_rows() - 1))),
             symbol, filename_timeframe),
      __FILE__, __LINE__, true);

  // 새로운 Open Time의 시작은 Klines file의 마지막 Open Time의 다음 값
  const auto end_open_time = any_cast<int64_t>(
      GetCellValue(klines_file, "Open Time", klines_file->num_rows() - 1));
  const string& start_time = to_string(end_open_time + 1);

  const unordered_map<string, string>& params = {{"pair", symbol},
                                                 {"contractType", "PERPETUAL"},
                                                 {"interval", timeframe},
                                                 {"startTime", start_time},
                                                 {"limit", "1000"}};

  const auto& futures_klines =
      FetchKlines(continuous_klines_url_, params, true).get();

  if (const auto& transformed_klines = TransformKlines(futures_klines, true);
      !transformed_klines.empty()) {
    // 업데이트된 데이터가 있다면 Array에 저장
    const auto& arrays = GetArraysAddedKlines(transformed_klines);

    // 새로운 Array로 새로운 Table 생성
    const auto& schema = klines_file->schema();
    const auto& new_table = arrow::Table::Make(schema, arrays);

    // 기존 Table과 새로운 Table을 수직으로 결합
    const vector<shared_ptr<arrow::Table>>& tables_to_concatenate = {
        klines_file, new_table};
    const auto& concatenated_tables_result =
        ConcatenateTables(tables_to_concatenate);

    // 저장
    TableToParquet(concatenated_tables_result.ValueOrDie(), directory_path,
                   filename_timeframe + ".parquet", true, true);

    logger_->Log(
        INFO_L,
        format("[{} - {}] 기간의 [{} {}] 연속 선물 캔들스틱 파일이 "
               "업데이트되었습니다.",
               UtcTimestampToUtcDatetime(transformed_klines.front().at(0)),
               UtcTimestampToUtcDatetime(transformed_klines.back().at(6)),
               symbol, filename_timeframe),
        __FILE__, __LINE__, true);
  } else {
    logger_->Log(
        WARN_L,
        format("[{} {}] 연속 선물 캔들스틱 파일이 이미 최신 버전입니다.",
               symbol, filename_timeframe),
        __FILE__, __LINE__, true);
  }

  const auto& end = chrono::high_resolution_clock::now();
  logger_->Log(
      INFO_L,
      "소요 시간: " +
          FormatTimeDiff(
              duration_cast<chrono::milliseconds>(end - start).count()),
      __FILE__, __LINE__, true);

  Engine::LogSeparator(true);
}

void BinanceFetcher::FetchMarkPriceKlines(const string& symbol,
                                          const string& timeframe) const {
  const auto& start = chrono::high_resolution_clock::now();

  // 타임프레임 유효성 검사
  ParseTimeframe(timeframe);

  // 저장 경로 생성
  const string& timeframe_filename = GetFilenameWithTimeframe(timeframe);
  const string& save_directory = mark_price_klines_directory_ + "/" + symbol;
  const string& file_path =
      save_directory + "/" + timeframe_filename + ".parquet";
  filesystem::create_directories(save_directory);

  if (filesystem::exists(file_path)) {
    logger_->Log(
        WARN_L,
        format("[{} {}] 마크 가격 캔들스틱 파일이 [{}] 경로에 이미 존재합니다.",
               symbol, timeframe_filename, ConvertBackslashToSlash(file_path)),
        __FILE__, __LINE__, true);

    Engine::LogSeparator(true);
    return;
  }

  logger_->Log(INFO_L,
               format("[{} {}] 마크 가격 캔들스틱 파일 생성을 시작합니다.",
                      symbol, timeframe_filename),
               __FILE__, __LINE__, true);

  const unordered_map<string, string>& params = {{"symbol", symbol},
                                                 {"interval", timeframe},
                                                 {"startTime", "0"},
                                                 {"limit", "1000"}};

  // 마크 가격 캔들스틱 데이터 Fetch
  logger_->Log(INFO_L, "마크 가격 캔들스틱 데이터 요청을 시작합니다.", __FILE__,
               __LINE__, true);

  const auto& klines = FetchKlines(mark_price_klines_url_, params, true).get();

  // 캔들스틱 데이터 변환
  const auto& transformed_klines = TransformKlines(klines, true);

  // 저장
  SaveKlines(transformed_klines, save_directory,
             timeframe_filename + ".parquet", false, false);

  logger_->Log(
      INFO_L,
      format("[{} - {}] 기간의 [{} {}] 마크 가격 캔들스틱 파일이 [{}] 경로에 "
             "저장되었습니다.",
             UtcTimestampToUtcDatetime(transformed_klines.front().at(0)),
             UtcTimestampToUtcDatetime(transformed_klines.back().at(6)), symbol,
             timeframe_filename, ConvertBackslashToSlash(file_path)),
      __FILE__, __LINE__, true);

  const auto& end = chrono::high_resolution_clock::now();
  logger_->Log(
      INFO_L,
      "소요 시간: " +
          FormatTimeDiff(
              duration_cast<chrono::milliseconds>(end - start).count()),
      __FILE__, __LINE__, true);
  Engine::LogSeparator(true);
}

void BinanceFetcher::UpdateMarkPriceKlines(const string& symbol,
                                           const string& timeframe) const {
  const auto& start = chrono::high_resolution_clock::now();

  // 타임프레임 유효성 검사
  ParseTimeframe(timeframe);

  const auto& filename_timeframe = GetFilenameWithTimeframe(timeframe);
  const auto& directory_path = mark_price_klines_directory_ + "/" + symbol;
  const auto& file_path =
      directory_path + "/" + filename_timeframe + ".parquet";

  if (!filesystem::exists(file_path)) {
    logger_->Log(WARN_L,
                 format("[{} {}] 마크 가격 캔들스틱 파일이 존재하지 않아 "
                        "업데이트할 수 없습니다.",
                        symbol, filename_timeframe),
                 __FILE__, __LINE__, true);
    Engine::LogSeparator(true);
    return;
  }

  // Parquet 파일 읽기
  shared_ptr<arrow::Table> klines_file;
  try {
    klines_file = ReadParquet(file_path);
  } catch (const exception& e) {
    Logger::LogAndThrowError(e.what(), __FILE__, __LINE__);
  }

  logger_->Log(
      INFO_L,
      format("[{} - {}] 기간의 [{} {}] 마크 가격 캔들스틱 파일이 존재합니다. "
             "최신 마크 가격 캔들스틱 데이터 업데이트를 시작합니다.",
             UtcTimestampToUtcDatetime(
                 any_cast<int64_t>(GetCellValue(klines_file, "Open Time", 0))),
             UtcTimestampToUtcDatetime(any_cast<int64_t>(GetCellValue(
                 klines_file, "Close Time", klines_file->num_rows() - 1))),
             symbol, filename_timeframe),
      __FILE__, __LINE__, true);

  // 새로운 Open Time의 시작은 Klines file의 마지막 Open Time의 다음 값
  const auto end_open_time = any_cast<int64_t>(
      GetCellValue(klines_file, "Open Time", klines_file->num_rows() - 1));
  const string& start_time = to_string(end_open_time + 1);

  const unordered_map<string, string>& params = {{"symbol", symbol},
                                                 {"interval", timeframe},
                                                 {"startTime", start_time},
                                                 {"limit", "1000"}};

  const auto& mark_price_klines =
      FetchKlines(mark_price_klines_url_, params, true).get();

  if (const auto& transformed_klines = TransformKlines(mark_price_klines, true);
      !transformed_klines.empty()) {
    // 업데이트된 데이터가 있다면 Array에 저장
    const auto& arrays = GetArraysAddedKlines(transformed_klines);

    // 새로운 Array로 새로운 Table 생성
    const auto& schema = klines_file->schema();
    const auto& new_table = arrow::Table::Make(schema, arrays);

    // 기존 Table과 새로운 Table을 수직으로 결합
    const vector<shared_ptr<arrow::Table>>& tables_to_concatenate = {
        klines_file, new_table};
    const auto& concatenated_tables_result =
        ConcatenateTables(tables_to_concatenate);

    // 저장
    TableToParquet(concatenated_tables_result.ValueOrDie(), directory_path,
                   filename_timeframe + ".parquet", false, false);

    logger_->Log(
        INFO_L,
        format("[{} - {}] 기간의 [{} {}] 마크 가격 캔들스틱 파일이 "
               "업데이트되었습니다.",
               UtcTimestampToUtcDatetime(transformed_klines.front().at(0)),
               UtcTimestampToUtcDatetime(transformed_klines.back().at(6)),
               symbol, filename_timeframe),
        __FILE__, __LINE__, true);
  } else {
    logger_->Log(
        WARN_L,
        format("[{} {}] 마크 가격 캔들스틱 파일이 이미 최신 버전입니다.",
               symbol, filename_timeframe),
        __FILE__, __LINE__, true);
  }

  const auto& end = chrono::high_resolution_clock::now();
  logger_->Log(
      INFO_L,
      "소요 시간: " +
          FormatTimeDiff(
              duration_cast<chrono::milliseconds>(end - start).count()),
      __FILE__, __LINE__, true);
  Engine::LogSeparator(true);
}

void BinanceFetcher::FetchFundingRates(const string& symbol) const {
  const auto& start = chrono::high_resolution_clock::now();

  const string& file_path = funding_rates_directory_ + "/" + symbol + ".json";

  if (filesystem::exists(file_path)) {
    logger_->Log(WARN_L,
                 format("[{}] 펀딩 비율 파일이 [{}] 경로에 이미 존재합니다.",
                        symbol, ConvertBackslashToSlash(file_path)),
                 __FILE__, __LINE__, true);
    Engine::LogSeparator(true);
    return;
  }

  logger_->Log(INFO_L, format("[{}] 펀딩 비율 파일 생성을 시작합니다.", symbol),
               __FILE__, __LINE__, true);

  // startTime이 0이면 펀딩 비율은 최신 데이터만 fetch하므로 1부터 시작
  const unordered_map<string, string>& params = {
      {"symbol", symbol}, {"startTime", "1"}, {"limit", "1000"}};

  // 펀딩 비율 데이터 Fetch
  logger_->Log(INFO_L, "펀딩 비율 데이터 요청을 시작합니다.", __FILE__,
               __LINE__, true);

  const auto& funding_rates =
      FetchContinuousFundingRates(funding_rates_url_, params).get();

  // 펀딩 비율 데이터가 비어있으면 처리 중단
  if (funding_rates.empty()) {
    logger_->Log(ERROR_L, "펀딩 비율 데이터가 비어있습니다. 처리를 중단합니다.",
                 __FILE__, __LINE__, true);
    Engine::LogSeparator(true);
    return;
  }

  // 저장
  ofstream file(file_path);
  if (!file.is_open()) {
    logger_->Log(
        ERROR_L,
        format("파일을 열 수 없습니다: {}", ConvertBackslashToSlash(file_path)),
        __FILE__, __LINE__, true);
    Engine::LogSeparator(true);
    return;
  }

  // vector<json> → json array로 변환해서 저장 (fundingRate를 double로 변환)
  json output_json = json::array();
  for (const auto& funding_rate : funding_rates) {
    json transformed_rate = funding_rate;
    transformed_rate["fundingRate"] =
        stod(funding_rate["fundingRate"].get<string>());
    output_json.push_back(transformed_rate);
  }

  file << output_json.dump(4);
  file.close();

  logger_->Log(INFO_L,
               format("[{}] 펀딩 비율 파일이 [{}] 경로에 "
                      "저장되었습니다.",
                      symbol, ConvertBackslashToSlash(file_path)),
               __FILE__, __LINE__, true);

  const auto& end = chrono::high_resolution_clock::now();
  logger_->Log(
      INFO_L,
      "소요 시간: " +
          FormatTimeDiff(
              duration_cast<chrono::milliseconds>(end - start).count()),
      __FILE__, __LINE__, true);
  Engine::LogSeparator(true);
}

void BinanceFetcher::UpdateFundingRates(const string& symbol) const {
  const auto& start = chrono::high_resolution_clock::now();

  const auto& file_path = funding_rates_directory_ + "/" + symbol + ".json";

  if (!filesystem::exists(file_path)) {
    logger_->Log(WARN_L,
                 format("[{}] 펀딩 비율 파일이 존재하지 않아 "
                        "업데이트할 수 없습니다.",
                        symbol),
                 __FILE__, __LINE__, true);
    Engine::LogSeparator(true);
    return;
  }

  // json 파일 읽기
  ifstream file(file_path);
  if (!file.is_open()) {
    logger_->Log(
        ERROR_L,
        format("[{}] 펀딩 비율 파일을 열 수 없습니다: {}", symbol, file_path),
        __FILE__, __LINE__, true);
    return;
  }

  json funding_rates;
  try {
    file >> funding_rates;
  } catch (const std::exception& e) {
    logger_->Log(ERROR_L,
                 format("[{}] JSON 파싱 중 오류 발생: {}", symbol, e.what()),
                 __FILE__, __LINE__, true);
    return;
  }

  logger_->Log(INFO_L,
               format("[{} - {}] 기간의 [{}] 펀딩 비율 파일이 존재합니다. "
                      "최신 펀딩 비율 데이터 업데이트를 시작합니다.",
                      UtcTimestampToUtcDatetime(
                          funding_rates.front()["fundingTime"].get<int64_t>()),
                      UtcTimestampToUtcDatetime(
                          funding_rates.back()["fundingTime"].get<int64_t>()),
                      symbol),
               __FILE__, __LINE__, true);

  // 새로운 startTime의 시작은 파일의의 마지막 fundingTime의 다음 값 + 1 이후
  const auto end_open_time = funding_rates.back()["fundingTime"].get<int64_t>();
  const string& start_time = to_string(end_open_time + 1);

  const unordered_map<string, string>& params = {
      {"symbol", symbol}, {"startTime", start_time}, {"limit", "1000"}};

  const auto& fetch_result =
      FetchContinuousFundingRates(funding_rates_url_, params).get();

  if (!fetch_result.empty()) {
    // 기존 데이터에 새로운 데이터 추가 (fundingRate를 double로 변환)
    for (const auto& new_funding_rate : fetch_result) {
      json transformed_rate = new_funding_rate;
      transformed_rate["fundingRate"] =
          stod(new_funding_rate["fundingRate"].get<string>());
      funding_rates.push_back(transformed_rate);
    }

    // 파일에 저장
    ofstream output_file(file_path);
    if (!output_file.is_open()) {
      logger_->Log(ERROR_L,
                   format("[{}] 펀딩 비율 파일을 저장할 수 없습니다: {}",
                          symbol, file_path),
                   __FILE__, __LINE__, true);
      return;
    }

    output_file << funding_rates.dump(4);
    output_file.close();

    logger_->Log(
        INFO_L,
        format("[{} - {}] 기간의 [{}] 펀딩 비율 파일이 업데이트되었습니다.",
               UtcTimestampToUtcDatetime(
                   fetch_result.front()["fundingTime"].get<int64_t>()),
               UtcTimestampToUtcDatetime(
                   fetch_result.back()["fundingTime"].get<int64_t>()),
               symbol),
        __FILE__, __LINE__, true);
  } else {
    logger_->Log(WARN_L,
                 format("[{}] 펀딩 비율 파일이 이미 최신 버전입니다.", symbol),
                 __FILE__, __LINE__, true);
  }

  const auto& end = chrono::high_resolution_clock::now();
  logger_->Log(
      INFO_L,
      "소요 시간: " +
          FormatTimeDiff(
              duration_cast<chrono::milliseconds>(end - start).count()),
      __FILE__, __LINE__, true);
  Engine::LogSeparator(true);
}

void BinanceFetcher::FetchExchangeInfo() const {
  const string& save_path = market_data_directory_ + "/exchange_info.json";

  try {
    JsonToFile(Fetch(exchange_info_url_), save_path);
  } catch (const exception& e) {
    logger_->Log(ERROR_L, "\n" + string(e.what()), __FILE__, __LINE__, true);
    Logger::LogAndThrowError(
        "바이낸스 거래소 정보 파일을 생성하는 데 실패했습니다.", __FILE__,
        __LINE__);
  }

  logger_->Log(INFO_L,
               format("바이낸스 거래소 정보 파일이 [{}] 경로에 저장되었습니다.",
                      ConvertBackslashToSlash(save_path)),
               __FILE__, __LINE__, true);
}

void BinanceFetcher::FetchLeverageBracket() const {
  const string& save_path = market_data_directory_ + "/leverage_bracket.json";

  try {
    JsonToFile(Fetch(leverage_bracket_url_,
                     {{"timestamp", to_string(GetServerTime())}}, true, false,
                     header_, api_key_env_var_, api_secret_env_var_),
               save_path);
  } catch (const exception& e) {
    logger_->Log(ERROR_L, "\n" + string(e.what()), __FILE__, __LINE__, true);
    Logger::LogAndThrowError(
        "바이낸스 레버리지 구간 파일을 생성하는 데 실패했습니다.", __FILE__,
        __LINE__);
  }

  logger_->Log(
      INFO_L,
      format("바이낸스 레버리지 구간 파일이 [{}] 경로에 저장되었습니다.",
             ConvertBackslashToSlash(save_path)),
      __FILE__, __LINE__, true);
}

future<vector<json>> BinanceFetcher::FetchKlines(
    const string& url, const unordered_map<string, string>& params,
    const bool forward) {
  return async(launch::async, [=] {
    deque<json> result;
    auto param = params;

    while (true) {
      try {
        // fetched_future를 미리 받아둠
        auto fetched_future = Fetch(url, param);

        // Fetch 대기
        const auto& fetched_data = fetched_future.get();

        // fetch 해온 데이터가 비어있거나 잘못된 데이터면 종료
        if (fetched_data.empty() ||
            (fetched_data.contains("code") && fetched_data["code"] == -1121)) {
          break;
        }

        logger_->Log(INFO_L,
                     format("[{} - {}] 요청 완료",
                            UtcTimestampToUtcDatetime(
                                fetched_data.front().at(0).get<int64_t>()),
                            UtcTimestampToUtcDatetime(
                                fetched_data.back().at(6).get<int64_t>())),
                     __FILE__, __LINE__, true);

        if (forward) {
          // 앞부터 순회하여 뒤에 붙임
          for (const auto& data : fetched_data) {
            result.push_back(data);
          }

          // 다음 startTime은 마지막 startTime의 뒤 시간
          param["startTime"] =
              to_string(result.back().at(0).get<int64_t>() + 1);
        } else {
          // 뒤부터 순회하여 앞에 붙임
          for (auto kline = fetched_data.rbegin(); kline != fetched_data.rend();
               ++kline)
            result.push_front(*kline);

          // 다음 entTime은 첫 startTime의 앞 시간
          param["endTime"] = to_string(result.front().at(0).get<int64_t>() - 1);
        }
      } catch (const exception& e) {
        logger_->Log(ERROR_L, "\n" + string(e.what()), __FILE__, __LINE__,
                     true);
        Logger::LogAndThrowError("데이터를 요청하는 중 에러가 발생했습니다.",
                                 __FILE__, __LINE__);
      }
    }

    if (!result.empty()) {
      logger_->Log(
          INFO_L,
          format("[{} - {}] 기간의 데이터가 요청 완료 되었습니다.",
                 UtcTimestampToUtcDatetime(result.front().at(0).get<int64_t>()),
                 UtcTimestampToUtcDatetime(result.back().at(6).get<int64_t>())),
          __FILE__, __LINE__, true);
    } else {
      logger_->Log(INFO_L, "요청한 데이터가 비어있습니다.", __FILE__, __LINE__,
                   true);
    }

    return vector(result.begin(), result.end());
  });
}

future<vector<json>> BinanceFetcher::FetchContinuousFundingRates(
    const string& url, const unordered_map<string, string>& params) {
  return async(launch::async, [=] {
    vector<json> result;
    auto param = params;

    while (true) {
      try {
        // fetched_future를 미리 받아둠
        auto fetched_future = Fetch(url, param);

        // Fetch 대기
        const auto& fetched_data = fetched_future.get();

        // fetch 해온 데이터가 비어있거나 잘못된 데이터면 종료
        if (fetched_data.empty() ||
            (fetched_data.contains("code") && fetched_data["code"] == -1121)) {
          break;
        }

        logger_->Log(
            INFO_L,
            format("[{} - {}] 요청 완료",
                   UtcTimestampToUtcDatetime(
                       fetched_data.front()["fundingTime"].get<int64_t>()),
                   UtcTimestampToUtcDatetime(
                       fetched_data.back()["fundingTime"].get<int64_t>())),
            __FILE__, __LINE__, true);

        // 뒤에 붙임
        for (const auto& data : fetched_data) {
          result.push_back(data);
        }

        // 다음 startTime은 마지막 startTime의 뒤 시간
        param["startTime"] =
            to_string(result.back()["fundingTime"].get<int64_t>() + 1);
      } catch (const exception& e) {
        logger_->Log(ERROR_L, "\n" + string(e.what()), __FILE__, __LINE__,
                     true);
        Logger::LogAndThrowError("데이터를 요청하는 중 에러가 발생했습니다.",
                                 __FILE__, __LINE__);
      }
    }

    if (!result.empty()) {
      logger_->Log(INFO_L,
                   format("[{} - {}] 기간의 데이터가 요청 완료 되었습니다.",
                          UtcTimestampToUtcDatetime(
                              result.front()["fundingTime"].get<int64_t>()),
                          UtcTimestampToUtcDatetime(
                              result.back()["fundingTime"].get<int64_t>())),
                   __FILE__, __LINE__, true);
    } else {
      logger_->Log(INFO_L, "요청한 데이터가 비어있습니다.", __FILE__, __LINE__,
                   true);
    }

    return result;
  });
}

string BinanceFetcher::GetFilenameWithTimeframe(const string& timeframe) {
  // 윈도우는 1m과 1M이 같은 것으로 취급하므로 명시적 이름 변환이 필요함
  if (timeframe == "1M") {
    return "1mo";
  }

  return timeframe;
}

vector<json> BinanceFetcher::TransformKlines(const vector<json>& klines,
                                             const bool drop_latest) {
  logger_->Log(INFO_L, "데이터 변환을 시작합니다.", __FILE__, __LINE__, true);

  const size_t size = drop_latest ? klines.size() - 1 : klines.size();
  vector<json> transformed_klines(size);

#pragma omp parallel for
  for (int i = 0; i < size; i++) {
    const auto& kline = klines[i];

    try {
      json bar;

      // Open Time 추가
      bar.push_back(static_cast<int64_t>(kline[0]));

      // OHLCV 추가
      // 가격 데이터는 String 타입으로 들어오기 때문에 String을 Double로 변환
      for (int j = 1; j <= 5; j++) {
        bar.push_back(stod(kline[j].get<string>()));
      }

      // Close Time 추가
      bar.push_back(static_cast<int64_t>(kline[6]));

      // 미리 할당한 위치에 저장
      transformed_klines[i] = bar;
    } catch (const exception& e) {
#pragma omp critical
      {
        const string& err =
            format("데이터 변환 중 에러가 발생했습니다: {}", e.what());
        logger_->Log(WARN_L, err, __FILE__, __LINE__, true);
      }

      // 에러 발생 시 빈 JSON 객체 저장
      transformed_klines[i] = json();
    }
  }

  return transformed_klines;
}

vector<json> BinanceFetcher::ConcatKlines(const vector<json>& spot_klines,
                                          const vector<json>& futures_klines) {
  // 연결 시점의 시간 정보 로깅
  logger_->Log(
      INFO_L,
      format(
          "현물-선물 연결 시점: 현물 마지막 바({}) - 선물 첫 바({})",
          UtcTimestampToUtcDatetime(spot_klines.back()[0].get<int64_t>()),
          UtcTimestampToUtcDatetime(futures_klines.front()[0].get<int64_t>())),
      __FILE__, __LINE__, true);

  // 현물 데이터를 그대로 복사
  vector<json> combined_klines = spot_klines;

  // futures 데이터를 한 번에 추가
  combined_klines.insert(combined_klines.end(), futures_klines.begin(),
                         futures_klines.end());

  return combined_klines;
}

void BinanceFetcher::SaveKlines(const vector<json>& klines,
                                const string& directory_path,
                                const string& file_name,
                                const bool save_split_files,
                                const bool reset_directory) {
  logger_->Log(INFO_L, "데이터 저장을 시작합니다.", __FILE__, __LINE__, true);

  // column 추가
  const vector<string>& column_names{"Open Time", "Open",   "High",      "Low",
                                     "Close",     "Volume", "Close Time"};

  vector<shared_ptr<arrow::Field>> arrow_fields;
  for (const auto& column_name : column_names) {
    const auto& field_type =
        column_name == "Open Time" || column_name == "Close Time"
            ? arrow::int64()
            : arrow::float64();
    arrow_fields.push_back(field(column_name, field_type));
  }

  // 스키마 생성
  const auto& schema = arrow::schema(arrow_fields);

  // Klines를 Array에 추가
  const auto& arrays = GetArraysAddedKlines(klines);

  // Table 생성
  const auto& table = arrow::Table::Make(schema, arrays);

  // 저장
  TableToParquet(table, directory_path, file_name, save_split_files,
                 reset_directory);
}

vector<shared_ptr<arrow::Array>> BinanceFetcher::GetArraysAddedKlines(
    const vector<json>& klines) {
  const size_t num_rows = klines.size();

  // 임시 데이터 저장소
  vector<int64_t> open_time(num_rows);
  vector<double> open(num_rows);
  vector<double> high(num_rows);
  vector<double> low(num_rows);
  vector<double> close(num_rows);
  vector<double> volume(num_rows);
  vector<int64_t> close_time(num_rows);

#pragma omp parallel for
  for (int row = 0; row < num_rows; ++row) {
    const auto& json = klines[row];
    open_time[row] = json[0].get<int64_t>();
    open[row] = json[1].get<double>();
    high[row] = json[2].get<double>();
    low[row] = json[3].get<double>();
    close[row] = json[4].get<double>();
    volume[row] = json[5].get<double>();
    close_time[row] = json[6].get<int64_t>();
  }

  // 빌더 생성
  vector<shared_ptr<arrow::ArrayBuilder>> builders(7);
  builders[0] = make_shared<arrow::Int64Builder>();  // Open Time
  for (int i = 1; i <= 5; i++) {
    builders[i] = make_shared<arrow::DoubleBuilder>();  // OHLCV
  }
  builders[6] = make_shared<arrow::Int64Builder>();  // Close Time

  // 데이터를 Builder에 추가 후 Array에 추가
  vector<shared_ptr<arrow::Array>> arrays(7);
  const vector<vector<double>*>& price_data = {nullptr, &open,   &high,  &low,
                                               &close,  &volume, nullptr};
  const vector<vector<int64_t>*>& time_data = {
      &open_time, nullptr, nullptr, nullptr, nullptr, nullptr, &close_time};

  for (int i = 0; i < 7; ++i) {
    if (i == 0 || i == 6) {  // 시간 데이터
      auto builder = make_shared<arrow::Int64Builder>();
      if (auto* int64_builder = builder.get();
          !int64_builder->AppendValues(*time_data[i]).ok() ||
          !int64_builder->Finish(&arrays[i]).ok()) {
        Logger::LogAndThrowError(
            format("{} time 데이터를 처리하는 데 실패했습니다.",
                   (i == 0 ? "Open" : "Close")),
            __FILE__, __LINE__);
      }
    } else {  // 가격 데이터
      auto builder = make_shared<arrow::DoubleBuilder>();
      if (auto* double_builder = builder.get();
          !double_builder->AppendValues(*price_data[i]).ok() ||
          !double_builder->Finish(&arrays[i]).ok()) {
        Logger::LogAndThrowError("가격 데이터를 처리하는 데 실패했습니다.",
                                 __FILE__, __LINE__);
      }
    }
  }

  return arrays;
}

string BinanceFetcher::ConvertBackslashToSlash(const string& path_string) {
  string converted = path_string;

  ranges::replace(converted, '\\', '/');

  return converted;
}

int64_t BinanceFetcher::GetServerTime() {
  try {
    return Fetch(server_time_url_).get()["serverTime"];
  } catch (const exception& e) {
    logger_->Log(ERROR_L, "\n" + string(e.what()), __FILE__, __LINE__, true);
    Logger::LogAndThrowError("서버 시간의 요청이 실패했습니다.", __FILE__,
                             __LINE__);
  }

  return 0;
}

}  // namespace backtesting::fetcher
