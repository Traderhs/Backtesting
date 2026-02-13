// 표준 라이브러리
#include <format>

// 파일 헤더
#include "Engines/StrategyLoader.hpp"

// 내부 헤더
#include "Engines/Logger.hpp"

namespace backtesting::strategy {

bool StrategyLoader::Load(const string& dll_path, string& out_error) {
  // 이미 로드되어 있다면 먼저 언로드
  if (dll_handle_) {
    Unload();
  }

  // DLL 로드
  dll_handle_ = LoadLibraryA(dll_path.c_str());
  if (!dll_handle_) {
    const DWORD error = GetLastError();
    out_error = format("[{}] DLL 로드를 실패했습니다. 오류 코드: [{}]",
                       dll_path, error);
    return false;
  }

  // AddStrategyFromDll 함수 포인터 가져오기
  add_strategy_func_ = reinterpret_cast<AddStrategyFromDllFunc>(
      GetProcAddress(dll_handle_, "AddStrategyFromDll"));
  if (!add_strategy_func_) {
    const DWORD error = GetLastError();
    out_error =
        format("AddStrategyFromDll 함수를 찾을 수 없습니다. 오류 코드: [{}]",
               dll_path, error);

    Unload();
    return false;
  }

  return true;
}

bool StrategyLoader::AddStrategyFromDll(const string& name,
                                        string& out_error) const {
  if (!dll_handle_ || !add_strategy_func_) {
    out_error = "DLL이 로드되지 않았거나 함수 포인터가 유효하지 않습니다.";
    return false;
  }

  try {
    // DLL 내부에서 AddStrategy를 호출하는 함수 실행
    if (const bool success = add_strategy_func_(name.c_str()); !success) {
      out_error = "DLL의 AddStrategyFromDll 함수가 전략 추가를 실패했습니다.";
      return false;
    }

    return true;
  } catch (const std::exception& e) {
    out_error = format("전략 추가 중 예외가 발생했습니다. {}", e.what());
    return false;
  } catch (...) {
    out_error = "전략 추가 중 알 수 없는 예외가 발생했습니다.";
    return false;
  }
}

void StrategyLoader::Unload() {
  add_strategy_func_ = nullptr;

  if (dll_handle_) {
    FreeLibrary(dll_handle_);
    dll_handle_ = nullptr;
  }
}

}  // namespace backtesting::strategy
