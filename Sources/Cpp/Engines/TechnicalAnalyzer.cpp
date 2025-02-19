// 파일 헤더
#include "Engines/TechnicalAnalyzer.hpp"

TechnicalAnalyzer::TechnicalAnalyzer() = default;
void TechnicalAnalyzer::Deleter::operator()(const TechnicalAnalyzer* p) const {
  delete p;
}

mutex TechnicalAnalyzer::mutex_;
shared_ptr<TechnicalAnalyzer> TechnicalAnalyzer::instance_;

shared_ptr<TechnicalAnalyzer>& TechnicalAnalyzer::GetTechnicalAnalyzer() {
  lock_guard lock(mutex_);  // 다중 스레드에서 안전하게 접근하기 위해 mutex 사용

  // 인스턴스가 생성됐는지 확인
  if (!instance_) {
    instance_ =
        shared_ptr<TechnicalAnalyzer>(new TechnicalAnalyzer(), Deleter());
  }

  return instance_;
}