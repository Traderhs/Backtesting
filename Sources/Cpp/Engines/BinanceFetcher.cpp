// 표준 라이브러리
#include <deque>
#include <format>
#include <future>
#include <string>
#include <utility>

// 외부 라이브러리
#include <arrow/io/file.h>

// 파일 헤더
#include "Engines/BinanceFetcher.hpp"

// 내부 헤더
#include "Engines/DataUtils.hpp"
#include "Engines/Logger.hpp"
#include "Engines/TimeUtils.hpp"

// 네임 스페이스
using namespace data_utils;
using namespace filesystem;
using namespace time_utils;

BinanceFetcher::BinanceFetcher() : market_data_path_("../../Data") {
  klines_path_ = market_data_path_ + "/Klines";
  funding_rate_path_ = market_data_path_ + "/Funding Rate";

  if (!exists(klines_path_)) create_directory(klines_path_);
  if (!exists(funding_rate_path_)) create_directory(funding_rate_path_);
}
BinanceFetcher::BinanceFetcher(string market_data_path)
    : market_data_path_(move(market_data_path)) {
  klines_path_ = market_data_path_ + "/Klines";
  funding_rate_path_ = market_data_path_ + "/Funding Rate";

  if (!exists(klines_path_)) create_directory(klines_path_);
  if (!exists(funding_rate_path_)) create_directory(funding_rate_path_);
}

shared_ptr<Logger>& BinanceFetcher::logger_ = Logger::GetLogger();

void BinanceFetcher::FetchBarData(const string& symbol,
                                  const string& timeframe) const {
  const string& timeframe_filename = GetFilenameWithTimeframe(timeframe);

  if (exists(klines_path_ + "/" + symbol + "/" + timeframe_filename +
             ".parquet")) {
    logger_->Log(LogLevel::WARNING_L,
                 format("[Spot-Futures {} {}] 파일이 이미 존재합니다.\n{}",
                        symbol, timeframe_filename, string(217, '-')),
                 __FILE__, __LINE__);
    return;
  }

  logger_->Log(LogLevel::INFO_L,
               format("[Spot-Futures {} {}] 파일 생성을 시작합니다.", symbol,
                      timeframe_filename),
               __FILE__, __LINE__);

  const unordered_map<string, string>& futures_params = {
      {"pair", symbol},
      {"contractType", "PERPETUAL"},
      {"interval", timeframe},
      {"startTime", "0"},
      {"limit", "1500"}};  // 1회 요청 최대량 1500

  // 선물 데이터 Fetch
  if (const auto& futures_klines =
          FetchKlines(futures_klines_url_, futures_params, true).get();
      !futures_klines.empty()) {
    // 저장 경로 생성
    const string& symbol_path = klines_path_ + "/" + symbol;
    if (!exists(symbol_path)) create_directory(symbol_path);

    // json deque의 첫 값을 가져오고 첫 번째 요소인 Open Time을 가져옴
    const auto& open_time = futures_klines.front().at(0);

    // Spot의 마지막 시간은 Future 첫 시간의 전 시간
    const string& end_time = to_string(static_cast<int64_t>(open_time) - 1);

    const unordered_map<string, string>& spot_params = {{"symbol", symbol},
                                                        {"interval", timeframe},
                                                        {"endTime", end_time},
                                                        {"limit", "1500"}};

    // 선물 데이터 변환
    const auto& transformed_futures_klines =
        TransformKlines(futures_klines, true);

    // 현물 데이터 Fetch
    if (const auto& spot_klines =
            FetchKlines(spot_klines_url_, spot_params, false).get();
        !spot_klines.empty()) {
      // 현물 데이터 변환
      const auto& transformed_spot_klines = TransformKlines(spot_klines, false);

      // 현물과 선물 데이터 병합
      const auto& spot_futures_klines =
          ConcatKlines(transformed_spot_klines, transformed_futures_klines);

      // 저장
      SaveKlines(spot_futures_klines,
                 symbol_path + "/" + timeframe_filename + ".parquet");

      logger_->Log(
          LogLevel::INFO_L,
          format("[{} - {}] 기간의 [Spot-Futures {} {}] 파일이 "
                 "생성되었습니다.\n{}",
                 UtcTimestampToUtcDatetime(spot_futures_klines.front().at(0)),
                 UtcTimestampToUtcDatetime(spot_futures_klines.back().at(6)),
                 symbol, timeframe_filename, string(217, '-')),
          __FILE__, __LINE__);
    } else {
      // 현물 데이터가 없는 종목이면 선물만 저장
      SaveKlines(transformed_futures_klines,
                 symbol_path + "/" + timeframe + ".parquet");

      logger_->Log(LogLevel::INFO_L,
                   format("[{} - {}] 기간의 [Futures {} {}] 파일이 "
                          "생성되었습니다.\n{}",
                          UtcTimestampToUtcDatetime(
                              transformed_futures_klines.front().at(0)),
                          UtcTimestampToUtcDatetime(
                              transformed_futures_klines.back().at(6)),
                          symbol, timeframe_filename, string(217, '-')),
                   __FILE__, __LINE__);
    }
  } else {
    Logger::LogAndThrowError(
        format("[Futures {} {}] Fetch가 실패했습니다.", symbol, timeframe),
        __FILE__, __LINE__);
  }
}

