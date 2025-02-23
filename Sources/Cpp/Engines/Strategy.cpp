// 표준 라이브러리
#include <format>
#include <utility>

// 파일 헤더
#include "Engines/Strategy.hpp"

// 내부 헤더
#include "Engines/BarHandler.hpp"
#include "Engines/BaseBarHandler.hpp"
#include "Engines/OrderHandler.hpp"

Strategy::Strategy(string name)
    : order(OrderHandler::GetOrderHandler(name)),
      open(Indicator::Create<Open>("Open", "1d")),
      high(Indicator::Create<High>("High", "1d")),
      low(Indicator::Create<Low>("Low", "1d")),
      close(Indicator::Create<Close>("Close", "1d")),
      volume(Indicator::Create<Volume>("Volume", "1d")),
      name_(move(name)) {}
Strategy::~Strategy() = default;

// ReSharper disable once CppInconsistentNaming
shared_ptr<BarHandler>& Strategy::bar = BarHandler::GetBarHandler();

void Strategy::SetTradingTimeframe(const string& trading_timeframe) {
  this->trading_timeframe = trading_timeframe;
}
string Strategy::GetName() const { return name_; }
shared_ptr<OrderHandler> Strategy::GetOrderHandler() const { return order; }
