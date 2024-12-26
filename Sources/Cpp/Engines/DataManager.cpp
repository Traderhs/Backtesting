// 파일 헤더
#include "Engines/DataManager.hpp"

DataManager::DataManager()
    : capital_updated_current_bar(false),
      initial_capital(-1),
      market_commission(-1),
      limit_commission(-1),
      commission_type(CommissionType::COMMISSION_PERCENTAGE),
      market_slippage(-1),
      limit_slippage(-1),
      slippage_type(SlippageType::SLIPPAGE_PERCENTAGE),
      current_capital(-1),
      available_capital(-1),
      max_capital(-1),
      drawdown(0),
      max_drawdown(0),
      margin_call_number(0) {}

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

void DataManager::SetCommissionAndType(const double market_commission,
                                       const double limit_commission,
                                       const CommissionType commission_type) {
  this->market_commission = market_commission;
  this->limit_commission = limit_commission;
  this->commission_type = commission_type;
}

void DataManager::SetSlippageAndType(const double market_slippage,
                                     const double limit_slippage,
                                     const SlippageType slippage_type) {
  this->market_slippage = market_slippage;
  this->limit_slippage = limit_slippage;
  this->slippage_type = slippage_type;
}

void DataManager::SetCurrentCapital(const double current_capital) {
  this->current_capital = current_capital;
}

void DataManager::SetAvailableCapital(const double available_capital) {
  this->available_capital = available_capital;
}

double DataManager::GetInitialCapital() const { return initial_capital; }

pair<double, double> DataManager::GetCommission() const {
  return {market_commission, limit_commission};
}

DataManager::CommissionType DataManager::GetCommissionType() const {
  return commission_type;
}

pair<double, double> DataManager::GetSlippage() const {
  return {market_slippage, limit_slippage};
}

DataManager::SlippageType DataManager::GetSlippageType() const {
  return slippage_type;
}

double DataManager::GetCurrentCapital() const { return current_capital; }

double DataManager::GetAvailableCapital() const { return available_capital; }

double DataManager::GetTickSize(const string& symbol) const {
  const auto& symbol_it = tick_size.find(symbol);
  if (symbol_it == tick_size.end()) {
    Logger::LogAndThrowError("심볼 {}이(가) min_tick에 설정되지 않았습니다.",
                             __FILE__, __LINE__);
  }

  return symbol_it->second;
}
mutex DataManager::mutex;
unique_ptr<DataManager> DataManager::instance;

Logger& DataManager::logger = Logger::GetLogger();