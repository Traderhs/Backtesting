#pragma once

// 표준 라이브러리
#include <mutex>

// 전방 선언
namespace arrow {
class Table;
}

// 내부 헤더
#include "Engines/BaseBarHandler.hpp"

// 네임 스페이스
using namespace arrow;

/// 바 데이터를 추가하고 세부 관리 및 처리를 하는 클래스
class BarHandler final : public BaseBarHandler {
 public:
  // 싱글톤 특성 유지
  BarHandler(const BarHandler&) = delete;             // 복사 생성자 삭제
  BarHandler& operator=(const BarHandler&) = delete;  // 대입 연산자 삭제

  /**
   * 전략이 하나라도 생성되었는지 확인하는 플래그.
   * 전략 생성 전 트레이딩 바를 모두 추가해야 전략에서 정상적으로 지표가
   * 계산되기 때문에 이 플래그가 필요
   */
  bool is_strategy_created_;

  /// BarHandler의 싱글톤 인스턴스를 반환하는 함수
  static shared_ptr<BarHandler>& GetBarHandler();

  /**
   * 주어진 파일 경로에서 Parquet 데이터를 읽고
   * 지정된 바 타입으로 처리하여 핸들러에 추가하는 함수
   *
   * @param symbol_name 심볼 이름
   * @param file_path Parquet 파일의 경로
   * @param bar_type 추가할 데이터의 바 타입
   * @param columns 파일에서 데이터를 추출할 컬럼의 인덱스를 다음 순서로 지정
   *                [Open Time, Open, High, Low, Close, Volume, Close Time]
   */
  void AddBarData(const string& symbol_name, const string& file_path,
                  BarType bar_type, const vector<int>& columns);

  // ===========================================================================
  /// 지정된 바 데이터 및 심볼에 해당되는 인덱스를 base_close_time 시점의
  /// 인덱스까지 진행시키는 함수
  void ProcessBarIndex(int symbol_idx, BarType bar_type,
                       const string& timeframe, int64_t base_close_time);

  /// 지정된 바 데이터의 모든 심볼의 인덱스를 base_close_time 시점의
  /// 인덱스까지 진행시키는 함수
  void ProcessBarIndices(BarType bar_type, const string& timeframe,
                         int64_t base_close_time);
  // ===========================================================================
  /// 현재 사용 중인 바의 타입을 설정하는 함수.
  /// 타임프레임은 참조 바 사용 시에만 지정.
  /// ※ 주의: 함수 내에서 사용할 때 함수 종료 시 원상복구해야 함
  void SetCurrentBarType(BarType bar_type, const string& timeframe);

  /// 현재 사용 중인 심볼의 인덱스를 설정하는 함수
  /// ※ 주의: 함수 내에서 사용할 때 함수 종료 시 원상복구해야 함
  void SetCurrentSymbolIndex(int symbol_index);

  /// 현재 사용 중인 바 데이터 타입 및 심볼과 타임프레임에 해당되는 바 데이터의
  /// 현재 인덱스를 설정하는 함수
  void SetCurrentBarIndex(size_t bar_index);

  /// 지정된 바 데이터 타입 및 심볼에 해당되는 바 데이터의
  /// 인덱스를 하나 증가시키는 함수
  void IncrementBarIndex(BarType bar_type, const string& timeframe,
                         int symbol_index);

  // ===========================================================================
  /// 현재 사용 중인 바의 타입을 반환하는 함수
  [[nodiscard]] inline BarType GetCurrentBarType() const;

  /// 현재 참조 바 데이터에서 사용 중인 타임프레임을 반환하는 함수
  [[nodiscard]] inline string GetCurrentReferenceTimeframe() const;

  /// 현재 사용 중인 심볼의 인덱스를 반환하는 함수
  [[nodiscard]] inline int GetCurrentSymbolIndex() const;

  /// 현재 사용 중인 바 데이터 타입 및 심볼과 타임프레임에 해당되는 바 데이터의
  /// 현재 인덱스를 반환하는 함수
  size_t GetCurrentBarIndex();

 private:
  // 싱글톤 인스턴스 관리
  BarHandler();
  class Deleter {
   public:
    void operator()(const BarHandler* p) const;
  };

  static mutex mutex_;
  static shared_ptr<BarHandler> instance_;

  /// 현재 사용 중인 바의 타입: TRADING, MAGNIFIER, REFERENCE
  BarType current_bar_type_;

  /// 현재 사용 중인 심볼의 인덱스
  int current_symbol_index_;

  /// 참조 바 데이터 사용 시 사용 중인 타임프레임
  string current_reference_timeframe_;

  /**
   * 주어진 데이터에서 첫 Open Time과 다음 Open Time의 시간 차이를 계산하여 타임프레임을
   * 문자열로 반환하는 함수
   *
   * @param bar_data 바 데이터가 포함된 `Table` 객체를 가리키는 shared_ptr
   * @param open_time_column Open Time이 포함된 열의 인덱스
   * @return 첫 번째 Open Time과 두 번째 Open Time의 차이를 포맷한 타임프레임 문자열
   */
  static string CalculateTimeframe(const shared_ptr<Table>& bar_data, int open_time_column);

  /**
   * 트레이딩 바 데이터가 추가 가능한지 검증하는 함수.
   * 전략 생성 시 OHLCV 지표 계산으로 인해, 미리 지표들의 output_을
   * resize해야 하는데, 이때 트레이딩 바 데이터를 이용하므로 전략 추가 이후엔
   * 트레이딩 바 데이터 추가가 불가능해짐.
   */
  void IsTradingBarAddable() const;

  /// 바 데이터 타입간 타임프레임이 유효한지 검증하는 함수
  void IsValidTimeframeBetweenBars(const string& timeframe, BarType bar_type);

  /// 지정된 타임프레임이 레퍼런스 바에 존재하는지 검증하는 함수
  void IsValidReferenceBarTimeframe(const string& timeframe);
};
