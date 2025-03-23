#pragma once

// 표준 라이브러리
#include <memory>
#include <string>
#include <vector>

// 외부 라이브러리
#include "inja/inja.hpp"
#include "nlohmann/json_fwd.hpp"

// 내부 헤더
#include "Engines/Logger.hpp"

// 전방 선언
namespace backtesting::analyzer {
class Trade;
}

namespace backtesting::bar {
class BarHandler;
}  // namespace backtesting::bar

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
using namespace logger;
using namespace indicator;
using namespace strategy;
using namespace order;
using namespace plot;
}  // namespace backtesting

namespace backtesting::analyzer {

/// 기본적인 거래 통계를 생성하는 분석기의 기초 클래스
class BaseAnalyzer {
 public:
  /// 분석기를 초기화하는 함수
  void Initialize(double initial_balance);

  /// 심볼 정보를 초기화 하는 함수
  static void SetSymbolInfo(const vector<SymbolInfo>& symbol_info);

  /// 거래 목록에 거래를 추가하는 함수
  void AddTrade(Trade& new_trade, int exit_count);

  /// 거래 목록을 csv 파일로 저장하는 함수
  void SaveTradeList(const string& file_path) const;

  /// 전략의 지표와 매매 표시를 포함한 캔들스틱 차트를 저장하는 함수
  void SaveCharts(const string& main_directory,
                  const vector<shared_ptr<Strategy>>& strategies);

 protected:
  BaseAnalyzer();
  ~BaseAnalyzer();

  static shared_ptr<BarHandler>& bar_;
  static shared_ptr<Logger>& logger_;

 private:
  /// 심볼 정보
  static vector<SymbolInfo> symbol_info_;

  /// 거래 목록
  vector<Trade> trade_list_;

  /// 마지막 거래 번호
  int trade_num_;

  /// HTML & js 파일들
  string base_chart_;
  string area_template_;
  string base_line_template_;
  string histogram_template_;
  string line_template_;

  // inja 환경
  inja::Environment inja_env_;

  /// 플롯 타입별 현재까지 생성 횟수
  unsigned char area_count_;
  unsigned char base_line_count_;
  unsigned char histogram_count_;
  unsigned char line_count_;

  /// 지표 정보에 따라 적절한 HTML 템플릿을 사용하여
  /// TradingView Lightweight Charts 시리즈로 렌더링한 String을 반환하는 함수
  [[nodiscard]] string RenderToSeries(const shared_ptr<Indicator>& indicator,
                                      int pane_idx);

  // 레퍼런스 바의 Open Time과 지표값으로 파싱한 JSON을 반환하는 함수
  [[nodiscard]] static json GetParsedJson(
      const shared_ptr<Indicator>& indicator);

  // 레퍼런스 바의 Open Time과 지표값 및 캔들이 양봉 및 음봉일 때 히스토그램의
  // 색상을 파싱한 JSON을 반환하는 함수
  [[nodiscard]] static json GetParsedJson(
      const shared_ptr<Indicator>& indicator, const string& bullish_color,
      const string& bearish_color);
};

}  // namespace backtesting::analyzer