// 표준 라이브러리
#include <cmath>
#include <format>
#include <iomanip>

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
using namespace data_utils;
using namespace time_utils;

Indicator::Indicator(const string& name, const string& timeframe)
    : is_calculated_(false) {
  try {
    if (name.empty()) {
      logger_->LogAndThrowError("지표 이름이 비어있습니다.", __FILE__,
                                __LINE__);
    }

    if (timeframe.empty()) {
      logger_->LogAndThrowError(
          format("[{}] 지표의 타임프레임이 비어있습니다.", name), __FILE__,
          __LINE__);
    }

    name_ = name;
    timeframe_ = timeframe;
  } catch ([[maybe_unused]] const exception& e) {
    logger_->LogAndThrowError("지표 생성 중 오류가 발생했습니다.", __FILE__,
                              __LINE__);
  }
}
Indicator::~Indicator() = default;

vector<reference_wrapper<Indicator>> Indicator::indicators_;
bool Indicator::is_calculating_ = false;
string Indicator::calculating_name_;

shared_ptr<Analyzer>& Indicator::analyzer_ = Analyzer::GetAnalyzer();
shared_ptr<BarHandler>& Indicator::bar_ = BarHandler::GetBarHandler();
shared_ptr<Engine>& Indicator::engine_ = Engine::GetEngine();
shared_ptr<Logger>& Indicator::logger_ = Logger::GetLogger();

Numeric<double> Indicator::operator[](const size_t index) {
  if (!is_calculated_) {
    CalculateIndicator();
  }

  // =======================================================================
  // 원래 사용중인 데이터 정보 저장
  const auto original_bar_type = bar_->GetCurrentBarType();
  const auto& original_reference_tf = bar_->GetCurrentReferenceTimeframe();

  // 특정 지표 계산 중 해당 지표와 다른 타임프레임의 지표 사용 시 에러 발생
  // -> 특정 지표 내 사용하는 지표의 타임프레임은
  //    해당 지표의 타임프레임과 일치해야 함
  if (is_calculating_ && original_reference_tf != timeframe_) {
    Logger::LogAndThrowError(
        format("{} 지표 계산에 사용하는 {} 지표의 타임프레임 {}은(는) "
               "{} 지표의 타임프레임 {}와(과) 동일해야 합니다.",
               calculating_name_, name_, timeframe_, calculating_name_,
               original_reference_tf),
        __FILE__, __LINE__);
  }

  // 지표 참조를 위해 데이터 환경 설정
  bar_->SetCurrentBarType(BarType::REFERENCE, timeframe_);
  const auto symbol_idx = bar_->GetCurrentSymbolIndex();
  const auto bar_index = bar_->GetCurrentBarIndex();

  // 원래 사용 중이던 데이터 환경으로 복구
  bar_->SetCurrentBarType(original_bar_type, original_reference_tf);

  // 진행한 인덱스보다 과거 인덱스 참조 시 NaN을 반환
  if (index > bar_index) {
    return nan("");
  }

  // 결과값 반환
  return output_[symbol_idx][bar_index - index];
}

void Indicator::CalculateIndicator() {
  try {
    // 타 지표에서 다른 지표 참조 시 미계산 됐으면 자동 계산 후 참조하는데,
    // 이러한 상황에서 중복 계산을 방지하기 위함
    if (is_calculated_) {
      return;
    }

    // 엔진이 초기화되기 전 지표를 계산하면 모든 심볼의 계산이 어려우므로 에러
    if (!engine_->IsEngineInitialized()) {
      Logger::LogAndThrowError(
          format("엔진 초기화 전 [{} {}] 지표 계산을 시도했습니다.", name_,
                 timeframe_),
          __FILE__, __LINE__);
    }
    // ===========================================================================

    // 원본 설정을 저장
    const auto original_bar_type = bar_->GetCurrentBarType();
    const auto original_reference_tf = bar_->GetCurrentReferenceTimeframe();
    const auto original_symbol_idx = bar_->GetCurrentSymbolIndex();

    // 사전 설정
    const auto& reference_bar =
        bar_->GetBarData(BarType::REFERENCE, timeframe_);
    is_calculating_ = true;
    calculating_name_ = name_;
    output_.resize(reference_bar.GetNumSymbols());
    bar_->SetCurrentBarType(BarType::REFERENCE, timeframe_);

    // 전체 트레이딩 심볼들을 순회하며 지표 계산
    for (int symbol_idx = 0; symbol_idx < output_.size(); symbol_idx++) {
      this->Initialize();
      bar_->SetCurrentSymbolIndex(symbol_idx);

      // 해당 심볼의 모든 바를 순회
      const auto num_bars = reference_bar.GetNumBars(symbol_idx);
      output_[symbol_idx].resize(num_bars);

      for (int bar_idx = 0; bar_idx < num_bars; bar_idx++) {
        // 현재 심볼의 바 인덱스를 증가시키며 지표 계산
        bar_->SetCurrentBarIndex(bar_idx);

        /* 지표 계산 시 타 지표를 사용하는 경우가 있는데,
           현재 바에서 타 지표 계산이 안 되어 nan이면 해당 지표 값도 nan이 됨 */
        output_[symbol_idx][bar_idx] = Calculate();
      }

      // 변경한 바 인덱스를 초기화
      bar_->SetCurrentBarIndex(0);
    }

    is_calculated_ = true;
    logger_->Log(LogLevel::INFO_L,
                 format("모든 심볼의 [{} {}] 지표 계산이 완료되었습니다.",
                        name_, timeframe_),
                 __FILE__, __LINE__);

    // 원본 설정을 복원
    is_calculating_ = false;
    bar_->SetCurrentBarType(original_bar_type, original_reference_tf);
    bar_->SetCurrentSymbolIndex(original_symbol_idx);
  } catch (...) {
    Logger::LogAndThrowError(
        format("[{} {}] 지표 계산 중 오류가 발생했습니다.", name_, timeframe_),
        __FILE__, __LINE__);
  }
}

