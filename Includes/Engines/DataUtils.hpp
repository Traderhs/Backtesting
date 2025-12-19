#pragma once

// 표준 라이브러리
#include <any>
#include <future>
#include <locale>
#include <regex>
#include <string>

// 외부 라이브러리
#include <nlohmann/json_fwd.hpp>

// 전방 선언
namespace arrow {
class Array;
class Table;
struct Scalar;
}  // namespace arrow

// 네임스페이스
using namespace std;
using namespace nlohmann;

/// 데이터 핸들링을 위한 유틸리티 네임스페이스
namespace backtesting::utils {

/**
 * 주어진 값의 소수점 자릿수를 계산하는 함수
 *
 * @param value 소수 자릿수를 계산할 값
 * @return 주어진 double 값에 존재하는 소수점 자릿수의 개수
 */
[[nodiscard]] int CountDecimalPlaces(double value);

/**
 * 주어진 값을 지정된 소수 자릿수로 반올림하여 반환하는 함수
 *
 * @param value 반올림할 값
 * @param decimal_places 값이 반올림될 소수점 자릿수
 * @return 주어진 값이 지정된 소수 자릿수로 반올림된 값
 */
[[nodiscard]] double RoundToDecimalPlaces(double value, size_t decimal_places);

/**
 * typeid().name()에서 클래스 이름을 추출하는 함수
 *
 * @param type_name typeid(T).name()의 결과 문자열
 * @return 추출된 클래스 이름
 */
[[nodiscard]] string ExtractClassName(const string& type_name);

/**
 * 지정된 경로의 Parquet 파일에서 특정 컬럼만 읽어오는 함수
 *
 * @param file_path 읽을 Parquet 파일의 경로
 * @param column_indices 읽을 컬럼의 인덱스 목록 (빈 벡터면 모든 컬럼 읽기)
 * @return 변환된 테이블을 포함하는 shared_ptr 객체
 */
[[nodiscard]] shared_ptr<arrow::Table> ReadParquet(
    const string& file_path, const vector<int>& column_indices = {});

/**
 * 여러 Parquet 파일을 병렬로 읽어들이는 함수
 *
 * @param file_paths 읽을 Parquet 파일들의 경로 목록
 * @param column_indices 읽을 컬럼의 인덱스 목록
 * @return 변환된 테이블들을 포함하는 vector
 */
[[nodiscard]] vector<shared_ptr<arrow::Table>> ReadParquetBatch(
    const vector<string>& file_paths, const vector<int>& column_indices);

/**
 * Parquet 파일 메타데이터 캐시를 정리하는 함수
 */
void ClearParquetMetadataCache();

/**
 * 지정된 테이블에서 주어진 열 이름과 행 인덱스에 해당하는
 * 셀의 값을 반환하는 함수
 *
 * 리턴받은 값은 any_cast가 필요
 *
 * @param table 데이터를 포함하는 테이블의 공유 포인터
 * @param column_name 값을 검색할 열의 이름
 * @param row_index 값을 검색할 행의 인덱스
 * @return 지정된 열과 행의 인덱스에 해당하는 셀의 값
 */
[[nodiscard]] any GetCellValue(const shared_ptr<arrow::Table>& table,
                               const string& column_name, int64_t row_index);

/**
 * 지정된 테이블에서 주어진 열 인덱스와 행 인덱스에 해당하는
 * 셀의 값을 반환하는 함수
 *
 * 리턴받은 값은 any_cast가 필요
 *
 * @param table 데이터를 포함하는 테이블의 공유 포인터
 * @param column_index 값을 검색할 열의 인덱스
 * @param row_index 값을 검색할 행의 인덱스
 * @return 지정된 열과 행의 인덱스에 해당하는 셀의 값
 */
[[nodiscard]] any GetCellValue(const shared_ptr<arrow::Table>& table,
                               int column_index, int64_t row_index);

/**
 * 주어진 Scalar 객체에서 값을 추출하여 반환하는 함수
 *
 * 반환받은 값은 any_cast로 사용
 *
 * @param scalar 값을 추출할 Scalar 객체의 shared_ptr
 * @return 주어진 Scalar의 실제 값
 */
[[nodiscard]] any GetScalarValue(const shared_ptr<arrow::Scalar>& scalar);

/// 주어진 Json에서 주어진 키를 찾고 Double로 반환하는 함수
[[nodiscard]] double GetDoubleFromJson(const json& data, const string& key);

/**
 * 주어진 테이블을 Parquet 파일 형식으로 지정된 파일 경로에 저장하는 함수
 *
 * 로딩 최적화를 위해 분할한 parquet 파일을 추가 저장하는 기능 제공
 *
 * @param table 저장할 데이터를 포함하는 Table 객체에 대한 shared_ptr
 * @param directory_path 데이터를 저장할 폴더의 경로
 * @param file_name 파일 이름
 * @param save_split_files 분할 저장을 할지 결정하는 플래그
 */
void TableToParquet(const shared_ptr<arrow::Table>& table,
                    const string& directory_path, const string& file_name,
                    bool save_split_files);

/// Json을 지정된 경로에 파일로 저장하는 함수
void JsonToFile(future<json> data, const string& file_path);

/**
 * 주어진 테이블을 주어진 비율로 분할하여 두 개의 서브 테이블을 생성하여
 * 반환하는 함수
 *
 * @param table 분할할 원본 테이블을 가리키는 shared_ptr
 * @param split_ratio 테이블을 나눌 비율을 나타내는 값 (0.0 - 1.0)
 * @return 주어진 테이블을 `split_ratio`에 따라 나눈 두 개의 서브 테이블을
 * 포함하는 pair 객체. 첫 번째는 `split_ratio` 비율, 두 번째는 나머지 비율
 */
[[nodiscard]] pair<shared_ptr<arrow::Table>, shared_ptr<arrow::Table>>
SplitTable(const shared_ptr<arrow::Table>& table, double split_ratio);

/// 최소 스텝 크기로 값을 반올림하여 반환하는 함수
[[nodiscard]] double RoundToStep(double value, double step);

// FormatDollar에서 사용하는 공통 Locale 설정
static locale global_locale("en_US.UTF-8");

/// 금액을 천 단위 쉼표와 달러 표기로 포맷하여 반환하는 함수
[[nodiscard]] string FormatDollar(double price, bool use_rounding);

/// 퍼센트 값을 퍼센트 형식으로 포맷하여 반환하는 함수
[[nodiscard]] string FormatPercentage(double percentage, bool use_rounding);

/// 값을 주어진 정밀도의 string으로 변환하여 반환하는 함수
string ToFixedString(double value, int precision);

/// 환경 변수 값을 가져오는 함수
[[nodiscard]] string GetEnvVariable(const string& env_var);

/// 파이썬 스크립트를 실행하는 함수
void RunPythonScript(const string& script_path,
                     const vector<string>& args = {});

/// 지정된 경로에서 Html 파일을 열고 String으로 반환하는 함수
[[nodiscard]] string OpenHtml(const string& html_path);

/// Parquet 확장자를 제거한 문자열을 반환하는 함수
string RemoveParquetExtension(const string& file_path);

/// 부동 소수점 같은 값 비교를 위한 함수.
/// 왼쪽 값이 오른쪽 값과 같으면 true를 반환함.
template <typename T, typename U>
[[nodiscard]] __forceinline bool IsEqual(T a, U b) noexcept {
  using CommonType = common_type_t<T, U>;

  const auto ca = static_cast<CommonType>(a);
  const auto cb = static_cast<CommonType>(b);

  // NaN 체크
  if (std::isnan(ca) | std::isnan(cb)) [[unlikely]] {
    return false;
  }

  // 절대적 오차 (매우 작은 값들을 위해) - double precision 기준
  constexpr CommonType abs_tolerance =
      numeric_limits<CommonType>::epsilon() * 100;

  // 상대적 오차
  constexpr CommonType rel_tolerance = CommonType(1e-12);

  const CommonType diff = ca > cb ? ca - cb : cb - ca;

  // 절대적 오차 체크 (0에 가까운 값들)
  if (diff <= abs_tolerance) {
    return true;
  }

  // 상대적 오차 체크 (큰 값들)
  const CommonType abs_ca = ca < 0 ? -ca : ca;
  const CommonType abs_cb = cb < 0 ? -cb : cb;
  const CommonType max_val = abs_ca > abs_cb ? abs_ca : abs_cb;
  return diff <= rel_tolerance * max_val;
}

/// 부동 소수점 같지 않음 비교를 위한 함수.
/// 왼쪽 값이 오른쪽 값과 같지 않으면 true를 반환함.
template <typename T, typename U>
[[nodiscard]] __forceinline bool IsDiff(T a, U b) noexcept {
  using CommonType = common_type_t<T, U>;

  const auto ca = static_cast<CommonType>(a);
  const auto cb = static_cast<CommonType>(b);

  // NaN 체크
  if (std::isnan(ca) | std::isnan(cb)) [[unlikely]] {
    return true;
  }

  // 절대적 오차 (매우 작은 값들을 위해) - double precision 기준
  constexpr CommonType abs_tolerance =
      numeric_limits<CommonType>::epsilon() * 100;

  // 상대적 오차
  constexpr CommonType rel_tolerance = CommonType(1e-12);

  const CommonType diff = ca > cb ? ca - cb : cb - ca;

  // 절대적 오차 체크 (0에 가까운 값들)
  if (diff <= abs_tolerance) {
    return false;
  }

  // 상대적 오차 체크 (큰 값들)
  const CommonType abs_ca = ca < 0 ? -ca : ca;
  const CommonType abs_cb = cb < 0 ? -cb : cb;
  const CommonType max_val = abs_ca > abs_cb ? abs_ca : abs_cb;
  return diff > rel_tolerance * max_val;
}

/// 부동 소수점 크기 비교를 위한 함수.
/// 왼쪽 값이 오른쪽 값보다 크면 true를 반환함.
template <typename T, typename U>
[[nodiscard]] __forceinline bool IsGreater(T a, U b) noexcept {
  using CommonType = common_type_t<T, U>;

  const auto ca = static_cast<CommonType>(a);
  const auto cb = static_cast<CommonType>(b);

  // NaN 체크
  if (std::isnan(ca) | std::isnan(cb)) [[unlikely]] {
    return false;
  }

  // 절대적 오차
  constexpr CommonType abs_tolerance =
      numeric_limits<CommonType>::epsilon() * 100;
  constexpr CommonType rel_tolerance = CommonType(1e-12);

  const CommonType diff = ca - cb;
  const CommonType abs_diff = diff < 0 ? -diff : diff;

  // 같은 값인지 체크
  if (abs_diff <= abs_tolerance) {
    return false;
  }

  const CommonType abs_ca = ca < 0 ? -ca : ca;
  const CommonType abs_cb = cb < 0 ? -cb : cb;
  const CommonType max_val = abs_ca > abs_cb ? abs_ca : abs_cb;
  if (abs_diff <= rel_tolerance * max_val) {
    return false;
  }

  return diff > 0;
}

/// 부동 소수점 크기 비교를 위한 함수.
/// 왼쪽 값이 오른쪽 값보다 크거나 같으면 true를 반환함.
template <typename T, typename U>
[[nodiscard]] __forceinline bool IsGreaterOrEqual(T a, U b) noexcept {
  using CommonType = common_type_t<T, U>;

  const auto ca = static_cast<CommonType>(a);
  const auto cb = static_cast<CommonType>(b);

  // NaN 체크
  if (std::isnan(ca) | std::isnan(cb)) [[unlikely]] {
    return false;
  }

  // 절대적 오차
  constexpr CommonType abs_tolerance =
      numeric_limits<CommonType>::epsilon() * 100;
  constexpr CommonType rel_tolerance = CommonType(1e-12);

  const CommonType diff = ca - cb;
  const CommonType abs_diff = diff < 0 ? -diff : diff;

  // 같은 값인지 체크
  if (abs_diff <= abs_tolerance) {
    return true;
  }

  const CommonType abs_ca = ca < 0 ? -ca : ca;
  const CommonType abs_cb = cb < 0 ? -cb : cb;
  const CommonType max_val = abs_ca > abs_cb ? abs_ca : abs_cb;
  if (abs_diff <= rel_tolerance * max_val) {
    return true;
  }

  return diff > 0;
}

/// 부동 소수점 크기 비교를 위한 함수.
/// 왼쪽 값이 오른쪽 값보다 작으면 true를 반환함.
template <typename T, typename U>
[[nodiscard]] __forceinline bool IsLess(T a, U b) noexcept {
  using CommonType = common_type_t<T, U>;

  const auto ca = static_cast<CommonType>(a);
  const auto cb = static_cast<CommonType>(b);

  // NaN 체크
  if (std::isnan(ca) | std::isnan(cb)) [[unlikely]] {
    return false;
  }

  // 절대적 오차
  constexpr CommonType abs_tolerance =
      numeric_limits<CommonType>::epsilon() * 100;
  constexpr CommonType rel_tolerance = CommonType(1e-12);

  const CommonType diff = ca - cb;
  const CommonType abs_diff = diff < 0 ? -diff : diff;

  // 같은 값인지 체크
  if (abs_diff <= abs_tolerance) {
    return false;
  }

  const CommonType abs_ca = ca < 0 ? -ca : ca;
  const CommonType abs_cb = cb < 0 ? -cb : cb;
  const CommonType max_val = abs_ca > abs_cb ? abs_ca : abs_cb;
  if (abs_diff <= rel_tolerance * max_val) {
    return false;
  }

  return diff < 0;
}

/// 부동 소수점 크기 비교를 위한 함수.
/// 왼쪽 값이 오른쪽 값보다 작거나 같으면 true를 반환함.
template <typename T, typename U>
[[nodiscard]] __forceinline bool IsLessOrEqual(T a, U b) noexcept {
  using CommonType = common_type_t<T, U>;

  const auto ca = static_cast<CommonType>(a);
  const auto cb = static_cast<CommonType>(b);

  // NaN 체크
  if (std::isnan(ca) | std::isnan(cb)) [[unlikely]] {
    return false;
  }

  // 절대적 오차
  constexpr CommonType abs_tolerance =
      numeric_limits<CommonType>::epsilon() * 100;
  constexpr CommonType rel_tolerance = CommonType(1e-12);

  const CommonType diff = ca - cb;
  const CommonType abs_diff = diff < 0 ? -diff : diff;

  // 같은 값인지 체크
  if (abs_diff <= abs_tolerance) {
    return true;
  }

  const CommonType abs_ca = ca < 0 ? -ca : ca;
  const CommonType abs_cb = cb < 0 ? -cb : cb;
  const CommonType max_val = abs_ca > abs_cb ? abs_ca : abs_cb;
  if (abs_diff <= rel_tolerance * max_val) {
    return true;
  }

  return diff < 0;
}

}  // namespace backtesting::utils