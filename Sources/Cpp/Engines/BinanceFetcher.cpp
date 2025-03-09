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

  data_path_ = filesystem::absolute("../../Data").string();
  continuous_klines_path_ = data_path_ + "/Continuous Klines";
  mark_price_klines_path_ = data_path_ + "/Mark Price Klines";
  funding_rates_path_ = data_path_ + "/Funding Rates";

  if (!filesystem::exists(data_path_)) {
    filesystem::create_directory(data_path_);
  }

  if (!filesystem::exists(continuous_klines_path_)) {
    filesystem::create_directory(continuous_klines_path_);
  }

  if (!filesystem::exists(mark_price_klines_path_)) {
    filesystem::create_directory(mark_price_klines_path_);
  }

  if (!filesystem::exists(funding_rates_path_)) {
    filesystem::create_directory(funding_rates_path_);
  }
}
BinanceFetcher::BinanceFetcher(string api_key_env_var,
                               string api_secret_env_var,
                               string market_data_path) {
  api_key_env_var_ = move(api_key_env_var);
  api_secret_env_var_ = move(api_secret_env_var);

  data_path_ = move(market_data_path);
  continuous_klines_path_ = data_path_ + "/Continuous Klines";
  mark_price_klines_path_ = data_path_ + "/Mark Price Klines";
  funding_rates_path_ = data_path_ + "/Funding Rates";

  if (!filesystem::exists(data_path_)) {
    filesystem::create_directory(data_path_);
  }

  if (!filesystem::exists(continuous_klines_path_)) {
    filesystem::create_directory(continuous_klines_path_);
  }

  if (!filesystem::exists(mark_price_klines_path_)) {
    filesystem::create_directory(mark_price_klines_path_);
  }

  if (!filesystem::exists(funding_rates_path_)) {
    filesystem::create_directory(funding_rates_path_);
  }
}

shared_ptr<Logger>& BinanceFetcher::logger_ = Logger::GetLogger();

string BinanceFetcher::header_ = "X-MBX-APIKEY: ";
string BinanceFetcher::futures_endpoint_ = "https://fapi.binance.com";
string BinanceFetcher::spot_endpoint_ = "https://api.binance.com";

string BinanceFetcher::server_time_url_ = futures_endpoint_ + "/fapi/v1/time";
string BinanceFetcher::continuous_klines_url_ =
    futures_endpoint_ + "/fapi/v1/continuousKlines";
string BinanceFetcher::spot_klines_url_ = spot_endpoint_ + "/api/v3/klines";
string BinanceFetcher::mark_price_klines_url_ =
    futures_endpoint_ + "/fapi/v1/markPriceKlines";
string BinanceFetcher::exchange_info_url_ =
    futures_endpoint_ + "/fapi/v1/exchangeInfo";
string BinanceFetcher::leverage_bracket_url_ =
    futures_endpoint_ + "/fapi/v1/leverageBracket";

