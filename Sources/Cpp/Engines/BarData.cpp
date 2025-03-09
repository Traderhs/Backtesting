// 표준 라이브러리
#include <format>

// 외부 라이브러리
#include "arrow/array/array_primitive.h"
#include "arrow/table.h"

// 파일 헤더
#include "Engines/BarData.hpp"

// 내부 헤더
#include "Engines/Exception.hpp"
#include "Engines/Logger.hpp"

// 네임 스페이스
namespace backtesting {
using namespace exception;
using namespace logger;
}  // namespace backtesting

namespace backtesting::bar {

BarData::BarData() : num_symbols_(0) {}
BarData::~BarData() = default;

void BarData::SetBarData(const string& symbol_name, const string& timeframe,
                         const shared_ptr<arrow::Table>& bar_data,
                         const int open_time_column, const int open_column,
                         const int high_column, const int low_column,
                         const int close_column, const int volume_column,
                         const int close_time_column) {
  // 유효성 검사
  IsValidSettings(symbol_name, timeframe, bar_data, open_time_column,
                  open_column, high_column, low_column, close_column,
                  volume_column, close_time_column);

  const auto total_rows = bar_data->num_rows();
  const auto symbol_idx = symbol_names_.size();  // 기존 마지막 인덱스 + 1

  // 한번에 메모리 할당
  bar_data_.emplace_back();
  bar_data_[symbol_idx].resize(total_rows);

  // 바 정보 설정
  symbol_names_.push_back(symbol_name);
  num_symbols_++;
  num_bars_.push_back(total_rows);
  if (timeframe_.empty()) timeframe_ = timeframe;

  // 컬럼 데이터를 미리 캐스팅하여 저장
  const auto& open_time_array = static_pointer_cast<arrow::Int64Array>(
      bar_data->column(open_time_column)->chunk(0));
  const auto& open_array = static_pointer_cast<arrow::DoubleArray>(
      bar_data->column(open_column)->chunk(0));
  const auto& high_array = static_pointer_cast<arrow::DoubleArray>(
      bar_data->column(high_column)->chunk(0));
  const auto& low_array = static_pointer_cast<arrow::DoubleArray>(
      bar_data->column(low_column)->chunk(0));
  const auto& close_array = static_pointer_cast<arrow::DoubleArray>(
      bar_data->column(close_column)->chunk(0));
  const auto& volume_array = static_pointer_cast<arrow::DoubleArray>(
      bar_data->column(volume_column)->chunk(0));
  const auto& close_time_array = static_pointer_cast<arrow::Int64Array>(
      bar_data->column(close_time_column)->chunk(0));

  // raw 포인터로 직접 접근
  const int64_t* open_time_data = open_time_array->raw_values();
  const double* open_data = open_array->raw_values();
  const double* high_data = high_array->raw_values();
  const double* low_data = low_array->raw_values();
  const double* close_data = close_array->raw_values();
  const double* volume_data = volume_array->raw_values();
  const int64_t* close_time_data = close_time_array->raw_values();

  // 데이터 복사
#pragma omp parallel for
  for (int64_t row = 0; row < total_rows; row++) {
    bar_data_[symbol_idx][row] = {open_time_data[row], open_data[row],
                                  high_data[row],      low_data[row],
                                  close_data[row],     volume_data[row],
                                  close_time_data[row]};
  }
}

Bar& BarData::SafeGetBar(const int symbol_idx, const size_t bar_idx) {
  IsValidIndex(symbol_idx, bar_idx);

  return bar_data_[symbol_idx][bar_idx];
}

Bar& BarData::GetBar(const int symbol_idx, const size_t bar_idx) {
  return bar_data_[symbol_idx][bar_idx];
}

string& BarData::GetSymbolName(const int symbol_idx) {
  try {
    IsValidSymbolIndex(symbol_idx);
  } catch (const IndexOutOfRange& e) {
    Logger::LogAndThrowError(e.what(), __FILE__, __LINE__);
  }

  return symbol_names_[symbol_idx];
}

int BarData::GetNumSymbols() const { return num_symbols_; }

size_t BarData::GetNumBars(const int symbol_idx) const {
  try {
    IsValidSymbolIndex(symbol_idx);
  } catch (const IndexOutOfRange& e) {
    Logger::LogAndThrowError(e.what(), __FILE__, __LINE__);
  }

  return num_bars_[symbol_idx];
}

string& BarData::GetTimeframe() { return timeframe_; }

void BarData::IsValidIndex(const int symbol_idx, const size_t bar_idx) const {
  IsValidSymbolIndex(symbol_idx);

  // 0보다 작은 조건은 size_t이므로 제외
  if (bar_idx > num_bars_[symbol_idx] - 1) {
    throw IndexOutOfRange(
        format("지정된 바 인덱스 {}은(는) 최대값 {}을(를) 초과했습니다.",
               symbol_idx, num_bars_[symbol_idx] - 1));
  }
}

void BarData::IsValidSymbolIndex(int symbol_idx) const {
  if (symbol_idx > num_symbols_ - 1 || symbol_idx < 0) {
    throw IndexOutOfRange(
        format("지정된 심볼 인덱스 {}은(는) 최대값 {}을(를)"
               " 초과했거나 0 미만입니다.",
               symbol_idx, num_symbols_ - 1));
  }
}

void BarData::IsValidSettings(const string& symbol_name,
                              const string& timeframe,
                              const shared_ptr<arrow::Table>& bar_data,
                              const int open_time_column, const int open_column,
                              const int high_column, const int low_column,
                              const int close_column, const int volume_column,
                              const int close_time_column) const {
  if (symbol_name.empty()) {
    Logger::LogAndThrowError("심볼 이름이 비어있습니다.", __FILE__, __LINE__);
  }

  for (const auto& symbol : symbol_names_) {
    if (symbol == symbol_name) {
      Logger::LogAndThrowError(
          format("[{}]은(는) 이미 추가된 심볼입니다.", symbol_name), __FILE__,
          __LINE__);
      return;
    }
  }

  if (!timeframe_.empty() && timeframe != timeframe_) {
    Logger::LogAndThrowError(
        format("주어진 타임프레임 [{}]은(는) 바 데이터로 추가된 타임프레임 "
               "[{}]와(과) 일치하지 않습니다.",
               timeframe, timeframe_),
        __FILE__, __LINE__);
  }

  int columns[7] = {open_time_column, open_column,  high_column,
                    low_column,       close_column, volume_column,
                    close_time_column};

  if (ranges::any_of(columns, [&](const int column) { return column < 0; })) {
    Logger::LogAndThrowError("지정된 열 인덱스가 0보다 작습니다.", __FILE__,
                             __LINE__);
  }

  // 열 인덱스가 num_columns보다 큰지 검사
  if (ranges::any_of(columns, [&](const int column) {
        return column >= bar_data->num_columns();
      })) {
    Logger::LogAndThrowError(
        format("지정된 열 인덱스가 데이터 열의 최대 개수 {}을(를) 초과합니다.",
               bar_data->num_columns()),
        __FILE__, __LINE__);
  }

  // 데이터 타입 검사
  if (bar_data->schema()->field(columns[0])->type()->id() !=
      arrow::Type::INT64) {
    Logger::LogAndThrowError(
        format("[Open Time] 데이터로 사용되는 인덱스 {}의 데이터 타입이 "
               "Int64_t가 아닙니다. 현재 타입: {}",
               columns[0],
               bar_data->schema()->field(columns[0])->type()->ToString()),
        __FILE__, __LINE__);
  }

  string data_str[5] = {"Open", "High", "Low", "Close", "Volume"};

  for (int i = 1; i <= 5; ++i) {
    if (bar_data->schema()->field(columns[i])->type()->id() !=
        arrow::Type::DOUBLE) {
      Logger::LogAndThrowError(
          format("[{}] 데이터로 사용되는 인덱스 {}의 데이터 타입이 Double이 "
                 "아닙니다. 현재 타입: {}",
                 data_str[i - 1], columns[i],
                 bar_data->schema()->field(columns[i])->type()->ToString()),
          __FILE__, __LINE__);
    }
  }

  if (bar_data->schema()->field(columns[6])->type()->id() !=
      arrow::Type::INT64) {
    Logger::LogAndThrowError(
        format("[Close Time] 데이터로 사용되는 인덱스 {}의 데이터 타입이 "
               "Int64_t가 아닙니다. 현재 타입: {}",
               columns[6],
               bar_data->schema()->field(columns[6])->type()->ToString()),
        __FILE__, __LINE__);
  }
}

}  // namespace backtesting::bar