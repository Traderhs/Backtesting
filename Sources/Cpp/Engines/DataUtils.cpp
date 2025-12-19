// 표준 라이브러리
#include <algorithm>
#include <any>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <execution>
#include <filesystem>
#include <format>
#include <future>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <numeric>
#include <thread>

// 외부 라이브러리
#include "arrow/io/file.h"
#include "arrow/memory_pool.h"
#include "arrow/scalar.h"
#include "arrow/table.h"
#include "nlohmann/json.hpp"
#include "parquet/arrow/reader.h"
#include "parquet/arrow/writer.h"
#include "parquet/properties.h"

// 파일 헤더
#include "Engines/DataUtils.hpp"

// 내부 헤더
#include "Engines/Config.hpp"
#include "Engines/Logger.hpp"

// 네임 스페이스
namespace backtesting {
using namespace engine;
using namespace logger;
}  // namespace backtesting

namespace backtesting::utils {

int CountDecimalPlaces(const double value) {
  // 0이거나 정수인 경우 0 반환
  if (value == 0.0 || value == floor(value)) {
    return 0;
  }

  // 고정 소수점 표기법으로 강제 변환하여 과학적 표기법 방지
  ostringstream oss;
  oss << fixed << setprecision(15) << value;  // 15자리 정밀도로 고정
  string str = oss.str();

  // 소수점 위치 찾기
  const size_t pos = str.find('.');
  if (pos == string::npos) {
    return 0;
  }

  // 뒤에서부터 0을 제거하여 실제 유효한 소수 자릿수만 카운트
  string decimal_part = str.substr(pos + 1);
  while (!decimal_part.empty() && decimal_part.back() == '0') {
    decimal_part.pop_back();
  }

  return static_cast<int>(decimal_part.length());
}

double RoundToDecimalPlaces(const double value, const size_t decimal_places) {
  const auto scale = pow(10, decimal_places);
  return round(value * scale) / scale;
}

// typeid().name()에서 클래스 이름을 추출하는 함수
string ExtractClassName(const string& type_name) {
  // 1. 먼저 MSVC 스타일 체크 (class 포함된 형식)
  if (type_name.find("class") != string::npos) {
    // MSVC 형식: "class namespace::ClassName" 또는 "class ClassName"
    static const regex msvc_pattern(R"(.*::(\w+)$|^class\s+(\w+)$)");
    if (smatch match; regex_search(type_name, match, msvc_pattern)) {
      // 첫 번째 패턴이 매치되면 match[1],
      // 두 번째 패턴이 매치되면 match[2]를 사용
      for (size_t i = 1; i < match.size(); ++i) {
        if (match[i].matched) {
          return match[i].str();
        }
      }
    }
  }

  // 2. GCC 스타일 체크 (숫자와 알파벳 혼합)
  static const regex gcc_pattern(R"(\d+(\w+)E?$)");
  smatch match;
  string result;
  string temp = type_name;

  // 마지막 매치 찾기 (가장 마지막 알파벳 덩어리가 클래스 이름)
  while (regex_search(temp, match, gcc_pattern)) {
    if (match.size() > 1 && match[1].matched) {
      result = match[1].str();
    }
    temp = match.suffix();
  }

  if (!result.empty()) {
    return result;
  }

  // 3. 기타 컴파일러 대응 (첫 글자가 숫자인 경우 건너뛰기)
  const char* name = type_name.c_str();
  while (*name && isdigit(*name)) {
    name++;
  }

  return name;
}

shared_ptr<arrow::Table> ReadParquet(const string& file_path,
                                     const vector<int>& column_indices) {
  // 메모리 맵 파일 열기
  const auto& memory_mapped_result =
      arrow::io::MemoryMappedFile::Open(file_path, arrow::io::FileMode::READ);

  if (!memory_mapped_result.ok()) {
    throw runtime_error(
        format("[{}] 경로의 Parquet 파일을 열 수 없습니다.", file_path));
  }

  const auto& random_access_file = memory_mapped_result.ValueOrDie();
  const auto memory_pool = arrow::default_memory_pool();

  // Parquet Reader 속성
  parquet::ReaderProperties parquet_props =
      parquet::default_reader_properties();
  parquet_props.set_buffer_size(128 * 1024 * 1024);  // 128MB
  parquet_props.enable_buffered_stream();

  // Arrow Reader 속성
  parquet::ArrowReaderProperties arrow_props;
  arrow_props.set_batch_size(1048576);  // 1M
  arrow_props.set_pre_buffer(true);
  arrow_props.set_use_threads(true);

  // Parquet FileReader 생성
  auto parquet_reader =
      parquet::ParquetFileReader::Open(random_access_file, parquet_props);

  // Arrow FileReader 생성
  unique_ptr<parquet::arrow::FileReader> arrow_reader;
  PARQUET_THROW_NOT_OK(parquet::arrow::FileReader::Make(
      memory_pool, move(parquet_reader), arrow_props, &arrow_reader));
  arrow_reader->set_use_threads(true);

  // Row Group 병렬 읽기
  const int num_row_groups = arrow_reader->num_row_groups();
  vector<int> row_group_indices(num_row_groups);
  iota(row_group_indices.begin(), row_group_indices.end(), 0);

  // Table 읽기
  shared_ptr<arrow::Table> table;

  if (column_indices.empty()) {
    // 모든 컬럼을 Row Group 단위로 병렬 읽기
    PARQUET_THROW_NOT_OK(
        arrow_reader->ReadRowGroups(row_group_indices, &table));
  } else {
    // 지정된 컬럼만 Row Group 단위로 병렬 읽기
    PARQUET_THROW_NOT_OK(
        arrow_reader->ReadRowGroups(row_group_indices, column_indices, &table));
  }

  return table;
}

vector<shared_ptr<arrow::Table>> ReadParquetBatch(
    const vector<string>& file_paths, const vector<int>& column_indices) {
  if (file_paths.empty()) {
    return {};
  }

  const size_t num_files = file_paths.size();
  vector<shared_ptr<arrow::Table>> results(num_files);

  // 인덱스 벡터 생성
  vector<size_t> indices(num_files);
  iota(indices.begin(), indices.end(), 0);

  // 완전 병렬 처리 - 컬럼 프로젝션 적용
  exception_ptr first_exception = nullptr;
  mutex exc_mutex;
  atomic stop_flag{false};

  for_each(execution::par_unseq, indices.begin(), indices.end(),
           [&](const size_t idx) {
             // 이미 예외가 발생했으면 즉시 종료
             if (stop_flag.load(memory_order_acquire)) {
               return;
             }

             try {
               results[idx] = ReadParquet(file_paths[idx], column_indices);
             } catch (...) {
               // 첫 번째 예외만 저장하고 즉시 stop_flag를 세워 다른 작업을 종료
               lock_guard lock(exc_mutex);

               if (!first_exception) {
                 first_exception = current_exception();
                 stop_flag.store(true, memory_order_release);
               }

               results[idx] = nullptr;
             }
           });

  // 병렬 루프에서 예외가 발생했으면 즉시 상위로 전파
  if (first_exception) {
    rethrow_exception(first_exception);
  }

  return results;
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
    [[unlikely]] default:
      Logger::LogAndThrowError(
          "스칼라 값을 얻기 위해 일치하는 타입이 없습니다.: " + type, __FILE__,
          __LINE__);
      return nullptr;
  }
}