void BinanceFetcher::FetchContinuousKlines(const string& symbol,
                                           const string& timeframe) const {
  const auto& start = chrono::high_resolution_clock::now();

  // 저장 경로 생성
  const string& timeframe_filename = GetFilenameWithTimeframe(timeframe);
  const string& symbol_path = continuous_klines_path_ + "/" + symbol;
  const string& save_path = symbol_path + "/" + timeframe_filename + ".parquet";
  filesystem::create_directories(symbol_path);

  if (filesystem::exists(save_path)) {
    logger_->Log(
        WARNING_L,
        format("[{} {}] 연속 선물 캔들스틱 파일이 [{}] 경로에 이미 존재합니다.",
               symbol, timeframe_filename, ConvertBackslashToSlash(save_path)),
        __FILE__, __LINE__);
    PrintSeparator();
    return;
  }

  logger_->Log(INFO_L,
               format("[{} {}] 연속 선물 캔들스틱 파일 생성을 시작합니다.",
                      symbol, timeframe_filename),
               __FILE__, __LINE__);

  const unordered_map<string, string>& futures_params = {
      {"pair", symbol},
      {"contractType", "PERPETUAL"},
      {"interval", timeframe},
      {"startTime", "0"},
      {"limit", "1000"}};

  // 연속 선물 캔들스틱 데이터 Fetch
  logger_->Log(INFO_L, "연속 선물 캔들스틱 데이터 요청을 시작합니다.", __FILE__,
               __LINE__);

  const auto& futures_klines =
      FetchKlines(continuous_klines_url_, futures_params, true).get();

  // 연속 선물 캔들스틱 데이터 변환
  const auto& transformed_futures_klines =
      TransformKlines(futures_klines, true);

  // json deque의 첫 값을 가져오고 첫 번째 요소인 Open Time을 가져옴
  const auto& open_time = futures_klines.front().at(0);

  // Spot의 마지막 시간은 Future 첫 시간의 전 시간
  const string& end_time = to_string(static_cast<int64_t>(open_time) - 1);

  const unordered_map<string, string>& spot_params = {{"symbol", symbol},
                                                      {"interval", timeframe},
                                                      {"endTime", end_time},
                                                      {"limit", "1000"}};

  // 현물 캔들스틱 데이터 Fetch
  logger_->Log(INFO_L, "현물 캔들스틱 데이터 요청을 시작합니다.", __FILE__,
               __LINE__);

  if (const auto& spot_klines =
          FetchKlines(spot_klines_url_, spot_params, false).get();
      !spot_klines.empty()) {
    // 현물 캔들스틱 데이터 변환
    const auto& transformed_spot_klines = TransformKlines(spot_klines, false);

    // 현물과 선물 캔들스틱 데이터 병합
    const auto& spot_futures_klines =
        ConcatKlines(transformed_spot_klines, transformed_futures_klines);

    // 저장
    SaveKlines(spot_futures_klines, save_path);

    logger_->Log(
        INFO_L,
        format("[{} - {}] 기간의 [{} {}] 연속 선물 캔들스틱 파일이 [{}] 경로에 "
               "저장되었습니다.",
               UtcTimestampToUtcDatetime(spot_futures_klines.front().at(0)),
               UtcTimestampToUtcDatetime(spot_futures_klines.back().at(6)),
               symbol, timeframe_filename, ConvertBackslashToSlash(save_path)),
        __FILE__, __LINE__);
  } else {
    // 과거의 현물 데이터가 없는 종목이면 선물만 저장
    SaveKlines(transformed_futures_klines, save_path);

    logger_->Log(
        INFO_L,
        format(
            "[{} - {}] 기간의 [{} {}] 연속 선물 캔들스틱 파일이 [{}] 경로에 "
            "저장되었습니다.",
            UtcTimestampToUtcDatetime(transformed_futures_klines.front().at(0)),
            UtcTimestampToUtcDatetime(transformed_futures_klines.back().at(6)),
            symbol, timeframe_filename, ConvertBackslashToSlash(save_path)),
        __FILE__, __LINE__);
  }

  const auto& end = chrono::high_resolution_clock::now();
  logger_->Log(
      INFO_L,
      "소요 시간: " +
          FormatTimeDiff(
              duration_cast<chrono::milliseconds>(end - start).count()),
      __FILE__, __LINE__);
  PrintSeparator();
}

