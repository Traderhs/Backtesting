// 표준 라이브러리
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <execution>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <iomanip>
#include <numeric>
#include <ranges>
#include <set>
#include <thread>
#include <unordered_set>

// 외부 라이브러리
#include "arrow/array/builder_decimal.h"
#include "arrow/array/builder_primitive.h"
#include "arrow/table.h"
#include "nlohmann/json.hpp"

// 파일 헤더
#include "Engines/Analyzer.hpp"

// 내부 헤더
#include "Engines/BarData.hpp"
#include "Engines/BarHandler.hpp"
#include "Engines/Config.hpp"
#include "Engines/DataUtils.hpp"
#include "Engines/Engine.hpp"
#include "Engines/Logger.hpp"
#include "Engines/Slippage.hpp"
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

Analyzer::Analyzer() : trade_num_(1) {}
void Analyzer::Deleter::operator()(const Analyzer* p) const { delete p; }

mutex Analyzer::mutex_;
shared_ptr<Analyzer> Analyzer::instance_;

shared_ptr<BarHandler>& Analyzer::bar_ = BarHandler::GetBarHandler();
shared_ptr<Config>& Analyzer::config_ = Engine::GetConfig();
shared_ptr<Engine>& Analyzer::engine_ = Engine::GetEngine();
shared_ptr<Logger>& Analyzer::logger_ = Logger::GetLogger();
vector<SymbolInfo> Analyzer::symbol_info_;

shared_ptr<Analyzer>& Analyzer::GetAnalyzer() {
  lock_guard lock(mutex_);  // 스레드에서 안전하게 접근하기 위해 mutex 사용

  // 인스턴스가 생성됐는지 확인
  if (!instance_) {
    // 인스턴스가 생성되지 않았으면 생성 후 저장
    instance_ = shared_ptr<Analyzer>(new Analyzer(), Deleter());
  }

  return instance_;
}

void Analyzer::Initialize(const int64_t begin_open_time,
                          const int64_t end_close_time,
                          const double initial_balance) {
  // 백테스팅 기간 초기화
  begin_open_time_ = UtcTimestampToUtcDatetime(begin_open_time);
  end_close_time_ = UtcTimestampToUtcDatetime(end_close_time);

  if (trade_list_.empty()) {
    // 매매 목록 0행 초기화
    trade_list_.push_back(Trade()
                              .SetTradeNumber(0)
                              .SetSymbolName("-")
                              .SetEntryName("-")
                              .SetExitName("-")
                              .SetEntryDirection("-")
                              .SetEntryTime("-")
                              .SetExitTime("-")
                              .SetHoldingTime("-")
                              .SetWalletBalance(initial_balance)
                              .SetMaxWalletBalance(initial_balance));
  } else {
    Logger::LogAndThrowError(
        "분석기가 이미 초기화되어 다시 초기화할 수 없습니다.", __FILE__,
        __LINE__);
  }
}

void Analyzer::SetSymbolInfo(const vector<SymbolInfo>& symbol_info) {
  if (symbol_info_.empty()) {
    symbol_info_ = symbol_info;
  } else [[unlikely]] {
    Logger::LogAndThrowError(
        "심볼 정보가 이미 초기화되어 다시 초기화할 수 없습니다.", __FILE__,
        __LINE__);
  }
}

void Analyzer::AddTrade(Trade& new_trade, const int exit_count) {
  if (exit_count == 1) {
    // 첫 청산에서 전량 청산 거래거나 첫 분할 청산 거래인 경우 거래 번호 추가
    trade_list_.push_back(new_trade.SetTradeNumber(trade_num_++));
  } else {
    // 두 번째 분할 청산 거래부터는 첫 분할 청산 거래의 진입 거래 번호가
    // 거래 번호가 됨
    int trade_num = 1;
    for (const auto& trade : ranges::reverse_view(trade_list_)) {
      if (trade.GetEntryName() == new_trade.GetEntryName() &&
          IsEqual(trade.GetLeverage(), new_trade.GetLeverage()) &&
          IsEqual(trade.GetEntryPrice(), new_trade.GetEntryPrice()) &&
          IsEqual(trade.GetEntrySize(), new_trade.GetEntrySize())) {
        trade_num = trade.GetTradeNumber();
        break;
      }
    }

    trade_list_.push_back(new_trade.SetTradeNumber(trade_num));
  }
}

string Analyzer::GetMainDirectory() const { return main_directory_; }

// =============================================================================
void Analyzer::CreateDirectories() {
  try {
    // 현재 시간이 이번 백테스팅의 메인 폴더
    string main_directory = GetCurrentLocalDatetime();

    // 시간 구분 문자 제거 및 공백 언더 스코어화
    erase(main_directory, ':');
    erase(main_directory, '-');
    ranges::replace(main_directory, ' ', '_');

    // 최종 경로로 수정
    // ':' 문자 언더 스코어화 시 드라이브 경로가 변경되므로 마지막에 수정
    main_directory =
        Config::GetProjectDirectory() + "/Results/" + main_directory;
    main_directory_ = main_directory;

    // 지표 데이터 저장 폴더 생성
    filesystem::create_directories(main_directory + "/Backboard/Indicators");

    // 소스 코드 저장 폴더 생성
    filesystem::create_directories(
        format("{}/Backboard/Sources", main_directory));
  } catch (const exception& e) {
    logger_->Log(ERROR_L, e.what(), __FILE__, __LINE__, true);
    Logger::LogAndThrowError("폴더 생성 중 에러가 발생했습니다.", __FILE__,
                             __LINE__);
  }
}

