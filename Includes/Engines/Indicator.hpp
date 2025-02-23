#pragma once

// 표준 라이브러리
#include <memory>
#include <vector>

// 전방 선언
class Analyzer;
class BarHandler;

// 내부 헤더
#include "Engines/Engine.hpp"
#include "Engines/Logger.hpp"
#include "Engines/Numeric.hpp"

// 네임 스페이스
using namespace std;

/// 전략 구현 시 사용하는 지표를 생성하기 위한 가상 클래스
class Indicator {
 public:
  // 커스텀 지표에서 실수로 이름과 타임프레임을 미전달하는 경우를 방지하기 위함
  Indicator() = delete;

  // Factory 메서드 추가
  template <typename CustomIndicator, typename... Args>
  static CustomIndicator Create(const string& name, const string& timeframe,
                                Args&&... args) {
    CustomIndicator indicator(name, timeframe, std::forward<Args>(args)...);
    Engine::GetEngine()->AddIndicator(indicator);
    return indicator;
  }

  /// 지표의 계산된 값을 반환하는 연산자 오버로딩.
  /// 사용법: 지표 클래스 객체[n개 바 전 인덱스]
  [[nodiscard]] Numeric<double> operator[](size_t index);

  /// 모든 심볼의 모든 바에 해당되는 지표 값을 계산하는 함수.
  void CalculateIndicator();

  /// 계산된 지표값을 csv로 저장하는 함수
  void SaveIndicator() const;

  /// 지표의 이름을 반환하는 함수
  [[nodiscard]] string GetName() const;

  /// 지표의 타임프레임을 반환하는 함수
  [[nodiscard]] string GetTimeframe() const;

 protected:
  Indicator(const string& name, const string& timeframe);
  virtual ~Indicator();

  static shared_ptr<Analyzer>& analyzer_;
  static shared_ptr<BarHandler>& bar_;
  static shared_ptr<Engine>& engine_;
  static shared_ptr<Logger>& logger_;

  /// 커스텀 지표들의 멤버 변수들을 초기화하는 함수.
  /// 심볼별 지표 계산 시 각 심볼에서 멤버 변수들을 초기 상태로 복구하여
  /// 초기부터 계산할 수 있게 해야 함.
  virtual void Initialize() = 0;

  /// 각 바에서 지표를 계산하는 함수. 메인 로직을 작성.
  virtual Numeric<double> Calculate() = 0;

 private:
  string name_;                             // 지표의 이름
  string timeframe_;                        // 지표의 타임프레임
  vector<double> input_;                    // 지표의 파라미터
  vector<vector<Numeric<double>>> output_;  // 지표의 계산된 값: 심볼<값>
  bool is_calculated_;  // 지표가 계산되었는지 확인하는 플래그

  // 지표가 현재 계산 중인지 확인하는 플래그 ->
  // 지표 계산 시 사용하는 다른 지표가 계산하는 지표와 다른 타임프레임을 가질 수
  // 없게 검사할 때 사용
  static bool is_calculating_;

  // 계산 중인 지표의 이름을 저장 (로그용)
  static string calculating_name_;
};
