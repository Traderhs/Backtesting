// 파일 헤더
#include "Strategies/DiceSystem.hpp"

// 내부 헤더
#include "Engines/SymbolInfo.hpp"

// TODO 각 지표들 plot해서 제대로 계산되나 체크
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
      cached_current_position_size_(0.0),
      cached_wallet_balance_(0.0),
      max_risk_amount_per_trade_(50000),
      max_risk_ratio_per_trade_(0.02),
      balance_ratio_per_symbol_(0.09),
      long_stop_ratio_(0.25),
      short_stop_ratio_(0.25) {}
DiceSystem::~DiceSystem() = default;

void DiceSystem::Initialize() {
  // TODO 가장 큰 문제: 같은 봉에서 여러 주문이 체결되는 경우 체결 순서에 맞는
  // 시뮬레이션이 되지 않고, 그저 대기 주문에 들어간 순서로 체결되고 있음
  const auto num_symbols = bar->GetBarData(TRADING)->GetNumSymbols();
  for (int symbol_idx = 0; symbol_idx < num_symbols; symbol_idx++) {
    symbol_info_.push_back(engine->GetSymbolInfo(symbol_idx));
  }

  cached_wallet_balance_ = engine->GetWalletBalance();
}

void DiceSystem::ExecuteOnClose() {
  // TODO 빅 펀딩비가 사용 가능 자금 다 갉아먹을 때가 문제임
  const auto swing_high = swing_high_[0];
  const auto swing_low = swing_low_[0];

  if (static_cast<bool>(eod_[0])) {
    cached_current_position_size_ = order->GetCurrentPositionSize();

    // 지갑 자금의 90% 미만을 마진으로 사용하고 있을 경우에만 진입 가능
    if (cached_wallet_balance_ = engine->GetWalletBalance(); IsGreaterOrEqual(
            engine->GetAvailableBalance(), cached_wallet_balance_ * 0.1)) {
      // 매수 진입 중이 아니고 장 마감 시 일일 고점이 스윙 하이보다 낮으면
      // 신규 매수 진입 주문 or 매수 진입 주문 수정
      if (IsLessOrEqual(cached_current_position_size_, 0.0)) {
        if (daily_high_[0] < swing_high) {
          // TODO 손절 포인트가 너무 작으면 진입하지 않기: atr 기반으로 할것
          // TODO 추가 안하면 계산된 진입 마진 [$273,071.51]가 심볼당 할당 마진
          // [$21,931.95]를 초과하므로 포지션 크기 [125325.4] → [10065.6]로 조정
          // 과 같은 현상 발생. 매수 매도 둘 다 추가해야 함

          const string& entry_name = "매수 진입";
          const auto& [position_size, leverage] =
              CalculatePositionSizeAndLeverage(
                  fabs(swing_high - swing_low) * long_stop_ratio_, swing_high,
                  Direction::LONG, entry_name);

          // 계산된 포지션 크기가 유의미한 값이면 신규 진입 주문 or 주문 수정
          if (!IsEqual(position_size, 0)) {
            order->MitEntry(entry_name, Direction::LONG, swing_high,
                            position_size, leverage);
          } else {
            // 문제가 있다면 기존 진입 주문 취소
            order->Cancel(entry_name, CancelType::ENTRY,
                          "업데이트된 포지션 크기 및 레버리지 계산 불가");
          }
        } else {
          // 당일 고점이 스윙 상단보다 높았다면 매수 주문 취소
          order->Cancel("매수 진입", CancelType::ENTRY,
                        "일일 고점이 스윙 상단 초과");
        }
      } else {
        // 매수 진입 중이면 매수 주문 취소
        order->Cancel("매수 진입", CancelType::ENTRY, "매수 진입 주문 존재");
      }

      // 매도 진입 중이 아니고 장 마감 시 일일 저점이 스윙 로우보다 높으면
      // 신규 매도 진입 주문 or 매도 진입 주문 수정
      if (IsGreaterOrEqual(cached_current_position_size_, 0.0)) {
        if (daily_low_[0] > swing_low) {
          const string& entry_name = "매도 진입";
          const auto& [position_size, leverage] =
              CalculatePositionSizeAndLeverage(
                  fabs(swing_high - swing_low) * short_stop_ratio_, swing_low,
                  Direction::SHORT, entry_name);

          // 계산된 포지션 크기가 유의미한 값이면 신규 진입 주문 or 주문 수정
          if (!IsEqual(position_size, 0)) {
            order->MitEntry(entry_name, Direction::SHORT, swing_low,
                            position_size, leverage);
          } else {
            // 문제가 있다면 기존 진입 주문 취소
            order->Cancel(entry_name, CancelType::ENTRY,
                          "업데이트된 포지션 크기 및 레버리지 계산 불가");
          }
        } else {
          // 당일 저점이 스윙 하단보다 낮았다면 매도 주문 취소
          order->Cancel("매도 진입", CancelType::ENTRY,
                        "일일 저점이 스윙 하단 미만");
        }
      } else {
        // 매도 진입 중이면 매수 주문 취소
        order->Cancel("매도 진입", CancelType::ENTRY, "매도 진입 주문 존재");
      }

    } else {
      order->Cancel("매수 진입", CancelType::ENTRY,
                    "지갑 자금의 90% 이상을 마진으로 사용 중");
      order->Cancel("매도 진입", CancelType::EXIT,
                    "지갑 자금의 90% 이상을 마진으로 사용 중");
    }

    // 장 마감 시 스윙 로우의 값이 달라졌다면 매수 청산 주문 갱신
    if (IsGreater(cached_current_position_size_, 0.0) &&
        swing_low != swing_low_[1]) {
      // TODO 만약 종가가 swing low보다 낮으면 시장가 청산 (채널 p가 값자기
      // 변하면 그럴 수 있음)
      order->MitExit("매수 청산", "매수 진입", swing_low, left_size);
      return;
    }

    // 장 마감 시 스윙 하이의 값이 달라졌다면 매도 청산 주문 갱신
    if (IsLess(cached_current_position_size_, 0.0) &&
        swing_high != swing_high_[1]) {
      // TODO 만약 종가가 swing high보다 높으면 시장가 청산 (채널 p가 값자기
      // 변하면 그럴 수 있음)
      order->MitExit("매도 청산", "매도 진입", swing_high, left_size);
    }
  }
}