void Analyzer::SaveIndicatorData() {
  try {
    const auto& strategy = engine_->strategy_;
    const auto& indicators = strategy->GetIndicators();

    // 미리 계산된 값들
    const auto& trading_bar_data = bar_->GetBarData(TRADING, "");
    const int num_symbols = trading_bar_data->GetNumSymbols();
    const int64_t trading_time_diff = engine_->trading_bar_time_diff_;

    arrow::MemoryPool* pool = arrow::default_memory_pool();

    // 기본 시간 벡터 생성
    vector<int64_t> time_vector;
    {
      // 예상 크기 계산 및 메모리 예약
      size_t estimated_size = 0;
      for (int symbol_idx = 0; symbol_idx < num_symbols; symbol_idx++) {
        estimated_size += trading_bar_data->GetNumBars(symbol_idx);
      }
      time_vector.reserve(estimated_size);

      // 모든 심볼의 모든 바를 순회하며 open_time 값을 수집
      for (int symbol_idx = 0; symbol_idx < num_symbols; symbol_idx++) {
        const auto num_bars = trading_bar_data->GetNumBars(symbol_idx);
        for (size_t bar_idx = 0; bar_idx < num_bars; bar_idx++) {
          time_vector.push_back(
              trading_bar_data->GetBar(symbol_idx, bar_idx).open_time);
        }
      }

      // 중복 제거 및 정렬 (unordered_set 사용으로 최적화)
      unordered_set unique_times(time_vector.begin(), time_vector.end());
      time_vector.assign(unique_times.begin(), unique_times.end());
      sort(execution::par_unseq, time_vector.begin(), time_vector.end());
    }

    // time 배열 생성
    shared_ptr<arrow::Array> time_array;
    {
      arrow::TimestampBuilder time_builder(timestamp(arrow::TimeUnit::MILLI),
                                           pool);
      auto status = time_builder.AppendValues(time_vector);
      if (status.ok()) {
        status = time_builder.Finish(&time_array);
      }

      if (!status.ok()) {
        throw runtime_error(status.message());
      }
    }

    // 테이블 스키마 생성
    vector schema_fields = {field("time", timestamp(arrow::TimeUnit::MILLI))};
    auto base_schema = make_shared<arrow::Schema>(schema_fields);
    auto base_table = arrow::Table::Make(
        base_schema, {make_shared<arrow::ChunkedArray>(time_array)});

    const size_t total_rows = time_vector.size();

    // 모든 전략의 각 지표를 순회하며 저장
    for (const auto& indicator : indicators) {
      // OHLCV와 플롯하지 않는 지표는 저장하지 않음
      const auto& indicator_name = indicator->GetName();
      if (indicator_name == strategy->open.GetName() ||
          indicator_name == strategy->high.GetName() ||
          indicator_name == strategy->low.GetName() ||
          indicator_name == strategy->close.GetName() ||
          indicator_name == strategy->volume.GetName() ||
          indicator->plot_type_ == "Null") {
        continue;
      }

      // 테이블 복사
      auto table = base_table;

      // 지표의 타임프레임이 트레이딩 바 타임프레임보다 큰지 확인
      const auto& timeframe = indicator->GetTimeframe();
      const auto& reference_bar_data = bar_->GetBarData(REFERENCE, timeframe);
      const bool is_indicator_timeframe_larger =
          trading_time_diff < ParseTimeframe(timeframe);

      // 참조 바 close_time 캐싱 (큰 타임프레임인 경우)
      vector<vector<int64_t>> reference_close_times_cache;

      // 참조 바 타임프레임
      const int64_t reference_time_diff = ParseTimeframe(timeframe);

      // 각 심볼의 마지막 트레이딩 바 close_time
      vector<int64_t> last_trading_close_times;

      if (is_indicator_timeframe_larger) {
        reference_close_times_cache.resize(num_symbols);
        last_trading_close_times.resize(num_symbols);

        for (int symbol_idx = 0; symbol_idx < num_symbols; symbol_idx++) {
          // 참조 바 데이터 정보 캐싱
          const size_t reference_num_bars =
              reference_bar_data->GetNumBars(symbol_idx);
          auto& reference_close_times = reference_close_times_cache[symbol_idx];
          reference_close_times.reserve(reference_num_bars);

          // 참조 바 데이터 캐싱
          for (size_t i = 0; i < reference_num_bars; i++) {
            reference_close_times.push_back(
                reference_bar_data->GetBar(symbol_idx, i).close_time);
          }

          // 트레이딩 바 마지막 close_time 저장
          last_trading_close_times[symbol_idx] =
              trading_bar_data
                  ->GetBar(symbol_idx,
                           trading_bar_data->GetNumBars(symbol_idx) - 1)
                  .close_time;
        }
      }

      const auto& indicator_output = indicator->output_;
      const auto num_indicator_symbols = indicator_output.size();

      // 심볼별 처리를 병렬화
      vector<pair<string, shared_ptr<arrow::Array>>> symbol_arrays(
          num_indicator_symbols);
      vector<int> symbol_indices(num_indicator_symbols);
      iota(symbol_indices.begin(), symbol_indices.end(), 0);

      for_each(
          execution::par_unseq, symbol_indices.begin(), symbol_indices.end(),
          [&](const int symbol_idx) {
            vector<double> value_vector(total_rows);

            if (is_indicator_timeframe_larger) {
              // 지표의 타임프레임이 트레이딩 바보다 큰 경우 특별 처리
              const auto& output = indicator_output[symbol_idx];
              const auto& reference_close_times =
                  reference_close_times_cache[symbol_idx];
              const size_t reference_num_bars = reference_close_times.size();
              const int64_t last_trading_close_time =
                  last_trading_close_times[symbol_idx];

              // 포워드 필 제한 시간: 참조 바 마지막 close_time +
              // 참조 바 time diff - 트레이딩 바 time diff
              const int64_t forward_fill_limit =
                  reference_close_times[reference_num_bars - 1] +
                  reference_time_diff - trading_time_diff;

              // 시간 정렬이 보장되므로, 마지막으로 사용한 인덱스부터 검색
              size_t last_found_idx = 0;

              for (size_t row_idx = 0; row_idx < total_rows; row_idx++) {
                const int64_t current_time = time_vector[row_idx];
                // 현재 트레이딩 바의 close_time
                const int64_t current_bar_close_time =
                    current_time + trading_time_diff - 1;

                // 1. 해당 심볼의 트레이딩 바 Close Time을 초과하면 종료
                if (current_bar_close_time > last_trading_close_time) {
                  value_vector[row_idx] = NAN;
                  continue;
                }

                // 지표 바의 인덱스 찾기
                size_t reference_bar_idx = 0;
                bool found = false;

                // 시간이 순차적으로 증가하므로 이전에 찾은 인덱스부터 시작
                for (size_t bar_idx = last_found_idx;
                     bar_idx < reference_num_bars; bar_idx++) {
                  // 현재 트레이딩 바의 close_time이 참조 바의
                  // close_time보다 작으면 아직 완성되지 않은 참조 바이므로
                  // 이전 바 사용
                  if (current_bar_close_time < reference_close_times[bar_idx]) {
                    // 첫 참조 바는 미해당
                    if (bar_idx > 0) {
                      reference_bar_idx = bar_idx - 1;
                      found = true;
                      last_found_idx = bar_idx - 1;  // 다음 검색 시작점
                    }

                    break;
                  }

                  // 마지막 참조 바이거나 현재 참조 바의 close_time과
                  // 정확히 일치하면
                  if (bar_idx == reference_num_bars - 1 ||
                      current_bar_close_time ==
                          reference_close_times[bar_idx]) {
                    reference_bar_idx = bar_idx;
                    found = true;
                    last_found_idx = bar_idx;  // 다음 검색 시작점

                    break;
                  }
                }

                // 아직 첫 번째 지표 바가 완성되지 않은 경우 또는
                // 지표 바를 찾지 못한 경우
                if (!found || reference_bar_idx >= output.size()) {
                  value_vector[row_idx] = NAN;
                  continue;
                }

                // 2. 포워드 필 제한: 참조 바가 트레이딩 바보다 먼저 끝난 경우
                //    다음 참조 바 업데이트 시점 전 트레이딩 바까지만 포워드 필
                if (reference_bar_idx == reference_num_bars - 1 &&
                    current_bar_close_time > forward_fill_limit) {
                  value_vector[row_idx] = NAN;
                  continue;
                }

                // 현재 트레이딩 바에 대응하는 지표 값 할당
                value_vector[row_idx] = output[reference_bar_idx];
              }
            } else {
              // 타임프레임이 같은 경우
              // 시간 벡터는 모든 심볼의 최대 시간 범위이므로,
              // output의 시간 범위는 시간 벡터 범위와 다름
              const auto& output = indicator_output[symbol_idx];
              size_t bar_idx = 0;

              for (size_t row_idx = 0; row_idx < total_rows; row_idx++) {
                if (bar_idx < reference_bar_data->GetNumBars(symbol_idx)) {
                  if (const int64_t current_time = time_vector[row_idx];
                      current_time ==
                      reference_bar_data->GetBar(symbol_idx, bar_idx)
                          .open_time) {
                    value_vector[row_idx] = output[bar_idx];
                    bar_idx++;
                  } else {
                    value_vector[row_idx] = NAN;
                  }
                } else {
                  value_vector[row_idx] = NAN;
                }
              }
            }

            // 값 배열 생성
            shared_ptr<arrow::Array> value_array;
            arrow::DoubleBuilder value_builder(pool);
            auto status = value_builder.AppendValues(value_vector);

            if (status.ok()) {
              status = value_builder.Finish(&value_array);
            }

            if (!status.ok()) {
              throw runtime_error(status.message());
            }

            symbol_arrays[symbol_idx] = make_pair(
                engine_->symbol_names_[symbol_idx], move(value_array));
          });

      // 테이블에 컬럼 추가
      for (const auto& [symbol_name, value_array] : symbol_arrays) {
        auto value_field = field(symbol_name, arrow::float64());
        table = table
                    ->AddColumn(table->num_columns(), value_field,
                                make_shared<arrow::ChunkedArray>(value_array))
                    .ValueOrDie();
      }

      // 테이블을 parquet로 저장
      TableToParquet(
          table,
          format("{}/Backboard/Indicators/{}", main_directory_, indicator_name),
          indicator_name + ".parquet", true, false);
    }

    logger_->Log(INFO_L, "지표 데이터가 저장되었습니다.", __FILE__, __LINE__,
                 true);
  } catch (const exception& e) {
    logger_->Log(ERROR_L, e.what(), __FILE__, __LINE__, true);
    Logger::LogAndThrowError("지표 데이터를 저장하는 중 오류가 발생했습니다.",
                             __FILE__, __LINE__);
  }
}

