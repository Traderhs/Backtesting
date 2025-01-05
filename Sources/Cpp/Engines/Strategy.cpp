// 표준 라이브러리
#include <format>
#include <utility>

// 파일 헤더
#include "Engines/Strategy.hpp"

Strategy::Strategy(string name) : name_(move(name)) {}
Strategy::~Strategy() = default;

OrderHandler& Strategy::GetOrderHandler() const {
  return OrderHandler::GetOrderHandler(name_);
}

OrderHandler& Strategy::order_ = OrderHandler::GetOrderHandler(name_);