void BinanceFetcher::UpdateBarData(const string& symbol,
                                   const string& timeframe) const {
  const string& filename_timeframe = GetFilenameWithTimeframe(timeframe);
  const string& file_path =
      klines_path_ + "/" + symbol + "/" + filename_timeframe + ".parquet";

  if (!exists(file_path)) {
    logger_->Log(LogLevel::WARNING_L,
                 format("[Spot-Futures {} {}] 파일이 존재하지 않아 업데이트할 "
                        "수 없습니다.\n{}",
                        symbol, filename_timeframe, string(217, '-')),
                 __FILE__, __LINE__);
    return;
  }

  // Parquet 파일 읽기
  const auto& klines_file = ReadParquet(file_path);

  // 로그용 Open Time의 첫 값과 마지막 Close Time
  const auto begin_open_time =
      any_cast<int64_t>(GetCellValue(klines_file, "Open Time", 0));
  const auto end_close_time = any_cast<int64_t>(
      GetCellValue(klines_file, "Close Time", klines_file->num_rows() - 1));

  logger_->Log(
      LogLevel::INFO_L,
      format("[{} - {}] 기간의 [Spot-Futures {} {}] 파일이 존재합니다. 최신 "
             "선물 데이터 업데이트를 시작합니다.",
             UtcTimestampToUtcDatetime(begin_open_time),
             UtcTimestampToUtcDatetime(end_close_time), symbol,
             filename_timeframe),
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
      {"limit", "1500"}};

  const auto& futures_klines =
      FetchKlines(futures_klines_url_, futures_params, true).get();

  if (const auto& transformed_futures_klines =
          TransformKlines(futures_klines, true);
      !transformed_futures_klines.empty()) {
    // 업데이트된 데이터가 있다면 Array에 저장
    const auto& arrays = GetArraysAddedKlines(transformed_futures_klines);

    // 새로운 Array로 새로운 Table 생성
    const auto& schema = klines_file->schema();
    const auto& new_table = Table::Make(schema, arrays);

    // 기존 Table과 새로운 Table을 수직으로 결합
    const vector<shared_ptr<Table>>& tables_to_concatenate = {klines_file,
                                                              new_table};
    const auto& concatenated_tables_result =
        ConcatenateTables(tables_to_concatenate);

    // 저장
    TableToParquet(concatenated_tables_result.ValueOrDie(), file_path);

    logger_->Log(
        LogLevel::INFO_L,
        format(
            "[{} - {}] 기간의 [Spot-Futures {} {}] 파일이 "
            "업데이트되었습니다.\n{}",
            UtcTimestampToUtcDatetime(transformed_futures_klines.front().at(0)),
            UtcTimestampToUtcDatetime(transformed_futures_klines.back().at(6)),
            symbol, filename_timeframe, string(217, '-')),
        __FILE__, __LINE__);
  } else {
    logger_->Log(LogLevel::WARNING_L,
                 format("[Spot-Futures {} {}] 파일이 이미 최신 버전입니다.\n{}",
                        symbol, filename_timeframe, string(217, '-')),
                 __FILE__, __LINE__);
  }
}