void Analyzer::SaveTradeList() const {
  const auto& file_path = main_directory_ + "/Backboard/trade_list.json";

  ofstream trade_list_file(file_path);
  if (!trade_list_file.is_open()) {
    Logger::LogAndThrowError(
        format("거래 내역 [{}]을(를) 생성할 수 없습니다.", file_path), __FILE__,
        __LINE__);
  }

  ordered_json trade_list_json = json::array();  // JSON 배열로 시작
  const string& strategy_name =
      engine_->strategy_->GetName();  // 전략 이름 캐싱

  for (const auto& trade : trade_list_) {
    const auto trade_num = trade.GetTradeNumber();
    const ordered_json& trade_json = {
        {"거래 번호", trade_num},
        {"전략 이름", trade_num == 0 ? "-" : strategy_name},
        {"심볼 이름", trade.GetSymbolName()},
        {"진입 이름", trade.GetEntryName()},
        {"청산 이름", trade.GetExitName()},
        {"진입 방향", trade.GetEntryDirection()},
        {"진입 시간", trade.GetEntryTime()},
        {"청산 시간", trade.GetExitTime()},
        {"보유 시간", trade.GetHoldingTime()},
        {"레버리지", trade.GetLeverage()},
        {"진입 가격", trade.GetEntryPrice()},
        {"진입 수량", trade.GetEntrySize()},
        {"청산 가격", trade.GetExitPrice()},
        {"청산 수량", trade.GetExitSize()},
        {"강제 청산 가격", trade.GetLiquidationPrice()},
        {"펀딩 수령 횟수", trade.GetReceivedFundingCount()},
        {"펀딩비 수령", trade.GetReceivedFundingAmount()},
        {"펀딩 지불 횟수", trade.GetPaidFundingCount()},
        {"펀딩비 지불", trade.GetPaidFundingAmount()},
        {"펀딩 횟수", trade.GetTotalFundingCount()},
        {"펀딩비", trade.GetTotalFundingAmount()},
        {"진입 수수료", trade.GetEntryFee()},
        {"청산 수수료", trade.GetExitFee()},
        {"강제 청산 수수료", trade.GetLiquidationFee()},
        {"손익", trade.GetPnl()},
        {"순손익", trade.GetNetPnl()},
        {"개별 순손익률", trade.GetIndividualPnlPer()},
        {"전체 순손익률", trade.GetTotalPnlPer()},
        {"현재 자금", trade.GetWalletBalance()},
        {"최고 자금", trade.GetMaxWalletBalance()},
        {"드로우다운", trade.GetDrawdown()},
        {"최고 드로우다운", trade.GetMaxDrawdown()},
        {"누적 손익", trade.GetCumPnl()},
        {"누적 손익률", trade.GetCumPnlPer()},
        {"보유 심볼 수", trade.GetSymbolCount()}};

    trade_list_json.push_back(trade_json);
  }

  // UTF-8 BOM 추가
  trade_list_file << "\xEF\xBB\xBF";

  // JSON 문자열로 저장
  trade_list_file << trade_list_json.dump(2);

  trade_list_file.close();

  logger_->Log(INFO_L, "거래 내역이 저장되었습니다.", __FILE__, __LINE__, true);
}

