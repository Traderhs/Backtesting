// 표준 라이브러리
#include <cmath>
#include <format>
#include <iomanip>
#include <sstream>

// 파일 헤더
#include "Engines/Indicator.hpp"

// 내부 헤더
#include "Engines/Analyzer.hpp"
#include "Engines/BarData.hpp"
#include "Engines/BarHandler.hpp"
#include "Engines/BaseBarHandler.hpp"
#include "Engines/DataUtils.hpp"
#include "Engines/Engine.hpp"
#include "Engines/TimeUtils.hpp"

// 네임 스페이스
using namespace backtesting::utils;

namespace backtesting::indicator {

Indicator::Indicator(const string& name, const string& timeframe,
                     const bool overlay, const PlotStyle plot_style,
                     const Color& color, const int line_width)
    : is_calculated_(false), color_(color) {
  try {
    if (name.empty()) {
      Logger::LogAndThrowError("지표 이름이 비어있습니다.", __FILE__, __LINE__);
    }

    if (timeframe.empty()) {
      Logger::LogAndThrowError(
          format("[{}] 지표의 타임프레임이 비어있습니다.", name), __FILE__,
          __LINE__);
    }

    if (line_width < 1 || line_width > 4) {
      Logger::LogAndThrowError(
          format(
              "지정된 지표의 굵기 [{}]은(는) [1 - 5] 사이로 지정해야 합니다.",
              line_width),
          __FILE__, __LINE__);
    }

    name_ = name;
    timeframe_ = timeframe;
    overlay_ = overlay;
    plot_style_ = plot_style;
    line_width_ = line_width;
  } catch ([[maybe_unused]] const exception& e) {
    Logger::LogAndThrowError("지표를 생성하는 중 오류가 발생했습니다.",
                             __FILE__, __LINE__);
  }

  // 증가 카운터는 AddIndicator 함수로만 증가하는데 AddIndicator 없이 직접
  // 생성자 호출로 전 증가 카운터가 현재 증가 카운터와 같다면 오류 발생
  if (pre_creation_counter_ == creation_counter_) {
    logger_->Log(ERROR_L,
                 "지표의 추가는 AddIndicator 함수의 호출로만 가능합니다.",
                 __FILE__, __LINE__);
    Logger::LogAndThrowError(
        format("[{} {}] 지표를 추가하는 중 에러가 발생했습니다.", name,
               timeframe),
        __FILE__, __LINE__);
  }

  // 정상적으로 AddIndicator 함수를 통했다면 전 증가 가운터에 현재 카운터를 대입
  pre_creation_counter_ = creation_counter_;
}
Indicator::~Indicator() = default;

shared_ptr<Analyzer>& Indicator::analyzer_ = Analyzer::GetAnalyzer();
shared_ptr<BarHandler>& Indicator::bar_ = BarHandler::GetBarHandler();
shared_ptr<Engine>& Indicator::engine_ = Engine::GetEngine();
shared_ptr<Logger>& Indicator::logger_ = Logger::GetLogger();
size_t Indicator::creation_counter_;
size_t Indicator::pre_creation_counter_;
bool Indicator::is_calculating_ = false;
string Indicator::calculating_name_;
string Indicator::calculating_timeframe_;

Numeric<double> Indicator::operator[](const size_t index) {
  // 특정 지표 계산 중 해당 지표와 다른 타임프레임의 지표 사용 시 에러 발생
  // -> 특정 지표 내 사용하는 지표의 타임프레임은
  //    해당 지표의 타임프레임과 일치해야 함
  if (is_calculating_ && timeframe_ != calculating_timeframe_) {
    throw runtime_error(
        format("[{} {}] 지표 계산에 사용하는 [{} {}] 지표의 타임프레임은 "
               "[{} {}] 지표의 타임프레임과 동일해야 합니다.",
               calculating_name_, calculating_timeframe_, name_, timeframe_,
               calculating_name_, calculating_timeframe_));
  }

  // =======================================================================
  // 원래 사용중인 데이터 환경 저장
  const auto original_bar_type = bar_->GetCurrentBarType();
  const auto& original_reference_tf = bar_->GetCurrentReferenceTimeframe();

  // 지표 참조를 위해 데이터 환경 설정
  bar_->SetCurrentBarType(REFERENCE, timeframe_);

  // 필요한 인덱스 로딩
  const auto symbol_idx = bar_->GetCurrentSymbolIndex();
  const auto bar_idx = bar_->GetCurrentBarIndex();

  // 원래 사용중인 데이터 환경으로 복구
  bar_->SetCurrentBarType(original_bar_type, original_reference_tf);

  // 진행한 인덱스보다 과거 인덱스를 참조하거나
  // 현재 인덱스가 최대값을 초과했으면 NaN을 반환
  if (index > bar_idx || bar_idx >= reference_num_bars_[symbol_idx]) {
    return nan("");
  }

  // 결과값 반환
  return output_[symbol_idx][bar_idx - index];
}

void Indicator::CalculateIndicator(const string& strategy_name) {
  try {
    // 엔진이 초기화되기 전 지표를 계산하면 모든 심볼의 계산이 어려우므로 에러
    if (!engine_->IsEngineInitialized()) {
      Logger::LogAndThrowError(
          format("엔진 초기화 전 [{}] 전략의 [{} {}] 지표 계산을 시도했습니다.",
                 strategy_name, name_, timeframe_),
          __FILE__, __LINE__);
    }
    // ===========================================================================
    // 사전 설정
    const auto& indicator_reference_bar =
        bar_->GetBarData(REFERENCE, timeframe_);
    is_calculating_ = true;
    calculating_name_ = name_;
    calculating_timeframe_ = timeframe_;

    const auto num_symbols = indicator_reference_bar->GetNumSymbols();
    output_.resize(num_symbols);
    reference_num_bars_.resize(num_symbols);

    // 전체 트레이딩 심볼들을 순회하며 지표 계산
    for (int symbol_idx = 0; symbol_idx < output_.size(); symbol_idx++) {
      this->Initialize();
      bar_->SetCurrentBarType(REFERENCE, timeframe_);
      bar_->SetCurrentSymbolIndex(symbol_idx);

      // 해당 심볼의 모든 바를 순회
      const auto num_bars =
          bar_->GetBarData(REFERENCE, timeframe_)->GetNumBars(symbol_idx);
      output_[symbol_idx].resize(num_bars);
      reference_num_bars_[symbol_idx] = num_bars;

      for (int bar_idx = 0; bar_idx < num_bars; bar_idx++) {
        // 현재 심볼의 바 인덱스를 증가시키며 지표 계산
        bar_->SetCurrentBarIndex(bar_idx);

        // 지표 계산 시 타 지표를 사용하는 경우가 있는데,
        // 현재 바에서 타 지표 계산이 안 되어 nan이면 해당 지표 값도 nan이 됨
        output_[symbol_idx][bar_idx] = Calculate();
      }

      // 변경한 바 인덱스를 초기화
      bar_->SetCurrentBarIndex(0);
    }

    is_calculated_ = true;
    is_calculating_ = false;
    logger_->Log(INFO_L,
                 format("[{}] | [{} {}] 지표 계산 완료", strategy_name, name_,
                        timeframe_),
                 __FILE__, __LINE__);
  } catch (const exception& e) {
    logger_->Log(ERROR_L, e.what(), __FILE__, __LINE__);
    Logger::LogAndThrowError(
        format("[{} {}] 지표 계산 중 오류가 발생했습니다.", name_, timeframe_),
        __FILE__, __LINE__);
  }
}

void Indicator::SaveIndicator(const string& indicators_strategy_path) const {
  // 계산 여부를 미리 체크하여 반복 검사 줄이기
  if (!is_calculated_) {
    throw runtime_error("지표 계산 전 저장할 수 없습니다.");
  }

  const auto& bar_data = bar_->GetBarData(REFERENCE, timeframe_);

  // 심볼마다 개별 파일에 직접 기록
  for (size_t symbol_idx = 0; symbol_idx < output_.size(); ++symbol_idx) {
    const auto& symbol_name = bar_data->GetSymbolName(symbol_idx);
    const auto& output = output_[symbol_idx];

    // 파일 경로 구성 (경로 구성 과정에서 불필요한 문자열 연결 연산 줄임)
    const string file_path = indicators_strategy_path + "/" + symbol_name +
                             "/" + name_ + " " + timeframe_ + ".csv";

    ofstream file(file_path, ios::trunc);
    if (!file.is_open()) {
      const string error_msg = file_path + " 파일을 열지 못했습니다.";
      logger_->Log(ERROR_L, error_msg.c_str(), __FILE__, __LINE__);
      Logger::LogAndThrowError(
          format("[{}] [{} {}] 지표를 저장하는 중 오류가 발생했습니다.",
                 symbol_name, name_, timeframe_),
          __FILE__, __LINE__);
    }

    // 헤더 작성 (직접 파일 스트림에 기록)
    file << "Open Time," << name_ << "\n";

    // 고정 소수점 및 소수점 10자리 포맷 지정 (숫자 출력 형식 설정은 한 번만)
    file << fixed << setprecision(10);
    for (size_t bar_idx = 0; bar_idx < output.size(); ++bar_idx) {
      // 각 행을 직접 스트림에 기록 (문자열 연결 대신 연속 << 사용)
      file << UtcTimestampToUtcDatetime(
                  bar_data->GetBar(symbol_idx, bar_idx).open_time)
           << "," << output[bar_idx] << "\n";
    }
    // ofstream은 소멸 시 자동 close되지만, 명시적으로 close를 호출해도
    // 좋습니다.
    file.close();
  }
}

void Indicator::SetTimeframe(const string& timeframe) {
  if (!is_calculated_) {
    timeframe_ = timeframe;
  } else {
    Logger::LogAndThrowError(
        format("[{}] 지표가 계산되었으므로 타임프레임 변경을 할 수 없습니다.",
               name_),
        __FILE__, __LINE__);
  }
}

string Indicator::GetName() const { return name_; }

string Indicator::GetTimeframe() const { return timeframe_; }

void Indicator::IncreaseCreationCounter() { creation_counter_++; }

}  // namespace backtesting::indicator