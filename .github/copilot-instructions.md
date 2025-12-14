## 요약
이 파일은 AI 코딩 에이전트가 이 리포지토리에서 즉시 생산적으로 작업할 수 있도록 핵심 정보를 제공합니다. 다중 심볼 포트폴리오 백테스팅 시스템으로, C++ 고성능 엔진과 React/TypeScript 웹 UI로 구성되어 있습니다.

## 한 줄로 보는 큰 그림
- **메인 엔진**: C++20 백테스팅 실행 파일 (`Backtesting.exe`) — 벡터화된 고속 백테스팅, Parquet 기반 데이터 처리
- **웹 UI**: Vite + React + TypeScript 앱 (`Backboard.exe`) — 성과 분석, 차트 시각화, 트레이드 리스트
- **데이터 흐름**: `Data/` (Parquet 시계열) → C++ 엔진 시뮬레이션 → `Results/` (JSON/Parquet) → 웹 UI 시각화
- **핵심 개념**: 격리 마진(Isolated Margin), 돋보기 바(Magnifier), 벡터화 지표 계산, 다중 타임프레임 전략

## 아키텍처 핵심 개념

### C++ 엔진 구조 (Includes/Engines/, Sources/cpp/)
프로젝트는 팩토리 패턴과 싱글톤을 활용한 컴포넌트 기반 아키텍처입니다:

1. **BaseEngine** (`BaseEngine.hpp`): 지갑 자금, 마진, 거래소 정보 관리. 모든 엔진 클래스의 기반.
2. **Engine** (`Engine.hpp`): 백테스팅 메인 루프 실행, 바 핸들러와 오더 핸들러 조율.
3. **Strategy** (`Strategy.hpp`): 사용자 정의 전략의 추상 베이스 클래스.
   - `Initialize()`: 엔진 초기화 시 1회 실행
   - `ExecuteOnClose()`: 트레이딩 바 종가마다 모든 심볼에서 실행
   - `ExecuteAfterEntry()`: 진입 체결 직후 해당 심볼에서 실행
   - `ExecuteAfterExit()`: 청산 체결 직후 해당 심볼에서 실행 (우선순위 최상위)
4. **Indicator** (`Indicator.hpp`): 커스텀 지표의 추상 베이스. 
   - `Initialize()`: 지표 계산 시 1회 실행
   - `Calculate()`: 각 바마다 값 계산 및 반환
   - `operator[]`: 지표 값 참조 (예: `sma[0]`은 현재 바, `sma[1]`은 1봉 전)
5. **OrderHandler** (`OrderHandler.hpp`): 주문 접수, 체결 확인, 포지션 관리.
   - 진입 유형: `MarketEntry`, `LimitEntry`, `MitEntry`, `LitEntry`, `TrailingEntry`
   - 청산 유형: `MarketExit`, `LimitExit`, `StopExit`, `TrailingExit`
   - **격리 마진(Isolated)**: 각 진입은 독립적으로 마진 관리, 단방향 동시 진입만 가능
6. **BarHandler** (`BarHandler.hpp`, `BarData.hpp`): Parquet 데이터 로드 및 벡터화 저장.
   - `Bar` 구조체: `{open_time, open, high, low, close, volume, close_time}`
   - 타임프레임 계층: **돋보기(Magnifier) < 트레이딩(Trading) ≤ 참조(Reference)**
   - 마크 프라이스: 미실현 손익 계산 및 강제 청산 확인에 사용
7. **Config** (`Config.hpp`): 빌더 패턴으로 백테스팅 설정.
   - `SetRootDirectory()`, `SetBacktestPeriod()`, `SetInitialBalance()`, `SetTakerFeePercentage()`, `SetSlippage()` 등
8. **Plot** (`Plot.hpp`): 지표 시각화 설정 (Area, BaseLine, Histogram, Line).
   - `Rgba` 색상 구조체, `LineStyle`, `LineType` 열거형 제공

### 프론트엔드 구조 (Sources/js/)
- **엔트리 포인트**: `launch.js` — Express 서버 + WebSocket 통신 (포트 7777부터 자동 검색)
- **주요 컴포넌트** (`src/components/`):
  - `Overview`: 전략 성과 요약 (총 수익률, 승률, Sharpe Ratio 등)
  - `Performance`: 심볼별/전략별 성과 비교 차트
  - `Plot`: Equity Curve, Drawdown, PnL Distribution, Symbol Performance
  - `Chart`: 캔들차트 + 지표 오버레이 (Plotly/ECharts 기반)
  - `TradeList`: 트레이드 목록 (가상화 테이블)
  - `Config`: 백테스팅 설정 표시
  - `Log`: 백테스팅 진행 로그
- **데이터 로딩**: Parquet 파일은 `@dsnp/parquetjs` / `apache-arrow`로 파싱
- **스타일링**: TailwindCSS v4 + shadcn/ui 컴포넌트
- **라우팅**: React Router v7
- **상태 관리**: Context API (`WebSocketProvider`, `LogoProvider`, `TradeFilterProvider`)

