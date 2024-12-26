// 표준 라이브러리
#include <format>
#include <set>
#include <variant>

// 외부 라이브러리
#include <arrow/api.h>
#include <arrow/table.h>

#include <nlohmann/json.hpp>

// 내부 헤더
#include "Engines/DataUtils.hpp"
#include "Engines/TimeUtils.hpp"

// 파일 헤더
#include "Engines/BarDataManager.hpp"

// 네임 스페이스
using namespace arrow;
using namespace DataUtils;
using namespace TimeUtils;

BarDataManager::BarDataManager()
    : current_bar_data_type(BarDataType::TRADING) {}
BarDataManager::~BarDataManager() = default;

void BarDataManager::AddTradingBarData(const string& name,
                                       const string& file_path,
                                       const vector<int>& columns,
                                       const double split_ratio) {
  // Parquet 파일 읽기
  const auto& bar_data = ReadParquet(file_path);

  // 데이터 유효성 검증
  const auto& [engine_trading_testing_tf, bar_data_tf] =
      IsValidTradingBarData(name, bar_data, columns[0]);

  // split_ratio에 따라 데이터 분할
  const auto& [trading_bar_data_table, test_trading_bar_data_table] =
      SplitTable(bar_data, split_ratio);

  // Table을 Vector로 변환
  const auto& trading_bar_data =
      GetVectorAddedBarData(trading_bar_data_table, columns);
  const auto& test_trading_bar_data =
      GetVectorAddedBarData(test_trading_bar_data_table, columns);

  // 데이터 추가
  this->trading_bar_data.emplace(name, trading_bar_data);
  this->test_trading_bar_data.emplace(name, test_trading_bar_data);

  // 타임프레임 설정
  if (engine_trading_testing_tf.empty())
    this->SetTimeframe(BarDataType::TRADING, bar_data_tf);

  logger.Log(
      Logger::INFO_L,
      format("[{} - {}] 기간의 {} {}이(가) 트레이딩 바 데이터로 엔진에 "
             "추가되었습니다.",
             UTCTimestampToUtcDatetime(trading_bar_data.begin()->open_time),
             UTCTimestampToUtcDatetime(prev(trading_bar_data.end())->open_time),
             name, bar_data_tf),
      __FILE__, __LINE__);
}

void BarDataManager::AddMagnifierBarData(const string& name,
                                         const string& file_path,
                                         const vector<int>& columns) {
  // Parquet 파일 읽기
  const auto& bar_data = ReadParquet(file_path);

  // 데이터 유효성 검증
  const auto& [engine_magnifier_tf, bar_data_tf] =
      IsValidMagnifierBarData(name, bar_data, columns[0]);

  // Table을 Vector로 변환
  const auto& magnifier_bar_data = GetVectorAddedBarData(bar_data, columns);

  // 데이터 추가
  this->magnifier_bar_data.emplace(name, magnifier_bar_data);

  // 타임프레임 설정
  if (engine_magnifier_tf.empty())
    this->SetTimeframe(BarDataType::MAGNIFIER, bar_data_tf);

  logger.Log(
      Logger::INFO_L,
      format(
          "[{} - {}] 기간의 {} {}이(가) 돋보기 바 데이터로 엔진에 "
          "추가되었습니다.",
          UTCTimestampToUtcDatetime(magnifier_bar_data.begin()->open_time),
          UTCTimestampToUtcDatetime(prev(magnifier_bar_data.end())->open_time),
          name, bar_data_tf),
      __FILE__, __LINE__);
}

BarDataManager& BarDataManager::GetBarDataManager() {
  if (!instance) {
    lock_guard lock(mutex);
    instance.reset(new BarDataManager());
  }
  return *instance;
}

unordered_map<string, vector<BarDataManager::bar_data>>&
BarDataManager::GetTradingBarData() {
  return trading_bar_data;
}

unordered_map<string, vector<BarDataManager::bar_data>>&
BarDataManager::GetMagnifierBarData() {
  return magnifier_bar_data;
}

unordered_map<string, unordered_map<string, vector<BarDataManager::bar_data>>>&
BarDataManager::GetSubBarData() {
  return sub_bar_data;
}

