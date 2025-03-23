// 표준 라이브러리
#include <format>
#include <fstream>
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
  static string html_directory =
      Engine::GetConfig()->GetRootDirectory() + "/Sources/html";

  try {
    base_chart_ = OpenHtml(html_directory + "/BaseChart.html");
    area_template_ = OpenHtml(html_directory + "/AreaSeriesTemplate.js");
    base_line_template_ =
        OpenHtml(html_directory + "/BaselineSeriesTemplate.js");
    histogram_template_ =
        OpenHtml(html_directory + "/HistogramSeriesTemplate.js");
    line_template_ = OpenHtml(html_directory + "/LineSeriesTemplate.js");
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
    for (int idx = static_cast<int>(trade_list_.size()) - 1; idx >= 0; --idx) {
      if (const auto& trade = trade_list_[idx];
          trade.GetEntryName() == new_trade.GetEntryName() &&
          IsEqual(trade.GetEntryPrice(), new_trade.GetEntryPrice())) {
        trade_num = trade.GetTradeNumber();
        break;
      }
    }

    trade_list_.push_back(new_trade.SetTradeNumber(trade_num));
  }
}

void BaseAnalyzer::SaveTradeList(const string& file_path) const {
  ofstream file(file_path);
  if (!file.is_open()) {
    Logger::LogAndThrowError("파일을 열 수 없습니다", __FILE__, __LINE__);
  }

  // BOM 추가 (한글 인코딩 깨짐 방지)
  file << "\xEF\xBB\xBF";

  // CSV 헤더 작성
  file << "거래 번호,전략 이름,심볼 이름,진입 이름,청산 이름,진입 방향,"
          "진입 시간,청산 시간,보유 시간,레버리지,진입 가격,진입 수량,"
          "청산 가격,청산 수량,강제 청산 가격,진입 수수료,청산 수수료,"
          "강제 청산 수수료,손익,순손익,개별 손익률,전체 손익률,현재 자금,"
          "최고 자금,드로우다운,최고 드로우다운,누적 손익,누적 손익률,"
          "보유 심볼 수\n";

  // 데이터 작성
  for (const auto& trade : trade_list_) {
    file << "\"" << trade.GetTradeNumber() << "\","
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

  file.close();

  logger_->Log(INFO_L, "매매 목록이 저장되었습니다.", __FILE__, __LINE__);
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

        // 이미 추가된 거래 번호가 아닐 때만 매수 체결 마커 추가
        if (const auto trade_num = trade.GetTradeNumber();
            !added_trade_num.contains(trade_num)) {
          added_trade_num.insert(trade_num);

          json entry_marker;
          const auto entry_size = trade.GetEntrySize();
          ostringstream entry_size_oss;
          entry_size_oss << fixed
                         << setprecision(static_cast<streamsize>(
                                CountDecimalPlaces(qty_step)));
          entry_size_oss << (entry_direction == "매수" ? "+" : "-")
                         << entry_size;

          entry_marker["time"] =
              IsTimestampMs(entry_time) ? entry_time / 1000 : entry_time;
          entry_marker["position"] =  // 매수 밑 위치, 매도 위 위치
              entry_direction == "매수" ? "left" : "right";
          entry_marker["marker_price"] = entry_price;
          entry_marker["marker_color"] =  // 롱 Green 700, 숏 Red 700
              entry_direction == "매수" ? green : red;
          entry_marker["text"] = format("{}\\n{} ({})", trade.GetEntryName(),
                                        entry_price, entry_size_oss.str());

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
        exit_marker["text"] = format("{} ({})\\n{}", exit_price,
                                     exit_size_oss.str(), trade.GetExitName());

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
            format("[{}] 심볼의 차트 파일 [{}]을 생성할 수 없습니다.",
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

}  // namespace backtesting::analyzer