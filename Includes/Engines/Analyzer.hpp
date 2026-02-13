#pragma once

// 표준 라이브러리
#include <memory>
#include <string>
#include <vector>

// 외부 라이브러리
#include "nlohmann/json_fwd.hpp"

// 내부 헤더
#include "Engines/Export.hpp"
#include "Engines/Logger.hpp"
#include "Engines/Trade.hpp"

namespace backtesting::bar {
class BarData;
class BarHandler;
}  // namespace backtesting::bar

namespace backtesting::engine {
class Config;
class Engine;
}  // namespace backtesting::engine

namespace backtesting::main {
class Backtesting;
}

namespace backtesting::order {
class Order;
}

namespace backtesting::indicator {
class Indicator;
}

namespace backtesting::strategy {
class Strategy;
}

namespace backtesting::order {
class SymbolInfo;
}

namespace backtesting::plot {
class Plot;
class Area;
class Baseline;
class Histogram;
class Line;
}  // namespace backtesting::plot

// 네임 스페이스
using namespace std;
using namespace nlohmann;

namespace backtesting {
using namespace bar;
using namespace engine;
using namespace logger;
using namespace main;
using namespace indicator;
using namespace strategy;
using namespace order;
using namespace plot;
}  // namespace backtesting

namespace backtesting::analyzer {

/// 거래 통계를 생성하는 분석기 클래스
class BACKTESTING_API Analyzer {
  friend class Backtesting;

 public:
  static shared_ptr<Analyzer>& GetAnalyzer();

  /// 분석기를 초기화하는 함수
  void Initialize(int64_t begin_open_time, int64_t end_close_time,
                  double initial_balance);

  /// 심볼 정보를 초기화 하는 함수
  static void SetSymbolInfo(const vector<SymbolInfo>& symbol_info);

  /// 거래 내역에 거래를 추가하는 함수
  void AddTrade(Trade& new_trade, int exit_count);

  /// 이번 백테스팅의 결과가 저장될 메인 폴더의 경로를 반환하는 함수
  [[nodiscard]] string GetMainDirectory() const;

  // ===========================================================================
  // 함수 나열 순서는 BackBoard 대시보드 순서

  /// 백테스팅 결과 저장에 필요한 폴더들을 생성하고 이번 백테스팅의
  /// 메인 폴더 경로를 반환하는 함수
  void CreateDirectories();

  /// OHLCV와 플롯하지 않는 지표를 제외한
  /// 지표 데이터를 parquet 파일로 저장하는 함수
  void SaveIndicatorData();

  /// 거래 내역을 파일로 저장하는 함수
  void SaveTradeList() const;

  /// 각 백테스팅의 설정 정보를 파일로 저장하는 함수
  void SaveConfig();

  /// 전략들의 소스 코드를 파일로 저장하는 함수
  void SaveSourcesAndHeaders();

  /// 백보드를 저장하는 함수
  /// 로컬 저장소에서 찾을 수 없을 때에는 원격 저장소로 fallback
  void SaveBackBoard() const;

  /// 해당 회차의 백테스팅의 로그를 지정된 폴더에 저장하는 함수
  void SaveBacktestingLog() const;

 private:
  // 싱글톤 인스턴스 관리
  Analyzer();
  class Deleter {
   public:
    void operator()(const Analyzer* p) const;
  };

  static mutex mutex_;
  static shared_ptr<Analyzer> instance_;

  static shared_ptr<BarHandler>& bar_;
  static shared_ptr<Config>& config_;
  static shared_ptr<Engine>& engine_;
  static shared_ptr<Logger>& logger_;
  static vector<SymbolInfo> symbol_info_;

  /// 이번 백테스팅의 결과가 저장될 메인 폴더
  string main_directory_;

  /// 백테스팅 기간
  string begin_open_time_;
  string end_close_time_;

  /// 거래 목록
  vector<Trade> trade_list_;

  /// 마지막 거래 번호
  int trade_num_;

  /// Analyzer의 싱글톤 인스턴스를 초기화하는 함수
  static void ResetAnalyzer();

  /// 주어진 바 데이터에서 누락된 바들의 개수와
  /// 해당되는 바 범위의 Open Time 문자열 벡터를 반환하는 함수
  [[nodiscard]] static pair<int, vector<string>> FindMissingBars(
      const shared_ptr<BarData>& bar_data, int symbol_idx, int64_t interval);

  /// Json 객체에 지표의 플롯 정보를 기록하는 함수
  static void ParsePlotInfo(ordered_json& indicator_json,
                            const shared_ptr<Indicator>& indicator);

  /// GitHub 릴리즈에서 백보드를 다운로드하는 함수
  void DownloadBackBoardFromGitHub() const;
};

}  // namespace backtesting::analyzer