void Indicator::SaveIndicator() const {
  const auto& trading_bar = bar_->GetBarData(BarType::TRADING);
  const auto& file_path = engine_->GetMainDirectory() +
                          format("/Indicators/{} {}.csv", name_, timeframe_);

  try {
    if (!is_calculated_) {
      throw runtime_error("지표 계산 전 저장할 수 없습니다.");
    }

    // 파일 출력 스트림 열기
    ofstream file(file_path, ios::trunc);  // trunc 옵션으로 파일 내용 초기화

    // 파일 열기 실패 시 에러 출력
    if (!file.is_open()) {
      throw runtime_error(file_path + " 파일을 열지 못했습니다.");
    }

    // 최대 15자리 소수점 저장
    file << fixed << setprecision(15);

    size_t max_num_bars = 0;
    for (int symbol_idx = 0; symbol_idx < output_.size(); symbol_idx++) {
      // 심볼 이름들로 파일 헤더를 추가
      file << trading_bar.GetSymbolName(symbol_idx);

      if (symbol_idx != output_.size() - 1) {
        file << ',';  // 마지막 symbol_name 뒤에는 쉼표를 추가하지 않음
      }

      // 심볼 중 최대 바 인덱스 크기 계산
      if (const auto num_bars = trading_bar.GetNumBars(symbol_idx);
          num_bars > max_num_bars) {
        max_num_bars = num_bars;
      }
    }

    file << '\n';  // 헤더 끝난 후 한 줄 개행

    // 각 심볼의 지표값을 한 줄씩 쓰기
    for (size_t bar_idx = 0; bar_idx < max_num_bars; bar_idx++) {
      for (const auto& output : output_) {
        try {
          // 예외를 발생시키기 위해 at을 사용
          file << output.at(bar_idx);
        } catch ([[maybe_unused]] const exception& e) {
          // 심볼별로 바 개수가 다르므로 최대 개수에 도달하면 저장하지 않음
        }

        file << ',';  // 값 사이에 쉼표 추가
      }

      // 하나의 바 인덱스 기록이 완료되었으면 개행
      file << '\n';
    }

    // 파일 닫기
    file.close();
  } catch (const exception& e) {
    logger_->Log(LogLevel::ERROR_L, e.what(), __FILE__, __LINE__);
    Logger::LogAndThrowError(
        format("[{} {}] 지표를 {} 경로에 저장하는 중 에러가 발생했습니다.",
               name_, timeframe_, file_path),
        __FILE__, __LINE__);
  }

  logger_->Log(LogLevel::INFO_L,
               format("[{} {}] 지표가 저장되었습니다.", name_, timeframe_),
               __FILE__, __LINE__);
}

void Indicator::SetTimeframe(const string& timeframe) {
  if (!is_calculated_) {
    timeframe_ = timeframe;
  } else {
    logger_->LogAndThrowError(
        format(
            "[{}] 지표가 계산 완료되었으므로 타임프레임 변경을 할 수 없습니다.",
            name_),
        __FILE__, __LINE__);
  }
}

vector<reference_wrapper<Indicator>> Indicator::GetIndicators() {
  return indicators_;
}

string Indicator::GetName() const { return name_; }

string Indicator::GetTimeframe() const { return timeframe_; }