void Analyzer::SaveConfig() {
  ordered_json config;

  // 미리 계산된 데이터
  const auto& trading_bar_data = bar_->GetBarData(TRADING, "");
  const auto& magnifier_bar_data = bar_->GetBarData(MAGNIFIER, "");
  const auto& reference_bar_data = bar_->GetAllReferenceBarData();
  const auto& mark_price_bar_data = bar_->GetBarData(MARK_PRICE, "");
  const auto& strategy = engine_->strategy_;
  const auto& strategy_class_name = strategy->GetClassName();
  const auto& strategy_name = strategy->GetName();
  const auto use_bar_magnifier = *config_->GetUseBarMagnifier();
  const int num_symbols = trading_bar_data->GetNumSymbols();

  // 공통 경로 캐싱
  const string backboard_path = format("{}/Backboard", main_directory_);
  const string sources_path = format("{}/Sources", backboard_path);
  const string indicators_path = format("{}/Indicators", backboard_path);

  // 심볼 배열 예약
  config["심볼"] = ordered_json::array();

  // 심볼별 처리를 위한 데이터 구조
  vector<ordered_json> symbol_configs(num_symbols);
  vector<int> symbol_indices(num_symbols);
  iota(symbol_indices.begin(), symbol_indices.end(), 0);

  // 심볼별 처리 병렬화
  for_each(
      execution::par_unseq, symbol_indices.begin(), symbol_indices.end(),
      [&](const int symbol_idx) {
        ordered_json& symbol = symbol_configs[symbol_idx];

        // 심볼 이름 미리 저장
        symbol["심볼 이름"] = engine_->symbol_names_[symbol_idx];

        // 심볼 정보 참조
        auto& symbol_info = symbol_info_[symbol_idx];

        // 거래소 정보 저장
        symbol["거래소 정보"] = {
            {"데이터 경로", symbol_info.GetExchangeInfoPath()},
            {"가격 최소 단위", symbol_info.GetPriceStep()},
            {"가격 소수점 정밀도", symbol_info.GetPricePrecision()},
            {"수량 최소 단위", symbol_info.GetQtyStep()},
            {"수량 소수점 정밀도", symbol_info.GetQtyPrecision()},
            {"지정가 최대 수량", symbol_info.GetLimitMaxQty()},
            {"지정가 최소 수량", symbol_info.GetLimitMinQty()},
            {"시장가 최대 수량", symbol_info.GetMarketMaxQty()},
            {"시장가 최소 수량", symbol_info.GetMarketMinQty()},
            {"최소 명목 가치", symbol_info.GetMinNotionalValue()},
            {"강제 청산 수수료율", symbol_info.GetLiquidationFeeRate()}};

        // 레버리지 구간 저장
        const auto& leverage_brackets = symbol_info.GetLeverageBracket();
        symbol["레버리지 구간"]["데이터 경로"] =
            symbol_info.GetLeverageBracketPath();
        symbol["레버리지 구간"]["구간"] = ordered_json::array();
        int bracket_num = 1;
        for (const auto& [min_notional_value, max_notional_value, max_leverage,
                          maintenance_margin_rate, maintenance_amount] :
             leverage_brackets) {
          symbol["레버리지 구간"]["구간"].push_back(
              {{"구간 번호", bracket_num++},
               {"최소 명목 가치", min_notional_value},
               {"최대 명목 가치", max_notional_value},
               {"최대 레버리지", max_leverage},
               {"유지 마진율", maintenance_margin_rate},
               {"유지 금액", maintenance_amount}});
        }

        // 펀딩 비율 저장
        const auto& funding_rates = symbol_info.GetFundingRates();
        int positive_funding_count = 0, negative_funding_count = 0;
        double max_funding_rate = 0, min_funding_rate = 0;
        double total_funding_rate = 0;

        for (const auto& funding_info : funding_rates) {
          const auto funding_rate = funding_info.funding_rate;
          total_funding_rate += funding_rate;

          if (funding_rate > 0) {
            positive_funding_count++;

            if (funding_rate > max_funding_rate) {
              max_funding_rate = funding_rate;
            }
          } else if (funding_rate < 0) {
            negative_funding_count++;

            if (funding_rate < min_funding_rate) {
              min_funding_rate = funding_rate;
            }
          }
        }

        // 평균 펀딩 비율 계산 (소수점 8자리에서 반올림)
        // -> 백보드에서는 100을 곱하므로 6자리로 보임
        double average_funding_rate = 0;
        if (!funding_rates.empty()) {
          average_funding_rate =
              total_funding_rate / static_cast<double>(funding_rates.size());
          average_funding_rate = round(average_funding_rate * 1e8) / 1e8;
        }

        symbol["펀딩 비율"] = {
            {"데이터 경로", symbol_info.GetFundingRatesPath()},
            {"데이터 기간",
             {{"시작",
               UtcTimestampToUtcDatetime(funding_rates.front().funding_time)},
              {"종료",
               UtcTimestampToUtcDatetime(funding_rates.back().funding_time)}}},
            {"합계 펀딩 횟수", funding_rates.size()},
            {"양수 펀딩 횟수", positive_funding_count},
            {"음수 펀딩 횟수", negative_funding_count},
            {"평균 펀딩 비율", average_funding_rate},
            {"최고 펀딩 비율", max_funding_rate},
            {"최저 펀딩 비율", min_funding_rate}};

        // 트레이딩 바 정보 저장
        const auto trading_num_bars = trading_bar_data->GetNumBars(symbol_idx);
        const auto& [trading_missing_count, trading_missing_times] =
            FindMissingBars(trading_bar_data, symbol_idx,
                            engine_->trading_bar_time_diff_);

        symbol["트레이딩 바 데이터"] = {
            {"데이터 경로", trading_bar_data->GetBarDataPath(symbol_idx)},
            {"데이터 기간",
             {{"시작", UtcTimestampToUtcDatetime(
                           trading_bar_data->GetBar(symbol_idx, 0).open_time)},
              {"종료",
               UtcTimestampToUtcDatetime(
                   trading_bar_data->GetBar(symbol_idx, trading_num_bars - 1)
                       .close_time)}}},
            {"타임프레임", trading_bar_data->GetTimeframe()},
            {"바 개수", trading_num_bars},
            {"누락된 바",
             {{"개수", trading_missing_count},
              {"시간", trading_missing_times}}}};

        // 돋보기 기능 사용 시 돋보기 바 정보 저장
        if (use_bar_magnifier) {
          const auto magnifier_num_bars =
              magnifier_bar_data->GetNumBars(symbol_idx);
          const auto& [magnifier_missing_count, magnifier_missing_times] =
              FindMissingBars(magnifier_bar_data, symbol_idx,
                              engine_->magnifier_bar_time_diff_);

          symbol["돋보기 바 데이터"] = {
              {"데이터 경로", magnifier_bar_data->GetBarDataPath(symbol_idx)},
              {"데이터 기간",
               {{"시작",
                 UtcTimestampToUtcDatetime(
                     magnifier_bar_data->GetBar(symbol_idx, 0).open_time)},
                {"종료", UtcTimestampToUtcDatetime(
                             magnifier_bar_data
                                 ->GetBar(symbol_idx, magnifier_num_bars - 1)
                                 .close_time)}}},
              {"타임프레임", magnifier_bar_data->GetTimeframe()},
              {"바 개수", magnifier_num_bars},
              {"누락된 바",
               {{"개수", magnifier_missing_count},
                {"시간", magnifier_missing_times}}}};
        } else {
          symbol["돋보기 바 데이터"] = ordered_json::object();
        }

        // 참조 바 정보 저장
        symbol["참조 바 데이터"] = ordered_json::array();

        for (const auto& [timeframe, bar_data] : reference_bar_data) {
          const auto& [reference_missing_count, reference_missing_times] =
              FindMissingBars(bar_data, symbol_idx,
                              engine_->reference_bar_time_diff_.at(timeframe));

          const auto reference_num_bars = bar_data->GetNumBars(symbol_idx);

          symbol["참조 바 데이터"].push_back(
              {{"데이터 경로", bar_data->GetBarDataPath(symbol_idx)},
               {"데이터 기간",
                {{"시작", UtcTimestampToUtcDatetime(
                              bar_data->GetBar(symbol_idx, 0).open_time)},
                 {"종료",
                  UtcTimestampToUtcDatetime(
                      bar_data->GetBar(symbol_idx, reference_num_bars - 1)
                          .close_time)}}},
               {"타임프레임", timeframe},
               {"바 개수", reference_num_bars},
               {"누락된 바",
                {{"개수", reference_missing_count},
                 {"시간", reference_missing_times}}}});
        }

        // 마크 가격 바 정보 저장
        const auto mark_price_num_bars =
            mark_price_bar_data->GetNumBars(symbol_idx);
        const auto& [mark_price_missing_count, mark_price_missing_times] =
            FindMissingBars(mark_price_bar_data, symbol_idx,
                            use_bar_magnifier
                                ? engine_->magnifier_bar_time_diff_
                                : engine_->trading_bar_time_diff_);

        symbol["마크 가격 바 데이터"] = {
            {"데이터 경로", mark_price_bar_data->GetBarDataPath(symbol_idx)},
            {"데이터 기간",
             {{"시작",
               UtcTimestampToUtcDatetime(
                   mark_price_bar_data->GetBar(symbol_idx, 0).open_time)},
              {"종료", UtcTimestampToUtcDatetime(
                           mark_price_bar_data
                               ->GetBar(symbol_idx, mark_price_num_bars - 1)
                               .close_time)}}},
            {"타임프레임", mark_price_bar_data->GetTimeframe()},
            {"바 개수", mark_price_num_bars},
            {"누락된 바",
             {{"개수", mark_price_missing_count},
              {"시간", mark_price_missing_times}}}};
      });

  // 병렬 처리 결과를 메인 config에 추가
  for (auto& symbol_config : symbol_configs) {
    config["심볼"].push_back(move(symbol_config));
  }

  // 전략 정보 저장
  config["전략"] = {{"헤더 파일 경로",
                     format("{}/{}.hpp", sources_path, strategy_class_name)},
                    {"소스 파일 경로",
                     format("{}/{}.cpp", sources_path, strategy_class_name)},
                    {"전략 클래스 이름", strategy_class_name},
                    {"전략 이름", strategy_name}};

  // 지표들의 정보 저장
  const auto& indicators = strategy->GetIndicators();
  config["지표"] = ordered_json::array();

  // OHLCV 지표 이름 캐싱
  const string open_name = strategy->open.GetName();
  const string high_name = strategy->high.GetName();
  const string low_name = strategy->low.GetName();
  const string close_name = strategy->close.GetName();
  const string volume_name = strategy->volume.GetName();

  // 필터링된 지표만 처리
  for (const auto& indicator : indicators) {
    const auto& indicator_name = indicator->GetName();

    // OHLCV를 제외한 지표 정보 기록
    if (indicator_name == open_name || indicator_name == high_name ||
        indicator_name == low_name || indicator_name == close_name ||
        indicator_name == volume_name) {
      continue;
    }

    const auto& indicator_class_name = indicator->GetClassName();

    ordered_json indicator_json = {
        {"데이터 경로", format("{}/{}/{}.parquet", indicators_path,
                               indicator_name, indicator_name)},
        {"헤더 파일 경로",
         format("{}/{}.hpp", sources_path, indicator_class_name)},
        {"소스 파일 경로",
         format("{}/{}.cpp", sources_path, indicator_class_name)},
        {"지표 클래스 이름", indicator_class_name},
        {"지표 이름", indicator_name},
        {"타임프레임", indicator->GetTimeframe()}};

    ParsePlotInfo(indicator_json, indicator);  // 플롯 정보
    config["지표"].push_back(move(indicator_json));
  }

  // 엔진 설정 저장
  config["엔진 설정"] = {
      {"프로젝트 폴더", Config::GetProjectDirectory()},
      {"백테스팅 기간",
       {{"시작", begin_open_time_}, {"종료", end_close_time_}}},
      {"바 돋보기 기능", use_bar_magnifier ? "활성화" : "비활성화"},
      {"초기 자금", FormatDollar(config_->GetInitialBalance(), true)},
      {"테이커 수수료율",
       FormatPercentage(config_->GetTakerFeePercentage(), false)},
      {"메이커 수수료율",
       FormatPercentage(config_->GetMakerFeePercentage(), false)}};

  // 슬리피지 정보 저장
  const auto& slippage = config_->GetSlippage();
  if (const auto* percentage_slippage =
          dynamic_cast<PercentageSlippage*>(slippage.get())) {
    config["엔진 설정"]["슬리피지 모델"] = "퍼센트 슬리피지";
    config["엔진 설정"]["테이커 슬리피지율"] = FormatPercentage(
        percentage_slippage->GetTakerSlippagePercentage(), false);
    config["엔진 설정"]["메이커 슬리피지율"] = FormatPercentage(
        percentage_slippage->GetMakerSlippagePercentage(), false);
  } else if (const auto* market_impact_slippage =
                 dynamic_cast<MarketImpactSlippage*>(slippage.get())) {
    config["엔진 설정"]["슬리피지 모델"] = "시장 충격 슬리피지";
    config["엔진 설정"]["슬리피지 스트레스 계수"] =
        format("{}배", market_impact_slippage->GetStressMultiplier());
  }

  // 검사 옵션들 추가
  config["엔진 설정"]["지정가 최대 수량 검사"] =
      *config_->GetCheckLimitMaxQty() ? "활성화" : "비활성화";
  config["엔진 설정"]["지정가 최소 수량 검사"] =
      *config_->GetCheckLimitMinQty() ? "활성화" : "비활성화";
  config["엔진 설정"]["시장가 최대 수량 검사"] =
      *config_->GetCheckMarketMaxQty() ? "활성화" : "비활성화";
  config["엔진 설정"]["시장가 최소 수량 검사"] =
      *config_->GetCheckMarketMinQty() ? "활성화" : "비활성화";
  config["엔진 설정"]["최소 명목 가치 검사"] =
      *config_->GetCheckMinNotionalValue() ? "활성화" : "비활성화";
  config["엔진 설정"]["마크 가격 바 데이터와 목표 바 데이터 중복 검사"] =
      config_->GetCheckSameBarDataWithTarget() ? "활성화" : "비활성화";

  // 심볼 간 바 데이터 중복 검사 설정
  const vector<string> bar_data_type_str = {
      "트레이딩 바 데이터", "돋보기 바 데이터", "참조 바 데이터",
      "마크 가격 바 데이터"};
  const auto& check_same_bar_data = config_->GetCheckSameBarData();

  auto& same_bar_check = config["엔진 설정"]["심볼 간 바 데이터 중복 검사"];
  for (int i = 0; i < 4; i++) {
    same_bar_check[bar_data_type_str[i]] =
        check_same_bar_data[i] ? "활성화" : "비활성화";
  }

  // 파일로 저장
  ofstream config_file(format("{}/config.json", backboard_path));
  config_file << setw(4) << config << endl;
  config_file.close();

  logger_->Log(INFO_L, "백테스팅 설정이 저장되었습니다.", __FILE__, __LINE__,
               true);
}

