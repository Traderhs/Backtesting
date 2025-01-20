#pragma once

// 표준 라이브러리
#include <stdexcept>
#include <string>

// 네임 스페이스
using namespace std;

/// 지표 값 참조 시 인덱스가 범위를 벗어났을 때 발생하는 에러
class IndicatorOutOfRange final : public runtime_error {
 public:
  explicit IndicatorOutOfRange(const string& message)
      : runtime_error(message) {}
};

/// 지표 값 참조 시 nan이면 발생하는 에러
class IndicatorInvalidValue final : public runtime_error {
 public:
  explicit IndicatorInvalidValue(const string& message)
      : runtime_error(message) {}
};

class Bankruptcy final : public runtime_error {
  public:
  explicit Bankruptcy(const string& message)
    : runtime_error(message) {}
};
