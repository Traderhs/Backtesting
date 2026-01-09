// 표준 라이브러리
#include <format>

// 파일 헤더
#include "Engines/Strategy.hpp"

// 내부 헤더
#include "Engines/BarHandler.hpp"
#include "Engines/Engine.hpp"
#include "Engines/OrderHandler.hpp"

namespace backtesting::strategy {

Strategy::Strategy(const string& name)
    : name_(name),
      order(OrderHandler::GetOrderHandler()),
      open(AddIndicator<Open>("Open", trading_timeframe)),
      high(AddIndicator<High>("High", trading_timeframe)),
      low(AddIndicator<Low>("Low", trading_timeframe)),
      close(AddIndicator<Close>("Close", trading_timeframe)),
      volume(AddIndicator<Volume>("Volume", trading_timeframe)) {
  // AddStrategy 함수를 거치지 않은 전략 생성자는 오류
  if (!used_creation_function_) {
    logger->Log(ERROR_L,
                "전략의 추가는 AddStrategy 함수의 호출로만 가능합니다.",
                __FILE__, __LINE__, true);
    Logger::LogAndThrowError(
        format("[{}] 전략을 추가하는 중 에러가 발생했습니다.", name), __FILE__,
        __LINE__);
  } else {
    // 추후 OrderHandler를 또 받을 수 있기 때문에 초기화
    used_creation_function_ = false;
  }

  if (name.empty()) {
    logger->Log(ERROR_L, "전략 이름이 비어있습니다.", __FILE__, __LINE__, true);
    Logger::LogAndThrowError("전략 생성 중 오류가 발생했습니다.", __FILE__,
                             __LINE__);
  }
}
Strategy::~Strategy() = default;

BACKTESTING_API shared_ptr<Strategy> Strategy::strategy_;
BACKTESTING_API bool Strategy::used_creation_function_ = false;

// ReSharper disable once CppInconsistentNaming
BACKTESTING_API shared_ptr<BarHandler>& Strategy::bar =
    BarHandler::GetBarHandler();

// ReSharper disable once CppInconsistentNaming
BACKTESTING_API shared_ptr<Engine>& Strategy::engine = Engine::GetEngine();

// ReSharper disable once CppInconsistentNaming
BACKTESTING_API shared_ptr<Logger>& Strategy::logger = Logger::GetLogger();

// ReSharper disable once CppInconsistentNaming
BACKTESTING_API string Strategy::trading_timeframe = "TRADING_TIMEFRAME";

// =============================================================================
void Strategy::SetTradingTimeframe(const string& trading_tf) {
  if (trading_timeframe == "TRADING_TIMEFRAME") {
    trading_timeframe = trading_tf;
  } else {
    Logger::LogAndThrowError(
        format("트레이딩 바 데이터의 타임프레임 [{}]이(가) 이미 설정되어 "
               "재설정할 수 없습니다.",
               trading_timeframe),
        __FILE__, __LINE__);
  }
}

shared_ptr<Strategy>& Strategy::GetStrategy() { return strategy_; }

vector<shared_ptr<Indicator>>& Strategy::GetIndicators() { return indicators_; }

string Strategy::GetStrategyName() const { return name_; }

string Strategy::GetStrategyClassName() const { return class_name_; }

shared_ptr<OrderHandler> Strategy::GetOrderHandler() const { return order; }

string Strategy::GetSourcePath() { return source_path_; }

string Strategy::GetHeaderPath() { return header_path_; }

void Strategy::ResetStrategy() {
  strategy_.reset();
  used_creation_function_ = false;
  trading_timeframe = "TRADING_TIMEFRAME";
}

}  // namespace backtesting::strategy
