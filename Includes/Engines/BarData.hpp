#pragma once

// 표준 라이브러리
#include <string>
#include <vector>

// 외부 라이브러리
#include <arrow/api.h>
#include <arrow/table.h>

// 네임 스페이스
using namespace arrow;
using namespace std;

/// 심볼별로 바 데이터를 시계열 순서대로 벡터화하여 저장하는 클래스
class BarData final {
 public:
  BarData();
  ~BarData();

  /// 한 심볼 테이블에 저장된 값을 Vector에 저장하는 함수
  /// @param name 심볼 이름
  /// @param timeframe 심볼 타임프레임
  /// @param bar_data 테이블에 저장된 바 데이터
  /// @param columns 테이블 내 컬럼의 인덱스를 다음 순서로 지정
  ///                [Open Time, Open, High, Low, Close, Volume, Close Time]
  void SetBarData(const string& name, const string& timeframe,
                  const shared_ptr<Table>& bar_data,
                  const vector<int>& columns);

  /// 심볼의 바 인덱스와 값 타입에 따라 적절한 값을 반환하는 함수.
  /// 오버헤드 감소를 통해 속도를 최적화하므로 함수를 분할
  /// @param symbol_idx 심볼의 인덱스
  /// @param bar_idx 바의 인덱스
  [[nodiscard]] int64_t GetOpenTime(int symbol_idx, size_t bar_idx) const;
  [[nodiscard]] double GetOpen(int symbol_idx, size_t bar_idx) const;
  [[nodiscard]] double GetHigh(int symbol_idx, size_t bar_idx) const;
  [[nodiscard]] double GetLow(int symbol_idx, size_t bar_idx) const;
  [[nodiscard]] double GetClose(int symbol_idx, size_t bar_idx) const;
  [[nodiscard]] double GetVolume(int symbol_idx, size_t bar_idx) const;
  [[nodiscard]] int64_t GetCloseTime(int symbol_idx, size_t bar_idx) const;

  /// 심볼 인덱스에 해당하는 심볼의 이름을 반환하는 함수
  [[nodiscard]] string GetSymbolName(int symbol_idx) const;

 private:
  vector<vector<int64_t>> open_time_;   // symbols<open_time>
  vector<vector<double>> open_;         // symbols<open>
  vector<vector<double>> high_;         // symbols<high>
  vector<vector<double>> low_;          // symbols<low>
  vector<vector<double>> close_;        // symbols<close>
  vector<vector<double>> volume_;       // symbols<volume>
  vector<vector<int64_t>> close_time_;  // symbols<close_time>

  vector<string> symbol_names_;  // 심볼 인덱스별 심볼 이름
  size_t num_symbols_;           // 심볼의 개수
  string timeframe_;             // 바 데이터의 타임프레임

  // 심볼 설정의 유효성 검사
  void IsValidSettings(const string& name, const string& timeframe,
                       const shared_ptr<Table>& bar_data,
                       const vector<int>& columns) const;

  // 인덱스 유효성 검사
  void IsValidIndex(int symbol_idx, size_t bar_idx) const;
};
