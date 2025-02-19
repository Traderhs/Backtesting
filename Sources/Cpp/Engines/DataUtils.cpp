// 표준 라이브러리
#include <any>
#include <cmath>
#include <format>
#include <iomanip>

// 외부 라이브러리
#include <arrow/io/file.h>
#include <parquet/arrow/reader.h>
#include <parquet/arrow/writer.h>

// 파일 헤더
#include "Engines/DataUtils.hpp"

// 내부 헤더
#include "Engines/Logger.hpp"
#include "Engines/TimeUtils.hpp"

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

shared_ptr<arrow::Table> ReadParquet(const string& file_path) {
  try {
    // Arrow의 ReadableFile 생성
    shared_ptr<arrow::io::ReadableFile> infile;
    PARQUET_ASSIGN_OR_THROW(
        infile,
        arrow::io::ReadableFile::Open(file_path, arrow::default_memory_pool()));

    // Parquet Arrow의 FileReader 생성
    arrow::Result<unique_ptr<parquet::arrow::FileReader>> result =
        parquet::arrow::OpenFile(infile, arrow::default_memory_pool());

    PARQUET_THROW_NOT_OK(result.status());
    const unique_ptr<parquet::arrow::FileReader> reader =
        move(result).ValueOrDie();

    // FileReader로 Table을 읽어옴
    shared_ptr<arrow::Table> table;
    PARQUET_THROW_NOT_OK(reader->ReadTable(&table));

    return table;
  } catch (const parquet::ParquetException& e) {
    Logger::LogAndThrowError(
        "테이블을 불러오는 데 실패했습니다.: " + string(e.what()), __FILE__,
        __LINE__);
    return nullptr;
  }
}

any GetCellValue(const shared_ptr<arrow::Table>& table,
                 const string& column_name, const int64_t row_index) {
  // 열 인덱스 찾기
  const int column_index = table->schema()->GetFieldIndex(column_name);
  if (column_index == -1) {
    Logger::LogAndThrowError("해당되는 열 이름이 없습니다: " + column_name,
                             __FILE__, __LINE__);
    return nullptr;
  }

  return GetCellValue(table, column_index, row_index);
}

any GetCellValue(const shared_ptr<arrow::Table>& table, const int column_index,
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

any GetScalarValue(const shared_ptr<arrow::Scalar>& scalar) {
  switch (const auto type = scalar->type->id()) {
    case arrow::Type::INT16:
      return dynamic_pointer_cast<arrow::Int16Scalar>(scalar)->value;
    case arrow::Type::INT32:
      return dynamic_pointer_cast<arrow::Int32Scalar>(scalar)->value;
    case arrow::Type::INT64:
      return dynamic_pointer_cast<arrow::Int64Scalar>(scalar)->value;
    case arrow::Type::DOUBLE:
      return dynamic_pointer_cast<arrow::DoubleScalar>(scalar)->value;
    case arrow::Type::STRING:
      return dynamic_pointer_cast<arrow::StringScalar>(scalar)->ToString();
    default:
      Logger::LogAndThrowError("해당되는 타입이 없습니다.: " + type, __FILE__,
                               __LINE__);
      return nullptr;
  }
}

void TableToParquet(const shared_ptr<arrow::Table>& table,
                    const string& file_path) {
  static shared_ptr<arrow::io::FileOutputStream> outfile;
  auto result = arrow::io::FileOutputStream::Open(file_path);
  if (!result.ok()) {
    Logger::LogAndThrowError(
        "파일을 여는 데 실패했습니다.: " + result.status().ToString(), __FILE__,
        __LINE__);
  }

  outfile = result.ValueOrDie();

  const auto write_result = parquet::arrow::WriteTable(
      *table, arrow::default_memory_pool(), outfile, table->num_rows());
  if (!write_result.ok())
    Logger::LogAndThrowError(
        "테이블을 저장하는 데 실패했습니다.: " + write_result.ToString(),
        __FILE__, __LINE__);
}

void VectorToCsv(const vector<double>& data, const string& file_name) {
  // 파일 출력 스트림 열기
  ofstream file(file_name, ios::trunc);  // trunc 옵션으로 파일 내용 초기화

  // 파일 열기 실패 시 에러 출력
  if (!file.is_open()) {
    Logger::LogAndThrowError(file_name + " 파일을 열지 못했습니다.", __FILE__,
                             __LINE__);
  }

  // 최대 15자리 소수점 저장
  file << fixed << setprecision(15);

  // 벡터 데이터를 CSV 형식으로 작성
  for (const auto& value : data) {
    file << value << '\n';  // 각 값을 한 줄씩 쓰기
  }

  // 파일 닫기
  file.close();
}

pair<shared_ptr<arrow::Table>, shared_ptr<arrow::Table>> SplitTable(
    const shared_ptr<arrow::Table>& table, const double split_ratio) {
  const int64_t num_rows = table->num_rows();
  const auto split_index =
      static_cast<int64_t>(static_cast<double>(num_rows) * split_ratio);

  vector<shared_ptr<arrow::ChunkedArray>> first_chunked_arrays;
  vector<shared_ptr<arrow::ChunkedArray>> second_chunked_arrays;
  first_chunked_arrays.reserve(table->num_columns());
  second_chunked_arrays.reserve(table->num_columns());

  for (int i = 0; i < table->num_columns(); ++i) {
    first_chunked_arrays.push_back(table->column(i)->Slice(0, split_index));
    second_chunked_arrays.push_back(
        table->column(i)->Slice(split_index, num_rows - split_index));
  }

  return {arrow::Table::Make(table->schema(), first_chunked_arrays),
          arrow::Table::Make(table->schema(), second_chunked_arrays)};
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

string FormatDollar(const double price) {
  ostringstream oss;
  oss.imbue(global_locale); // 천 단위 쉼표 추가
  oss << showpoint; // 소수점 유지
  oss << fixed; // 고정 소수점 형식

  if (price < 0)
    oss << "-$" << -price;  // 음수일 때 -$
  else
    oss << "$" << price;     // 양수일 때 $

  return oss.str();
}
}