# Backtesting

A Windows-oriented, high-performance multi-symbol portfolio backtesting system.
It combines a C++20 engine built for speed and determinism with a React/TypeScript dashboard (Backboard) that visualizes outputs saved to disk.

---

## Overview

This repository is organized around a simple contract:

1. **Market data in** (Parquet time series under `Data/`)
2. **Simulation in** (C++ engine: vectorized bars, margin/fees/slippage)
3. **Results out** (a timestamped folder under `Results/`)
4. **Visualization** (Backboard reads `Results/<run>/Backboard/*`)

The engine is designed to backtest **multiple symbols in a single run**, while keeping execution rules explicit (bar assumptions, magnifier bars, isolated margin).

---

## Repository Layout

- **C++ Engine (headers):** `Includes/Engines/`, `Includes/Indicators/`, `Includes/Strategies/`
- **C++ Engine (implementation):** `Sources/cpp/Engines/`, `Sources/cpp/Indicators/`, `Sources/cpp/Strategies/`
- **Backboard (Node + React):** `Sources/js/`
  - Entry shim: `Sources/js/launch.js`
  - Server: `Sources/js/server/launch.js` (Express + WebSocket)
- **Market data:** `Data/`
  - `Continuous Klines/` (OHLCV Parquet)
  - `Mark Price Klines/` (mark price Parquet)
  - `Funding Rates/` (JSON)
  - `exchange_info.json`, `leverage_bracket.json`
- **Backtest outputs:** `Results/<YYYYMMDD_HHMMSS>/`

---

## Engine Model (What is Simulated)

- **Bar-driven execution**
  - Core processing is based on OHLC traversal; the exact intrabar ordering is controlled by the engine’s internal price-queue assumptions.
- **Magnifier bars (optional)**
  - When enabled, the engine refines execution inside a trading bar using a smaller timeframe bar stream.
- **Mark price integration**
  - Unrealized PnL and liquidation checks use mark price when available; when mark price data is missing, the engine falls back to market price.
- **Isolated margin entries**
  - Each entry manages its own margin; concurrent entries are restricted to a single direction per symbol (no hedge-style long+short concurrency).
- **Single-strategy constraint per run**
  - The engine runs **one** `Strategy` per backtest execution.
  - Backboard can be used to compare/compose results across multiple independent runs.

---

## Data Contract

### Bar schema

Bars are expected as Parquet with the following logical columns:

- `open_time` (epoch ms)
- `open`
- `high`
- `low`
- `close`
- `volume`
- `close_time` (epoch ms)

### Timeframes

Timeframes follow a compact string convention such as `1m`, `1h`, `1d`.

---

## Output Contract (What Backboard Reads)

Each run creates a timestamped directory:

```
Results/<YYYYMMDD_HHMMSS>/
  Backboard/
    config.json
    trade_list.json
    backtesting.log
    Indicators/
      <IndicatorName>/
        <IndicatorName>.parquet
    Sources/
      <StrategyClass>.cpp
      <StrategyClass>.hpp
      <IndicatorClass>.cpp
      <IndicatorClass>.hpp
  ... (Backboard static assets may also be copied alongside)
```

Notes:

- `Backboard/config.json` is a comprehensive run manifest (symbols, bar coverage, exchange/leverage/funding metadata, engine settings, strategy/indicator descriptors).
- `Backboard/trade_list.json` is exported as UTF-8 with BOM for compatibility.
- `Backboard/Indicators/*` stores indicator time series for plotted (non-OHLCV) indicators.
- `Backboard/Sources/*` stores copies of the strategy/indicator source/header files when paths are available.
- If a local Backboard package is present at `Sources/js/Backboard Package`, it is copied into the run directory; otherwise, the engine can fetch a packaged Backboard from a GitHub release as a fallback.

---

## Extending the System

### Strategies

- Add a new strategy as a class inheriting `Strategy`.
- Source path auto-detection expects matching filenames:
  - `Includes/Strategies/<ClassName>.hpp`
  - `Sources/cpp/Strategies/<ClassName>.cpp`

Execution hooks:

- `Initialize()` (once at engine initialization)
- `ExecuteOnClose()` (runs on each trading bar close across symbols)
- `ExecuteAfterEntry()` (runs immediately after an entry fill for the affected symbol)
- `ExecuteAfterExit()` (runs immediately after an exit fill for the affected symbol; highest priority)

### Indicators

- Implement a class inheriting `Indicator`.
- Non-OHLCV indicators with an active plot configuration are eligible for persistence under `Backboard/Indicators/`.

---

## License

This repository is governed by the terms in the root `LICENSE` file.

In particular, it is provided for personal, educational, and non-commercial use only; commercial use requires prior written permission from the author. Refer to `LICENSE` for the complete terms.

---

## Contact

For commercial licensing inquiries: `dice000908@gmail.com`
