// 표준 라이브러리
#include <memory>
#include <ranges>

// 파일 헤더
#include "Engines/BarHandler.hpp"

// 내부 헤더
#include "Engines/BarData.hpp"
#include "Engines/DataUtils.hpp"
#include "Engines/Logger.hpp"
#include "Engines/TimeUtils.hpp"

// 네임 스페이스
using namespace data_utils;
using namespace time_utils;

BarHandler::BarHandler()
    : is_strategy_created_(false),
      current_bar_type_(BarType::TRADING),
      current_symbol_index_(-1) {}
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
                            const BarType bar_type,
                            const vector<int>& columns) {
  // Parquet 파일 읽기
  const auto& bar_data = ReadParquet(file_path);

  // 타임프레임 계산
  const auto& bar_data_timeframe = CalculateTimeframe(bar_data, columns[0]);

  // 로그용 바 데이터 타입 문자열
  string bar_data_type_str;
  if (bar_type == BarType::TRADING) {
    bar_data_type_str = "트레이딩 및 참조";
  } else if (bar_type == BarType::MAGNIFIER) {
    bar_data_type_str = "돋보기";
  } else if (bar_type == BarType::REFERENCE) {
    bar_data_type_str = "참조";
  }

  // 데이터 추가
  try {
    switch (bar_type) {
      case BarType::TRADING: {
        // 트레이딩 바 추가가 가능한지 검증
        IsTradingBarAddable();

        // 데이터 추가
        trading_bar_.SetBarData(symbol_name, bar_data_timeframe, bar_data,
                                columns);

        // 트레이딩 바를 지표 계산용으로 사용하기 때문에 참조 바 데이터로도 추가
        reference_bar_[bar_data_timeframe].SetBarData(
            symbol_name, bar_data_timeframe, bar_data, columns);

        // 인덱스 심볼 개수 추가
        trading_index_.push_back(0);
        reference_index_[bar_data_timeframe].push_back(0);

        break;
      }

      case BarType::MAGNIFIER: {
        // 데이터 추가
        magnifier_bar_.SetBarData(symbol_name, bar_data_timeframe, bar_data,
                                  columns);

        // 인덱스 심볼 개수 추가
        magnifier_index_.push_back(0);
        break;
      }

      case BarType::REFERENCE: {
        auto& reference_bar = reference_bar_[bar_data_timeframe];

        // 데이터 추가
        reference_bar.SetBarData(symbol_name, bar_data_timeframe, bar_data,
                                 columns);

        // 인덱스 심볼 개수 추가
        reference_index_[bar_data_timeframe].push_back(0);
        break;
      }
    }

    // 타임프레임 유효성 검증
    IsValidTimeframeBetweenBars(bar_data_timeframe, bar_type);

  } catch (...) {
    Logger::LogAndThrowError(
        format("{} {}을(를) {} 바 데이터로 추가하는 중 오류가 발생했습니다.",
               symbol_name, bar_data_timeframe, bar_data_type_str),
        __FILE__, __LINE__);
  }

  logger_->Log(
      LogLevel::INFO_L,
      format("[{} - {}] 기간의 {} {}이(가) {} 바 데이터로 추가되었습니다.",
             UtcTimestampToUtcDatetime(
                 any_cast<int64_t>(GetCellValue(bar_data, columns[0], 0))),
             UtcTimestampToUtcDatetime(any_cast<int64_t>(
                 GetCellValue(bar_data, columns[6], bar_data->num_rows() - 1))),
             symbol_name, bar_data_timeframe, bar_data_type_str),
      __FILE__, __LINE__);
}

void BarHandler::ProcessBarIndex(const int symbol_idx, const BarType bar_type,
                                 const string& timeframe,
                                 const int64_t base_close_time) {
  const auto& target_bar_data = GetBarData(bar_type, timeframe);
  auto& target_index = GetIndexVector(bar_type, timeframe);

  try {
    // 현재 Close Time이 Base Close Time보다 작을 때만 인덱스 증가 가능
    while (target_bar_data.GetCloseTime(symbol_idx, target_index[symbol_idx]) <
           base_close_time) {
      /* 다음 바의 Close Time이 Base Close Time보다 작거나 같을 때만
         인덱스 증가 */
      if (const auto next_close_time = target_bar_data.GetCloseTime(
              symbol_idx, target_index[symbol_idx] + 1);
          next_close_time <= base_close_time) {
        target_index[symbol_idx]++;
      } else {
        // 다음 바 Close Time이 Base Close Time보다 크면 증가하지 않고 종료
        break;
      }
    }
  } catch (...) {
    /* next_close_time이 바 데이터의 범위를 넘으면
       최대 인덱스로 이동한 것이므로 return */
    return;
  }
}