double GetDoubleFromJson(const json& data, const string& key) {
  try {
    const auto& value = data.at(key);

    // String이면 Double로 변환 후 반환
    if (value.is_string()) {
      return stod(value.get<string>());
    }

    // 숫자형이면 Double로 반환
    if (value.is_number()) {
      return value.get<double>();
    }

    // 그 외의 경우에는 오류 처리
    Logger::LogAndThrowError("유효하지 않은 값이 존재합니다.", __FILE__,
                             __LINE__);
  } catch ([[maybe_unused]] const exception& e) {
    Logger::LogAndThrowError(format("[{}] 키에서 오류가 발생했습니다.", key),
                             __FILE__, __LINE__);
    throw;
  }

  return NAN;
}

void TableToParquet(const shared_ptr<arrow::Table>& table,
                    const string& directory_path, const string& file_name,
                    const bool save_split_files) {
  // 폴더가 존재하지 않으면 생성
  if (!filesystem::exists(directory_path)) {
    filesystem::create_directories(directory_path);
  }

  // ※ 전체 저장
  // 파일 열기
  auto result =
      arrow::io::FileOutputStream::Open(directory_path + "/" + file_name);
  if (!result.ok()) {
    Logger::LogAndThrowError(
        "파일을 여는 데 실패했습니다.: " + result.status().ToString(), __FILE__,
        __LINE__);
  }

  // outfile를 지역 변수로 보관
  const auto& outfile = result.ValueOrDie();

  // Parquet로 테이블 쓰기
  const auto write_result = parquet::arrow::WriteTable(
      *table, arrow::default_memory_pool(), outfile, table->num_rows());
  if (!write_result.ok()) {
    Logger::LogAndThrowError(
        "테이블을 저장하는 데 실패했습니다.: " + write_result.ToString(),
        __FILE__, __LINE__);
  }

  // 파일 스트림 닫기
  if (const auto close_status = outfile->Close(); !close_status.ok()) {
    Logger::LogAndThrowError(
        "파일을 닫는 데 실패했습니다.: " + close_status.ToString(), __FILE__,
        __LINE__);
  }

  if (!save_split_files) {
    return;
  }

  // ===========================================================================
  // ※ 분할 저장
  // 전체 행 수와 청크 크기 설정
  int64_t total_rows = table->num_rows();
  int64_t chunk_size = 10000;

  // 테이블을 10000행씩 분할하여 저장
  for (int64_t offset = 0; offset < total_rows; offset += chunk_size) {
    int64_t current_chunk_size = min(chunk_size, total_rows - offset);
    auto table_chunk = table->Slice(offset, current_chunk_size);

    // "Open Time" 컬럼은 chunked array 형태이므로 첫 청크와 마지막 청크에서
    // 값을 추출
    auto open_time_column = table_chunk->column(0);

    // 첫 번째 청크의 첫 번째 값
    auto first_chunk = open_time_column->chunk(0);
    auto first_scalar_result = first_chunk->GetScalar(0);
    if (!first_scalar_result.ok()) {
      cerr << "Error getting first scalar value." << endl;
      continue;
    }
    const auto& first_scalar = first_scalar_result.ValueOrDie();

    // 마지막 청크의 마지막 값
    int last_chunk_index = open_time_column->num_chunks() - 1;
    auto last_chunk = open_time_column->chunk(last_chunk_index);
    int64_t last_index = last_chunk->length() - 1;
    auto last_scalar_result = last_chunk->GetScalar(last_index);
    if (!last_scalar_result.ok()) {
      cerr << "Error getting last scalar value." << endl;
      continue;
    }
    const auto& last_scalar = last_scalar_result.ValueOrDie();

    // Open Time 값은 Int64Scalar라고 가정하고, ms 단위를 second로 변환
    int64_t start_time_ms =
        static_pointer_cast<arrow::Int64Scalar>(first_scalar)->value;
    int64_t end_time_ms =
        static_pointer_cast<arrow::Int64Scalar>(last_scalar)->value;
    int64_t start_time_sec = start_time_ms / 1000;
    int64_t end_time_sec = end_time_ms / 1000;

    // 파일명 생성: <start_time_sec>_<end_time_sec>.parquet
    stringstream ss;
    ss << directory_path << "/" << start_time_sec << "_" << end_time_sec
       << ".parquet";
    string out_file = ss.str();

    // FileOutputStream 열기 (append false: 덮어쓰기)
    auto out_result = arrow::io::FileOutputStream::Open(out_file, false);
    if (!out_result.ok()) {
      Logger::LogAndThrowError("파일을 여는 데 실패했습니다.: " + out_file,
                               __FILE__, __LINE__);
    }
    const auto& split_file = *out_result;

    // 테이블 청크를 Parquet 파일로 저장
    // (row_group_size를 current_chunk_size로 지정)
    auto write_status =
        parquet::arrow::WriteTable(*table_chunk, arrow::default_memory_pool(),
                                   split_file, current_chunk_size);
    if (!write_status.ok()) {
      Logger::LogAndThrowError("파일을 저장하는 데 실패했습니다.: " + out_file,
                               __FILE__, __LINE__);
    }

    if (const auto close_status = split_file->Close(); !close_status.ok()) {
      Logger::LogAndThrowError(
          "파일을 닫는 데 실패했습니다.: " + close_status.ToString(), __FILE__,
          __LINE__);
    }
  }
}

