#pragma once

// 표준 라이브러리
#include <mutex>

// 내부 헤더
#include "Engines/DataManager.hpp"
#include "Engines/Logger.hpp"
#include "Indicators/Indicators.hpp"

// 네임 스페이스
using namespace std;

/**
 * 바 데이터의 추가, 관리 등을 담당하는 클래스
 */
class BarDataManager final {
 public:
  /// 하나의 바 구조를 나타내는 구조체
  struct bar_data {
    int64_t open_time = -1;      // 바 시작 시간
    double open = nan("");    // 시가
    double high = nan("");    // 고가
    double low = nan("");     // 저가
    double close = nan("");   // 종가
    double volume = nan("");  // 거래량
    int64_t close_time = -1;     // 바 종료 시간
  };

  /// 바 데이터 타입을 지정하는 열거형 클래스
  enum class BarDataType { TRADING, MAGNIFIER, SUB };

  // 현재 사용 중인 바 데이터 타입: TRADING, MAGNIFIER, SUB
  BarDataType current_bar_data_type;

  // 현재 사용 중인 심볼
  string current_symbol;

  // 현재 심볼과 현재 바의 정보들
  int64_t current_open_time;
  double current_open;
  double current_high;
  double current_low;
  double current_close;
  double current_volume;
  int64_t current_close_time;

  /// BarDataManager의 싱글톤 인스턴스를 반환하는 함수
  static BarDataManager& GetBarDataManager();

  /**
   * 주어진 파일 경로에서 Parquet 데이터를 읽고, 지정된 비율로
   * 트레이딩 데이터와 테스트 트레이딩 데이터로 분할한 후 각각의 데이터를
   * 처리하여 엔진에 추가하는 함수
   *
   * @param name 데이터를 추가할 때 사용할 고유 키 이름
   *             중복된 이름은 허용되지 않음
   * @param file_path Parquet 파일의 경로
   * @param columns Parquet 파일에서 데이터를 추출할
   *                컬럼의 인덱스를 다음 순서로 지정
   *                [Open Time, Open, High, Low, Close, Volume, Close Time]
   * @param split_ratio 트레이딩 데이터와 테스트 트레이딩 데이터로
   *                    분할할 때 사용할 비율
   */
  void AddTradingBarData(const string& name, const string& file_path,
                         const vector<int>& columns, double split_ratio);

  /**
   * 주어진 파일 경로에서 Parquet 데이터를 읽고 돋보기 데이터로 처리하여
   * 엔진에 추가하는 함수
   *
   * @param name 데이터를 추가할 때 사용할 고유 키 이름
   *             중복된 이름은 허용되지 않음
   * @param file_path Parquet 파일의 경로
   * @param columns Parquet 파일에서 데이터를 추출할
   *                컬럼의 인덱스를 다음 순서로 지정
   *                [Open Time, Open, High, Low, Close, Volume, Close Time]
   */
  void AddMagnifierBarData(const string& name, const string& file_path,
                           const vector<int>& columns);

  /// 트레이딩 바 데이터를 반환하는 함수
  unordered_map<string, vector<bar_data>>& GetTradingBarData();

  /// 돋보기 바 데이터를 반환하는 함수
  unordered_map<string, vector<bar_data>>& GetMagnifierBarData();

  /// 서브 바 데이터를 반환하는 함수
  unordered_map<string, unordered_map<string, vector<bar_data>>>&
  GetSubBarData();

  /// 엔진에 추가된 트레이딩 바 데이터의 타임프레임을 반환하는 함수
  string& GetTradingTimeframe();

  /// 엔진에 추가된 돋보기 바 데이터의 타임프레임을 반환하는 함수
  string& GetMagnifierTimeframe();

  /// 엔진에 추가된 서브 바 데이터의 타임프레임을 반환하는 함수
  set<string>& GetSubTimeframe();

  /// 심볼과 타임프레임 및 바 데이터 타입에 해당되는 바 데이터의
  /// 현재 인덱스를 반환하는 함수
  size_t GetCurrentIndex(const string& symbol, const string& timeframe);

  /// 타임프레임, 인덱스 오류를 확인하고 가격 타입과 바 데이터 타입에 따라
  /// 해당 바를 반환하는 함수
  bar_data GetBar(const string& timeframe, size_t index);

  /// 해당되는 바 데이터 타입의 타임프레임을 설정하는 함수
  void SetTimeframe(BarDataType bar_data_type, const string& timeframe);

