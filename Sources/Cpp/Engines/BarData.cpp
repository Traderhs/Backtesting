// 표준 라이브러리
#include <format>

// 외부 헤더
#include <arrow\api.h>
#include <arrow\table.h>

// 파일 헤더
#include "Engines\BarData.hpp"

// 내부 헤더
#include "Engines\Logger.hpp"

BarData::BarData() : num_symbols_(0) {}
BarData::~BarData() = default;

void BarData::SetBarData(const string& symbol_name, const string& timeframe,
                         const shared_ptr<Table>& bar_data,
                         const vector<int>& columns) {
  IsValidSettings(symbol_name, timeframe, bar_data, columns);

  const size_t total_rows = bar_data->num_rows();
  const size_t symbol_idx = symbol_names_.size();  // 기존 마지막 인덱스 + 1

  // 한번에 메모리 할당 및 초기화
  open_time_.emplace_back();
  open_.emplace_back();
  high_.emplace_back();
  low_.emplace_back();
  close_.emplace_back();
  volume_.emplace_back();
  close_time_.emplace_back();

  // 바 정보 설정
  symbol_names_.push_back(symbol_name);
  num_symbols_++;
  num_bars_.push_back(total_rows);
  if (timeframe_.empty()) timeframe_ = timeframe;

  // 벡터들 미리 리사이즈
  open_time_[symbol_idx].reserve(total_rows);
  open_[symbol_idx].reserve(total_rows);
  high_[symbol_idx].reserve(total_rows);
  low_[symbol_idx].reserve(total_rows);
  close_[symbol_idx].reserve(total_rows);
  volume_[symbol_idx].reserve(total_rows);
  close_time_[symbol_idx].reserve(total_rows);

  // 컬럼 배열 미리 리사이즈
  vector<shared_ptr<ChunkedArray>> column_arrays;
  column_arrays.reserve(columns.size());
  for (const int column : columns) {
    column_arrays.push_back(bar_data->column(column));
  }

  // 청크별 처리
#pragma omp parallel
  {
    // 스레드별 임시 버퍼
    vector<int64_t> local_open_time;
    vector<double> local_open, local_high, local_low, local_close, local_volume;
    vector<int64_t> local_close_time;

#pragma omp for ordered nowait
    // 청크 순회
    for (int chunk_idx = 0; chunk_idx < column_arrays[0]->num_chunks();
         ++chunk_idx) {
      const int64_t chunk_length = column_arrays[0]->chunk(chunk_idx)->length();

      // 로컬 버퍼 리사이즈
      local_open_time.resize(chunk_length);
      local_open.resize(chunk_length);
      local_high.resize(chunk_length);
      local_low.resize(chunk_length);
      local_close.resize(chunk_length);
      local_volume.resize(chunk_length);
      local_close_time.resize(chunk_length);

      // 포인터로 직접 접근
      const auto open_time_arr =
          static_pointer_cast<Int64Array>(column_arrays[0]->chunk(chunk_idx));
      const auto open_arr =
          static_pointer_cast<DoubleArray>(column_arrays[1]->chunk(chunk_idx));
      const auto high_arr =
          static_pointer_cast<DoubleArray>(column_arrays[2]->chunk(chunk_idx));
      const auto low_arr =
          static_pointer_cast<DoubleArray>(column_arrays[3]->chunk(chunk_idx));
      const auto close_arr =
          static_pointer_cast<DoubleArray>(column_arrays[4]->chunk(chunk_idx));
      const auto volume_arr =
          static_pointer_cast<DoubleArray>(column_arrays[5]->chunk(chunk_idx));
      const auto close_time_arr =
          static_pointer_cast<Int64Array>(column_arrays[6]->chunk(chunk_idx));

      // memcpy로 한번에 복사
      memcpy(local_open_time.data(), open_time_arr->raw_values(),
             chunk_length * sizeof(int64_t));
      memcpy(local_open.data(), open_arr->raw_values(),
             chunk_length * sizeof(double));
      memcpy(local_high.data(), high_arr->raw_values(),
             chunk_length * sizeof(double));
      memcpy(local_low.data(), low_arr->raw_values(),
             chunk_length * sizeof(double));
      memcpy(local_close.data(), close_arr->raw_values(),
             chunk_length * sizeof(double));
      memcpy(local_volume.data(), volume_arr->raw_values(),
             chunk_length * sizeof(double));
      memcpy(local_close_time.data(), close_time_arr->raw_values(),
             chunk_length * sizeof(int64_t));

      // 순서대로 결과 합치기
#pragma omp ordered
      {
        open_time_[symbol_idx].insert(open_time_[symbol_idx].end(),
                                      local_open_time.begin(),
                                      local_open_time.end());
        open_[symbol_idx].insert(open_[symbol_idx].end(), local_open.begin(),
                                 local_open.end());
        high_[symbol_idx].insert(high_[symbol_idx].end(), local_high.begin(),
                                 local_high.end());
        low_[symbol_idx].insert(low_[symbol_idx].end(), local_low.begin(),
                                local_low.end());
        close_[symbol_idx].insert(close_[symbol_idx].end(), local_close.begin(),
                                  local_close.end());
        volume_[symbol_idx].insert(volume_[symbol_idx].end(),
                                   local_volume.begin(), local_volume.end());
        close_time_[symbol_idx].insert(close_time_[symbol_idx].end(),
                                       local_close_time.begin(),
                                       local_close_time.end());
      }
    }
  }
}