## 주요 디렉터리 및 파일

### 루트 레벨
- `CMakeLists.txt`: CMake 빌드 설정. **vcpkg 툴체인 경로 `D:/vcpkg/...` 하드코딩** — 환경에 맞게 수정 필요.
- `Builds/Debug`, `Builds/Release`: CMake 빌드 출력 디렉터리 (Ninja 사용 권장).

### C++ 코드
- `Includes/Backtesting.hpp`: 메인 API 진입점. `Backtesting::Run()`, `Backtesting::SetConfig()` 등 제공.
- `Includes/Engines/`: 엔진 관련 헤더 (BaseEngine, Engine, Strategy, Indicator, OrderHandler 등).
- `Includes/Indicators/`: 내장 지표 (SMA, EMA, ATR, StandardDeviation, SwingHigh/Low 등).
- `Includes/Strategies/`: 사용자 전략 (TestStrategy, DiceSystem 등).
- `Sources/cpp/Backtesting.cpp`: 실행 파일 엔트리 (`main` 함수).
- `Sources/cpp/Engines/`, `Sources/cpp/Indicators/`, `Sources/cpp/Strategies/`: 각 헤더의 구현 파일.

### 프론트엔드
- `Sources/js/launch.js`: Node.js 백엔드 서버 (Express + WebSocket). Parquet 파일 읽기, 차트 데이터 제공.
- `Sources/js/package.json`: npm 스크립트 — `dev`, `build`, `pkg:build` (Backboard.exe 생성).
- `Sources/js/src/App.tsx`: 메인 React 컴포넌트. 탭 전환, 애니메이션, lazy loading.
- `Sources/js/src/components/`: 각 탭 컴포넌트 (Overview, Performance, Plot, Chart, TradeList, Config, Log).

### 데이터
- `Data/Continuous Klines/`: 연속 선물 캔들 데이터 (Parquet, 심볼별 디렉터리).
- `Data/Mark Price Klines/`: 마크 프라이스 캔들 (미실현 손익 계산용).
- `Data/Funding Rates/`: 펀딩 비율 데이터 (JSON).
- `Data/exchange_info.json`: 바이낸스 거래소 정책 (최소/최대 수량, 틱 사이즈 등).
- `Data/leverage_bracket.json`: 레버리지 구간별 마진 정보.
- `Results/`: 백테스팅 결과 출력 (타임스탬프별 디렉터리, JSON/Parquet).

## 데이터 흐름 & 실행 프로세스

### 1. 데이터 수집 (선택적)
```cpp
// BinanceFetcher를 통한 데이터 다운로드
Backtesting::FetchContinuousKlines("BTCUSDT", "1h");
Backtesting::FetchExchangeInfo();
Backtesting::FetchLeverageBracket();
```
- `BinanceFetcher` (`BinanceFetcher.hpp`): libcurl + OpenSSL로 바이낸스 API 호출, Parquet 저장.

### 2. 백테스팅 설정 및 실행
```cpp
// Sources/cpp/Backtesting.cpp main() 함수 예시
const vector<string>& symbol_list = {"BTCUSDT", "ETHUSDT", "SOLUSDT"};

// 바 데이터 추가 (Trading, Magnifier, Reference, Mark Price 각각)
Backtesting::AddBarData(symbol_list, "1h", "Data/Continuous Klines", TRADING);
Backtesting::AddBarData(symbol_list, "1m", "Data/Continuous Klines", MAGNIFIER);

// 거래소 정책 및 펀딩 비율 로드
Backtesting::AddExchangeInfo("Data/exchange_info.json");
Backtesting::AddFundingRates(symbol_list, "Data/Funding Rates");

// 설정 (빌더 패턴)
Backtesting::SetConfig()
    .SetRootDirectory("D:/Programming/Backtesting")
    .SetBacktestPeriod()  // 생략 시 전체 기간
    .SetUseBarMagnifier(true)  // 돋보기 바 활성화
    .SetInitialBalance(10000)
    .SetTakerFeePercentage(0.045)
    .SetSlippage(MarketImpactSlippage(1.5));

// 전략 추가 (팩토리 패턴)
Backtesting::AddStrategy<DiceSystem>("Dice System");

// 실행
Backtesting::Run();
```

### 3. C++ 엔진 내부 처리
1. **초기화**: `Engine::Initialize()` → 바 데이터 로드, 전략/지표 초기화
2. **메인 루프**: 트레이딩 바를 순회하며:
   - 돋보기 바 활성화 시: 트레이딩 바 내부를 돋보기 바 단위로 세밀하게 시뮬레이션 (체결 정확도 향상)
   - 각 바에서 `Strategy::ExecuteOnClose()` 호출
   - 주문 체결 확인 → `ExecuteAfterEntry()` / `ExecuteAfterExit()` 호출
   - 미실현 손익 업데이트, 강제 청산 확인 (마크 프라이스 기준)
