// 표준 라이브러리
#include <cmath>
#include <cstdint>
#include <format>

// 파일 헤더
#include "Engines/OrderHandler.hpp"

// 내부 헤더
#include "Engines/Analyzer.hpp"
#include "Engines/BarData.hpp"
#include "Engines/BarHandler.hpp"
#include "Engines/DataUtils.hpp"
#include "Engines/Engine.hpp"
#include "Engines/Exception.hpp"
#include "Engines/Order.hpp"
#include "Engines/SymbolInfo.hpp"
#include "Engines/TimeUtils.hpp"
#include "Engines/Trade.hpp"

// 네임 스페이스
namespace backtesting {
using namespace exception;
using namespace utils;
}  // namespace backtesting

namespace backtesting::order {

OrderHandler::OrderHandler() = default;
void OrderHandler::Deleter::operator()(const OrderHandler* p) const {
  delete p;
}

mutex OrderHandler::mutex_;
shared_ptr<OrderHandler> OrderHandler::instance_;

shared_ptr<OrderHandler>& OrderHandler::GetOrderHandler() {
  lock_guard lock(mutex_);  // 스레드에서 안전하게 접근하기 위해 mutex 사용

  // 인스턴스가 존재하는지 확인
  if (!instance_) {
    // 인스턴스가 없으면 생성 후 저장
    instance_ = shared_ptr<OrderHandler>(new OrderHandler(), Deleter());
  }

  return instance_;
}

void OrderHandler::CheckLiquidation(const double price,
                                    const PriceType price_type,
                                    const int symbol_idx,
                                    const BarType market_bar_type) {
  // 현재 심볼의 체결된 진입 주문들을 로딩
  const auto& filled_entries = filled_entries_[symbol_idx];

  // 강제 청산된 주문은 진입 체결 주문에서 삭제되므로 역순으로 조회
  for (int order_idx = static_cast<int>(filled_entries.size() - 1);
       order_idx >= 0; order_idx--) {
    const auto filled_entry = filled_entries[order_idx];
    const auto liquidation_price = filled_entry->GetLiquidationPrice();

    // 매수 진입 -> 현재 가격이 강제 청산 가격과 같거나 밑일 때
    // 매도 진입 -> 현재 가격이 강제 청산 가격과 같거나 위일 때
    if (const auto entry_direction = filled_entry->GetEntryDirection();
        (entry_direction == LONG && IsLessOrEqual(price, liquidation_price)) ||
        (entry_direction == SHORT &&
         IsGreaterOrEqual(price, liquidation_price))) {
      // 실제 시장 가격 찾기
      const auto market_bar_data = bar_->GetBarData(market_bar_type, "");
      double order_price = 0;
      switch (price_type) {
        case OPEN: {
          order_price =
              market_bar_data->GetBar(symbol_idx, bar_->GetCurrentBarIndex())
                  .open;
          break;
        }

        case HIGH: {
          // 고가에서 청산 가격까지의 차이를 구하여 실제 시장 가격에서 조정
          order_price =
              market_bar_data->GetBar(symbol_idx, bar_->GetCurrentBarIndex())
                  .high -
              (price - liquidation_price);
          break;
        }

        case LOW: {
          // 청산 가격에서 저가까지의 차이를 구하여 실제 시장 가격에서 조정
          order_price =
              market_bar_data->GetBar(symbol_idx, bar_->GetCurrentBarIndex())
                  .low +
              (liquidation_price - price);
          break;
        }

        case CLOSE: {
          order_price =
              market_bar_data->GetBar(symbol_idx, bar_->GetCurrentBarIndex())
                  .close;
          break;
        }
      }

      ExecuteLiquidation("강제 청산 (청산가)", symbol_idx, order_idx,
                         order_price);
    }
  }
}

void OrderHandler::CheckPendingEntries(const double price,
                                       const PriceType price_type,
                                       const int symbol_idx) {
  const auto& pending_entries = pending_entries_[symbol_idx];
  int order_idx = 0;

  while (order_idx < pending_entries.size()) {
    // 현재 인덱스의 주문 포인터를 저장
    const auto current_order = pending_entries[order_idx];

    switch (current_order->GetEntryOrderType()) {
      case MARKET: {
        // 시장가 대기 주문은 ON_CLOSE 주문에서만 생기므로, 이 함수에 들어온
        // 타이밍은 무조건 시가이기 때문에 즉시 체결하면 됨
        try {
          FillPendingMarketEntry(symbol_idx, order_idx, price, price_type);
        } catch ([[maybe_unused]] const OrderFailed& e) {
          LogFormattedInfo(
              WARNING_L,
              format("시장가 [{}] 체결 실패", current_order->GetEntryName()),
              __FILE__, __LINE__);
        }
        break;
      }

      case LIMIT: {
        CheckPendingLimitEntries(symbol_idx, order_idx, price, price_type);
        break;
      }

      case MIT: {
        CheckPendingMitEntries(symbol_idx, order_idx, price, price_type);
        break;
      }

      case LIT: {
        CheckPendingLitEntries(symbol_idx, order_idx, price, price_type);
        break;
      }

      case TRAILING: {
        CheckPendingTrailingEntries(symbol_idx, order_idx, price, price_type);
        break;
      }

      case ORDER_NONE: {
        LogFormattedInfo(ERROR_L, "진입 대기 주문에 NONE 주문 존재", __FILE__,
                         __LINE__);
        throw;
      }
    }

    // 처리 후, current_order가 여전히 같은 위치에 있다면 인덱스를 증가
    if (order_idx < pending_entries.size() &&
        pending_entries[order_idx] == current_order) {
      ++order_idx;
    }
    // 만약 주문이 체결되어 삭제되었다면 다음 주문이 현재 order_idx 위치로
    // 이동했으므로 그대로 반복
  }
}

void OrderHandler::CheckPendingExits(const double price,
                                     const PriceType price_type,
                                     const int symbol_idx) {
  const auto& pending_exits = pending_exits_[symbol_idx];
  long long order_idx = 0;

  while (order_idx < pending_exits.size()) {
    // 현재 인덱스의 주문 포인터를 저장
    const auto current_order = pending_exits[order_idx];

    // 주문 확인 함수가 반환하는 값은
    // 현재 주문 처리 도중 현재 인덱스보다 낮은 위치의 주문이 삭제된 개수
    int deleted_below_count = -1;

    switch (pending_exits[order_idx]->GetExitOrderType()) {
      case MARKET: {
        // 시장가 대기 주문은 ON_CLOSE 주문에서만 생기므로, 이 함수에 들어온
        // 타이밍은 무조건 시가이기 때문에 즉시 체결하면 됨
        try {
          deleted_below_count =
              FillPendingExitOrder(symbol_idx, order_idx, price);
        } catch ([[maybe_unused]] const OrderFailed& e) {
          // 청산 주문은 파산이 아닌 이상 체결되므로 엔진 오류
          LogFormattedInfo(ERROR_L,
                           format("엔진 오류: 시장가 [{}] 체결 실패",
                                  current_order->GetExitName()),
                           __FILE__, __LINE__);
          throw;
        }
        break;
      }

      case LIMIT: {
        deleted_below_count =
            CheckPendingLimitExits(symbol_idx, order_idx, price, price_type);
        break;
      }

      case MIT: {
        deleted_below_count =
            CheckPendingMitExits(symbol_idx, order_idx, price, price_type);
        break;
      }

      case LIT: {
        deleted_below_count =
            CheckPendingLitExits(symbol_idx, order_idx, price, price_type);
        break;
      }

      case TRAILING: {
        deleted_below_count =
            CheckPendingTrailingExits(symbol_idx, order_idx, price, price_type);
        break;
      }

      case ORDER_NONE: {
        LogFormattedInfo(WARNING_L, "청산 대기 주문에 NONE 주문 존재", __FILE__,
                         __LINE__);
        throw;
      }
    }

    // deleted_below_count가 -1이면, 주문 처리 중 체결이 없었으므로
    // 다음 주문으로 넘어가기 위해 인덱스를 증가
    if (deleted_below_count == -1) {
      order_idx++;
      continue;
    }

    // deleted_below_count가 0이면, 현재 주문은 무조건 삭제되고 order_idx보다 큰
    // 인덱스의 주문도 삭제될 수 있지만, order_idx보다 작은 인덱스의 주문은
    // 삭제되지 않음.
    // 따라서 현재 인덱스에 다음 주문이 채워지므로 현재 인덱스로 재검사
    // (만약 다음 주문이 없다면 자연스럽게 그대로 루프 종료)
    if (deleted_below_count == 0) {
      continue;
    }

    // 만약 deleted_below_count가 0보다 크면,
    // 현재 order_idx 앞쪽의 주문들이 삭제되어 컨테이너가 앞으로 당겨짐.
    // 따라서 원래 인덱스에서 삭제된 개수만큼 인덱스를 빼주어 당겨진 위치의
    // 주문부터 다시 검사
    order_idx -= deleted_below_count;
  }
}

void OrderHandler::ExecuteFunding(const double funding_rate,
                                  const string& funding_time,
                                  const double funding_price,
                                  const int symbol_idx) {
  const auto& filled_entries = filled_entries_[symbol_idx];

  // 주문마다 펀딩비 정산
  // ※ 체결된 주문이 없으면 펀딩비는 없음
  // ※ 펀딩비 부족 시 강제 청산될 수 있고, 강제 청산된 주문은 진입 체결 주문에서
  //    삭제되므로 역순으로 조회
  for (int order_idx = static_cast<int>(filled_entries.size() - 1);
       order_idx >= 0; order_idx--) {
    const auto& filled_entry = filled_entries[order_idx];
    const auto& entry_name = filled_entry->GetEntryName();

    // 펀딩비: 펀딩 비율 * 펀딩 가격(마크 가격) * 진입 포지션 잔량
    const auto entry_direction = filled_entry->GetEntryDirection();
    const auto current_position_size =
        filled_entry->GetEntryFilledSize() - filled_entry->GetExitFilledSize();
    double funding_amount =
        abs(funding_rate * funding_price * current_position_size);
    const double abs_funding_amount = funding_amount;

    // 포지션 방향에 따라 지불인지 수령인지 결정
    // 펀딩 비율 양수: 롱은 지불, 숏은 수령
    // 펀딩 비율 음수: 롱은 수령, 숏은 지불
    if ((funding_rate > 0 && entry_direction == LONG) ||
        (funding_rate < 0 && entry_direction == SHORT)) {
      funding_amount = -funding_amount;
    }

    if (funding_amount > 0) {
      // 펀딩비 수령은 단순히 지갑 자금에 합산
      engine_->IncreaseWalletBalance(funding_amount);

      const auto received_funding_amount =
          filled_entry->GetReceivedFundingAmount() + abs_funding_amount;

      // 진입 주문에 펀딩비 정산
      filled_entry->AddReceivedFundingCount().SetReceivedFundingAmount(
          received_funding_amount);

      // 해당 진입 주문을 목표로 하는 청산 대기 주문에 펀딩비 정산
      for (const auto& pending_exit : pending_exits_[symbol_idx]) {
        if (pending_exit->GetEntryName() == entry_name) {
          pending_exit->AddReceivedFundingCount().SetReceivedFundingAmount(
              received_funding_amount);
        }
      }

      LogFormattedInfo(
          INFO_L,
          format("[{}] 펀딩비 [{}] 수령 (펀딩 시간 {} | 펀딩 비율 "
                 "{} | 펀딩 가격 {} | 포지션 수량 {})",
                 entry_name, FormatDollar(abs_funding_amount, true),
                 funding_time, FormatPercentage(funding_rate * 100, false),
                 funding_price, current_position_size),
          __FILE__, __LINE__);
    } else {
      // 펀딩비가 사용 가능 자금보다 적을 경우 지갑 자금에서 지불
      if (const auto available_balance = engine_->GetAvailableBalance();
          IsLessOrEqual(abs_funding_amount, available_balance)) {
        engine_->DecreaseWalletBalance(abs_funding_amount);

        const auto paid_funding_amount =
            filled_entry->GetPaidFundingAmount() - abs_funding_amount;

        // 진입 주문에 펀딩비 정산
        filled_entry->AddPaidFundingCount().SetPaidFundingAmount(
            paid_funding_amount);

        // 해당 진입 주문을 목표로 하는 청산 대기 주문에 펀딩비 정산
        for (const auto& pending_exit : pending_exits_[symbol_idx]) {
          if (pending_exit->GetEntryName() == entry_name) {
            pending_exit->AddPaidFundingCount().SetPaidFundingAmount(
                paid_funding_amount);
          }
        }

        LogFormattedInfo(
            INFO_L,
            format("[{}] 펀딩비 [{}] 지불 (펀딩 시간 {} | 펀딩 비율 "
                   "{} | 펀딩 가격 {} | 포지션 수량 {})",
                   entry_name, FormatDollar(abs_funding_amount, true),
                   funding_time, FormatPercentage(funding_rate * 100, false),
                   funding_price, current_position_size),
            __FILE__, __LINE__);
      } else {
        // 펀딩비가 사용 가능 자금보다 많을 경우 우선 사용 가능 자금에서 최대한
        // 지불 후 주문 마진에서 감소

        // 1. 우선 사용 가능 자금에서 최대한 감소
        if (available_balance > 0) {
          engine_->DecreaseWalletBalance(available_balance);

          LogFormattedInfo(
              WARNING_L,
              format("[{}] 일부 펀딩비 [{}] 지불 (펀딩 시간 {} | 펀딩 비율 "
                     "{} | 펀딩 가격 {} | 포지션 수량 {} | 전체 펀딩비 {})",
                     entry_name, FormatDollar(available_balance, true),
                     funding_time, FormatPercentage(funding_rate * 100, false),
                     funding_price, current_position_size,
                     FormatDollar(abs_funding_amount, true)),
              __FILE__, __LINE__);

          engine_->LogBalance();
        }

        // 2. 남은 펀딩비는 진입 주문의 잔여 마진에서 감소
        const auto margin_deduction =
            abs_funding_amount - available_balance;  // 마진에서 충당할 금액

        // 2.1 남은 펀딩비가 잔여 마진보다 적을 경우, 잔여 마진에서 남은
        //     펀딩비를 전부 부과
        if (const auto left_margin = filled_entry->GetLeftMargin();
            margin_deduction < left_margin) {
          // 포지션의 잔여 마진과 강제 청산 가격 조정
          const auto adjusted_margin = left_margin - margin_deduction;
          const auto liquidation_price = filled_entry->GetLiquidationPrice();
          const auto adjusted_liquidation_price = CalculateLiquidationPrice(
              entry_direction, filled_entry->GetEntryFilledPrice(),
              current_position_size, adjusted_margin);
          const auto paid_funding_amount =
              filled_entry->GetPaidFundingAmount() - abs_funding_amount;

          // 진입 주문의 잔여 마진, 강제 청산 가격, 펀딩비 지불 재설정
          filled_entry->SetLeftMargin(adjusted_margin)
              .SetLiquidationPrice(adjusted_liquidation_price)
              .AddPaidFundingCount()
              .SetPaidFundingAmount(paid_funding_amount);

          // 해당 진입 주문을 목표로 하는 청산 대기 주문의 잔여 마진,
          // 강제 청산 가격, 펀딩비 지불 재설정
          for (const auto& pending_exit : pending_exits_[symbol_idx]) {
            if (pending_exit->GetEntryName() == entry_name) {
              pending_exit->SetLeftMargin(adjusted_margin)
                  .SetLiquidationPrice(adjusted_liquidation_price)
                  .AddPaidFundingCount()
                  .SetPaidFundingAmount(paid_funding_amount);
            }
          }

          // Used Margin은 포지션의 Left Margin보다 무조건 같거나 많으므로,
          // 음수는 절대 되지 않으므로 단순 감소시키면 됨
          engine_->DecreaseUsedMargin(margin_deduction);
          engine_->DecreaseWalletBalance(margin_deduction);

          LogFormattedInfo(
              WARNING_L,
              format("[{}] 남은 펀딩비 [{}]를 마진 [{}]에서 지불 "
                     "(펀딩 시간 {} | 펀딩 비율 {} | 펀딩 가격 {} | "
                     "포지션 수량 {} | 전체 펀딩비 {})",
                     entry_name, FormatDollar(margin_deduction, true),
                     FormatDollar(left_margin, true), funding_time,
                     FormatPercentage(funding_rate * 100, false), funding_price,
                     current_position_size,
                     FormatDollar(abs_funding_amount, true)),
              __FILE__, __LINE__);

          LogFormattedInfo(
              WARNING_L,
              format(
                  "[{}] 펀딩비를 지불할 사용 가능 자금 부족으로 인해 잔여 마진 "
                  "[{}] → [{}], 강제 청산 가격 [{}] → [{}] 조정",
                  entry_name, FormatDollar(left_margin, true),
                  FormatDollar(adjusted_margin, true), liquidation_price,
                  adjusted_liquidation_price),
              __FILE__, __LINE__);
        } else {
          // 2.2 남은 펀딩비가 잔여 마진보다 많거나 같을 경우, 잔여 마진까지만
          //     펀딩비가 부과되고 남은 펀딩비는 보험 기금에서 충당되며 포지션은
          //     강제 청산 됨

          // 진입 주문의 잔여 마진은 0
          // FundingAmount에서 잔여 마진은 항상 양수이고 지불하는 것이니 빼기
          filled_entry->SetLeftMargin(0)
              .SetLiquidationPrice(CalculateLiquidationPrice(
                  entry_direction, filled_entry->GetEntryFilledPrice(),
                  current_position_size, 0))
              .AddPaidFundingCount()
              .SetPaidFundingAmount(filled_entry->GetPaidFundingAmount() -
                                    left_margin);

          // 어차피 강제 청산 되므로 해당 진입을 목표로 하는 청산 주문은 수정할
          // 필요가 없음 (자동으로 취소됨)

          engine_->DecreaseUsedMargin(left_margin);
          engine_->DecreaseWalletBalance(left_margin);

          LogFormattedInfo(
              WARNING_L,
              format("[{}] 남은 펀딩비 [{}]가 잔여 마진 [{}]"
                     "보다 많거나 같으므로, 잔여 마진 전체를 펀딩비로 지불하고 "
                     "강제 청산됩니다. (부족분은 보험 기금에서 충당됩니다.)",
                     entry_name, FormatDollar(margin_deduction, true),
                     FormatDollar(left_margin, true)),
              __FILE__, __LINE__);

          LogFormattedInfo(
              WARNING_L,
              format("[{}] 남은 펀딩비 일부를 마진 [{}]로 전체 지불 "
                     "(펀딩 시간 {} | 펀딩 비율 {} | 펀딩 가격 {} | "
                     "포지션 수량 {} | 전체 펀딩비 {})",
                     entry_name, FormatDollar(left_margin, true), funding_time,
                     FormatPercentage(funding_rate * 100, false), funding_price,
                     current_position_size,
                     FormatDollar(abs_funding_amount, true)),
              __FILE__, __LINE__);

          engine_->LogBalance();

          // 현재 Open 가격에서 강제 청산
          // (펀딩비는 항상 OHLC 시작 전에 정산되므로 시가에서 강제 청산이 타당)
          // 현재 바의 Open Time이 현재 진행 시간의 Open Time과 같지 않으면
          // 펀딩비는 부과되지 않으므로 이 함수 실행 시점에서는 단순히 Open
          // 가격만 가져와서 사용하면 됨
          ExecuteLiquidation(
              "강제 청산 (펀딩비)", symbol_idx, order_idx,
              (bar_->GetCurrentBarType() == TRADING
                   ? bar_->GetBarData(TRADING, "")
                   : bar_->GetBarData(MAGNIFIER, ""))
                  ->GetBar(symbol_idx, bar_->GetCurrentBarIndex())
                  .open);

          // 강제 청산 시 LogBalance가 실행되므로 따로 호출할 필요가 없으므로
          // 바로 다음 펀딩 징수로 넘어감
          continue;
        }
      }
    }

    engine_->LogBalance();
  }
}

void OrderHandler::MarketEntry(const string& entry_name,
                               const Direction entry_direction,
                               const double order_size) {
  const auto symbol_idx = bar_->GetCurrentSymbolIndex();

  try {
    // 현재 바 혹은 다음 바 시가에서 진입할지 결정하는 플래그
    bool entry_now;

    // 바 정보 로딩
    int64_t order_time = 0;
    double order_price = 0.0;
    try {
      // On Close 전략일 시 주문 시간과 주문 가격은 다음 봉의 Open Time과 Open
      if (const auto& strategy_type = engine_->GetCurrentStrategyType();
          strategy_type == ON_CLOSE) {
        const auto& next_bar =
            bar_->GetBarData(bar_->GetCurrentBarType(), "NONE")
                ->SafeGetBar(symbol_idx, bar_->GetCurrentBarIndex() + 1);
        order_time = next_bar.open_time;
        order_price = next_bar.open;

        entry_now = false;
      } else {
        // After Entry, After Exit 전략일 시 주문 시간은 현재 바의 Open Time,
        // 주문 가격은 마지막 진입 가격 혹은 마지막 청산 가격
        order_time = engine_->GetCurrentOpenTime();

        if (strategy_type == AFTER_ENTRY) {
          order_price = LastEntryPrice();
        } else if (strategy_type == AFTER_EXIT) {
          order_price = LastExitPrice();
        }

        entry_now = true;
      }
    } catch ([[maybe_unused]] const IndexOutOfRange& e) {
      LogFormattedInfo(WARNING_L, "마지막 바에서 시장가 진입 주문 불가",
                       __FILE__, __LINE__);
      throw OrderFailed("시장가 진입 실패");
    }

    // 유효성 검사
    try {
      IsValidEntryName(entry_name);
      IsValidDirection(entry_direction);
      IsValidPositionSize(order_size, MARKET);
      IsValidNotionalValue(order_price, order_size);
      IsValidLeverage(order_price, order_size);
    } catch (const InvalidValue& e) {
      LogFormattedInfo(WARNING_L, e.what(), __FILE__, __LINE__);
      throw OrderFailed("시장가 진입 실패");
    }

    // 주문 생성
    const auto market_entry = make_shared<Order>();
    market_entry->SetLeverage(GetLeverage(symbol_idx))
        .SetWbWhenEntryOrder(engine_->GetWalletBalance())
        .SetEntryName(entry_name)
        .SetEntryOrderType(MARKET)
        .SetEntryDirection(entry_direction)
        .SetEntryOrderTime(order_time)
        .SetEntryOrderPrice(order_price)
        .SetEntryOrderSize(order_size)
        .SetEntryFilledTime(order_time)
        .SetEntryFilledSize(order_size);

    // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 (주문 수정)
    Cancel(entry_name);

    if (entry_now) {
      // 슬리피지가 포함된 체결가
      const double slippage_filled_price =
          CalculateSlippagePrice(MARKET, entry_direction, order_price);
      market_entry->SetEntryFilledPrice(slippage_filled_price);

      // 수수료
      market_entry->SetEntryFee(
          CalculateTradingFee(MARKET, slippage_filled_price, order_size));

      // 자금 관련 처리 후 체결 주문에 추가
      ExecuteMarketEntry(market_entry, CLOSE);
    } else {
      // 시장가 진입 대기 -> 실제 체결은 다음 봉의 시가
      pending_entries_[symbol_idx].push_back(market_entry);
    }
  } catch ([[maybe_unused]] const OrderFailed& e) {
    LogFormattedInfo(WARNING_L, format("시장가 [{}] 주문 실패", entry_name),
                     __FILE__, __LINE__);
  }
}

void OrderHandler::LimitEntry(const string& entry_name,
                              const Direction entry_direction,
                              double order_price, const double order_size) {
  const int symbol_idx = bar_->GetCurrentSymbolIndex();

  try {
    // 바 정보 로딩
    int64_t order_time = 0;
    double base_price = 0.0;
    try {
      // On Close 전략일 시 주문 시간과 기준 가격은 다음 봉의 Open Time과 Open
      if (const auto& strategy_type = engine_->GetCurrentStrategyType();
          strategy_type == ON_CLOSE) {
        const auto& next_bar =
            bar_->GetBarData(bar_->GetCurrentBarType(), "NONE")
                ->SafeGetBar(symbol_idx, bar_->GetCurrentBarIndex() + 1);
        order_time = next_bar.open_time;
        base_price = next_bar.open;
      } else {
        // After Entry 혹은 After Exit 전략일 시 주문 시간은 현재 바의 Open
        // Time, 기준 가격은 마지막 진입 가격 혹은 마지막 청산 가격

        order_time = engine_->GetCurrentOpenTime();

        if (strategy_type == AFTER_ENTRY) {
          base_price = LastEntryPrice();
        } else if (strategy_type == AFTER_EXIT) {
          base_price = LastExitPrice();
        }
      }
    } catch ([[maybe_unused]] const IndexOutOfRange& e) {
      LogFormattedInfo(WARNING_L, "마지막 바에서 지정가 진입 주문 불가",
                       __FILE__, __LINE__);
      throw OrderFailed("지정가 진입 실패");
    }

    // 주문 가격을 틱 사이즈로 반올림
    order_price =
        RoundToTickSize(order_price, symbol_info_[symbol_idx].GetTickSize());

    // 주문 생성
    const auto limit_entry = make_shared<Order>();
    limit_entry->SetLeverage(GetLeverage(symbol_idx))
        .SetWbWhenEntryOrder(engine_->GetWalletBalance())
        .SetEntryName(entry_name)
        .SetEntryOrderType(LIMIT)
        .SetEntryDirection(entry_direction)
        .SetEntryOrderTime(order_time)
        .SetEntryOrderPrice(order_price)
        .SetEntryOrderSize(order_size);

    // 유효성 검사
    try {
      IsValidDirection(entry_direction);
      IsValidPrice(order_price);
      IsValidLimitOrderPrice(order_price, base_price, entry_direction);
      IsValidPositionSize(order_size, LIMIT);
      IsValidNotionalValue(order_price, order_size);
      IsValidLeverage(order_price, order_size);
    } catch (const InvalidValue& e) {
      LogFormattedInfo(WARNING_L, e.what(), __FILE__, __LINE__);
      throw OrderFailed("지정가 진입 실패");
    }

    // 예약 증거금 계산
    const double entry_margin = CalculateMargin(order_price, order_size, CLOSE);
    limit_entry->SetEntryMargin(entry_margin).SetLeftMargin(entry_margin);

    // 주문 가능 여부 체크
    try {
      HasEnoughBalance(engine_->GetAvailableBalance(), entry_margin,
                       "사용 가능", "지정가 주문 마진");
    } catch (const InsufficientBalance& e) {
      LogFormattedInfo(WARNING_L, e.what(), __FILE__, __LINE__);
      throw OrderFailed("지정가 진입 실패");
    }

    // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 (주문 수정)
    Cancel(entry_name);

    // 사용한 마진에 예약 증거금 증가
    engine_->IncreaseUsedMargin(entry_margin);

    // 지정가 진입 대기
    pending_entries_[symbol_idx].push_back(limit_entry);

    LogFormattedInfo(INFO_L,
                     format("지정가 [{}] 주문 (주문가 {} | 주문량 {})",
                            entry_name, order_price, order_size),
                     __FILE__, __LINE__);
    engine_->LogBalance();
  } catch ([[maybe_unused]] const OrderFailed& e) {
    LogFormattedInfo(WARNING_L, format("지정가 [{}] 주문 실패", entry_name),
                     __FILE__, __LINE__);
  }
}

void OrderHandler::MitEntry(const string& entry_name,
                            const Direction entry_direction, double touch_price,
                            const double order_size) {
  const int symbol_idx = bar_->GetCurrentSymbolIndex();

  try {
    // 바 정보 로딩
    double base_price = 0.0;
    try {
      // On Close 전략일 시 기준 가격은 다음 봉의 Open
      if (const auto& strategy_type = engine_->GetCurrentStrategyType();
          strategy_type == ON_CLOSE) {
        base_price =
            bar_->GetBarData(bar_->GetCurrentBarType(), "NONE")
                ->SafeGetBar(symbol_idx, bar_->GetCurrentBarIndex() + 1)
                .open;
      } else {
        // After Entry 혹은 After Exit 전략일 시
        // 기준 가격은 마지막 진입 가격 혹은 마지막 청산 가격
        if (strategy_type == AFTER_ENTRY) {
          base_price = LastEntryPrice();
        } else if (strategy_type == AFTER_EXIT) {
          base_price = LastExitPrice();
        }
      }
    } catch ([[maybe_unused]] const IndexOutOfRange& e) {
      LogFormattedInfo(WARNING_L, "마지막 바에서 MIT 진입 대기 주문 불가",
                       __FILE__, __LINE__);
      throw OrderFailed("MIT 진입 실패");
    }

    // 터치 가격을 틱 사이즈로 반올림
    touch_price =
        RoundToTickSize(touch_price, symbol_info_[symbol_idx].GetTickSize());

    // 주문 생성
    const auto mit_entry = make_shared<Order>();
    mit_entry->SetLeverage(GetLeverage(symbol_idx))
        .SetWbWhenEntryOrder(engine_->GetWalletBalance())
        .SetEntryName(entry_name)
        .SetEntryOrderType(MIT)
        .SetEntryDirection(entry_direction)
        .SetEntryTouchPrice(touch_price)
        .SetEntryTouchDirection(
            IsGreaterOrEqual(touch_price, base_price) ? LONG : SHORT)
        .SetEntryOrderSize(order_size);

    // 유효성 검사
    try {
      IsValidDirection(entry_direction);
      IsValidPrice(touch_price);
      IsValidPositionSize(order_size, MIT);
      IsValidNotionalValue(touch_price, order_size);
      IsValidLeverage(touch_price, order_size);
    } catch (const InvalidValue& e) {
      LogFormattedInfo(WARNING_L, e.what(), __FILE__, __LINE__);
      throw OrderFailed("MIT 진입 실패");
    }

    // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 (주문 수정)
    Cancel(entry_name);

    // MIT 진입 터치 대기
    pending_entries_[symbol_idx].push_back(mit_entry);

    LogFormattedInfo(INFO_L,
                     format("MIT [{}] 대기 주문 (터치가 {} | 주문량 {})",
                            entry_name, touch_price, order_size),
                     __FILE__, __LINE__);
  } catch ([[maybe_unused]] const OrderFailed& e) {
    LogFormattedInfo(WARNING_L, format("MIT [{}] 대기 주문 실패", entry_name),
                     __FILE__, __LINE__);
  }
}

void OrderHandler::LitEntry(const string& entry_name,
                            const Direction entry_direction, double touch_price,
                            double order_price, const double order_size) {
  const auto symbol_idx = bar_->GetCurrentSymbolIndex();

  try {
    // 바 정보 로딩
    double base_price = 0.0;
    try {
      // On Close 전략일 시 기준 가격은 다음 봉의 Open
      if (const auto& strategy_type = engine_->GetCurrentStrategyType();
          strategy_type == ON_CLOSE) {
        base_price =
            bar_->GetBarData(bar_->GetCurrentBarType(), "NONE")
                ->SafeGetBar(symbol_idx, bar_->GetCurrentBarIndex() + 1)
                .open;
      } else {
        // After Entry 혹은 After Exit 전략일 시
        // 기준 가격은 마지막 진입 가격 혹은 마지막 청산 가격
        if (strategy_type == AFTER_ENTRY) {
          base_price = LastEntryPrice();
        } else if (strategy_type == AFTER_EXIT) {
          base_price = LastExitPrice();
        }
      }
    } catch ([[maybe_unused]] const IndexOutOfRange& e) {
      LogFormattedInfo(WARNING_L, "마지막 바에서 LIT 진입 대기 주문 불가",
                       __FILE__, __LINE__);
      throw OrderFailed("LIT 진입 실패");
    }

    // 터치 가격과 주문 가격을 틱 사이즈로 반올림
    const auto tick_size = symbol_info_[symbol_idx].GetTickSize();
    touch_price = RoundToTickSize(touch_price, tick_size);
    order_price = RoundToTickSize(order_price, tick_size);

    // 주문 생성
    const auto lit_entry = make_shared<Order>();
    lit_entry->SetLeverage(GetLeverage(symbol_idx))
        .SetWbWhenEntryOrder(engine_->GetWalletBalance())
        .SetEntryName(entry_name)
        .SetEntryOrderType(LIT)
        .SetEntryDirection(entry_direction)
        .SetEntryTouchPrice(touch_price)
        .SetEntryTouchDirection(
            IsGreaterOrEqual(touch_price, base_price) ? LONG : SHORT)
        .SetEntryOrderPrice(order_price)
        .SetEntryOrderSize(order_size);

    // 유효성 검사
    try {
      IsValidDirection(entry_direction);
      IsValidPrice(touch_price);
      IsValidPrice(order_price);
      IsValidLimitOrderPrice(order_price, touch_price, entry_direction);
      IsValidPositionSize(order_size, LIT);
      IsValidNotionalValue(order_price, order_size);
      IsValidLeverage(order_price, order_size);
    } catch (const InvalidValue& e) {
      LogFormattedInfo(WARNING_L, e.what(), __FILE__, __LINE__);
      throw OrderFailed("LIT 진입 실패");
    }

    // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 (주문 수정)
    Cancel(entry_name);

    // LIT 진입 터치 대기
    pending_entries_[symbol_idx].push_back(lit_entry);

    LogFormattedInfo(
        INFO_L,
        format("LIT [{}] 대기 주문 (터치가 {} | 주문가 {} | 주문량 {})",
               entry_name, touch_price, order_price, order_size),
        __FILE__, __LINE__);
  } catch ([[maybe_unused]] const OrderFailed& e) {
    LogFormattedInfo(WARNING_L, format("LIT [{}] 대기 주문 실패", entry_name),
                     __FILE__, __LINE__);
  }
}

void OrderHandler::TrailingEntry(const string& entry_name,
                                 const Direction entry_direction,
                                 double touch_price, const double trail_point,
                                 const double order_size) {
  const int symbol_idx = bar_->GetCurrentSymbolIndex();

  try {
    // 바 정보 로딩
    double base_price = 0.0;
    try {
      // On Close 전략일 시 기준 가격은 다음 봉의 Open
      if (const auto& strategy_type = engine_->GetCurrentStrategyType();
          strategy_type == ON_CLOSE) {
        base_price =
            bar_->GetBarData(bar_->GetCurrentBarType(), "NONE")
                ->SafeGetBar(symbol_idx, bar_->GetCurrentBarIndex() + 1)
                .open;
      } else {
        // After Entry 혹은 After Exit 전략일 시
        // 기준 가격은 마지막 진입 가격 혹은 마지막 청산 가격
        if (strategy_type == AFTER_ENTRY) {
          base_price = LastEntryPrice();
        } else if (strategy_type == AFTER_EXIT) {
          base_price = LastExitPrice();
        }
      }
    } catch ([[maybe_unused]] const IndexOutOfRange& e) {
      LogFormattedInfo(WARNING_L, "마지막 바에서 트레일링 진입 대기 주문 불가",
                       __FILE__, __LINE__);
      throw OrderFailed("트레일링 진입 실패");
    }

    // 터치 가격을 틱 사이즈로 반올림
    touch_price =
        RoundToTickSize(touch_price, symbol_info_[symbol_idx].GetTickSize());

    // 주문 생성
    const auto trailing_entry = make_shared<Order>();
    trailing_entry->SetLeverage(GetLeverage(symbol_idx))
        .SetWbWhenEntryOrder(engine_->GetWalletBalance())
        .SetEntryName(entry_name)
        .SetEntryOrderType(TRAILING)
        .SetEntryDirection(entry_direction)
        .SetEntryTouchPrice(touch_price)
        .SetEntryTouchDirection(
            IsGreaterOrEqual(touch_price, base_price) ? LONG : SHORT)
        .SetEntryTrailPoint(trail_point)
        .SetEntryOrderSize(order_size);

    // touch_price가 0이면 다음 시가부터 최고저가 추적을 시작
    if (IsEqual(touch_price, 0.0)) {
      trailing_entry->SetEntryExtremePrice(base_price);
    }

    // 유효성 검사
    try {
      IsValidDirection(entry_direction);
      IsValidTrailingTouchPrice(touch_price);
      IsValidTrailPoint(trail_point);
      IsValidPositionSize(order_size, TRAILING);

      double start_price;
      if (IsEqual(touch_price, 0.0)) {
        start_price = base_price;
      } else {
        start_price = touch_price;
      }

      const double target_price =  // 가장 불리한 진입가로 검사
          entry_direction == LONG ? start_price + trail_point
                                  : start_price - trail_point;

      IsValidNotionalValue(target_price, order_size);
      IsValidLeverage(target_price, order_size);
    } catch (const InvalidValue& e) {
      LogFormattedInfo(WARNING_L, e.what(), __FILE__, __LINE__);
      throw OrderFailed("트레일링 진입 실패");
    }

    // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 (주문 수정)
    Cancel(entry_name);

    // 트레일링 진입 터치 대기
    pending_entries_[symbol_idx].push_back(trailing_entry);

    if (!IsEqual(touch_price, 0.0)) {
      LogFormattedInfo(INFO_L,
                       format("트레일링 [{}] 대기 주문 (터치가 {} | "
                              "트레일 포인트 {} | 주문량 {})",
                              entry_name, touch_price, trail_point, order_size),
                       __FILE__, __LINE__);
    } else {
      // touch_price가 0이라 바로 추적 시작한 경우
      LogFormattedInfo(INFO_L,
                       format("트레일링 {} 주문 (터치가 {} | "
                              "트레일 포인트 {} | 주문량 {})",
                              entry_name, touch_price, trail_point, order_size),
                       __FILE__, __LINE__);
    }
  } catch ([[maybe_unused]] const OrderFailed& e) {
    if (!IsEqual(touch_price, 0.0)) {
      LogFormattedInfo(WARNING_L,
                       format("트레일링 [{}] 대기 주문 실패", entry_name),
                       __FILE__, __LINE__);
    } else {
      LogFormattedInfo(WARNING_L, format("트레일링 [{}] 주문 실패", entry_name),
                       __FILE__, __LINE__);
    }
  }
}

void OrderHandler::MarketExit(const string& exit_name,
                              const string& target_name, double order_size) {
  const auto symbol_idx = bar_->GetCurrentSymbolIndex();

  try {
    // 현재 바 혹은 다음 바 시가에서 청산할지 결정하는 플래그
    bool exit_now;

    // 바 정보 로딩
    int64_t order_time = 0;
    double order_price = 0.0;

    // 바 종가에 시장가 청산하도록 만드는 람다 함수
    const auto market_exit_on_close = [&] {
      const auto& current_bar =
          bar_->GetBarData(bar_->GetCurrentBarType(), "NONE")
              ->GetBar(symbol_idx, bar_->GetCurrentBarIndex());

      order_time = current_bar.close_time;
      order_price = current_bar.close;

      exit_now = true;
    };

    // 청산 시간 및 청산 가격을 결정
    // 모든 심볼의 트레이딩이 끝나지 않았다면 세부 로직에 따라 처리.
    // 끝났다면 종가에 청산.
    if (!engine_->IsAllTradingEnded()) {
      try {
        // 리버스 청산일 시 주문 시간과 주문 가격은 현재 봉의 Open Time과
        // 리버스 목표 진입 주문의 진입 주문 가격
        if (is_reverse_exit_) {
          order_time = engine_->GetCurrentOpenTime();
          order_price = reverse_exit_price_;

          exit_now = true;

        } else if (const auto& strategy_type =
                       engine_->GetCurrentStrategyType();
                   strategy_type == ON_CLOSE) {
          // On Close 전략일 시 주문 시간과 주문 가격은
          // 다음 봉의 Open Time과 Open
          const auto& next_bar =
              bar_->GetBarData(bar_->GetCurrentBarType(), "NONE")
                  ->SafeGetBar(bar_->GetCurrentSymbolIndex(),
                               bar_->GetCurrentBarIndex() + 1);
          order_time = next_bar.open_time;
          order_price = next_bar.open;

          exit_now = false;

        } else {
          // After Entry, After Exit 전략일 시 주문 시간은 현재 바의
          // Open Time, 주문 가격은 마지막 진입 가격 혹은 마지막 청산 가격
          order_time = engine_->GetCurrentOpenTime();

          if (strategy_type == AFTER_ENTRY) {
            order_price = LastEntryPrice();
          } else if (strategy_type == AFTER_EXIT) {
            order_price = LastExitPrice();
          }

          exit_now = true;
        }

      } catch ([[maybe_unused]] const IndexOutOfRange& e) {
        // 마지막 바인 경우 현재 바 종가에 청산
        market_exit_on_close();
      }

    } else {
      market_exit_on_close();
    }

    // 원본 진입 주문 찾기
    shared_ptr<Order> entry_order;
    int entry_order_idx;
    try {
      const auto [order, index] = FindEntryOrder(target_name);
      entry_order = order;
      entry_order_idx = index;
    } catch (const EntryOrderNotFound& e) {
      // 원본 진입 주문을 찾지 못하면 청산 실패
      LogFormattedInfo(WARNING_L, e.what(), __FILE__, __LINE__);
      throw OrderFailed("시장가 청산 실패");
    }

    // 시장가에서 주문 가격은 항상 바 데이터 가격이므로 틱 사이즈로 조정 불필요

    // 총 청산 주문 수량이 진입 체결 수량보다 크지 않게 조정
    order_size = GetAdjustedExitSize(order_size, entry_order);

    // 청산 주문 생성
    const auto market_exit = make_shared<Order>(*entry_order);
    market_exit->SetExitName(exit_name)
        .SetExitOrderType(MARKET)
        .SetExitDirection(market_exit->GetEntryDirection() == LONG ? SHORT
                                                                   : LONG)
        .SetExitOrderTime(order_time)
        .SetExitOrderPrice(order_price)
        .SetExitOrderSize(order_size)
        .SetExitFilledTime(order_time)
        .SetExitFilledSize(order_size);

    // 유효성 검사
    try {
      IsValidExitName(exit_name);
      IsValidPositionSize(order_size, MARKET);
      IsValidNotionalValue(order_price, order_size);
    } catch (const InvalidValue& e) {
      LogFormattedInfo(WARNING_L, e.what(), __FILE__, __LINE__);
      throw OrderFailed("시장가 청산 실패");
    }

    // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 (주문 수정)
    Cancel(exit_name);

    if (exit_now) {
      // 슬리피지가 포함된 체결가
      const double slippage_filled_price = CalculateSlippagePrice(
          MARKET, market_exit->GetExitDirection(), order_price);
      market_exit->SetExitFilledPrice(slippage_filled_price);

      // 수수료
      market_exit->SetExitFee(
          CalculateTradingFee(MARKET, slippage_filled_price, order_size));

      // 원본 진입 주문에 청산 체결 수량 추가
      const double total_exit_filled_size =
          entry_order->GetExitFilledSize() + order_size;
      entry_order->SetExitFilledSize(total_exit_filled_size);

      // 원본 진입 주문의 청산 체결 수량이 진입 체결 수량과 같으면
      // filled_entries에서 삭제
      if (IsEqual(total_exit_filled_size, entry_order->GetEntryFilledSize())) {
        auto& filled_entries = filled_entries_[symbol_idx];
        filled_entries.erase(filled_entries.begin() + entry_order_idx);

        // 같은 진입 이름을 목표로 하는 청산 대기 주문 취소
        // 여러 주문이 삭제될 수 있으므로 역순으로 순회
        for (int order_idx =
                 static_cast<int>(pending_exits_[symbol_idx].size()) - 1;
             order_idx >= 0; order_idx--) {
          if (const auto& pending_exit = pending_exits_[symbol_idx][order_idx];
              target_name == pending_exit->GetEntryName()) {
            Cancel(pending_exit->GetExitName());
          }
        }
      }

      // 자금, 통계 업데이트
      ExecuteExit(market_exit);
    } else {
      // 시장가 청산 대기 -> 실제 체결은 다음 봉의 시가
      pending_exits_[symbol_idx].push_back(market_exit);
    }
  } catch ([[maybe_unused]] const OrderFailed& e) {
    LogFormattedInfo(WARNING_L, format("시장가 [{}] 주문 실패", exit_name),
                     __FILE__, __LINE__);
  }
}

void OrderHandler::LimitExit(const string& exit_name, const string& target_name,
                             double order_price, double order_size) {
  try {
    // 바 정보 로딩
    const auto symbol_idx = bar_->GetCurrentSymbolIndex();
    int64_t order_time;
    double base_price = 0.0;
    try {
      // On Close 전략일 시 주문 시간과 기준 가격은 다음 봉의 Open Time과 Open
      if (const auto& strategy_type = engine_->GetCurrentStrategyType();
          strategy_type == ON_CLOSE) {
        const auto& next_bar =
            bar_->GetBarData(bar_->GetCurrentBarType(), "NONE")
                ->SafeGetBar(symbol_idx, bar_->GetCurrentBarIndex() + 1);
        order_time = next_bar.open_time;
        base_price = next_bar.open;
      } else {
        // After Entry 혹은 After Exit 전략일 시 주문 시간은 현재 바의 Open
        // Time, 기준 가격은 마지막 진입 가격 혹은 마지막 청산 가격

        order_time = engine_->GetCurrentOpenTime();

        if (strategy_type == AFTER_ENTRY) {
          base_price = LastEntryPrice();
        } else if (strategy_type == AFTER_EXIT) {
          base_price = LastExitPrice();
        }
      }
    } catch ([[maybe_unused]] const IndexOutOfRange& e) {
      LogFormattedInfo(WARNING_L, "마지막 바에서 지정가 청산 주문 불가",
                       __FILE__, __LINE__);
      throw OrderFailed("지정가 청산 실패");
    }

    // 원본 진입 주문 찾기
    shared_ptr<Order> entry_order;
    try {
      entry_order = FindEntryOrder(target_name).first;
    } catch (const EntryOrderNotFound& e) {
      // 원본 진입 주문을 찾지 못하면 청산 실패
      LogFormattedInfo(WARNING_L, e.what(), __FILE__, __LINE__);
      throw OrderFailed("지정가 청산 실패");
    }

    // 주문 가격을 틱 사이즈로 반올림
    order_price =
        RoundToTickSize(order_price, symbol_info_[symbol_idx].GetTickSize());

    // 총 청산 주문 수량이 진입 체결 수량보다 크지 않게 조정
    order_size = GetAdjustedExitSize(order_size, entry_order);

    // 청산 주문 생성
    const auto limit_exit = make_shared<Order>(*entry_order);
    limit_exit->SetExitName(exit_name)
        .SetExitOrderType(LIMIT)
        .SetExitDirection(limit_exit->GetEntryDirection() == LONG ? SHORT
                                                                  : LONG)
        .SetExitOrderTime(order_time)
        .SetExitOrderPrice(order_price)
        .SetExitOrderSize(order_size);

    // 유효성 검사
    try {
      IsValidExitName(exit_name);
      IsValidPrice(order_price);
      IsValidLimitOrderPrice(order_price, base_price,
                             limit_exit->GetExitDirection());
      IsValidPositionSize(order_size, LIMIT);
      IsValidNotionalValue(order_price, order_size);
    } catch (const InvalidValue& e) {
      LogFormattedInfo(WARNING_L, e.what(), __FILE__, __LINE__);
      throw OrderFailed("지정가 청산 실패");
    }

    // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 (주문 수정)
    Cancel(exit_name);

    // 대기 중인 청산에 추가
    pending_exits_[symbol_idx].push_back(limit_exit);

    LogFormattedInfo(INFO_L,
                     format("지정가 [{}] 주문 (주문가 {} | 주문량 {})",
                            exit_name, order_price, order_size),
                     __FILE__, __LINE__);
  } catch ([[maybe_unused]] const OrderFailed& e) {
    LogFormattedInfo(WARNING_L, format("지정가 [{}] 주문 실패", exit_name),
                     __FILE__, __LINE__);
  }
}

void OrderHandler::MitExit(const string& exit_name, const string& target_name,
                           double touch_price, double order_size) {
  try {
    // 바 정보 로딩
    const auto symbol_idx = bar_->GetCurrentSymbolIndex();
    double base_price = 0.0;
    try {
      // On Close 전략일 시 기준 가격은 다음 봉의 Open
      if (const auto& strategy_type = engine_->GetCurrentStrategyType();
          strategy_type == ON_CLOSE) {
        const auto& next_bar =
            bar_->GetBarData(bar_->GetCurrentBarType(), "NONE")
                ->SafeGetBar(symbol_idx, bar_->GetCurrentBarIndex() + 1);
        base_price = next_bar.open;
      } else {
        // After Entry 혹은 After Exit 전략일 시
        // 기준 가격은 마지막 진입 가격 혹은 마지막 청산 가격

        if (strategy_type == AFTER_ENTRY) {
          base_price = LastEntryPrice();
        } else if (strategy_type == AFTER_EXIT) {
          base_price = LastExitPrice();
        }
      }
    } catch ([[maybe_unused]] const IndexOutOfRange& e) {
      LogFormattedInfo(WARNING_L, "마지막 바에서 MIT 청산 대기 주문 불가",
                       __FILE__, __LINE__);
      throw OrderFailed("MIT 청산 실패");
    }

    // 원본 진입 주문 찾기
    shared_ptr<Order> entry_order;
    try {
      entry_order = FindEntryOrder(target_name).first;
    } catch (const EntryOrderNotFound& e) {
      // 원본 진입 주문을 찾지 못하면 청산 실패
      LogFormattedInfo(WARNING_L, e.what(), __FILE__, __LINE__);
      throw OrderFailed("MIT 청산 실패");
    }

    // 터치 가격을 틱 사이즈로 반올림
    touch_price =
        RoundToTickSize(touch_price, symbol_info_[symbol_idx].GetTickSize());

    // 총 청산 주문 수량이 진입 체결 수량보다 크지 않게 조정
    order_size = GetAdjustedExitSize(order_size, entry_order);

    // 청산 주문 생성
    const auto mit_exit = make_shared<Order>(*entry_order);
    mit_exit->SetExitName(exit_name)
        .SetExitOrderType(MIT)
        .SetExitDirection(mit_exit->GetEntryDirection() == LONG ? SHORT : LONG)
        .SetExitTouchPrice(touch_price)
        .SetExitTouchDirection(
            IsGreaterOrEqual(touch_price, base_price) ? LONG : SHORT)
        .SetExitOrderSize(order_size);

    // 유효성 검사
    try {
      IsValidExitName(exit_name);
      IsValidPrice(touch_price);
      IsValidPositionSize(order_size, MIT);
      IsValidNotionalValue(touch_price, order_size);
    } catch (const InvalidValue& e) {
      LogFormattedInfo(WARNING_L, e.what(), __FILE__, __LINE__);
      throw OrderFailed("MIT 청산 실패");
    }

    // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 (주문 수정)
    Cancel(exit_name);

    // 대기 중인 청산에 추가
    pending_exits_[symbol_idx].push_back(mit_exit);

    LogFormattedInfo(INFO_L,
                     format("MIT [{}] 대기 주문 (터치가 {} | 주문량 {})",
                            exit_name, touch_price, order_size),
                     __FILE__, __LINE__);
  } catch ([[maybe_unused]] const OrderFailed& e) {
    LogFormattedInfo(WARNING_L, format("MIT [{}] 대기 주문 실패", exit_name),
                     __FILE__, __LINE__);
  }
}

void OrderHandler::LitExit(const string& exit_name, const string& target_name,
                           double touch_price, double order_price,
                           double order_size) {
  try {
    // 바 정보 로딩
    const auto symbol_idx = bar_->GetCurrentSymbolIndex();
    double base_price = 0.0;
    try {
      // On Close 전략일 시 기준 가격은 다음 봉의 Open
      if (const auto& strategy_type = engine_->GetCurrentStrategyType();
          strategy_type == ON_CLOSE) {
        const auto& next_bar =
            bar_->GetBarData(bar_->GetCurrentBarType(), "NONE")
                ->SafeGetBar(symbol_idx, bar_->GetCurrentBarIndex() + 1);
        base_price = next_bar.open;
      } else {
        // After Entry 혹은 After Exit 전략일 시
        // 기준 가격은 마지막 진입 가격 혹은 마지막 청산 가격

        if (strategy_type == AFTER_ENTRY) {
          base_price = LastEntryPrice();
        } else if (strategy_type == AFTER_EXIT) {
          base_price = LastExitPrice();
        }
      }
    } catch ([[maybe_unused]] const IndexOutOfRange& e) {
      LogFormattedInfo(WARNING_L, "마지막 바에서 LIT 청산 대기 주문 불가",
                       __FILE__, __LINE__);
      throw OrderFailed("LIT 청산 실패");
    }

    // 원본 진입 주문 찾기
    shared_ptr<Order> entry_order;
    try {
      entry_order = FindEntryOrder(target_name).first;
    } catch (const EntryOrderNotFound& e) {
      // 원본 진입 주문을 찾지 못하면 청산 실패
      LogFormattedInfo(WARNING_L, e.what(), __FILE__, __LINE__);
      throw OrderFailed("LIT 청산 실패");
    }

    // 터치 가격과 주문 가격을 틱 사이즈로 반올림
    const auto tick_size = symbol_info_[symbol_idx].GetTickSize();
    touch_price = RoundToTickSize(touch_price, tick_size);
    order_price = RoundToTickSize(order_price, tick_size);

    // 총 청산 주문 수량이 진입 체결 수량보다 크지 않게 조정
    order_size = GetAdjustedExitSize(order_size, entry_order);

    // 청산 주문 생성
    const auto lit_exit = make_shared<Order>(*entry_order);
    lit_exit->SetExitName(exit_name)
        .SetExitOrderType(LIT)
        .SetExitDirection(lit_exit->GetEntryDirection() == LONG ? SHORT : LONG)
        .SetExitTouchPrice(touch_price)
        .SetExitTouchDirection(
            IsGreaterOrEqual(touch_price, base_price) ? LONG : SHORT)
        .SetExitOrderPrice(order_price)
        .SetExitOrderSize(order_size);

    // 유효성 검사
    try {
      IsValidExitName(exit_name);
      IsValidPrice(touch_price);
      IsValidPrice(order_price);
      IsValidLimitOrderPrice(order_price, touch_price,
                             lit_exit->GetExitDirection());
      IsValidPositionSize(order_size, LIT);
      IsValidNotionalValue(order_price, order_size);
    } catch (const InvalidValue& e) {
      LogFormattedInfo(WARNING_L, e.what(), __FILE__, __LINE__);
      throw OrderFailed("LIT 청산 실패");
    }

    // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 (주문 수정)
    Cancel(exit_name);

    // 대기 중인 청산에 추가
    pending_exits_[symbol_idx].push_back(lit_exit);

    LogFormattedInfo(
        INFO_L,
        format("LIT [{}] 대기 주문 (터치가 {} | 주문가 {} | 주문량 {})",
               exit_name, touch_price, order_price, order_size),
        __FILE__, __LINE__);
  } catch ([[maybe_unused]] const OrderFailed& e) {
    LogFormattedInfo(WARNING_L, format("LIT [{}] 대기 주문 실패", exit_name),
                     __FILE__, __LINE__);
  }
}

void OrderHandler::TrailingExit(const string& exit_name,
                                const string& target_name, double touch_price,
                                const double trail_point, double order_size) {
  try {
    // 바 정보 로딩
    const auto symbol_idx = bar_->GetCurrentSymbolIndex();
    double base_price = 0.0;
    try {
      // On Close 전략일 시 기준 가격은 다음 봉의 Open
      if (const auto& strategy_type = engine_->GetCurrentStrategyType();
          strategy_type == ON_CLOSE) {
        const auto& next_bar =
            bar_->GetBarData(bar_->GetCurrentBarType(), "NONE")
                ->SafeGetBar(symbol_idx, bar_->GetCurrentBarIndex() + 1);
        base_price = next_bar.open;
      } else {
        // After Entry 혹은 After Exit 전략일 시
        // 기준 가격은 마지막 진입 가격 혹은 마지막 청산 가격

        if (strategy_type == AFTER_ENTRY) {
          base_price = LastEntryPrice();
        } else if (strategy_type == AFTER_EXIT) {
          base_price = LastExitPrice();
        }
      }
    } catch ([[maybe_unused]] const IndexOutOfRange& e) {
      LogFormattedInfo(WARNING_L, "마지막 바에서 트레일링 청산 대기 주문 불가",
                       __FILE__, __LINE__);
      throw OrderFailed("트레일링 청산 실패");
    }

    // 원본 진입 주문 찾기
    shared_ptr<Order> entry_order;
    try {
      entry_order = FindEntryOrder(target_name).first;
    } catch (const EntryOrderNotFound& e) {
      // 원본 진입 주문을 찾지 못하면 청산 실패
      LogFormattedInfo(WARNING_L, e.what(), __FILE__, __LINE__);
      throw OrderFailed("트레일링 청산 실패");
    }

    // 터치 가격을 틱 사이즈로 반올림
    touch_price =
        RoundToTickSize(touch_price, symbol_info_[symbol_idx].GetTickSize());

    // 총 청산 주문 수량이 진입 체결 수량보다 크지 않게 조정
    order_size = GetAdjustedExitSize(order_size, entry_order);

    // 청산 주문 생성
    const auto trailing_exit = make_shared<Order>(*entry_order);
    trailing_exit->SetExitName(exit_name)
        .SetExitOrderType(TRAILING)
        .SetExitDirection(trailing_exit->GetEntryDirection() == LONG ? SHORT
                                                                     : LONG)
        .SetExitTouchPrice(touch_price)
        .SetExitTouchDirection(
            IsGreaterOrEqual(touch_price, base_price) ? LONG : SHORT)
        .SetExitTrailPoint(trail_point)
        .SetExitOrderSize(order_size);

    // touch_price가 0이면 다음 시가부터 최고저가 추적을 시작
    if (IsEqual(touch_price, 0.0)) {
      trailing_exit->SetEntryExtremePrice(base_price);
    }

    // 유효성 검사
    try {
      IsValidExitName(exit_name);
      IsValidTrailingTouchPrice(touch_price);
      IsValidTrailPoint(trail_point);
      IsValidPositionSize(order_size, TRAILING);

      double start_price;
      if (IsEqual(touch_price, 0.0)) {
        start_price = base_price;
      } else {
        start_price = touch_price;
      }

      IsValidNotionalValue(  // 가장 불리한 청산가로 검사
          trailing_exit->GetExitDirection() == LONG ? start_price + trail_point
                                                    : start_price - trail_point,
          order_size);
    } catch (const InvalidValue& e) {
      LogFormattedInfo(WARNING_L, e.what(), __FILE__, __LINE__);
      throw OrderFailed("트레일링 청산 실패");
    }

    // 해당 주문과 같은 이름의 대기 주문이 있으면 취소 (주문 수정)
    Cancel(exit_name);

    // 대기 중인 청산에 추가
    pending_exits_[symbol_idx].push_back(trailing_exit);

    if (!IsEqual(touch_price, 0.0)) {
      LogFormattedInfo(INFO_L,
                       format("트레일링 [{}] 대기 주문 (터치가 {} | "
                              "트레일 포인트 {} | 주문량 {})",
                              exit_name, touch_price, trail_point, order_size),
                       __FILE__, __LINE__);
    } else {
      // touch_price가 0이라 바로 추적 시작한 경우
      LogFormattedInfo(INFO_L,
                       format("트레일링 [{}] 주문 (터치가 {} | "
                              "트레일 포인트 {} | 주문량 {})",
                              exit_name, touch_price, trail_point, order_size),
                       __FILE__, __LINE__);
    }
  } catch ([[maybe_unused]] const OrderFailed& e) {
    if (!IsEqual(touch_price, 0.0)) {
      LogFormattedInfo(WARNING_L,
                       format("트레일링 [{}] 대기 주문 실패", exit_name),
                       __FILE__, __LINE__);
    } else {
      LogFormattedInfo(WARNING_L, format("트레일링 [{}] 주문 실패", exit_name),
                       __FILE__, __LINE__);
    }
  }
}

void OrderHandler::CancelAll() {
  const auto symbol_idx = bar_->GetCurrentSymbolIndex();

  // 루프 중간에 주문이 삭제되므로 역순으로 순회
  for (int order_idx =
           static_cast<int>(pending_entries_[symbol_idx].size()) - 1;
       order_idx >= 0; order_idx--) {
    Cancel(pending_entries_[symbol_idx][order_idx]->GetEntryName());
  }

  for (int order_idx = static_cast<int>(pending_exits_[symbol_idx].size()) - 1;
       order_idx >= 0; order_idx--) {
    Cancel(pending_exits_[symbol_idx][order_idx]->GetExitName());
  }
}

void OrderHandler::CloseAll() {
  const auto original_strategy_type = engine_->GetCurrentStrategyType();
  engine_->SetCurrentStrategyType(ON_CLOSE);

  // 루프 중간에 주문이 삭제되므로 역순으로 순회
  const auto symbol_idx = bar_->GetCurrentSymbolIndex();
  for (int order_idx = static_cast<int>(filled_entries_[symbol_idx].size()) - 1;
       order_idx >= 0; order_idx--) {
    const auto& filled_entry = filled_entries_[symbol_idx][order_idx];

    MarketExit(
        "전량 청산", filled_entry->GetEntryName(),
        filled_entry->GetEntryFilledSize() - filled_entry->GetExitFilledSize());
  }

  engine_->SetCurrentStrategyType(original_strategy_type);
}

void OrderHandler::ExecuteLiquidation(const string& exit_name,
                                      const int symbol_idx, const int order_idx,
                                      const double order_price) {
  // 바 정보 로딩
  const auto current_open_time = engine_->GetCurrentOpenTime();

  // 진입 체결 주문을 받아오고 진입 체결 주문에서 삭제
  auto& filled_entries = filled_entries_[symbol_idx];
  const auto entry_order = filled_entries[order_idx];
  filled_entries.erase(filled_entries.begin() + order_idx);

  // 강제 청산 주문 생성
  const auto liquidation_exit = make_shared<Order>(*entry_order);
  const auto exit_size = liquidation_exit->GetEntryFilledSize() -
                         liquidation_exit->GetExitFilledSize();
  liquidation_exit->SetExitName(exit_name)
      .SetExitOrderType(MARKET)
      .SetExitDirection(liquidation_exit->GetEntryDirection() == LONG ? SHORT
                                                                      : LONG)
      .SetExitOrderTime(current_open_time)
      .SetExitOrderPrice(order_price)
      .SetExitOrderSize(exit_size)
      .SetExitFilledTime(current_open_time)
      .SetExitFilledSize(exit_size);

  // 슬리피지가 포함된 체결가 계산
  const double slippage_filled_price = CalculateSlippagePrice(
      MARKET, liquidation_exit->GetExitDirection(), order_price);

  // 체결가 및 청산/강제 청산 수수료 설정
  liquidation_exit->SetExitFilledPrice(slippage_filled_price)
      .SetExitFee(CalculateTradingFee(MARKET, slippage_filled_price, exit_size))
      .SetLiquidationFee(slippage_filled_price * exit_size *
                         symbol_info_[symbol_idx].GetLiquidationFeeRate());

  // 강제 청산된 진입 이름을 목표로 하는 청산 대기 주문 취소
  // 여러 주문이 취소될 될 수도 있으므로 역순으로 순회
  const auto& target_entry_name = liquidation_exit->GetEntryName();
  for (int idx = static_cast<int>(pending_exits_[symbol_idx].size()) - 1;
       idx >= 0; idx--) {
    if (const auto& pending_exit = pending_exits_[symbol_idx][idx];
        target_entry_name == pending_exit->GetEntryName()) {
      Cancel(pending_exit->GetExitName());
    }
  }

  // 자금, 통계 업데이트
  ExecuteExit(liquidation_exit);
}

void OrderHandler::ExecuteMarketEntry(const shared_ptr<Order>& market_entry,
                                      const PriceType price_type) {
  // 필요한 정보 로딩
  const auto symbol_idx = bar_->GetCurrentSymbolIndex();
  const auto entry_direction = market_entry->GetEntryDirection();
  const auto entry_filled_price = market_entry->GetEntryFilledPrice();
  const auto entry_filled_size = market_entry->GetEntryFilledSize();
  const auto& order_type_str =
      Order::OrderTypeToString(market_entry->GetEntryOrderType());
  const string& entry_name = market_entry->GetEntryName();
  const auto entry_fee = market_entry->GetEntryFee();

  // 시장가 진입 마진 계산
  const double entry_margin =
      CalculateMargin(entry_filled_price, entry_filled_size, price_type);
  market_entry->SetEntryMargin(entry_margin).SetLeftMargin(entry_margin);

  // 강제 청산 가격 계산
  market_entry->SetLiquidationPrice(CalculateLiquidationPrice(
      entry_direction, entry_filled_price, entry_filled_size, entry_margin));

  // 진입 가능 여부 체크 (사용 가능 자금 >= 시장가 진입 마진 + 진입 수수료)
  try {
    HasEnoughBalance(engine_->GetAvailableBalance(), entry_margin + entry_fee,
                     "사용 가능",
                     format("{} 진입 마진 및 진입 수수료", order_type_str));
  } catch (const InsufficientBalance& e) {
    LogFormattedInfo(WARNING_L, e.what(), __FILE__, __LINE__);
    throw OrderFailed("시장가 진입 실패");
  }

  // 해당 주문과 반대 방향의 체결 주문이 있으면 모두 청산
  ExitOppositeFilledEntries(entry_direction,
                            market_entry->GetEntryOrderPrice());

  // 지갑 자금에서 진입 수수료 차감
  engine_->DecreaseWalletBalance(entry_fee);

  // 수수료 차감 로그
  LogFormattedInfo(INFO_L,
                   format("{} [{}] 진입 수수료 [{}] 차감", order_type_str,
                          entry_name, FormatDollar(entry_fee, true)),
                   __FILE__, __LINE__);
  engine_->LogBalance();

  // 사용한 마진에 시장가 진입 마진 증가
  engine_->IncreaseUsedMargin(entry_margin);

  // 전역 항목들 업데이트
  UpdateLastEntryBarIndex(symbol_idx);
  last_entry_prices_[symbol_idx] = entry_filled_price;
  just_entered_ = true;

  // 시장가 진입
  filled_entries_[symbol_idx].push_back(market_entry);

  LogFormattedInfo(INFO_L,
                   format("{} [{}] 체결 (체결가 {} | 체결량 {} | 진입 마진 {})",
                          order_type_str, entry_name, entry_filled_price,
                          entry_filled_size, FormatDollar(entry_margin, true)),
                   __FILE__, __LINE__);
  engine_->LogBalance();
}

void OrderHandler::ExitOppositeFilledEntries(
    const Direction target_entry_direction, const double entry_order_price) {
  is_reverse_exit_ = true;
  reverse_exit_price_ = entry_order_price;

  const auto symbol_idx = bar_->GetCurrentSymbolIndex();

  // 여러 주문이 청산될 수도 있으므로 역순으로 순회
  for (int order_idx = static_cast<int>(filled_entries_[symbol_idx].size()) - 1;
       order_idx >= 0; order_idx--) {
    const auto& filled_entry = filled_entries_[symbol_idx][order_idx];

    if (const auto filled_entry_direction = filled_entry->GetEntryDirection();
        target_entry_direction != filled_entry_direction) {
      MarketExit(filled_entry_direction == LONG ? "매수 청산 (리버스)"
                                                : "매도 청산 (리버스)",
                 filled_entry->GetEntryName(),
                 filled_entry->GetEntryFilledSize() -
                     filled_entry->GetExitFilledSize());
    }
  }

  is_reverse_exit_ = false;
  reverse_exit_price_ = NAN;
}

void OrderHandler::ExecuteExit(const shared_ptr<Order>& exit_order) {
  // 주문 정보 로딩
  const auto symbol_idx = bar_->GetCurrentSymbolIndex();
  const auto entry_direction = exit_order->GetEntryDirection();
  const auto& entry_name = exit_order->GetEntryName();
  const auto entry_filled_price = exit_order->GetEntryFilledPrice();
  const auto entry_filled_size = exit_order->GetEntryFilledSize();
  const auto exit_filled_price = exit_order->GetExitFilledPrice();
  const auto exit_filled_size = exit_order->GetExitFilledSize();
  const auto left_margin = exit_order->GetLeftMargin();
  const auto liquidation_price = exit_order->GetLiquidationPrice();
  const auto& order_type_str =
      Order::OrderTypeToString(exit_order->GetExitOrderType());
  const auto& exit_name = exit_order->GetExitName();

  // 청산 횟수 추가
  exit_order->AddExitCount();

  // 지갑 자금에서 청산 수수료 차감
  const auto exit_fee = exit_order->GetExitFee();
  engine_->DecreaseWalletBalance(exit_fee);

  // 수수료 차감 로그
  LogFormattedInfo(INFO_L,
                   format("{} [{}] 청산 수수료 [{}] 차감", order_type_str,
                          exit_name, FormatDollar(exit_fee, true)),
                   __FILE__, __LINE__);
  engine_->LogBalance();

  try {
    // 원본 진입 찾기 (존재하면 분할 청산, 미존재하면 전량 청산)
    const auto& entry_order = FindEntryOrder(entry_name).first;

    // 이번 청산 실행 전 보유 포지션 수량 대비 청산 수량 비율 계산
    // ExecuteExit 함수 호출 전, 진입 주문의 총 청산량에 이번 청산의 청산 수량이
    // 포함되도록 계산을 하기 때문에 때문에 조정이 필요함
    const auto total_exit_filled_size = entry_order->GetExitFilledSize();
    const auto exit_size_ratio =
        exit_filled_size /
        (entry_filled_size - total_exit_filled_size + exit_filled_size);

    // 분할 청산이라면 이번 청산 수량 비율만큼 사용한 마진 감소
    const auto exit_margin = left_margin * exit_size_ratio;

    // 사용한 마진 감소
    engine_->DecreaseUsedMargin(exit_margin);

    // 부분 청산 시 포지션 수량 비율에 따른 펀딩비 조정
    const auto received_funding_amount =
        entry_order->GetReceivedFundingAmount();
    const auto exit_received_funding_amount =
        received_funding_amount * exit_size_ratio;
    exit_order->SetReceivedFundingAmount(exit_received_funding_amount);

    const auto paid_funding_amount = entry_order->GetPaidFundingAmount();
    const auto exit_paid_funding_amount = paid_funding_amount * exit_size_ratio;
    exit_order->SetPaidFundingAmount(exit_paid_funding_amount);

    // 진입 마진이 감소했으므로 원본 진입과 원본 진입을 목표로 하는
    // 청산 대기 주문들의 잔여 마진과 강제 청산 가격 수정
    //
    // 진입 주문으로 청산 체결량을 로딩한 이유는, 원본 진입의 청산 체결량이
    // 누적 청산 체결량이기 때문에 정확한 진입 잔량을 구할 수 있기 때문
    const auto adjusted_margin = left_margin - exit_margin;
    const auto adjusted_liquidation_price = CalculateLiquidationPrice(
        entry_direction, entry_filled_price,
        entry_filled_size - total_exit_filled_size, adjusted_margin);
    const auto adjusted_received_funding_amount =
        received_funding_amount - exit_received_funding_amount;
    const auto adjusted_paid_funding_amount =
        paid_funding_amount - exit_paid_funding_amount;

    entry_order->SetLeftMargin(adjusted_margin)
        .SetLiquidationPrice(adjusted_liquidation_price)
        .SetReceivedFundingAmount(adjusted_received_funding_amount)
        .SetPaidFundingAmount(adjusted_paid_funding_amount)
        .AddExitCount();

    for (const auto& pending_exit : pending_exits_[symbol_idx]) {
      if (pending_exit->GetEntryName() == entry_name) {
        pending_exit->SetLeftMargin(adjusted_margin)
            .SetLiquidationPrice(adjusted_liquidation_price)
            .SetReceivedFundingAmount(adjusted_received_funding_amount)
            .SetPaidFundingAmount(adjusted_paid_funding_amount)
            .AddExitCount();
      }
    }

    LogFormattedInfo(
        INFO_L,
        format(
            "[{}] 부분 청산으로 인해 잔여 마진 [{}] → [{}], 강제 청산 가격 "
            "[{}] → [{}], 펀딩비 수령 [{}] → [{}], 펀딩비 지불 [{}] -> [{}] "
            "조정 (진입 수량 [{}] | 부분 청산 수량 [{}] | 전체 청산 수량 [{}])",
            entry_name, FormatDollar(left_margin, true),
            FormatDollar(adjusted_margin, true), liquidation_price,
            adjusted_liquidation_price,
            FormatDollar(received_funding_amount, true),
            FormatDollar(adjusted_received_funding_amount, true),
            FormatDollar(paid_funding_amount, true),
            FormatDollar(adjusted_paid_funding_amount, true), entry_filled_size,
            exit_filled_size, total_exit_filled_size),
        __FILE__, __LINE__);

    // 명목 가치가 감소했는데 레버리지 구간에 의한 현재 레버리지 조정을 하지
    // 않는 이유는, 명목 가치가 감소할수록 레버리지 구간에서 최대 레버리지가
    // 증가하기 때문
  } catch ([[maybe_unused]] const EntryOrderNotFound& e) {
    // 전량 청산이라면 이 함수 실행 전 진입 주문에서 이 주문의 원본 주문이
    // 삭제됨 즉, 진입 주문을 찾지 못하면 전량 청산된 것
    //
    // 전량 청산이라면 잔여 마진만큼 사용한 마진 감소
    engine_->DecreaseUsedMargin(left_margin);
  }

  // 실현 손익 계산
  const double calculated_pnl = CalculatePnl(
      entry_direction, exit_filled_price, entry_filled_price, exit_filled_size);
  double realized_pnl = calculated_pnl;
  const double abs_realized_pnl = abs(calculated_pnl);
  bool is_bankruptcy_position = false;

  // 지갑 자금에 실현 손익 계산
  if (calculated_pnl >= 0.0) {
    engine_->IncreaseWalletBalance(calculated_pnl);
  } else {
    // 실현 손실이 청산 시 잔여 마진과 같거나 크다면 파산 포지션으로,
    // 추가 손실은 보험 기금으로 충당됨.
    // 즉, 최대 손실은 잔여 마진
    if (IsGreaterOrEqual(abs_realized_pnl, left_margin)) {
      realized_pnl = -left_margin;
      is_bankruptcy_position = true;
    } else {
      realized_pnl = -abs_realized_pnl;
    }

    engine_->DecreaseWalletBalance(abs(realized_pnl));
  }

  LogFormattedInfo(
      IsEqual(exit_order->GetLiquidationFee(), 0.0) ? INFO_L : ERROR_L,
      format("{} [{}] 체결 ({} [{}] | 체결가 {} | 체결량 {} | 마진 {} | "
             "손익 {} | 실손익 {})",
             order_type_str, exit_name,
             Order::OrderTypeToString(exit_order->GetEntryOrderType()),
             entry_name, exit_filled_price,
             RoundToTickSize(exit_order->GetExitFilledSize(),
                             symbol_info_[symbol_idx].GetQtyStep()),
             FormatDollar(left_margin, true),
             FormatDollar(calculated_pnl, true),
             FormatDollar(realized_pnl, true)),
      __FILE__, __LINE__);
  engine_->LogBalance();

  // 강제 청산이고 파산 포지션이 아니라면 강제 청산 수수료 부과.
  // 수수료는 청산 시 잔여 마진에서 실현 손실을 뺀 잔여 마진까지만 부과
  if (const auto liquidation_fee = exit_order->GetLiquidationFee();
      !IsEqual(liquidation_fee, 0.0)) {
    // 파산 포지션이거나, 잔여 마진이 0인 경우에는 강제 청산 수수료는
    // 보험 기금에서 충당
    // -> PnL이 양수여도 펀딩비 부족으로 강제 청산 당할 수 있음
    if (!is_bankruptcy_position && left_margin != 0.0) {
      double left_margin_after_exit;
      double real_liquidation_fee;

      if (left_margin_after_exit = left_margin - abs_realized_pnl;
          IsGreaterOrEqual(liquidation_fee, left_margin_after_exit)) {
        // 계산된 강제 청산 수수료가 PnL 정산 후 잔여 마진보다 많다면 잔여
        // 마진까지만 부과
        real_liquidation_fee = left_margin_after_exit;
      } else {
        real_liquidation_fee = liquidation_fee;
      }

      exit_order->SetLiquidationFee(real_liquidation_fee);
      engine_->DecreaseWalletBalance(real_liquidation_fee);

      LogFormattedInfo(
          ERROR_L,
          format(
              "[{}] 강제 청산 수수료 [{}] 차감 (계산된 강제 청산 수수료 {} | "
              "청산 후 잔여 마진 {})",
              entry_name, FormatDollar(real_liquidation_fee, true),
              FormatDollar(liquidation_fee, true),
              FormatDollar(left_margin_after_exit, true)),
          __FILE__, __LINE__);
      engine_->LogBalance();
    } else {
      exit_order->SetLiquidationFee(0);
    }
  }

  // 전역 항목들 업데이트
  engine_->UpdateStatistics();
  UpdateLastExitBarIndex(symbol_idx);
  last_exit_prices_[symbol_idx] = exit_filled_price;
  just_exited_ = true;

  // 분석기에 청산된 거래 추가
  AddTrade(exit_order, realized_pnl);
}

void OrderHandler::CheckPendingLimitEntries(const int symbol_idx,
                                            const int order_idx,
                                            const double price,
                                            const PriceType price_type) {
  const auto limit_entry = pending_entries_[symbol_idx][order_idx];

  if (const auto order_price = limit_entry->GetEntryOrderPrice();
      IsLimitPriceSatisfied(limit_entry->GetEntryDirection(), price,
                            order_price)) {
    try {
      // 시가에서 진입 조건 달성 시 체결 가격은 시가가 됨 (갭 고려)
      FillPendingLimitEntry(symbol_idx, order_idx,
                            price_type == OPEN ? price : order_price,
                            price_type);
    } catch ([[maybe_unused]] const OrderFailed& e) {
      LogFormattedInfo(
          WARNING_L,
          format("지정가 [{}] 체결 실패", limit_entry->GetEntryName()),
          __FILE__, __LINE__);
    }
  }
}

void OrderHandler::CheckPendingMitEntries(const int symbol_idx,
                                          const int order_idx,
                                          const double price,
                                          const PriceType price_type) {
  const auto mit_entry = pending_entries_[symbol_idx][order_idx];

  if (const auto touch_price = mit_entry->GetEntryTouchPrice();
      IsPriceTouched(mit_entry->GetEntryTouchDirection(), price, touch_price)) {
    try {
      // 시가에서 터치 조건 달성 시 주문 가격은 시가가 됨 (갭 고려)
      FillPendingMarketEntry(symbol_idx, order_idx,
                             price_type == OPEN ? price : touch_price,
                             price_type);
    } catch ([[maybe_unused]] const OrderFailed& e) {
      LogFormattedInfo(WARNING_L,
                       format("MIT [{}] 체결 실패", mit_entry->GetEntryName()),
                       __FILE__, __LINE__);
    }
  }
}

void OrderHandler::CheckPendingLitEntries(const int symbol_idx,
                                          const int order_idx,
                                          const double price,
                                          const PriceType price_type) {
  const auto lit_entry = pending_entries_[symbol_idx][order_idx];

  // Order Time이 설정되지 않았으면 지정가 미주문 -> 터치 확인
  if (lit_entry->GetEntryOrderTime() == -1) {
    if (IsPriceTouched(lit_entry->GetEntryTouchDirection(), price,
                       lit_entry->GetEntryTouchPrice())) {
      try {
        OrderPendingLitEntry(symbol_idx, order_idx, price_type);
      } catch ([[maybe_unused]] const OrderFailed& e) {
        // 주문 실패 시 해당 주문 삭제 후 다음 주문 확인으로 넘어감
        LogFormattedInfo(
            WARNING_L,
            format("LIT [{}] 대기 주문 취소", lit_entry->GetEntryName()),
            __FILE__, __LINE__);
        return;
      }
    }
  }

  // Order Time이 설정됐으면 지정가 주문 완료 -> 체결 확인
  // 터치 후 바로 진입될 수도 있으므로 Order Time을 다시 불러와서 확인
  if (lit_entry->GetEntryOrderTime() != -1) {
    if (const auto order_price = lit_entry->GetEntryOrderPrice();
        IsLimitPriceSatisfied(lit_entry->GetEntryDirection(), price,
                              order_price)) {
      try {
        // 시가에서 터치 조건 달성 시 체결 가격은 시가가 됨 (갭 고려)
        FillPendingLimitEntry(symbol_idx, order_idx,
                              price_type == OPEN ? price : order_price,
                              price_type);
      } catch ([[maybe_unused]] const OrderFailed& e) {
        LogFormattedInfo(
            WARNING_L, format("LIT [{}] 체결 실패", lit_entry->GetEntryName()),
            __FILE__, __LINE__);
      }
    }
  }
}

void OrderHandler::CheckPendingTrailingEntries(const int symbol_idx,
                                               const int order_idx,
                                               const double price,
                                               const PriceType price_type) {
  const auto trailing_entry = pending_entries_[symbol_idx][order_idx];

  // Extreme Price가 지정되지 않았으면 추적 미시작 -> 터치 확인
  if (isnan(trailing_entry->GetEntryExtremePrice())) {
    if (IsPriceTouched(trailing_entry->GetEntryTouchDirection(), price,
                       trailing_entry->GetEntryTouchPrice())) {
      trailing_entry->SetEntryExtremePrice(price);
    }
  }

  try {
    // Extreme Price가 지정되었으면 추적 시작
    // -> 고저가 업데이트 및 체결 터치 확인
    if (double extreme_price = trailing_entry->GetEntryExtremePrice();
        !isnan(extreme_price)) {
      // 주문 정보 로딩
      const auto entry_direction = trailing_entry->GetEntryDirection();
      const auto trail_point = trailing_entry->GetEntryTrailPoint();

      if (entry_direction == LONG) {
        // 진입 방향이 매수인 경우, 최저가를 추적
        if (IsLess(price, extreme_price)) {
          trailing_entry->SetEntryExtremePrice(price);
          extreme_price = price;
        }

        // 진입 방향이 매수인 경우, 최저가로부터 Trail Point 증가 시 진입
        if (const double trail_price = extreme_price + trail_point;
            IsGreaterOrEqual(price, trail_price)) {
          // 시가에서 조건 달성 시 주문 가격은 시가가 됨 (갭 고려)
          FillPendingMarketEntry(symbol_idx, order_idx,
                                 price_type == OPEN ? price : trail_price,
                                 price_type);
        }
      } else if (entry_direction == SHORT) {
        // 진입 방향이 매도인 경우, 최고가를 추적
        if (IsGreater(price, extreme_price)) {
          trailing_entry->SetEntryExtremePrice(price);
          extreme_price = price;
        }

        // 진입 방향이 매도인 경우, 최고가로부터 Trail Point 감소 시 진입
        if (const double trail_price = extreme_price - trail_point;
            IsLessOrEqual(price, trail_price)) {
          // 시가에서 조건 달성 시 주문 가격은 시가가 됨 (갭 고려)
          FillPendingMarketEntry(symbol_idx, order_idx,
                                 price_type == OPEN ? price : trail_price,
                                 price_type);
        }
      }
    }
  } catch ([[maybe_unused]] const OrderFailed& e) {
    LogFormattedInfo(
        WARNING_L,
        format("트레일링 [{}] 체결 실패", trailing_entry->GetEntryName()),
        __FILE__, __LINE__);
  }
}

void OrderHandler::FillPendingMarketEntry(const int symbol_idx,
                                          const int order_idx,
                                          const double current_price,
                                          const PriceType price_type) {
  // 바 정보 로딩
  const auto current_open_time = engine_->GetCurrentOpenTime();

  // 대기 주문을 받아오고 대기 주문에서 삭제
  auto& pending_entries = pending_entries_[symbol_idx];
  const auto market_entry = pending_entries[order_idx];
  pending_entries.erase(pending_entries.begin() + order_idx);

  // 예외 체크
  try {
    IsValidEntryName(market_entry->GetEntryName());
  } catch (const InvalidValue& e) {
    LogFormattedInfo(WARNING_L, e.what(), __FILE__, __LINE__);
    throw OrderFailed("시장가 대기 주문 체결 실패");
  }

  // 주문 정보 로딩
  const auto entry_filled_size = market_entry->GetEntryOrderSize();
  const auto entry_direction = market_entry->GetEntryDirection();

  // 주문 업데이트
  market_entry->SetEntryOrderTime(current_open_time)
      .SetEntryOrderPrice(current_price)
      .SetEntryFilledTime(current_open_time)
      .SetEntryFilledSize(entry_filled_size);

  // 슬리피지가 포함된 체결가
  const double slippage_filled_price =
      CalculateSlippagePrice(MARKET, entry_direction, current_price);
  market_entry->SetEntryFilledPrice(slippage_filled_price);

  // 수수료
  market_entry->SetEntryFee(
      CalculateTradingFee(MARKET, slippage_filled_price, entry_filled_size));

  // 자금 관련 처리 후 체결 주문에 추가
  ExecuteMarketEntry(market_entry, price_type);
}

void OrderHandler::FillPendingLimitEntry(const int symbol_idx,
                                         const int order_idx,
                                         const double current_price,
                                         const PriceType price_type) {
  // 대기 주문을 받아오고 대기 주문에서 삭제
  auto& pending_entries = pending_entries_[symbol_idx];
  const auto limit_entry = pending_entries[order_idx];
  pending_entries.erase(pending_entries.begin() + order_idx);

  // 예외 체크
  const auto& entry_name = limit_entry->GetEntryName();
  try {
    IsValidEntryName(entry_name);
  } catch (const InvalidValue& e) {
    // 유효성 검증 실패 시 사용한 마진(예약 증거금) 감소
    engine_->DecreaseUsedMargin(limit_entry->GetEntryMargin());

    LogFormattedInfo(WARNING_L, e.what(), __FILE__, __LINE__);
    throw OrderFailed("지정가 대기 주문 체결 실패");
  }

  // 주문 정보 로딩
  const auto& order_type_str =
      Order::OrderTypeToString(limit_entry->GetEntryOrderType());
  const auto entry_filled_size = limit_entry->GetEntryOrderSize();
  const auto entry_direction = limit_entry->GetEntryDirection();

  // 주문 업데이트
  limit_entry->SetEntryFilledTime(engine_->GetCurrentOpenTime())
      .SetEntryFilledSize(entry_filled_size);

  // 슬리피지가 포함된 체결가
  const double slippage_filled_price =
      CalculateSlippagePrice(LIMIT, entry_direction, current_price);
  limit_entry->SetEntryFilledPrice(slippage_filled_price);

  // 수수료
  const double entry_fee =
      CalculateTradingFee(LIMIT, slippage_filled_price, entry_filled_size);
  limit_entry->SetEntryFee(entry_fee);

  // 현재 미실현 손실을 반영한 지정가 진입 마진 재계산
  const auto entry_margin =
      CalculateMargin(slippage_filled_price, entry_filled_size, price_type);

  // 강제 청산 가격
  limit_entry->SetLiquidationPrice(CalculateLiquidationPrice(
      entry_direction, slippage_filled_price, entry_filled_size, entry_margin));

  // 마진 재설정 후 진입 가능 자금과 재비교
  engine_->DecreaseUsedMargin(limit_entry->GetEntryMargin());
  limit_entry->SetEntryMargin(entry_margin).SetLeftMargin(entry_margin);

  try {
    HasEnoughBalance(engine_->GetAvailableBalance(), entry_margin + entry_fee,
                     "사용 가능",
                     format("{} 진입 마진 및 수수료", order_type_str));
  } catch (const InsufficientBalance& e) {
    LogFormattedInfo(WARNING_L, e.what(), __FILE__, __LINE__);
    throw OrderFailed("지정가 진입 실패");
  }

  // 해당 주문과 반대 방향의 체결 주문이 있으면 모두 청산
  ExitOppositeFilledEntries(entry_direction, limit_entry->GetEntryOrderPrice());

  // 지갑 자금에서 진입 수수료 차감
  engine_->DecreaseWalletBalance(entry_fee);

  // 수수료 차감 로그
  LogFormattedInfo(INFO_L,
                   format("{} [{}] 진입 수수료 [{}] 차감", order_type_str,
                          entry_name, FormatDollar(entry_fee, true)),
                   __FILE__, __LINE__);
  engine_->LogBalance();

  // 사용한 마진에 지정가 진입 마진 증가
  engine_->IncreaseUsedMargin(entry_margin);

  // 전역 항목들 업데이트
  UpdateLastEntryBarIndex(symbol_idx);
  last_entry_prices_[symbol_idx] = slippage_filled_price;
  just_entered_ = true;

  // 지정가 진입
  filled_entries_[symbol_idx].push_back(limit_entry);

  LogFormattedInfo(INFO_L,
                   format("{} [{}] 체결 (체결가 {} | 체결량 {} | 진입 마진 {})",
                          order_type_str, entry_name, slippage_filled_price,
                          entry_filled_size, FormatDollar(entry_margin, true)),
                   __FILE__, __LINE__);
  engine_->LogBalance();
}

void OrderHandler::OrderPendingLitEntry(const int symbol_idx,
                                        const int order_idx,
                                        const PriceType price_type) {
  // 대기 주문 로딩
  auto& pending_entries = pending_entries_[symbol_idx];
  const auto lit_entry = pending_entries[order_idx];
  const auto order_price = lit_entry->GetEntryOrderPrice();
  const auto order_size = lit_entry->GetEntryOrderSize();

  // 주문 업데이트
  lit_entry->SetEntryOrderTime(engine_->GetCurrentOpenTime());

  // 예약 증거금 계산
  const double entry_margin =
      CalculateMargin(order_price, order_size, price_type);

  lit_entry->SetEntryMargin(entry_margin).SetLeftMargin(entry_margin);

  // 주문 가능 여부 체크
  try {
    HasEnoughBalance(engine_->GetAvailableBalance(), entry_margin, "사용 가능",
                     "LIT 주문 마진");
  } catch (const InsufficientBalance& e) {
    LogFormattedInfo(WARNING_L, e.what(), __FILE__, __LINE__);

    // 주문 실패 시 대기 주문에서 삭제
    pending_entries.erase(pending_entries.begin() + order_idx);
    throw OrderFailed("LIT 주문 실패");
  }

  // 사용한 마진에 예약 증거금 증가
  engine_->IncreaseUsedMargin(entry_margin);

  LogFormattedInfo(INFO_L,
                   format("LIT [{}] 주문 (주문가 {} | 주문량 {})",
                          lit_entry->GetEntryName(), order_price, order_size),
                   __FILE__, __LINE__);
  engine_->LogBalance();
}

int OrderHandler::CheckPendingLimitExits(const int symbol_idx,
                                         const long long order_idx,
                                         const double price,
                                         const PriceType price_type) {
  const auto limit_exit = pending_exits_[symbol_idx][order_idx];

  if (const auto order_price = limit_exit->GetExitOrderPrice();
      IsLimitPriceSatisfied(limit_exit->GetExitDirection(), price,
                            order_price)) {
    try {
      // 시가에서 진입 조건 달성 시 체결 가격은 시가가 됨 (갭 고려)
      return FillPendingExitOrder(symbol_idx, order_idx,
                                  price_type == OPEN ? price : order_price);
    } catch ([[maybe_unused]] const OrderFailed& e) {
      // 청산 주문은 파산이 아닌 이상 체결되므로 엔진 오류
      LogFormattedInfo(
          ERROR_L,
          format("엔진 오류: 지정가 [{}] 체결 실패", limit_exit->GetExitName()),
          __FILE__, __LINE__);
      throw;
    }
  }

  return -1;
}

int OrderHandler::CheckPendingMitExits(const int symbol_idx,
                                       const long long order_idx,
                                       const double price,
                                       const PriceType price_type) {
  const auto mit_exit = pending_exits_[symbol_idx][order_idx];

  if (const auto touch_price = mit_exit->GetExitTouchPrice();
      IsPriceTouched(mit_exit->GetExitTouchDirection(), price, touch_price)) {
    try {
      // 시가에서 터치 조건 달성 시 주문 가격은 시가가 됨 (갭 고려)
      return FillPendingExitOrder(symbol_idx, order_idx,
                                  price_type == OPEN ? price : touch_price);
    } catch ([[maybe_unused]] const OrderFailed& e) {
      // 청산 주문은 파산이 아닌 이상 체결되므로 엔진 오류
      LogFormattedInfo(
          ERROR_L,
          format("엔진 오류: MIT [{}] 체결 실패", mit_exit->GetExitName()),
          __FILE__, __LINE__);
      throw;
    }
  }

  return -1;
}

int OrderHandler::CheckPendingLitExits(const int symbol_idx,
                                       const long long order_idx,
                                       const double price,
                                       const PriceType price_type) {
  const auto lit_exit = pending_exits_[symbol_idx][order_idx];

  // Order Time이 설정되지 않았으면 지정가 미주문 -> 터치 확인
  if (lit_exit->GetExitOrderTime() == -1) {
    if (IsPriceTouched(lit_exit->GetExitTouchDirection(), price,
                       lit_exit->GetExitTouchPrice())) {
      // 주문 업데이트
      lit_exit->SetExitOrderTime(engine_->GetCurrentOpenTime());

      LogFormattedInfo(
          INFO_L,
          format("LIT [{}] 주문 (주문가 {} | 주문량 {})",
                 lit_exit->GetEntryName(), lit_exit->GetEntryOrderPrice(),
                 lit_exit->GetEntryOrderSize()),
          __FILE__, __LINE__);
    }
  }

  // Order Time이 설정됐으면 지정가 주문 완료 -> 체결 확인
  // 터치 후 바로 진입될 수도 있으므로 Order Time을 다시 불러와서 확인
  if (lit_exit->GetExitOrderTime() != -1) {
    if (const auto order_price = lit_exit->GetExitOrderPrice();
        IsLimitPriceSatisfied(lit_exit->GetExitDirection(), price,
                              order_price)) {
      try {
        // 시가에서 터치 조건 달성 시 체결 가격은 시가가 됨 (갭 고려)
        return FillPendingExitOrder(symbol_idx, order_idx,
                                    price_type == OPEN ? price : order_price);
      } catch ([[maybe_unused]] const OrderFailed& e) {
        // 청산 주문은 파산이 아닌 이상 체결되므로 엔진 오류
        LogFormattedInfo(
            ERROR_L,
            format("엔진 오류: LIT [{}] 체결 실패", lit_exit->GetExitName()),
            __FILE__, __LINE__);
        throw;
      }
    }
  }

  return -1;
}

int OrderHandler::CheckPendingTrailingExits(const int symbol_idx,
                                            const long long order_idx,
                                            const double price,
                                            const PriceType price_type) {
  const auto trailing_exit = pending_exits_[symbol_idx][order_idx];

  // Extreme Price가 지정되지 않았으면 추적 미시작 -> 터치 확인
  if (isnan(trailing_exit->GetExitExtremePrice())) {
    if (IsPriceTouched(trailing_exit->GetExitTouchDirection(), price,
                       trailing_exit->GetExitTouchPrice())) {
      trailing_exit->SetExitExtremePrice(price);
    }
  }

  try {
    // Extreme Price가 지정되었으면 추적 시작
    // -> 고저가 업데이트 및 체결 터치 확인
    if (double extreme_price = trailing_exit->GetExitExtremePrice();
        !isnan(extreme_price)) {
      // 주문 정보 로딩
      const auto exit_direction = trailing_exit->GetExitDirection();
      const auto trail_point = trailing_exit->GetExitTrailPoint();

      if (exit_direction == LONG) {
        // 청산 방향이 매수인 경우, 최저가를 추적
        if (IsLess(price, extreme_price)) {
          trailing_exit->SetExitExtremePrice(price);
          extreme_price = price;
        }

        // 청산 방향이 매수인 경우, 최저가로부터 Trail Point 증가 시 진입
        if (const double trail_price = extreme_price + trail_point;
            IsGreaterOrEqual(price, trail_price)) {
          // 시가에서 조건 달성 시 주문 가격은 시가가 됨 (갭 고려)
          return FillPendingExitOrder(symbol_idx, order_idx,
                                      price_type == OPEN ? price : trail_price);
        }
      } else if (exit_direction == SHORT) {
        // 청산 방향이 매도인 경우, 최고가를 추적
        if (IsGreater(price, extreme_price)) {
          trailing_exit->SetExitExtremePrice(price);
          extreme_price = price;
        }

        // 진입 방향이 매도인 경우, 최고가로부터 Trail Point 감소 시 진입
        if (const double trail_price = extreme_price - trail_point;
            IsLessOrEqual(price, trail_price)) {
          // 시가에서 조건 달성 시 주문 가격은 시가가 됨 (갭 고려)
          return FillPendingExitOrder(symbol_idx, order_idx,
                                      price_type == OPEN ? price : trail_price);
        }
      }
    }
  } catch ([[maybe_unused]] const OrderFailed& e) {
    // 청산 주문은 파산이 아닌 이상 체결되므로 엔진 오류
    LogFormattedInfo(WARNING_L,
                     format("엔진 오류: 트레일링 [{}] 체결 실패",
                            trailing_exit->GetExitName()),
                     __FILE__, __LINE__);
    throw;
  }

  return -1;
}

int OrderHandler::FillPendingExitOrder(const int symbol_idx,
                                       const long long order_idx,
                                       const double current_price) {
  // 바 정보 로딩
  const auto current_open_time = engine_->GetCurrentOpenTime();

  // 대기 주문을 받아오고 대기 주문에서 삭제
  auto& pending_exits = pending_exits_[symbol_idx];
  const auto exit_order = pending_exits[order_idx];
  pending_exits.erase(pending_exits.begin() + order_idx);

  // 주문 정보 로딩
  const auto& target_entry_name = exit_order->GetEntryName();
  const auto order_type = exit_order->GetExitOrderType();
  const auto exit_direction = exit_order->GetExitDirection();

  // 원본 진입 주문 찾기
  shared_ptr<Order> entry_order;
  int entry_order_idx;
  try {
    const auto [order, index] = FindEntryOrder(target_entry_name);
    entry_order = order;
    entry_order_idx = index;
  } catch ([[maybe_unused]] const EntryOrderNotFound& e) {
    // 대기 주문 시 이미 진입 주문을 찾았으므로 원본 진입은 무조건 존재
    LogFormattedInfo(
        ERROR_L,
        format("{} [{}] 주문 체결 실패 (원본 진입 주문 [{}] 미존재)",
               Order::OrderTypeToString(exit_order->GetExitOrderType()),
               exit_order->GetExitName(), exit_order->GetEntryName()),
        __FILE__, __LINE__);
    throw OrderFailed("엔진 오류: 청산 시 원본 진입 주문 미존재");
  }

  // 총 청산 체결 수량이 진입 체결 수량보다 크지 않게 조정
  const double exit_filled_size =
      GetAdjustedExitSize(exit_order->GetExitOrderSize(), entry_order);

  // 주문 업데이트
  if (order_type == MIT || order_type == TRAILING) {
    exit_order->SetExitOrderTime(current_open_time)
        .SetExitOrderPrice(current_price);
  }

  exit_order->SetExitFilledTime(current_open_time)
      .SetExitFilledSize(exit_filled_size);

  // 슬리피지가 포함된 체결가
  const double slippage_filled_price =
      CalculateSlippagePrice(order_type, exit_direction, current_price);
  exit_order->SetExitFilledPrice(slippage_filled_price);

  // 수수료
  exit_order->SetExitFee(
      CalculateTradingFee(order_type, slippage_filled_price, exit_filled_size));

  // 원본 진입 주문에 청산 체결 수량 추가
  const double total_exit_filled_size =
      entry_order->GetExitFilledSize() + exit_filled_size;
  entry_order->SetExitFilledSize(total_exit_filled_size);

  // 원본 진입 주문의 청산 체결 수량이 진입 체결 수량과 같으면
  // filled_entries에서 삭제
  int deleted_below_count = 0;
  if (IsEqual(total_exit_filled_size, entry_order->GetEntryFilledSize())) {
    auto& filled_entries = filled_entries_[symbol_idx];
    filled_entries.erase(filled_entries.begin() + entry_order_idx);

    // 같은 진입 이름을 목표로 하는 청산 대기 주문 취소
    for (int pending_order_idx = static_cast<int>(pending_exits.size()) - 1;
         pending_order_idx >= 0; pending_order_idx--) {
      if (const auto pending_exit = pending_exits[pending_order_idx];
          target_entry_name == pending_exit->GetEntryName()) {
        Cancel(pending_exit->GetExitName());

        /* 청산 대기 주문 인덱스가 함수를 호출한 order_idx보다 작을 때만
           deleted_count를 증가.
           같거나 클 때 증가시키면 CheckPendingExits 함수에서 order_idx보다
           작은 인덱스의 청산 체결 체크가 누락됨 */
        if (pending_order_idx < order_idx) {
          deleted_below_count++;
        }
      }
    }
  }

  // 자금, 통계 업데이트
  ExecuteExit(exit_order);

  return deleted_below_count;
}

pair<shared_ptr<Order>, int> OrderHandler::FindEntryOrder(
    const string& target_name) const {
  // 현재 심볼의 체결된 진입들 순회
  const auto& filled_entries = filled_entries_[bar_->GetCurrentSymbolIndex()];
  for (int order_idx = 0; order_idx < filled_entries.size(); order_idx++) {
    // target_entry_name과 같은 이름의 진입이 있으면 반환
    if (const auto filled_entry = filled_entries[order_idx];
        filled_entry->GetEntryName() == target_name) {
      return {filled_entry, order_idx};
    }
  }

  // 원본 진입 주문을 찾지 못하면 청산 실패
  throw EntryOrderNotFound(
      format("원본 진입 주문 [{}] 미존재로 청산 불가", target_name));
}

double OrderHandler::GetAdjustedExitSize(const double exit_size,
                                         const shared_ptr<Order>& entry_order) {
  const auto entry_filled_size = entry_order->GetEntryFilledSize();
  const auto exit_filled_size = entry_order->GetExitFilledSize();

  // 청산 수량 + 분할 청산한 수량이 진입 수량보다 많다면
  if (const auto total_exit_size = exit_size + exit_filled_size;
      IsGreater(total_exit_size, entry_filled_size)) {
    // 청산 가능한 최대값으로 조정하여 반환
    return entry_filled_size - exit_filled_size;
  }

  return exit_size;
}

void OrderHandler::AddTrade(const shared_ptr<Order>& exit_order,
                            const double realized_pnl) const {
  // 중복 사용 변수 로딩
  const auto qty_step =
      symbol_info_[bar_->GetCurrentSymbolIndex()].GetQtyStep();
  const int64_t entry_time = exit_order->GetEntryFilledTime();
  const int64_t exit_time = exit_order->GetExitFilledTime();
  const auto entry_filled_size = exit_order->GetEntryFilledSize();
  const auto exit_filled_size = exit_order->GetExitFilledSize();

  // 진입 수량 대비 청산 수량만큼 진입 수수료를 배분
  // 배분하는 이유는, 첫 분할 청산에서 모든 진입 수수료를 감당하면
  // 손익률이 왜곡되기 때문
  const auto entry_fee =
      exit_filled_size / entry_filled_size * exit_order->GetEntryFee();
  const auto exit_fee = exit_order->GetExitFee();
  const auto liquidation_fee = exit_order->GetLiquidationFee();
  const auto received_funding_amount = exit_order->GetReceivedFundingAmount();
  const auto received_funding_count = exit_order->GetReceivedFundingCount();
  const auto paid_funding_amount = exit_order->GetPaidFundingAmount();
  const auto paid_funding_count = exit_order->GetPaidFundingCount();
  const auto total_funding_amount =
      received_funding_amount + paid_funding_amount;
  const double realized_net_pnl =  // 펀딩비를 정산하고 진입, 청산, 강제 청산
                                   // 수수료를 제외한 순손익
      realized_pnl + total_funding_amount - entry_fee - exit_fee -
      liquidation_fee;

  const auto current_wallet_balance = engine_->GetWalletBalance();
  const auto initial_balance = config_->GetInitialBalance();
  const auto cum_pnl = current_wallet_balance - initial_balance;

  // 동시 보유 심볼 개수 카운트
  int symbol_count = 0;

  // 모든 심볼의 체결된 진입 순회
  for (const auto& filled_entry : filled_entries_) {
    // 해당 심볼 덱에 체결된 진입이 있다면 심볼 개수 추가
    if (!filled_entry.empty()) {
      symbol_count++;
    }
  }

  // 거래 내역에 거래 추가
  analyzer_->AddTrade(
      Trade()
          .SetSymbolName(bar_->GetBarData(TRADING, "NONE")
                             ->GetSymbolName(bar_->GetCurrentSymbolIndex()))
          .SetEntryName(exit_order->GetEntryName())
          .SetExitName(exit_order->GetExitName())
          .SetEntryDirection(exit_order->GetEntryDirection() == LONG ? "매수"
                                                                     : "매도")
          .SetEntryTime(UtcTimestampToUtcDatetime(entry_time))
          .SetExitTime(UtcTimestampToUtcDatetime(exit_time))
          // 단순한 차를 쓰는 이유는 체결 시간의 정확성을 보장할 수 없으므로
          // 평균값을 사용하기 때문. 예를 들면, 1시 ~ 4시 주문일 시
          // 평균 1시 30분 진입 ~ 평균 4시 30분 청산 => 3시간 보유
          .SetHoldingTime(FormatTimeDiff(exit_time - entry_time))
          .SetLeverage(exit_order->GetLeverage())
          .SetEntryPrice(exit_order->GetEntryFilledPrice())
          .SetEntrySize(  // 부동 소수점 오류 방지를 위해 반올림
              RoundToTickSize(exit_order->GetEntryFilledSize(), qty_step))
          .SetExitPrice(exit_order->GetExitFilledPrice())
          .SetExitSize(  // 부동 소수점 오류 방지를 위해 반올림
              RoundToTickSize(exit_order->GetExitFilledSize(), qty_step))
          .SetLiquidationPrice(exit_order->GetLiquidationPrice())
          .SetReceivedFundingCount(received_funding_count)
          .SetReceivedFundingAmount(received_funding_amount)
          .SetPaidFundingCount(paid_funding_count)
          .SetPaidFundingAmount(paid_funding_amount)
          .SetTotalFundingCount(received_funding_count + paid_funding_count)
          .SetTotalFundingAmount(total_funding_amount)
          .SetEntryFee(entry_fee)
          .SetExitFee(exit_fee)
          .SetLiquidationFee(liquidation_fee)
          .SetPnl(realized_pnl)
          .SetNetPnl(realized_net_pnl)
          .SetIndividualPnlPer(realized_net_pnl / exit_order->GetEntryMargin() *
                               100)
          .SetTotalPnlPer(realized_net_pnl / exit_order->GetWbWhenEntryOrder() *
                          100)
          .SetWalletBalance(current_wallet_balance)
          .SetMaxWalletBalance(engine_->GetMaxWalletBalance())
          .SetDrawdown(engine_->GetDrawdown())
          .SetMaxDrawdown(engine_->GetMaxDrawdown())
          .SetCumPnl(cum_pnl)
          .SetCumPnlPer(cum_pnl / initial_balance * 100)
          .SetSymbolCount(symbol_count),
      exit_order->GetExitCount());
}

}  // namespace backtesting::order