// 표준 라이브러리
#include <format>
#include <utility>

// 파일 헤더
#include "Engines/Indicator.hpp"

// 내부 헤더
#include "Engines/BarData.hpp"
#include "Engines/BarHandler.hpp"
#include "Engines/BaseBarHandler.hpp"
#include "Engines/DataUtils.hpp"
#include "Engines/Engine.hpp"
#include "Engines/Exception.hpp"
#include "Engines/TimeUtils.hpp"

// 네임 스페이스
using namespace data_utils;
using namespace time_utils;

Indicator::Indicator(string name, string timeframe)
    : name_(move(name)), timeframe_(move(timeframe)), is_calculated_(false) {
  const int num_symbols = bar_->GetBarData(BarType::TRADING).GetNumSymbols();

  /* Strategy 클래스에 포함된 OHLCV 지표 계산으로 인해, 미리 output_을
     resize해야 하기 때문에 전략 추가 전 트레이딩 바 데이터를 추가해야 함 */
  if (num_symbols == 0) {
    // 전략 추가 -> 지표 추가 순서이므로 로그 메세지는 '전략'으로 사용
    Logger::LogAndThrowError(
        "트레이딩 바 데이터가 추가되지 않았습니다. 트레이딩 바 데이터 추가 후 "
        "전략을 추가해야 합니다.",
        __FILE__, __LINE__);
  }

  // output_의 심볼 개수를 트레이딩 바 심볼의 개수로 초기화
  output_.resize(num_symbols);
}
Indicator::~Indicator() = default;

bool Indicator::is_calculating_ = false;

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

  // 원래 사용중인 데이터 정보 저장
  const auto original_bar_type = bar_->GetCurrentBarType();
  const auto& original_reference_tf = bar_->GetCurrentReferenceTimeframe();

  // 지표 계산 중 다른 타임프레임 사용 시 에러 발생
  if (is_calculating_ && original_reference_tf != timeframe_) {
    throw runtime_error(
        format("지표 계산에 사용하는 {} 지표의 타임프레임 {}은(는) "
               "해당 지표의 타임프레임 {}와 동일해야 합니다.",
               name_, timeframe_, original_reference_tf));
  }

  // 지표 참조를 위해 데이터 환경 설정
  bar_->SetCurrentBarType(BarType::REFERENCE, timeframe_);
  const auto symbol_idx = bar_->GetCurrentSymbolIndex();
  const auto bar_index = bar_->GetCurrentBarIndex();

  // 진행한 인덱스보다 과거 인덱스 참조 시 throw
  IsValidReferenceIndex(bar_index, index);

  // 원래 사용 중이던 데이터 환경으로 복구
  bar_->SetCurrentBarType(original_bar_type, original_reference_tf);

  // 결과값 반환
  const auto value = output_[symbol_idx][bar_index - index];
  return !isnan(value) ? value : throw IndicatorInvalidValue("");
}

void Indicator::OutputToCsv(const string& file_name,
                            const int symbol_index) const {
  const auto& trading_bar = bar_->GetBarData(BarType::TRADING);

  try {
    trading_bar.IsValidSymbolIndex(symbol_index);

    VectorToCsv(output_[symbol_index], file_name);
  } catch (...) {
    Logger::LogAndThrowError(
        format("{} {}의 {}을(를) 파일로 저장하는 중 에러가 발생했습니다.",
               trading_bar.GetSymbolName(symbol_index), timeframe_, name_),
        __FILE__, __LINE__);
  }

  logger_->Log(
      LogLevel::INFO_L,
      format("{} | {} {}의 {}이(가) csv파일로 저장되었습니다.", file_name,
             trading_bar.GetSymbolName(symbol_index), timeframe_, name_),
      __FILE__, __LINE__);
}

void Indicator::CalculateAll() {
  // 원본 설정을 저장
  const auto original_bar_type = bar_->GetCurrentBarType();
  const auto original_reference_tf = bar_->GetCurrentReferenceTimeframe();
  const auto original_symbol_idx = bar_->GetCurrentSymbolIndex();

  try {
    const auto& reference_bar =
        bar_->GetBarData(BarType::REFERENCE, timeframe_);

    is_calculating_ = true;
    bar_->SetCurrentBarType(BarType::REFERENCE, timeframe_);

    // 전체 트레이딩 심볼들을 순회하며 지표 계산
    for (int i = 0; i < output_.size(); i++) {
      bar_->SetCurrentSymbolIndex(i);

      // 해당 심볼의 모든 바를 순회
      const auto num_bars = reference_bar.GetNumBars(i);
      output_[i].resize(num_bars);

      for (int j = 0; j < num_bars; j++) {
        // 현재 바 데이터의 인덱스 증가
        bar_->SetCurrentBarIndex(j);

        try {
          output_[i][j] = Calculate();
        } catch ([[maybe_unused]] IndicatorInvalidValue& e) {
          /* 해당 지표 계산 시 타 지표를 사용하는데,
             현재 바에서 타 지표 계산이 안 되어 nan이면 해당 지표 값도 nan */
          output_[i][j] = nan("");
        }
      }

      // 다른 지표 계산을 위해서 변경한 인덱스와 지표 멤버 변수 초기화
      bar_->SetCurrentBarIndex(0);
      Initialize();
    }

    is_calculated_ = true;

    // 계산 완료 로그
    logger_->Log(
        LogLevel::INFO_L,
        format("모든 심볼 {} {} 지표의 계산이 완료되었습니다.", name_, timeframe_),
        __FILE__, __LINE__);

  } catch (exception& e) {
    Logger::LogAndThrowError(
        format("{} {} 지표를 계산하는 중 에러가 발생했습니다. | {}", name_,
               timeframe_, e.what()),
        __FILE__, __LINE__);
  }

  // 원본 설정을 복원
  is_calculating_ = false;
  bar_->SetCurrentBarType(original_bar_type, original_reference_tf);
  bar_->SetCurrentSymbolIndex(original_symbol_idx);
}

void Indicator::SetInput(const vector<double>& input) { input_ = input; }

string Indicator::GetName() const { return name_; }

string Indicator::GetTimeframe() const { return timeframe_; }

vector<double> Indicator::GetInput() const { return input_; }

void Indicator::IsValidReferenceIndex(const size_t current_bar_index, const size_t target_index) const {
  // 진행한 인덱스보다 과거 인덱스 참조 시 throw
  if (current_bar_index < target_index) {
    const auto& bar = bar_->GetBarData(BarType::REFERENCE, timeframe_);
    const int symbol_idx = bar_->GetCurrentSymbolIndex();
    const size_t bar_idx = bar_->GetCurrentBarIndex();

    throw IndicatorOutOfRange(format(
        "{} | {} | 진행한 인덱스보다 과거 인덱스를 사용하여 지표값을 참조할 "
        "수 없습니다.",
        bar.GetSymbolName(symbol_idx),
        UtcTimestampToUtcDatetime(bar.GetOpenTime(symbol_idx, bar_idx))));
  }
}