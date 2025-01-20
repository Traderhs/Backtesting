// 표준 라이브러리
#include <format>
#include <utility>

// 파일 헤더
#include "Engines/Strategy.hpp"

// 내부 헤더
#include "Engines/BarData.hpp"
#include "Engines/BarHandler.hpp"
#include "Engines/BaseBarHandler.hpp"
#include "Engines/OrderHandler.hpp"

Strategy::Strategy(string name)
    : order(OrderHandler::GetOrderHandler(name)),
      open("Open", bar->GetBarData(BarType::TRADING).GetTimeframe()),
      high("High", bar->GetBarData(BarType::TRADING).GetTimeframe()),
      low("Low", bar->GetBarData(BarType::TRADING).GetTimeframe()),
      close("Close", bar->GetBarData(BarType::TRADING).GetTimeframe()),
      volume("Volume", bar->GetBarData(BarType::TRADING).GetTimeframe()),
      name_(move(name)) {
  /* 전략 생성 시 OHLCV 지표 계산으로 인해, 지표들의 output_을
   resize해야 하기 때문에 전략 추가 전 모든 트레이딩 바 데이터를 추가해야 함 */
  bar->is_strategy_created_ = true;
}
Strategy::~Strategy() = default;

string Strategy::GetName() const { return name_; }
shared_ptr<OrderHandler> Strategy::GetOrderHandler() const { return order; }

// ReSharper disable once CppInconsistentNaming
shared_ptr<BarHandler>& Strategy::bar = BarHandler::GetBarHandler();
