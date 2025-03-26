// 표준 라이브러리
#include <format>
#include <fstream>
#include <ranges>
#include <unordered_set>

// 파일 헤더
#include "Engines/BaseAnalyzer.hpp"

// 내부 헤더
#include "Engines/BarData.hpp"
#include "Engines/BarHandler.hpp"
#include "Engines/Config.hpp"
#include "Engines/DataUtils.hpp"
#include "Engines/Engine.hpp"
#include "Engines/Logger.hpp"
#include "Engines/Strategy.hpp"
#include "Engines/SymbolInfo.hpp"
#include "Engines/TimeUtils.hpp"
#include "Engines/Trade.hpp"

// 네임 스페이스
namespace backtesting {
using namespace analyzer;
using namespace engine;
using namespace logger;
using namespace utils;
}  // namespace backtesting

namespace backtesting::analyzer {

BaseAnalyzer::BaseAnalyzer()
    : trade_num_(1),
      area_count_(0),
      base_line_count_(0),
      histogram_count_(0),
      line_count_(0) {}
BaseAnalyzer::~BaseAnalyzer() = default;

shared_ptr<BarHandler>& BaseAnalyzer::bar_ = BarHandler::GetBarHandler();
shared_ptr<Config>& BaseAnalyzer::config_ = Engine::GetConfig();
shared_ptr<Engine>& BaseAnalyzer::engine_ = Engine::GetEngine();
shared_ptr<Logger>& BaseAnalyzer::logger_ = Logger::GetLogger();
vector<SymbolInfo> BaseAnalyzer::symbol_info_;

void BaseAnalyzer::Initialize(const double initial_balance) {
  // 매매 목록 0행 초기화
  trade_list_.push_back(Trade()
                            .SetSymbolName("-")
                            .SetStrategyName("-")
                            .SetEntryName("-")
                            .SetExitName("-")
                            .SetEntryTime("-")
                            .SetExitTime("-")
                            .SetHoldingTime("-")
                            .SetWalletBalance(initial_balance)
                            .SetMaxWalletBalance(initial_balance));

  // HTML 파일 로드
  static string chart_template_directory =
      config_->GetRootDirectory() + "/Sources/html/chart";

  try {
    base_chart_ = OpenHtml(chart_template_directory + "/BaseChart.html");
    area_template_ =
        OpenHtml(chart_template_directory + "/AreaSeriesTemplate.js");
    base_line_template_ =
        OpenHtml(chart_template_directory + "/BaselineSeriesTemplate.js");
    histogram_template_ =
        OpenHtml(chart_template_directory + "/HistogramSeriesTemplate.js");
    line_template_ =
        OpenHtml(chart_template_directory + "/LineSeriesTemplate.js");
  } catch (const exception& e) {
    logger_->Log(ERROR_L, e.what(), __FILE__, __LINE__);
    Logger::LogAndThrowError("분석기를 초기화하는 중 오류가 발생했습니다.",
                             __FILE__, __LINE__);
  }
}

void BaseAnalyzer::SetSymbolInfo(const vector<SymbolInfo>& symbol_info) {
  symbol_info_ = symbol_info;
}

void BaseAnalyzer::AddTrade(Trade& new_trade, const int exit_count) {
  if (exit_count == 1) {
    // 전량 청산 거래거나 첫 분할 청산 거래인 경우 거래 번호 추가
    trade_list_.push_back(new_trade.SetTradeNumber(trade_num_++));
  } else {
    // 두 번째 분할 청산 거래부터는 첫 분할 청산 거래의 진입 거래 번호가
    // 거래 번호가 됨
    int trade_num = 0;
    for (auto& trade : ranges::reverse_view(trade_list_)) {
      if (trade.GetEntryName() == new_trade.GetEntryName() &&
          IsEqual(trade.GetEntryPrice(), new_trade.GetEntryPrice())) {
        trade_num = trade.GetTradeNumber();
        break;
      }
    }

    trade_list_.push_back(new_trade.SetTradeNumber(trade_num));
  }
}

// =============================================================================
string BaseAnalyzer::CreateDirectories() {
  string main_directory;

  try {
    // 전략 이름들을 이어붙인 이름 + 현재 시간이 이번 백테스팅의 메인 폴더
    for (const auto& strategy : engine_->strategies_) {
      main_directory += strategy->GetName() + "_";
    }

    main_directory += GetCurrentLocalDatetime();

    // 시간 구분 문자 제거 및 공백 언더 스코어화
    erase(main_directory, ':');
    erase(main_directory, '-');
    ranges::replace(main_directory, ' ', '_');

    // 경로로 수정
    main_directory = config_->GetRootDirectory() + "/Results/" + main_directory;

    // 메인 폴더 생성
    filesystem::create_directory(main_directory);

    // 커스텀 전략의 소스 코드 저장 폴더 생성
    filesystem::create_directories(main_directory + "/Sources");

    // 차트 저장 폴더 생성
    filesystem::create_directories(format("{}/Charts", main_directory));
  } catch (const std::exception& e) {
    logger_->Log(ERROR_L, e.what(), __FILE__, __LINE__);
    Logger::LogAndThrowError("폴더 생성 중 에러가 발생했습니다.", __FILE__,
                             __LINE__);
  }

  return main_directory;
}

void BaseAnalyzer::SaveStreaks(const string& file_path) const {
  try {
    // Trade Number 별 Net PnL 집계 (분할 청산 합산)
    unordered_map<int, double> net_pnl;
    vector<int> order;
    for (const auto& trade : trade_list_) {
      int num = trade.GetTradeNumber();
      if (num == 0) continue;

      if (!net_pnl.contains(num)) {
        order.push_back(num);
        net_pnl[num] = trade.GetPnlNet();
      } else {
        net_pnl[num] += trade.GetPnlNet();
      }
    }

    map<int, pair<int, double>> win, lose;  // 연승패 횟수<등장 건수, 누적 PnL>
    bool current_winning = false;  // 현재 연승(true)인지 연패(false)인지 표시
    int streak_len = 0;            // 현재 몇 연승패 중인지 기록
    double pnl_sum = 0;            // 현재 연승패의 누적 Net PnL 합계
    bool first = true;             // 첫 거래 번호 여부

    // 거래 번호 순서대로 순회하면서 연승 연패 계산
    for (int order_num : order) {
      // 해당 거래 번호의 Net PnL 합계 가져오기
      const double pnl = net_pnl.at(order_num);

      // 새 연승연패 시작 조건: 첫 반복이거나 승패가 바뀌었을 때
      if (const bool is_win = pnl >= 0; first || is_win != current_winning) {
        if (!first) {
          // 연승패 종료 → 저장
          auto& streaks = current_winning ? win : lose;
          streaks[streak_len].first++;            // 해당 연승패의 건수 증가
          streaks[streak_len].second += pnl_sum;  // 해당 횟수의 누적 PnL 합산
        }

        // 연승패 리셋
        current_winning = is_win;
        streak_len = 1;
        pnl_sum = pnl;
        first = false;
      } else {
        // 같은 승/패 연속 → 연승패 연장
        streak_len++;
        pnl_sum += pnl;
      }
    }

    // 마지막 연승패가 남아있으면 저장
    if (!first) {
      auto& streaks = current_winning ? win : lose;
      streaks[streak_len].first++;
      streaks[streak_len].second += pnl_sum;
    }

    // CSV 파일 열기
    ofstream streaks_file(file_path);
    if (!streaks_file.is_open()) {
      throw runtime_error(
          format("연승/연패 기록 [{}]을(를) 생성할 수 없습니다.", file_path));
    }

    // BOM 추가 (한글 인코딩 깨짐 방지)
    streaks_file << "\xEF\xBB\xBF";

    // 헤더 작성
    streaks_file << "연승 횟수,연승 건수,수익 합계,거래당 평균 수익,"
                 << "연패 횟수,연패 건수,손실 합계,거래당 평균 손실\n";

    // 최대 행 수 계산
    size_t max_row = max(win.size(), lose.size());
    auto win_it = win.begin();
    auto lose_it = lose.begin();

    // 각 행에 대해 데이터 작성
    for (size_t row = 0; row < max_row; ++row) {
      string win_len, win_count, win_total, win_avg;
      string lose_len, lose_count, lose_total, lose_avg;

      if (win_it != win.end()) {
        int len = win_it->first;
        int count = win_it->second.first;
        double total_pnl = win_it->second.second;
        double avg_pnl = total_pnl / (len * count);
        win_len = to_string(len) + "연승";
        win_count = to_string(count) + "건";
        win_total = FormatDollar(total_pnl, true);
        win_avg = FormatDollar(avg_pnl, true);
        ++win_it;
      }

      if (lose_it != lose.end()) {
        int len = lose_it->first;
        int count = lose_it->second.first;
        double total_pnl = lose_it->second.second;
        double avg_pnl = total_pnl / (len * count);
        lose_len = to_string(len) + "연패";
        lose_count = to_string(count) + "건";
        lose_total = FormatDollar(total_pnl, true);
        lose_avg = FormatDollar(avg_pnl, true);
        ++lose_it;
      }

      streaks_file << win_len << "," << win_count << "," << win_total << ","
                   << win_avg << "," << lose_len << "," << lose_count << ","
                   << lose_total << "," << lose_avg << "\n";
    }

    streaks_file.close();

    logger_->Log(INFO_L, "연승/연패 기록이 저장되었습니다.", __FILE__,
                 __LINE__);
  } catch (const exception& e) {
    logger_->Log(ERROR_L, e.what(), __FILE__, __LINE__);
    Logger::LogAndThrowError(
        "연승/연패 기록을 저장하는 중 오류가 발생했습니다.", __FILE__,
        __LINE__);
  }
}

void BaseAnalyzer::SaveCharts(const string& main_directory,
                              const vector<shared_ptr<Strategy>>& strategies) {
  try {
    const auto& trading_bar = bar_->GetBarData(TRADING);
    const auto& trading_timeframe = trading_bar->GetTimeframe();

    // 각 심볼들을 순회하며 저장
    for (int symbol_idx = 0; symbol_idx < trading_bar->GetNumSymbols();
         symbol_idx++) {
      const auto& symbol_name = trading_bar->GetSymbolName(symbol_idx);
      bar_->SetCurrentSymbolIndex(symbol_idx);

      // 페인 그룹 생성: 페인 이름<지표들>
      ordered_map<string, vector<shared_ptr<Indicator>>> indicators_group;
      int pane_count = 0;  // 오버레이하지 않는 페인의 개수
      for (const auto& strategy : strategies) {
        for (const auto& indicator : strategy->GetIndicators()) {
          // 플롯하지 않는 지표 제외
          if (indicator->plot_type_ == "NullPlot") {
            continue;
          }

          // 메인 차트에 플롯하는 지표
          if (const auto& plot = indicator->plot_; plot->overlay_) {
            indicators_group["chart"].push_back(indicator);
          } else {
            // 새로운 페인에 플롯하는 지표
            auto& pane_group = indicators_group[plot->pane_name_];

            // 페인 이름으로 첫 추가 시에만 페인 개수 추가
            if (pane_group.empty()) {
              pane_count++;
            }

            pane_group.push_back(indicator);
          }
        }
      }

      // 지표 시리즈 렌더링
      string indicator_series_to_chart;
      string indicator_series_to_pane;
      int pane_idx = 1;  // 0은 메인 차트이므로 1부터 시작

      for (const auto& [pane_name, indicators] : indicators_group) {
        const auto [idx, series] =
            pane_name == "chart"
                // 메인 차트 인덱스는 항상 0
                ? make_pair(0, ref(indicator_series_to_chart))
                // 페인 인덱스는 1부터 사용 후 증가
                : make_pair(pane_idx++, ref(indicator_series_to_pane));

        // 현재 페인의 모든 지표에 대해 렌더링 수행
        for (const auto& indicator : indicators) {
          series += RenderToSeries(indicator, idx);
        }
      }

      // 거래 마커 추가
      json markers;
      unordered_set<int> added_trade_num;
      const auto& green = "#388e3c";  // Green 700
      const auto& red = "#d32f2f";    // Red 700
      const auto qty_step = symbol_info_[symbol_idx].GetQtyStep();

      for (const auto& trade : trade_list_) {
        // 현재 심볼의 거래만 추가
        if (trade.GetSymbolName() != symbol_name) {
          continue;
        }

        const auto& entry_direction = trade.GetEntryDirection();
        const auto entry_time = UtcDatetimeToUtcTimestamp(trade.GetEntryTime(),
                                                          "%Y-%m-%d %H:%M:%S");
        const auto entry_price = trade.GetEntryPrice();
        const auto entry_size = trade.GetEntrySize();
        ostringstream entry_size_oss;
        entry_size_oss << fixed
                       << setprecision(static_cast<streamsize>(
                              CountDecimalPlaces(qty_step)));
        entry_size_oss << (entry_direction == "매수" ? "+" : "-") << entry_size;

        // 이미 추가된 거래 번호가 아닐 때만 매수 체결 마커 추가
        // -> 분할 청산 시 마커가 누적되는 것을 방지
        if (const auto trade_num = trade.GetTradeNumber();
            !added_trade_num.contains(trade_num)) {
          added_trade_num.insert(trade_num);

          json entry_marker;
          entry_marker["time"] =
              IsTimestampMs(entry_time) ? entry_time / 1000 : entry_time;
          entry_marker["position"] =  // 매수 밑 위치, 매도 위 위치
              entry_direction == "매수" ? "left" : "right";
          entry_marker["marker_price"] = entry_price;
          entry_marker["marker_color"] =  // 롱 Green 700, 숏 Red 700
              entry_direction == "매수" ? green : red;
          entry_marker["text"] = trade.GetEntryName();

          markers.push_back(entry_marker);
        }

        // 매도 체결 마커 추가
        json exit_marker;
        const auto exit_time =
            UtcDatetimeToUtcTimestamp(trade.GetExitTime(), "%Y-%m-%d %H:%M:%S");
        const auto exit_price = trade.GetExitPrice();
        const auto exit_size = trade.GetExitSize();
        ostringstream exit_size_oss;
        exit_size_oss << fixed
                      << setprecision(static_cast<streamsize>(
                             CountDecimalPlaces(qty_step)));
        exit_size_oss << (entry_direction == "매수" ? "-" : "+") << exit_size;

        exit_marker["time"] =
            IsTimestampMs(exit_time) ? exit_time / 1000 : exit_time;
        exit_marker["position"] =  // 매도 위 위치, 매수 밑 위치
            entry_direction == "매수" ? "right" : "left";
        exit_marker["marker_price"] = exit_price;
        exit_marker["marker_color"] =  // 숏 Red 700, 롱 Green 700
            entry_direction == "매수" ? red : green;
        exit_marker["from_time"] =
            IsTimestampMs(entry_time) ? entry_time / 1000 : entry_time;
        exit_marker["from_price"] = entry_price;
        exit_marker["line_color"] =  // 이익 Green 700, 손실 Red 700
            trade.GetPnlNet() >= 0 ? green : red;
        exit_marker["text"] = format("{}", trade.GetExitName());
        exit_marker["tooltip_text"] = format(
            "전략 이름 : {}\\n레버리지 : {}\\n진입 가격 : {}\\n"
            "진입 수량 : {}\\n청산 가격 : {}\\n청산 수량 : {}\\n"
            "순손익 : {}\\n개별 손익률 : {:.2f}%\\n전체 손익률 : {:.2f}%",
            trade.GetStrategyName(), trade.GetLeverage(), entry_price,
            entry_size_oss.str(), exit_price, exit_size_oss.str(),
            FormatDollar(trade.GetPnlNet(), true), trade.GetIndividualPnlPer(),
            trade.GetTotalPnlPer());

        markers.push_back(exit_marker);
      }

      // 바 데이터 JSON 생성
      json bar_data = json::array();
      for (int bar_idx = 0; bar_idx < trading_bar->GetNumBars(symbol_idx);
           bar_idx++) {
        const auto& bar = trading_bar->GetBar(symbol_idx, bar_idx);
        json json_bar;

        auto open_time = bar.open_time;
        // TradingView Lightweight Charts에서 시간은 초 단위 필요
        json_bar["time"] =
            IsTimestampMs(open_time) ? open_time / 1000 : open_time;
        json_bar["open"] = bar.open;
        json_bar["high"] = bar.high;
        json_bar["low"] = bar.low;
        json_bar["close"] = bar.close;
        json_bar["volume"] = bar.volume;

        bar_data.push_back(json_bar);
      }

      // Base Chart에서 렌더링 할 json 생성
      json render;
      const auto tick_size =
          symbol_info_[bar_->GetCurrentSymbolIndex()].GetTickSize();
      render["symbol_name"] = symbol_name;
      render["trading_timeframe"] = trading_timeframe;
      render["indicator_series_to_chart"] = indicator_series_to_chart.empty()
                                                ? "// ※ 차트에 추가할 지표 없음"
                                                : indicator_series_to_chart;
      render["indicator_series_to_pane"] = indicator_series_to_pane.empty()
                                               ? "// ※ 페인에 추가할 지표 없음"
                                               : indicator_series_to_pane;
      render["bar_data"] = bar_data.dump();
      render["markers"] = markers.dump();
      render["pane_count"] = pane_count;
      render["price_precision"] = CountDecimalPlaces(tick_size);
      render["tick_size"] = tick_size;

      // HTML 파일 생성
      const auto& file_path =
          format("{}/Charts/{}.html", main_directory, symbol_name);
      ofstream html_file(file_path);
      if (!html_file.is_open()) {
        throw runtime_error(
            format("[{}] 심볼의 차트 파일 [{}]을(을) 생성할 수 없습니다.",
                   symbol_name, file_path));
      }
      html_file << inja_env_.render(base_chart_, render);
      html_file.close();
    }

    logger_->Log(INFO_L, "백테스팅 차트가 저장되었습니다.", __FILE__, __LINE__);
  } catch (const exception& e) {
    logger_->Log(ERROR_L, e.what(), __FILE__, __LINE__);
    Logger::LogAndThrowError("백테스팅 차트를 저장하는 중 오류가 발생했습니다.",
                             __FILE__, __LINE__);
  }
}

void BaseAnalyzer::SaveTradeList(const string& file_path) const {
  ofstream trade_list_file(file_path);
  if (!trade_list_file.is_open()) {
    Logger::LogAndThrowError(
        format("매매 목록 [{}]을(를) 생성할 수 없습니다.", file_path), __FILE__,
        __LINE__);
  }

  // BOM 추가 (한글 인코딩 깨짐 방지)
  trade_list_file << "\xEF\xBB\xBF";

  // CSV 헤더 작성
  trade_list_file
      << "거래 번호,전략 이름,심볼 이름,진입 이름,청산 이름,진입 방향,"
         "진입 시간,청산 시간,보유 시간,레버리지,진입 가격,진입 수량,"
         "청산 가격,청산 수량,강제 청산 가격,진입 수수료,청산 수수료,"
         "강제 청산 수수료,손익,순손익,개별 손익률,전체 손익률,현재 자금,"
         "최고 자금,드로우다운,최고 드로우다운,누적 손익,누적 손익률,"
         "보유 심볼 수\n";

  // 데이터 작성
  for (const auto& trade : trade_list_) {
    trade_list_file
        << "\"" << trade.GetTradeNumber() << "\","
        << "\"" << trade.GetStrategyName() << "\","
        << "\"" << trade.GetSymbolName() << "\","
        << "\"" << trade.GetEntryName() << "\","
        << "\"" << trade.GetExitName() << "\","
        << "\"" << trade.GetEntryDirection() << "\","
        << "\"" << trade.GetEntryTime() << "\","
        << "\"" << trade.GetExitTime() << "\","
        << "\"" << trade.GetHoldingTime() << "\","
        << "\"" << trade.GetLeverage() << "\","
        << "\"" << trade.GetEntryPrice() << "\","
        << "\"" << trade.GetEntrySize() << "\","
        << "\"" << trade.GetExitPrice() << "\","
        << "\"" << trade.GetExitSize() << "\","
        << "\"" << trade.GetLiquidationPrice() << "\","
        << "\"" << FormatDollar(trade.GetEntryFee(), false) << "\","
        << "\"" << FormatDollar(trade.GetExitFee(), false) << "\","
        << "\"" << FormatDollar(trade.GetLiquidationFee(), false) << "\","
        << "\"" << FormatDollar(trade.GetPnl(), false) << "\","
        << "\"" << FormatDollar(trade.GetPnlNet(), false) << "\","
        << "\""
        << to_string(RoundToDecimalPlaces(trade.GetIndividualPnlPer(), 2)) +
               "%\","
        << "\""
        << to_string(RoundToDecimalPlaces(trade.GetTotalPnlPer(), 2)) + "%\","
        << "\"" << FormatDollar(trade.GetWalletBalance(), false) << "\","
        << "\"" << FormatDollar(trade.GetMaxWalletBalance(), false) << "\","
        << "\""
        << to_string(RoundToDecimalPlaces(trade.GetDrawdown(), 2)) + "%\","
        << "\""
        << to_string(RoundToDecimalPlaces(trade.GetMaxDrawdown(), 2)) + "%\","
        << "\"" << FormatDollar(trade.GetCumPnl(), false) << "\","
        << "\""
        << to_string(RoundToDecimalPlaces(trade.GetCumPnlPer(), 2)) + "%\","
        << "\"" << trade.GetSymbolCount() << "\"\n";
  }

  trade_list_file.close();

  logger_->Log(INFO_L, "매매 목록이 저장되었습니다.", __FILE__, __LINE__);
}

[[nodiscard]] string BaseAnalyzer::RenderToSeries(
    const shared_ptr<Indicator>& indicator, const int pane_idx) {
  const auto& plot = indicator->plot_;
  const auto tick_size =
      symbol_info_[bar_->GetCurrentSymbolIndex()].GetTickSize();

  // 렌더링 할 JSON 생성
  json render;

  // 공통 변수 설정
  render["precision"] = CountDecimalPlaces(tick_size);
  render["tick_size"] = tick_size;
  render["line_style"] = plot->line_style_;
  render["line_width"] = plot->line_width_;
  render["line_type"] = plot->line_type_;
  render["point_markers_visible"] =
      plot->plot_point_markers_ ? "true" : "false";
  render["point_markers_radius"] = plot->point_markers_radius_;
  render["pane_idx"] = pane_idx;
  render["indicator_name"] = indicator->GetName();

  // 플롯 타입별 렌더링 후 반환
  const auto& plot_type = indicator->plot_type_;

  // 영역 플롯 렌더링
  if (plot_type == "Area") {
    const shared_ptr<Area>& area = dynamic_pointer_cast<Area>(plot);

    render["area_series"] = "areaSeries_" + to_string(++area_count_);
    render["top_color"] = area->top_gradient_color_.RgbaToHex();
    render["bottom_color"] = area->bottom_gradient_color_.RgbaToHex();
    render["line_color"] = area->line_color_.RgbaToHex();
    render["data"] = GetParsedJson(indicator);

    return inja_env_.render(area_template_, render);
  }

  // 기준선 플롯 렌더링
  if (plot_type == "Baseline") {
    const shared_ptr<Baseline>& base_line =
        dynamic_pointer_cast<Baseline>(plot);

    render["baseline_series"] =
        "baselineSeries_" + to_string(++base_line_count_);
    render["base_value"] = base_line->base_value_,
    render["top_fill_color1"] = base_line->top_gradient_color1_.RgbaToHex(),
    render["top_fill_color2"] = base_line->top_gradient_color2_.RgbaToHex(),
    render["top_line_color"] = base_line->top_line_color_.RgbaToHex(),
    render["bottom_fill_color1"] =
        base_line->bottom_gradient_color1_.RgbaToHex(),
    render["bottom_fill_color2"] =
        base_line->bottom_gradient_color2_.RgbaToHex(),
    render["bottom_line_color"] = base_line->bottom_line_color_.RgbaToHex();
    render["data"] = GetParsedJson(indicator);

    return inja_env_.render(base_line_template_, render);
  }

  // 히스토그램 플롯 렌더링
  if (plot_type == "Histogram") {
    const shared_ptr<Histogram>& histogram =
        dynamic_pointer_cast<Histogram>(plot);

    render["histogram_series"] =
        "histogramSeries_" + to_string(++histogram_count_);
    render["base_value"] = histogram->base_value_;
    render["data"] =
        GetParsedJson(indicator, histogram->bullish_color_.RgbaToHex(),
                      histogram->bearish_color_.RgbaToHex());

    return inja_env_.render(histogram_template_, render);
  }

  // 선 플롯 렌더링
  if (plot_type == "Line") {
    const shared_ptr<Line>& line = dynamic_pointer_cast<Line>(plot);

    render["line_series"] = "lineSeries_" + to_string(++line_count_);
    render["color"] = line->line_color_.RgbaToHex();
    render["data"] = GetParsedJson(indicator);

    return inja_env_.render(line_template_, render);
  }

  return {};
}

json BaseAnalyzer::GetParsedJson(const shared_ptr<Indicator>& indicator) {
  json data;
  const auto& reference_bar =
      bar_->GetBarData(REFERENCE, indicator->timeframe_);
  const auto symbol_idx = bar_->GetCurrentSymbolIndex();
  const auto& output = indicator->output_[symbol_idx];

  for (int bar_idx = 0; bar_idx < output.size(); bar_idx++) {
    json json_bar;

    auto open_time = reference_bar->GetBar(symbol_idx, bar_idx).open_time;
    json_bar["time"] = IsTimestampMs(open_time) ? open_time / 1000 : open_time;
    json_bar["value"] = static_cast<double>(output[bar_idx]);

    data.push_back(json_bar);
  }

  return data.dump();
}

json BaseAnalyzer::GetParsedJson(const shared_ptr<Indicator>& indicator,
                                 const string& bullish_color,
                                 const string& bearish_color) {
  json data;
  const auto& reference_bar =
      bar_->GetBarData(REFERENCE, indicator->timeframe_);
  const auto symbol_idx = bar_->GetCurrentSymbolIndex();
  const auto& output = indicator->output_[symbol_idx];

  for (int bar_idx = 0; bar_idx < output.size(); bar_idx++) {
    json json_bar;
    const auto& bar = reference_bar->GetBar(symbol_idx, bar_idx);

    auto open_time = bar.open_time;
    json_bar["time"] = IsTimestampMs(open_time) ? open_time / 1000 : open_time;
    json_bar["value"] = static_cast<double>(output[bar_idx]);

    // 양봉, 음봉에 따라 색상을 추가
    if (bar.close >= bar.open) {
      json_bar["color"] = bullish_color;
    } else {
      json_bar["color"] = bearish_color;
    }

    data.push_back(json_bar);
  }

  return data.dump();
}

void BaseAnalyzer::SaveConfig(const string& file_path) {
  ordered_json config;

  const auto& trading_bar = bar_->GetBarData(TRADING);
  const auto& magnifier_bar = bar_->GetBarData(MAGNIFIER);
  const auto& reference_bar = bar_->GetAllReferenceBarData();
  const auto& mark_price_bar = bar_->GetBarData(MARK_PRICE);

  // 심볼 값 배열에 각 심볼의 객체를 push
  auto& symbol_json = config["심볼"];
  for (int symbol_idx = 0; symbol_idx < trading_bar->GetNumSymbols();
       symbol_idx++) {
    // 각 심볼의 배열
    ordered_json local_symbol_json;
    local_symbol_json["심볼명"] = trading_bar->GetSymbolName(symbol_idx);

    // 트레이딩 바 정보 저장
    const auto trading_num_bars = trading_bar->GetNumBars(symbol_idx);
    const auto& [trading_missing_count, trading_missing_times] =
        GetMissingOpenTimes(trading_bar, symbol_idx,
                            engine_->trading_bar_time_diff_);

    local_symbol_json["트레이딩 바 데이터"]["기간"]["시작"] =
        UtcTimestampToUtcDatetime(trading_bar->GetBar(symbol_idx, 0).open_time);
    local_symbol_json["트레이딩 바 데이터"]["기간"]["끝"] =
        UtcTimestampToUtcDatetime(
            trading_bar->GetBar(symbol_idx, trading_num_bars - 1).close_time);
    local_symbol_json["트레이딩 바 데이터"]["타임프레임"] =
        trading_bar->GetTimeframe();
    local_symbol_json["트레이딩 바 데이터"]["바 개수"] = trading_num_bars;
    local_symbol_json["트레이딩 바 데이터"]["누락된 바"]["개수"] =
        trading_missing_count;
    local_symbol_json["트레이딩 바 데이터"]["누락된 바"]["시간"] =
        trading_missing_times;
    local_symbol_json["트레이딩 바 데이터"]["데이터 경로"] =
        trading_bar->GetBarDataPath(symbol_idx);

    // 돋보기 기능 사용 시 돋보기 바 정보 저장
    const auto use_bar_magnifier = config_->GetUseBarMagnifier();
    if (use_bar_magnifier) {
      const auto magnifier_num_bars = magnifier_bar->GetNumBars(symbol_idx);
      const auto& [magnifier_missing_count, magnifier_missing_times] =
          GetMissingOpenTimes(magnifier_bar, symbol_idx,
                              engine_->magnifier_bar_time_diff_);

      local_symbol_json["돋보기 바 데이터"]["기간"]["시작"] =
          UtcTimestampToUtcDatetime(
              magnifier_bar->GetBar(symbol_idx, 0).open_time);
      local_symbol_json["돋보기 바 데이터"]["기간"]["끝"] =
          UtcTimestampToUtcDatetime(
              magnifier_bar->GetBar(symbol_idx, magnifier_num_bars - 1)
                  .close_time);
      local_symbol_json["돋보기 바 데이터"]["타임프레임"] =
          magnifier_bar->GetTimeframe();
      local_symbol_json["돋보기 바 데이터"]["바 개수"] = magnifier_num_bars;
      local_symbol_json["돋보기 바 데이터"]["누락된 바"]["개수"] =
          magnifier_missing_count;
      local_symbol_json["돋보기 바 데이터"]["누락된 바"]["시간"] =
          magnifier_missing_times;
      local_symbol_json["돋보기 바 데이터"]["데이터 경로"] =
          magnifier_bar->GetBarDataPath(symbol_idx);
    } else {
      local_symbol_json["돋보기 바 데이터"] = json::array();
    }

    // 각 참조 바 정보 저장
    for (const auto& [timeframe, bar_data] : reference_bar) {
      ordered_json local_reference_bar_json;
      const auto& [reference_missing_count, reference_missing_times] =
          GetMissingOpenTimes(bar_data, symbol_idx,
                              engine_->reference_bar_time_diff_.at(timeframe));

      const auto reference_num_bars = bar_data->GetNumBars(symbol_idx);
      local_reference_bar_json["기간"]["시작"] =
          UtcTimestampToUtcDatetime(bar_data->GetBar(symbol_idx, 0).open_time);
      local_reference_bar_json["기간"]["끝"] = UtcTimestampToUtcDatetime(
          bar_data->GetBar(symbol_idx, reference_num_bars - 1).close_time);
      local_reference_bar_json["타임프레임"] = timeframe;
      local_reference_bar_json["바 개수"] = reference_num_bars;
      local_reference_bar_json["누락된 바"]["개수"] = reference_missing_count;
      local_reference_bar_json["누락된 바"]["시간"] = reference_missing_times;
      local_reference_bar_json["데이터 경로"] =
          bar_data->GetBarDataPath(symbol_idx);

      local_symbol_json["참조 바 데이터"].push_back(local_reference_bar_json);
    }

    // 마크 가격 바 정보 저장
    const auto mark_price_num_bars = mark_price_bar->GetNumBars(symbol_idx);
    const auto& [mark_price_missing_count, mark_price_missing_times] =
        GetMissingOpenTimes(mark_price_bar, symbol_idx,
                            use_bar_magnifier
                                ? engine_->magnifier_bar_time_diff_
                                : engine_->trading_bar_time_diff_);

    local_symbol_json["마크 가격 바 데이터"]["기간"]["시작"] =
        UtcTimestampToUtcDatetime(
            mark_price_bar->GetBar(symbol_idx, 0).open_time);
    local_symbol_json["마크 가격 바 데이터"]["기간"]["끝"] =
        UtcTimestampToUtcDatetime(
            mark_price_bar->GetBar(symbol_idx, mark_price_num_bars - 1)
                .close_time);
    local_symbol_json["마크 가격 바 데이터"]["타임프레임"] =
        mark_price_bar->GetTimeframe();
    local_symbol_json["마크 가격 바 데이터"]["바 개수"] = mark_price_num_bars;
    local_symbol_json["마크 가격 바 데이터"]["누락된 바"]["개수"] =
        mark_price_missing_count;
    local_symbol_json["마크 가격 바 데이터"]["누락된 바"]["시간"] =
        mark_price_missing_times;
    local_symbol_json["마크 가격 바 데이터"]["데이터 경로"] =
        mark_price_bar->GetBarDataPath(symbol_idx);

    // 한 심볼의 정보저장
    symbol_json.push_back(local_symbol_json);
  }

  // 전략 값 배열에 각 전략의 객체를 push
  auto& strategy_json = config["전략"];
  for (const auto& strategy : engine_->strategies_) {
    ordered_json local_strategy_json;

    local_strategy_json["전략명"] = strategy->GetName();

    // 해당 전략에서 사용하는 지표들을 저장
    for (const auto& indicator : strategy->GetIndicators()) {
      ordered_json local_indicator_json;
      local_indicator_json["지표명"] = indicator->GetName();
      local_indicator_json["타임프레임"] = indicator->GetTimeframe();

      local_strategy_json["지표"].push_back(local_indicator_json);
    }

    strategy_json.push_back(local_strategy_json);
  }

  // 설정 값 배열에 각 설정의 객체를 push
  auto& config_json = config["설정"];
  config_json["루트 폴더"] = config_->GetRootDirectory();
  config_json["바 돋보기"] =
      config_->GetUseBarMagnifier() ? "활성화" : "비활성화";
  config_json["초기 자금"] = FormatDollar(config_->GetInitialBalance(), true);

  ostringstream taker_fee_percentage, maker_fee_percentage,
      taker_slippage_percentage, maker_slippage_percentage;
  taker_fee_percentage << fixed << setprecision(4)
                       << config_->GetTakerFeePercentage();
  maker_fee_percentage << fixed << setprecision(4)
                       << config_->GetMakerFeePercentage();
  taker_slippage_percentage << fixed << setprecision(4)
                            << config_->GetTakerSlippagePercentage();
  maker_slippage_percentage << fixed << setprecision(4)
                            << config_->GetMakerSlippagePercentage();

  config_json["테이커 수수료 퍼센트"] = taker_fee_percentage.str() + "%";
  config_json["메이커 수수료 퍼센트"] = maker_fee_percentage.str() + "%";
  config_json["테이커 슬리피지 퍼센트"] = taker_slippage_percentage.str() + "%";
  config_json["메이커 슬리피지 퍼센트"] = maker_slippage_percentage.str() + "%";

  string bar_type_str[4] = {"트레이딩 바 데이터", "돋보기 바 데이터",
                            "참조 바 데이터", "마크 가격 바 데이터"};
  for (int i = 0; i < 4; i++) {
    config_json["심볼 간 동일한 바 데이터 검사"][bar_type_str[i]] =
        config_->GetCheckSameBarData()[i] ? "활성화" : "비활성화";
  }

  config_json["마크 가격 바 데이터와 동일한 목표 바 데이터 검사"] =
      config_->GetCheckSameBarDataWithTarget() ? "활성화" : "비활성화";

  // 파일로 저장
  ofstream config_file(file_path);
  config_file << setw(4) << config << endl;
  config_file.close();

  logger_->Log(INFO_L, "백테스팅 설정이 저장되었습니다.", __FILE__, __LINE__);
}

void BaseAnalyzer::SaveBacktestingLog(const string& file_path) {
  try {
    logger_->backtesting_log_.close();

    filesystem::rename(logger_->backtesting_log_temp_path_, file_path);
  } catch (const exception& e) {
    Logger::LogAndThrowError(
        "백테스팅 로그 파일을 저장하는 데 오류가 발생했습니다.: " +
            string(e.what()),
        __FILE__, __LINE__);
  }
}

pair<int, vector<string>> BaseAnalyzer::GetMissingOpenTimes(
    const shared_ptr<BarData>& bar_data, const int symbol_idx,
    const int64_t interval) {
  int missing_count = 0;
  vector<string> missing_ranges;

  int64_t range_start = 0;
  int64_t range_end = 0;

  // 누락 구간 안에 있는지 표시하는 플래그
  bool in_range = false;

  // 모든 바를 순회하면서 누락된 시간을 확인
  for (size_t bar_idx = 1; bar_idx < bar_data->GetNumBars(symbol_idx);
       ++bar_idx) {
    // 다음 예상 시간은 이전 바의 open_time에 interval의 합
    int64_t expected =
        bar_data->GetBar(symbol_idx, bar_idx - 1).open_time + interval;

    // 현재 바의 실제 open_time
    const int64_t current = bar_data->GetBar(symbol_idx, bar_idx).open_time;

    // 예상 시간이 실제 시간보다 작으면 누락된 바가 존재
    while (expected < current) {
      if (!in_range) {
        // 누락이 시작되는 첫 시점 저장
        range_start = expected;
        in_range = true;
      }

      // 마지막 누락된 시점을 계속 갱신
      range_end = expected;

      missing_count++;
      expected += interval;
    }

    // 예상 시각이 현재 시각보다 크거나 같아졌으면 누락 구간의 끝
    if (in_range && expected >= current) {
      if (range_start == range_end) {
        // 한 구간만 빠졌을 경우는 단일 시간 문자열로 저장
        missing_ranges.push_back(UtcTimestampToUtcDatetime(range_start));
      } else {
        // 여러 구간이 연속해서 빠졌으면 시작 - 끝 형태로 저장
        missing_ranges.push_back(UtcTimestampToUtcDatetime(range_start) +
                                 " - " + UtcTimestampToUtcDatetime(range_end));
      }

      // 다음 누락 구간 추적을 위해 초기화
      in_range = false;
    }
  }

  return {missing_count, missing_ranges};
}

}  // namespace backtesting::analyzer