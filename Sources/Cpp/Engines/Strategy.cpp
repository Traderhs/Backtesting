// 표준 라이브러리
#include <format>
#include <utility>

// 파일 헤더
#include "Engines/Strategy.hpp"

// 내부 헤더
#include "Engines/BarHandler.hpp"
#include "Engines/BaseBarHandler.hpp"
#include "Engines/Engine.hpp"
#include "Engines/OrderHandler.hpp"

Strategy::Strategy(string name)
    : order(OrderHandler::GetOrderHandler(name)),
      trading_timeframe("NULL"),
      open(Indicator::Create<Open>("Open", trading_timeframe)),
      high(Indicator::Create<High>("High", trading_timeframe)),
      low(Indicator::Create<Low>("Low", trading_timeframe)),
      close(Indicator::Create<Close>("Close", trading_timeframe)),
      volume(Indicator::Create<Volume>("Volume", trading_timeframe)),
      name_(move(name)) {}
Strategy::~Strategy() = default;

// ReSharper disable once CppInconsistentNaming
shared_ptr<BarHandler>& Strategy::bar = BarHandler::GetBarHandler();

// ReSharper disable once CppInconsistentNaming
shared_ptr<Engine>& Strategy::engine = Engine::GetEngine();

void Strategy::SetTradingTimeframe(const string& trading_timeframe) {
  this->trading_timeframe = trading_timeframe;
}

string Strategy::GetName() const { return name_; }
shared_ptr<OrderHandler> Strategy::GetOrderHandler() const { return order; }