string& BarDataManager::GetTradingTimeframe() { return trading_timeframe; }

string& BarDataManager::GetMagnifierTimeframe() { return trading_timeframe; }

set<string>& BarDataManager::GetSubTimeframe() { return sub_timeframe; }

size_t BarDataManager::GetCurrentIndex(const string& symbol,
                                       const string& timeframe) {
  switch (current_bar_data_type) {
    case BarDataType::TRADING:
      return trading_index[symbol];

    case BarDataType::MAGNIFIER:
      return magnifier_index[symbol];

    case BarDataType::SUB:
      const auto& symbol_it = sub_index.find(symbol);
      if (symbol_it == sub_index.end()) {
        Logger::LogAndThrowError(
            format("심볼 {}을(를) 찾을 수 없습니다.", symbol), __FILE__,
            __LINE__);
      }

      const auto& timeframe_it = symbol_it->second.find(timeframe);
      if (timeframe_it == symbol_it->second.end()) {
        Logger::LogAndThrowError(
            format("타임프레임 {}을(를) 찾을 수 없습니다.", timeframe),
            __FILE__, __LINE__);
      }

      return timeframe_it->second;

    case default:
      Logger::LogAndThrowError(
          format("심볼 {} 또는 타임프레임 {}을 찾을 수 없습니다.", symbol,
                 timeframe),
          __FILE__, __LINE__);
      return {};
  }
}

BarDataManager::bar_data BarDataManager::GetBar(const string& timeframe,
                                                const size_t index) {
  vector<bar_data> current_bar_data;
  size_t current_index;

  // 현재 바 데이터 타입의 해당 심볼과 타임프레임에 해당되는
  // 바 데이터와 인덱스 찾기
  switch (current_bar_data_type) {
    case BarDataType::TRADING:
      current_bar_data = move(trading_bar_data.find(current_symbol)->second);
      current_index = trading_index.find(current_symbol)->second;
      break;

    case BarDataType::MAGNIFIER:
      current_bar_data = move(magnifier_bar_data.find(current_symbol)->second);
      current_index = magnifier_index.find(current_symbol)->second;
      break;

    case BarDataType::SUB:
      // 해당 심볼과 타임프레임에 해당되는 서브 바 데이터 찾기
      auto& sub_bar_data_it = sub_bar_data.find(current_symbol)->second;
      const auto sub_bar_data_timeframe_it = sub_bar_data_it.find(timeframe);

      // 찾지 못하면 기본 bar_data 반환
      if (sub_bar_data_timeframe_it == sub_bar_data_it.end()) bar_data{};

      // 해당 심볼과 타임프레임에 해당되는 서브 인덱스 찾기
      auto& sub_index_it = sub_index.find(current_symbol)->second;
      const auto sub_index_timeframe_it = sub_index_it.find(timeframe);

      // 찾지 못하면 기본 bar_data 반환
      if (sub_index_timeframe_it == sub_index_it.end()) bar_data{};

      auto&& sub_bar_data = move(sub_bar_data_timeframe_it->second);
      auto&& sub_index = sub_index_timeframe_it->second;

      current_bar_data = move(sub_bar_data);
      current_index = sub_index;
      break;

    default:
      return bar_data{};
  }

  // 인덱스 범위 체크
  if (current_index < index ||
      (current_index - index) >= current_bar_data.size()) {
    return bar_data{};
  }

  return current_bar_data[current_index - index];
}

void BarDataManager::SetTimeframe(const BarDataType bar_data_type,
                                  const string& timeframe) {
  switch (bar_data_type) {
    case BarDataType::TRADING:
      trading_timeframe = timeframe;
      return;

    case BarDataType::MAGNIFIER:
      magnifier_timeframe = timeframe;
      return;

    case BarDataType::SUB:
      sub_timeframe.insert(timeframe);
  }
}

