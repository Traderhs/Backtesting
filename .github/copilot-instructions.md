# Backtesting System - Copilot Instructions

## Project Overview
This is a financial backtesting system with a React frontend (TypeScript) and a backend that supports algorithmic trading strategy development. The system is designed to test trading strategies against historical market data for cryptocurrency trading.

## Architecture

### Core Components
- **Frontend (React/TypeScript)**: UI for visualization and interaction with backtest results
  - Located in `Sources/js/`
  - Uses lightweight-charts for financial charting
  - WebSocket for real-time communication with the backend
- **Backend (C++)**: Core backtesting engine 
  - Strategy implementation in C++
  - Historical price data processing
  - Trade execution simulation

### Data Flow
1. Configuration defined in `config.json` specifies symbols, timeframes, and data sources
2. Backend loads historical data from Parquet files (located in `/Data/`)
3. Strategies (like `TestStrategy2`) process data and generate signals
4. Indicators (like `SimpleMovingAverage`) provide analysis functions
5. Results visualized in the frontend dashboard

## Key Conventions

### Strategy Development
- Strategies inherit from the `Strategy` base class
- Must implement: `Initialize()`, `ExecuteOnClose()`, `ExecuteAfterEntry()`, `ExecuteAfterExit()`
- Example: `TestStrategy2` uses moving averages for entry/exit decisions

```cpp
// Strategy implementation pattern
void TestStrategy2::ExecuteOnClose() {
  if (order->current_position_size == 0) {
    if (close[0] > sma1[0] && close[1] < sma1[1]) {
      order->MarketEntry("이평선 매수", LONG, order_size);
      return;
    }
    // ... other conditions
  }
}
```

### Indicator Development
- Indicators inherit from the `Indicator` base class
- Must implement: `Initialize()` and `Calculate()`
- Access via reference variables (e.g., `sma1`, `sma2`)

### Configuration Structure
- Configuration in `config.json` uses Korean language keys
- Symbol configurations include exchange info, leverage brackets, and data paths
- Strategy and indicator configurations specify implementation files and visualization properties

## Development Workflow

### Adding New Strategies
1. Create header (.hpp) and implementation (.cpp) files for strategy
2. Inherit from `Strategy` base class and implement required methods
3. Update `config.json` to reference the new strategy

### Visualizing Results
- Overview tab: Summary statistics of backtest performance
- Performance tab: Detailed metrics and comparisons
- Plot tab: Equity curve, drawdown, and distribution charts
- Chart tab: Price chart with indicators and trade markers
- TradeList tab: Detailed list of executed trades
- Config tab: Configuration details
- Log tab: System logs and debugging information

## Data Structure
- Historical price data stored in Parquet files (fast columnar format)
- Data paths specified in `config.json`
- Multiple timeframes supported (e.g., "1m", "5m", "1h")
- Mark price and trading price data managed separately

## Debugging Tips
- Frontend performance monitoring is enabled in development mode
- FPS and long task detection help identify UI performance issues
- WebSocket connection errors are displayed in the UI via `ServerAlert`

## 반드시 지킬 것
- git 복원 사용 금지
- 빌드 금지 - 사용자에게 맡길 것
- 변경 사항을 주석으로 남기기 금지
- 주석은 기능에 대한 설명만 작성