void BarHandler::ProcessBarIndices(const BarType bar_type,
                                   const string& timeframe,
                                   const int64_t base_close_time) {
  for (int i = 0; i < GetBarData(bar_type, timeframe).GetNumSymbols(); i++) {
    ProcessBarIndex(i, bar_type, timeframe, base_close_time);
  }
}

void BarHandler::SetCurrentBarType(const BarType bar_type,
                                   const string& timeframe) {
  current_bar_type_ = bar_type;

  if (bar_type == BarType::REFERENCE) {
    IsValidReferenceBarTimeframe(timeframe);

    current_reference_timeframe_ = timeframe;
  }
}

void BarHandler::SetCurrentSymbolIndex(const int symbol_index) {
  current_symbol_index_ = symbol_index;
}

void BarHandler::SetCurrentBarIndex(const size_t bar_index) {
  switch (current_bar_type_) {
    case BarType::TRADING: {
      trading_index_[current_symbol_index_] = bar_index;
      return;
    }

    case BarType::MAGNIFIER: {
      magnifier_index_[current_symbol_index_] = bar_index;
      return;
    }

    case BarType::REFERENCE: {
      reference_index_[current_reference_timeframe_][current_symbol_index_] =
          bar_index;
    }
  }
}

void BarHandler::IncrementBarIndex(const BarType bar_type,
                                   const string& timeframe,
                                   const int symbol_index) {
  switch (bar_type) {
    case BarType::TRADING: {
      trading_bar_.IsValidSymbolIndex(symbol_index);

      trading_index_[symbol_index]++;
    }

    case BarType::MAGNIFIER: {
      magnifier_bar_.IsValidSymbolIndex(symbol_index);

      magnifier_index_[symbol_index]++;
    }

    case BarType::REFERENCE: {
      IsValidReferenceBarTimeframe(timeframe);
      reference_bar_[timeframe].IsValidSymbolIndex(symbol_index);

      reference_index_[timeframe][symbol_index]++;
    }
  }
}

BarType BarHandler::GetCurrentBarType() const { return current_bar_type_; }

string BarHandler::GetCurrentReferenceTimeframe() const {
  return current_reference_timeframe_;
}

int BarHandler::GetCurrentSymbolIndex() const { return current_symbol_index_; }

size_t BarHandler::GetCurrentBarIndex() {
  switch (current_bar_type_) {
    case BarType::TRADING: {
      return trading_index_[current_symbol_index_];
    }

    case BarType::MAGNIFIER: {
      return magnifier_index_[current_symbol_index_];
    }

    case BarType::REFERENCE: {
      return reference_index_[current_reference_timeframe_]
                             [current_symbol_index_];
    }
  }

  return -1;
}

string BarHandler::CalculateTimeframe(const shared_ptr<Table>& bar_data,
                                      const int open_time_column) {
  const int64_t fst_open_time =
      any_cast<int64_t>(GetCellValue(bar_data, open_time_column, 0));
  const int64_t snd_open_time =
      any_cast<int64_t>(GetCellValue(bar_data, open_time_column, 1));

  // 두 번째 Open Time과 첫 번째 Open Time의 차이
  return FormatTimeframe(snd_open_time - fst_open_time);
}

void BarHandler::IsTradingBarAddable() const {
  if (is_strategy_created_) {
    Logger::LogAndThrowError(
        "전략이 생성되어 트레이딩 바 데이터를 추가할 수 없습니다. "
        "전략 생성 전 트레이딩 바 데이터를 모두 추가해야 합니다.",
        __FILE__, __LINE__);
  }
}