void Analyzer::SaveSourcesAndHeaders() {
  try {
    const auto& strategy = engine_->strategy_;
    const auto& strategy_class_name = strategy->GetClassName();
    set<string> saved_indicator_classes;  // 이미 저장한 지표 클래스명 집합

    // 보조: 프로젝트 상위 폴더에서 파일을 검색하는 헬퍼
    const auto find_file_in_parent = [&](const string& filename) -> string {
      try {
        const filesystem::path parent =
            filesystem::path(Config::GetProjectDirectory()).parent_path();

        for (const auto& entry :
             filesystem::recursive_directory_iterator(parent)) {
          if (entry.is_regular_file() && entry.path().filename() == filename) {
            return entry.path().string();
          }
        }
      } catch (...) {
        // 검색 중 오류 발생 시 무시하고 빈 문자열 반환
      }

      return {};
    };

    // 전략 소스 파일 저장 (존재하지 않으면 프로젝트 상위 폴더에서 검색)
    const auto& source_path = strategy->GetSourcePath();
    string source_path_to_copy;

    if (!source_path.empty() && filesystem::exists(source_path)) {
      source_path_to_copy = source_path;
    } else {
      source_path_to_copy = find_file_in_parent(strategy_class_name + ".cpp");

      if (source_path_to_copy.empty()) {
        logger_->Log(WARN_L,
                     format("전략 소스 파일을 찾을 수 없습니다: {}",
                            strategy_class_name + ".cpp"),
                     __FILE__, __LINE__, true);
      }
    }

    if (!source_path_to_copy.empty()) {
      filesystem::copy(source_path_to_copy,
                       format("{}/Backboard/Sources/{}.cpp", main_directory_,
                              strategy_class_name));
    }

    // 전략 헤더 파일 저장 (존재하지 않으면 프로젝트 상위 폴더에서 검색)
    const auto& header_path = strategy->GetHeaderPath();
    string header_path_to_copy;

    if (!header_path.empty() && filesystem::exists(header_path)) {
      header_path_to_copy = header_path;
    } else {
      header_path_to_copy = find_file_in_parent(strategy_class_name + ".hpp");

      if (header_path_to_copy.empty()) {
        logger_->Log(WARN_L,
                     format("전략 헤더 파일을 찾을 수 없습니다: {}",
                            strategy_class_name + ".hpp"),
                     __FILE__, __LINE__, true);
      }
    }

    if (!header_path_to_copy.empty()) {
      filesystem::copy(header_path_to_copy,
                       format("{}/Backboard/Sources/{}.hpp", main_directory_,
                              strategy_class_name));
    }

    // 전략에서 사용하는 지표 소스 코드 저장
    for (const auto& indicator : strategy->GetIndicators()) {
      const auto& indicator_name = indicator->GetName();
      const auto& indicator_class_name = indicator->GetClassName();

      // OHLCV는 저장하지 않음
      if (indicator_name == strategy->open.GetName() ||
          indicator_name == strategy->high.GetName() ||
          indicator_name == strategy->low.GetName() ||
          indicator_name == strategy->close.GetName() ||
          indicator_name == strategy->volume.GetName()) {
        continue;
      }

      // 이미 저장한 지표 클래스는 건너뜀
      if (saved_indicator_classes.contains(indicator_class_name)) {
        continue;
      }

      saved_indicator_classes.insert(indicator_class_name);

      // 소스 파일 저장 (존재하지 않으면 프로젝트 상위 폴더에서 검색)
      const auto& indicator_source_path = indicator->GetSourcePath();
      string indicator_source_path_to_copy;

      if (!indicator_source_path.empty() &&
          filesystem::exists(indicator_source_path)) {
        indicator_source_path_to_copy = indicator_source_path;
      } else {
        indicator_source_path_to_copy =
            find_file_in_parent(indicator_class_name + ".cpp");

        if (indicator_source_path_to_copy.empty()) {
          logger_->Log(WARN_L,
                       format("지표 소스 파일을 찾을 수 없습니다: {}",
                              indicator_class_name + ".cpp"),
                       __FILE__, __LINE__, true);
        }
      }

      if (!indicator_source_path_to_copy.empty()) {
        filesystem::copy(indicator_source_path_to_copy,
                         format("{}/Backboard/Sources/{}.cpp", main_directory_,
                                indicator_class_name));
      }

      // 헤더 파일 저장 (존재하지 않으면 프로젝트 상위 폴더에서 검색)
      const auto& indicator_header_path = indicator->GetHeaderPath();
      string indicator_header_path_to_copy;

      if (!indicator_header_path.empty() &&
          filesystem::exists(indicator_header_path)) {
        indicator_header_path_to_copy = indicator_header_path;
      } else {
        indicator_header_path_to_copy =
            find_file_in_parent(indicator_class_name + ".hpp");

        if (indicator_header_path_to_copy.empty()) {
          logger_->Log(WARN_L,
                       format("지표 헤더 파일을 찾을 수 없습니다: {}",
                              indicator_class_name + ".hpp"),
                       __FILE__, __LINE__, true);
        }
      }

      if (!indicator_header_path_to_copy.empty()) {
        filesystem::copy(indicator_header_path_to_copy,
                         format("{}/Backboard/Sources/{}.hpp", main_directory_,
                                indicator_class_name));
      }
    }

    logger_->Log(INFO_L,
                 "전략과 지표의 헤더 파일 및 소스 파일이 저장되었습니다.",
                 __FILE__, __LINE__, true);
  } catch (const exception& e) {
    logger_->Log(ERROR_L, e.what(), __FILE__, __LINE__, true);
    Logger::LogAndThrowError(
        "전략과 지표의 헤더 파일 및 소스 파일을 저장하는 중 오류가 "
        "발생했습니다.",
        __FILE__, __LINE__);
  }
}