  /// 바 데이터 타입 및 심볼과 타임프레임에 해당되는 바 데이터의
  /// 현재 인덱스를 설정하는 함수
  void SetCurrentIndex(const string& symbol, const string& timeframe, size_t index);

 protected:
  // Google Test용 Protected

  /**
   * 주어진 바 데이터 테이블을 이용해 바 데이터를 추가로 가공하여 벡터 형태로
   * 반환하는 함수
   *
   * @param bar_data 바 데이터가 포함된 `Table` 객체를 가리키는 shared_ptr
   * @param columns [Open Time, Open, High, Low, Close, Volume, Close Time]
   *                컬럼의 인덱스를 순서대로 지정
   * @return `bar_data` 구조체의 벡터
   */
  static vector<bar_data> GetVectorAddedBarData(
      const shared_ptr<Table>& bar_data, const vector<int>& columns);

 private:
  // 싱글톤 인스턴스 관리
  static mutex mutex;
  static unique_ptr<BarDataManager> instance;

  static DataManager& data;
  static Logger& logger;

  /// 거래를 위한 바 데이터. 심볼간 타임프레임을 통일
  unordered_map<string, vector<bar_data>> trading_bar_data;

  /// 바 세부 움직임을 추적하는 돋보기 기능을 위한 바 데이터.
  /// 심볼간 타임프레임을 통일
  unordered_map<string, vector<bar_data>> magnifier_bar_data;
  // 돋보기 사용 안하는 설정이면 #include <functional>으로 봉
  // 계산 방법 다르게 해서 다른 함수로 초기화 때 지정해서 실행

  /// 지표 계산 혹은 상위 타임프레임 가격 참조를 위한 바 데이터.
  /// 구조: 심볼, <타임프레임, 바 데이터>
  unordered_map<string, unordered_map<string, vector<bar_data>>> sub_bar_data;

  /// 실제 매매 성과 테스트를 위한 트레이딩 바 데이터. 심볼간 타임프레임을 통일
  unordered_map<string, vector<bar_data>> test_trading_bar_data;

  string trading_timeframe;    // 트레이딩 바 데이터 타임프레임
  string magnifier_timeframe;  // 돋보기 바 데이터 타임프레임
  set<string> sub_timeframe;   // 서브 바 데이터 타임프레임

  /// 각 심볼의 트레이딩 진행 인덱스
  unordered_map<string, size_t> trading_index;

  /// 각 심볼의 돋보기 진행 인덱스
  unordered_map<string, size_t> magnifier_index;

  /// 각 심볼의 타임프레임 및 서브 진행 인덱스
  unordered_map<string, unordered_map<string, size_t>> sub_index;

  BarDataManager();
  ~BarDataManager();

  /**
   * 트레이딩 바 데이터의 유효성을 검증하는 함수
   *
   * @param name 트레이딩 데이터의 이름
   * @param bar_data 트레이딩 바 데이터로 추가할 분할한 데이터
   * @param open_time_column Open Time 데이터의 컬럼 인덱스
   * @return 엔진에 추가된 트레이딩 바 데이터의 타임프레임과 bar_data의 타임프레임
   */
  pair<string, string> IsValidTradingBarData(
    const string& name, const shared_ptr<Table>& bar_data, int open_time_column);

  /**
   * 돋보기 바 데이터의 유효성을 검증하는 함수
   *
   * @param name 돋보기 바 데이터의 이름
   * @param bar_data 돋보기 바 데이터로 추가할 분할한 데이터
   * @param open_time_column Open Time 데이터의 컬럼 인덱스
   * @return 엔진에 추가된 돋보기 바 데이터의 타임프레임과 bar_data의 타임프레임
   */
  pair<string, string> IsValidMagnifierBarData(
    const string& name, const shared_ptr<Table>& bar_data, int open_time_column);

  /**
   * 주어진 데이터에서 첫 Open Time과 다음 Open Time의 시간 차이를 계산하여 타임프레임을
   * 문자열로 반환하는 함수
   *
   * @param bar_data 바 데이터가 포함된 `Table` 객체를 가리키는 shared_ptr
   * @param open_time_column Open Time이 포함된 열의 인덱스
   * @return 첫 번째 Open Time과 두 번째 Open Time의 차이를 포맷한 타임프레임 문자열
   */
  static string CalculateTimeframe(const shared_ptr<Table>& bar_data, int open_time_column);
};