#pragma once

// 표준 라이브러리
#include <memory>

// 전방 선언
class Engine;
class BinanceFetcher;
class Logger;

// 내부 헤더
#include <Engines/BaseBarHandler.hpp>

// 네임 스페이스
using namespace std;
using enum BarType;

class Backtesting final {
  public:
   static shared_ptr<Engine>& engine_;
   static shared_ptr<Logger>& logger_;
   static shared_ptr<BinanceFetcher> fetcher_;
};