#pragma once

// 표준 라이브러리
#include <string>
#include <unordered_map>
#include <vector>

// 내부 헤더
#include "Engines/Analyzer.hpp"

// 외부 라이브러리
#include "nlohmann/json_fwd.hpp"

// 전방 선언
namespace backtesting::analyzer {
class Analyzer;
}  // namespace backtesting::analyzer

namespace backtesting::engine {
class Config;
}

namespace backtesting::logger {
class Logger;
}

// 네임 스페이스
using namespace std;
using namespace nlohmann;
namespace backtesting {
using namespace analyzer;
using namespace logger;
}  // namespace backtesting

namespace backtesting::engine {
/// 엔진의 기본적인 설정, 초기화를 담당하는 클래스
class BaseEngine {
 public:
  /// 거래소 정보를 엔진에 추가하는 함수.
  static void AddExchangeInfo(const string& exchange_info_path);

  /// 레버리지 구간을 엔진에 추가하는 함수.
  static void AddLeverageBracket(const string& leverage_bracket_path);

  /// 펀딩 비율을 엔진에 추가하는 함수.
  static void AddFundingRates(const vector<string>& symbol_names,
                              const string& funding_rates_directory);

  // ==========================================================================
  /// 엔진이 초기화 되었는지 여부를 반환하는 함수
  [[nodiscard]] bool IsEngineInitialized() const;

  /// 지갑 자금을 증가시키는 함수.
  void IncreaseWalletBalance(double increase_balance);

  /// 지갑 자금을 감소시키는 함수 (양수로 지정).
  void DecreaseWalletBalance(double decrease_balance);

  /// 사용한 마진을 증가시키는 함수
  void IncreaseUsedMargin(double increase_margin);

  /// 사용한 마진을 감소시키는 함수 (양수로 지정)
  void DecreaseUsedMargin(double decrease_margin);

  /// 파산을 당했을 때 설정하는 함수
  void SetBankruptcy();

  /// 해당되는 심볼 인덱스의 거래소 정보를 반환하는 함수
  [[nodiscard]] const SymbolInfo& GetSymbolInfo(int symbol_idx) const;

  /// 엔진 설정값을 반환하는 함수
  [[nodiscard]] static const shared_ptr<Config>& GetConfig();

  /// 지갑 자금을 반환하는 함수
  [[nodiscard]] double GetWalletBalance() const;

  /// 사용한 마진을 반환하는 함수
  [[nodiscard]] double GetUsedMargin() const;

  //// 사용 가능 자금을 업데이트하고 반환하는 함수
  double GetAvailableBalance();

  /// 최고 지갑 자금을 반환하는 함수
  [[nodiscard]] double GetMaxWalletBalance() const;

  // 현재 드로우다운을 반환하는 함수
  [[nodiscard]] double GetDrawdown() const;

  // 최고 드로우다운을 반환하는 함수
  [[nodiscard]] double GetMaxDrawdown() const;

  /// 자금 관련 통계 항목을 업데이트하는 함수
  void UpdateStatistics();

  /// 현재 자금을 로그하는 함수
  void LogBalance();

  /// '='로 콘솔창을 분리하는 로그를 발생시키는 함수
  static void LogSeparator(bool log_to_console);

 protected:
  BaseEngine();
  ~BaseEngine();

  static shared_ptr<Analyzer>& analyzer_;
  static shared_ptr<BarHandler>& bar_;
  static shared_ptr<Logger>& logger_;

  /// 엔진이 초기화 되었는지 여부를 결정하는 플래그
  bool engine_initialized_;

  int trading_bar_num_symbols_;      /// 트레이딩 바 심볼 개수
  string trading_bar_timeframe_;     /// 트레이딩 바 타임프레임
  int64_t trading_bar_time_diff_;    /// 트레이딩 바 사이의 타임스탬프 차이
  int64_t magnifier_bar_time_diff_;  /// 돋보기 바 사이의 타임스탬프 차이
  unordered_map<string, int64_t>
      reference_bar_time_diff_;  /// 참조 바 사이의 타임스탬프 차이

  /// 거래소 정보
  static json exchange_info_;
  static string exchange_info_path_;

  /// 레버리지 구간
  static json leverage_bracket_;
  static string leverage_bracket_path_;

  // 심볼별 거래소 정보
  vector<SymbolInfo> symbol_info_;

  /// 펀딩 비율 (벡터는 심볼 순서)
  static vector<json> funding_rates_;
  static vector<string> funding_rates_paths_;

  /// 엔진의 사전 설정 항목
  static shared_ptr<Config> config_;
  friend class Config;

  /// 엔진에 추가된 전략
  shared_ptr<Strategy> strategy_;

  /// 전략에서 사용하는 지표들
  vector<shared_ptr<Indicator>> indicators_;

  // 자금 항목
  /// 지갑 자금 = 초기 자금 ± 실현 손익 ± 펀딩비 - 수수료
  double wallet_balance_;

  /// 사용한 마진: 진입 증거금 + 예약 증거금
  double used_margin_;

  /// 사용 가능 자금 = 지갑 자금 - 사용한 마진
  double available_balance_;

  /// 파산 여부를 나타내는 플래그
  bool is_bankruptcy_;

  // 자금 관련 통계 항목
  double max_wallet_balance_;  // 최고 자금
  double drawdown_;            // 현재 드로우다운
  double max_drawdown_;        // 최고 드로우다운
};

}  // namespace backtesting::engine