// Windows API 충돌 방지
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#undef byte  // Windows에서 정의된 byte 매크로 제거
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

// 표준 라이브러리
#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

// 파일 헤더
#include "Engines/Logger.hpp"

// 내부 헤더
#include "Engines/TimeUtils.hpp"

// 네임 스페이스
using namespace backtesting::utils;

namespace backtesting::logger {

// Thread-local 변수 정의
thread_local char Logger::format_cache_[2048];
thread_local char Logger::filename_cache_[256];

// 파일별 전용 버퍼 - 전역 static 변수
static FastLogBuffer debug_buffer;
static FastLogBuffer info_buffer;
static FastLogBuffer warn_buffer;
static FastLogBuffer error_buffer;
static FastLogBuffer backtesting_buffer;

// 정적 멤버 변수 정의
mutex Logger::mutex_;
shared_ptr<Logger> Logger::instance_;
string Logger::log_directory_;

// 빠른 레벨 문자열 반환 - 브랜치 예측 최적화
const char* Logger::GetLevelString(const LogLevel level) {
  switch (level) {
    case DEBUG_L: {
      return "DEBUG";
    }

    case INFO_L: {
      return "INFO";
    }

    case WARN_L: {
      return "WARN";
    }

    case ERROR_L: {
      return "ERROR";
    }

    case BALANCE_L: {
      return "BALANCE";
    }

    default: {
      return "UNKNOWN";
    }
  }
}

// 빠른 파일명 추출 - 역방향 스캔
const char* Logger::ExtractFilename(const char* filepath) {
  if (!filepath) return "";

  const char* filename = filepath;
  const char* p = filepath;

  while (*p) {
    if (*p == '/' || *p == '\\') {
      filename = p + 1;
    }
    ++p;
  }

  return filename;
}

// 고성능 메시지 포맷팅 - sprintf 대신 수동 구현
size_t Logger::FormatMessageFast(char* buffer, const LogLevel level,
                                 const char* file, const int line,
                                 const char* message) {
  const char* level_str = GetLevelString(level);
  const char* filename = ExtractFilename(file);

  char* p = buffer;

  // "[TIME] [LEVEL] [filename:line] | message" 형식
  *p++ = '[';

  // 시간 포맷팅 (최적화된 함수 사용)
  const size_t time_len = FormatCurrentTimeFast(p);
  p += time_len;

  *p++ = ']';
  *p++ = ' ';
  *p++ = '[';

  // 레벨 복사
  const char* lvl = level_str;
  while (*lvl) {
    *p++ = *lvl++;
  }

  *p++ = ']';
  *p++ = ' ';
  *p++ = '[';

  // 파일명 복사
  const char* fn = filename;
  while (*fn) {
    *p++ = *fn++;
  }

  *p++ = ':';

  // 라인 번호 변환 (역순으로 처리)
  if (line == 0) {
    *p++ = '0';
  } else {
    char temp_line[16];
    int temp_idx = 0;
    int temp_line_num = line;

    while (temp_line_num > 0) {
      temp_line[temp_idx++] = static_cast<char>('0' + temp_line_num % 10);
      temp_line_num /= 10;
    }

    // 역순으로 복사
    for (int i = temp_idx - 1; i >= 0; --i) {
      *p++ = temp_line[i];
    }
  }

  *p++ = ']';
  *p++ = ' ';
  *p++ = '|';
  *p++ = ' ';

  // 메시지 복사
  const char* msg = message;
  while (*msg) {
    *p++ = *msg++;
  }

  *p++ = '\n';
  *p = '\0';

  return p - buffer;
}

Logger::Logger(const string& debug_log_name, const string& info_log_name,
               const string& warn_log_name, const string& error_log_name,
               const string& backtesting_log_name)
    : stop_logging_(false) {
  const string& log_path = log_directory_.empty() ? "./" : log_directory_ + "/";

  debug_log_.open(log_path + debug_log_name, ios::app);
  info_log_.open(log_path + info_log_name, ios::app);
  warn_log_.open(log_path + warn_log_name, ios::app);
  error_log_.open(log_path + error_log_name, ios::app);

  const string& backtesting_log_path = log_path + backtesting_log_name;

  // 생성자는 새 프로그램 실행이므로 항상 새로 시작
  backtesting_log_.open(backtesting_log_path, ios::out | ios::trunc);
  backtesting_log_temp_path_ = backtesting_log_path;
  backtesting_log_created_in_current_session_ = true;

  // 파일 버퍼 크기 최적화 (128KB - 대용량 배치 쓰기에 최적)
  static char debug_file_buf[128 * 1024];
  static char info_file_buf[128 * 1024];
  static char warn_file_buf[128 * 1024];
  static char error_file_buf[128 * 1024];
  static char backtesting_file_buf[128 * 1024];

  debug_log_.rdbuf()->pubsetbuf(debug_file_buf, sizeof(debug_file_buf));
  info_log_.rdbuf()->pubsetbuf(info_file_buf, sizeof(info_file_buf));
  warn_log_.rdbuf()->pubsetbuf(warn_file_buf, sizeof(warn_file_buf));
  error_log_.rdbuf()->pubsetbuf(error_file_buf, sizeof(error_file_buf));
  backtesting_log_.rdbuf()->pubsetbuf(backtesting_file_buf,
                                      sizeof(backtesting_file_buf));

  // 고성능 비동기 스레드 시작
  logging_thread_ = thread(&Logger::ProcessMultiBuffer, this);

  // 스레드 우선순위 최적화 (백그라운드 작업이므로 낮은 우선순위)
#ifdef _WIN32
  SetThreadPriority(logging_thread_.native_handle(),
                    THREAD_PRIORITY_BELOW_NORMAL);
#else
  sched_param sch_params;
  sch_params.sched_priority = sched_get_priority_min(SCHED_IDLE);
  pthread_setschedparam(logging_thread_.native_handle(), SCHED_IDLE,
                        &sch_params);
#endif
}

Logger::~Logger() {
  stop_logging_ = true;
  FlushAllBuffers();
}

void Logger::Deleter::operator()(Logger* p) const {
  if (p) {
    p->stop_logging_ = true;
    if (p->logging_thread_.joinable()) {
      p->logging_thread_.join();
    }

    p->FlushAllBuffers();
    if (p->debug_log_.is_open()) {
      p->debug_log_.close();
    }

    if (p->info_log_.is_open()) {
      p->info_log_.close();
    }

    if (p->warn_log_.is_open()) {
      p->warn_log_.close();
    }

    if (p->error_log_.is_open()) {
      p->error_log_.close();
    }

    if (p->backtesting_log_.is_open()) {
      p->backtesting_log_.close();
    }

    delete p;
  }
}

void Logger::SetLogDirectory(const string& log_directory) {
  try {
    if (!filesystem::exists(log_directory)) {
      filesystem::create_directories(log_directory);
    }

    // 이전에 생성된 로거 인스턴스가 있으면 파일을 닫고
    // 현재 디렉토리의 로그 파일들을 새 디렉토리로 이동
    if (instance_) {
      // 파일을 전부 닫음
      if (instance_->debug_log_.is_open()) {
        instance_->debug_log_.close();
      }

      if (instance_->info_log_.is_open()) {
        instance_->info_log_.close();
      }

      if (instance_->warn_log_.is_open()) {
        instance_->warn_log_.close();
      }

      if (instance_->error_log_.is_open()) {
        instance_->error_log_.close();
      }

      if (instance_->backtesting_log_.is_open()) {
        instance_->backtesting_log_.close();
      }

      // 기본 파일 이름 설정
      const string& debug_name = "debug.log";
      const string& info_name = "info.log";
      const string& warn_name = "warn.log";
      const string& error_name = "error.log";
      const string& backtesting_name = "backtesting.log";

      // 기존 파일 이동 (현재 디렉토리에 있는 로그 파일을 새 디렉토리로 이동)
      const vector log_files = {debug_name, info_name, warn_name, error_name,
                                backtesting_name};

      for (const auto& file : log_files) {
        if (filesystem::exists("./" + file)) {
          try {
            filesystem::rename("./" + file,
                               format("{}/{}", log_directory, file));
          } catch (...) {
            // 이동 실패 시 기존 파일에 내용 추가 후 현재 파일 삭제

            // 기존 파일이 있다면 현재 파일 내용을 끝에 추가
            if (const string& target_path =
                    format("{}/{}", log_directory, file);
                filesystem::exists(target_path)) {
              ofstream target_file(target_path, ios::app);
              ifstream source_file("./" + file);

              target_file << source_file.rdbuf();
              target_file.close();
              source_file.close();
            } else {
              // 기존 파일이 없다면 단순 복사
              filesystem::copy_file("./" + file, target_path);
            }

            filesystem::remove("./" + file);
          }
        }
      }

      // 새 위치에 파일 다시 열기
      instance_->debug_log_.open(log_directory + "/" + debug_name, ios::app);
      instance_->info_log_.open(log_directory + "/" + info_name, ios::app);
      instance_->warn_log_.open(log_directory + "/" + warn_name, ios::app);
      instance_->error_log_.open(log_directory + "/" + error_name, ios::app);

      // 파일 버퍼 크기 재설정
      instance_->debug_log_.rdbuf()->pubsetbuf(nullptr, 0);
      instance_->info_log_.rdbuf()->pubsetbuf(nullptr, 0);
      instance_->warn_log_.rdbuf()->pubsetbuf(nullptr, 0);
      instance_->error_log_.rdbuf()->pubsetbuf(nullptr, 0);

      // 백테스팅 로그 경로 업데이트
      instance_->backtesting_log_temp_path_ =
          log_directory + "/" + backtesting_name;

      // 현재 세션에서 생성된 로그라면 내용 보존, 아니라면 새로 시작
      const ios::openmode mode =
          instance_->backtesting_log_created_in_current_session_
              ? ios::app
              : ios::out | ios::trunc;

      instance_->backtesting_log_.open(instance_->backtesting_log_temp_path_,
                                       mode);
      instance_->backtesting_log_.rdbuf()->pubsetbuf(nullptr, 0);

      // 이제 현재 세션에서 관리되는 것으로 표시
      instance_->backtesting_log_created_in_current_session_ = true;
    }
  } catch (const exception& e) {
    LogAndThrowError(e.what(), __FILE__, __LINE__);
  }

  log_directory_ = log_directory;
}

shared_ptr<Logger>& Logger::GetLogger(const string& debug_log_name,
                                      const string& info_log_name,
                                      const string& warn_log_name,
                                      const string& error_log_name,
                                      const string& backtesting_log_name) {
  if (!instance_) {
    lock_guard lock(mutex_);
    instance_ = shared_ptr<Logger>(
        new Logger(debug_log_name, info_log_name, warn_log_name, error_log_name,
                   backtesting_log_name),
        Deleter());
  }
  return instance_;
}

// 하드웨어 레벨 최적화된 백그라운드 처리 스레드
NO_INLINE void Logger::ProcessMultiBuffer() {
  int consecutive_idle_count = 0;

  while (!stop_logging_) {
    bool any_work = false;

    // 각 버퍼를 순회하면서 플러시할 데이터가 있는지 확인
    PREFETCH_READ(&info_buffer);
    any_work |= FlushBufferIfReady(debug_buffer, debug_log_);

    PREFETCH_READ(&warn_buffer);
    any_work |= FlushBufferIfReady(info_buffer, info_log_);

    PREFETCH_READ(&error_buffer);
    any_work |= FlushBufferIfReady(warn_buffer, warn_log_);

    PREFETCH_READ(&backtesting_buffer);
    any_work |= FlushBufferIfReady(error_buffer, error_log_);

    any_work |= FlushBufferIfReady(backtesting_buffer, backtesting_log_);

    // 작업이 없으면 짧은 Sleep (조건 변수보다 빠름)
    if (!any_work) {
      consecutive_idle_count++;
#ifdef _WIN32
      // Windows: 고정밀 Sleep
      Sleep(0);  // 즉시 양보만
#else
      usleep(sleep_time);
#endif
    } else {
      consecutive_idle_count = 0;  // 작업이 있으면 리셋
    }
  }

  FlushAllBuffers();
}

bool Logger::FlushBufferIfReady(FastLogBuffer& buffer, ofstream& file) {
  bool did_work = false;

  for (auto& buf : buffer.buffers) {
    // ready_to_flush 플래그가 설정되었거나, 버퍼가 충분히 찼을 때 플러시
    const size_t data_size = buf.write_pos.load(memory_order_acquire);
    const bool should_flush = buf.ready_to_flush.load(memory_order_acquire) ||
                              (data_size >= FastLogBuffer::flush_threshold);

    if (should_flush && data_size > 0) {
      // 배치 쓰기 최적화 - flush 호출 최소화
      file.write(buf.data, static_cast<streamsize>(data_size));
      did_work = true;
      buf.reset();
    }
  }

  // 모든 버퍼 처리 후 한 번만 flush
  if (did_work) {
    file.flush();
  }

  return did_work;
}

void Logger::FlushAllBuffers() {
  for (size_t i = 0; i < FastLogBuffer::max_buffers; ++i) {
    FlushBuffer(debug_buffer, i, debug_log_);
    FlushBuffer(info_buffer, i, info_log_);
    FlushBuffer(warn_buffer, i, warn_log_);
    FlushBuffer(error_buffer, i, error_log_);
    FlushBuffer(backtesting_buffer, i, backtesting_log_);
  }
}

void Logger::ForceFlushAll() {
  ForceFlushBuffer(debug_buffer, debug_log_);
  ForceFlushBuffer(info_buffer, info_log_);
  ForceFlushBuffer(warn_buffer, warn_log_);
  ForceFlushBuffer(error_buffer, error_log_);
  ForceFlushBuffer(backtesting_buffer, backtesting_log_);
}

void Logger::ForceFlushBuffer(FastLogBuffer& buffer, ofstream& file) {
  size_t current_buf_idx = buffer.current_buffer.load(memory_order_acquire);
  auto& buf = buffer.buffers[current_buf_idx];
  if (const size_t data_size = buf.write_pos.load(memory_order_acquire);
      data_size > 0) {
    file.write(buf.data, static_cast<streamsize>(data_size));
    file.flush();

    if (const size_t next_buf =
            (current_buf_idx + 1) % FastLogBuffer::max_buffers;
        buffer.current_buffer.compare_exchange_strong(current_buf_idx, next_buf,
                                                      memory_order_release)) {
      buf.reset();
    }
  }
}

void Logger::FlushBuffer(FastLogBuffer& buffer, const size_t buffer_idx,
                         ofstream& file) {
  auto& buf = buffer.buffers[buffer_idx];
  if (const size_t data_size = buf.write_pos.load(memory_order_acquire);
      data_size > 0) {
    file.write(buf.data, static_cast<streamsize>(data_size));
    file.flush();
    buf.reset();
  }
}

FORCE_INLINE void Logger::Log(const LogLevel& log_level, const string& message,
                              const string& file, const int line,
                              const bool log_to_console) {
  // 프리페치로 format_cache 미리 로드
  PREFETCH_WRITE(format_cache_);

  // 메시지 포맷팅 (thread-local 캐시 사용)
  const size_t msg_len = FormatMessageFast(format_cache_, log_level,
                                           file.c_str(), line, message.c_str());

  // 콘솔 출력 (필요한 경우만)
  if (UNLIKELY(log_to_console)) {
    // 브랜치 예측 최적화된 레벨 문자열 선택
    const char* level_str;
    switch (log_level) {
      case INFO_L: {
        level_str = "INFO_L";
        break;
      }

      case DEBUG_L: {
        level_str = "DEBUG_L";
        break;
      }

      case WARN_L: {
        level_str = "WARN_L";
        break;
      }

      case ERROR_L: {
        level_str = "ERROR_L";
        break;
      }

      case BALANCE_L: {
        level_str = "BALANCE_L";
        break;
      }

      default: {
        level_str = "INFO_L";
        break;
      }
    }

    format_cache_[msg_len - 1] = '\0';  // \n 제거
    ConsoleLog(level_str, format_cache_);
    format_cache_[msg_len - 1] = '\n';  // 복원
  }

  // 멀티 버퍼에 한 번만 쓰기 (로그 레벨 + 백테스팅 로그 동시 처리)
  WriteToBuffersFast(log_level, format_cache_, msg_len);
}

void Logger::LogNoFormat(const LogLevel& log_level, const string& message,
                         const bool log_to_console) {
  // 콘솔 출력 (필요한 경우만)
  if (UNLIKELY(log_to_console)) {
    // 브랜치 예측 최적화된 레벨 문자열 선택
    const char* level_str;
    switch (log_level) {
      case INFO_L:
        level_str = "INFO_L";
        break;
      case DEBUG_L:
        level_str = "DEBUG_L";
        break;
      case WARN_L:
        level_str = "WARN_L";
        break;
      case ERROR_L:
        level_str = "ERROR_L";
        break;
      case BALANCE_L:
        level_str = "BALANCE_L";
        break;
      default:
        level_str = "INFO_L";
        break;
    }
    ConsoleLog(level_str, message);
  }

  // 개행 문자 추가 (최적화된 string 생성)
  const string formatted_message = message + "\n";

  // 멀티 버퍼에 한 번만 쓰기 (로그 레벨 + 백테스팅 로그 동시 처리)
  WriteToBuffersFast(log_level, formatted_message.c_str(),
                     formatted_message.length());
}

// 하드웨어 레벨 최적화된 극한 성능 쓰기 함수
FORCE_INLINE void Logger::WriteToBuffersFast(const LogLevel log_level,
                                             const char* RESTRICT data,
                                             const size_t len) {
  // 프리페치로 버퍼 메타데이터 미리 로드 (브랜치 예측 최적화)
  FastLogBuffer* target_buffer;
  switch (log_level) {
    case INFO_L:  // 가장 자주 사용되는 케이스를 맨 위로
      [[fallthrough]];
    case BALANCE_L: {
      target_buffer = &info_buffer;  // BALANCE는 INFO와 같은 버퍼 사용
      PREFETCH_WRITE(target_buffer);
      break;
    }

    case WARN_L: {
      target_buffer = &warn_buffer;
      PREFETCH_WRITE(target_buffer);
      break;
    }

    case ERROR_L: {
      target_buffer = &error_buffer;
      PREFETCH_WRITE(target_buffer);
      break;
    }

    case DEBUG_L: {
      target_buffer = &debug_buffer;
      PREFETCH_WRITE(target_buffer);
      break;
    }

    default: {
      target_buffer = &info_buffer;  // 기본값
      PREFETCH_WRITE(target_buffer);
      break;
    }
  }

  // 백테스팅 버퍼 프리페치
  PREFETCH_WRITE(&backtesting_buffer);

  // 레벨별 버퍼에 쓰기 (가장 빠른 경로)
  const bool level_written = target_buffer->write_message(data, len);

  // 백테스팅 로그에 쓰기 (동시에 처리)
  const bool backtesting_written =
      backtesting_log_.is_open() && backtesting_buffer.write_message(data, len);

  // 극한 상황에서만 직접 파일 쓰기 (매우 드물어야 함)
  if (UNLIKELY(!level_written)) {
    switch (log_level) {
      case INFO_L:
        [[fallthrough]];
      case BALANCE_L: {
        info_log_.write(
            data, static_cast<streamsize>(len));  // BALANCE는 INFO 로그에 쓰기
        break;
      }

      case WARN_L: {
        warn_log_.write(data, static_cast<streamsize>(len));
        break;
      }

      case ERROR_L: {
        error_log_.write(data, static_cast<streamsize>(len));
        break;
      }

      case DEBUG_L: {
        debug_log_.write(data, static_cast<streamsize>(len));
        break;
      }
    }
  }

  if (UNLIKELY(!backtesting_written && backtesting_log_.is_open())) {
    backtesting_log_.write(data, static_cast<streamsize>(len));
  }
}

void Logger::LogAndThrowError(const string& message, const string& file,
                              const int line) {
  instance_->Log(ERROR_L, message, file, line, true);
  throw runtime_error(message);
}

void Logger::ConsoleLog(const string& level, const string& message) {
  if (level == "BALANCE_L" || level == "DEBUG_L") {
    cout << "\033[90m" << message << "\033[0m" << endl;  // Gray
  } else if (level == "INFO_L") {
    cout << "\033[38;2;200;200;200m" << message << "\033[0m" << endl;  // White
  } else if (level == "WARN_L") {
    cout << "\033[33m" << message << "\033[0m" << endl;  // Yellow
  } else if (level == "ERROR_L") {
    cout << "\033[31m" << message << "\033[0m" << endl;  // Red
  }
}

}  // namespace backtesting::logger