void BinanceFetcher::UpdateContinuousKlines(const string& symbol,
                                            const string& timeframe) const {
  const auto& start = chrono::high_resolution_clock::now();

  const string& filename_timeframe = GetFilenameWithTimeframe(timeframe);
  const string& file_path = continuous_klines_path_ + "/" + symbol + "/" +
                            filename_timeframe + ".parquet";

  if (!filesystem::exists(file_path)) {
    logger_->Log(WARNING_L,
                 format("[{} {}] 연속 선물 캔들스틱 파일이 존재하지 않아 "
                        "업데이트할 수 없습니다.",
                        symbol, filename_timeframe),
                 __FILE__, __LINE__);
    PrintSeparator();
    return;
  }

  // Parquet 파일 읽기
  const auto& klines_file = ReadParquet(file_path);

  logger_->Log(
      INFO_L,
      format("[{} - {}] 기간의 [{} {}] 연속 선물 캔들스틱 파일이 존재합니다. "
             "최신 연속 선물 캔들스틱 데이터 업데이트를 시작합니다.",
             UtcTimestampToUtcDatetime(
                 any_cast<int64_t>(GetCellValue(klines_file, "Open Time", 0))),
             UtcTimestampToUtcDatetime(any_cast<int64_t>(GetCellValue(
                 klines_file, "Close Time", klines_file->num_rows() - 1))),
             symbol, filename_timeframe),
      __FILE__, __LINE__);

  // 새로운 Open Time의 시작은 Klines file의 마지막 Open Time의 다음 값
  const auto end_open_time = any_cast<int64_t>(
      GetCellValue(klines_file, "Open Time", klines_file->num_rows() - 1));
  const string& start_time = to_string(end_open_time + 1);

  const unordered_map<string, string>& futures_params = {
      {"pair", symbol},
      {"contractType", "PERPETUAL"},
      {"interval", timeframe},
      {"startTime", start_time},
      {"limit", "1000"}};

  const auto& futures_klines =
      FetchKlines(continuous_klines_url_, futures_params, true).get();

  if (const auto& transformed_futures_klines =
          TransformKlines(futures_klines, true);
      !transformed_futures_klines.empty()) {
    // 업데이트된 데이터가 있다면 Array에 저장
    const auto& arrays = GetArraysAddedKlines(transformed_futures_klines);

    // 새로운 Array로 새로운 Table 생성
    const auto& schema = klines_file->schema();
    const auto& new_table = arrow::Table::Make(schema, arrays);

    // 기존 Table과 새로운 Table을 수직으로 결합
    const vector<shared_ptr<arrow::Table>>& tables_to_concatenate = {
        klines_file, new_table};
    const auto& concatenated_tables_result =
        ConcatenateTables(tables_to_concatenate);

    // 저장
    TableToParquet(concatenated_tables_result.ValueOrDie(), file_path);

    logger_->Log(
        INFO_L,
        format(
            "[{} - {}] 기간의 [{} {}] 연속 선물 캔들스틱 파일이 "
            "업데이트되었습니다.",
            UtcTimestampToUtcDatetime(transformed_futures_klines.front().at(0)),
            UtcTimestampToUtcDatetime(transformed_futures_klines.back().at(6)),
            symbol, filename_timeframe),
        __FILE__, __LINE__);
  } else {
    logger_->Log(
        WARNING_L,
        format("[{} {}] 연속 선물 캔들스틱 파일이 이미 최신 버전입니다.",
               symbol, filename_timeframe),
        __FILE__, __LINE__);
  }

  const auto& end = chrono::high_resolution_clock::now();
  logger_->Log(
      INFO_L,
      "소요 시간: " +
          FormatTimeDiff(
              duration_cast<chrono::milliseconds>(end - start).count()),
      __FILE__, __LINE__);
  PrintSeparator();
}