void Analyzer::SaveBackboard() const {
  try {
    // Backboard Package 경로 설정
    const string backboard_package_path =
        Config::GetProjectDirectory() + "/Sources/js/Backboard Package";

    // Backboard Package가 존재하는지 확인
    if (filesystem::exists(backboard_package_path) &&
        filesystem::is_directory(backboard_package_path)) {
      // 기존 패키지가 있으면 복사
      filesystem::copy(backboard_package_path, main_directory_,
                       filesystem::copy_options::recursive |
                           filesystem::copy_options::overwrite_existing);
    } else {
      // 패키지가 없으면 GitHub 릴리즈에서 다운로드
      logger_->Log(
          WARN_L,
          "로컬 저장소에서 백보드를 찾을 수 없어 GitHub에서 다운로드합니다.",
          __FILE__, __LINE__, true);

      DownloadBackboardFromGitHub();
    }

    logger_->Log(INFO_L, "백보드가 저장되었습니다.", __FILE__, __LINE__, true);
  } catch (const exception& e) {
    logger_->Log(ERROR_L, e.what(), __FILE__, __LINE__, true);
    Logger::LogAndThrowError("백보드를 저장하는 중 오류가 발생했습니다.",
                             __FILE__, __LINE__);
  }
}

void Analyzer::SaveBacktestingLog() const {
  try {
    logger_->FlushAllBuffers();
    logger_->backtesting_log_.close();

    filesystem::rename(logger_->backtesting_log_temp_path_,
                       main_directory_ + "/Backboard/backtesting.log");
  } catch (const exception& e) {
    Logger::LogAndThrowError(
        "백테스팅 로그 파일을 저장하는 데 오류가 발생했습니다.: " +
            string(e.what()),
        __FILE__, __LINE__);
  }
}

