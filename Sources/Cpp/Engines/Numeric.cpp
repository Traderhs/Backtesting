// 파일 헤더
#include "Engines/Numeric.hpp"

template <typename T>
Numeric<T>::Numeric(T v) : value_(v) {}

template <typename T>
Numeric<T>::operator T() const {
  return value_;
}

// Numeric 간 산술 연산자
template <typename T>
Numeric<T> Numeric<T>::operator+(const Numeric& other) const {
  return Numeric(value_ + other.value_);
}

template <typename T>
Numeric<T> Numeric<T>::operator-(const Numeric& other) const {
  return Numeric(value_ - other.value_);
}

template <typename T>
Numeric<T> Numeric<T>::operator*(const Numeric& other) const {
  return Numeric(value_ * other.value_);
}

template <typename T>
Numeric<T> Numeric<T>::operator/(const Numeric& other) const {
  if (other.value_ == 0) {
    throw runtime_error("0으로 나눌 수 없습니다.");
  }
  return Numeric(value_ / other.value_);
}

template <typename T>
Numeric<T> Numeric<T>::operator+=(const Numeric& other) const {
  value_ += other.value_; // 다른 객체의 value를 더함
  return *this;  // 자기 자신을 반환
}

template <typename T>
Numeric<T> Numeric<T>::operator-=(const Numeric& other) const {
  value_ -= other.value_; // 다른 객체의 value를 뺌
  return *this;  // 자기 자신을 반환
}

// Numeric 간 비교 연산자
template <typename T>
bool Numeric<T>::operator==(const Numeric& other) const {
  return data_utils::IsEqual(value_, other.value_);
}

template <typename T>
bool Numeric<T>::operator!=(const Numeric& other) const {
  return !data_utils::IsEqual(value_, other.value_);
}

template <typename T>
bool Numeric<T>::operator>(const Numeric& other) const {
  return data_utils::IsGreater(value_, other.value_);
}

template <typename T>
bool Numeric<T>::operator>=(const Numeric& other) const {
  return data_utils::IsGreaterOrEqual(value_, other.value_);
}

template <typename T>
bool Numeric<T>::operator<(const Numeric& other) const {
  return data_utils::IsLess(value_, other.value_);
}

template <typename T>
bool Numeric<T>::operator<=(const Numeric& other) const {
  return data_utils::IsLessOrEqual(value_, other.value_);
}

// Numeric + T 산술 연산자
template <typename T>
template <typename U>
Numeric<T> Numeric<T>::operator+(U other) const {
  return Numeric(value_ + other);
}

template <typename T>
template <typename U>
Numeric<T> Numeric<T>::operator-(U other) const {
  return Numeric(value_ - other);
}

template <typename T>
template <typename U>
Numeric<T> Numeric<T>::operator*(U other) const {
  return Numeric(value_ * other);
}

template <typename T>
template <typename U>
Numeric<T> Numeric<T>::operator/(U other) const {
  if (other == 0) {
    throw std::runtime_error("0으로 나눌 수 없습니다.");
  }
  return Numeric(value_ / other);
}

template <typename T>
template <typename U>
Numeric<T> Numeric<T>::operator+=(U other) const {
  value_ += other; // 다른 객체를 더함
  return *this;  // 자기 자신을 반환
}

template <typename T>
template <typename U>
Numeric<T> Numeric<T>::operator-=(U other) const {
  value_ -= other; // 다른 객체를 뺌
  return *this;  // 자기 자신을 반환
}

// Numeric + T 비교 연산자
template <typename T>
template <typename U>
bool Numeric<T>::operator==(U other) const {
  return data_utils::IsEqual(value_, other);
}

template <typename T>
template <typename U>
bool Numeric<T>::operator!=(U other) const {
  return !data_utils::IsEqual(value_, other);
}

template <typename T>
template <typename U>
bool Numeric<T>::operator>(U other) const {
  return data_utils::IsGreater(value_, other);
}

template <typename T>
template <typename U>
bool Numeric<T>::operator>=(U other) const {
  return data_utils::IsGreaterOrEqual(value_, other);
}

template <typename T>
template <typename U>
bool Numeric<T>::operator<(U other) const {
  return data_utils::IsLess(value_, other);
}

template <typename T>
template <typename U>
bool Numeric<T>::operator<=(U other) const {
  return data_utils::IsLessOrEqual(value_, other);
}