#pragma once

// 내부 헤더
#include "Engines/BarHandler.hpp"
#include "Engines/Engine.hpp"
#include "Engines/Logger.hpp"

// 네임 스페이스
using namespace std;

/**
 * 백테스팅 시 사용하는 지표를 생성하기 위한 가상 클래스
 */
class Indicator {
 public:
  explicit Indicator(string name, string timeframe);
  virtual ~Indicator();

  /// 매 봉마다 지표를 계산하는 함수
  virtual double Calculate() = 0;

  /// 값을 반환하는 연산자 오버로딩
  /// 사용법: 지표 클래스 객체[인덱스]
  [[nodiscard]] double operator[](size_t index);

 protected:
  static BarHandler& bar_;
  static Logger& logger_;

  /// 모든 심볼의 모든 봉의 지표를 계산하는 함수.
  /// ※ 중요: 상속받은 지표의 생성자에서 호출해야 함
  void CalculateAll();

  /// 지표의 파라미터를 설정하는 함수.
  /// ※ 중요: 상속받은 지표의 생성자에서 호출해야 함
  void SetInput(const vector<double>& input);

  /// 지표의 파라미터를 반환하는 함수
  vector<double> GetInput();

  /// 지표의 타임프레임을 반환하는 함수
  [[nodiscard]] string GetTimeframe() const;

 private:
  string name_;  // 지표의 이름
  string timeframe_;  // 지표의 타임프레임
  vector<double> input_;  // 지표의 파라미터
  unordered_map<string, vector<double>> output_;  // 지표의 계산된 값: <심볼, 값>
};

