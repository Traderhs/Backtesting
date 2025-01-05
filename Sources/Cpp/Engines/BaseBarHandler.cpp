// 내부 헤더
#include <Engines/TimeUtils.hpp>

// 파일 헤더
#include "Engines/BaseBarHandler.hpp"

// 네임 스페이스
using namespace time_utils;

BaseBarHandler::BaseBarHandler()
    : parsed_trading_timeframe_(-1), parsed_magnifier_timeframe_(-1) {};
BaseBarHandler::~BaseBarHandler() = default;

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
        Logger::LogAndThrowError("타임프레임 " + timeframe +
                                     "은(는) 레퍼런스 바에 존재하지 않습니다.",
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

variant<string, set<string>> BaseBarHandler::GetTimeframe(
    const BarType bar_type) {
  switch (bar_type) {
    case BarType::TRADING: {
      return trading_timeframe_;
    }

    case BarType::MAGNIFIER: {
      return magnifier_timeframe_;
    }

    case BarType::REFERENCE: {
      return reference_timeframe_;
    }
    default: {
      return string();
    }
  }
}

Logger& BaseBarHandler::logger_ = Logger::GetLogger();

void BaseBarHandler::SetTimeframe(const string& timeframe,
                                  const BarType bar_type) {
  switch (bar_type) {
    case BarType::TRADING: {
      if (trading_timeframe_.empty()) {
        trading_timeframe_ = timeframe;
        parsed_trading_timeframe_ = ParseTimeframe(trading_timeframe_);
      }
      return;
    }

    case BarType::MAGNIFIER: {
      if (magnifier_timeframe_.empty()) {
        magnifier_timeframe_ = timeframe;
        parsed_magnifier_timeframe_ = ParseTimeframe(magnifier_timeframe_);
      }
      return;
    }

    case BarType::REFERENCE: {
      if (const auto& timeframe_it = reference_timeframe_.find(timeframe);
          timeframe_it == reference_timeframe_.end()) {
        reference_timeframe_.insert(timeframe);
      }
    }
  }
}


