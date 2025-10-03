// 표준 라이브러리
#include <algorithm>
#include <execution>
#include <format>
#include <future>
#include <memory>
#include <ranges>
#include <thread>

// 외부 라이브러리
#include "arrow/table.h"

// 파일 헤더
#include "Engines/BarHandler.hpp"

// 내부 헤더
#include "Engines/BarData.hpp"
#include "Engines/DataUtils.hpp"
#include "Engines/Exception.hpp"
#include "Engines/Logger.hpp"
#include "Engines/TimeUtils.hpp"

// 네임 스페이스
namespace backtesting {
using namespace bar;
using namespace exception;
using namespace logger;
using namespace utils;
}  // namespace backtesting

namespace backtesting::bar {

BarHandler::BarHandler()
    : current_bar_type_(TRADING), current_symbol_index_(-1) {}
void BarHandler::Deleter::operator()(const BarHandler* p) const { delete p; }

mutex BarHandler::mutex_;
shared_ptr<BarHandler> BarHandler::instance_;

shared_ptr<BarHandler>& BarHandler::GetBarHandler() {
  lock_guard lock(mutex_);  // 스레드에서 안전하게 접근하기 위해 mutex 사용

  // 인스턴스가 생성됐는지 확인
  if (!instance_) {
    // 인스턴스가 생성되지 않았으면 생성 후 저장
    instance_ = shared_ptr<BarHandler>(new BarHandler(), Deleter());
  }

  return instance_;
}

void BarHandler::AddBarData(const string& symbol_name, const string& file_path,
                            const BarType bar_type, const int open_time_column,
                            const int open_column, const int high_column,
                            const int low_column, const int close_column,
                            const int volume_column,
                            const int close_time_column) {
  // 로그용 바 데이터 타입 문자열
  string bar_data_type_str;
  switch (bar_type) {
    case TRADING: {
      bar_data_type_str = "트레이딩 및 참조";
      break;
    }
    case MAGNIFIER: {
      bar_data_type_str = "돋보기";
      break;
    }
    case REFERENCE: {
      bar_data_type_str = "참조";
      break;
    }
    case MARK_PRICE: {
      bar_data_type_str = "마크 가격";
    }
  }

  shared_ptr<arrow::Table> bar_data;
  string bar_data_timeframe;
  try {
    // Parquet 파일 읽기
    bar_data = ReadParquet(file_path);

    // 타임프레임 계산
    bar_data_timeframe = CalculateTimeframe(bar_data, open_time_column);

    // 타임프레임 유효성 검증
    IsValidTimeframeBetweenBars(bar_data_timeframe, bar_type);

    // 데이터 추가
    switch (bar_type) {
      case TRADING: {
        // 데이터 추가
        trading_bar_data_->SetBarData(
            symbol_name, bar_data_timeframe, file_path, bar_data,
            open_time_column, open_column, high_column, low_column,
            close_column, volume_column, close_time_column);

        // 트레이딩 바를 지표 계산용으로도 사용하기 때문에
        // 참조 바 데이터로 추가
        if (const auto& timeframe_it =
                reference_bar_data_.find(bar_data_timeframe);
            timeframe_it == reference_bar_data_.end()) {
          reference_bar_data_[bar_data_timeframe] = make_shared<BarData>();
        }
        reference_bar_data_[bar_data_timeframe]->SetBarData(
            symbol_name, bar_data_timeframe, file_path, bar_data,
            open_time_column, open_column, high_column, low_column,
            close_column, volume_column, close_time_column);

        // 인덱스 심볼 개수 추가
        trading_index_.push_back(0);
        reference_index_[bar_data_timeframe].push_back(0);

        break;
      }

      case MAGNIFIER: {
        // 데이터 추가
        magnifier_bar_data_->SetBarData(
            symbol_name, bar_data_timeframe, file_path, bar_data,
            open_time_column, open_column, high_column, low_column,
            close_column, volume_column, close_time_column);

        // 인덱스 심볼 개수 추가
        magnifier_index_.push_back(0);
        break;
      }

      case REFERENCE: {
        // 데이터 추가
        if (const auto& timeframe_it =
                reference_bar_data_.find(bar_data_timeframe);
            timeframe_it == reference_bar_data_.end()) {
          reference_bar_data_[bar_data_timeframe] = make_shared<BarData>();
        }
        reference_bar_data_[bar_data_timeframe]->SetBarData(
            symbol_name, bar_data_timeframe, file_path, bar_data,
            open_time_column, open_column, high_column, low_column,
            close_column, volume_column, close_time_column);

        // 인덱스 심볼 개수 추가
        reference_index_[bar_data_timeframe].push_back(0);
        break;
      }

      case MARK_PRICE: {
        // 데이터 추가
        mark_price_bar_data_->SetBarData(
            symbol_name, bar_data_timeframe, file_path, bar_data,
            open_time_column, open_column, high_column, low_column,
            close_column, volume_column, close_time_column);

        // 인덱스 심볼 개수 추가
        mark_price_index_.push_back(0);
        break;
      }
    }
  } catch (...) {
    Logger::LogAndThrowError(
        format("{} 바 데이터를 추가하는 중 오류가 발생했습니다.",
               bar_data_type_str),
        __FILE__, __LINE__);
  }

  logger_->Log(
      INFO_L,
      format("[{} - {}] 기간의 [{} {}]이(가) {} 바 데이터로 추가되었습니다.",
             UtcTimestampToUtcDatetime(any_cast<int64_t>(
                 GetCellValue(bar_data, open_time_column, 0))),
             UtcTimestampToUtcDatetime(any_cast<int64_t>(GetCellValue(
                 bar_data, close_time_column, bar_data->num_rows() - 1))),
             symbol_name, bar_data_timeframe, bar_data_type_str),
      __FILE__, __LINE__, true);
}

void BarHandler::AddBarDataBatch(
    const vector<string>& symbol_names, const vector<string>& file_paths,
    const BarType bar_type, const int open_time_column, const int open_column,
    const int high_column, const int low_column, const int close_column,
    const int volume_column, const int close_time_column) {
  if (symbol_names.size() != file_paths.size()) {
    Logger::LogAndThrowError(
        "심볼 이름과 파일 경로의 개수가 일치하지 않습니다.", __FILE__,
        __LINE__);
  }

  if (symbol_names.empty()) {
    return;
  }

  // 로그용 바 데이터 타입 문자열
  string bar_data_type_str;
  switch (bar_type) {
    case TRADING: {
      bar_data_type_str = "트레이딩 및 참조";
      break;
    }
    case MAGNIFIER: {
      bar_data_type_str = "돋보기";
      break;
    }
    case REFERENCE: {
      bar_data_type_str = "참조";
      break;
    }
    case MARK_PRICE: {
      bar_data_type_str = "마크 가격";
    }
  }

  logger_->Log(INFO_L,
               format("{} 바 데이터 추가를 시작합니다.", bar_data_type_str),
               __FILE__, __LINE__, true);

  // 배치로 모든 Parquet 파일을 병렬 읽기 (최적화된 함수 사용)
  const vector<shared_ptr<arrow::Table>>& bar_data_tables =
      ReadParquetBatch(file_paths);

  // 타임프레임 계산 및 검증을 병렬로 수행
  const size_t num_symbols = symbol_names.size();
  const size_t num_threads =
      min(num_symbols, static_cast<size_t>(thread::hardware_concurrency()));

  // 전처리 결과를 저장할 구조체
  struct BarDataInfo {
    string timeframe;
    bool success = false;
    string error_message;
  };

  vector<BarDataInfo> bar_data_infos(num_symbols);
  vector<future<void>> futures;

  // 병렬 전처리 (타임프레임 계산 및 검증)
  const size_t chunk_size = (num_symbols + num_threads - 1) / num_threads;

  for (size_t thread_idx = 0; thread_idx < num_threads; ++thread_idx) {
    size_t start_idx = thread_idx * chunk_size;
    size_t end_idx = min(start_idx + chunk_size, num_symbols);

    if (start_idx >= num_symbols) break;

    futures.emplace_back(async(launch::async, [&, start_idx, end_idx]() {
      for (size_t i = start_idx; i < end_idx; ++i) {
        try {
          if (!bar_data_tables[i]) {
            bar_data_infos[i].success = false;
            bar_data_infos[i].error_message = "파일 읽기 실패";
            continue;
          }

          // 타임프레임 계산
          bar_data_infos[i].timeframe =
              CalculateTimeframe(bar_data_tables[i], open_time_column);

          // 타임프레임 유효성 검증
          IsValidTimeframeBetweenBars(bar_data_infos[i].timeframe, bar_type);

          bar_data_infos[i].success = true;
        } catch (const std::exception& e) {
          bar_data_infos[i].success = false;
          bar_data_infos[i].error_message = e.what();
        } catch (...) {
          bar_data_infos[i].success = false;
          bar_data_infos[i].error_message = "알 수 없는 오류가 발생했습니다.";
        }
      }
    }));
  }

  // 모든 병렬 작업 완료 대기
  for (auto& future : futures) {
    future.wait();
  }

  // 순차적으로 데이터 추가
  for (size_t i = 0; i < num_symbols; ++i) {
    if (!bar_data_infos[i].success) {
      Logger::LogAndThrowError(
          format("심볼 [{}]의 바 데이터 처리 중 오류 발생: {}", symbol_names[i],
                 bar_data_infos[i].error_message),
          __FILE__, __LINE__);
    }

    const auto& symbol_name = symbol_names[i];
    const auto& file_path = file_paths[i];
    const auto& bar_data = bar_data_tables[i];  // 배치 읽기 결과 사용
    const auto& bar_data_timeframe = bar_data_infos[i].timeframe;

    // 데이터 추가
    switch (bar_type) {
      case TRADING: {
        trading_bar_data_->SetBarData(
            symbol_name, bar_data_timeframe, file_path, bar_data,
            open_time_column, open_column, high_column, low_column,
            close_column, volume_column, close_time_column);

        if (const auto& timeframe_it =
                reference_bar_data_.find(bar_data_timeframe);
            timeframe_it == reference_bar_data_.end()) {
          reference_bar_data_[bar_data_timeframe] = make_shared<BarData>();
        }
        reference_bar_data_[bar_data_timeframe]->SetBarData(
            symbol_name, bar_data_timeframe, file_path, bar_data,
            open_time_column, open_column, high_column, low_column,
            close_column, volume_column, close_time_column);

        trading_index_.push_back(0);
        reference_index_[bar_data_timeframe].push_back(0);
        break;
      }

      case MAGNIFIER: {
        magnifier_bar_data_->SetBarData(
            symbol_name, bar_data_timeframe, file_path, bar_data,
            open_time_column, open_column, high_column, low_column,
            close_column, volume_column, close_time_column);
        magnifier_index_.push_back(0);
        break;
      }

      case REFERENCE: {
        if (const auto& timeframe_it =
                reference_bar_data_.find(bar_data_timeframe);
            timeframe_it == reference_bar_data_.end()) {
          reference_bar_data_[bar_data_timeframe] = make_shared<BarData>();
        }
        reference_bar_data_[bar_data_timeframe]->SetBarData(
            symbol_name, bar_data_timeframe, file_path, bar_data,
            open_time_column, open_column, high_column, low_column,
            close_column, volume_column, close_time_column);
        reference_index_[bar_data_timeframe].push_back(0);
        break;
      }

      case MARK_PRICE: {
        mark_price_bar_data_->SetBarData(
            symbol_name, bar_data_timeframe, file_path, bar_data,
            open_time_column, open_column, high_column, low_column,
            close_column, volume_column, close_time_column);
        mark_price_index_.push_back(0);
        break;
      }
    }

    logger_->Log(
        INFO_L,
        format("[{} - {}] 기간의 [{} {}]이(가) {} 바 데이터로 추가되었습니다.",
               UtcTimestampToUtcDatetime(any_cast<int64_t>(
                   GetCellValue(bar_data, open_time_column, 0))),
               UtcTimestampToUtcDatetime(any_cast<int64_t>(GetCellValue(
                   bar_data, close_time_column, bar_data->num_rows() - 1))),
               symbol_name, bar_data_timeframe, bar_data_type_str),
        __FILE__, __LINE__, true);
  }
}

void BarHandler::ProcessBarIndex(const BarType bar_type,
                                 const string& timeframe, const int symbol_idx,
                                 const int64_t target_close_time) {
  const auto& bar_data = GetBarData(bar_type, timeframe);
  auto& bar_indices = GetBarIndices(bar_type, timeframe);
  const auto max_bar_idx = bar_data->GetNumBars(symbol_idx) - 1;

  while (true) {
    size_t& bar_idx = bar_indices[symbol_idx];

    // 최대 인덱스로 이후로는 이동 불가
    if (bar_idx == max_bar_idx) {
      return;
    }

    if (const auto next_close_time =
            bar_data->GetBar(symbol_idx, bar_idx + 1).close_time;
        next_close_time < target_close_time) {
      // 다음 바의 Close Time이 Target Close Time보다 작으면
      // 인덱스 증가 후 반복
      bar_idx++;
    } else if (next_close_time == target_close_time) {
      // 다음 바의 Close Time이 Target Close Time과 같으면 인덱스 증가 후 탈출
      bar_idx++;
      return;
    } else {
      // 다음 바 Close Time이 Target Close Time보다 크면 증가하지 않고 종료
      return;
    }
  }
}

void BarHandler::ProcessBarIndices(const BarType bar_type,
                                   const string& timeframe,
                                   const int64_t target_close_time) {
  for (int symbol_idx = 0;
       symbol_idx < GetBarData(bar_type, timeframe)->GetNumSymbols();
       symbol_idx++) {
    ProcessBarIndex(bar_type, timeframe, symbol_idx, target_close_time);
  }
}

void BarHandler::SetCurrentBarType(const BarType bar_type,
                                   const string& timeframe) {
  current_bar_type_ = bar_type;

  if (bar_type == REFERENCE) {
    IsValidReferenceBarTimeframe(timeframe);

    current_reference_timeframe_ = timeframe;
  }
}

void BarHandler::SetCurrentSymbolIndex(const int symbol_index) {
  current_symbol_index_ = symbol_index;
}

void BarHandler::SetCurrentBarIndex(const size_t bar_index) {
  switch (current_bar_type_) {
    case TRADING: {
      trading_index_[current_symbol_index_] = bar_index;
      return;
    }

    case MAGNIFIER: {
      magnifier_index_[current_symbol_index_] = bar_index;
      return;
    }

    case REFERENCE: {
      reference_index_[current_reference_timeframe_][current_symbol_index_] =
          bar_index;
      return;
    }

    case MARK_PRICE: {
      mark_price_index_[current_symbol_index_] = bar_index;
    }
  }
}

size_t BarHandler::IncreaseBarIndex(const BarType bar_type,
                                    const string& timeframe,
                                    const int symbol_index) {
  switch (bar_type) {
    case TRADING: {
      return ++trading_index_[symbol_index];
    }

    case MAGNIFIER: {
      return ++magnifier_index_[symbol_index];
    }

    case REFERENCE: {
      return ++reference_index_[timeframe][symbol_index];
    }

    case MARK_PRICE: {
      return ++mark_price_index_[symbol_index];
    }
  }

  [[unlikely]] throw;
}

BarType BarHandler::GetCurrentBarType() const { return current_bar_type_; }

string BarHandler::GetCurrentReferenceTimeframe() const {
  return current_reference_timeframe_;
}

int BarHandler::GetCurrentSymbolIndex() const { return current_symbol_index_; }

size_t BarHandler::GetCurrentBarIndex() {
  switch (current_bar_type_) {
    case TRADING: {
      return trading_index_[current_symbol_index_];
    }

    case MAGNIFIER: {
      return magnifier_index_[current_symbol_index_];
    }

    case REFERENCE: {
      return reference_index_[current_reference_timeframe_]
                             [current_symbol_index_];
    }

    case MARK_PRICE: {
      return mark_price_index_[current_symbol_index_];
    }
  }

  return -1;
}

string BarHandler::CalculateTimeframe(const shared_ptr<arrow::Table>& bar_data,
                                      const int open_time_column) {
  const auto num_bars = bar_data->num_rows();

  // 앞뒤 10개 데이터를 비교해서 타임프레임을 구함
  vector<int64_t> time_diffs;

  // 앞 10개 차이 계산
  for (size_t i = 1; i <= 10 && i < num_bars; i++) {
    const auto fst_open_time = any_cast<int64_t>(
        GetCellValue(bar_data, open_time_column, static_cast<int64_t>(i - 1)));
    const auto snd_open_time = any_cast<int64_t>(
        GetCellValue(bar_data, open_time_column, static_cast<int64_t>(i)));
    time_diffs.push_back(snd_open_time - fst_open_time);
  }

  // 뒤 10개 차이 계산
  for (size_t i = num_bars - 1; i >= num_bars - 10 && i > 0; i--) {
    const auto fst_open_time = any_cast<int64_t>(
        GetCellValue(bar_data, open_time_column, static_cast<int64_t>(i - 1)));
    const auto snd_open_time = any_cast<int64_t>(
        GetCellValue(bar_data, open_time_column, static_cast<int64_t>(i)));
    time_diffs.push_back(snd_open_time - fst_open_time);
  }

  // 최빈값 계산
  unordered_map<int64_t, int> freq_map;
  for (const auto& diff : time_diffs) {
    ++freq_map[diff];
  }

  // 최빈값 찾기
  int64_t most_frequent_diff = 0;
  int max_count = 0;
  for (const auto& [diff, count] : freq_map) {
    if (count > max_count) {
      most_frequent_diff = diff;
      max_count = count;
    }
  }

  // 최빈값을 포맷하여 리턴
  return FormatTimeframe(most_frequent_diff);
}

void BarHandler::IsValidTimeframeBetweenBars(const string& timeframe,
                                             const BarType bar_type) {
  const auto parsed_bar_data_tf = ParseTimeframe(timeframe);

  switch (bar_type) {
    case TRADING: {
      if (const string& magnifier_tf = magnifier_bar_data_->GetTimeframe();
          !magnifier_tf.empty()) {
        const auto parsed_magnifier_tf = ParseTimeframe(magnifier_tf);

        if (parsed_magnifier_tf >= parsed_bar_data_tf) {
          Logger::LogAndThrowError(
              format("주어진 트레이딩 바 데이터 타임프레임 [{}]은(는) "
                     "돋보기 바 데이터 타임프레임 [{}]보다 높아야합니다.",
                     timeframe, magnifier_tf),
              __FILE__, __LINE__);
          return;
        }

        if (parsed_bar_data_tf % parsed_magnifier_tf != 0) {
          Logger::LogAndThrowError(
              format("주어진 트레이딩 바 데이터 타임프레임 [{}]은(는) "
                     "돋보기 바 데이터 타임프레임 [{}]의 배수여야 합니다.",
                     timeframe, magnifier_tf),
              __FILE__, __LINE__);
          return;
        }
      }

      for (const auto& reference_tf : views::keys(reference_bar_data_)) {
        const auto parsed_reference_tf = ParseTimeframe(reference_tf);

        if (parsed_reference_tf < parsed_bar_data_tf) {
          Logger::LogAndThrowError(
              format(
                  "주어진 트레이딩 바 데이터 타임프레임 [{}]은(는) "
                  "참조 바 데이터 타임프레임 [{}]와(과) 같거나 낮아야합니다.",
                  timeframe, reference_tf),
              __FILE__, __LINE__);
          return;
        }

        if (parsed_reference_tf % parsed_bar_data_tf != 0) {
          Logger::LogAndThrowError(
              format("주어진 트레이딩 바 데이터 타임프레임 [{}]은(는) "
                     "참조 바 데이터 타임프레임 [{}]의 약수여야 합니다.",
                     timeframe, reference_tf),
              __FILE__, __LINE__);
          return;
        }
      }

      return;
    }

    case MAGNIFIER: {
      if (const string& trading_tf = trading_bar_data_->GetTimeframe();
          !trading_tf.empty()) {
        const auto parsed_trading_tf = ParseTimeframe(trading_tf);

        if (parsed_trading_tf <= parsed_bar_data_tf) {
          Logger::LogAndThrowError(
              format("주어진 돋보기 바 데이터 타임프레임 [{}]은(는) "
                     "트레이딩 바 데이터 타임프레임 [{}]보다 낮아야합니다.",
                     timeframe, trading_tf),
              __FILE__, __LINE__);
          return;
        }

        if (parsed_trading_tf % parsed_bar_data_tf != 0) {
          Logger::LogAndThrowError(
              format("주어진 돋보기 바 데이터 타임프레임 [{}]은(는) "
                     "트레이딩 바 데이터 타임프레임 [{}]의 약수여야 합니다.",
                     timeframe, trading_tf),
              __FILE__, __LINE__);
          return;
        }
      }

      for (const auto& reference_tf : views::keys(reference_bar_data_)) {
        if (ParseTimeframe(reference_tf) <= parsed_bar_data_tf) {
          Logger::LogAndThrowError(
              format("주어진 돋보기 바 데이터 타임프레임 [{}]은(는) "
                     "참조 바 데이터 타임프레임 [{}]보다 낮아야합니다.",
                     timeframe, reference_tf),
              __FILE__, __LINE__);
          return;
        }
      }

      return;
    }

    case REFERENCE: {
      if (const auto& trading_tf = trading_bar_data_->GetTimeframe();
          !trading_tf.empty()) {
        const auto parsed_trading_tf = ParseTimeframe(trading_tf);

        if (parsed_trading_tf > parsed_bar_data_tf) {
          Logger::LogAndThrowError(
              format("주어진 참조 바 데이터 타임프레임 [{}]은(는) 트레이딩 바 "
                     "데이터 타임프레임 [{}]와(과) 같거나 높아야합니다.",
                     timeframe, trading_tf),
              __FILE__, __LINE__);
          return;
        }

        if (parsed_bar_data_tf % parsed_trading_tf != 0) {
          Logger::LogAndThrowError(
              format("주어진 참조 바 데이터 타임프레임 [{}]은(는) 트레이딩 바 "
                     "데이터 타임프레임 [{}]의 배수여야 합니다.",
                     timeframe, trading_tf),
              __FILE__, __LINE__);
          return;
        }
      }

      if (const auto& magnifier_tf = magnifier_bar_data_->GetTimeframe();
          !magnifier_tf.empty() &&
          ParseTimeframe(magnifier_tf) >= parsed_bar_data_tf) {
        Logger::LogAndThrowError(
            format("주어진 참조 바 데이터 타임프레임 [{}]은(는) 돋보기 바 "
                   "데이터 타임프레임 [{}]보다 높아야합니다.",
                   timeframe, magnifier_tf),
            __FILE__, __LINE__);
      }

      return;
    }

    case MARK_PRICE: {
      // 바 데이터 추가 시점에는 돋보기 기능 사용 여부를 알 수 없으므로
      // 트레이딩 혹은 돋보기 타임프레임 중 하나만 같아도 일단 통과
      const auto& trading_tf = trading_bar_data_->GetTimeframe();
      if (const auto& magnifier_tf = magnifier_bar_data_->GetTimeframe();
          !trading_tf.empty() && !magnifier_tf.empty()) {
        if (ParseTimeframe(trading_tf) != parsed_bar_data_tf &&
            ParseTimeframe(magnifier_tf) != parsed_bar_data_tf) {
          Logger::LogAndThrowError(
              format("주어진 마크 가격 바 데이터 타임프레임 [{}]은(는) "
                     "트레이딩 바 데이터 타임프레임 [{}] 혹은 돋보기 바 "
                     "타임프레임 [{}]와(과) 같아야 합니다.",
                     timeframe, trading_tf, magnifier_tf),
              __FILE__, __LINE__);
        }
      }
    }
  }
}

void BarHandler::IsValidReferenceBarTimeframe(const string& timeframe) {
  if (const auto& timeframe_it = reference_bar_data_.find(timeframe);
      timeframe_it == reference_bar_data_.end()) {
    Logger::LogAndThrowError(
        format("참조 바 데이터에 타임프레임 {}은(는) 존재하지 않습니다.",
               timeframe),
        __FILE__, __LINE__);
  }
}

}  // namespace backtesting::bar