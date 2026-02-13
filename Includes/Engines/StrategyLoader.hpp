#pragma once

// 표준 라이브러리
#include <windows.h>

#include <string>

// 내부 헤더
#include "Engines/Strategy.hpp"

// 네임스페이스
using namespace std;
namespace backtesting::strategy {

/**
 * DLL로부터 전략을 동적 로드하는 클래스
 *
 * Windows LoadLibrary/GetProcAddress를 사용하여
 * 런타임에 전략 DLL을 로드하고 인스턴스를 생성
 */
class BACKTESTING_API StrategyLoader {
 public:
  StrategyLoader() : dll_handle_(nullptr) {}
  ~StrategyLoader() { Unload(); }

  // 복사/이동 방지
  StrategyLoader(const StrategyLoader&) = delete;
  StrategyLoader& operator=(const StrategyLoader&) = delete;
  StrategyLoader(StrategyLoader&&) = delete;
  StrategyLoader& operator=(StrategyLoader&&) = delete;

  /// DLL 파일을 로드하는 함수
  /// @param dll_path DLL 파일 경로
  /// @param out_error 에러 메시지 출력용
  /// @return 성공 시 true, 실패 시 false
  bool Load(const string& dll_path, string& out_error);

  /// DLL로부터 전략을 AddStrategy를 통해 등록하는 함수
  /// @param name 전략 이름
  /// @param out_error 에러 메시지 출력용
  /// @return 성공 시 true, 실패 시 false
  bool AddStrategyFromDll(const string& name, string& out_error) const;

  /// DLL을 언로드하는 함수
  void Unload();

  /// DLL이 로드되어 있는지 확인하는 함수
  [[nodiscard]] bool IsLoaded() const { return dll_handle_ != nullptr; }

 private:
  HMODULE dll_handle_;  // DLL 핸들

  // DLL에서 export된 함수 포인터 타입
  // DLL 내부에서 AddStrategy를 호출하고 성공 여부를 반환
  using AddStrategyFromDllFunc = bool (*)(const char*);
  AddStrategyFromDllFunc add_strategy_func_ = nullptr;
};

}  // namespace backtesting::strategy
