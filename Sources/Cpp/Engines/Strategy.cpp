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

shared_ptr<Strategy> Strategy::strategy_;
bool Strategy::used_creation_function_ = false;

// ReSharper disable once CppInconsistentNaming
shared_ptr<BarHandler>& Strategy::bar = BarHandler::GetBarHandler();

// ReSharper disable once CppInconsistentNaming
shared_ptr<Engine>& Strategy::engine = Engine::GetEngine();

// ReSharper disable once CppInconsistentNaming
shared_ptr<Logger>& Strategy::logger = Logger::GetLogger();

// ReSharper disable once CppInconsistentNaming
string Strategy::trading_timeframe = "TRADING_TIMEFRAME";

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

void Strategy::SetSourcePath(const string& source_path) {
  if (!fs::exists(source_path)) {
    Logger::LogAndThrowError(
        format("[{}] 전략의 소스 파일 경로 [{}]이(가) 존재하지 않습니다.",
               name_, source_path),
        __FILE__, __LINE__);
  }

  cpp_file_path_ = source_path;
}

void Strategy::SetHeaderPath(const string& header_path) {
  if (!fs::exists(header_path)) {
    Logger::LogAndThrowError(
        format("[{}] 전략의 헤더 파일 경로 [{}]이(가) 존재하지 않습니다.",
               name_, header_path),
        __FILE__, __LINE__);
  }

  header_file_path_ = header_path;
}

shared_ptr<Strategy>& Strategy::GetStrategy() { return strategy_; }

vector<shared_ptr<Indicator>>& Strategy::GetIndicators() { return indicators_; }

string Strategy::GetStrategyName() const { return name_; }

string Strategy::GetStrategyClassName() const { return class_name_; }

shared_ptr<OrderHandler> Strategy::GetOrderHandler() const { return order; }

string Strategy::GetSourcePath() { return cpp_file_path_; }

string Strategy::GetHeaderPath() { return header_file_path_; }

bool Strategy::SetFilePath(const string& path, const bool is_cpp) {
  if (fs::exists(path)) {
    if (is_cpp) {
      cpp_file_path_ = path;
    } else {
      header_file_path_ = path;
    }

    return true;
  }

  // 파일 경로 감지 실패
  if (is_cpp) {
    logger->Log(
        ERROR_L,
        format("[{}] 전략의 소스 파일 경로 감지가 실패했습니다.", name_),
        __FILE__, __LINE__, true);
    Logger::LogAndThrowError(
        format("전략의 클래스명과 소스 파일명은 동일해야 하며, "
               "[프로젝트 폴더/Sources/cpp/Strategies/{}.cpp] 경로에 "
               "존재해야 합니다.",
               class_name_),
        __FILE__, __LINE__);
  } else {
    logger->Log(
        ERROR_L,
        format("[{}] 전략의 헤더 파일 경로 감지가 실패했습니다.", name_),
        __FILE__, __LINE__, true);
    Logger::LogAndThrowError(
        format("전략의 클래스명과 헤더 파일명은 동일해야 하며, "
               "[프로젝트 폴더/Includes/Strategies/{}.hpp] 경로에 "
               "존재해야 합니다.",
               class_name_),
        __FILE__, __LINE__);
  }

  return false;
}

string Strategy::FindFileInParent(const string& filename) {
  try {
    const fs::path parent =
        fs::path(Config::GetProjectDirectory()).parent_path();

    for (const auto& entry : fs::recursive_directory_iterator(parent)) {
      if (entry.is_regular_file() && entry.path().filename() == filename) {
        return entry.path().string();
      }
    }
  } catch (...) {
    // 검색 중 오류 발생 시 빈 문자열 반환
  }

  return {};
}

}  // namespace backtesting::strategy