3. **결과 저장**: `Analyzer::SaveResults()` → `Results/{타임스탬프}/` 디렉터리에 JSON/Parquet 출력
   - `config.json`: 백테스팅 설정
   - `equity.parquet`: 자산 곡선
   - `trades.parquet`: 개별 트레이드 상세
   - `indicators.parquet`: 지표 값 (플롯 활성화 시)

### 4. 프론트엔드 시각화
1. **서버 시작**: `node launch.js` → Express (포트 7777~) + WebSocket
2. **클라이언트 접속**: 브라우저 자동 실행 (`open` 패키지), React 앱 로드
3. **데이터 요청**: WebSocket으로 `Results/` 디렉터리 목록 요청 → 사용자가 결과 선택
4. **Parquet 파싱**: `@dsnp/parquetjs` / `apache-arrow`로 `.parquet` 파일 읽기
5. **시각화**: Plotly, ECharts, Chart.js로 차트 렌더링
   - `Plot` 컴포넌트: Equity/Drawdown, PnL Distribution
   - `Chart` 컴포넌트: 캔들차트 + 지표 오버레이
   - `TradeList`: 가상화 테이블 (`react-window`)

### 5. 주요 데이터 스키마
**Bar 구조체** (`BarData.hpp`):
```cpp
struct Bar {
  int64_t open_time;
  double open, high, low, close, volume;
  int64_t close_time;
};
```

**Parquet 출력 예시** (`trades.parquet`):
- `symbol`, `entry_name`, `exit_name`, `direction`, `entry_price`, `exit_price`, `size`, `pnl`, `entry_time`, `exit_time`, `leverage` 등

## 빌드 & 실행 워크플로우

### C++ 백테스팅 엔진 빌드 (Windows, MSVC + vcpkg + Ninja)

#### 사전 요구사항
- **vcpkg**: `D:/vcpkg` 경로에 설치 (또는 `CMakeLists.txt` 수정)
- **필수 패키지**: Arrow, Parquet, OpenSSL, libcurl, nlohmann_json, ZLIB
  ```powershell
  vcpkg install arrow parquet openssl curl nlohmann-json zlib --triplet x64-windows
  ```
- **빌드 도구**: MSVC 2022+, Ninja (또는 MSBuild)

#### 빌드 명령 (PowerShell)
```powershell
# 프로젝트 루트에서
cmake -S . -B Builds/Release -G "Ninja" `
  -DCMAKE_TOOLCHAIN_FILE="D:/vcpkg/scripts/buildsystems/vcpkg.cmake" `
  -DCMAKE_BUILD_TYPE=Release

cmake --build Builds/Release --config Release
```

#### 빌드 출력
- **실행 파일**: `Builds/Release/Release/Backtesting.exe`
- **최적화 플래그** (Release): `/O2 /Oi /Ot /GL /arch:AVX2` (SIMD 벡터화)
- **디버그 빌드**: `Builds/Debug` 디렉터리 사용 (`/Od /Zi /W4`)

#### 디버그 실행
```powershell
# Visual Studio에서 CMake 프로젝트 열기
cmake --open Builds/Release

# 또는 직접 실행
.\Builds\Release\Release\Backtesting.exe
```

#### 프로파일링 (선택적)
`CMakeLists.txt`에서 주석 해제:
```cmake
target_compile_options(Backtesting PRIVATE /Zi /DEBUG:FULL)
target_link_options(Backtesting PRIVATE /PROFILE /DEBUG:FULL)
```
그 후 Visual Studio Profiler 사용.

---

### 프론트엔드 (Vite + React + TypeScript)

#### 개발 서버 실행
```powershell
cd Sources/js
npm install  # 최초 1회
npm run dev  # Vite 개발 서버 (포트 5173)
```

#### 백엔드 서버 (Node.js + Express)
```powershell
node launch.js  # 포트 7777~에서 자동으로 사용 가능한 포트 탐색
```
- **기능**: Parquet 파일 읽기, WebSocket 통신, 브라우저 자동 실행 (`open` 패키지)
- **API 엔드포인트**: `/api/results` (결과 목록), `/api/parquet` (Parquet 데이터)

#### 프로덕션 빌드
```powershell
npm run build  # dist/ 폴더에 정적 파일 생성
```

#### 네이티브 실행 파일 생성 (PKG)
```powershell
npm run pkg:build  # dist/Backboard.exe 생성 (Node.js + 번들 포함)
```
- **타겟**: `node18-win-x64`
- **에셋**: axios, React 빌드 파일 포함

---

### 테스트 & 디버깅

#### C++ 테스트
현재 프로젝트에는 CTest 설정이 없으나, `Tests/` 디렉터리에 테스트 파일 존재:
- `Tests/BinanceDataManagerTest.cpp`: 데이터 페칭 테스트 (수동 실행)

