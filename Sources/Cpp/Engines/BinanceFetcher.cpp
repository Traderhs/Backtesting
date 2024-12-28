// 표준 라이브러리
#include <format>
#include <future>
#include <string>
#include <utility>

// 외부 라이브러리
#include <arrow/api.h>
#include <arrow/io/file.h>

// 내부 헤더
#include "Engines/DataUtils.hpp"
#include "Engines/Logger.hpp"
#include "Engines/TimeUtils.hpp"

// 파일 헤더
#include "Engines/BinanceFetcher.hpp"

// 네임 스페이스
using namespace DataUtils;
using namespace filesystem;
using namespace TimeUtils;

BinanceFetcher::BinanceFetcher() : klines_path("../../Klines") {}
BinanceFetcher::BinanceFetcher(string klines_path) : klines_path(move(klines_path)) {}

void BinanceFetcher::FetchAndSaveBarData(const string& symbol,
                                  const string& timeframe) const {
  const string& timeframe_filename = GetFilenameWithTimeframe(timeframe);

  if (exists(klines_path + "/" + symbol + "/" + timeframe_filename +
             ".parquet")) {
    logger.Log(Logger::WARNING_L,
               format("[Spot-Futures {} {}] 파일이 이미 존재합니다.\n{}",
                      symbol, timeframe_filename, string(200, '-')),
               __FILE__, __LINE__);
    return;
  }

  logger.Log(Logger::INFO_L,
             format("[Spot-Futures {} {}] 파일 생성을 시작합니다.", symbol,
                    timeframe_filename),
             __FILE__, __LINE__);

  const unordered_map<string, string>& f_params = {
      {"pair", symbol},
      {"contractType", "PERPETUAL"},
      {"interval", timeframe},
      {"startTime", "0"},
      {"limit", "1500"}};

  // 선물 데이터 Fetch
  if (const auto& f_klines =
          FetchKlines(futures_klines_url, f_params, true).get();
      !f_klines.empty()) {
    // 저장 경로 생성
    const string& symbol_path = klines_path + "/" + symbol;
    if (!exists(symbol_path)) create_directory(symbol_path);

    // json deque의 첫 값을 가져오고 첫 번째 요소인 Open Time을 가져옴
    const auto& open_time = f_klines.front().at(0);

    // Spot의 마지막 시간은 Future 첫 시간의 전 시간
    const string& end_time = to_string(static_cast<int64_t>(open_time) - 1);

    const unordered_map<string, string>& s_params = {{"symbol", symbol},
                                                     {"interval", timeframe},
                                                     {"endTime", end_time},
                                                     {"limit", "1500"}};

    // 선물 데이터 변환
    const auto& transformed_f_klines = TransformKlines(f_klines, true);

    // 현물 데이터 Fetch
    if (const auto& s_klines =
            FetchKlines(spot_klines_url, s_params, false).get();
        !s_klines.empty()) {
      // 현물 데이터 변환
      const auto& transformed_s_klines = TransformKlines(s_klines, false);

      // 현물과 선물 데이터 병합
      const auto& spot_futures_klines =
          ConcatKlines(transformed_s_klines, transformed_f_klines);

      // 저장
      SaveKlines(spot_futures_klines,
                 symbol_path + "/" + timeframe_filename + ".parquet");

      logger.Log(
          Logger::INFO_L,
          format("[{} - {}] 기간의 [Spot-Futures {} {}] 파일이 "
                 "생성되었습니다.\n{}",
                 UTCTimestampToUtcDatetime(spot_futures_klines.front().at(0)),
                 UTCTimestampToUtcDatetime(spot_futures_klines.back().at(0)),
                 symbol, timeframe_filename, string(200, '-')),
          __FILE__, __LINE__);
    } else {
      // 현물 데이터가 없는 종목이면 선물만 저장
      SaveKlines(transformed_f_klines,
                 symbol_path + "/" + timeframe + ".parquet");

      logger.Log(
          Logger::INFO_L,
          format("[{} - {}] 기간의 [Futures {} {}] 파일이 "
                 "생성되었습니다.\n{}",
                 UTCTimestampToUtcDatetime(transformed_f_klines.front().at(0)),
                 UTCTimestampToUtcDatetime(transformed_f_klines.back().at(0)),
                 symbol, timeframe_filename, string(200, '-')),
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
      klines_path + "/" + symbol + "/" + filename_timeframe + ".parquet";

  if (!exists(file_path)) {
    logger.Log(Logger::WARNING_L,
               format("[Spot-Futures {} {}] 파일이 존재하지 않습니다.\n{}",
                      symbol, filename_timeframe, string(200, '-')),
               __FILE__, __LINE__);
    return;
  }

  // Parquet 파일 읽기
  const auto& klines_file = ReadParquet(file_path);

  // Open Time의 첫 값과 마지막 값을 가져옴
  const auto begin_open_time =
      any_cast<int64_t>(GetCellValue(klines_file, "Open Time", 0));
  const auto end_open_time = any_cast<int64_t>(
      GetCellValue(klines_file, "Open Time", klines_file->num_rows() - 1));

  logger.Log(
      Logger::INFO_L,
      format("[{} - {}] 기간의 [Spot-Futures {} {}] 파일이 존재합니다. 최신 "
             "선물 데이터 업데이트를 시작합니다.",
             UTCTimestampToUtcDatetime(begin_open_time),
             UTCTimestampToUtcDatetime(end_open_time), symbol,
             filename_timeframe),
      __FILE__, __LINE__);

  // 새로운 Open Time의 시작은 Klines file의 마지막 Open Time의 다음 값
  const string& start_time = to_string(end_open_time + 1);

  const unordered_map<string, string>& f_params = {
      {"pair", symbol},
      {"contractType", "PERPETUAL"},
      {"interval", timeframe},
      {"startTime", start_time},
      {"limit", "1500"}};

  const auto& f_klines = FetchKlines(futures_klines_url, f_params, true).get();

  if (const auto& transformed_f_klines = TransformKlines(f_klines, true);
      !transformed_f_klines.empty()) {
    // 업데이트된 데이터가 있다면 Array에 저장
    const auto& arrays = GetArraysAddedKlines(transformed_f_klines);

    // 새로운 Array로 새로운 Table 생성
    const auto& schema = klines_file->schema();
    const auto& new_table = Table::Make(schema, arrays);

    // 기존 Table과 새로운 Table을 수직으로 결합
    const vector<shared_ptr<Table>>& tables_to_concatenate = {klines_file,
                                                              new_table};
    const auto& concatenated_table_result =
        ConcatenateTables(tables_to_concatenate);

    // 저장
    TableToParquet(concatenated_table_result.ValueOrDie(), file_path);

    logger.Log(
        Logger::INFO_L,
        format("[{} - {}] 기간의 [Spot-Futures {} {}] 파일이 "
               "업데이트되었습니다.\n{}",
               UTCTimestampToUtcDatetime(transformed_f_klines.front().at(0)),
               UTCTimestampToUtcDatetime(transformed_f_klines.back().at(0)),
               symbol, filename_timeframe, string(200, '-')),
        __FILE__, __LINE__);
  } else {
    logger.Log(Logger::WARNING_L,
               format("[Spot-Futures {} {}] 파일이 이미 최신 버전입니다.\n{}",
                      symbol, filename_timeframe, string(200, '-')),
               __FILE__, __LINE__);
  }
}

future<deque<json>> BinanceFetcher::FetchKlines(
    const string& url, const unordered_map<string, string>& params,
    const bool forward) {
  return async(launch::async, [=] {
    deque<json> klines;
    unordered_map<string, string> param = params;

    while (true) {
      try {
        // fetched_future를 미리 받아둠
        shared_future fetched_future = Fetch(url, param);

        // Fetch 대기
        const json& fetched = fetched_future.get();

        // fetch 해온 데이터가 비어있거나 잘못된 데이터면 종료
        if (fetched.empty() ||
            (fetched.contains("code") && fetched["code"] == -1121)) {
          break;
        }

        if (forward) {
          // 앞부터 순회하여 뒤에 붙임
          for (auto& data : fetched) klines.push_back(data);

          // 다음 startTime은 마지막 startTime의 뒤 시간
          param["startTime"] =
              to_string(static_cast<int64_t>(klines.back().at(0)) + 1);
        } else {
          // 뒤부터 순회하여 앞에 붙임
          for (auto data = fetched.rbegin(); data != fetched.rend(); ++data)
            klines.push_front(*data);

          // 다음 entTime은 첫 startTime의 앞 시간
          param["endTime"] =
              to_string(static_cast<int64_t>(klines.front().at(0)) - 1);
        }
      } catch (const exception& e) {
        logger.Log(Logger::ERROR_L,
                   "Fetch하는 중 에러가 발생했습니다.: " + string(e.what()),
                   __FILE__, __LINE__);
        return klines;
      }
    }

    return klines;
  });
}

Logger& BinanceFetcher::logger = Logger::GetLogger();

string BinanceFetcher::GetFilenameWithTimeframe(const string& timeframe) {
  // 윈도우는 1m과 1M이 같은 것으로 취급하므로 명시적 이름 변환이 필요함
  if (timeframe == "1M") {
    return "1month";
  }

  return timeframe;
}

deque<json> BinanceFetcher::TransformKlines(const deque<json>& klines,
                                     const bool drop_latest) {
  deque<json> transformed_klines;
  for (const auto& kline : klines) {
    json data;

    try {
      // Open Time 추가
      data.push_back(static_cast<int64_t>(kline[0]));

      // OHLCV 추가
      // 값이 "1.23" 형식으로 들어오기 때문에 string을 double로 변환
      for (int i = 1; i <= 5; i++) {
        data.push_back(stod(kline[i].get<string>()));
      }

      // Close Time 추가
      data.push_back(static_cast<int64_t>(kline[6]));

    } catch (const exception& e) {
      const string& err =
          format("데이터 변환 중 에러가 발생했습니다: {}", e.what());
      logger.Log(Logger::WARNING_L, err, __FILE__, __LINE__);
      continue;
    }

    transformed_klines.push_back(data);
  }

  // true일 시, 최신 데이터의 마지막 행은 데이터가 계속 바뀌므로
  // 바가 완성되기 전까지 저장하지 않음
  if (drop_latest) transformed_klines.pop_back();

  return transformed_klines;
}

deque<json> BinanceFetcher::ConcatKlines(const deque<json>& spot_klines,
                                  const deque<json>& futures_klines) {
  // Futures 첫 데이터 시가 / Spot 마지막 데이터 종가
  const double ratio = static_cast<double>(futures_klines.front()[1]) /
                       static_cast<double>(spot_klines.back()[4]);

  deque<json> adjusted_spot_klines;
  double adjusted_price = 0.0;
  size_t decimal_places = 0;

  for (auto& kline : spot_klines) {
    json data;

    data.push_back(kline[0]);  // Open Time 추가

    // 해당 행 원본 OHLC의 최대 소숫점 자릿수 Count
    decimal_places =
        max({CountDecimalPlaces(kline[1]), CountDecimalPlaces(kline[2]),
             CountDecimalPlaces(kline[3]), CountDecimalPlaces(kline[4])});

    // OHLC 추가
    for (int i = 1; i <= 4; i++) {
      // ratio 곱으로 인해 생긴 소수점 반올림
      // Futures의 최대 소수점 자릿수 사용
      adjusted_price = RoundToDecimalPlaces(
          kline[i].get<double>() * ratio,  // OHLC를 조정 비율로 계산
          decimal_places);

      data.push_back(adjusted_price);
    }

    // 거래량, Close Time 추가
    for (int i = 5; i <= 6; i++) data.push_back(kline[i]);

    adjusted_spot_klines.push_back(data);
  }

  // 조정된 Spot 데이터에 futures 데이터 추가
  for (auto& kline : futures_klines) adjusted_spot_klines.push_back(kline);

  return adjusted_spot_klines;
}

void BinanceFetcher::SaveKlines(const deque<json>& klines, const string& file_path) {
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
    const deque<json>& klines) {
  // 각 컬럼의 데이터를 저장할 빌더 벡터 초기화
  vector<shared_ptr<ArrayBuilder>> builders(7);
  builders[0] = make_shared<Int64Builder>();  // Open Time
  builders[6] = make_shared<Int64Builder>();  // Close Time

  for (int i = 1; i <= 5; i++) {
    builders[i] = make_shared<DoubleBuilder>();  // OHLCV
  }

  // Array Builder에 Klines 추가
  // 전체 행 순회
  for (const auto& json : klines) {
    // 전체 열 순회
    for (int i = 0; i < 7; ++i) {
      if (i == 0 || i == 6) {  // 날짜, 시간: int64 타입 추가
        if (auto* int64_builder = dynamic_cast<Int64Builder*>(builders[i].get());
          !int64_builder->Append(json[i].get<int64_t>()).ok()) {
          Logger::LogAndThrowError(format(
              "{} 데이터를 추가하는 데 실패했습니다.", json[i].get<int64_t>()),__FILE__,__LINE__);
        }
      } else {  // OHLCV: double 타입 추가
        if (auto* double_builder = dynamic_cast<DoubleBuilder*>(builders[i].get());
          !double_builder->Append(json[i].get<double>()).ok()) {
          Logger::LogAndThrowError(format(
              "{} 데이터를 추가하는 데 실패했습니다.", json[i].get<double>()),__FILE__,__LINE__);
        }
      }
    }
  }

  // Array 생성
  vector<shared_ptr<Array>> arrays(7);

  // Array에 Array Builder 추가
  for (int i = 0; i < 7; ++i) {
    if (i == 0 || i == 6) {
      if (auto* int64_builder = dynamic_cast<Int64Builder*>(builders[i].get());
        !int64_builder->Finish(&arrays[i]).ok()) {
        Logger::LogAndThrowError("시간 데이터를 저장 완료하는 데 실패했습니다.",__FILE__,__LINE__);
      }
    } else {
      if (auto* double_builder = dynamic_cast<DoubleBuilder*>(builders[i].get());
          !double_builder->Finish(&arrays[i]).ok())
        Logger::LogAndThrowError("실수 데이터를 저장 완료하는 데 실패했습니다.",__FILE__,__LINE__);
    }
  }

  return arrays;
}
