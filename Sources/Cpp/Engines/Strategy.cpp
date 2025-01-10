// 표준 라이브러리
#include <format>
#include <utility>

// 파일 헤더
#include "Engines\Strategy.hpp"

// 내부 헤더
#include <Engines/BarData.hpp>
#include <Engines/BarHandler.hpp>
#include <Engines/BaseBarHandler.hpp>
#include <Engines/OrderHandler.hpp>

Strategy::Strategy(string name)
    : order(OrderHandler::GetOrderHandler(name)),
      open("Open", bar->GetBarData(BarType::TRADING).GetTimeframe()),
      high("High", bar->GetBarData(BarType::TRADING).GetTimeframe()),
      low("Low", bar->GetBarData(BarType::TRADING).GetTimeframe()),
      close("Close", bar->GetBarData(BarType::TRADING).GetTimeframe()),
      volume("Volume", bar->GetBarData(BarType::TRADING).GetTimeframe()),
      name_(move(name)) {}
Strategy::~Strategy() = default;

shared_ptr<OrderHandler> Strategy::GetOrderHandler() const { return order; }

// ReSharper disable once CppInconsistentNaming
shared_ptr<BarHandler>& Strategy::bar = BarHandler::GetBarHandler();
