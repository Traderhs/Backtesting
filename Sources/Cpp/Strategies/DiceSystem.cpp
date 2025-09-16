// 파일 헤더
#include "Strategies/DiceSystem.hpp"

// 내부 헤더
#include "Engines/SymbolInfo.hpp"

DiceSystem::DiceSystem(const string& name)
    : Strategy(name),
      eod_(
          AddIndicator<EndOfDay>("EOD", trading_timeframe, Null(), "23:59:59")),
      daily_high_(AddIndicator<High>("Daily High", "1d", Null())),
      daily_low_(AddIndicator<Low>("Daily Low", "1d", Null())),
      swing_high_(AddIndicator<SwingHigh>(
          "Swing High", "1d",
          Line(Rgba::white, 2, SOLID, SIMPLE, false, 0, true), 1)),
      swing_low_(AddIndicator<SwingLow>(
          "Swing Low", "1d",
          Line(Rgba::white, 2, SOLID, SIMPLE, false, 0, true), 1)),
      current_position_size_(0),
      wallet_balance_(0.0),
      risk_ratio_per_trade_(0.02),
      balance_ratio_per_symbol_(0.05),
      long_stop_ratio_(0.25),
      short_stop_ratio_(0.25) {}
DiceSystem::~DiceSystem() = default;

void DiceSystem::Initialize() {
  const auto num_symbols = bar->GetBarData(TRADING)->GetNumSymbols();
  for (int symbol_idx = 0; symbol_idx < num_symbols; symbol_idx++) {
    symbol_info_.push_back(engine->GetSymbolInfo(symbol_idx));
  }

  wallet_balance_ = engine->GetWalletBalance();
}

void DiceSystem::ExecuteOnClose() {
  const double swing_high = swing_high_[0];
  const double swing_low = swing_low_[0];

  if (static_cast<bool>(eod_[0])) {
    current_position_size_ = order->GetCurrentPositionSize();

    // 지갑 자금의 90% 미만을 마진으로 사용하고 있을 경우에만 진입 가능
    if (wallet_balance_ = engine->GetWalletBalance();
        engine->GetAvailableBalance() >= wallet_balance_ * 0.1) {
      // 장 마감 시 당일 고점이 스윙 하이보다 낮으면
      // 신규 매수 진입 주문 or 매수 진입 주문 수정
      if (current_position_size_ <= 0 && daily_high_[0] < swing_high) {
        const auto [position_size, leverage] = CalculatePositionSizeAndLeverage(
            abs(swing_high - swing_low) * long_stop_ratio_, swing_high,
            Direction::LONG);

        // 계산된 포지션 크기가 유의미한 값이면 신규 진입 주문 or 주문 수정
        if (position_size != 0) {
          order->MitEntry("매수 진입", Direction::LONG, swing_high,
                          position_size, leverage);
        } else {
          // 문제가 있다면 기존 진입 주문 취소
          order->LogFormattedInfo(
              WARNING_L,
              "매수 진입 주문 포지션 크기 및 레버리지 계산 중 문제 발생",
              __FILE__, __LINE__);
          order->Cancel("매수 진입");
        }
      } else {
        // 매수 진입 중이거나 당일 고점이 스윙 상단보다 높았다면 매수 주문 취소
        order->Cancel("매수 진입");
      }

      // 장 마감 시 당일 저점이 스윙 로우보다 높으면
      // 신규 매도 진입 주문 or 매도 진입 주문 수정
      if (current_position_size_ >= 0 && daily_low_[0] > swing_low) {
        const auto [position_size, leverage] = CalculatePositionSizeAndLeverage(
            abs(swing_high - swing_low) * short_stop_ratio_, swing_low,
            Direction::SHORT);

        // 계산된 포지션 크기가 유의미한 값이면 신규 진입 주문 or 주문 수정
        if (position_size != 0) {
          order->MitEntry("매도 진입", Direction::SHORT, swing_low,
                          position_size, leverage);
        } else {
          // 문제가 있다면 기존 진입 주문 취소
          order->LogFormattedInfo(
              WARNING_L,
              "매도 진입 주문 포지션 크기 및 레버리지 계산 중 문제 발생",
              __FILE__, __LINE__);
          order->Cancel("매도 진입");
        }
      } else {
        // 매도 진입 중이거나 당일 저점이 스윙 하단보다 낮았다면 매도 주문 취소
        order->Cancel("매도 진입");
      }

    } else {
      order->LogFormattedInfo(WARNING_L,
                              "지갑 자금의 90% 이상을 마진으로 사용 중이므로 "
                              "모든 진입 대기 주문 취소",
                              __FILE__, __LINE__);
      engine->LogBalance();

      order->Cancel("매수 진입");
      order->Cancel("매도 진입");
    }

    // 장 마감 시 스윙 채널의 값이 달라졌다면 청산 주문 갱신
    if (current_position_size_ > 0 && swing_high != swing_high_[1]) {
      order->MitExit("매수 청산", "매수 진입", swing_low, left_size);
      return;
    }

    if (current_position_size_ < 0 && swing_low != swing_low_[1]) {
      order->MitExit("매도 청산", "매도 진입", swing_high, left_size);
    }
  }
}

