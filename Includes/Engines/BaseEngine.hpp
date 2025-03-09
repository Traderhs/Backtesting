#pragma once

// 표준 라이브러리
#include <string>
#include <vector>

// 외부 라이브러리
#include "nlohmann/json_fwd.hpp"

// 전방 선언
namespace backtesting::analyzer {
class Analyzer;
}

namespace backtesting::bar {
class BarHandler;
enum class BarType;
}  // namespace backtesting::bar

namespace backtesting::config {
class Config;
}

namespace backtesting::logger {
class Logger;
}

namespace backtesting::strategy {
class Strategy;
}

namespace backtesting::indicator {
class Indicator;
}

// 네임 스페이스
using namespace std;
using namespace nlohmann;
namespace backtesting {
using namespace bar;
using namespace analyzer;
using namespace config;
using namespace logger;
using namespace indicator;
using namespace strategy;
}  // namespace backtesting

namespace backtesting::engine {

/// 엔진의 기본적인 설정, 초기화를 담당하는 클래스
class BaseEngine {
 public:
  /// 주어진 파일 경로에서 Parquet 데이터를 읽고
  /// 지정된 바 타입으로 처리하여 바 핸들러에 추가하는 함수
  ///
  /// @param symbol_name 심볼 이름
  /// @param file_path Parquet 파일의 경로
  /// @param bar_type 추가할 데이터의 바 타입
  /// @param open_time_column Open Time 컬럼 인덱스
  /// @param open_column Open 컬럼 인덱스
  /// @param high_column High 컬럼 인덱스
  /// @param low_column Low 컬럼 인덱스
  /// @param close_column Close 컬럼 인덱스
  /// @param volume_column Volume 컬럼 인덱스
  /// @param close_time_column Close Time 컬럼 인덱스
  static void AddBarData(const string& symbol_name, const string& file_path,
                         BarType bar_type, int open_time_column = 0,
                         int open_column = 1, int high_column = 2,
                         int low_column = 3, int close_column = 4,
                         int volume_column = 5, int close_time_column = 6);

  /// 거래소 정보를 엔진에 추가하는 함수.
  static void AddExchangeInfo(const string& exchange_info_path);

  /// 레버리지 구간을 엔진에 추가하는 함수.
  static void AddLeverageBracket(const string& leverage_bracket_path);

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

  /// 강제 청산 횟수를 증가시키는 함수
  void IncreaseLiquidationCount();

  /// 메인 디렉토리를 반환하는 함수
  [[nodiscard]] string GetMainDirectory() const;

  /// 엔진 설정값을 반환하는 함수
  [[nodiscard]] static shared_ptr<Config> GetConfig();

  /// 지갑 자금을 반환하는 함수
  [[nodiscard]] double GetWalletBalance() const;

  /// 최고 지갑 자금을 반환하는 함수
  [[nodiscard]] double GetMaxWalletBalance() const;

  // 현재 드로우다운을 반환하는 함수
  [[nodiscard]] double GetDrawdown() const;

  // 최고 드로우다운을 반환하는 함수
  [[nodiscard]] double GetMaxDrawdown() const;

  /// 자금 관련 통계 항목을 업데이트하는 함수
  void UpdateStatistics();

 protected:
  BaseEngine();
  ~BaseEngine();

  static shared_ptr<Analyzer>& analyzer_;
  static shared_ptr<BarHandler>& bar_;
  static shared_ptr<Logger>& logger_;

  /// 엔진이 초기화 되었는지 여부를 결정하는 플래그
  bool engine_initialized_;

  /// 거래소 정보
  static json exchange_info_;

  /// 레버리지 구간
  static json leverage_bracket_;

  /// 엔진의 사전 설정 항목
  static shared_ptr<Config> config_;
  friend class Config;

  /// 한 회의 백테스팅에서 사용할 메인 디렉토리
  string main_directory_;

  /// 엔진에 추가된 전략
  vector<shared_ptr<Strategy>> strategies_;

  /// 전략에서 사용하는 지표
  ///
  /// 전략들<지표들>
  vector<vector<shared_ptr<Indicator>>> indicators_;

  // 자금 항목
  /// 지갑 자금 = 초기 자금 ± 실현 손익 ± 펀딩피 - 수수료
  double wallet_balance_;

  /// 사용 가능 자금 = 지갑 자금 - 사용한 마진
  double available_balance_;

  /// 미실현 손익 = 진입한 포지션의 손익의 합
  double unrealized_pnl_;

  /// 사용한 마진: 진입 증거금 + 예약 증거금
  double used_margin_;

  /// 파산 여부를 나타내는 플래그
  bool is_bankruptcy_;

  // 자금 관련 통계 항목
  double max_wallet_balance_;  /// 최고 자금
  double drawdown_;            /// 현재 드로우다운
  double max_drawdown_;        /// 최고 드로우다운
  int liquidation_count_;      /// 강제 청산 횟수
  // @@@@@@@@@@@@@@@@ 마진콜 X ->
  // 유지 증거금 계산 메커니즘 도입 -> 진입 금액에 따라 진입 레버리지 제한 ->
  // api로 받아와야함 유지 증거금 -> 강제 청산 가격 계산 강제 청산시 보험 기금
  // 감소됨 -> 이것도 만들기

  /// =로 콘솔창을 분리하는 출력을 발생시키는 함수
  static void PrintSeparator();
};

}  // namespace backtesting::engine