int64_t BarData::GetOpenTime(const int symbol_idx, const size_t bar_idx) const {
  IsValidIndex(symbol_idx, bar_idx);

  return open_time_[symbol_idx][bar_idx];
}

double BarData::GetOpen(const int symbol_idx, const size_t bar_idx) const {
  IsValidIndex(symbol_idx, bar_idx);

  return open_[symbol_idx][bar_idx];
}

double BarData::GetHigh(const int symbol_idx, const size_t bar_idx) const {
  IsValidIndex(symbol_idx, bar_idx);

  return high_[symbol_idx][bar_idx];
}

double BarData::GetLow(const int symbol_idx, const size_t bar_idx) const {
  IsValidIndex(symbol_idx, bar_idx);

  return low_[symbol_idx][bar_idx];
}

double BarData::GetClose(const int symbol_idx, const size_t bar_idx) const {
  IsValidIndex(symbol_idx, bar_idx);

  return close_[symbol_idx][bar_idx];
}

double BarData::GetVolume(const int symbol_idx, const size_t bar_idx) const {
  IsValidIndex(symbol_idx, bar_idx);

  return volume_[symbol_idx][bar_idx];
}

int64_t BarData::GetCloseTime(const int symbol_idx,
                              const size_t bar_idx) const {
  IsValidIndex(symbol_idx, bar_idx);

  return close_time_[symbol_idx][bar_idx];
}

string BarData::GetSymbolName(const int symbol_idx) const {
  IsValidSymbolIndex(symbol_idx);

  return symbol_names_[symbol_idx];
}

int BarData::GetNumSymbols() const {
  return num_symbols_;
}

size_t BarData::GetNumBars(const int symbol_idx) const {
  IsValidSymbolIndex(symbol_idx);

  return num_bars_[symbol_idx];
}

string BarData::GetTimeframe() const {
  return timeframe_;
}

void BarData::IsValidSettings(const string& symbol_name,
                              const string& timeframe,
                              const shared_ptr<Table>& bar_data,
                              const vector<int>& columns) const {
  // Column 인덱스 오류 체크
  if (columns.size() != 7 || ranges::any_of(columns, [&](const int column) {
        return column < 0 || column >= bar_data->num_columns();
      })) {
    Logger::LogAndThrowError("열 인덱스가 잘못 지정되었습니다.", __FILE__,
                             __LINE__);
  }

  for (const auto& symbol : symbol_names_) {
    if (symbol == symbol_name) {
      Logger::LogAndThrowError(symbol_name + "은(는) 이미 추가된 심볼입니다.",
                               __FILE__, __LINE__);
      return;
    }
  }

  if (!timeframe_.empty() && timeframe != timeframe_) {
    Logger::LogAndThrowError(format("주어진 타임프레임 {}은(는) 바 데이터로 "
                                    "추가된 타임프레임 {}와 일치하지 않습니다.",
                                    timeframe, timeframe_),
                             __FILE__, __LINE__);
  }
}

void BarData::IsValidIndex(const int symbol_idx, const size_t bar_idx) const {
  IsValidSymbolIndex(symbol_idx);

  if (const auto num_bar = open_time_[symbol_idx].size(); bar_idx >= num_bar) {
    Logger::LogAndThrowError(format("지정된 바 인덱스 {}은(는) 0 미만이거나 "
                                    "최대값 {}을(를) 초과했습니다.",
                                    symbol_idx, num_bar - 1),
                             __FILE__, __LINE__);
  }
}

void BarData::IsValidSymbolIndex(int symbol_idx) const {
  if (symbol_idx < 0 || symbol_idx >= num_symbols_) {
    Logger::LogAndThrowError(format("지정된 심볼 인덱스 {}은(는) 0 미만이거나 "
                                    "최대값 {}을(를) 초과했습니다.",
                                    symbol_idx, num_symbols_ - 1),
                             __FILE__, __LINE__);
  }
}
