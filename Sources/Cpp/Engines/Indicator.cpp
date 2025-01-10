// 표준 라이브러리
#include <format>
#include <utility>

// 파일 헤더
#include "Engines\Indicator.hpp"

// 내부 헤더
#include "Engines\BarData.hpp"
#include "Engines\BarHandler.hpp"
#include "Engines\BaseBarHandler.hpp"
#include "Engines\Engine.hpp"

Indicator::Indicator(string name, string timeframe)
    : name_(move(name)), timeframe_(move(timeframe)), is_calculated_(false) {
  const int num_symbols = bar_->GetBarData(BarType::TRADING).GetNumSymbols();

  if (num_symbols == 0) {
    // 전략 추가 -> 지표 추가 순서이므로 로그 메세지는 '전략'으로 사용
    Logger::LogAndThrowError(
        "트레이딩 바에 데이터가 추가되지 않았습니다. 바 데이터 추가 후 전략을 "
        "추가해야 합니다.",
        __FILE__, __LINE__);
  }

  // output_의 심볼 개수를 트레이딩 바 심볼의 개수로 초기화
  output_.resize(num_symbols);
}
Indicator::~Indicator() = default;

shared_ptr<BarHandler>& Indicator::bar_ = BarHandler::GetBarHandler();
shared_ptr<Engine>& Indicator::engine_ = Engine::GetEngine();
shared_ptr<Logger>& Indicator::logger_ = Logger::GetLogger();

double Indicator::operator[](const size_t index) {
  if (!is_calculated_) {
    Logger::LogAndThrowError(
        format("{} {} 지표가 계산되지 않았습니다. CalculateAll 함수를 지표 "
               "생성자에서 호출해야 합니다.",
               name_, timeframe_),
        __FILE__, __LINE__);
  }

  bar_->SetCurrentBarType(BarType::REFERENCE, timeframe_);
  const size_t bar_index = bar_->GetCurrentBarIndex();

  if (bar_index < index) {
    return {nan("")};  // 진행한 인덱스보다 과거 인덱스 참조 시 nan 반환
  }

  return output_[bar_->GetCurrentSymbolIndex()][bar_index - index];
}

void Indicator::CalculateAll() {
  bar_->SetCurrentBarType(BarType::REFERENCE, timeframe_);

  const auto& reference_bar =
      bar_->GetBarData(BarType::REFERENCE, timeframe_);

  // 전체 트레이딩 심볼들을 순회하며 지표 계산
  for (int i = 0; i < output_.size(); i++) {
    bar_->SetCurrentSymbolIndex(i);

    for (int j = 0; j < reference_bar.GetNumBars(i); j++) {
      bar_->SetCurrentBarIndex(j);
      output_[i].push_back(Calculate());
    }

    // 다른 지표 계산을 위해 인덱스 초기화
    bar_->SetCurrentBarIndex(0);
  }

  is_calculated_ = true;

  // 계산 완료 디버그 로그
  if (engine_->debug_mode_) {
    logger_->Log(
        LogLevel::DEBUG_L,
        format("{} {} 지표의 계산이 완료되었습니다.", name_, timeframe_),
        __FILE__, __LINE__);
  }
}

void Indicator::SetInput(const vector<double>& input) {
  input_ = input;
}

vector<double> Indicator::GetInput() {
  return input_;
}

string Indicator::GetTimeframe() const {
  return timeframe_;
}
