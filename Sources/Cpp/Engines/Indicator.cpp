// 표준 라이브러리
#include <algorithm>   // 최적화 알고리즘용
#include <filesystem>  // 파일 존재 여부 확인용 추가
#include <format>
#include <regex>

// 파일 헤더
#include "Engines/Indicator.hpp"

// 내부 헤더
#include "Engines/Analyzer.hpp"
#include "Engines/BarData.hpp"
#include "Engines/BarHandler.hpp"
#include "Engines/BaseBarHandler.hpp"
#include "Engines/DataUtils.hpp"
#include "Engines/Engine.hpp"

// 네임 스페이스
using namespace backtesting::utils;
namespace fs = std::filesystem;

namespace backtesting::indicator {

Indicator::Indicator(const string& name, const string& timeframe,
                     const Plot& plot)
    : is_calculated_(false), is_higher_timeframe_indicator_(false) {
  try {
    if (name.empty()) {
      Logger::LogAndThrowError("지표 이름이 비어있습니다.", __FILE__, __LINE__);
    }

    if (timeframe.empty()) {
      Logger::LogAndThrowError(
          format("[{}] 지표의 타임프레임이 비어있습니다.", name), __FILE__,
          __LINE__);
    }
  } catch ([[maybe_unused]] const exception& e) {
    Logger::LogAndThrowError("지표를 추가하는 중 오류가 발생했습니다.",
                             __FILE__, __LINE__);
  }

  // 증가 카운터는 AddIndicator 함수로만 증가하는데 AddIndicator 없이 직접
  // 생성자 호출로 전 증가 카운터가 현재 증가 카운터와 같다면 오류 발생
  if (pre_creation_counter_ == creation_counter_) {
    logger_->Log(ERROR_L,
                 "지표의 추가는 AddIndicator 함수의 호출로만 가능합니다.",
                 __FILE__, __LINE__, true);
    Logger::LogAndThrowError(
        format("[{} {}] 지표를 추가하는 중 에러가 발생했습니다.", name,
               timeframe),
        __FILE__, __LINE__);
  }

  // 정상적으로 AddIndicator 함수를 통했다면 전 증가 가운터에 현재 카운터를 대입
  pre_creation_counter_ = creation_counter_;

  // 클래스 변수 설정
  name_ = name;
  timeframe_ = timeframe;

  plot_type_ = ExtractClassName(typeid(plot).name());
  plot_ = plot.MakeShared();
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
vector<string> Indicator::saved_indicator_classes_;

Numeric<long double> Indicator::operator[](const size_t index) {
  // =========================================================================
  // 사전 검증
  // =========================================================================

  // 지표 계산 전 참조 호출 시 에러 발생
  // 특정 지표 계산 중 다른 지표 참조하는데 참조 지표의 정의 순서가 더 늦는 경우
  if (!is_calculated_) [[unlikely]] {
    throw runtime_error(
        format("[{} {}] 지표 계산에 사용하는 [{} {}] 지표가 [{} {}] 지표보다 "
               "먼저 정의되지 않아 계산되지 않았으므로 참조할 수 없습니다.",
               calculating_name_, calculating_timeframe_, name_, timeframe_,
               calculating_name_, calculating_timeframe_));
  }

  // 특정 지표 계산 중 해당 지표와 다른 타임프레임의 지표 사용 시 에러 발생
  if (is_calculating_ && timeframe_ != calculating_timeframe_) [[unlikely]] {
    throw runtime_error(
        format("[{} {}] 지표 계산에 사용하는 [{} {}] 지표의 타임프레임은 "
               "[{} {}] 지표의 타임프레임과 동일해야 합니다.",
               calculating_name_, calculating_timeframe_, name_, timeframe_,
               calculating_name_, calculating_timeframe_));
  }

  // 음수 index는 size_t 타입이므로 검사하지 않음
  // 음수로 전달된 경우 최대값이 되므로 index 범위 검사에서 NaN 반환

  // =========================================================================
  // CASE 1: 지표 계산 시 타 지표 참조
  // - 원본 바 타입: REFERENCE, 원본 타임프레임: 계산 중인 지표의 타임프레임
  // - 계산 중인 지표와 같은 타임프레임의 지표인 케이스만 존재
  //   (다른 타임프레임은 사전 검증에서 에러 발생)
  // =========================================================================
  if (is_calculating_) [[likely]] {
    const auto bar_idx = bar_->GetCurrentBarIndex();

    // 범위 검사
    if (index > bar_idx) [[unlikely]] {
      return nanf("");
    }

    const auto target_bar_idx = bar_idx - index;
    const auto symbol_idx = bar_->GetCurrentSymbolIndex();

    return output_[symbol_idx][target_bar_idx];
  }

  // =========================================================================
  // CASE 2-1: 전략 실행 시 지표 참조
  //           트레이딩 바 타임프레임과 같은 타임프레임의 지표인 경우
  // - 원본 바 타입: TRADING, 원본 타임프레임: NONE
  // =========================================================================
  if (!is_higher_timeframe_indicator_) [[likely]] {
    const auto bar_idx = bar_->GetCurrentBarIndex();

    // 범위 검사
    if (index > bar_idx) [[unlikely]] {
      return nanf("");
    }

    const auto target_bar_idx = bar_idx - index;
    const auto symbol_idx = bar_->GetCurrentSymbolIndex();

    return output_[symbol_idx][target_bar_idx];
  }

  // =========================================================================
  // CASE 2-2: 전략 실행 시 지표 참조
  //           트레이딩 바 타임프레임보다 큰 타임프레임의 지표인 경우
  // - 원본 바 타입: TRADING, 원본 타임프레임: NONE
  // - 현재 바 타입과 타임프레임 캐시
  // - Close Time 기반 복잡한 계산 필요
  // =========================================================================

  // 호출 시점의 데이터 환경 정보 저장
  const auto symbol_idx = bar_->GetCurrentSymbolIndex();
  const auto trading_bar_idx = bar_->GetCurrentBarIndex();

  // 범위 검사
  if (index > trading_bar_idx) [[unlikely]] {
    return nanf("");
  }

  const size_t target_trading_bar_idx = trading_bar_idx - index;

  // 캐시 히트 체크
  if (cached_symbol_idx_ == symbol_idx &&
      cached_trading_bar_idx_ == trading_bar_idx &&
      cached_target_bar_idx_ == target_trading_bar_idx) [[likely]] {
    // 캐시된 결과가 NaN인 경우 (해당되는 Close Time이 없는 경우)
    if (cached_ref_bar_idx_ == SIZE_MAX) [[unlikely]] {
      return nanf("");
    }

    return output_[symbol_idx][cached_ref_bar_idx_];
  }

  // 캐시 미스 - 새로 계산
  // =========================================================================
  // CASE 2-2 상세 로직: Close Time 기반 참조 바 인덱스 찾기
  // =========================================================================
  // 목표: 트레이딩 바의 특정 시점(target_trading_bar_idx)에서
  //       해당 시점의 Close Time과 같거나 작은 상위 타임프레임 바 찾기
  //
  // 예시: 1분봉 전략에서 5분봉 지표 참조하는 경우
  // - 1분봉 100번째 바 (예: 10:05 close)에서 5분봉 지표[2] 참조
  // - 1분봉 98번째 바 (예: 10:03 close) 시점의 5분봉 값을 찾아야 함
  // - 5분봉에서 10:03보다 작거나 같은 Close Time을 가진 마지막 바를 찾음
  // =========================================================================

  // 대상 close_time 계산 (target_trading_bar_idx 시점의 close time)
  const int64_t target_close_time =
      trading_bar_data_->GetBar(symbol_idx, target_trading_bar_idx).close_time;

  // 해당 시점에서 사용 가능했던 상위 TF 바 인덱스 계산
  bar_->SetCurrentBarType(REFERENCE, timeframe_);
  size_t ref_bar_idx = bar_->GetCurrentBarIndex();

  // =========================================================================
  // 1단계: 역방향 검색 (현재 -> 과거)
  // 현재 참조 바가 target_close_time보다 미래에 있다면 과거로 이동
  // =========================================================================
  while (ref_bar_idx > 0 &&
         reference_bar_data_->GetBar(symbol_idx, ref_bar_idx).close_time >
             target_close_time) [[likely]] {
    --ref_bar_idx;
  }

  // =========================================================================
  // 예외 처리: 인덱스 0까지 갔는데도 target_close_time보다 큰 경우
  // 이는 target_close_time이 모든 참조 바보다 과거인 경우 -> NaN 반환
  // =========================================================================
  if (ref_bar_idx == 0 &&
      reference_bar_data_->GetBar(symbol_idx, 0).close_time > target_close_time)
      [[unlikely]] {
    // 결과 캐싱 (NaN 케이스도 캐싱하여 반복 계산 방지)
    cached_symbol_idx_ = symbol_idx;
    cached_trading_bar_idx_ = trading_bar_idx;
    cached_target_bar_idx_ = target_trading_bar_idx;
    cached_ref_bar_idx_ = SIZE_MAX;  // NaN 표시

    // 원래 데이터 환경 복구
    bar_->SetCurrentBarType(TRADING, "");

    return nanf("");
  }

  // =========================================================================
  // 2단계: 순방향 검색 (과거 -> 현재)
  // target_close_time과 정확히 일치하는 다음 바가 있는지 확인
  // 정확히 일치하는 바가 있다면 그 바를 사용 (더 정확한 시점)
  // =========================================================================
  const auto num_bars = reference_num_bars_[symbol_idx];
  while (ref_bar_idx + 1 < num_bars) [[likely]] {
    const auto next_close_time =
        reference_bar_data_->GetBar(symbol_idx, ref_bar_idx + 1).close_time;

    if (next_close_time < target_close_time) {
      // 다음 바도 target_close_time보다 과거 -> 더 가까운 바로 이동
      ++ref_bar_idx;
    } else if (next_close_time == target_close_time) {
      // 정확히 일치하는 바 발견 -> 해당 바 사용
      ++ref_bar_idx;
      break;
    } else {
      // 다음 바가 target_close_time보다 미래 -> 현재 바가 최적
      break;
    }
  }

  // 결과 캐싱
  cached_symbol_idx_ = symbol_idx;
  cached_trading_bar_idx_ = trading_bar_idx;
  cached_target_bar_idx_ = target_trading_bar_idx;
  cached_ref_bar_idx_ = ref_bar_idx;

  // 원래 데이터 환경 복구 (전략 실행 중)
  bar_->SetCurrentBarType(TRADING, "");

  return output_[symbol_idx][ref_bar_idx];
}

void Indicator::CalculateIndicator() {
  try {
    // 엔진이 초기화되기 전 지표를 계산하면 모든 심볼의 계산이 어려우므로 에러
    if (!engine_->IsEngineInitialized()) [[unlikely]] {
      Logger::LogAndThrowError(
          format("엔진 초기화 전 [{} {}] 지표 계산을 시도했습니다.", name_,
                 timeframe_),
          __FILE__, __LINE__);
    }

    // 바 데이터 설정
    if (trading_bar_data_ == nullptr) {
      trading_bar_data_ = bar_->GetBarData(TRADING, "");
    }

    if (reference_bar_data_ == nullptr) {
      reference_bar_data_ = bar_->GetBarData(REFERENCE, timeframe_);
    }

    // 캐시 무효화 - 새로운 계산 시작 (메모리 효율적으로 초기화)
    cached_symbol_idx_ = -1;
    cached_trading_bar_idx_ = -1;
    cached_target_bar_idx_ = -1;
    cached_ref_bar_idx_ = -1;

    // ===========================================================================
    // 사전 설정 - 공통 변수들 미리 캐시
    const int num_symbols = reference_bar_data_->GetNumSymbols();

    // 계산 상태 설정
    is_calculating_ = true;
    calculating_name_ = name_;
    calculating_timeframe_ = timeframe_;

    // 메모리 미리 할당으로 리얼로케이션 방지 - 효율적인 메모리 관리
    output_.clear();
    output_.reserve(num_symbols);
    output_.resize(num_symbols);

    reference_num_bars_.clear();
    reference_num_bars_.reserve(num_symbols);
    reference_num_bars_.resize(num_symbols);

    // 바 타입 한 번만 설정
    bar_->SetCurrentBarType(REFERENCE, timeframe_);

    // 전체 트레이딩 심볼들을 순회하며 지표 계산
    for (int symbol_idx = 0; symbol_idx < num_symbols; ++symbol_idx) {
      // 심볼 인덱스 설정
      bar_->SetCurrentSymbolIndex(symbol_idx);

      // 해당 심볼의 바 개수 미리 계산 및 캐시
      const auto num_bars = reference_bar_data_->GetNumBars(symbol_idx);
      reference_num_bars_[symbol_idx] = num_bars;

      // 메모리 미리 할당 (리얼로케이션 방지) - cache-friendly 패턴
      auto& symbol_output = output_[symbol_idx];
      symbol_output.clear();
      symbol_output.reserve(num_bars);
      symbol_output.resize(num_bars);

      // 초기화 - 심볼별로 한 번만 호출
      this->Initialize();

      // 해당 심볼의 모든 바를 순회
      // 컴파일러 최적화를 위한 지역 변수 사용
      for (int bar_idx = 0; bar_idx < num_bars; ++bar_idx) {
        // 현재 심볼의 바 인덱스를 증가시키며 지표 계산
        bar_->SetCurrentBarIndex(bar_idx);

        // 지표 계산 - 메모리 접근 최적화
        symbol_output[bar_idx] = this->Calculate();
      }

      // 바 인덱스 초기화
      bar_->SetCurrentBarIndex(0);
    }

    // 상태 정리
    is_calculated_ = true;
    is_calculating_ = false;

    // 로그 출력 - 성공 메시지
    logger_->Log(INFO_L, format("[{} {}] 지표 계산 완료", name_, timeframe_),
                 __FILE__, __LINE__, true);
  } catch (const exception& e) {
    // 예외 발생 시 상태 정리 - 안전한 상태 복구
    is_calculating_ = false;

    logger_->Log(ERROR_L, e.what(), __FILE__, __LINE__, true);
    Logger::LogAndThrowError(
        format("[{} {}] 지표 계산 중 오류가 발생했습니다.", name_, timeframe_),
        __FILE__, __LINE__);
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

void Indicator::SetHigherTimeframeIndicator() {
  is_higher_timeframe_indicator_ = true;
}

void Indicator::SetSourcePath(const string& source_path) {
  if (!filesystem::exists(source_path)) {
    Logger::LogAndThrowError(
        format("[{}] 지표의 소스 파일 경로 [{}]이(가) 존재하지 않습니다.",
               name_, source_path),
        __FILE__, __LINE__);
  }

  cpp_file_path_ = source_path;
}

void Indicator::SetHeaderPath(const string& header_path) {
  if (!filesystem::exists(header_path)) {
    Logger::LogAndThrowError(
        format("[{}] 지표의 헤더 파일 경로 [{}]이(가) 존재하지 않습니다.",
               name_, header_path),
        __FILE__, __LINE__);
  }

  header_file_path_ = header_path;
}

string Indicator::GetName() const { return name_; }

string Indicator::GetClassName() const { return class_name_; }

string Indicator::GetTimeframe() const { return timeframe_; }

void Indicator::IncreaseCreationCounter() { creation_counter_++; }

bool Indicator::SetFilePath(const string& path, const bool is_cpp) {
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
    logger_->Log(
        ERROR_L,
        format("[{}] 지표의 소스 파일 경로 감지가 실패했습니다.", name_),
        __FILE__, __LINE__, true);
    Logger::LogAndThrowError(
        "지표의 클래스명과 소스 파일명은 동일해야 하며, "
        "[루트 폴더/Sources/cpp/Indicators/클래스명.cpp] 경로에 존재해야 "
        "합니다.",
        __FILE__, __LINE__);
  } else {
    logger_->Log(
        ERROR_L,
        format("[{}] 지표의 헤더 파일 경로 감지가 실패했습니다.", name_),
        __FILE__, __LINE__, true);
    Logger::LogAndThrowError(
        "지표의 클래스명과 헤더 파일명은 동일해야 하며, "
        "[루트 폴더/Includes/Indicators/클래스명.hpp] 경로에 존재해야 합니다.",
        __FILE__, __LINE__);
  }

  return false;
}

bool Indicator::IsIndicatorClassSaved(const string& class_name) {
  return ranges::find(saved_indicator_classes_, class_name) !=
         saved_indicator_classes_.end();
}

void Indicator::AddSavedIndicatorClass(const string& class_name) {
  if (!IsIndicatorClassSaved(class_name)) {
    saved_indicator_classes_.push_back(class_name);
  }
}

string Indicator::GetSourcePath() { return cpp_file_path_; }

string Indicator::GetHeaderPath() { return header_file_path_; }

}  // namespace backtesting::indicator