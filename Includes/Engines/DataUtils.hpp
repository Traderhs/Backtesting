#pragma once

// 외부 라이브러리
#include <arrow/api.h>

#include <nlohmann/json.hpp>

// 네임스페이스
using namespace arrow;
using namespace nlohmann;
using namespace std;

/// 데이터 핸들링을 위한 유틸리티 네임스페이스
namespace data_utils {
/**
 * 주어진 값의 소수점 자릿수를 계산하는 함수
 *
 * @param value 소수 자릿수를 계산할 값
 * @return 주어진 double 값에 존재하는 소수점 자릿수의 개수
 */
size_t CountDecimalPlaces(double value);

/**
 * 주어진 값을 지정된 소수 자릿수로 반올림하여 반환하는 함수
 *
 * @param value 반올림할 값
 * @param decimal_places 값이 반올림될 소수점 자릿수
 * @return 주어진 값이 지정된 소수 자릿수로 반올림된 값
 */
double RoundToDecimalPlaces(double value, size_t decimal_places);

/**
 * 지정된 경로의 Parquet 파일을 읽고 테이블로 변환하여 반환하는 함수
 *
 * @param file_path 읽을 Parquet 파일의 경로
 * @return 변환된 테이블을 포함하는 shared_ptr 객체
 */
shared_ptr<Table> ReadParquet(const string& file_path);

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
any GetCellValue(const shared_ptr<Table>& table, const string& column_name,
                 int64_t row_index);

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
any GetCellValue(const shared_ptr<Table>& table, int column_index,
                 int64_t row_index);

/**
 * 주어진 Scalar 객체에서 값을 추출하여 반환하는 함수
 *
 * 반환받은 값은 any_cast로 사용
 *
 * @param scalar 값을 추출할 Scalar 객체의 shared_ptr
 * @return 주어진 Scalar의 실제 값
 */
any GetScalarValue(const shared_ptr<Scalar>& scalar);

/**
 * 주어진 테이블을 Parquet 파일 형식으로 지정된 파일 경로에 저장하는 함수
 *
 * @param table 저장할 데이터를 포함하는 Table 객체에 대한 shared_ptr
 * @param file_path 데이터를 저장할 Parquet 파일의 경로
 */
void TableToParquet(const shared_ptr<Table>& table, const string& file_path);

/// 1차원 벡터를 csv로 저장하는 함수.
/// 파일이 존재하는 경우 기존 내용은 초기화 됨.
void VectorToCsv(const vector<double>& data, const string& file_name);

/**
 * 주어진 테이블을 주어진 비율로 분할하여 두 개의 서브 테이블을 생성하여 반환하는 함수
 *
 * @param table 분할할 원본 테이블을 가리키는 shared_ptr
 * @param split_ratio 테이블을 나눌 비율을 나타내는 값 (0.0 - 1.0)
 * @return 주어진 테이블을 `split_ratio`에 따라 나눈 두 개의 서브 테이블을 포함하는 pair 객체.
 *         첫 번째는 `split_ratio` 비율, 두 번째는 나머지 비율
 */
pair<shared_ptr<Table>, shared_ptr<Table>> SplitTable(const shared_ptr<Table>& table, double split_ratio);

/// 최소 틱 크기로 가격을 반올림하여 반환하는 함수
double RoundToTickSize(double price, double tick_size);

// FormatDollar에서 사용하는 공통 Locale 설정
static locale global_locale("en_US.UTF-8");

/// 금액을 천 단위 쉼표와 달러 표기로 포맷하여 반환하는 함수
string FormatDollar(double price);
}