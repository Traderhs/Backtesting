// 파일 헤더

#include "Engines/DataManager.hpp"

DataManager::DataManager()
    : initial_capital(-1),
      commission(-1),
      commission_type(CommissionType::COMMISSION_PERCENTAGE),
      slippage(-1),
      slippage_type(SlippageType::SLIPPAGE_PERCENTAGE),
      current_capital(-1),
      max_capital(-1),
      drawdown(0),
      maximum_drawdown(0) {}

DataManager::~DataManager() = default;

DataManager& DataManager::GetDataManager() {
  if (!instance) {
    lock_guard lock(mutex);
    instance.reset(new DataManager());
  }
  return *instance;
}

void DataManager::SetInitialCapital(const double initial_capital) {
  this->initial_capital = initial_capital;
}

void DataManager::SetCommissionAndType(const double commission,
                                       const CommissionType commission_type) {
  this->commission = commission;
  this->commission_type = commission_type;
}

void DataManager::SetSlippageAndType(const double slippage,
                                     const SlippageType slippage_type) {
  this->slippage = slippage;
  this->slippage_type = slippage_type;
}

double DataManager::GetInitialCapital() const {
  return initial_capital;
}

double DataManager::GetCommission() const {
  return commission;
}

double DataManager::GetSlippage() const {
  return slippage;
}

mutex DataManager::mutex;
unique_ptr<DataManager> DataManager::instance;