추가 방법:
```cmake
# CMakeLists.txt에 추가
enable_testing()
add_executable(BacktestingTests Tests/BinanceDataManagerTest.cpp)
target_link_libraries(BacktestingTests PRIVATE ...)
add_test(NAME DataManagerTest COMMAND BacktestingTests)
```

#### 디버깅 팁
**C++ 엔진**:
- `Logger::Log()` (`Logger.hpp`): 콘솔/파일 로그 출력
- Visual Studio Debugger: `F5`로 실행, 중단점 설정
- 메모리 프로파일링: `/DEBUG:FULL` + Visual Studio Profiler

**프론트엔드**:
- Chrome DevTools: React DevTools, Network 탭 (WebSocket 통신 확인)
- `console.log()`: `launch.js`에서 서버 로그, 브라우저 콘솔에서 클라이언트 로그
- Hot Reload: Vite는 코드 변경 시 자동 새로고침

#### 일반적인 문제 해결
1. **vcpkg 경로 오류**: `CMakeLists.txt` 3번째 줄 수정
   ```cmake
   set(CMAKE_TOOLCHAIN_FILE "당신의경로/vcpkg/scripts/buildsystems/vcpkg.cmake")
   ```
2. **Parquet 파일 없음**: `Data/` 디렉터리에 `.parquet` 파일 배치 또는 `FetchContinuousKlines()` 실행
3. **포트 충돌**: `launch.js`는 7777부터 자동 증가하며 사용 가능한 포트 탐색
4. **npm 의존성 오류**: `rm -rf node_modules package-lock.json; npm install` 재실행

## 코딩 규약 & 패턴

### C++ 스타일
- **명명 규칙**:
  - 클래스: `PascalCase` (예: `OrderHandler`, `SimpleMovingAverage`)
  - 함수: `PascalCase` (예: `GetWalletBalance()`, `MarketEntry()`)
  - 변수: `snake_case` (예: `wallet_balance_`, `used_margin_`)
  - 멤버 변수: 후행 언더스코어 `_` (예: `strategy_`, `config_`)
  - 상수/열거형: `UPPER_CASE` (예: `TRADING`, `MAGNIFIER`, `SOLID`)
- **파일 구조**: 1파일 1클래스 원칙
  - 헤더: `Includes/{Category}/{ClassName}.hpp`
  - 구현: `Sources/cpp/{Category}/{ClassName}.cpp`
  - 예: `Includes/Indicators/SimpleMovingAverage.hpp` ↔ `Sources/cpp/Indicators/SimpleMovingAverage.cpp`
- **디자인 패턴**:
  - **싱글톤**: `Engine`, `OrderHandler`, `Config` (정적 `GetInstance()` 메서드)
  - **팩토리**: `Strategy::AddStrategy<T>()`, 전략/지표 생성 시 팩토리 함수 사용
  - **빌더**: `Config::SetConfig().SetInitialBalance().SetTakerFee()...`
  - **추상 베이스 클래스**: `Base*` 접두사 (`BaseEngine`, `BaseOrderHandler`, `BaseFetcher`)
- **메모리 관리**: `shared_ptr` 사용 (RAII), `new`/`delete` 직접 사용 금지
- **에러 처리**: `Logger::LogAndThrowError()` — 로그 기록 후 예외 throw
  ```cpp
  if (error_condition) {
    Logger::LogAndThrowError("설명", __FILE__, __LINE__);
  }
  ```

### 전략 작성 패턴 (Strategy)
**기본 구조**:
```cpp
// Includes/Strategies/MyStrategy.hpp
#pragma once
#include "Engines/Strategy.hpp"

class MyStrategy final : public Strategy {
 public:
  explicit MyStrategy(const string& name);
  void Initialize() override;
  void ExecuteOnClose() override;
  void ExecuteAfterEntry() override;
  void ExecuteAfterExit() override;

 private:
  SimpleMovingAverage& sma_;  // 지표 참조
};

// Sources/cpp/Strategies/MyStrategy.cpp
MyStrategy::MyStrategy(const string& name)
    : Strategy(name),
      sma_(AddIndicator<SimpleMovingAverage>("SMA", trading_timeframe, 
                                              Line(Rgba::blue), close, 20)) {
  // trading_timeframe: 자동 제공되는 변수
  // close: 기본 제공 Close 지표
}

void MyStrategy::ExecuteOnClose() {
  if (sma_[0] > sma_[1]) {  // 현재 봉, 1봉 전 참조
    order_handler->MarketEntry("long1", LONG, 0.01, 10);
  }
}
```

