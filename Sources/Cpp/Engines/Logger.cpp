// 표준 라이브러리
#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>

// 파일 헤더
#include "Engines/Logger.hpp"

// 내부 헤더
#include "Engines/TimeUtils.hpp"

// 네임 스페이스
using namespace backtesting::utils;

namespace backtesting::logger {

Logger::Logger(const string& log_directory, const string& debug_log_name,
               const string& info_log_name, const string& warning_log_name,
               const string& error_log_name,
               const string& backtesting_log_name) {
  filesystem::create_directories(log_directory);

  // Log 파일 Open, 없으면 생성
  debug_log_.open(log_directory + "/" + debug_log_name, ios::app);
  info_log_.open(log_directory + "/" + info_log_name, ios::app);
  warning_log_.open(log_directory + "/" + warning_log_name, ios::app);
  error_log_.open(log_directory + "/" + error_log_name, ios::app);

  // 전 백테스팅 로그가 오류로 남아있을 수도 있으므로 초기화 모드로 열기
  const string& backtesting_log_path =
      log_directory + "/" + backtesting_log_name;
  backtesting_log_.open(backtesting_log_path, ios::out | ios::trunc);
  backtesting_log_temp_path_ = backtesting_log_path;
}

void Logger::Deleter::operator()(Logger* p) const {
  if (p->debug_log_.is_open()) p->debug_log_.close();
  if (p->info_log_.is_open()) p->info_log_.close();
  if (p->warning_log_.is_open()) p->warning_log_.close();
  if (p->error_log_.is_open()) p->error_log_.close();
  if (p->backtesting_log_.is_open()) p->backtesting_log_.close();

  delete p;
}

mutex Logger::mutex_;
shared_ptr<Logger> Logger::instance_;

shared_ptr<Logger>& Logger::GetLogger(const string& log_directory,
                                      const string& debug_log_name,
                                      const string& info_log_name,
                                      const string& warning_log_name,
                                      const string& error_log_name,
                                      const string& backtesting_log_name) {
  if (!instance_) {
    lock_guard lock(mutex_);
    instance_ = shared_ptr<Logger>(
        new Logger(log_directory, debug_log_name, info_log_name,
                   warning_log_name, error_log_name, backtesting_log_name),
        Deleter());
  }
  return instance_;
}

void Logger::Log(const LogLevel& log_level, const string& message,
                 const string& file, const int line) {
  const string& log_message = format(
      "[{}] [{}:{}] | {}", GetCurrentLocalDatetime(),
      filesystem::path(file).filename().string(), to_string(line), message);

  switch (log_level) {
    case DEBUG_L:
      ConsoleLog("DEBUG_L", log_message);
      WriteToFile(debug_log_, log_message);
      break;

    case INFO_L:
      ConsoleLog("INFO_L", log_message);
      WriteToFile(info_log_, log_message);
      break;

    case WARNING_L:
      ConsoleLog("WARNING_L", log_message);
      WriteToFile(warning_log_, log_message);
      break;

    case ERROR_L:
      ConsoleLog("ERROR_L", log_message);
      WriteToFile(error_log_, log_message);
      break;
  }
}

void Logger::LogNoFormat(const LogLevel& log_level, const string& message) {
  switch (log_level) {
    case DEBUG_L:
      ConsoleLog("DEBUG_L", message);
      WriteToFile(debug_log_, message);
      break;

    case INFO_L:
      ConsoleLog("INFO_L", message);
      WriteToFile(info_log_, message);
      break;

    case WARNING_L:
      ConsoleLog("WARNING_L", message);
      WriteToFile(warning_log_, message);
      break;

    case ERROR_L:
      ConsoleLog("ERROR_L", message);
      WriteToFile(error_log_, message);
      break;
  }
}

void Logger::LogAndThrowError(const string& message, const string& file,
                              const int line) {
  instance_->Log(ERROR_L, message, file, line);
  throw runtime_error(message);
}

void Logger::SaveBacktestingLog(const string& file_path) {
  try {
    backtesting_log_.close();

    filesystem::rename(backtesting_log_temp_path_, file_path);
  } catch (const exception& e) {
    LogAndThrowError("백테스팅 로그 파일을 저장하는 데 오류가 발생했습니다.: " +
                         string(e.what()),
                     __FILE__, __LINE__);
  }
}

void Logger::ConsoleLog(const string& level, const string& message) {
  if (level == "DEBUG_L") {
    cout << "\033[90m" << message << "\033[0m" << endl;  // Gray
  } else if (level == "INFO_L") {
    cout << "\033[38;2;200;200;200m" << message << "\033[0m" << endl;  // White
  } else if (level == "WARNING_L") {
    cout << "\033[33m" << message << "\033[0m" << endl;  // Yellow
  } else if (level == "ERROR_L") {
    cout << "\033[31m" << message << "\033[0m" << endl;  // Red
  }
}

void Logger::WriteToFile(ofstream& file, const string& message) {
  file << message << endl;

  // 백테스팅 로그 파일은 백테스팅이 끝나면 바로 닫는데,
  // 저장할 때 오류가 발생하면 로그를 쓰는 데도 오류가 나기 때문에 항상 확인
  if (backtesting_log_.is_open()) {
    backtesting_log_ << message << endl;
  }
}

}  // namespace backtesting::logger
