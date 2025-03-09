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
               const string& error_log_name) {
  filesystem::create_directories(log_directory);

  // Log 파일 Open, 없으면 생성
  debug_file_.open(log_directory + "/" + debug_log_name, ios::app);
  info_file_.open(log_directory + "/" + info_log_name, ios::app);
  warning_file_.open(log_directory + "/" + warning_log_name, ios::app);
  error_file_.open(log_directory + "/" + error_log_name, ios::app);
}

void Logger::Deleter::operator()(Logger* p) const {
  if (p->debug_file_.is_open()) p->debug_file_.close();
  if (p->info_file_.is_open()) p->info_file_.close();
  if (p->warning_file_.is_open()) p->warning_file_.close();
  if (p->error_file_.is_open()) p->error_file_.close();

  delete p;
}

mutex Logger::mutex_;
shared_ptr<Logger> Logger::instance_;

shared_ptr<Logger>& Logger::GetLogger(const string& log_directory,
                                      const string& debug_log_name,
                                      const string& info_log_name,
                                      const string& warning_log_name,
                                      const string& error_log_name) {
  if (!instance_) {
    lock_guard lock(mutex_);
    instance_ = shared_ptr<Logger>(
        new Logger(log_directory, debug_log_name, info_log_name,
                   warning_log_name, error_log_name),
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
      WriteToFile(debug_file_, log_message);
      break;

    case INFO_L:
      ConsoleLog("INFO_L", log_message);
      WriteToFile(info_file_, log_message);
      break;

    case ORDER_L:
      ConsoleLog("ORDER_L", log_message);
      // 주문 로그는 info 파일에 기록
      WriteToFile(info_file_, log_message);
      break;

    case WARNING_L:
      ConsoleLog("WARNING_L", log_message);
      WriteToFile(warning_file_, log_message);
      break;

    case ERROR_L:
      ConsoleLog("ERROR_L", log_message);
      WriteToFile(error_file_, log_message);
      break;
  }
}

void Logger::ConsoleLog(const string& level, const string& message) {
  if (level == "DEBUG_L") {
    cout << "\033[90m" << message << "\033[0m" << endl;  // Gray
  } else if (level == "INFO_L") {
    cout << "\033[38;2;200;200;200m" << message << "\033[0m" << endl;  // White
  } else if (level == "ORDER_L") {
    cout << "\033[32m" << message << "\033[0m" << endl;  // Green
  } else if (level == "WARNING_L") {
    cout << "\033[33m" << message << "\033[0m" << endl;  // Yellow
  } else if (level == "ERROR_L") {
    cout << "\033[31m" << message << "\033[0m" << endl;  // Red
  }
}

void Logger::WriteToFile(ofstream& file, const string& message) {
  if (file.is_open()) {
    file << message << endl;
  }
}

void Logger::LogAndThrowError(const string& message, const string& file,
                              const int line) {
  instance_->Log(ERROR_L, message, file, line);
  throw runtime_error(message);
}

}  // namespace backtesting::logger
