// 표준 라이브러리
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <mutex>

// 내부 헤더
#include "Engines/Engine.hpp"
#include "Engines/TimeUtils.hpp"

// 파일 헤더
#include "Engines/Logger.hpp"

Logger::Logger(const string& log_directory, const string& debug_log_name,
               const string& info_log_name, const string& warning_log_name,
               const string& error_log_name) {
  filesystem::create_directories(log_directory);

  // Log 파일 Open, 없으면 생성
  debug_file.open(log_directory + "/" + debug_log_name, ios::app);
  info_file.open(log_directory + "/" + info_log_name, ios::app);
  warning_file.open(log_directory + "/" + warning_log_name, ios::app);
  error_file.open(log_directory + "/" + error_log_name, ios::app);
}

Logger::~Logger() {
  if (debug_file.is_open()) debug_file.close();
  if (info_file.is_open()) info_file.close();
  if (warning_file.is_open()) warning_file.close();
  if (error_file.is_open()) error_file.close();
}

mutex Logger::mutex;
unique_ptr<Logger> Logger::instance;

Logger& Logger::GetLogger(const string& log_directory,
                          const string& debug_log_name,
                          const string& info_log_name,
                          const string& warning_log_name,
                          const string& error_log_name) {
  if (!instance) {
    lock_guard lock(mutex);
    instance.reset(new Logger(log_directory, debug_log_name, info_log_name,
                              warning_log_name, error_log_name));
  }
  return *instance;
}

void Logger::Log(const LogLevel& log_level, const string& message,
                 const string& file, const int line) {
  const string& log_message = format(
      "[{}] [{}:{}] | {}", TimeUtils::GetCurrentLocalDatetime(),
      filesystem::path(file).filename().string(), to_string(line), message);

  switch (log_level) {
    case DEBUG_L:
      ConsoleLog("DEBUG_L", log_message);
      WriteToFile(debug_file, log_message);
      break;

    case INFO_L:
      ConsoleLog("INFO_L", log_message);
      WriteToFile(info_file, log_message);
      break;

    case WARNING_L:
      ConsoleLog("WARNING_L", log_message);
      WriteToFile(warning_file, log_message);
      break;

    case ERROR_L:
      ConsoleLog("ERROR_L", log_message);
      WriteToFile(error_file, log_message);
      break;
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
  if (file.is_open()) {
    file << message << endl;
  }
}

void Logger::LogAndThrowError(const string& message, const string& file,
                              const int line) {
  logger.Log(ERROR_L, message, file, line);
  throw runtime_error(message);
}

Logger& Logger::logger = GetLogger();
