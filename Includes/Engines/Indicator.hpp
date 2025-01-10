#pragma once

// 표준 라이브러리
#include <vector>

// 전방 선언
class BarHandler;
class Engine;

// 내부 헤더
#include "Engines\Logger.hpp"

// 네임 스페이스
using namespace std;

/// 전략 구현 시 사용하는 지표를 생성하기 위한 가상 클래스
class Indicator {
 public:
  /// 지표의 계산된 값을 반환하는 연산자 오버로딩.
  /// 사용법: 지표 클래스 객체[인덱스] => 0은 현재 바 값, 1은 1개 바 전 값, 2는 2개 바 전 값 ...
  [[nodiscard]] double operator[](size_t index);

 protected:
  explicit Indicator(string name, string timeframe);
  virtual ~Indicator();

  static shared_ptr<BarHandler>& bar_;
  static shared_ptr<Engine>& engine_;
  static shared_ptr<Logger>& logger_;

  /// 매 봉마다 지표를 계산하는 함수. 메인 로직을 작성.
  virtual double Calculate() = 0;

  /// 모든 심볼의 모든 바에 해당되는 지표 값을 계산하는 함수.
  /// ※ 주의: 상속받은 커스텀 지표의 생성자에서 호출해야 함.
  void CalculateAll();

  /// 지표의 파라미터를 설정하는 함수. 파라미터 성과 최적화를 위해 사용.
  /// ※ 주의: 상속받은 커스텀 지표의 생성자에서 호출해야 함.
  void SetInput(const vector<double>& input);

  /// 지표의 파라미터를 반환하는 함수
  vector<double> GetInput();

  /// 지표의 타임프레임을 반환하는 함수
  [[nodiscard]] string GetTimeframe() const;

 private:
  string name_;  // 지표의 이름
  string timeframe_;  // 지표의 타임프레임
  vector<double> input_;  // 지표의 파라미터
  vector<vector<double>> output_;  // 지표의 계산된 값: 심볼<값>
  bool is_calculated_;  // 지표가 계산되었는지 확인하는 플래그
};