**핵심 규칙**:
1. **지표 추가**: 생성자에서 `AddIndicator<T>(이름, 타임프레임, 플롯, 파라미터...)`
2. **지표 참조**: `Indicator&` 타입 멤버 변수에 저장, `operator[]`로 접근
3. **주문 함수**: `order_handler->MarketEntry/LimitEntry/...` (전역 싱글톤 자동 제공)
4. **진입 잔량 참조**: `left_size` 변수 (현재 심볼의 체결된 진입 잔량)
5. **파일명 = 클래스명**: `MyStrategy.hpp` + `MyStrategy.cpp` (자동 탐색)

### 지표 작성 패턴 (Indicator)
**기본 구조**:
```cpp
// Includes/Indicators/MyIndicator.hpp
#pragma once
#include "Engines/Indicator.hpp"

class MyIndicator final : public Indicator {
 public:
  explicit MyIndicator(const string& name, const string& timeframe,
                       const Plot& plot, Indicator& source, double period);

 private:
  Indicator& source_;
  double period_;
  void Initialize() override;
  Numeric<double> Calculate() override;
};

// Sources/cpp/Indicators/MyIndicator.cpp
MyIndicator::MyIndicator(const string& name, const string& timeframe,
                         const Plot& plot, Indicator& source, double period)
    : Indicator(name, timeframe, plot), source_(source), period_(period) {}

void MyIndicator::Initialize() {
  // 최초 1회 실행 (버퍼 초기화 등)
}

Numeric<double> MyIndicator::Calculate() {
  // 각 바마다 실행, 계산된 값 반환
  return source_[0] + period_;  // 예시
}
```

**핵심 규칙**:
1. **생성자 시그니처**: `(이름, 타임프레임, Plot, [커스텀 파라미터...])`
2. **다른 지표 참조**: 생성자 인수로 `Indicator&` 받기 (전략보다 먼저 정의)
3. **타임프레임 일치**: 계산에 사용하는 지표는 동일 타임프레임만 가능
4. **플롯 설정**: `Line(색상, 스타일, 두께)`, `Area(색상)`, `Histogram(색상)`, `BaseLine(색상, 기준값)`

### 프론트엔드 규약 (TypeScript/React)
- **명명**: `PascalCase` (컴포넌트), `camelCase` (함수/변수)
- **파일 구조**:
  - 컴포넌트: `src/components/{Feature}/{ComponentName}.tsx`
  - 훅: `src/hooks/use{HookName}.ts`
  - 유틸: `src/utils/{utilName}.ts`
- **스타일링**: Tailwind CSS 클래스 사용 (인라인), shadcn/ui 컴포넌트 재사용
- **상태 관리**: Context API (`WebSocketProvider`, `TradeFilterProvider`)
- **데이터 페칭**: WebSocket 통신 (REST API는 최소화)
- **성능 최적화**:
  - `lazy()` + `Suspense`: 탭 컴포넌트 코드 스플리팅
  - `react-window` / `react-virtuoso`: 대량 데이터 가상화
  - `useMemo` / `useCallback`: 무거운 계산/함수 메모이제이션

### 공통 관례
- **시간 표현**: GMT 기준, millisecond epoch (`int64_t`)
- **타임프레임 형식**: `"1m"`, `"5m"`, `"1h"`, `"1d"` (소문자 + 단위)
- **심볼 형식**: `"BTCUSDT"`, `"ETHUSDT"` (대문자, USDT 페어)
- **주석**: 한국어 사용 (코드 내 설명), Doxygen 스타일 (함수 문서)
  ```cpp
  /// 지갑 자금을 반환하는 함수
  /// @return 현재 지갑 자금
  [[nodiscard]] double GetWalletBalance() const;
  ```

## 통합 포인트 & 의존성

### C++ ↔ 프론트엔드 데이터 계약
**출력 디렉터리 구조**:
```
Results/
  └── {YYYYMMDD_HHMMSS}/
      ├── config.json          # 백테스팅 설정 (Config 클래스 직렬화)
      ├── equity.parquet       # 시간별 자산 곡선 (wallet_balance, unrealized_pnl)
      ├── trades.parquet       # 개별 트레이드 상세
      └── indicators.parquet   # 지표 값 (플롯 활성화 시)
```

**Parquet 스키마** (`trades.parquet`):
| 컬럼             | 타입     | 설명                          |
|------------------|----------|-------------------------------|
| `symbol`         | `string` | 심볼 이름                     |
| `entry_name`     | `string` | 진입 주문 이름                |
| `exit_name`      | `string` | 청산 주문 이름                |
| `direction`      | `string` | `"LONG"` / `"SHORT"`          |
| `entry_price`    | `double` | 진입 가격                     |
| `exit_price`     | `double` | 청산 가격                     |
| `size`           | `double` | 포지션 크기 (레버리지 미포함) |
| `pnl`            | `double` | 실현 손익                     |
| `entry_time`     | `int64`  | 진입 시각 (밀리초 epoch)      |
| `exit_time`      | `int64`  | 청산 시각 (밀리초 epoch)      |
| `leverage`       | `int`    | 레버리지                      |

