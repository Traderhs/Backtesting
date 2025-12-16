#pragma once
// 표준 라이브러리
#include <string>

// 외부 라이브러리
#include <gtest/gtest.h>

// 내부 헤더
#include "BinanceBarDataManager.hpp"

using namespace std;

class BinanceDataManagerTest : public testing::Test, public BinanceBarDataManager {
 public:
  // 다른 테스트에서 변경해도 값 영향 미치지 않음
  BinanceBarDataManager test;
  string url;
  unordered_map<string, string> forward_params;
  unordered_map<string, string> backward_params;

 protected:
  void SetUp() override;
  void TearDown() override;
};