pair<int, vector<string>> Analyzer::FindMissingBars(
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

    // 예상 시각이 현재 시각보다 크거나 같아졌으면 누락 구간의 종료
    if (in_range && expected >= current) {
      if (range_start == range_end) {
        // 한 구간만 빠졌을 경우는 단일 시간 문자열로 저장
        missing_ranges.push_back(UtcTimestampToUtcDatetime(range_start));
      } else {
        // 여러 구간이 연속해서 빠졌으면 시작 - 종료 형태로 저장
        missing_ranges.push_back(UtcTimestampToUtcDatetime(range_start) +
                                 " - " + UtcTimestampToUtcDatetime(range_end));
      }

      // 다음 누락 구간 추적을 위해 초기화
      in_range = false;
    }
  }

  return {missing_count, missing_ranges};
}

void Analyzer::ParsePlotInfo(ordered_json& indicator_json,
                             const shared_ptr<Indicator>& indicator) {
  const auto& plot_type = indicator->plot_type_;
  const auto& plot = indicator->plot_;
  auto& plot_info = indicator_json["플롯"];

  // 플롯 타입별 고유 정보 기록
  if (plot_type == "Area") {
    const shared_ptr<Area>& area = dynamic_pointer_cast<Area>(plot);

    plot_info["플롯 종류"] = "영역";
    plot_info["위쪽 그라이데이션 색상"] = area->top_gradient_color_.RgbaToHex();
    plot_info["아래쪽 그라이데이션 색상"] =
        area->bottom_gradient_color_.RgbaToHex();
    plot_info["선 색상"] = area->line_color_.RgbaToHex();

  } else if (plot_type == "Baseline") {
    const shared_ptr<Baseline>& baseline = dynamic_pointer_cast<Baseline>(plot);

    plot_info["플롯 종류"] = "기준선";
    plot_info["위/아래 영역을 나눌 기준값"] = baseline->base_value_;
    plot_info["기준값보다 높은 값에 대한 선 색상"] =
        baseline->top_line_color_.RgbaToHex();
    plot_info["기준값보다 높은 값 영역의 위쪽 그라데이션 색상"] =
        baseline->top_gradient_color1_.RgbaToHex();
    plot_info["기준값보다 높은 값 영역의 아래쪽 그라데이션 색상"] =
        baseline->top_gradient_color2_.RgbaToHex();
    plot_info["기준값보다 낮은 값에 대한 선 색상"] =
        baseline->bottom_line_color_.RgbaToHex();
    plot_info["기준값보다 낮은 값 영역의 위쪽 그라데이션 색상"] =
        baseline->bottom_gradient_color1_.RgbaToHex();
    plot_info["기준값보다 낮은 값 영역의 아래쪽 그라데이션 색상"] =
        baseline->bottom_gradient_color2_.RgbaToHex();

  } else if (plot_type == "Histogram") {
    const shared_ptr<Histogram>& histogram =
        dynamic_pointer_cast<Histogram>(plot);

    plot_info["플롯 종류"] = "히스토그램";
    plot_info["기준값"] = histogram->base_value_;
    plot_info["양봉일 때 히스토그램 색상"] =
        histogram->bullish_color_.RgbaToHex();
    plot_info["음봉일 때 히스토그램 색상"] =
        histogram->bearish_color_.RgbaToHex();

  } else if (plot_type == "Line") {
    const shared_ptr<Line>& line = dynamic_pointer_cast<Line>(plot);

    plot_info["플롯 종류"] = "선";
    plot_info["선 색상"] = line->line_color_.RgbaToHex();

  } else if (plot_type == "Null") {
    plot_info["플롯 종류"] = "비활성화";

    // Null은 공통 정보 불필요
    return;
  }

  // Area와 Line의 공통 정보 기록
  if (plot_type == "Area" || plot_type == "Line") {
    plot_info["선 굵기"] = plot->line_width_;

    auto& line_style = plot_info["선 모양"];
    switch (plot->line_style_) {
      case SOLID: {
        line_style = "실선";
        break;
      }
      case DOTTED: {
        line_style = "점선";
        break;
      }
      case DASHED: {
        line_style = "파선";
        break;
      }
      case WIDE_DOTTED: {
        line_style = "넓은 점선";
        break;
      }
      case WIDE_DASHED: {
        line_style = "넓은 파선";
        break;
      }
    }

    auto& line_type = plot_info["선 종류"];
    switch (plot->line_type_) {
      case SIMPLE: {
        line_type = "직선";
        break;
      }
      case STEPPED: {
        line_type = "계단선";
        break;
      }
      case CURVED: {
        line_type = "곡선";
        break;
      }
    }

    if (plot->plot_point_markers_) {
      plot_info["선 위 값에 마커 표시"] = "활성화";
      plot_info["마커 반지름"] = plot->point_markers_radius_;
    } else {
      plot_info["선 위 값에 마커 표시"] = "비활성화";
    }
  }

  // Null을 제외한 모든 플롯 공통 정보 기록
  if (plot->overlay_) {
    plot_info["메인 차트에 지표 겹치기"] = "활성화";
  } else {
    plot_info["메인 차트에 지표 겹치기"] = "비활성화";
    plot_info["페인 이름"] = plot->pane_name_;
  }

  switch (plot->format_) {
    case NONE: {
      plot_info["포맷"] = "없음";
      break;
    }
    case PERCENT: {
      plot_info["포맷"] = "퍼센트";
      break;
    }
    case DOLLAR: {
      plot_info["포맷"] = "달러";
      break;
    }
    case VOLUME: {
      plot_info["포맷"] = "거래량";
      break;
    }
  }

  if (plot->precision_) {
    plot_info["소수점 정밀도"] = *plot->precision_;
  } else {
    // 값이 없는 경우 심볼 가격 소수점 정밀도 사용
    // (VOLUME은 수량 최소 단위의 정밀도 사용)
    // → 프론트에서 처리됨
    plot_info["소수점 정밀도"] = "기본값";
  }
}

