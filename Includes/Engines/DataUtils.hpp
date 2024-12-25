#pragma once

// 표준 라이브러리
#include <deque>

// 외부 라이브러리
#include <arrow/api.h>

#include <nlohmann/json.hpp>

// 네임스페이스
using namespace arrow;
using namespace nlohmann;
using namespace std;

/**
 * 데이터 핸들링을 위한 유틸리티 네임스페이스
 */
namespace DataUtils {
/**
 * 주어진 double 값의 소수 자릿수를 계산합니다.
 *
 * @param value 소수 자릿수를 계산할 double 값입니다.
 * @return 주어진 double 값에 존재하는 소수 자릿수의 개수를 반환합니다.
 */
size_t CountDecimalPlaces(double value);

/**
 * 주어진 JSON 배열에서 특정 인덱스의 값들에 대해 가장 많은 소수 자릿수를
 * 찾습니다.
 *
 * @param data 소수 자릿수를 계산할 JSON 객체들의 deque입니다.
 * @param index 소수 자릿수를 계산할 값이 위치한 각 Json의 인덱스입니다.
 * @return 주어진 인덱스 위치에서 JSON 데이터들 중 가장 많은 소수 자릿수를
 * 반환합니다.
 */
size_t GetMaxDecimalPlaces(const deque<json>& data, size_t index);

/**
 * 주어진 double 값을 지정된 소수 자릿수로 반올림합니다.
 *
 * @param value 반올림할 double 값입니다.
 * @param decimal_places 값이 반올림될 소수 자릿수입니다.
 * @return 주어진 double 값이 지정된 소수 자릿수로 반올림된 값을 반환합니다.
 */
double RoundToDecimalPlaces(double value, size_t decimal_places);

/**
 * 지정된 경로의 Parquet 파일을 읽고 테이블로 변환합니다.
 *
 * @param file_path 읽을 Parquet 파일의 경로를 나타내는 문자열입니다.
 * @return 변환된 테이블을 포함하는 shared_ptr 객체를 반환합니다.
 */
shared_ptr<Table> ReadParquet(const string& file_path);

/**
 * 지정된 테이블에서 주어진 열 이름과 행 인덱스에 해당하는 셀의 값을 반환합니다.
 *
 * 리턴받은 값은 any_cast가 필요합니다.
 *
 * @param table 데이터를 포함하는 테이블의 공유 포인터입니다.
 * @param column_name 값을 검색할 열의 이름입니다.
 * @param row_index 값을 검색할 행의 인덱스입니다.
 * @return 지정된 열과 행의 인덱스에 해당하는 셀의 값을 반환합니다.
 */
any GetCellValue(const shared_ptr<Table>& table, const string& column_name,
                 int64_t row_index);

/**
 * 지정된 테이블에서 주어진 열 인덱스와 행 인덱스에 해당하는 셀의 값을 반환합니다.
 *
 * 리턴받은 값은 any_cast가 필요합니다.
 *
 * @param table 데이터를 포함하는 테이블의 공유 포인터입니다.
 * @param column_index 값을 검색할 열의 인덱스입니다.
 * @param row_index 값을 검색할 행의 인덱스입니다.
 * @return 지정된 열과 행의 인덱스에 해당하는 셀의 값을 반환합니다.
 */
any GetCellValue(const shared_ptr<Table>& table, int column_index,
                 int64_t row_index);

/**
 * 주어진 Scalar 객체에서 값을 추출하여 반환합니다.
 *
 * 반환받은 값은 any_cast로 사용합니다.
 *
 * @param scalar 값을 추출할 Scalar 객체의 shared_ptr입니다.
 * @return 주어진 Scalar의 실제 값이 any 타입으로 반환됩니다.
 *         추출된 값은 Scalar의 타입에 따라 정수, 실수, 문자열일 수 있습니다.
 */
any GetScalarValue(const shared_ptr<Scalar>& scalar);

/**
 * 주어진 테이블을 Parquet 파일 형식으로 지정된 파일 경로에 저장합니다.
 *
 * @param table 저장할 데이터를 포함하는 Table 객체에 대한 shared_ptr입니다.
 * @param file_path 데이터를 저장할 Parquet 파일의 경로를 나타내는 문자열입니다.
 */
void TableToParquet(const shared_ptr<Table>& table, const string& file_path);

/**
 * 주어진 테이블을 주어진 비율로 분할하여 두 개의 서브 테이블을 생성합니다.
 *
 * 이 함수는 입력 테이블을 `split_ratio`에 따라 두 부분으로 나눕니다.
 * 첫 번째 테이블은 입력 테이블의 `split_ratio` 비율에 해당하는 부분이며,
 * 두 번째 테이블은 나머지 부분을 포함합니다.
 *
 * 반환되는 두 테이블은 입력 테이블의 스키마와 동일하며, 각 컬럼의 데이터는
 * 적절한 슬라이스(Slice)를 통해 나누어집니다.
 *
 * @param table 분할할 원본 테이블을 가리키는 shared_ptr입니다.
 * @param split_ratio 테이블을 나눌 비율을 나타내는 double 값입니다. (0.0과 1.0 사이의 값)
 * @return 주어진 테이블을 `split_ratio`에 따라 나눈 두 개의 서브 테이블을 포함하는 pair 객체입니다.
 *         반환되는 두 테이블은 첫 번째는 `split_ratio` 비율, 두 번째는 나머지 비율을 가집니다.
 */
pair<shared_ptr<Table>, shared_ptr<Table>> SplitTable(const shared_ptr<Table>& table, double split_ratio);
}