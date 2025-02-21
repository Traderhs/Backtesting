#pragma once

// 내부 헤더
#include "Engines/DataUtils.hpp"

// 네임 스페이스
using namespace std;

/// 부동소숫점 오류를 방지하기 위한 연산자를 지원하는 템플릿 숫자 클래스
template <typename T>
class Numeric final {
 public:
  // 생성자
  // ReSharper disable once CppNonExplicitConvertingConstructor
  Numeric(T v);  // NOLINT(*-explicit-constructor)

  // ReSharper disable once CppNonExplicitConversionOperator
  operator T() const;  // NOLINT(*-explicit-constructor)

  // Numeric 간 산술 연산자
  Numeric operator+(const Numeric& other) const;
  Numeric operator-(const Numeric& other) const;
  Numeric operator*(const Numeric& other) const;
  Numeric operator/(const Numeric& other) const;
  Numeric operator+=(const Numeric& other) const;
  Numeric operator-=(const Numeric& other) const;

  // Numeric 간 비교 연산자
  bool operator==(const Numeric& other) const;
  bool operator!=(const Numeric& other) const;
  bool operator>(const Numeric& other) const;
  bool operator>=(const Numeric& other) const;
  bool operator<(const Numeric& other) const;
  bool operator<=(const Numeric& other) const;

  // Numeric + T (산술 연산자)
  template <typename U>
  Numeric operator+(U other) const;
  template <typename U>
  Numeric operator-(U other) const;
  template <typename U>
  Numeric operator*(U other) const;
  template <typename U>
  Numeric operator/(U other) const;
  template <typename U>
  Numeric operator+=(U other) const;
  template <typename U>
  Numeric operator-=(U other) const;

  // Numeric + T 비교 연산자
  template <typename U>
  bool operator==(U other) const;
  template <typename U>
  bool operator!=(U other) const;
  template <typename U>
  bool operator>(U other) const;
  template <typename U>
  bool operator>=(U other) const;
  template <typename U>
  bool operator<(U other) const;
  template <typename U>
  bool operator<=(U other) const;

  // T + Numeric 산술 연산자 (friend)
  template <typename U>
  friend Numeric operator+(U lhs, const Numeric& rhs) {
    return Numeric(lhs + rhs.value_);
  }
  template <typename U>
  friend Numeric operator-(U lhs, const Numeric& rhs) {
    return Numeric(lhs - rhs.value_);
  }
  template <typename U>
  friend Numeric operator*(U lhs, const Numeric& rhs) {
    return Numeric(lhs * rhs.value_);
  }
  template <typename U>
  friend Numeric operator/(U lhs, const Numeric& rhs) {
    return Numeric(lhs / rhs.value_);
  }
  template <typename U>
  friend Numeric operator+=(U lhs, const Numeric& rhs) {
    lhs += rhs.value_;
    return lhs;
  }
  template <typename U>
  friend Numeric operator-=(U lhs, const Numeric& rhs) {
    lhs -= rhs.value_;
    return lhs;
  }

  // T + Numeric 비교 연산자 (friend)
  template <typename U>
  friend bool operator==(U lhs, const Numeric& rhs) {
    return data_utils::IsEqual(lhs, rhs.value_);
  }
  template <typename U>
  friend bool operator!=(U lhs, const Numeric& rhs) {
    return !data_utils::IsEqual(lhs, rhs.value_);
  }
  template <typename U>
  friend bool operator>(U lhs, const Numeric& rhs) {
    return data_utils::IsGreater(lhs, rhs.value_);
  }
  template <typename U>
  friend bool operator>=(U lhs, const Numeric& rhs) {
    return data_utils::IsGreaterOrEqual(lhs, rhs.value_);
  }
  template <typename U>
  friend bool operator<(U lhs, const Numeric& rhs) {
    return data_utils::IsLess(lhs, rhs.value_);
  }
  template <typename U>
  friend bool operator<=(U lhs, const Numeric& rhs) {
    return data_utils::IsLessOrEqual(lhs, rhs.value_);
  }

 private:
  T value_;
};