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
using namespace arrow;
using namespace std;

/// 하나의 바 구조를 지정하는 구조체
struct Bar {
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
  /// @param bar_data 테이블에 저장된 바 데이터
  /// @param columns 테이블 내 컬럼의 인덱스를 다음 순서로 지정
  ///                [Open Time, Open, High, Low, Close, Volume, Close Time]
  void SetBarData(const string& symbol_name, const string& timeframe,
                  const shared_ptr<Table>& bar_data,
                  const vector<int>& columns);

  /// 심볼과 바 인덱스의 범위 검사 후 해당되는 바를 반환하는 함수.
  [[nodiscard]] Bar SafeGetBar(int symbol_idx, size_t bar_idx) const;

  /// 심볼과 바 인덱스에 해당되는 바를 반환하는 함수
  [[nodiscard]] Bar GetBar(int symbol_idx, size_t bar_idx) const;

  /// 심볼 인덱스에 해당하는 심볼의 이름을 반환하는 함수
  [[nodiscard]] string GetSymbolName(int symbol_idx) const;

  /// 바 데이터에 추가된 심볼의 개수를 반환하는 함수
  [[nodiscard]] int GetNumSymbols() const;

  /// 해당되는 심볼의 바 개수를 반환하는 함수
  [[nodiscard]] size_t GetNumBars(int symbol_idx) const;

  /// 바 데이터의 타임프레임을 반환하는 함수
  [[nodiscard]] string GetTimeframe() const;

  /// 인덱스 유효성을 검증하는 함수
  void IsValidIndex(int symbol_idx, size_t bar_idx) const;

  /// 심볼 인덱스 유효성을 검증하는 함수
  void IsValidSymbolIndex(int symbol_idx) const;

 private:
  // 첫 번째 벡터: 심볼 인덱스, 두 번째 벡터: 해당 벡터의 바 인덱스
  vector<vector<Bar>> bar_data_;

  vector<string> symbol_names_;  // 심볼 인덱스에 해당하는 심볼의 이름
  int num_symbols_;              // 심볼의 개수
  vector<size_t> num_bars_;      // 해당 심볼의 바 개수
  string timeframe_;             // 바 데이터의 타임프레임

  // 심볼 설정의 유효성 검사
  void IsValidSettings(const string& symbol_name, const string& timeframe,
                       const shared_ptr<Table>& bar_data,
                       const vector<int>& columns) const;
};