void BarHandler::IsValidTimeframeBetweenBars(const string& timeframe,
                                             const BarType bar_type) {
  const auto parsed_bar_data_tf = ParseTimeframe(timeframe);

  switch (bar_type) {
    case BarType::TRADING: {
      if (const string& magnifier_tf = magnifier_bar_.GetTimeframe();
          !magnifier_tf.empty()) {
        const auto parsed_magnifier_tf = ParseTimeframe(magnifier_tf);

        if (parsed_magnifier_tf >= parsed_bar_data_tf) {
          Logger::LogAndThrowError(
              format("주어진 트레이딩 타임프레임 {}은(는) "
                     "돋보기 타임프레임 {}보다 높아야합니다.",
                     timeframe, magnifier_tf),
              __FILE__, __LINE__);
          return;
        }

        if (parsed_bar_data_tf % parsed_magnifier_tf != 0) {
          Logger::LogAndThrowError(
              format("주어진 트레이딩 타임프레임 {}은(는) "
                     "돋보기 타임프레임 {}의 배수여야 합니다.",
                     timeframe, magnifier_tf),
              __FILE__, __LINE__);
          return;
        }
      }

      for (const auto& reference_tf : views::keys(reference_bar_)) {
        const auto parsed_reference_tf = ParseTimeframe(reference_tf);

        if (parsed_reference_tf < parsed_bar_data_tf) {
          Logger::LogAndThrowError(
              format("주어진 트레이딩 타임프레임 {}은(는) "
                     "참조 타임프레임 {}과 같거나 낮아야합니다.",
                     timeframe, reference_tf),
              __FILE__, __LINE__);
          return;
        }

        if (parsed_reference_tf % parsed_bar_data_tf != 0) {
          Logger::LogAndThrowError(
              format("주어진 트레이딩 타임프레임 {}은(는) "
                     "참조 타임프레임 {}의 약수여야 합니다.",
                     timeframe, reference_tf),
              __FILE__, __LINE__);
          return;
        }
      }

      return;
    }

    case BarType::MAGNIFIER: {
      if (const string& trading_tf = trading_bar_.GetTimeframe();
          !trading_tf.empty()) {
        const auto parsed_trading_tf = ParseTimeframe(trading_tf);

        if (parsed_trading_tf <= parsed_bar_data_tf) {
          Logger::LogAndThrowError(
              format("주어진 돋보기 타임프레임 {}은(는) "
                     "트레이딩 타임프레임 {}보다 낮아야합니다.",
                     timeframe, trading_tf),
              __FILE__, __LINE__);
          return;
        }

        if (parsed_trading_tf % parsed_bar_data_tf != 0) {
          Logger::LogAndThrowError(
              format("주어진 돋보기 타임프레임 {}은(는) "
                     "트레이딩 타임프레임 {}의 약수여야 합니다.",
                     timeframe, trading_tf),
              __FILE__, __LINE__);
          return;
        }
      }

      for (const auto& reference_tf : views::keys(reference_bar_)) {
        if (ParseTimeframe(reference_tf) <= parsed_bar_data_tf) {
          Logger::LogAndThrowError(
              format("주어진 돋보기 타임프레임 {}은(는) "
                     "참조 타임프레임 {}보다 낮아야합니다.",
                     timeframe, reference_tf),
              __FILE__, __LINE__);
          return;
        }
      }

      return;
    }

    case BarType::REFERENCE: {
      if (const auto& trading_tf = trading_bar_.GetTimeframe();
          !trading_tf.empty()) {
        const auto parsed_trading_tf = ParseTimeframe(trading_tf);

        if (parsed_trading_tf > parsed_bar_data_tf) {
          Logger::LogAndThrowError(
              format("주어진 참조 타임프레임 {}은(는) "
                     "트레이딩 타임프레임 {}과 같거나 높아야합니다.",
                       timeframe, trading_tf),
                __FILE__, __LINE__);
          return;
        }

        if (parsed_bar_data_tf % parsed_trading_tf != 0) {
          Logger::LogAndThrowError(
              format("주어진 참조 타임프레임 {}은(는) "
                     "트레이딩 타임프레임 {}의 배수여야 합니다.",
                     timeframe, trading_tf),
              __FILE__, __LINE__);
          return;
        }
      }

      if (const auto& magnifier_tf = magnifier_bar_.GetTimeframe();
        !magnifier_tf.empty() && ParseTimeframe(magnifier_tf) >= parsed_bar_data_tf) {
        Logger::LogAndThrowError(
            format("주어진 참조 타임프레임 {}은(는) "
                   "돋보기 타임프레임 {}보다 높아야합니다.",
                   timeframe, magnifier_tf),
            __FILE__, __LINE__);
      }
    }
  }
}

void BarHandler::IsValidReferenceBarTimeframe(const string& timeframe) {
  if (const auto& timeframe_it = reference_bar_.find(timeframe);
        timeframe_it == reference_bar_.end()) {
    Logger::LogAndThrowError(
        format("참조 바 데이터에 타임프레임 {}은(는) 존재하지 않습니다.",
               timeframe),
        __FILE__, __LINE__);
  }
}