void BinanceFetcher::FetchMarkPriceKlines(const string& symbol,
                                          const string& timeframe) const {
  const auto& start = chrono::high_resolution_clock::now();

  // 저장 경로 생성
  const string& timeframe_filename = GetFilenameWithTimeframe(timeframe);
  const string& symbol_path = mark_price_klines_path_ + "/" + symbol;
  const string& save_path = symbol_path + "/" + timeframe_filename + ".parquet";
  filesystem::create_directories(symbol_path);

  if (filesystem::exists(save_path)) {
    logger_->Log(
        WARNING_L,
        format("[{} {}] 마크 가격 캔들스틱 파일이 [{}] 경로에 이미 존재합니다.",
               symbol, timeframe_filename, ConvertBackslashToSlash(save_path)),
        __FILE__, __LINE__);
    PrintSeparator();
    return;
  }

  logger_->Log(INFO_L,
               format("[{} {}] 마크 가격 캔들스틱 파일 생성을 시작합니다.",
                      symbol, timeframe_filename),
               __FILE__, __LINE__);

  const unordered_map<string, string>& mark_price_params = {
      {"symbol", symbol},
      {"interval", timeframe},
      {"startTime", "0"},
      {"limit", "1000"}};

  // 마크 가격 캔들스틱 데이터 Fetch
  logger_->Log(INFO_L, "마크 가격 캔들스틱 데이터 요청을 시작합니다.", __FILE__,
               __LINE__);

  const auto& mark_price_klines =
      FetchKlines(mark_price_klines_url_, mark_price_params, true).get();

  // 마크 가격 캔들스틱 데이터 변환
  const auto& transformed_mark_price_klines =
      TransformKlines(mark_price_klines, true);

  // 저장
  SaveKlines(transformed_mark_price_klines, save_path);

  logger_->Log(
      INFO_L,
      format(
          "[{} - {}] 기간의 [{} {}] 마크 가격 캔들스틱 파일이 [{}] 경로에 "
          "저장되었습니다.",
          UtcTimestampToUtcDatetime(
              transformed_mark_price_klines.front().at(0)),
          UtcTimestampToUtcDatetime(transformed_mark_price_klines.back().at(6)),
          symbol, timeframe_filename, ConvertBackslashToSlash(save_path)),
      __FILE__, __LINE__);

  const auto& end = chrono::high_resolution_clock::now();
  logger_->Log(
      INFO_L,
      "소요 시간: " +
          FormatTimeDiff(
              duration_cast<chrono::milliseconds>(end - start).count()),
      __FILE__, __LINE__);
  PrintSeparator();
}

void BinanceFetcher::UpdateMarkPriceKlines(const string& symbol,
                                           const string& timeframe) const {
  const auto& start = chrono::high_resolution_clock::now();

  const string& filename_timeframe = GetFilenameWithTimeframe(timeframe);
  const string& file_path = mark_price_klines_path_ + "/" + symbol + "/" +
                            filename_timeframe + ".parquet";

  if (!filesystem::exists(file_path)) {
    logger_->Log(WARNING_L,
                 format("[{} {}] 마크 가격 캔들스틱 파일이 존재하지 않아 "
                        "업데이트할 수 없습니다.",
                        symbol, filename_timeframe),
                 __FILE__, __LINE__);
    PrintSeparator();
    return;
  }

  // Parquet 파일 읽기
  const auto& klines_file = ReadParquet(file_path);

  logger_->Log(
      INFO_L,
      format("[{} - {}] 기간의 [{} {}] 마크 가격 캔들스틱 파일이 존재합니다. "
             "최신 마크 가격 캔들스틱 데이터 업데이트를 시작합니다.",
             UtcTimestampToUtcDatetime(
                 any_cast<int64_t>(GetCellValue(klines_file, "Open Time", 0))),
             UtcTimestampToUtcDatetime(any_cast<int64_t>(GetCellValue(
                 klines_file, "Close Time", klines_file->num_rows() - 1))),
             symbol, filename_timeframe),
      __FILE__, __LINE__);

  // 새로운 Open Time의 시작은 Klines file의 마지막 Open Time의 다음 값
  const auto end_open_time = any_cast<int64_t>(
      GetCellValue(klines_file, "Open Time", klines_file->num_rows() - 1));
  const string& start_time = to_string(end_open_time + 1);

  const unordered_map<string, string>& mark_price_params = {
      {"symbol", symbol},
      {"interval", timeframe},
      {"startTime", start_time},
      {"limit", "1000"}};

  const auto& mark_price_klines =
      FetchKlines(mark_price_klines_url_, mark_price_params, true).get();

  if (const auto& transformed_mark_price_klines =
          TransformKlines(mark_price_klines, true);
      !transformed_mark_price_klines.empty()) {
    // 업데이트된 데이터가 있다면 Array에 저장
    const auto& arrays = GetArraysAddedKlines(transformed_mark_price_klines);

    // 새로운 Array로 새로운 Table 생성
    const auto& schema = klines_file->schema();
    const auto& new_table = arrow::Table::Make(schema, arrays);

    // 기존 Table과 새로운 Table을 수직으로 결합
    const vector<shared_ptr<arrow::Table>>& tables_to_concatenate = {
        klines_file, new_table};
    const auto& concatenated_tables_result =
        ConcatenateTables(tables_to_concatenate);

    // 저장
    TableToParquet(concatenated_tables_result.ValueOrDie(), file_path);

    logger_->Log(INFO_L,
                 format("[{} - {}] 기간의 [{} {}] 마크 가격 캔들스틱 파일이 "
                        "업데이트되었습니다.",
                        UtcTimestampToUtcDatetime(
                            transformed_mark_price_klines.front().at(0)),
                        UtcTimestampToUtcDatetime(
                            transformed_mark_price_klines.back().at(6)),
                        symbol, filename_timeframe),
                 __FILE__, __LINE__);
  } else {
    logger_->Log(
        WARNING_L,
        format("[{} {}] 마크 가격 캔들스틱 파일이 이미 최신 버전입니다.",
               symbol, filename_timeframe),
        __FILE__, __LINE__);
  }

  const auto& end = chrono::high_resolution_clock::now();
  logger_->Log(
      INFO_L,
      "소요 시간: " +
          FormatTimeDiff(
              duration_cast<chrono::milliseconds>(end - start).count()),
      __FILE__, __LINE__);
  PrintSeparator();
}

