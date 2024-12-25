// 표준 라이브러리
#include <iostream>

// 내부 헤더
#include "Engines/TimeUtils.hpp"

// 파일 헤더
#include "Engines/Print.hpp"

void Print::PrintBarData(const BarDataManager::bar_data& bar_data) {
  cout << TimeUtils::UTCTimestampToUtcDatetime(bar_data.open_time) << " ";
  cout << bar_data.open << " ";
  cout << bar_data.high << " ";
  cout << bar_data.low << " ";
  cout << bar_data.close << " ";
  cout << bar_data.volume << " ";
  cout << TimeUtils::UTCTimestampToUtcDatetime(bar_data.close_time) << endl;
}

void Print::PrintBarDataVector(
    const vector<BarDataManager::bar_data>& bar_data_vector,
    const int64_t length, const bool is_reverse) {
  if (!is_reverse) {
    for (auto i = 0; i < length; i++) PrintBarData(bar_data_vector[i]);
  } else {
    const auto size = bar_data_vector.size();
    for (auto i = size - length; i < size; i++) {
      PrintBarData(bar_data_vector[i]);
    }
  }
}