void BarDataManager::SetCurrentIndex(const string& symbol,
                                     const string& timeframe,
                                     const size_t index) {
  switch (current_bar_data_type) {
    case BarDataType::TRADING:
      trading_index[symbol] = index;
      return;

    case BarDataType::MAGNIFIER:
      magnifier_index[symbol] = index;
      return;

    case BarDataType::SUB:
      sub_index[symbol][timeframe] = index;
  }
}

vector<BarDataManager::bar_data> BarDataManager::GetVectorAddedBarData(
    const shared_ptr<Table>& bar_data, const vector<int>& columns) {
  // Column 인덱스 오류 체크
  if (columns.size() != 7 || ranges::any_of(columns, [&](const int column) {
        return column < 0 || column >= bar_data->num_columns();
      })) {
    Logger::LogAndThrowError("열 인덱스가 잘못 지정되었습니다.", __FILE__,
                             __LINE__);
  }

  // ===========================================================================
  vector<BarDataManager::bar_data> bar_data_vector(bar_data->num_rows());
  vector<shared_ptr<ChunkedArray>> column_arrays;

  for (const int column : columns) {
    // bar_data의 지정한 column의 Open Time부터 column_arrays에 push
    column_arrays.push_back(
        dynamic_pointer_cast<ChunkedArray>(bar_data->column(column)));
  }

// 병렬 처리
#pragma omp parallel for schedule(static)
  for (int chunk_idx = 0; chunk_idx < column_arrays[0]->num_chunks();
       ++chunk_idx) {
    // 해당 Chunk의 Local Array push
    vector<const Array*> local_arrays;
    for (const auto& arr : column_arrays)
      local_arrays.push_back(arr->chunk(chunk_idx).get());

    // 해당 청크까지 오기까지의 행 수의 합
    int64_t global_row_start = 0;
    for (int idx = 0; idx < chunk_idx; ++idx)
      global_row_start += column_arrays[0]->chunk(idx)->length();

    // 해당 청크의 행 순회
    for (int64_t local_row = 0; local_row < local_arrays[0]->length();
         ++local_row) {
      const int64_t global_row = global_row_start + local_row;

      // 벡터에 해당 행을 구조체로 변환하여 저장
      bar_data_vector[global_row] = {
          dynamic_pointer_cast<Int64Scalar>(
              local_arrays[0]->GetScalar(local_row).ValueOrDie())
              ->value,  // Open Time
          dynamic_pointer_cast<DoubleScalar>(
              local_arrays[1]->GetScalar(local_row).ValueOrDie())
              ->value,  // Open
          dynamic_pointer_cast<DoubleScalar>(
              local_arrays[2]->GetScalar(local_row).ValueOrDie())
              ->value,  // High
          dynamic_pointer_cast<DoubleScalar>(
              local_arrays[3]->GetScalar(local_row).ValueOrDie())
              ->value,  // Low
          dynamic_pointer_cast<DoubleScalar>(
              local_arrays[4]->GetScalar(local_row).ValueOrDie())
              ->value,  // Close
          dynamic_pointer_cast<DoubleScalar>(
              local_arrays[5]->GetScalar(local_row).ValueOrDie())
              ->value,  // Volume
          dynamic_pointer_cast<Int64Scalar>(
              local_arrays[6]->GetScalar(local_row).ValueOrDie())
              ->value};  // Close Time
    }
  }

  return bar_data_vector;
}

DataManager& BarDataManager::data = DataManager::GetDataManager();
Logger& BarDataManager::logger = Logger::GetLogger();

