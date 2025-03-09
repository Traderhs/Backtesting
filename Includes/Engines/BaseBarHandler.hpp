#pragma once

// 표준 라이브러리
#include <memory>
#include <string>
#include <unordered_map>

// 전방 선언
namespace backtesting::bar {
class BarData;
}

namespace backtesting::logger {
class Logger;
}

// 네임 스페이스
using namespace std;
namespace backtesting {
  using namespace bar;
  using namespace logger;
}

namespace backtesting::bar {

/// 바 데이터 타입을 지정하는 열거형 클래스
///
/// TRADING: 백테스팅 전략을 실행하는 메인 바 데이터
///
/// MAGNIFIER: 트레이딩 바 하나의 내부 움직임을 추적하는 돋보기 바 데이터
///
/// REFERENCE: 지표 혹은 전략에서 트레이딩 바 데이터 타임프레임과 같거나 배수
///            타임프레임의 바 데이터 값을 참조할 수 있는 바 데이터
///
/// MARK_PRICE: 여러 거래소의 시장 평균 가격을 나타내는 바 데이터
enum class BarType { TRADING, MAGNIFIER, REFERENCE, MARK_PRICE };
using enum BarType;

/// 타입별 바 데이터를 저장하고 기본적인 관리를 하는 클래스
class BaseBarHandler {
 public:
  /// 지정된 바 타입의 바 데이터를 반환하는 함수
  [[nodiscard]] shared_ptr<BarData> GetBarData(
      BarType bar_type, const string& timeframe = "");

  /// 지정된 바 타입의 모든 심볼이 포함된 인덱스 벡터를 반환하는 함수
  [[nodiscard]] vector<size_t>& GetBarIndices(BarType bar_type,
                                              const string& timeframe = "");

  /// 참조 바 데이터 전체를 반환하는 함수
  [[nodiscard]] unordered_map<string, shared_ptr<BarData>>
  GetAllReferenceBarData();

 protected:
  BaseBarHandler();
  ~BaseBarHandler();

  static shared_ptr<Logger>& logger_;

  /// 백테스팅 전략을 실행하는 메인 바 데이터. 심볼간 타임프레임을 통일.
  shared_ptr<BarData> trading_bar_data_;
  vector<size_t> trading_index_;  // 트레이딩 바 데이터의 각 심볼별 진행 인덱스

  /// 트레이딩 바 하나의 내부 움직임을 추적하는 돋보기 바 데이터. 심볼간
  /// 타임프레임을 통일.
  shared_ptr<BarData> magnifier_bar_data_;
  vector<size_t> magnifier_index_;  // 돋보기 바 데이터의 각 심볼별 진행 인덱스

  /// 지표 혹은 전략에서 트레이딩 바 데이터 타임프레임과 같거나 배수
  /// 타임프레임의 바 데이터 값을 참조할 수 있는 바 데이터.
  /// 심볼간 타임프레임을 통일.
  ///
  ///  [타임프레임, 바 데이터]
  unordered_map<string, shared_ptr<BarData>> reference_bar_data_;
  unordered_map<string, vector<size_t>>
      reference_index_;  // 각 타임프레임 참조 바 데이터의 각 심볼별 진행 인덱스

  /// 여러 거래소의 평균 가격을 나타내는 바 데이터. 심볼간 타임프레임을 통일.
  ///
  /// 미실현 손익, 청산 계산 시 사용.
  shared_ptr<BarData> mark_price_bar_data_;
  vector<size_t>
      mark_price_index_;  // 마크 가격 바 데이터의 각 심볼별 진행 인덱스
};

}  // namespace backtesting::bar