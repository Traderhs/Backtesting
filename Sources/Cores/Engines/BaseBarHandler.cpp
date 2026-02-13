// 표준 라이브러리
#include <algorithm>
#include <format>
#include <ranges>

// 파일 헤더
#include "Engines/BaseBarHandler.hpp"

// 내부 헤더
#include "Engines/BarData.hpp"
#include "Engines/Exception.hpp"
#include "Engines/TimeUtils.hpp"

// 네임 스페이스
namespace backtesting {
using namespace exception;
}

namespace backtesting::bar {

BaseBarHandler::BaseBarHandler()
    : trading_bar_data_(make_shared<BarData>("트레이딩")),
      magnifier_bar_data_(make_shared<BarData>("돋보기")),
      mark_price_bar_data_(make_shared<BarData>("마크 가격")) {}
BaseBarHandler::~BaseBarHandler() = default;

BACKTESTING_API shared_ptr<Logger>& BaseBarHandler::logger_ =
    Logger::GetLogger();

shared_ptr<BarData>& BaseBarHandler::GetBarData(const BarDataType bar_data_type,
                                                const string& timeframe) {
  switch (bar_data_type) {
    case TRADING: {
      return trading_bar_data_;
    }

    case MAGNIFIER: {
      return magnifier_bar_data_;
    }

    case REFERENCE: {
      const auto& timeframe_it = reference_bar_data_.find(timeframe);
      if (timeframe_it == reference_bar_data_.end()) {
        throw InvalidValue(
            format("타임프레임 [{}]은(는) 참조 바 데이터에 존재하지 않습니다.",
                   timeframe));
      }

      return timeframe_it->second;
    }

    case MARK_PRICE: {
      return mark_price_bar_data_;
    }

    default: {
      // 컴파일러 에러 방지용
      return trading_bar_data_;
    }
  }
}

vector<size_t>& BaseBarHandler::GetBarIndices(const BarDataType bar_data_type,
                                              const string& timeframe) {
  switch (bar_data_type) {
    case TRADING: {
      return trading_index_;
    }

    case MAGNIFIER: {
      return magnifier_index_;
    }

    case REFERENCE: {
      const auto& timeframe_it = reference_index_.find(timeframe);
      if (timeframe_it == reference_index_.end()) {
        throw InvalidValue(
            format("타임프레임 [{}]은(는) 참조 바 데이터 "
                   "인덱스에 존재하지 않습니다.",
                   timeframe));
      }

      return timeframe_it->second;
    }

    case MARK_PRICE: {
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

void BaseBarHandler::ResetBaseBarHandlerState() {
  // 모든 인덱스 벡터를 0으로 초기화
  ranges::fill(trading_index_, 0);
  ranges::fill(magnifier_index_, 0);

  for (auto& indices : reference_index_ | views::values) {
    ranges::fill(indices, 0);
  }

  ranges::fill(mark_price_index_, 0);
}

void BaseBarHandler::ResetBaseBarHandler() {
  // 바 데이터 초기화
  trading_bar_data_.reset();
  magnifier_bar_data_.reset();
  reference_bar_data_.clear();
  mark_price_bar_data_.reset();

  // 인덱스 초기화
  ResetBaseBarHandlerState();
}

}  // namespace backtesting::bar
