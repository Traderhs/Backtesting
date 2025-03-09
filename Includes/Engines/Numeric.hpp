#pragma once

// 내부 헤더
#include "Engines/DataUtils.hpp"

// 네임 스페이스
using namespace std;

namespace backtesting::numeric {

/// 부동소숫점 오류를 방지하기 위한 연산자를 지원하는 템플릿 숫자 클래스
template <typename T>
class Numeric final {
 public:
  // 생성자
  Numeric() : value_(T{}) {}
  // ReSharper disable once CppNonExplicitConvertingConstructor
  Numeric(T v) : value_(v) {}  // NOLINT(*-explicit-constructor)

  // ReSharper disable once CppNonExplicitConversionOperator
  operator T() const { return value_; }  // NOLINT(*-explicit-constructor)

  // Numeric 간 산술 연산자
  Numeric operator+(const Numeric& other) const {
    return Numeric(value_ + other.value_);
  }
  Numeric operator-(const Numeric& other) const {
    return Numeric(value_ - other.value_);
  }
  Numeric operator*(const Numeric& other) const {
    return Numeric(value_ * other.value_);
  }
  Numeric operator/(const Numeric& other) const {
    return Numeric(value_ / other.value_);
  }
  Numeric operator+=(const Numeric& other) {
    value_ += other.value_;  // 다른 객체의 value를 더함
    return *this;            // 자기 자신을 반환
  }
  Numeric operator-=(const Numeric& other) {
    value_ -= other.value_;  // 다른 객체의 value를 더함
    return *this;            // 자기 자신을 반환
  }

  // Numeric 간 비교 연산자
  bool operator==(const Numeric& other) const {
    return utils::IsEqual(value_, other.value_);
  }
  bool operator!=(const Numeric& other) const {
    return !utils::IsEqual(value_, other.value_);
  }
  bool operator>(const Numeric& other) const {
    return utils::IsGreater(value_, other.value_);
  }
  bool operator>=(const Numeric& other) const {
    return utils::IsGreaterOrEqual(value_, other.value_);
  }
  bool operator<(const Numeric& other) const {
    return utils::IsLess(value_, other.value_);
  }
  bool operator<=(const Numeric& other) const {
    return utils::IsLessOrEqual(value_, other.value_);
  }

  // Numeric + U (산술 연산자)
  template <typename U>
  Numeric operator+(U other) const {
    return Numeric(value_ + other);
  }
  template <typename U>
  Numeric operator-(U other) const {
    return Numeric(value_ - other);
  }
  template <typename U>
  Numeric operator*(U other) const {
    return Numeric(value_ * other);
  }
  template <typename U>
  Numeric operator/(U other) const {
    return Numeric(value_ / other);
  }
  template <typename U>
  Numeric operator+=(U other) const {
    value_ += other;  // 다른 객체를 더함
    return *this;     // 자기 자신을 반환
  }
  template <typename U>
  Numeric operator-=(U other) const {
    value_ -= other;  // 다른 객체를 뺌
    return *this;     // 자기 자신을 반환
  }

  // Numeric + U 비교 연산자
  template <typename U>
  bool operator==(U other) const {
    return utils::IsEqual(value_, other);
  }
  template <typename U>
  bool operator!=(U other) const {
    return !utils::IsEqual(value_, other);
  }
  template <typename U>
  bool operator>(U other) const {
    return utils::IsGreater(value_, other);
  }
  template <typename U>
  bool operator>=(U other) const {
    return utils::IsGreaterOrEqual(value_, other);
  }
  template <typename U>
  bool operator<(U other) const {
    return utils::IsLess(value_, other);
  }
  template <typename U>
  bool operator<=(U other) const {
    return utils::IsLessOrEqual(value_, other);
  }

  // U + Numeric 산술 연산자 (friend)
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
  friend U& operator+=(U& lhs, const Numeric& rhs) {
    lhs += rhs.value_;
    return lhs;
  }
  template <typename U>
  friend U& operator-=(U& lhs, const Numeric& rhs) {
    lhs -= rhs.value_;
    return lhs;
  }

  // U + Numeric 비교 연산자 (friend)
  template <typename U>
  friend bool operator==(U lhs, const Numeric& rhs) {
    return utils::IsEqual(lhs, rhs.value_);
  }
  template <typename U>
  friend bool operator!=(U lhs, const Numeric& rhs) {
    return !utils::IsEqual(lhs, rhs.value_);
  }
  template <typename U>
  friend bool operator>(U lhs, const Numeric& rhs) {
    return utils::IsGreater(lhs, rhs.value_);
  }
  template <typename U>
  friend bool operator>=(U lhs, const Numeric& rhs) {
    return utils::IsGreaterOrEqual(lhs, rhs.value_);
  }
  template <typename U>
  friend bool operator<(U lhs, const Numeric& rhs) {
    return utils::IsLess(lhs, rhs.value_);
  }
  template <typename U>
  friend bool operator<=(U lhs, const Numeric& rhs) {
    return utils::IsLessOrEqual(lhs, rhs.value_);
  }

 private:
  T value_;
};

}  // namespace backtesting::numeric