**JSON 스키마** (`config.json`):
```json
{
  "root_directory": "D:/Programming/Backtesting",
  "initial_balance": 10000,
  "taker_fee_percentage": 0.045,
  "maker_fee_percentage": 0.018,
  "slippage_type": "MarketImpactSlippage",
  "slippage_value": 1.5,
  "use_bar_magnifier": true,
  "backtest_period": { "start": "...", "end": "...", "format": "..." }
}
```

### 외부 라이브러리
**C++ (vcpkg 관리)**:
- `arrow` / `parquet`: Parquet 파일 읽기/쓰기
- `libcurl`: HTTP 요청 (바이낸스 API)
- `openssl`: HTTPS 통신, HMAC 서명
- `nlohmann_json`: JSON 파싱/직렬화
- `zlib`: 압축 (Parquet 내부 사용)

**JavaScript/TypeScript (npm 관리)**:
- `@dsnp/parquetjs` / `apache-arrow`: Parquet 파일 파싱
- `react-plotly.js`: 인터랙티브 차트 (Equity, Drawdown)
- `echarts-for-react`: 고성능 차트 (캔들차트, 지표)
- `express` / `ws`: 백엔드 서버 + WebSocket
- `tailwindcss` + `@radix-ui`: UI 컴포넌트 스타일링

### 성능 최적화 전략
**C++ 엔진**:
- **벡터화**: `vector<Bar>` 연속 메모리로 캐시 효율성 극대화
- **SIMD**: `/arch:AVX2` 플래그로 지표 계산 가속
- **멀티스레딩 고려**: 현재는 단일 스레드, 향후 심볼별 병렬 처리 가능
- **메모리 풀**: `shared_ptr` 재사용, 객체 재할당 최소화

**프론트엔드**:
- **코드 스플리팅**: `lazy()` + dynamic import (탭별 번들 분리)
- **가상화**: `react-window` (10,000+ 트레이드 렌더링)
- **WebWorker**: Parquet 파싱을 백그라운드 스레드에서 처리 (계획)
- **메모이제이션**: 차트 데이터 변환 결과 캐싱

## 변경 시 체크리스트

### 새 전략 추가
1. `Includes/Strategies/MyStrategy.hpp` + `Sources/cpp/Strategies/MyStrategy.cpp` 생성
2. `Strategy` 클래스 상속, 4개 메서드 구현
3. `Backtesting.cpp`에서 `Backtesting::AddStrategy<MyStrategy>("이름")` 호출
4. 빌드 후 실행 → `Results/` 확인

### 새 지표 추가
1. `Includes/Indicators/MyIndicator.hpp` + `Sources/cpp/Indicators/MyIndicator.cpp` 생성
2. `Indicator` 클래스 상속, `Initialize()` + `Calculate()` 구현
3. 전략에서 `AddIndicator<MyIndicator>(...)` 사용
4. (선택) `Includes/Indicators/Indicators.hpp`에 `#include` 추가

### 데이터 스키마 변경
1. **C++ 출력 변경**: `Analyzer.cpp`의 Parquet Writer 수정
2. **프론트엔드 파싱 변경**: `launch.js`의 Parquet Reader 수정
3. **타입 정의 갱신**: `Sources/js/src/types/*.ts` 수정
4. **컴포넌트 업데이트**: `TradeList`, `Plot` 등에서 새 컬럼 참조

### vcpkg 패키지 추가
```powershell
# 1. vcpkg 설치
vcpkg install {package-name} --triplet x64-windows

# 2. CMakeLists.txt 수정
find_package({PackageName} CONFIG REQUIRED)
target_link_libraries(Backtesting PRIVATE {PackageName}::{Target})

# 3. 재빌드
cmake -S . -B Builds/Release
cmake --build Builds/Release
```

### npm 패키지 추가
```powershell
cd Sources/js
npm install {package-name}
# package.json에 자동 추가됨
npm run build  # 빌드 확인
```

## 빠른 참조 (Quick Reference)

### 주요 파일 위치
| 목적                      | 경로                                                |
|---------------------------|-----------------------------------------------------|
| 메인 API                  | `Includes/Backtesting.hpp`                          |
| 엔진 코어                 | `Includes/Engines/Engine.hpp`                       |
| 전략 베이스               | `Includes/Engines/Strategy.hpp`                     |
| 지표 베이스               | `Includes/Engines/Indicator.hpp`                    |
| 주문 핸들러               | `Includes/Engines/OrderHandler.hpp`                 |
| 설정 빌더                 | `Includes/Engines/Config.hpp`                       |
| 바 데이터 구조            | `Includes/Engines/BarData.hpp`                      |
| 플롯 설정                 | `Includes/Engines/Plot.hpp`                         |
| 실행 파일 엔트리          | `Sources/cpp/Backtesting.cpp`                       |
| 프론트 서버               | `Sources/js/launch.js`                              |
| React 메인                | `Sources/js/src/App.tsx`                            |
| 빌드 설정                 | `CMakeLists.txt`                                    |
| npm 설정                  | `Sources/js/package.json`                           |

