// 파일 헤더
#include "Engines/Analyzer.hpp"

namespace backtesting::analyzer {

Analyzer::Analyzer() = default;
void Analyzer::Deleter::operator()(const Analyzer* p) const { delete p; }

mutex Analyzer::mutex_;
shared_ptr<Analyzer> Analyzer::instance_;

shared_ptr<Analyzer>& Analyzer::GetAnalyzer() {
  lock_guard lock(mutex_);  // 스레드에서 안전하게 접근하기 위해 mutex 사용

  // 인스턴스가 생성됐는지 확인
  if (!instance_) {
    // 인스턴스가 생성되지 않았으면 생성 후 저장
    instance_ = shared_ptr<Analyzer>(new Analyzer(), Deleter());
  }

  return instance_;
}

}  // namespace backtesting::analyzer