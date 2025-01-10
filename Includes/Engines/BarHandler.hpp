#pragma once

// 표준 라이브러리
#include <mutex>

// 전방 선언
namespace arrow {
class Table;
}

// 내부 헤더
#include "Engines\BaseBarHandler.hpp"

// 네임 스페이스
using namespace arrow;

/// 바 데이터를 추가하고 세부 관리 및 처리를 하는 클래스
class BarHandler final : public BaseBarHandler {
 public:
  // 싱글톤 특성 유지
  BarHandler(const BarHandler&) = delete;             // 복사 생성자 삭제
  BarHandler& operator=(const BarHandler&) = delete;  // 대입 연산자 삭제

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
  /// 현재 사용 중인 바의 타입을 설정하는 함수.
  /// 타임프레임은 참조 바 사용 시에만 지정하면 됨.
  void SetCurrentBarType(BarType bar_type, const string& timeframe);

  /// 현재 사용 중인 심볼의 인덱스를 설정하는 함수
  void SetCurrentSymbolIndex(int symbol_index);

  /// 현재 사용 중인 바 데이터 타입 및 심볼과 타임프레임에 해당되는 바 데이터의
  /// 현재 인덱스를 설정하는 함수
  void SetCurrentBarIndex(size_t bar_index);

  // ===========================================================================
  /// 현재 사용 중인 바의 타입을 반환하는 함수
  [[nodiscard]] BarType GetCurrentBarType() const;

  /// 현재 사용 중인 심볼의 인덱스를 반환하는 함수
  [[nodiscard]] int GetCurrentSymbolIndex() const;

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

  /// 바 데이터 타입간 타임프레임이 유효한지 검사하는 함수
  void IsValidTimeframeBetweenBars(const string& timeframe, BarType bar_type);
};
