#pragma once

// 표준 라이브러리
#include <memory>
#include <mutex>

// 내부 헤더
#include "BaseAnalyzer.hpp"

/// 파이썬 함수를 사용하여 세부적인 분석을 하는 분석기 클래스
class Analyzer final : public BaseAnalyzer {
 public:
  static shared_ptr<Analyzer>& GetAnalyzer();

 private:
  // 싱글톤 인스턴스 관리
  Analyzer();
  class Deleter {
   public:
    void operator()(const Analyzer* p) const;
  };

  static mutex mutex_;
  static shared_ptr<Analyzer> instance_;
};