void BinanceFetcher::FetchExchangeInfo() const {
  const string& save_path = data_path_ + "/exchange_info.json";

  try {
    JsonToFile(Fetch(exchange_info_url_), save_path);
  } catch (const exception& e) {
    logger_->Log(ERROR_L, e.what(), __FILE__, __LINE__);
    Logger::LogAndThrowError(
        "바이낸스 거래소 정보 파일을 생성하는 데 실패했습니다.", __FILE__,
        __LINE__);
  }

  logger_->Log(INFO_L,
               format("바이낸스 거래소 정보 파일이 [{}] 경로에 저장되었습니다.",
                      ConvertBackslashToSlash(save_path)),
               __FILE__, __LINE__);
}

void BinanceFetcher::FetchLeverageBracket() const {
  const string& save_path = data_path_ + "/leverage_bracket.json";

  try {
    JsonToFile(Fetch(leverage_bracket_url_,
                     {{"timestamp", to_string(GetServerTime())}}, true, false,
                     header_, api_key_env_var_, api_secret_env_var_),
               save_path);
  } catch (const exception& e) {
    logger_->Log(ERROR_L, e.what(), __FILE__, __LINE__);
    Logger::LogAndThrowError(
        "바이낸스 레버리지 구간 파일을 생성하는 데 실패했습니다.", __FILE__,
        __LINE__);
  }

  logger_->Log(
      INFO_L,
      format("바이낸스 레버리지 구간 파일이 [{}] 경로에 저장되었습니다.",
             ConvertBackslashToSlash(save_path)),
      __FILE__, __LINE__);
}

future<vector<json>> BinanceFetcher::FetchKlines(
    const string& url, const unordered_map<string, string>& params,
    const bool forward) {
  return async(launch::async, [=] {
    deque<json> klines;
    auto param = params;

    while (true) {
      try {
        // fetched_future를 미리 받아둠
        auto fetched_future = Fetch(url, param);

        // Fetch 대기
        const auto& fetched_klines = fetched_future.get();

        // fetch 해온 데이터가 비어있거나 잘못된 데이터면 종료
        if (fetched_klines.empty() || (fetched_klines.contains("code") &&
                                       fetched_klines["code"] == -1121)) {
          break;
        }

        logger_->Log(INFO_L,
                     format("[{} - {}] 요청 완료",
                            UtcTimestampToUtcDatetime(
                                fetched_klines.front().at(0).get<int64_t>()),
                            UtcTimestampToUtcDatetime(
                                fetched_klines.back().at(6).get<int64_t>())),
                     __FILE__, __LINE__);

        if (forward) {
          // 앞부터 순회하여 뒤에 붙임
          for (const auto& kline : fetched_klines) {
            klines.push_back(kline);
          }

          // 다음 startTime은 마지막 startTime의 뒤 시간
          param["startTime"] =
              to_string(klines.back().at(0).get<int64_t>() + 1);
        } else {
          // 뒤부터 순회하여 앞에 붙임
          for (auto kline = fetched_klines.rbegin();
               kline != fetched_klines.rend(); ++kline)
            klines.push_front(*kline);

          // 다음 entTime은 첫 startTime의 앞 시간
          param["endTime"] = to_string(klines.front().at(0).get<int64_t>() - 1);
        }
      } catch (const exception& e) {
        logger_->Log(ERROR_L, e.what(), __FILE__, __LINE__);
        Logger::LogAndThrowError("데이터를 요청하는 중 에러가 발생했습니다.",
                                 __FILE__, __LINE__);
      }
    }

    logger_->Log(
        INFO_L,
        format("[{} - {}] 기간의 데이터가 요청 완료 되었습니다.",
               UtcTimestampToUtcDatetime(klines.front().at(0).get<int64_t>()),
               UtcTimestampToUtcDatetime(klines.back().at(6).get<int64_t>())),
        __FILE__, __LINE__);

    return vector(klines.begin(), klines.end());
  });
}

