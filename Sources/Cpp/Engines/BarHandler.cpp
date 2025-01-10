// 표준 라이브러리
#include <memory>
#include <ranges>

// 파일 헤더
#include "Engines\BarHandler.hpp"

// 내부 헤더
#include "Engines\BarData.hpp"
#include "Engines\DataUtils.hpp"
#include "Engines\Logger.hpp"
#include "Engines\TimeUtils.hpp"

// 네임 스페이스
using namespace data_utils;
using namespace time_utils;

BarHandler::BarHandler()
    : current_bar_type_(BarType::TRADING), current_symbol_index_(0) {}
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

  // 타임프레임 유효성 검사
  IsValidTimeframeBetweenBars(bar_data_timeframe, bar_type);

  // 데이터 추가
  switch (bar_type) {
    case BarType::TRADING: {
      trading_bar_.SetBarData(symbol_name, bar_data_timeframe, bar_data,
                              columns);

      // 메인 바를 지표 계산용으로 사용하기 때문에 참조 바 데이터로 자동 추가
      reference_bar_[bar_data_timeframe].SetBarData(
          symbol_name, bar_data_timeframe, bar_data, columns);
      break;
    }

    case BarType::MAGNIFIER: {
      magnifier_bar_.SetBarData(symbol_name, bar_data_timeframe, bar_data,
                                columns);
      break;
    }

    case BarType::REFERENCE: {
      reference_bar_[bar_data_timeframe].SetBarData(
          symbol_name, bar_data_timeframe, bar_data, columns);
      break;
    }
  }

  // 로그용 바 데이터 타입 문자열
  string bar_data_type_str;
  if (bar_type == BarType::TRADING) {
    bar_data_type_str = "트레이딩";
  } else if (bar_type == BarType::MAGNIFIER) {
    bar_data_type_str = "돋보기";
  } else if (bar_type == BarType::REFERENCE) {
    bar_data_type_str = "참조";
  }

  logger_->Log(
      LogLevel::INFO_L,
      format("[{} - {}] 기간의 {} {}이(가) {} 바 데이터로 추가되었습니다.",
             UtcTimestampToUtcDatetime(
                 any_cast<int64_t>(GetCellValue(bar_data, columns[0], 0))),
             UtcTimestampToUtcDatetime(any_cast<int64_t>(
                 GetCellValue(bar_data, columns[0], bar_data->num_rows() - 1))),
             symbol_name, bar_data_timeframe, bar_data_type_str),
      __FILE__, __LINE__);
}

void BarHandler::SetCurrentBarType(const BarType bar_type,
                                   const string& timeframe) {
  current_bar_type_ = bar_type;

  if (bar_type == BarType::REFERENCE) {
    if (const auto& timeframe_it = reference_bar_.find(timeframe);
        timeframe_it == reference_bar_.end()) {
      Logger::LogAndThrowError(
          format("참조 바 데이터에 타임프레임 {}은(는) 존재하지 않습니다.",
                 timeframe),
          __FILE__, __LINE__);
    }

    current_reference_timeframe_ = timeframe;
  }
}

void BarHandler::SetCurrentSymbolIndex(const int symbol_index) {
  current_symbol_index_ = symbol_index;
}

void BarHandler::SetCurrentBarIndex(const size_t bar_index) {
  // @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ 인덱스 하나만 늘리는 함수도 있어야할 듯
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

BarType BarHandler::GetCurrentBarType() const { return current_bar_type_; }

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

void BarHandler::IsValidTimeframeBetweenBars(const string& timeframe,
                                             const BarType bar_type) {
  const auto parsed_bar_data_tf = ParseTimeframe(timeframe);

  switch (bar_type) {
    case BarType::TRADING: {
      const string& magnifier_tf = magnifier_bar_.GetTimeframe();
      const auto parsed_magnifier_tf = ParseTimeframe(magnifier_tf);

      if (!magnifier_tf.empty() && parsed_magnifier_tf >= parsed_bar_data_tf) {
        Logger::LogAndThrowError(
            format("주어진 트레이딩 타임프레임 {}은(는) "
                   "돋보기 타임프레임 {}보다 높아야합니다.",
                   timeframe, magnifier_tf),
            __FILE__, __LINE__);
        return;
      }

      if (!magnifier_tf.empty() &&
          parsed_bar_data_tf % parsed_magnifier_tf != 0) {
        Logger::LogAndThrowError(
            format("주어진 트레이딩 타임프레임 {}은(는) "
                   "돋보기 타임프레임 {}의 배수여야 합니다.",
                   timeframe, magnifier_tf),
            __FILE__, __LINE__);
        return;
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
    }

    case BarType::MAGNIFIER: {
      const string& trading_tf = trading_bar_.GetTimeframe();
      const auto parsed_trading_tf = ParseTimeframe(trading_tf);

      if (!trading_tf.empty() && parsed_trading_tf <= parsed_bar_data_tf) {
        Logger::LogAndThrowError(
            format("주어진 돋보기 타임프레임 {}은(는) "
                   "트레이딩 타임프레임 {}보다 낮아야합니다.",
                   timeframe, trading_tf),
            __FILE__, __LINE__);
        return;
      }

      if (!trading_tf.empty() && parsed_trading_tf % parsed_bar_data_tf != 0) {
        Logger::LogAndThrowError(
            format("주어진 돋보기 타임프레임 {}은(는) "
                   "트레이딩 타임프레임 {}의 약수여야 합니다.",
                   timeframe, trading_tf),
            __FILE__, __LINE__);
        return;
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
    }

    case BarType::REFERENCE: {
      const auto& trading_tf = trading_bar_.GetTimeframe();
      const auto parsed_trading_tf = ParseTimeframe(trading_tf);

      if (!trading_tf.empty() && parsed_trading_tf > parsed_bar_data_tf) {
        Logger::LogAndThrowError(
            format("주어진 참조 타임프레임 {}은(는) "
                   "트레이딩 타임프레임 {}과 같거나 높아야합니다.",
                     timeframe, trading_tf),
              __FILE__, __LINE__);
        return;
      }

      if (!trading_tf.empty() && parsed_bar_data_tf % parsed_trading_tf != 0) {
        Logger::LogAndThrowError(
            format("주어진 참조 타임프레임 {}은(는) "
                   "트레이딩 타임프레임 {}의 배수여야 합니다.",
                   timeframe, trading_tf),
            __FILE__, __LINE__);
        return;
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