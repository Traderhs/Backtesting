// 내부 헤더
#include <memory>
#include <ranges>
#include <tuple>

#include "Engines/DataUtils.hpp"
#include "Engines/TimeUtils.hpp"

// 파일 헤더
#include "Engines/BarHandler.hpp"

// 네임 스페이스
using namespace data_utils;
using namespace time_utils;

BarHandler::BarHandler()
    : current_bar_type_(BarType::TRADING), current_symbol_idx_(0) {}
BarHandler::~BarHandler() = default;

mutex BarHandler::mutex_;
unique_ptr<BarHandler> BarHandler::instance_;

BarHandler& BarHandler::GetBarHandler() {
  lock_guard lock(mutex_);  // 스레드에서 안전하게 접근하기 위해 mutex 사용

  // 인스턴스가 생성됐는지 확인
  if (!instance_) {
    // 인스턴스가 생성되지 않았으면 생성 후 저장
    instance_ = make_unique<BarHandler>();
  }

  return *instance_;
}

void BarHandler::AddBarData(const string& name, const string& file_path,
                            const vector<int>& columns,
                            const BarType bar_type) {
  // Parquet 파일 읽기
  const auto& bar_data = ReadParquet(file_path);

  // 타임프레임 계산
  const auto& bar_data_timeframe = CalculateTimeframe(bar_data, columns[0]);

  // 타임프레임 유효성 검사
  IsValidTimeframeBetweenBars(bar_data_timeframe, bar_type);

  // 데이터 추가
  GetBarData(bar_type, bar_data_timeframe)
      .SetBarData(name, bar_data_timeframe, bar_data, columns);

  // 타임프레임 설정
  SetTimeframe(bar_data_timeframe, bar_type);

  // 로그용 바 데이터 타입 문자열
  string bar_data_type_str;
  if (bar_type == BarType::TRADING) {
    bar_data_type_str = "트레이딩";
  } else if (bar_type == BarType::MAGNIFIER) {
    bar_data_type_str = "돋보기";
  } else if (bar_type == BarType::REFERENCE) {
    bar_data_type_str = "참조";
  }

  logger_.Log(
      Logger::INFO_L,
      format("[{} - {}] 기간의 {} {}이(가) {} 바 데이터로 추가되었습니다.",
             UtcTimestampToUtcDatetime(
                 any_cast<int64_t>(GetCellValue(bar_data, columns[0], 0))),
             UtcTimestampToUtcDatetime(any_cast<int64_t>(
                 GetCellValue(bar_data, columns[0], bar_data->num_rows() - 1))),
             name, bar_data_timeframe, bar_data_type_str),
      __FILE__, __LINE__);
}

void BarHandler::SetCurrentBarIndex(const size_t index, const int symbol_idx,
                                    const string& timeframe) {
  switch (current_bar_type_) {
    case BarType::TRADING: {
      if (const auto max_idx = trading_index_.size() - 1;
          max_idx < symbol_idx) {
        Logger::LogAndThrowError(
            format("주어진 심볼 인덱스 {}는 트레이딩 인덱스의 최대 인덱스인 "
                   "{}을(를) 초과했습니다.",
                   symbol_idx, max_idx),
            __FILE__, __LINE__);
      }

      trading_index_[symbol_idx] = index;
      return;
    }

    case BarType::MAGNIFIER: {
      if (const auto max_idx = magnifier_index_.size() - 1;
          max_idx < symbol_idx) {
        Logger::LogAndThrowError(
            format("주어진 심볼 인덱스 {}는 돋보기 인덱스의 최대 인덱스인 "
                   "{}을(를) 초과했습니다.",
                   symbol_idx, max_idx),
            __FILE__, __LINE__);
      }

      magnifier_index_[symbol_idx] = index;
      return;
    }

    case BarType::REFERENCE: {
      const auto& it = reference_index_.find(timeframe);
      if (it == reference_index_.end()) {
        Logger::LogAndThrowError(
            format(
                "레퍼런스 바 데이터에 타임프레임 {}은(는) 존재하지 않습니다.",
                timeframe),
            __FILE__, __LINE__);
      }

      if (const auto max_idx = it->second.size() - 1; max_idx < symbol_idx) {
        Logger::LogAndThrowError(
            format("주어진 심볼 인덱스 {}는 레퍼런스 인덱스의 최대 인덱스인 "
                   "{}을(를) 초과했습니다.",
                   symbol_idx, max_idx),
            __FILE__, __LINE__);
      }

      it->second[symbol_idx] = index;
    }
  }
}

BarHandler::BarType BarHandler::GetCurrentBarType() const {
  return current_bar_type_;
}

int BarHandler::GetCurrentSymbolIdx() const { return current_symbol_idx_; }