future<vector<json>> BinanceFetcher::FetchKlines(
    const string& url, const unordered_map<string, string>& params,
    const bool forward) {
  logger_->Log(LogLevel::INFO_L, "Klines 데이터 Fetch를 시작합니다.", __FILE__,
               __LINE__);

  return async(launch::async, [=] {
    deque<json> klines;
    auto param = params;

    while (true) {
      try {
        // fetched_future를 미리 받아둠
        const shared_future<json>& fetched_future = Fetch(url, param);

        // Fetch 대기
        const json& fetched_klines = fetched_future.get();

        // fetch 해온 데이터가 비어있거나 잘못된 데이터면 종료
        if (fetched_klines.empty() || (fetched_klines.contains("code") &&
                                       fetched_klines["code"] == -1121)) {
          break;
        }

        logger_->Log(LogLevel::INFO_L,
                     format("[{} - {}] 기간의 Fetch가 완료되었습니다.",
                            UtcTimestampToUtcDatetime(fetched_klines[0][0]),
                            UtcTimestampToUtcDatetime(
                                fetched_klines[fetched_klines.size() - 1][0])),
                     __FILE__, __LINE__);

        if (forward) {
          // 앞부터 순회하여 뒤에 붙임
          for (auto& kline : fetched_klines) klines.push_back(kline);

          // 다음 startTime은 마지막 startTime의 뒤 시간
          param["startTime"] =
              to_string(static_cast<int64_t>(klines.back().at(0)) + 1);
        } else {
          // 뒤부터 순회하여 앞에 붙임
          for (auto kline = fetched_klines.rbegin();
               kline != fetched_klines.rend(); ++kline)
            klines.push_front(*kline);

          // 다음 entTime은 첫 startTime의 앞 시간
          param["endTime"] =
              to_string(static_cast<int64_t>(klines.front().at(0)) - 1);
        }
      } catch (const exception& e) {
        Logger::LogAndThrowError(
            "Fetch하는 중 에러가 발생했습니다.: " + string(e.what()), __FILE__,
            __LINE__);
      }
    }

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
  logger_->Log(LogLevel::INFO_L, "Klines 데이터 변환을 시작합니다.", __FILE__,
               __LINE__);

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
      // 값이 "1.23" 형식으로 들어오기 때문에 string을 double로 변환
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
        logger_->Log(LogLevel::WARNING_L, err, __FILE__, __LINE__);
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
  // column 추가
  const vector<string>& column_names(
      {"Open Time", "Open", "High", "Low", "Close", "Volume", "Close Time"});

  vector<shared_ptr<Field>> arrow_fields;
  for (const auto& column_name : column_names) {
    const auto& field_type =
        column_name == "Open Time" || column_name == "Close Time" ? int64()
                                                                  : float64();
    arrow_fields.push_back(field(column_name, field_type));
  }

  // 스키마 생성
  const auto& schema = arrow::schema(arrow_fields);

  // Klines를 Array에 추가
  const auto& arrays = GetArraysAddedKlines(klines);

  // Table 생성
  const auto& table = Table::Make(schema, arrays);

  // 저장
  TableToParquet(table, file_path);
}

vector<shared_ptr<Array>> BinanceFetcher::GetArraysAddedKlines(
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
  vector<shared_ptr<ArrayBuilder>> builders(7);
  builders[0] = make_shared<Int64Builder>();  // Open Time
  for (int i = 1; i <= 5; i++) {
    builders[i] = make_shared<DoubleBuilder>();  // OHLCV
  }
  builders[6] = make_shared<Int64Builder>();  // Close Time

  // 데이터를 Builder에 추가 후 Array에 추가
  vector<shared_ptr<Array>> arrays(7);
  const vector<vector<double>*>& price_data = {nullptr, &open, &high, &low, &close, &volume, nullptr};
  const vector<vector<int64_t>*>& time_data = {&open_time, nullptr, nullptr, nullptr, nullptr, nullptr, &close_time};

  for (int i = 0; i < 7; ++i) {
    if (i == 0 || i == 6) {  // 시간 데이터
      auto builder = make_shared<Int64Builder>();
      if (auto* int64_builder = builder.get();
        !int64_builder->AppendValues(*time_data[i]).ok() ||
          !int64_builder->Finish(&arrays[i]).ok()) {
        Logger::LogAndThrowError(
            format("{} time 데이터를 처리하는 데 실패했습니다.",
                  (i == 0 ? "Open" : "Close")),
            __FILE__, __LINE__);
      }
    } else {  // 가격 데이터
      auto builder = make_shared<DoubleBuilder>();
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
