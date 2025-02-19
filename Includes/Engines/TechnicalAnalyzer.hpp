#pragma once

// 표준 라이브러리
#include <memory>
#include <mutex>

// 네임 스페이스
using namespace std;

class TechnicalAnalyzer final {
public:
  static shared_ptr<TechnicalAnalyzer>& GetTechnicalAnalyzer();


private:
 // 싱글톤 인스턴스 관리
 explicit TechnicalAnalyzer();
 class Deleter {
 public:
  void operator()(const TechnicalAnalyzer* p) const;
 };

 static mutex mutex_;
 static shared_ptr<TechnicalAnalyzer> instance_;
};