// 파일 헤더
#include "Engines/BaseBarHandler.hpp"

// 내부 헤더
#include "Engines/BarData.hpp"
#include "Engines/TimeUtils.hpp"

// 네임 스페이스
using namespace time_utils;

BaseBarHandler::BaseBarHandler() = default;
BaseBarHandler::~BaseBarHandler() = default;

shared_ptr<Logger>& BaseBarHandler::logger_ = Logger::GetLogger();

BarData& BaseBarHandler::GetBarData(const BarType bar_type,
                                    const string& timeframe) {
  switch (bar_type) {
    case BarType::TRADING: {
      return trading_bar_;
    }

    case BarType::MAGNIFIER: {
      return magnifier_bar_;
    }

    case BarType::REFERENCE: {
      const auto& timeframe_it = reference_bar_.find(timeframe);
      if (timeframe_it == reference_bar_.end()) {
        Logger::LogAndThrowError(
            "타임프레임 " + timeframe +
                "은(는) 참조 바 데이터에 존재하지 않습니다.",
            __FILE__, __LINE__);
      }

      return timeframe_it->second;
    }

    default: {
      // 컴파일러 에러 방지용
      return trading_bar_;
    }
  }
}

vector<size_t>& BaseBarHandler::GetIndexVector(const BarType bar_type,
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

    default: {
      // 컴파일러 에러 방지용
      return trading_index_;
    }
  }
}

unordered_map<string, BarData>& BaseBarHandler::GetAllReferenceBarData() {
  return reference_bar_;
}