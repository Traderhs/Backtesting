#pragma once

// 표준 라이브러리
#include <mutex>

// 내부 헤더
#include "BaseBarHandler.hpp"

// 전방 선언
namespace arrow {
class Table;
}

namespace backtesting::bar {

/// 바 데이터를 추가하고 세부 관리 및 처리를 하는 클래스
class BarHandler final : public BaseBarHandler {
 public:
  // 싱글톤 특성 유지
  BarHandler(const BarHandler&) = delete;             // 복사 생성자 삭제
  BarHandler& operator=(const BarHandler&) = delete;  // 대입 연산자 삭제

  /// BarHandler의 싱글톤 인스턴스를 반환하는 함수
  static shared_ptr<BarHandler>& GetBarHandler();

  /// 주어진 파일 경로에서 Parquet 데이터를 읽고
  /// 지정된 바 타입으로 처리하여 핸들러에 추가하는 함수
  ///
  /// @param symbol_names 심볼 이름들
  /// @param file_paths 각 심볼에 대응하는 Parquet 파일 경로들
  /// @param bar_type 추가할 데이터의 바 타입
  /// @param open_time_column Open Time 컬럼 인덱스
  /// @param open_column Open 컬럼 인덱스
  /// @param high_column High 컬럼 인덱스
  /// @param low_column Low 컬럼 인덱스
  /// @param close_column Close 컬럼 인덱스
  /// @param volume_column Volume 컬럼 인덱스
  /// @param close_time_column Close Time 컬럼 인덱스
  void AddBarData(const vector<string>& symbol_names,
                  const vector<string>& file_paths, BarType bar_type,
                  int open_time_column, int open_column, int high_column,
                  int low_column, int close_column, int volume_column,
                  int close_time_column);

  // ===========================================================================
  /// 지정된 바 데이터 및 심볼에 해당되는 인덱스를 target_close_time 시점의
  /// 인덱스까지 최대한 진행시키는 함수
  void ProcessBarIndex(BarType bar_type, const string& timeframe,
                       int symbol_idx, int64_t target_close_time);

  /// 지정된 바 데이터의 모든 심볼의 인덱스를 target_close_time 시점의
  /// 인덱스까지 진행시키는 함수
  void ProcessBarIndices(BarType bar_type, const string& timeframe,
                         int64_t target_close_time);
  // ===========================================================================
  /// 현재 사용 중인 바의 타입을 설정하는 함수.
  /// 타임프레임은 참조 바 사용 시에만 지정.
  void SetCurrentBarType(BarType bar_type, const string& timeframe);

  /// 현재 사용 중인 심볼의 인덱스를 설정하는 함수
  void SetCurrentSymbolIndex(int symbol_index);

  /// 현재 사용 중인 바 데이터 타입 및 심볼과 타임프레임에 해당되는 바 데이터의
  /// 현재 인덱스를 설정하는 함수
  void SetCurrentBarIndex(size_t bar_index);

  /// 지정된 바 데이터 타입 및 심볼에 해당되는 바 데이터의
  /// 인덱스를 하나 증가시키고 증가한 인덱스를 반환하는 함수
  size_t IncreaseBarIndex(BarType bar_type, const string& timeframe,
                          int symbol_index);

  // ===========================================================================
  /// 현재 사용 중인 바의 타입을 반환하는 함수
  [[nodiscard]] BarType GetCurrentBarType() const;

  /// 현재 참조 바 데이터에서 사용 중인 타임프레임을 반환하는 함수
  [[nodiscard]] string GetCurrentReferenceTimeframe() const;

  /// 현재 사용 중인 심볼의 인덱스를 반환하는 함수
  [[nodiscard]] int GetCurrentSymbolIndex() const;

  /// 현재 사용 중인 바 데이터 타입 및 심볼과 타임프레임에 해당되는 바 데이터의
  /// 현재 인덱스를 반환하는 함수
  [[nodiscard]] size_t GetCurrentBarIndex();

 private:
  // 싱글톤 인스턴스 관리
  BarHandler();
  class Deleter {
   public:
    void operator()(const BarHandler* p) const;
  };

  static mutex mutex_;
  static shared_ptr<BarHandler> instance_;

  /// 현재 사용 중인 바의 타입: TRADING, MAGNIFIER, REFERENCE, MARK
  BarType current_bar_type_;

  /// 현재 사용 중인 심볼의 인덱스
  int current_symbol_index_;

  /// 참조 바 데이터 사용 시 사용 중인 타임프레임
  string current_reference_timeframe_;

  /**
   * 주어진 데이터에서 Open Time과 다음 Open Time의 시간 차이를 계산하여
   * 타임프레임을 문자열로 반환하는 함수.
   *
   * 데이터 누락 시 부정확한 값이 계산될 수 있으므로 앞에서 10개, 뒤에서 10개의
   * 데이터를 비교 후 최빈값으로 반환.
   *
   * @param bar_data 바 데이터가 포함된 `Table` 객체를 가리키는 shared_ptr
   * @param open_time_column Open Time이 포함된 열의 인덱스
   * @return 타임프레임 문자열
   */
  [[nodiscard]] static string CalculateTimeframe(
      const shared_ptr<arrow::Table>& bar_data, int open_time_column);

  /// 바 데이터 타입간 타임프레임이 유효한지 검증하는 함수
  void IsValidTimeframeBetweenBars(const string& timeframe, BarType bar_type);

  /// 지정된 타임프레임이 레퍼런스 바에 존재하는지 검증하는 함수
  void IsValidReferenceBarTimeframe(const string& timeframe);
};

}  // namespace backtesting::bar
