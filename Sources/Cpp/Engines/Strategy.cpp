// 표준 라이브러리
#include <format>
#include <utility>

// 파일 헤더
#include "Engines/Strategy.hpp"

Strategy::Strategy(string name) : name(move(name)) {}
Strategy::~Strategy() = default;

OrderManager& Strategy::order = OrderManager::GetOrderManager();
