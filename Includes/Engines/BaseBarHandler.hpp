#pragma once

// 표준 라이브러리
#include <set>
#include <string>
#include <variant>

// 내부 헤더
#include "Engines/BarData.hpp"
#include "Engines/Logger.hpp"

// 네임 스페이스
using namespace std;

/// 타입별 바 데이터를 저장하고 기본적인 관리를 하는 클래스
class BaseBarHandler {
 public:
  /// 바 데이터 타입을 지정하는 열거형 클래스
  enum class BarType { TRADING, MAGNIFIER, REFERENCE };

  /// 지정된 바 타입의 바 데이터를 반환하는 함수
  BarData& GetBarData(BarType bar_type, const string& timeframe = "");

  /// 지정된 바 타입의 타임프레임을 반환하는 함수.
  /// Get 필요: string 혹은 set<string>
  variant<string, set<string>> GetTimeframe(BarType bar_type);

 protected:
  BaseBarHandler();
  ~BaseBarHandler();

  static Logger& logger_;

  /// 지정된 바 타입의 타임프레임을 설정하는 함수
  void SetTimeframe(const string& timeframe, BarType bar_type);

  /// 백테스팅을 위해 거래를 진행하는 메인 바 데이터. 심볼간 타임프레임을 통일
  BarData trading_bar_;
  string trading_timeframe_;          // 트레이딩 바 문자열 타임프레임
  int64_t parsed_trading_timeframe_;  // 트레이딩 바 파싱 타임프레임
  vector<size_t> trading_index_;      // 각 심볼별 트레이딩 바의 진행 인덱스

  /// 바 세부 움직임을 추적하는 돋보기 기능을 위한 바 데이터. 심볼간
  /// 타임프레임을 통일
  BarData magnifier_bar_;
  string magnifier_timeframe_;          // 돋보기 바 문자열 타임프레임
  int64_t parsed_magnifier_timeframe_;  // 돋보기 바 파싱 타임프레임
  vector<size_t> magnifier_index_;      // 각 심볼별 돋보기 바의 진행 인덱스

  /// 지표 계산 혹은 상위 타임프레임 가격 참조를 위한 바 데이터. <타임프레임, 바
  /// 데이터>
  unordered_map<string, BarData> reference_bar_;
  set<string> reference_timeframe_;          // 레퍼런스 바 문자열 타임프레임
  unordered_map<string, vector<size_t>>
      reference_index_;  // 해당 타임프레임 각 심볼별 참조 바의 진행 인덱스
};