void JsonToFile(future<json> data, const string& file_path) {
  if (ofstream file(file_path); file.is_open()) {
    file << data.get().dump(4);  // 4는 들여쓰기를 위한 인자
    file.close();
  } else {
    Logger::LogAndThrowError(format("{} 파일을 열 수 없습니다.", file_path),
                             __FILE__, __LINE__);
  }
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

double RoundToStep(const double value, const double step) {
  if (IsLessOrEqual(step, 0.0)) {
    Logger::LogAndThrowError(
        format("반올림을 위하여 주어진 스텝 [{}]은(는) 0보다 커야합니다.",
               to_string(step)),
        __FILE__, __LINE__);
  }

  const double result = round(value / step) * step;

  // 부동 소수점 정밀도 보정
  return round(result * 1e10) / 1e10;
}

string FormatDollar(const double price, const bool use_rounding) {
  // 음수 0 처리 - 매우 작은 값들은 0으로 처리
  double adjusted_price = price;
  if (fabs(price) < 1e-10) {
    adjusted_price = 0.0;
  }

  ostringstream oss;
  oss.imbue(global_locale);

  if (use_rounding) {
    // rounding 모드: 적절한 precision으로 반올림
    oss << showpoint << fixed;

    int precision = 2;  // 기본 2자리

    if (adjusted_price != 0.0) {
      // 2자리로 반올림했을 때 0이 되는지 빠르게 확인
      if (const double abs_price = fabs(adjusted_price); abs_price < 0.01) {
        // 로그를 이용한 빠른 정밀도 계산
        const double log_val = -log10(abs_price);
        precision = min(10, static_cast<int>(ceil(log_val)) + 1);
      }
    }

    oss << setprecision(precision);
  } else {
    // non-rounding 모드: trailing zeros 제거하고 원본 값 그대로 사용
    oss << noshowpoint;  // trailing zeros 제거
  }

  if (adjusted_price < 0.0) {
    oss << "-$" << -adjusted_price;
  } else {
    oss << "$" << adjusted_price;
  }

  return oss.str();
}

string FormatPercentage(double percentage, const bool use_rounding) {
  // 1. 음수 0 및 작은 값 처리 (분기 예측 최적화를 위해 간단한 비교 유지)
  if (abs(percentage) < 1e-10) {
    percentage = 0.0;
  }

  // 2. 스택 버퍼 할당 (힙 할당 제거)
  // double의 최대 자릿수 + 부호 + 소수점 + '%' + 여유분 고려 시 64바이트면 충분
  char buffer[64];
  char* pt = buffer;

  // 3. 부호 처리 (변환 함수에 넘기기 전 직접 처리하여 제어권 확보)
  if (percentage < 0) {
    *pt++ = '-';
    percentage = -percentage;
  }

  // 4. 숫자 변환
  auto [ptr, ec] = [&]() -> to_chars_result {
    if (use_rounding) {
      int precision = 2;  // 기본 정밀도

      // 0이 아니고 0.01 미만일 때만 정밀도 계산 (log 연산 비용 최소화)
      if (percentage > 0.0 && percentage < 0.01) {
        // log10은 무겁지만 필요악. 근사치 테이블을 쓰지 않는 한 유지.
        // 하지만 호출 빈도가 낮다면 큰 문제 없음.
        const double log_val = -log10(percentage);
        precision = min(10, static_cast<int>(ceil(log_val)) + 1);
      }

      // fixed 포맷으로 변환
      return to_chars(pt, buffer + sizeof(buffer), percentage,
                      chars_format::fixed, precision);
    }

    // [Non-rounding 모드]
    // 과학적 표기법 방지를 위해 fixed 사용.
    // precision을 15(double 유효자리)로 넉넉히 주어 잘림 방지.
    auto r = to_chars(pt, buffer + sizeof(buffer), percentage,
                      chars_format::fixed, 15);

    // [Trailing Zeros 제거 - 포인터 연산 최적화]
    // r.ptr은 숫자가 쓰여진 바로 다음 위치를 가리킴
    char* end_ptr = r.ptr - 1;

    // 소수점이 있는지 확인 (정수일 경우 0을 지우면 안 되므로)
    // to_chars의 fixed는 항상 소수점을 포함할 수 있으므로 안전하게 처리
    // 스캔 비용을 줄이기 위해 일단 뒤에서부터 '0'을 찾음
    while (end_ptr > pt && *end_ptr == '0') {
      end_ptr--;
    }

    // 만약 마지막이 소수점('.')이라면 그것도 제거
    if (end_ptr > pt && *end_ptr == '.') {
      end_ptr--;
    }

    // 유효한 끝 지점으로 포인터 업데이트
    r.ptr = end_ptr + 1;

    return r;
  }();

  // 5. '%' 기호 추가
  if (ec == errc()) {
    *ptr = '%';
    ptr++;
  }

  // 6. string 생성 (여기서만 힙 할당 1회 발생 - 반환 타입이 string이므로
  // 불가피) 포인터 차이를 이용해 길이를 계산하므로 strlen() 비용 없음
  return {buffer, static_cast<string::size_type>(ptr - buffer)};
}

string ToFixedString(const double value, const int precision) {
  ostringstream oss;
  oss.imbue(global_locale);
  oss << fixed << setprecision(precision) << value;
  return oss.str();
}

string GetEnvVariable(const string& env_var) {
  char* value = nullptr;
  size_t len = 0;

  if (const int result = _dupenv_s(&value, &len, env_var.c_str());
      result == 0 && value != nullptr) {
    string result_value(value);  // 가져온 값을 string으로 변환
    free(value);                 // value 메모리 해제
    return result_value;
  }

  // 환경 변수가 없을 경우 예외 처리
  Logger::LogAndThrowError(
      format("환경 변수 [{}]이(가) 존재하지 않습니다.", env_var), __FILE__,
      __LINE__);

  return {};  // 환경 변수가 없으면 빈 문자열 반환
}

void RunPythonScript(const string& script_path, const vector<string>& args) {
  const string& python_exe =
      Config::GetProjectDirectory() + "/Sources/py/Anaconda/python.exe";

  // 경로 확인
  if (!filesystem::exists(script_path)) {
    Logger::LogAndThrowError(
        format("파이썬 스크립트 [{}]을(를) 찾을 수 없습니다.", script_path),
        __FILE__, __LINE__);
  }
  if (!filesystem::exists(python_exe)) {
    Logger::LogAndThrowError(
        format("파이썬 인터프리터 [{}]을(를) 찾을 수 없습니다.", python_exe),
        __FILE__, __LINE__);
  }

  ostringstream command;
  command << "cmd /c \"\"" << python_exe << "\" \"" << script_path << "\"";

  for (const auto& arg : args) {
    command << " \"" << arg << "\"";
  }
  command << "\"";  // 마지막 닫는 따옴표

  if (const int ret = system(command.str().c_str()); ret != 0) {
    Logger::LogAndThrowError(format("파이썬 스크립트 [{}]을(를) 실행하는 중 "
                                    "오류가 발생했습니다. 반환 코드 [{}]",
                                    script_path, ret),
                             __FILE__, __LINE__);
  }
}

string OpenHtml(const string& html_path) {
  ifstream html(html_path);

  if (!html.is_open()) {
    throw runtime_error(
        format("[{}]이(가) 존재하지 않거나 열 수 없습니다.", html_path));
  }

  string html_str((istreambuf_iterator(html)), istreambuf_iterator<char>());

  return html_str;
}

string RemoveParquetExtension(const string& file_path) {
  constexpr size_t extension_length = 8;  // ".parquet" 의 길이는 8

  // .parquet를 제거한 경로 반환
  return file_path.substr(0, file_path.length() - extension_length);
}

}  // namespace backtesting::utils