void Analyzer::DownloadBackboardFromGitHub() const {
  try {
    const string temp_dir = filesystem::temp_directory_path().string();
    const string backboard_exe_url =
        "https://github.com/Traderhs/Backtesting/releases/latest/download/"
        "Backboard.exe";
    const string backboard_zip_url =
        "https://github.com/Traderhs/Backtesting/releases/latest/download/"
        "Backboard.zip";

    const string exe_temp_path = temp_dir + "/Backboard.exe";
    const string zip_temp_path = temp_dir + "/Backboard.zip";
    const string extract_temp_path = temp_dir + "/BackboardExtract";

    // Backboard.exe 다운로드
    logger_->Log(INFO_L, "Backboard.exe를 다운로드하는 중입니다.", __FILE__,
                 __LINE__, true);

    const string download_exe_cmd = format(
        "curl -L --retry 3 --retry-delay 2 --connect-timeout 30 --max-time "
        "300 --progress-bar "
        "-o \"{}\" \"{}\" 2>&1",
        exe_temp_path, backboard_exe_url);

    if (int exe_result = system(download_exe_cmd.c_str()); exe_result != 0) {
      // curl이 실패하면 PowerShell로 재시도
      logger_->Log(WARN_L, "curl로 다운로드 실패, PowerShell로 재시도합니다.",
                   __FILE__, __LINE__, true);

      const string fallback_exe_cmd = format(
          "powershell -Command \"$ProgressPreference = 'Continue'; "
          "[Net.ServicePointManager]::SecurityProtocol = "
          "[Net.SecurityProtocolType]::Tls12; "
          "try {{ Invoke-WebRequest -Uri '{}' -OutFile '{}' -UseBasicParsing "
          "-TimeoutSec 300 }} "
          "catch {{ Write-Output $_.Exception.Message; exit 1 }}\" 2>&1",
          backboard_exe_url, exe_temp_path);

      if (int fallback_result = system(fallback_exe_cmd.c_str());
          fallback_result != 0) {
        Logger::LogAndThrowError(
            "Backboard.exe를 다운로드하는 데 실패했습니다. "
            "네트워크 연결을 확인하고 다시 시도해주세요.",
            __FILE__, __LINE__);
      }
    }

    // Backboard.zip 다운로드
    logger_->Log(INFO_L, "Backboard.zip을 다운로드하는 중입니다.", __FILE__,
                 __LINE__, true);

    const string download_zip_cmd = format(
        "curl -L --retry 3 --retry-delay 2 --connect-timeout 30 --max-time "
        "300 --progress-bar "
        "-o \"{}\" \"{}\" 2>&1",
        zip_temp_path, backboard_zip_url);

    if (int zip_result = system(download_zip_cmd.c_str()); zip_result != 0) {
      // curl이 실패하면 PowerShell로 재시도
      logger_->Log(WARN_L, "curl로 다운로드 실패, PowerShell로 재시도합니다.",
                   __FILE__, __LINE__, true);

      const string fallback_zip_cmd = format(
          "powershell -Command \"$ProgressPreference = 'Continue'; "
          "[Net.ServicePointManager]::SecurityProtocol = "
          "[Net.SecurityProtocolType]::Tls12; "
          "try {{ Invoke-WebRequest -Uri '{}' -OutFile '{}' -UseBasicParsing "
          "-TimeoutSec 300 }} "
          "catch {{ Write-Output $_.Exception.Message; exit 1 }}\" 2>&1",
          backboard_zip_url, zip_temp_path);

      if (int zip_fallback_result = system(fallback_zip_cmd.c_str());
          zip_fallback_result != 0) {
        Logger::LogAndThrowError(
            "Backboard.zip을 다운로드하는 데 실패했습니다. "
            "네트워크 연결을 확인하고 다시 시도해주세요.",
            __FILE__, __LINE__);
      }
    }

    // 다운로드된 파일 크기 확인
    if (!filesystem::exists(exe_temp_path) ||
        filesystem::file_size(exe_temp_path) == 0) {
      Logger::LogAndThrowError(
          "Backboard.exe 파일이 올바르게 다운로드되지 않았습니다.", __FILE__,
          __LINE__);
    }

    if (!filesystem::exists(zip_temp_path) ||
        filesystem::file_size(zip_temp_path) == 0) {
      Logger::LogAndThrowError(
          "Backboard.zip 파일이 올바르게 다운로드되지 않았습니다.", __FILE__,
          __LINE__);
    }

    // ZIP 파일 압축 해제
    filesystem::create_directories(extract_temp_path);

    const string extract_cmd = format(
        "powershell -Command \"try {{ Expand-Archive -Path '{}' "
        "-DestinationPath '{}' "
        "-Force }} catch {{ Write-Output $_.Exception.Message; exit 1 }}\" "
        "2>&1",
        zip_temp_path, extract_temp_path);

    if (system(extract_cmd.c_str()) != 0) {
      Logger::LogAndThrowError("Backboard.zip 압축 해제에 실패했습니다.",
                               __FILE__, __LINE__);
    }

    // Backboard.exe 복사
    filesystem::copy_file(exe_temp_path, main_directory_ + "/Backboard.exe",
                          filesystem::copy_options::overwrite_existing);

    // 압축 해제된 폴더에서 Backboard 폴더 찾기
    const string backboard_dest_path = main_directory_ + "/Backboard";
    filesystem::create_directories(backboard_dest_path);

    if (const string backboard_folder_path = extract_temp_path + "/Backboard";
        filesystem::exists(backboard_folder_path) &&
        filesystem::is_directory(backboard_folder_path)) {
      // Backboard 폴더 안쪽 내용을 main_directory_/Backboard로 복사
      for (const auto& entry :
           filesystem::recursive_directory_iterator(backboard_folder_path)) {
        if (entry.is_regular_file()) {
          const auto relative_path =
              filesystem::relative(entry.path(), backboard_folder_path);
          const auto dest_path = backboard_dest_path / relative_path;

          // 목적지 디렉토리 생성
          filesystem::create_directories(dest_path.parent_path());

          // 파일 복사
          filesystem::copy_file(entry.path(), dest_path,
                                filesystem::copy_options::overwrite_existing);
        }
      }
    } else {
      // Backboard 폴더가 없으면 전체 내용을 main_directory_/Backboard로 복사
      for (const auto& entry :
           filesystem::recursive_directory_iterator(extract_temp_path)) {
        if (entry.is_regular_file()) {
          const auto relative_path =
              filesystem::relative(entry.path(), extract_temp_path);
          const auto dest_path = backboard_dest_path / relative_path;

          // 목적지 디렉토리 생성
          filesystem::create_directories(dest_path.parent_path());

          // 파일 복사
          filesystem::copy_file(entry.path(), dest_path,
                                filesystem::copy_options::overwrite_existing);
        }
      }
    }

    // 임시 파일 정리
    try {
      filesystem::remove(exe_temp_path);
      filesystem::remove(zip_temp_path);
      filesystem::remove_all(extract_temp_path);
    } catch (const exception& e) {
      logger_->Log(WARN_L, format("임시 파일 정리 중 오류 발생: {}", e.what()),
                   __FILE__, __LINE__, true);
    }

    logger_->Log(INFO_L, "GitHub에서 백보드가 성공적으로 다운로드되었습니다.",
                 __FILE__, __LINE__, true);

  } catch (const exception& e) {
    logger_->Log(ERROR_L, e.what(), __FILE__, __LINE__, true);
    Logger::LogAndThrowError(
        "GitHub에서 백보드를 다운로드하는 중 오류가 발생했습니다.", __FILE__,
        __LINE__);
  }
}

}  // namespace backtesting::analyzer