### 자주 사용하는 클래스/함수
**전략에서**:
```cpp
// 주문
order_handler->MarketEntry("진입이름", LONG, 수량, 레버리지);
order_handler->LimitExit("청산이름", "진입이름", 가격, 수량);

// 지표 추가 (생성자에서)
SimpleMovingAverage& sma = AddIndicator<SimpleMovingAverage>(
    "SMA", trading_timeframe, Line(Rgba::blue), close, 20);

// 지표 참조
if (sma[0] > sma[1]) { ... }  // 현재 봉 vs 1봉 전

// 기본 제공 지표 (생성자 없이 바로 사용)
close[0], open[0], high[0], low[0], volume[0]
```

**Config 설정**:
```cpp
Backtesting::SetConfig()
    .SetRootDirectory("경로")
    .SetInitialBalance(10000)
    .SetTakerFeePercentage(0.045)      // 0.045%
    .SetMakerFeePercentage(0.018)      // 0.018%
    .SetSlippage(MarketImpactSlippage(1.5))  // 또는 FixedSlippage(틱수)
    .SetUseBarMagnifier(true)          // 돋보기 바 활성화
    .SetBacktestPeriod("2023-01-01", "2023-12-31", "%Y-%m-%d");
```

**로깅**:
```cpp
Logger::Log("정보 메시지");
Logger::LogAndThrowError("에러 메시지", __FILE__, __LINE__);
```

### 컴파일러 플래그 (Release)
| 플래그           | 효과                                     |
|------------------|------------------------------------------|
| `/O2`            | 속도 최적화                              |
| `/Oi`            | 내장 함수 사용                           |
| `/Ot`            | 빠른 코드 우선                           |
| `/GL`            | 전체 프로그램 최적화                     |
| `/arch:AVX2`     | AVX2 SIMD 명령어 사용                    |
| `/LTCG`          | 링크 타임 코드 생성                      |
| `/fp:strict`     | 부동소수점 정밀도 엄격 모드              |

### 프론트엔드 주요 훅
```typescript
// WebSocket 통신
const { ws, serverError } = useWebSocket();

// 로고 로딩 상태
const { isGlobalLoading, setIsGlobalLoading } = useLogo();

// 트레이드 필터
const { filters, setFilters } = useTradeFilter();
```

### 디버깅 체크포인트
| 문제                          | 확인 사항                                      |
|-------------------------------|------------------------------------------------|
| 빌드 실패                     | vcpkg 경로, 패키지 설치, `CMakeLists.txt` 수정 |
| 실행 시 데이터 없음           | `Data/` 디렉터리 Parquet 파일 존재 여부        |
| 전략 미실행                   | `AddStrategy<T>()` 호출, 클래스/파일명 일치    |
| 지표 값 이상                  | 타임프레임 일치, `Initialize()` 구현 확인      |
| 주문 거부                     | 자금 부족, 반대 방향 포지션, 레버리지 제한     |
| 프론트엔드 연결 실패          | 서버 실행, 포트 확인, WebSocket URL            |
| Parquet 파싱 오류             | 스키마 일치, 컬럼 이름, 데이터 타입            |

### 코드 생성 템플릿 사용법
**새 전략 생성**:
1. `Includes/Strategies/MyStrategy.hpp` 복사 템플릿:
   ```cpp
   #pragma once
   #include "Engines/Strategy.hpp"
   
   class MyStrategy final : public Strategy {
    public:
     explicit MyStrategy(const string& name);
     void Initialize() override {}
     void ExecuteOnClose() override {}
     void ExecuteAfterEntry() override {}
     void ExecuteAfterExit() override {}
   };
   ```

2. `Sources/cpp/Strategies/MyStrategy.cpp`:
   ```cpp
   #include "Strategies/MyStrategy.hpp"
   
   MyStrategy::MyStrategy(const string& name) : Strategy(name) {
     // 지표 추가
   }
   ```

**새 지표 생성**:
```cpp
// .hpp
class MyIndicator final : public Indicator {
 public:
  explicit MyIndicator(const string& name, const string& timeframe,
                       const Plot& plot, double param);
 private:
  double param_;
  void Initialize() override;
  Numeric<double> Calculate() override;
};

// .cpp
MyIndicator::MyIndicator(const string& name, const string& timeframe,
                         const Plot& plot, double param)
    : Indicator(name, timeframe, plot), param_(param) {}

void MyIndicator::Initialize() { /* 초기화 */ }

Numeric<double> MyIndicator::Calculate() {
  return close[0] * param_;  // 예시
}
```

## 고급 기능 & 최적화

