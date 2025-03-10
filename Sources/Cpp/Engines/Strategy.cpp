// 표준 라이브러리
#include <format>

// 파일 헤더
#include "Engines/Strategy.hpp"

// 내부 헤더
#include "Engines/BarHandler.hpp"
#include "Engines/Engine.hpp"
#include "Engines/OrderHandler.hpp"

namespace backtesting::strategy {

Strategy::Strategy(const string& name, const string& file_path)
    : name_(name),
      order(OrderHandler::GetOrderHandler(name)),
      open(AddIndicator<Open>("Open", trading_timeframe)),
      high(AddIndicator<High>("High", trading_timeframe)),
      low(AddIndicator<Low>("Low", trading_timeframe)),
      close(AddIndicator<Close>("Close", trading_timeframe)),
      volume(AddIndicator<Volume>("Volume", trading_timeframe)) {
  // 증가 카운터는 AddStrategy 함수로만 증가하는데 AddStrategy 없이 직접 생성자
  // 호출로 전 증가 카운터가 현재 증가 카운터와 같다면 오류 발생
  if (pre_creation_counter_ == creation_counter_) {
    logger->Log(ERROR_L,
                "전략의 추가는 AddStrategy 함수의 호출로만 가능합니다.",
                __FILE__, __LINE__);
    Logger::LogAndThrowError(
        format("[{}] 전략을 추가하는 중 에러가 발생했습니다.", name), __FILE__,
        __LINE__);
  }

  // 정상적으로 AddStrategy 함수를 통했다면 전 증가 가운터에 현재 카운터를 대입
  pre_creation_counter_ = creation_counter_;

  if (name.empty()) {
    logger->Log(ERROR_L, "전략 이름이 비어있습니다.", __FILE__, __LINE__);
    Logger::LogAndThrowError("전략 생성 중 오류가 발생했습니다.", __FILE__,
                             __LINE__);
  }

  // 자식 파일 경로를 저장 -> 백테스팅 종료 후 저장 목적
  child_file_path_ = file_path;
}
Strategy::~Strategy() = default;

vector<shared_ptr<Strategy>> Strategy::strategies_;
size_t Strategy::creation_counter_;
size_t Strategy::pre_creation_counter_;

// ReSharper disable once CppInconsistentNaming
shared_ptr<BarHandler>& Strategy::bar = BarHandler::GetBarHandler();

// ReSharper disable once CppInconsistentNaming
shared_ptr<Engine>& Strategy::engine = Engine::GetEngine();

// ReSharper disable once CppInconsistentNaming
shared_ptr<Logger>& Strategy::logger = Logger::GetLogger();

// ReSharper disable once CppInconsistentNaming
string Strategy::trading_timeframe = "TRADING_TIMEFRAME";

void Strategy::SetTradingTimeframe(const string& trading_tf) {
  trading_timeframe = trading_tf;
}

vector<shared_ptr<Strategy>>& Strategy::GetStrategies() { return strategies_; }
vector<shared_ptr<Indicator>>& Strategy::GetIndicators() { return indicators_; }
string Strategy::GetName() const { return name_; }
string Strategy::GetSourcePath() { return child_file_path_; }
shared_ptr<OrderHandler> Strategy::GetOrderHandler() const { return order; }

}  // namespace backtesting::strategy