size_t BarHandler::GetCurrentBarIndex(const int symbol_idx,
                                      const string& timeframe) {
  switch (current_bar_type_) {
    case BarType::TRADING: {
      if (const auto max_idx = trading_index_.size() - 1;
          max_idx < symbol_idx) {
        Logger::LogAndThrowError(
            format("주어진 심볼 인덱스 {}는 트레이딩 인덱스의 최대 사이즈인 "
                   "{}을(를) 초과했습니다.",
                   symbol_idx, max_idx),
            __FILE__, __LINE__);
      }

      return trading_index_[symbol_idx];
    }

    case BarType::MAGNIFIER: {
      if (const auto max_idx = magnifier_index_.size() - 1;
          max_idx < symbol_idx) {
        Logger::LogAndThrowError(
            format("주어진 심볼 인덱스 {}는 돋보기 인덱스의 최대 사이즈인 "
                   "{}을(를) 초과했습니다.",
                   symbol_idx, max_idx),
            __FILE__, __LINE__);
      }

      return magnifier_index_[symbol_idx];
    }

    case BarType::REFERENCE: {
      const auto& it = reference_index_.find(timeframe);
      if (it == reference_index_.end()) {
        Logger::LogAndThrowError(
            format(
                "레퍼런스 바 데이터에 타임프레임 {}은(는) 존재하지 않습니다.",
                timeframe),
            __FILE__, __LINE__);
      }

      if (const auto max_idx = it->second.size() - 1; max_idx < symbol_idx) {
        Logger::LogAndThrowError(
            format("주어진 심볼 인덱스 {}는 트레이딩 인덱스의 최대 사이즈인 "
                   "{}을(를) 초과했습니다.",
                   symbol_idx, max_idx),
            __FILE__, __LINE__);
      }

      return it->second[symbol_idx];
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

void BarHandler::IsValidTimeframeBetweenBars(const string& timeframe,
                                             const BarType bar_type) {
  const auto parsed_bar_data_tf = ParseTimeframe(timeframe);

  switch (bar_type) {
    case BarType::TRADING: {
      if (!magnifier_timeframe_.empty() &&
          parsed_magnifier_timeframe_ >= parsed_bar_data_tf) {
        Logger::LogAndThrowError(
            format("주어진 트레이딩 타임프레임 {}은(는) "
                   "돋보기 타임프레임 {}보다 높아야합니다.",
                   timeframe, magnifier_timeframe_),
            __FILE__, __LINE__);
        return;
      }

      if (!magnifier_timeframe_.empty() &&
          parsed_bar_data_tf % parsed_magnifier_timeframe_ != 0) {
        Logger::LogAndThrowError(
            format("주어진 트레이딩 타임프레임 {}은(는) "
                   "돋보기 타임프레임 {}의 배수여야 합니다.",
                   timeframe, magnifier_timeframe_),
            __FILE__, __LINE__);
        return;
      }

      for (const auto& reference_tf : reference_timeframe_) {
        if (ParseTimeframe(reference_tf) < parsed_bar_data_tf) {
          Logger::LogAndThrowError(
              format("주어진 트레이딩 타임프레임 {}은(는) "
                     "참조 타임프레임 {}과 같거나 낮아야합니다.",
                     timeframe, reference_tf),
              __FILE__, __LINE__);
          return;
        }
      }
    }

    case BarType::MAGNIFIER: {
      if (!trading_timeframe_.empty() &&
          parsed_trading_timeframe_ <= parsed_bar_data_tf) {
        Logger::LogAndThrowError(
            format("주어진 돋보기 타임프레임 {}은(는) "
                   "트레이딩 타임프레임 {}보다 낮아야합니다.",
                   timeframe, trading_timeframe_),
            __FILE__, __LINE__);
        return;
      }

      if (!trading_timeframe_.empty() &&
          parsed_trading_timeframe_ % parsed_bar_data_tf != 0) {
        Logger::LogAndThrowError(
            format("주어진 돋보기 타임프레임 {}은(는) "
                   "트레이딩 타임프레임 {}의 약수여야 합니다.",
                   timeframe, trading_timeframe_),
            __FILE__, __LINE__);
        return;
      }

      for (const auto& reference_tf : reference_timeframe_) {
        if (ParseTimeframe(reference_tf) <= parsed_bar_data_tf) {
          Logger::LogAndThrowError(
              format("주어진 돋보기 타임프레임 {}은(는) "
                     "참조 타임프레임 {}보다 낮아야합니다.",
                     timeframe, reference_tf),
              __FILE__, __LINE__);
          return;
        }
      }
    }

    case BarType::REFERENCE: {
      if (!trading_timeframe_.empty() &&
          parsed_trading_timeframe_ > parsed_bar_data_tf) {
        Logger::LogAndThrowError(
            format("주어진 참조 타임프레임 {}은(는) "
                   "트레이딩 타임프레임 {}과 같거나 높아야합니다.",
                     timeframe, trading_timeframe_),
              __FILE__, __LINE__);
        return;
          }

      if (!magnifier_timeframe_.empty() &&
          parsed_magnifier_timeframe_ >= parsed_bar_data_tf) {
        Logger::LogAndThrowError(
            format("주어진 참조 타임프레임 {}은(는) "
                   "돋보기 타임프레임 {}보다 높아야합니다.",
                   timeframe, magnifier_timeframe_),
            __FILE__, __LINE__);
          }
    }
  }
}