### 돋보기 바 (Magnifier Bar) 시스템
- **목적**: 트레이딩 바 내부를 더 작은 타임프레임으로 시뮬레이션 (체결 정확도 향상)
- **예시**: 트레이딩 바 `1h`, 돋보기 바 `1m` → 각 1시간 바를 60개 1분 바로 세분화
- **활성화**: `SetConfig().SetUseBarMagnifier(true)`
- **주의사항**:
  - 마크 프라이스 타임프레임은 돋보기 바와 일치해야 함
  - 돋보기 바 < 트레이딩 바 (배수 관계)
  - 계산 시간 증가 (60배)

### 격리 마진 (Isolated Margin) 시스템
- **동작**: 각 진입은 독립적으로 마진 관리
- **제약**:
  - 한 심볼에서 **단방향 동시 진입**만 가능 (LONG+LONG ✓, LONG+SHORT ✗)
  - 반대 방향 진입 시 기존 포지션 **자동 리버스 청산**
- **마진 계산**:
  ```
  초기 마진 = (체결가 × 체결크기 / 레버리지) + 미실현손실
  ```
- **강제 청산**: 마크 프라이스 기준으로 마진 비율 초과 시 청산

### 성능 프로파일링
**C++ (Visual Studio)**:
1. `CMakeLists.txt`에서 프로파일링 플래그 주석 해제
2. Release 빌드 실행
3. Visual Studio → Analyze → Performance Profiler
4. CPU Usage / Memory Usage 탭 확인

**프론트엔드 (Chrome DevTools)**:
1. Performance 탭 → 녹화 시작
2. 차트 렌더링 / 데이터 로딩 수행
3. Flamegraph에서 병목 지점 확인
4. React Profiler 탭으로 컴포넌트별 렌더링 시간 측정

### 멀티 전략 합성 (프론트엔드)
- **제약**: C++ 엔진은 **1개 전략만** 실행 가능
- **해결**: 각 전략을 독립 실행 → 프론트엔드에서 결과 합성
- **구현**:
  1. 전략별로 `Backtesting::Run()` 실행 → `Results/{전략A}/`, `Results/{전략B}/`
  2. Backboard에서 "전략 합성" 기능 사용 (계획 중)
  3. 각 전략을 독립 계좌로 가정, 총 자산 = Σ(전략별 자산)

## 추가 리소스

### 참고 문서 위치
| 문서                          | 위치                                        |
|-------------------------------|---------------------------------------------|
| Strategy 작성 가이드          | `Includes/Engines/Strategy.hpp` (주석)      |
| Indicator 작성 가이드         | `Includes/Engines/Indicator.hpp` (주석)     |
| OrderHandler 사용법           | `Includes/Engines/OrderHandler.hpp` (주석)  |
| 봉 가정 (Price Queue)         | `Sources/cpp/Engines/Engine.cpp` 함수 주석  |
| Parquet 스키마                | `Sources/cpp/Engines/Analyzer.cpp`          |

### 외부 참조
- **vcpkg**: [https://github.com/microsoft/vcpkg](https://github.com/microsoft/vcpkg)
- **Arrow/Parquet C++**: [https://arrow.apache.org/docs/cpp/](https://arrow.apache.org/docs/cpp/)
- **React**: [https://react.dev](https://react.dev)
- **Vite**: [https://vitejs.dev](https://vitejs.dev)
- **TailwindCSS**: [https://tailwindcss.com](https://tailwindcss.com)
- **Plotly**: [https://plotly.com/javascript/](https://plotly.com/javascript/)

### 알려진 제한사항
1. **플랫폼**: Windows + MSVC만 지원 (Linux/macOS 미지원)
2. **데이터 형식**: Parquet만 지원 (CSV 미지원)
3. **바 데이터 컬럼**: 반드시 `open_time, open, high, low, close, volume, close_time` 순서
4. **타임프레임 배수**: 돋보기 < 트레이딩 ≤ 참조 (배수 관계 필수)
5. **동시 진입**: 단방향만 가능 (양방향 헤지 불가)
6. **전략 개수**: 1개만 실행 (멀티 전략은 프론트엔드에서 합성)

### 기여 가이드라인
(프로젝트에 CONTRIBUTING.md가 없으므로 기본 가이드)
1. **브랜치 전략**: `master` (메인), `feature/*` (기능), `bugfix/*` (버그 수정)
2. **커밋 메시지**: 한국어 사용, 명령형 (예: "SMA 지표 추가", "메모리 누수 수정")
3. **코드 리뷰**: Pull Request 생성 후 리뷰 요청
4. **테스트**: 변경 후 빌드 + 실행 확인, 결과 파일 검증

---

**이 문서는 AI 에이전트가 즉시 생산적으로 작업할 수 있도록 설계되었습니다. 추가 질문이나 불명확한 부분이 있다면 해당 섹션의 헤더 파일 주석을 참고하거나, `Sources/cpp/` 구현 파일에서 실제 동작을 확인하세요.**