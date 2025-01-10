// 표준 라이브러리
#include <any>
#include <cmath>
#include <format>

// 외부 라이브러리
#include <arrow\io\file.h>
#include <parquet\arrow\reader.h>
#include <parquet\arrow\writer.h>

// 파일 헤더
#include "Engines\DataUtils.hpp"

// 내부 헤더
#include "Engines\Logger.hpp"
#include "Engines\TimeUtils.hpp"

// 네임 스페이스
using namespace time_utils;

namespace data_utils {
size_t CountDecimalPlaces(const double value) {
  ostringstream oss;
  oss << value;
  const string& str = oss.str();

  // 소수점 위치 찾기
  const size_t& pos = str.find('.');

  // 소수점이 없는 경우 0
  if (pos == string::npos) {
    return 0;
  }

  // 소수점 이하의 부분 문자열의 길이 리턴
  return str.substr(pos + 1).length();
}

double RoundToDecimalPlaces(const double value, const size_t decimal_places) {
  const auto scale = pow(10, decimal_places);
  return round(value * scale) / scale;
}

shared_ptr<Table> ReadParquet(const string& file_path) {
  try {
    // Arrow의 ReadableFile 생성
    shared_ptr<io::ReadableFile> infile;
    PARQUET_ASSIGN_OR_THROW(
        infile,
        arrow::io::ReadableFile::Open(file_path, arrow::default_memory_pool()));

    // Parquet Arrow의 FileReader 생성
    unique_ptr<parquet::arrow::FileReader> reader;
    PARQUET_THROW_NOT_OK(parquet::arrow::OpenFile(
        infile, arrow::default_memory_pool(), &reader));

    // FileReader로 Table을 읽어옴
    shared_ptr<Table> table;
    PARQUET_THROW_NOT_OK(reader->ReadTable(&table));

    return table;
  } catch (const parquet::ParquetException& e) {
    Logger::LogAndThrowError(
        "테이블을 불러오는 데 실패했습니다.: " + string(e.what()), __FILE__,
        __LINE__);
    return nullptr;
  }
}

any GetCellValue(const shared_ptr<Table>& table, const string& column_name,
                 const int64_t row_index) {
  // 열 인덱스 찾기
  const int column_index = table->schema()->GetFieldIndex(column_name);
  if (column_index == -1) {
    Logger::LogAndThrowError("해당되는 열 이름이 없습니다: " + column_name,
                             __FILE__, __LINE__);
    return nullptr;
  }

  return GetCellValue(table, column_index, row_index);
}

any GetCellValue(const shared_ptr<Table>& table, const int column_index,
                 const int64_t row_index) {
  if (column_index < 0 || column_index >= table->num_columns()) {
    Logger::LogAndThrowError(
        "잘못된 열 인덱스입니다: " + to_string(column_index), __FILE__,
        __LINE__);
    return nullptr;
  }

  // 열 데이터 가져오기
  const auto& column = table->column(column_index);

  // 해당 행의 값을 가져오기
  const auto& chunked_array = column->chunk(0);
  return GetScalarValue(chunked_array->GetScalar(row_index).ValueOrDie());
}

any GetScalarValue(const shared_ptr<Scalar>& scalar) {
  switch (const auto type = scalar->type->id()) {
    case Type::INT16:
      return dynamic_pointer_cast<Int16Scalar>(scalar)->value;
    case Type::INT32:
      return dynamic_pointer_cast<Int32Scalar>(scalar)->value;
    case Type::INT64:
      return dynamic_pointer_cast<Int64Scalar>(scalar)->value;
    case Type::DOUBLE:
      return dynamic_pointer_cast<DoubleScalar>(scalar)->value;
    case Type::STRING:
      return dynamic_pointer_cast<StringScalar>(scalar)->ToString();
    default:
      Logger::LogAndThrowError("해당되는 타입이 없습니다.: " + type, __FILE__,
                               __LINE__);
      return nullptr;
  }
}

void TableToParquet(const shared_ptr<Table>& table, const string& file_path) {
  static shared_ptr<io::FileOutputStream> outfile;
  auto result = io::FileOutputStream::Open(file_path);
  if (!result.ok()) {
    Logger::LogAndThrowError(
        "파일을 여는 데 실패했습니다.: " + result.status().ToString(), __FILE__,
        __LINE__);
  }

  outfile = result.ValueOrDie();

  const auto write_result = parquet::arrow::WriteTable(
      *table, default_memory_pool(), outfile, table->num_rows());
  if (!write_result.ok())
    Logger::LogAndThrowError(
        "테이블을 저장하는 데 실패했습니다.: " + write_result.ToString(),
        __FILE__, __LINE__);
}

pair<shared_ptr<Table>, shared_ptr<Table>> SplitTable(
    const shared_ptr<Table>& table, const double split_ratio) {
  const int64_t num_rows = table->num_rows();
  const auto split_index =
      static_cast<int64_t>(static_cast<double>(num_rows) * split_ratio);

  vector<shared_ptr<ChunkedArray>> first_chunked_arrays;
  vector<shared_ptr<ChunkedArray>> second_chunked_arrays;
  first_chunked_arrays.reserve(table->num_columns());
  second_chunked_arrays.reserve(table->num_columns());

  for (int i = 0; i < table->num_columns(); ++i) {
    first_chunked_arrays.push_back(table->column(i)->Slice(0, split_index));
    second_chunked_arrays.push_back(
        table->column(i)->Slice(split_index, num_rows - split_index));
  }

  return {Table::Make(table->schema(), first_chunked_arrays),
          Table::Make(table->schema(), second_chunked_arrays)};
}

double RoundToTickSize(const double price, const double tick_size) {
  if (tick_size <= 0) {
    Logger::LogAndThrowError(
        format("주어진 틱 사이즈 {}은(는) 0보다 커야합니다.",
               to_string(tick_size)),
        __FILE__, __LINE__);
  }

  return round(price / tick_size) * tick_size;
}
}