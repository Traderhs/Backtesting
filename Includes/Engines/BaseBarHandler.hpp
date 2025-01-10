#pragma once

// 표준 라이브러리
#include <memory>
#include <string>
#include <unordered_map>

// 내부 헤더
#include "BarData.hpp"
#include "Engines\Logger.hpp"

// 네임 스페이스
using namespace std;

/// 바 데이터 타입을 지정하는 열거형 클래스
enum class BarType { TRADING, MAGNIFIER, REFERENCE };

/// 타입별 바 데이터를 저장하고 기본적인 관리를 하는 클래스
class BaseBarHandler {
 public:
  /// 지정된 바 타입의 바 데이터를 반환하는 함수
  BarData& GetBarData(BarType bar_type, const string& timeframe = "");

 protected:
  BaseBarHandler();
  ~BaseBarHandler();

  static shared_ptr<Logger>& logger_;

  /// 백테스팅을 위해 거래를 진행하는 메인 바 데이터. 심볼간 타임프레임을 통일
  BarData trading_bar_;
  vector<size_t> trading_index_;  // 트레이딩 바 데이터의 각 심볼별 진행 인덱스

  /// 바 세부 움직임을 추적하는 돋보기 기능을 위한 바 데이터. 심볼간
  /// 타임프레임을 통일
  BarData magnifier_bar_;
  vector<size_t> magnifier_index_;  // 돋보기 바 데이터의 각 심볼별 진행 인덱스

  /// 지표 계산 혹은 상위 타임프레임 가격 참조를 위한 바 데이터. <타임프레임, 바
  /// 데이터>
  unordered_map<string, BarData> reference_bar_;
  unordered_map<string, vector<size_t>>
      reference_index_;  // 해당 타임프레임 참조 바 데이터의 각 심볼별 진행 인덱스
};