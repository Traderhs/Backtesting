// 표준 라이브러리
#include <format>

// 파일 헤더
#include "Engines/BaseBarHandler.hpp"

// 내부 헤더
#include "Engines/BarData.hpp"
#include "Engines/TimeUtils.hpp"

// 네임 스페이스
using namespace time_utils;

BaseBarHandler::BaseBarHandler()
    : trading_bar_data_(make_shared<BarData>()),
      magnifier_bar_data_(make_shared<BarData>()),
      mark_price_bar_data_(make_shared<BarData>()) {}
BaseBarHandler::~BaseBarHandler() = default;

shared_ptr<Logger>& BaseBarHandler::logger_ = Logger::GetLogger();

shared_ptr<BarData> BaseBarHandler::GetBarData(const BarType bar_type,
                                               const string& timeframe) {
  switch (bar_type) {
    case BarType::TRADING: {
      return trading_bar_data_;
    }

    case BarType::MAGNIFIER: {
      return magnifier_bar_data_;
    }

    case BarType::REFERENCE: {
      const auto& timeframe_it = reference_bar_data_.find(timeframe);
      if (timeframe_it == reference_bar_data_.end()) {
        throw runtime_error(
            format("타임프레임 [{}]은(는) 참조 바 데이터에 존재하지 않습니다.",
                   timeframe));
      }

      return timeframe_it->second;
    }

    case BarType::MARK_PRICE: {
      return mark_price_bar_data_;
    }

    default: {
      // 컴파일러 에러 방지용
      return trading_bar_data_;
    }
  }
}

vector<size_t>& BaseBarHandler::GetBarIndices(const BarType bar_type,
                                              const string& timeframe) {
  switch (bar_type) {
    case BarType::TRADING: {
      return trading_index_;
    }

    case BarType::MAGNIFIER: {
      return magnifier_index_;
    }

    case BarType::REFERENCE: {
      const auto& timeframe_it = reference_index_.find(timeframe);
      if (timeframe_it == reference_index_.end()) {
        Logger::LogAndThrowError(
            "타임프레임 " + timeframe +
                "은(는) 참조 바 데이터 인덱스에 존재하지 않습니다.",
            __FILE__, __LINE__);
      }

      return timeframe_it->second;
    }

    case BarType::MARK_PRICE: {
      return mark_price_index_;
    }

    default: {
      // 컴파일러 에러 방지용
      return trading_index_;
    }
  }
}

unordered_map<string, shared_ptr<BarData>>
BaseBarHandler::GetAllReferenceBarData() {
  return reference_bar_data_;
}