void DiceSystem::ExecuteBeforeEntry() {}

void DiceSystem::ExecuteAfterEntry() {
  current_position_size_ = order->GetCurrentPositionSize();

  // After Entry 전략에서는 봉 완성 전일 수 있으므로 1봉 전 가격을 사용해야 함
  // 추후 EOD에서 갱신됨
  const double swing_high = swing_high_[1];
  const double swing_low = swing_low_[1];

  // 진입 직후 초기 청산 및 손절 주문 (손절 가격은 계속 유지됨)
  if (current_position_size_ > 0) {
    order->MitExit("매수 청산", "매수 진입", swing_low, left_size);
    order->MitExit(
        "매수 손절", "매수 진입",
        RoundToStep(swing_high - abs(swing_high - swing_low) * long_stop_ratio_,
                    symbol_info_[bar->GetCurrentSymbolIndex()].GetTickSize()),
        left_size);
    return;
  }

  if (current_position_size_ < 0) {
    order->MitExit("매도 청산", "매도 진입", swing_high, left_size);
    order->MitExit(
        "매도 손절", "매도 진입",
        RoundToStep(swing_low + abs(swing_high - swing_low) * short_stop_ratio_,
                    symbol_info_[bar->GetCurrentSymbolIndex()].GetTickSize()),
        left_size);
  }
}

void DiceSystem::ExecuteBeforeExit() {}

void DiceSystem::ExecuteAfterExit() {}

