#pragma once

// 표준 라이브러리
#include <stdexcept>
#include <string>

// 네임 스페이스
using namespace std;

namespace backtesting::exception {

/// 유효하지 않은 값일 때 발생하는 에러
class InvalidValue final : public runtime_error {
 public:
  explicit InvalidValue(const string& message) : runtime_error(message) {}
};

/// 인덱스가 범위를 벗어났을 때 발생하는 에러
class IndexOutOfRange final : public runtime_error {
 public:
  explicit IndexOutOfRange(const string& message) : runtime_error(message) {}
};

/// 진입 가능 자금이 부족할 때 발생하는 에러
class InsufficientBalance final : public runtime_error {
 public:
  explicit InsufficientBalance(const string& message)
      : runtime_error(message) {}
};

/// 주문 실패 시 발생하는 에러
class OrderFailed final : public runtime_error {
 public:
  explicit OrderFailed(const string& message) : runtime_error(message) {}
};

/// 파산 시 발생하는 에러
class Bankruptcy final : public runtime_error {
 public:
  explicit Bankruptcy(const string& message) : runtime_error(message) {}
};

}  // namespace backtesting::exception