pair<string, string> BarDataManager::IsValidTradingBarData(
    const string& name, const shared_ptr<Table>& bar_data,
    const int open_time_column) {
  // name이 중복된다면 오류 발생
  if (trading_bar_data.contains(name)) {
    Logger::LogAndThrowError(
        name + "은(는) 트레이딩 바 데이터로 이미 추가된 이름입니다.", __FILE__,
        __LINE__);
  }

  // ===========================================================================
  const string& bar_data_tf = CalculateTimeframe(bar_data, open_time_column);
  const string& engine_trading_tf = GetTradingTimeframe();

  // 트레이딩 바 데이터 심볼간 타임프레임이 다르면 오류 발생
  if (!engine_trading_tf.empty() && engine_trading_tf != bar_data_tf) {
    Logger::LogAndThrowError(
        format("트레이딩 바 데이터의 심볼간 타임프레임은 통일해야 합니다. | "
               "다른 심볼의 타임프레임: {} | {}의 타임프레임: {}",
               engine_trading_tf, name, bar_data_tf),
        __FILE__, __LINE__);
  }

  // ===========================================================================
  const string& engine_magnifier_tf = GetMagnifierTimeframe();

  /* 트레이딩 바 데이터의 타임프레임이 돋보기 바 데이터의 타임프레임보다
   * 작으면 오류 발생: 돋보기 바 데이터의 타임프레임이 더 작아야 함
   */
  if (const int64_t parsed_bar_data_tf = ParseTimeframe(bar_data_tf);
      !engine_magnifier_tf.empty() &&
      ParseTimeframe(engine_magnifier_tf) > parsed_bar_data_tf) {
    Logger::LogAndThrowError(
        format(
            "돋보기 바 데이터의 타임프레임은 트레이딩 바 데이터의 "
            "타임프레임보다 작아야합니다. | 돋보기 바 데이터의 타임프레임: {}"
            "트레이딩 바 데이터의 타임프레임: {} |",
            engine_magnifier_tf, bar_data_tf),
        __FILE__, __LINE__);
  }

  return {engine_trading_tf, bar_data_tf};
}

pair<string, string> BarDataManager::IsValidMagnifierBarData(
    const string& name, const shared_ptr<Table>& bar_data,
    const int open_time_column) {
  // name이 중복된다면 오류 발생
  if (magnifier_bar_data.contains(name)) {
    Logger::LogAndThrowError(
        name + "은(는) 돋보기 바 데이터로 이미 추가된 이름입니다.", __FILE__,
        __LINE__);
  }

  // ===========================================================================
  const string& bar_data_tf = CalculateTimeframe(bar_data, open_time_column);
  const string& engine_magnifier_tf = GetMagnifierTimeframe();

  // 돋보기 바 데이터 심볼간 타임프레임이 다르면 오류 발생
  if (!engine_magnifier_tf.empty() && engine_magnifier_tf != bar_data_tf) {
    Logger::LogAndThrowError(
        format("돋보기 바 데이터의 심볼간 타임프레임은 통일해야 합니다. | "
               "다른 심볼의 타임프레임: {} | {}의 타임프레임: {}",
               engine_magnifier_tf, name, bar_data_tf),
        __FILE__, __LINE__);
  }

  // ===========================================================================
  const string& engine_trading_tf = GetTradingTimeframe();

  /* 돋보기 바 데이터의 타임프레임이 트레이딩 바 데이터의
   * 타임프레임보다 크거나 같으면 오류 발생:
   * 돋보기 바 데이터의 타임프레임이 더 작아야 함
   */
  if (const int64_t parsed_bar_data_tf = ParseTimeframe(bar_data_tf);
      !engine_trading_tf.empty() &&
      ParseTimeframe(engine_trading_tf) <= parsed_bar_data_tf) {
    Logger::LogAndThrowError(
        format("돋보기 바 데이터의 타임프레임은 트레이딩 바 데이터의 "
               "타임프레임보다 작아야합니다. | 트레이딩 바 데이터의 "
               "타임프레임: {} | 돋보기 바 데이터의 타임프레임: {}",
               engine_trading_tf, bar_data_tf),
        __FILE__, __LINE__);
  }

  return {engine_magnifier_tf, bar_data_tf};
}

string BarDataManager::CalculateTimeframe(
    const shared_ptr<Table>& bar_data, const int open_time_column) {
  const int64_t fst_open_time =
      any_cast<int64_t>(GetCellValue(bar_data, open_time_column, 0));
  const int64_t snd_open_time =
      any_cast<int64_t>(GetCellValue(bar_data, open_time_column, 1));

  // 두 번째 Open Time과 첫 번째 Open Time의 차이
  return FormatTimeframe(snd_open_time - fst_open_time);
}

mutex BarDataManager::mutex;
unique_ptr<BarDataManager> BarDataManager::instance;