pair<double, int> DiceSystem::CalculatePositionSizeAndLeverage(
    double stop_loss_points, double order_price,
    const Direction entry_direction) {
  /* TODO 범용으로 만들때에는, 마진에 UnrealizedLoss 반영,
          거래소 포지션 크기 테스트 시 진입 타입 반영하여 로직 및 로그 수정 */

  if (isnan(stop_loss_points) || isnan(order_price) || stop_loss_points == 0 ||
      order_price == 0) {
    return make_pair(0, 0);
  }

  auto& symbol_info = symbol_info_[bar->GetCurrentSymbolIndex()];

  // 지갑 자금 = 13,800 USDT, 거래당 리스크 = 2%, 심볼당 할당 자금 = 5%,
  // 진입가 52,300 USDT, 손절 포인트 = 2,500 USDT

  // 1. 손절 시 허용 손실 금액
  // 13,800 USDT × 2% = 276 USDT (총 자금의 2%만 잃을 수 있음 = 276 USDT)
  const double allowed_loss = wallet_balance_ * risk_ratio_per_trade_;

  // 2. 심볼당 할당 가능한 마진
  // 13,800 USDT × 5% = 690 USDT (이 심볼에 할당 가능한 마진 = 690 USDT)
  const double available_margin = wallet_balance_ * balance_ratio_per_symbol_;

  // 3. 허용 손실 기반 포지션 크기 계산
  // 276 USDT / 2,500 USDT = 0.1104 BTC → 0.110 BTC
  // (0.110 BTC 가격에 진입하면 손절 시 276 USDT에 근접한 손실
  //  → 0.110 BTC * 2,500 USDT = 275 USDT < 276 USDT)
  const auto tick_size = symbol_info.GetTickSize();
  const auto qty_step = symbol_info.GetQtyStep();

  stop_loss_points = RoundToStep(stop_loss_points, tick_size);
  double position_size = RoundToStep(allowed_loss / stop_loss_points, qty_step);

  // 4. 명목 가치 계산
  // 0.11 BTC × 52,300 USDT = 5,753 USDT
  order_price = RoundToStep(order_price, tick_size);
  double notional_value = position_size * order_price;

  // 5. 레버리지 계산 (명목 가치를 심볼당 할당 가능한 마진으로 나눈 값)
  //    → 명목 가치보다 부족한 마진을 레버리지로 채우는 것
  //    → 심볼당 할당 가능 마진보다 적게 마진을 할당해야 하므로
  //      확실한 레버리지를 얻기 위해 ceil을 사용
  // ceil(5,753 USDT / 690 USDT) = 8.3376...x → 9x 레버리지
  int leverage = ceil(notional_value / available_margin);
  if (leverage < 1) {
    order->LogFormattedInfo(
        WARNING_L,
        format("계산된 레버리지 [{}x]이(가) 1 미만이므로 [{}x] → [1x]로 조정",
               leverage, leverage),
        __FILE__, __LINE__);

    leverage = 1;
  }

  // 6. 레버리지 구간을 통한 최대 레버리지 검증
  const auto& leverage_brackets = symbol_info.GetLeverageBracket();
  int max_allowed_leverage = 1;

  for (const auto& bracket : leverage_brackets) {
    if (notional_value >= bracket.min_notional_value &&
        notional_value <= bracket.max_notional_value) {
      max_allowed_leverage = bracket.max_leverage;
      break;
    }
  }

  // 7. 거래소 레버리지 제한 적용
  // min(9x, 125x) = 9x → 만약 계산된 레버리지가 142x고,
  // 해당 명목 가치 구간의 최대 레버리지가 125x였다면 125x로 제한됨
  leverage = min(leverage, max_allowed_leverage);

  // 8. 진입 시 필요 마진 계산
  // 원래는 CalculateMargin 함수를 사용해야 하나, 이 전략은 한 심볼에서
  // 분할 진입하지 않으므로 단순히 명목 가치를 레버리지로 나누어 마진 계산 가능
  // 5,753 USDT / 9 = 639.22... USDT
  double entry_margin = notional_value / leverage;

  // 9. 진입 마진이 심볼당 실제 할당 가능한 마진보다 많다면 포지션 사이즈 감소
  //    (허용 손실 기반 포지션 크기가 허용 손실을 넘는 경우가 많기 때문에
  //     포지션 크기를 약간 감소시켜도 문제되지 않음)
  // 예시는 639.22... USDT < 690 USDT이므로 감소하지 않아도 됨
  // 하지만 720 USDT > 690 USDT인 경우,
  // 0.11 BTC * (690 USDT / 720 USDT) = 0.105416... BTC → 0.105 BTC
  //
  // 포지션 크기 조정 이후의 진입 마진은
  // (0.105 BTC * 52,300 USDT) / 9x = 610.16... USDT < 심볼당 마진 690 USDT
  // 손절 시 손실 금액은
  // 0.105 BTC * 2,500 USDT = 262.5 USDT < 허용 손실 금액 276 USDT
  if (entry_margin > available_margin) {
    const auto pre_position_size = position_size;
    const auto pre_notional_value = notional_value;
    const auto pre_entry_margin = entry_margin;

    position_size = RoundToStep(
        position_size * (available_margin / entry_margin), qty_step);
    notional_value = position_size * order_price;
    entry_margin = notional_value / leverage;

    order->LogFormattedInfo(
        WARNING_L,
        format("계산된 진입 마진 [{}]가 심볼당 할당 마진 [{}]를 초과하므로 "
               "포지션 크기 [{}] → [{}]로 조정 (명목 가치 [{}] → [{}] | 진입 "
               "마진 [{}] → [{}])",
               FormatDollar(entry_margin, true),
               FormatDollar(available_margin, true), pre_position_size,
               position_size, FormatDollar(pre_notional_value, true),
               FormatDollar(notional_value, true),
               FormatDollar(pre_entry_margin, true),
               FormatDollar(entry_margin, true)),
        __FILE__, __LINE__);
  }

  // 10. 강제 청산가 계산
  // 롱 → 46,684 USDT
  // 숏 → 57,884 USDT
  double liquidation_price = OrderHandler::CalculateLiquidationPrice(
      entry_direction, order_price, position_size, entry_margin);

  // 11. 손절가 계산
  // 롱 → 52,300 USDT - 2,500 USDT = 49,800 USDT
  // 숏 → 52,300 USDT + 2,500 USDT = 54,800 USDT
  const double stop_loss_price = entry_direction == Direction::LONG
                                     ? order_price - stop_loss_points
                                     : order_price + stop_loss_points;

  // 손절가가 청산가보다 안전한 위치에 있는지 확인
  // 청산가가 손절가보다 가까우면 레버리지를 줄여서 청산가를 더 멀리 보냄
  //
  // 롱 정상 케이스: 손절가 49,800 USDT > 청산가 46,684 USDT → 안전
  // 롱 위험 케이스: 손절가 49,800 USDT < 예시 청산가 52,000 USDT → 조건 실행
  if (const bool is_stop_safe = entry_direction == Direction::LONG
                                    ? stop_loss_price > liquidation_price
                                    : stop_loss_price < liquidation_price;
      !is_stop_safe) {
    // 손절 가격보다 먼 청산 가격으로 계산되는 레버리지 찾기
    int safe_leverage = 1;
    double test_liquidation_price = 0;
    for (int test_leverage = leverage; test_leverage >= 1; test_leverage--) {
      test_liquidation_price = OrderHandler::CalculateLiquidationPrice(
          entry_direction, order_price, position_size,
          notional_value / test_leverage);

      // 롱 기준 예시 (청산가는 제대로 계산 안 했으므로 선택 로직만 참고 할 것)
      //
      // 9x: 마진 = 312 USDT → 계산된 청산가 = 52,000 USDT
      //     → 위험 (계산된 청산가 52,000 USDT > 손절가 49,800 USDT)
      // 8x: 마진 = 357 USDT → 계산된 청산가 = 50,554 USDT
      //     → 위험 (계산된 청산가 50,554 USDT > 손절가 49,800 USDT)
      // 7x: 마진 = 416 USDT → 계산된 청산가 = 49,133 USDT
      //     → 안전 (계산된 청산가 49,133 USDT < 손절가 49,800 USDT)
      // → 결과: safe_leverage = 7x 선택 (첫 번째 안전한 값에서 즉시 중단)

      // 즉, 새로 계산된 청산 가격이 손절 가격보다 멀어 안전하지만 진입 가격과
      // 최대한 가까운 청산 가격으로 계산되는 레버리지를 찾는 것이 목표
      if (entry_direction == Direction::LONG
              ? test_liquidation_price < stop_loss_price
              : test_liquidation_price > stop_loss_price) {
        safe_leverage = test_leverage;
        break;
      }
    }

    const auto pre_liquidation_price = liquidation_price;
    const auto pre_leverage = leverage;
    auto pre_entry_margin = entry_margin;

    liquidation_price = test_liquidation_price;
    leverage = safe_leverage;
    entry_margin = notional_value / leverage;

    order->LogFormattedInfo(
        WARNING_L,
        format("청산가 [{}]이(가) 손절가 [{}]보다 진입가 [{}]에 가까우므로 "
               "레버리지 [{}x] → [{}x]로 조정 (청산가 [{}] → [{}] | 진입 마진 "
               "[{}] → [{}])",
               pre_liquidation_price, stop_loss_price, order_price,
               pre_leverage, leverage, pre_liquidation_price, liquidation_price,
               FormatDollar(pre_entry_margin, true),
               FormatDollar(entry_margin, true)),

        __FILE__, __LINE__);

    // 레버리지 조정 후 실제 필요한 마진 재검증
    // 예시로, 9x → 7x 조정 시 진입 마진은
    // 5,753 USDT / 7x = 821.85... USDT > 690 USDT이므로,
    // 0.11 BTC * (690 USDT / 821.85... USDT) = 0.0923... BTC → 0.092 BTC
    //
    // 포지션 크기 조정 이후의 진입 마진은
    // (0.092 * 52,300 USDT) / 7x = 687.37 USDT < 심볼당 할당 가능 마진 690 USDT
    // 손절 시 손실 금액은
    // 0.092 BTC * 2,500 USDT = 230 USDT < 허용 손실 금액 276 USDT
    if (entry_margin > available_margin) {
      const auto pre_position_size = position_size;
      const auto pre_notional_value = notional_value;
      pre_entry_margin = entry_margin;

      // 청산가는 추후 사용하지 않으므로 재계산 하지 않음
      position_size = RoundToStep(
          position_size * (available_margin / entry_margin), qty_step);
      notional_value = position_size * order_price;
      entry_margin = notional_value / leverage;

      order->LogFormattedInfo(
          WARNING_L,
          format("조정된 진입 마진 [{}]가 심볼당 할당 마진 [{}]를 초과하므로 "
                 "포지션 크기 [{}] → [{}]로 조정 (명목 가치 [{}] → [{}] | 진입 "
                 "마진 [{}] → [{}])",
                 FormatDollar(entry_margin, true),
                 FormatDollar(available_margin, true), pre_position_size,
                 position_size, FormatDollar(pre_notional_value, true),
                 FormatDollar(notional_value, true),
                 FormatDollar(pre_entry_margin, true),
                 FormatDollar(entry_margin, true)),
          __FILE__, __LINE__);
    }
  }

  // 11. 최소/최대 수량 검증
  // 이 전략에서는 MIT 진입만을 사용하므로 Market으로 검증
  const double min_qty = symbol_info.GetMarketMinQty();
  const double max_qty = symbol_info.GetMarketMaxQty();

  // 0.11 BTC는 0.001 BTC ~ 1000 BTC 범위 내이므로 통과
  if (position_size < min_qty) {
    // 예: 계산된 포지션이 0.0005 BTC < 0.001 BTC (최소 수량) → 진입 불가
    order->LogFormattedInfo(
        WARNING_L,
        format(
            "계산된 포지션 크기 [{}]이(가) 시장가의 최소 포지션 크기 [{}]보다 "
            "적으므로 진입 불가",
            position_size, min_qty),
        __FILE__, __LINE__);

    position_size = 0;  // 최소 수량보다 작으면 진입하지 않음
  } else if (position_size > max_qty) {
    // 예: 계산된 포지션이 1500 BTC > 1000 BTC(최대 수량) → 1000 BTC로 제한
    order->LogFormattedInfo(
        WARNING_L,
        format(
            "계산된 포지션 크기 [{}]이(가) 시장가의 최대 포지션 크기 [{}]보다 "
            "많으므로 포지션 크기 [{}] → [{}]으로 조정",
            position_size, max_qty, position_size, max_qty),
        __FILE__, __LINE__);

    position_size = max_qty;  // 최대 수량으로 제한
  }

  // 12. 최소 명목 가치 검증
  // 최소 명목 가치를 만족하지 못하면 진입하지 않음
  // 0.11 BTC × 52,300 USDT = 5,753 USDT ≥ 10 USDT(최소 명목 가치)이므로 통과
  const double final_notional_value = position_size * order_price;
  if (const double min_notional_value = symbol_info.GetMinNotionalValue();
      final_notional_value < min_notional_value) {
    // 예: 0.001 BTC × 5,000 USDT = 5 USDT < 10 USDT(최소 명목 가치) → 진입 불가
    order->LogFormattedInfo(WARNING_L,
                            format("계산된 명목 가치 [{}]가 해당 심볼의 최소 "
                                   "명목 가치 [{}]보다 적으므로 진입 불가",
                                   FormatDollar(final_notional_value, true),
                                   FormatDollar(min_notional_value, true)),
                            __FILE__, __LINE__);

    position_size = 0;
  }

  // 최종 결과: 0.11 BTC, 9x 레버리지
  //
  // 예상 손실 0.11 BTC * 2,500 USDT = 275 USDT
  //     < 손절 시 허용 손실 금액 13,800 USDT × 2% = 276 USDT
  //
  // 진입 마진 (0.11 BTC * 52,300 USDT) / 9x = 639.2... USDT
  //     < 심볼당 할당한 가능 마진 13,800 USDT × 5% = 690 USDT
  return make_pair(position_size, leverage);
}
