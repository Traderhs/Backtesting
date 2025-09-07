// 표준 라이브러리
#include <algorithm>
#include <any>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <future>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <thread>
#include <unordered_map>

// 외부 라이브러리
#include "arrow/io/api.h"
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

// 파일 메타데이터 캐시
struct FileMetadata {
  int64_t file_size{};
  chrono::time_point<chrono::file_clock> last_write_time;
  shared_ptr<parquet::FileMetaData> parquet_metadata;
};
static unordered_map<string, FileMetadata> metadata_cache;
static mutex metadata_cache_mutex;

size_t CountDecimalPlaces(const double value) {
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

  return decimal_part.length();
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

// 파일 메타데이터 검증 및 캐싱
bool IsFileMetadataValid(const string& file_path,
                         const FileMetadata& cached_metadata) {
  try {
    const filesystem::path path(file_path);
    if (!filesystem::exists(path)) {
      return false;
    }

    const auto file_size = filesystem::file_size(path);
    const auto last_write_time = filesystem::last_write_time(path);

    return (file_size == cached_metadata.file_size &&
            last_write_time == cached_metadata.last_write_time);
  } catch (...) {
    return false;
  }
}

shared_ptr<arrow::Table> ReadParquet(const string& file_path) {
  try {
    // 파일 크기 확인을 통한 최적화된 읽기 방식 선택
    filesystem::path path(file_path);
    if (!filesystem::exists(path)) {
      Logger::LogAndThrowError("파일이 존재하지 않습니다: " + file_path,
                               __FILE__, __LINE__);
    }

    const auto file_size = filesystem::file_size(path);
    constexpr int64_t memory_map_threshold = 50 * 1024 * 1024;  // 50MB

    // 파일 메타데이터 캐시 확인
    {
      std::lock_guard lock(metadata_cache_mutex);
      if (auto cache_it = metadata_cache.find(file_path);
          cache_it != metadata_cache.end()) {
        if (!IsFileMetadataValid(file_path, cache_it->second)) {
          metadata_cache.erase(cache_it);
        }
      }
    }

    // 최적화된 메모리 풀 사용
    auto memory_pool = arrow::default_memory_pool();

    shared_ptr<arrow::io::ReadableFile> infile;
    shared_ptr<arrow::io::RandomAccessFile> random_access_file;

    // 파일 크기에 따른 최적화된 읽기 방식 선택
    if (file_size > memory_map_threshold) {
      // 큰 파일: 메모리 맵 사용
      auto memory_mapped_result = arrow::io::MemoryMappedFile::Open(
          file_path, arrow::io::FileMode::READ);
      if (memory_mapped_result.ok()) {
        random_access_file = memory_mapped_result.ValueOrDie();
      } else {
        // 메모리 맵 실패 시 일반 파일 읽기로 폴백
        PARQUET_ASSIGN_OR_THROW(
            infile, arrow::io::ReadableFile::Open(file_path, memory_pool));
        random_access_file = infile;
      }
    } else {
      // 작은 파일: 일반 파일 읽기 (버퍼링은 parquet 레벨에서 처리)
      PARQUET_ASSIGN_OR_THROW(
          infile, arrow::io::ReadableFile::Open(file_path, memory_pool));
      random_access_file = infile;
    }

    // Parquet Reader 속성 최적화
    parquet::ReaderProperties reader_props =
        parquet::default_reader_properties();
    reader_props.set_buffer_size(
        std::min(static_cast<int64_t>(1024 * 1024 * 4),
                 static_cast<int64_t>(file_size)));  // 최대 4MB
    reader_props.enable_buffered_stream();

    // Arrow Reader 속성 최적화
    parquet::ArrowReaderProperties arrow_props;
    arrow_props.set_batch_size(
        std::min(65536, static_cast<int>(file_size / 1000)));  // 동적 배치 크기
    arrow_props.set_pre_buffer(true);   // 프리버퍼링 활성화
    arrow_props.set_use_threads(true);  // 멀티스레딩 활성화

    // Parquet Arrow의 FileReader 생성 (최적화된 속성 사용)
    arrow::Result<unique_ptr<parquet::arrow::FileReader>> result =
        parquet::arrow::OpenFile(random_access_file, memory_pool);

    PARQUET_THROW_NOT_OK(result.status());
    const unique_ptr<parquet::arrow::FileReader> reader =
        std::move(result).ValueOrDie();

    // 메타데이터 캐싱
    auto parquet_metadata = reader->parquet_reader()->metadata();
    {
      std::lock_guard lock(metadata_cache_mutex);
      if (filesystem::exists(path)) {
        FileMetadata metadata;
        metadata.file_size = static_cast<int64_t>(file_size);
        metadata.last_write_time = filesystem::last_write_time(path);
        metadata.parquet_metadata = parquet_metadata;
        metadata_cache[file_path] = std::move(metadata);
      }
    }

    // 최적화된 Table 읽기
    shared_ptr<arrow::Table> table;

    // 행 그룹 수에 따른 읽기 방식 최적화
    if (const int num_row_groups = parquet_metadata->num_row_groups();
        num_row_groups > 1) {
      // 여러 행 그룹: 병렬 읽기
      vector<int> row_groups;
      for (int i = 0; i < num_row_groups; ++i) {
        row_groups.push_back(i);
      }
      PARQUET_THROW_NOT_OK(reader->ReadRowGroups(row_groups, &table));
    } else {
      // 단일 행 그룹: 일반 읽기
      PARQUET_THROW_NOT_OK(reader->ReadTable(&table));
    }

    // 안전한 스트림 닫기
    PARQUET_THROW_NOT_OK(random_access_file->Close());

    return table;
  } catch (const parquet::ParquetException& e) {
    Logger::LogAndThrowError(
        "Parquet 데이터 경로가 유효하지 않습니다.: " + string(e.what()),
        __FILE__, __LINE__);
    return nullptr;
  } catch (const exception& e) {
    Logger::LogAndThrowError(
        "파일 읽기 중 오류가 발생했습니다: " + string(e.what()), __FILE__,
        __LINE__);
    return nullptr;
  }
}

vector<shared_ptr<arrow::Table>> ReadParquetBatch(
    const vector<string>& file_paths) {
  if (file_paths.empty()) {
    return {};
  }

  const size_t num_files = file_paths.size();
  const size_t num_threads = std::min(
      num_files, static_cast<size_t>(std::thread::hardware_concurrency()));

  vector<shared_ptr<arrow::Table>> results(num_files);
  vector<std::future<void>> futures;

  // 파일들을 스레드별로 분할하여 병렬 처리
  const size_t chunk_size = (num_files + num_threads - 1) / num_threads;

  for (size_t thread_idx = 0; thread_idx < num_threads; ++thread_idx) {
    size_t start_idx = thread_idx * chunk_size;
    size_t end_idx = std::min(start_idx + chunk_size, num_files);

    if (start_idx >= num_files) break;

    futures.emplace_back(
        std::async(std::launch::async, [&, start_idx, end_idx]() {
          for (size_t i = start_idx; i < end_idx; ++i) {
            try {
              results[i] = ReadParquet(file_paths[i]);
            } catch (...) {
              results[i] = nullptr;
            }
          }
        }));
  }

  // 모든 병렬 작업 완료 대기
  for (auto& future : futures) {
    future.wait();
  }

  return results;
}

void ClearParquetMetadataCache() {
  std::lock_guard lock(metadata_cache_mutex);
  metadata_cache.clear();
  // 메모리 풀 캐시는 제거됨 (직접 arrow::system_memory_pool 사용)
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
    int64_t current_chunk_size = std::min(chunk_size, total_rows - offset);
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

double RoundToTickSize(const double price, const double tick_size) {
  if (IsLessOrEqual(tick_size, 0.0)) {
    Logger::LogAndThrowError(
        format("주어진 틱 사이즈 {}은(는) 0보다 커야합니다.",
               to_string(tick_size)),
        __FILE__, __LINE__);
  }

  const double result = round(price / tick_size) * tick_size;

  // 부동 소수점 정밀도 보정
  return round(result * 1e10) / 1e10;
}

string FormatDollar(const double price, const bool use_rounding) {
  // 음수 0 처리 - 매우 작은 값들은 0으로 처리
  double adjusted_price = price;
  if (abs(price) < 1e-10) {
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
      if (const double abs_price = abs(adjusted_price); abs_price < 0.01) {
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

string FormatPercentage(const double percentage, const bool use_rounding) {
  // 음수 0 처리 - 매우 작은 값들은 0으로 처리
  double adjusted_percentage = percentage;
  if (abs(percentage) < 1e-10) {
    adjusted_percentage = 0.0;
  }

  ostringstream oss;
  oss.imbue(global_locale);

  if (use_rounding) {
    // rounding 모드: 적절한 precision으로 반올림
    oss << showpoint << fixed;

    int precision = 2;  // 기본 2자리

    if (adjusted_percentage != 0.0) {
      // 2자리로 반올림했을 때 0이 되는지 빠르게 확인
      if (const double abs_percentage = abs(adjusted_percentage);
          abs_percentage < 0.01) {
        // 로그를 이용한 빠른 정밀도 계산
        const double log_val = -log10(abs_percentage);
        precision = min(10, static_cast<int>(ceil(log_val)) + 1);
      }
    }

    oss << setprecision(precision);
  } else {
    // non-rounding 모드: trailing zeros 제거하고 원본 값 그대로 사용
    oss << noshowpoint;  // trailing zeros 제거
  }

  if (adjusted_percentage < 0.0) {
    oss << "-" << -adjusted_percentage << "%";
  } else {
    oss << adjusted_percentage << "%";
  }

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
      Config::GetRootDirectory() + "/Sources/py/Anaconda/python.exe";

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