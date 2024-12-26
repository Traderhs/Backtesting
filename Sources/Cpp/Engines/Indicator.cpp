// 표준 라이브러리
#include <format>
#include <ranges>
#include <unordered_map>
#include <utility>

// 파일 헤더
#include <Engines/Indicator.hpp>

Indicator::Indicator(string name, string timeframe)
    : name(move(name)), timeframe(move(timeframe)) {}
Indicator::~Indicator() = default;

double Indicator::operator[](const size_t index) {
  if (output.empty()) {
    Logger::LogAndThrowError(
        format("{} {} 지표가 계산되지 않았습니다. CalculateAll 함수를 지표 "
               "생성자에서 호출해야 합니다.",
               name, this->timeframe),
        __FILE__, __LINE__);
  }

  const string& current_symbol = bar.current_symbol;
  const size_t current_index =
      bar.GetCurrentIndex(current_symbol, this->timeframe);

  // 인덱스 범위 체크
  if (const auto out_index =
          bar.GetSubBarData()[current_symbol][this->timeframe].size();
      current_index < index || (current_index - index) >= out_index) {
    return {nan("")};  // 범위 벗어난 경우 nan 반환
  }

  return output[current_symbol][current_index - index];
}

BarDataManager& Indicator::bar = BarDataManager::GetBarDataManager();
Logger& Indicator::logger = Logger::GetLogger();

void Indicator::CalculateAll() {
  bar.current_bar_data_type = BarDataManager::BarDataType::SUB;

  const auto& sub_bar_data = bar.GetSubBarData();

  // 전체 트레이딩 심볼들을 순회하며 지표에 해당하는 타임프레임의 지표 계산
  for (const auto& symbol : bar.GetTradingBarData() | views::keys) {
    // 서브 바 데이터에 심볼이 존재하는지 체크
    const auto& symbol_it = sub_bar_data.find(symbol);
    if (symbol_it == sub_bar_data.end()) {
      Logger::LogAndThrowError(
          format("트레이딩 바 데이터로 추가된 심볼 {}이(가) 서브 바 데이터로 "
                 "추가되지 않았습니다.",
                 symbol),
          __FILE__, __LINE__);
    }

    // 서브 바 데이터 심볼에 해당 타임프레임이 존재하는지 체크
    const auto& timeframe_it = symbol_it->second.find(timeframe);
    if (timeframe_it == symbol_it->second.end()) {
      Logger::LogAndThrowError(
          format("심볼 {}에 지표 타임프레임에 해당되는 "
                 "{}이(가) 서브 바 데이터로 추가되지 않았습니다.",
                 symbol, timeframe),
          __FILE__, __LINE__);
    }

    bar.current_symbol = symbol;

    // 해당 심볼과 타임 프레임에 해당되는 서브 바 데이터의 전체 데이터를
    // 순회하며 계산
    for (int i = 0; i < timeframe_it->second.size(); i++) {
      output[symbol].push_back(Calculate());
    }

    // 다른 지표 계산을 위해 인덱스 초기화
    bar.SetCurrentIndex(symbol, timeframe, 0);
  }

  // 계산 완료 로깅
  logger.Log(Logger::INFO_L, format(
    "{} {} 지표의 계산이 완료되었습니다.", name, timeframe), __FILE__, __LINE__);
}

void Indicator::SetInput(const vector<double>& input) {
  this->input = input;
}

vector<double> Indicator::GetInput() {
  return this->input;
}

string Indicator::GetTimeframe() const {
  return timeframe;
}
