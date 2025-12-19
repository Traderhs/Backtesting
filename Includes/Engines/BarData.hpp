#pragma once

// 표준 라이브러리
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// 전방 선언
namespace arrow {
class Table;
}

// 네임 스페이스
using namespace std;

namespace backtesting::bar {

/// 하나의 바 구조를 지정하는 구조체
struct Bar {
  Bar() = default;  // 명시적 초기화용
  Bar(const int64_t open_time, const double open, const double high,
      const double low, const double close, const double volume,
      const int64_t close_time) {
    this->open_time = open_time;
    this->open = open;
    this->high = high;
    this->low = low;
    this->close = close;
    this->volume = volume;
    this->close_time = close_time;
  }

  int64_t open_time;
  double open;
  double high;
  double low;
  double close;
  double volume;
  int64_t close_time;
};

/// 바 데이터를 심볼별 시계열 순서대로 벡터화하여 저장하는 클래스
class BarData final {
 public:
  BarData();
  ~BarData();

  /// 한 심볼 테이블에 저장된 값을 Vector에 저장하는 함수
  /// @param symbol_name 심볼 이름
  /// @param timeframe 심볼 타임프레임
  /// @param file_path 바 데이터 경로 (Config 저장용)
  /// @param bar_data 테이블에 저장된 바 데이터
  /// @param open_time_column Open Time 컬럼 인덱스
  /// @param open_column Open 컬럼 인덱스
  /// @param high_column High 컬럼 인덱스
  /// @param low_column Low 컬럼 인덱스
  /// @param close_column Close 컬럼 인덱스
  /// @param volume_column Volume 컬럼 인덱스
  /// @param close_time_column Close Time 컬럼 인덱스
  void SetBarData(const string& symbol_name, const string& timeframe,
                  const string& file_path,
                  const shared_ptr<arrow::Table>& bar_data,
                  int open_time_column, int open_column, int high_column,
                  int low_column, int close_column, int volume_column,
                  int close_time_column);

  /// 심볼과 바 인덱스의 범위 검사 후 해당되는 바를 반환하는 함수
  [[nodiscard]] Bar& SafeGetBar(int symbol_idx, size_t bar_idx);

  /// 심볼 인덱스와 바 인덱스에 해당되는 바를 반환하는 함수
  [[nodiscard]] Bar& GetBar(int symbol_idx, size_t bar_idx);

  /// 심볼 인덱스에 해당되는 바 데이터 경로를 반환하는 함수
  [[nodiscard]] string GetBarDataPath(int symbol_idx) const;

  /// 심볼 인덱스에 해당하는 심볼의 이름을 반환하는 함수
  [[nodiscard]] string& GetSafeSymbolName(int symbol_idx);

  /// 바 데이터에 추가된 심볼의 개수를 반환하는 함수
  [[nodiscard]] int GetNumSymbols() const;

  /// 해당되는 심볼 인덱스의 범위 검사 후 바 개수를 반환하는 함수
  [[nodiscard]] size_t GetSafeNumBars(int symbol_idx) const;

  /// 해당되는 심볼의 바 개수를 반환하는 함수
  [[nodiscard]] size_t GetNumBars(int symbol_idx) const;

  /// 바 데이터의 타임프레임을 반환하는 함수
  [[nodiscard]] string& GetTimeframe();

  /// 인덱스 유효성을 검증하는 함수
  void IsValidIndex(int symbol_idx, size_t bar_idx) const;

  /// 심볼 인덱스 유효성을 검증하는 함수
  void IsValidSymbolIndex(int symbol_idx) const;

  /// 바 인덱스 유효성을 검증하는 함수
  void IsValidBarIndex(int symbol_idx, size_t bar_idx) const;

 private:
  // 첫 번째 벡터: 심볼 인덱스, 두 번째 벡터: 해당 벡터의 바 인덱스
  vector<vector<Bar>> bar_data_;

  // 설정에서 경로 저장용
  vector<string> bar_data_path_;

  vector<string> symbol_names_;  // 심볼 인덱스에 해당하는 심볼의 이름
  int num_symbols_;              // 심볼의 개수
  vector<size_t> num_bars_;      // 심볼 인덱스에 해당하는 심볼의 바 개수
  string timeframe_;             // 바 데이터의 타임프레임

  // 심볼 설정의 유효성 검사
  void IsValidSettings(const string& symbol_name, const string& timeframe,
                       const shared_ptr<arrow::Table>& bar_data,
                       int open_time_column, int open_column, int high_column,
                       int low_column, int close_column, int volume_column,
                       int close_time_column) const;
};

}  // namespace backtesting::bar