string BinanceFetcher::GetFilenameWithTimeframe(const string& timeframe) {
  // 윈도우는 1m과 1M이 같은 것으로 취급하므로 명시적 이름 변환이 필요함
  if (timeframe == "1M") {
    return "1month";
  }

  return timeframe;
}

vector<json> BinanceFetcher::TransformKlines(const vector<json>& klines,
                                             const bool drop_latest) {
  logger_->Log(INFO_L, "데이터 변환을 시작합니다.", __FILE__, __LINE__);

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
        logger_->Log(WARNING_L, err, __FILE__, __LINE__);
      }

      // 에러 발생 시 빈 JSON 객체 저장
      transformed_klines[i] = json();
    }
  }

  return transformed_klines;
}

vector<json> BinanceFetcher::ConcatKlines(const vector<json>& spot_klines,
                                          const vector<json>& futures_klines) {
  // Futures 첫 데이터 시가 / Spot 마지막 데이터 종가
  const double ratio = static_cast<double>(futures_klines.front()[1]) /
                       static_cast<double>(spot_klines.back()[4]);

  vector<json> adjusted_spot_klines(spot_klines.size());
  size_t decimal_places = 0;

#pragma omp parallel for
  for (int i = 0; i < spot_klines.size(); i++) {
    json bar;
    const auto& kline = spot_klines[i];

    // Open Time 추가
    bar.push_back(kline[0]);

    // 해당 행 원본 OHLC의 최대 소숫점 자릿수 Count
    decimal_places =
        max({CountDecimalPlaces(kline[1]), CountDecimalPlaces(kline[2]),
             CountDecimalPlaces(kline[3]), CountDecimalPlaces(kline[4])});

    // OHLC 추가
    for (int j = 1; j <= 4; j++) {
      bar.push_back(
          RoundToDecimalPlaces(kline[j].get<double>() * ratio, decimal_places));
    }

    // 거래량, Close Time 추가
    bar.push_back(kline[5]);
    bar.push_back(kline[6]);

    adjusted_spot_klines[i] = bar;  // 미리 할당된 위치에 저장
  }

  // futures 데이터를 순차적으로 추가
  adjusted_spot_klines.reserve(adjusted_spot_klines.size() +
                               futures_klines.size());
  for (const auto& kline : futures_klines) {
    adjusted_spot_klines.push_back(kline);
  }

  return adjusted_spot_klines;
}

void BinanceFetcher::SaveKlines(const vector<json>& klines,
                                const string& file_path) {
  logger_->Log(INFO_L, "데이터 저장을 시작합니다.", __FILE__, __LINE__);

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
  TableToParquet(table, file_path);
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
    logger_->Log(ERROR_L, e.what(), __FILE__, __LINE__);
    Logger::LogAndThrowError("서버 시간의 요청이 실패했습니다.", __FILE__,
                             __LINE__);
  }

  return 0;
}

}  // namespace backtesting::fetcher