void DiceSystem::ExecuteAfterEntry() {
  cached_current_position_size_ = order->GetCurrentPositionSize();

  // After Entry 전략에서는 봉 완성 전일 수 있으므로 1봉 전 가격을 사용해야 함
  // 추후 EOD에서 갱신됨
  const double swing_high = swing_high_[1];
  const double swing_low = swing_low_[1];

  // 진입 직후 초기 청산 및 손절 주문 (손절 가격은 계속 유지됨)
  if (IsGreater(cached_current_position_size_, 0.0)) {
    order->MitExit("매수 청산", "매수 진입", swing_low, left_size);
    order->MitExit("매수 손절", "매수 진입",
                   swing_high - fabs(swing_high - swing_low) * long_stop_ratio_,
                   left_size);
    return;
  }

  if (IsLess(cached_current_position_size_, 0)) {
    order->MitExit("매도 청산", "매도 진입", swing_high, left_size);
    order->MitExit("매도 손절", "매도 진입",
                   swing_low + fabs(swing_high - swing_low) * short_stop_ratio_,
                   left_size);
  }
}

void DiceSystem::ExecuteAfterExit() {}

pair<double, int> DiceSystem::CalculatePositionSizeAndLeverage(
    double stop_loss_points, double order_price,
    const Direction entry_direction, const string& entry_name) {
  /* TODO 범용으로 만들때에는, 마진에 UnrealizedLoss 반영,
          거래소 포지션 크기 테스트 시 진입 타입 반영하여 로직 및 로그 수정 */

  if (isnan(stop_loss_points)) {
    OrderHandler::LogFormattedInfo(WARN_L,
                                   format("[{}] 스톱 로스 포인트가 NaN이므로 "
                                          "포지션 크기와 레버리지 계산 불가",
                                          entry_name),
                                   __FILE__, __LINE__);
    return make_pair(0, 0);
  }

  if (isnan(order_price)) {
    OrderHandler::LogFormattedInfo(
        WARN_L,
        format("[{}] 진입 가격이 NaN이므로 포지션 크기와 레버리지 계산 불가",
               entry_name),
        __FILE__, __LINE__);
    return make_pair(0, 0);
  }

  if (IsEqual(stop_loss_points, 0.0)) {
    OrderHandler::LogFormattedInfo(
        WARN_L,
        format(
            "[{}] 스톱 로스 포인트가 0이므로 포지션 크기와 레버리지 계산 불가",
            entry_name),
        __FILE__, __LINE__);
    return make_pair(0, 0);
  }

  if (IsEqual(order_price, 0.0)) {
    OrderHandler::LogFormattedInfo(
        WARN_L,
        format("[{}] 진입 가격이 0이므로 포지션 크기와 레버리지 계산 불가",
               entry_name),
        __FILE__, __LINE__);
    return make_pair(0, 0);
  }

  // 심볼 정보 로딩
  const auto symbol_idx = bar->GetCurrentSymbolIndex();
  auto& symbol_info = symbol_info_[symbol_idx];
  const auto price_step = symbol_info.GetPriceStep();
  const auto price_precision = symbol_info.GetPricePrecision();
  const auto qty_step = symbol_info.GetQtyStep();
  const auto qty_precision = symbol_info.GetQtyPrecision();

  // 지갑 자금 = 13,800 USDT, 거래당 리스크 = 2%, 심볼당 할당 자금 = 5%,
  // 진입가 52,300 USDT, 손절 포인트 = 2,500 USDT

  // 1. 손절 시 리스크 비율 계산
  // 100,000 USDT / 5,000,000 USDT → 2%
  // 100,000 USDT / 10,000,000 → 0.5%
  const double risk_ratio_per_trade =
      min(max_risk_ratio_per_trade_,
          max_risk_amount_per_trade_ / cached_wallet_balance_);

  // 2. 손절 시 허용 손실 금액
  // 13,800 USDT × 2% = 276 USDT (총 자금의 2%만 잃을 수 있음 = 276 USDT)
  const double allowed_loss = cached_wallet_balance_ * risk_ratio_per_trade;

  // 3. 심볼당 할당 가능한 마진
  // 13,800 USDT × 5% = 690 USDT (이 심볼에 할당 가능한 마진 = 690 USDT)
  const double available_margin =
      cached_wallet_balance_ * balance_ratio_per_symbol_;

  // 4. 허용 손실 기반 포지션 크기 계산
  // 276 USDT / 2,500 USDT = 0.1104 BTC → 0.110 BTC
  // (0.110 BTC 가격에 진입하면 손절 시 276 USDT에 근접한 손실
  //  → 0.110 BTC * 2,500 USDT = 275 USDT < 276 USDT)
  stop_loss_points = RoundToStep(stop_loss_points, price_step);
  double position_size = RoundToStep(allowed_loss / stop_loss_points, qty_step);

  // 5. 명목 가치 계산
  // 0.11 BTC × 52,300 USDT = 5,753 USDT
  order_price = RoundToStep(order_price, price_step);
  double notional_value = position_size * order_price;

  // 6. 레버리지 계산 (명목 가치를 심볼당 할당 가능한 마진으로 나눈 값)
  //    → 명목 가치보다 부족한 마진을 레버리지로 채우는 것
  //    → 심볼당 할당 가능 마진보다 적게 마진을 할당해야 하므로
  //      확실한 레버리지를 얻기 위해 ceil을 사용
  // ceil(5,753 USDT / 690 USDT) = 8.3376...x → 9x 레버리지
  int leverage = ceil(notional_value / available_margin);
  if (leverage < 1) {
    OrderHandler::LogFormattedInfo(
        WARN_L,
        format(
            "[{}] 계산된 레버리지 [{}x]이(가) 1 미만이므로 [{}x] → [1x]로 조정",
            entry_name, leverage, leverage),
        __FILE__, __LINE__);

    leverage = 1;
  }

  // 7. 레버리지 구간을 통한 최대 레버리지 검증
  const auto& leverage_brackets = symbol_info.GetLeverageBracket();
  int max_allowed_leverage = 1;

  for (const auto& bracket : leverage_brackets) {
    if (notional_value >= bracket.min_notional_value &&
        notional_value <= bracket.max_notional_value) {
      max_allowed_leverage = bracket.max_leverage;
      break;
    }
  }

  // 8. 거래소 레버리지 제한 적용
  // min(9x, 125x) = 9x → 만약 계산된 레버리지가 142x고,
  // 해당 명목 가치 구간의 최대 레버리지가 125x였다면 125x로 제한됨
  leverage = min(leverage, max_allowed_leverage);

  // 9. 진입 시 필요 마진 계산
  // 원래는 CalculateMargin 함수를 사용해야 하나, 이 전략은 한 심볼에서
  // 분할 진입하지 않으므로 단순히 명목 가치를 레버리지로 나누어 마진 계산 가능
  // 5,753 USDT / 9 = 639.22... USDT
  double entry_margin = notional_value / leverage;

  // 10. 진입 마진이 심볼당 실제 할당 가능한 마진보다 많다면 포지션 사이즈 감소
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
  if (IsGreater(entry_margin, available_margin)) {
    const auto pre_position_size = position_size;
    const auto pre_notional_value = notional_value;
    const auto pre_entry_margin = entry_margin;

    position_size = RoundToStep(
        position_size * (available_margin / entry_margin), qty_step);
    notional_value = position_size * order_price;
    entry_margin = notional_value / leverage;

    OrderHandler::LogFormattedInfo(
        WARN_L,
        format(
            "[{}] 계산된 진입 마진 [{}]가 심볼당 할당 마진 [{}]를 초과하므로 "
            "포지션 크기 [{}] → [{}]로 조정 (명목 가치 [{}] → [{}] | 진입 "
            "마진 [{}] → [{}] | 레버리지 [{}x])",
            entry_name, FormatDollar(pre_entry_margin, true),
            FormatDollar(available_margin, true),
            ToFixedString(pre_position_size, qty_precision),
            ToFixedString(position_size, qty_precision),
            FormatDollar(pre_notional_value, true),
            FormatDollar(notional_value, true),
            FormatDollar(pre_entry_margin, true),
            FormatDollar(entry_margin, true), leverage),
        __FILE__, __LINE__);
  }

  // 11. 강제 청산가 계산
  // 롱 → 46,684 USDT
  // 숏 → 57,884 USDT
  double liquidation_price = OrderHandler::CalculateLiquidationPrice(
      entry_direction, order_price, position_size, entry_margin, symbol_idx);

  // 12. 손절가 계산
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
  if (const bool is_stop_safe =
          entry_direction == Direction::LONG
              ? IsGreater(stop_loss_price, liquidation_price)
              : IsLess(stop_loss_price, liquidation_price);
      !is_stop_safe) {
    // 손절 가격보다 먼 청산 가격으로 계산되는 레버리지 찾기
    // 보통은 약간만 조절하면 되기 때문에 성능을 위해 역순 순회
    int safe_leverage = 0;
    double test_liquidation_price = 0;
    for (int test_leverage = leverage; test_leverage >= 1; test_leverage--) {
      test_liquidation_price = OrderHandler::CalculateLiquidationPrice(
          entry_direction, order_price, position_size,
          notional_value / test_leverage, symbol_idx);

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
              ? IsLess(test_liquidation_price, stop_loss_price)
              : IsGreater(test_liquidation_price, stop_loss_price)) {
        safe_leverage = test_leverage;
        break;
      }
    }

    // 안전한 레버리지를 찾지 못한 경우 진입 취소
    if (safe_leverage == 0) {
      OrderHandler::LogFormattedInfo(
          WARN_L,
          format(
              "[{}] 1x 레버리지에서도 안전한 청산가를 확보할 수 없어 진입 불가 "
              "(진입가 [{}] | 손절가 [{}] | 청산가 [{}] → [{}] | 초기 레버리지 "
              "[{}])",
              entry_name, ToFixedString(order_price, price_precision),
              ToFixedString(stop_loss_price, price_precision),
              ToFixedString(liquidation_price, price_precision),
              ToFixedString(test_liquidation_price, price_precision), leverage),
          __FILE__, __LINE__);
      return make_pair(0, 0);
    }

    const auto pre_liquidation_price = liquidation_price;
    const auto pre_leverage = leverage;
    auto pre_entry_margin = entry_margin;

    liquidation_price = test_liquidation_price;
    leverage = safe_leverage;
    entry_margin = notional_value / leverage;

    OrderHandler::LogFormattedInfo(
        WARN_L,
        format(
            "[{}] 청산가 [{}]이(가) 손절가 [{}]보다 진입가 [{}]에 가까우므로 "
            "레버리지 [{}x] → [{}x]로 조정 (청산가 [{}] → [{}] | 진입 마진 "
            "[{}] → [{}])",
            entry_name, ToFixedString(pre_liquidation_price, price_precision),
            ToFixedString(stop_loss_price, price_precision),
            ToFixedString(order_price, price_precision), pre_leverage, leverage,
            ToFixedString(pre_liquidation_price, price_precision),
            ToFixedString(liquidation_price, price_precision),
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
    if (IsGreater(entry_margin, available_margin)) {
      const auto pre_position_size = position_size;
      const auto pre_notional_value = notional_value;
      pre_entry_margin = entry_margin;

      // 청산가는 추후 사용하지 않으므로 재계산 하지 않음
      position_size = RoundToStep(
          position_size * (available_margin / entry_margin), qty_step);
      notional_value = position_size * order_price;
      entry_margin = notional_value / leverage;

      OrderHandler::LogFormattedInfo(
          WARN_L,
          format(
              "[{}] 조정된 진입 마진 [{}]가 심볼당 할당 마진 [{}]를 초과하므로 "
              "포지션 크기 [{}] → [{}]로 조정 (명목 가치 [{}] → [{}] | 진입 "
              "마진 [{}] → [{}] | 레버리지 [{}x])",
              entry_name, FormatDollar(pre_entry_margin, true),
              FormatDollar(available_margin, true),
              ToFixedString(pre_position_size, qty_precision),
              ToFixedString(position_size, qty_precision),
              FormatDollar(pre_notional_value, true),
              FormatDollar(notional_value, true),
              FormatDollar(pre_entry_margin, true),
              FormatDollar(entry_margin, true), leverage),
          __FILE__, __LINE__);
    }
  }

  // 13. 최소/최대 수량 검증
  // 이 전략에서는 MIT 진입만을 사용하므로 Market으로 검증
  const double min_qty = symbol_info.GetMarketMinQty();
  const double max_qty = symbol_info.GetMarketMaxQty();

  // 0.11 BTC는 0.001 BTC ~ 1000 BTC 범위 내이므로 통과
  if (IsLess(position_size, min_qty)) {
    // 예: 계산된 포지션이 0.0005 BTC < 0.001 BTC (최소 수량) → 진입 불가
    OrderHandler::LogFormattedInfo(
        WARN_L,
        format("[{}] 계산된 포지션 크기 [{}]이(가) 시장가의 최소 포지션 크기 "
               "[{}]보다 적으므로 진입 불가",
               entry_name, ToFixedString(position_size, qty_precision),
               ToFixedString(min_qty, qty_precision)),
        __FILE__, __LINE__);

    return make_pair(0, 0);  // 최소 수량보다 작으면 진입하지 않음
  }

  if (IsGreater(position_size, max_qty)) {
    // 예: 계산된 포지션이 1500 BTC > 1000 BTC(최대 수량) → 1000 BTC로 제한
    OrderHandler::LogFormattedInfo(
        WARN_L,
        format("[{}] 계산된 포지션 크기 [{}]이(가) 시장가의 최대 포지션 크기 "
               "[{}]보다 많으므로 포지션 크기 [{}] → [{}]으로 조정",
               entry_name, ToFixedString(position_size, qty_precision),
               ToFixedString(max_qty, qty_precision),
               ToFixedString(position_size, qty_precision),
               ToFixedString(max_qty, qty_precision)),
        __FILE__, __LINE__);

    // TODO 제한 말고 분할 진입할 것
    position_size = max_qty;  // 최대 수량으로 제한
  }

  // 14. 최소 명목 가치 검증
  // 최소 명목 가치를 만족하지 못하면 진입하지 않음
  // 0.11 BTC × 52,300 USDT = 5,753 USDT ≥ 10 USDT(최소 명목 가치)이므로 통과
  const double final_notional_value = position_size * order_price;
  if (const double min_notional_value = symbol_info.GetMinNotionalValue();
      IsLess(final_notional_value, min_notional_value)) {
    // 예: 0.001 BTC × 5,000 USDT = 5 USDT < 10 USDT(최소 명목 가치) → 진입 불가
    OrderHandler::LogFormattedInfo(
        WARN_L,
        format(
            "[{}] 계산된 명목 가치 [{}]가 해당 심볼의 최소 명목 가치 [{}]보다 "
            "적으므로 진입 불가",
            entry_name, FormatDollar(final_notional_value, true),
            FormatDollar(min_notional_value, true)),
        __FILE__, __LINE__);

    return make_pair(0, 0);
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
