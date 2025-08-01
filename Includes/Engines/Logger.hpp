#pragma once

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
#endif

// 표준 라이브러리
#include <atomic>
#include <chrono>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

// 전방 선언
namespace backtesting::analyzer {
class Analyzer;
}

// 네임 스페이스
using namespace std;

namespace backtesting::logger {

// 하드웨어 레벨 최적화 매크로
#ifdef _WIN32
#define PREFETCH_READ(addr) _mm_prefetch((const char*)(addr), _MM_HINT_T0)
#define PREFETCH_WRITE(addr) _mm_prefetch((const char*)(addr), _MM_HINT_T1)
#define MEMORY_BARRIER() _ReadWriteBarrier()
#define CPU_RELAX() _mm_pause()
#else
#include <x86intrin.h>
#define PREFETCH_READ(addr) __builtin_prefetch(addr, 0, 3)
#define PREFETCH_WRITE(addr) __builtin_prefetch(addr, 1, 3)
#define MEMORY_BARRIER() __sync_synchronize()
#define CPU_RELAX() __builtin_ia32_pause()
#endif

// 컴파일러 힌트 매크로 (극한 최적화)
#ifdef __GNUC__
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#define FORCE_INLINE __attribute__((always_inline)) inline
#define NO_INLINE __attribute__((noinline))
#define RESTRICT __restrict__
#else
#define LIKELY(x) (x)
#define UNLIKELY(x) (x)
#define FORCE_INLINE inline
#define NO_INLINE
#define RESTRICT
#endif

/// 로그 레벨을 지정하는 열거형 클래스
enum class LogLevel { DEBUG_L, INFO_L, WARNING_L, ERROR_L };
using enum LogLevel;

/**
 * 하드웨어 레벨 최적화된 고성능 비동기 로깅 버퍼
 * 멀티 버퍼링과 캐시 라인 정렬을 통한 극한 성능 최적화
 */
struct alignas(64) FastLogBuffer {
  static constexpr size_t buffer_size =
      4 * 1024 * 1024;                      // 4MB 버퍼 (L3 캐시 고려)
  static constexpr size_t max_buffers = 2;  // 더블 버퍼링

  /**
   * 개별 버퍼 구조체 - 캐시 라인 정렬 및 하드웨어 최적화
   */
  struct alignas(64) Buffer {
    char data[buffer_size];
    alignas(64) atomic<size_t> write_pos{0};
    alignas(64) atomic<bool> ready_to_flush{false};
    char padding[64];  // 다음 캐시 라인과 분리

    /**
     * 하드웨어 레벨 최적화된 쓰기 함수
     * @param msg 쓰기할 메시지 데이터
     * @param len 메시지 길이
     * @return 성공 여부
     */
    [[nodiscard]] FORCE_INLINE bool try_write(const char* RESTRICT msg,
                                              const size_t len) noexcept {
      PREFETCH_WRITE(this);

      if (const size_t current_pos = write_pos.load(memory_order_relaxed);
          LIKELY(current_pos + len <= buffer_size)) {
        PREFETCH_WRITE(data + current_pos);
        if (LIKELY(write_pos.compare_exchange_strong(
                const_cast<size_t&>(current_pos), current_pos + len,
                memory_order_acq_rel))) {
#ifdef _WIN32
          __movsb(reinterpret_cast<unsigned char*>(data + current_pos),
                  reinterpret_cast<const unsigned char*>(msg), len);
#else
          __builtin_memcpy(data + current_pos, msg, len);
#endif
          MEMORY_BARRIER();
          return true;
        }
      }

      CPU_RELAX();
      return false;
    }

    /**
     * 버퍼 리셋 함수
     */
    FORCE_INLINE void reset() noexcept {
      write_pos.store(0, memory_order_relaxed);
      ready_to_flush.store(false, memory_order_relaxed);
    }
  };

  alignas(64) Buffer buffers[max_buffers];
  alignas(64) atomic<size_t> current_buffer{0};
  alignas(64) atomic<size_t> flush_buffer{0};
  alignas(64) atomic<bool> stop_flag{false};
  char padding[64];  // 캐시 라인 분리

  /**
   * 하드웨어 레벨 최적화된 메시지 쓰기 함수
   * @param msg 쓰기할 메시지 데이터
   * @param len 메시지 길이
   * @return 성공 여부
   */
  [[nodiscard]] FORCE_INLINE bool write_message(const char* RESTRICT msg,
                                                const size_t len) noexcept {
    PREFETCH_READ(this);
    const size_t buf_idx = current_buffer.load(memory_order_relaxed);
    Buffer& buffer = buffers[buf_idx];

    PREFETCH_READ(&buffer);

    if (LIKELY(buffer.try_write(msg, len))) {
      return true;
    }

    if (const size_t next_buf = (buf_idx + 1) % max_buffers;
        UNLIKELY(current_buffer.compare_exchange_strong(
            const_cast<size_t&>(buf_idx), next_buf, memory_order_acq_rel))) {
      buffer.ready_to_flush.store(true, memory_order_release);
      PREFETCH_WRITE(&buffers[next_buf]);
    }

    const size_t new_buf_idx = current_buffer.load(memory_order_relaxed);
    return buffers[new_buf_idx].try_write(msg, len);
  }
};

/**
 * 시스템 로깅을 담당하는 클래스
 * 싱글톤 패턴을 사용하여 전역 로깅을 관리
 * 하드웨어 레벨 최적화된 비동기 로깅 시스템
 */
class Logger final {
  friend class backtesting::analyzer::Analyzer;

 public:
  // 싱글톤 특성 유지
  Logger(const Logger&) = delete;             // 복사 생성자 삭제
  Logger& operator=(const Logger&) = delete;  // 대입 연산자 삭제

  /**
   * 로그 파일이 저장될 폴더를 설정하는 함수
   * @param log_directory 로그 파일들이 저장될 디렉터리 경로
   */
  static void SetLogDirectory(const string& log_directory);

  /**
   * Logger의 싱글톤 인스턴스를 반환하는 함수
   * 로그 수준에 따라 서로 다른 파일에 로그를 관리
   * @param debug_log_name 디버그 수준 로그를 저장할 파일 이름
   * @param info_log_name 정보 수준 로그를 저장할 파일 이름
   * @param warning_log_name 경고 수준 로그를 저장할 파일 이름
   * @param error_log_name 오류 수준 로그를 저장할 파일 이름
   * @param backtesting_log_name 각 백테스팅 폴더에 로그를 저장할 파일 이름
   * @return 싱글톤 Logger 인스턴스에 대한 참조
   */
  static shared_ptr<Logger>& GetLogger(
      const string& debug_log_name = "debug.log",
      const string& info_log_name = "info.log",
      const string& warning_log_name = "warning.log",
      const string& error_log_name = "error.log",
      const string& backtesting_log_name = "backtesting.log");

  /**
   * 지정된 로그 레벨과 파일 및 라인 정보를 사용하여 메시지를 기록하는 함수
   * @param log_level 로그 메시지의 레벨
   * @param message 기록할 로그 메시지
   * @param file 로그가 생성된 파일의 이름. __FILE__로 지정
   * @param line 로그 명령문이 발생한 파일의 라인 번호. __LINE__으로 지정
   * @param log_to_console 콘솔에 로그를 출력할지 결정하는 플래그
   */
  void Log(const LogLevel& log_level, const string& message, const string& file,
           int line, bool log_to_console = false);

  /**
   * 포맷 없이 로그를 기록하는 함수
   * @param log_level 로그 메시지의 레벨
   * @param message 기록할 로그 메시지
   * @param log_to_console 콘솔에 로그를 출력할지 결정하는 플래그
   */
  void LogNoFormat(const LogLevel& log_level, const string& message,
                   bool log_to_console = false);

  /**
   * 에러를 로깅하고 Throw하는 함수
   * @param message 오류에 대한 설명 메시지
   * @param file __FILE__로 지정
   * @param line __LINE__으로 지정
   */
  static void LogAndThrowError(const string& message, const string& file,
                               int line);

  /**
   * 로거 소멸자 - 백그라운드 쓰레드 정리
   */
  ~Logger();

 private:
  /**
   * 싱글톤 인스턴스를 생성하는 private 생성자
   * @param debug_log_name 디버그 로그 파일명
   * @param info_log_name 정보 로그 파일명
   * @param warning_log_name 경고 로그 파일명
   * @param error_log_name 오류 로그 파일명
   * @param backtesting_log_name 백테스팅 로그 파일명
   */
  Logger(const string& debug_log_name, const string& info_log_name,
         const string& warning_log_name, const string& error_log_name,
         const string& backtesting_log_name);

  /**
   * 싱글톤 인스턴스 안전한 삭제를 위한 Deleter 클래스
   */
  class Deleter {
   public:
    void operator()(Logger* p) const;
  };

  // 싱글톤 관리 멤버
  static mutex mutex_;
  static shared_ptr<Logger> instance_;
  static string log_directory_;  // 로그 파일이 저장되는 경로

  // 백테스팅 로그 경로
  string backtesting_log_temp_path_;

  // 로그 파일 스트림
  ofstream debug_log_;
  ofstream info_log_;
  ofstream warning_log_;
  ofstream error_log_;
  ofstream backtesting_log_;

  // 백그라운드 스레드 관리
  atomic<bool> stop_logging_{false};
  thread logging_thread_;

  // 고성능 쓰기를 위한 내부 구조체
  struct LogMessage {
    LogLevel level;
    string message;
    string file;
    int line;
    bool log_to_console;
    bool is_formatted;

    LogMessage(const LogLevel l, const string&& msg, const string&& f,
               const int ln, const bool console, const bool fmt)
        : level(l),
          message(msg),
          file(f),
          line(ln),
          log_to_console(console),
          is_formatted(fmt) {}
  };

  // 스레드 로컬 캐시
  thread_local static char format_cache_[2048];  // 시간 추가로 인한 크기 증가
  thread_local static char filename_cache_[256];

  // 핵심 성능 최적화 함수들
  /**
   * 멀티 버퍼를 처리하는 백그라운드 스레드 함수
   */
  void ProcessMultiBuffer();

  /**
   * 하드웨어 레벨 최적화된 버퍼 쓰기 함수
   * @param log_level 로그 레벨
   * @param data 쓰기할 데이터
   * @param len 데이터 길이
   */
  void WriteToBuffersFast(LogLevel log_level, const char* data, size_t len);

  /**
   * 버퍼가 플러시 준비되었는지 확인하고 플러시하는 함수
   * @param buffer 확인할 버퍼
   * @param file 쓰기할 파일 스트림
   * @return 플러시 작업 수행 여부
   */
  static bool FlushBufferIfReady(FastLogBuffer& buffer, ofstream& file);

  /**
   * 모든 버퍼를 플러시하는 함수
   */
  void FlushAllBuffers();

  /**
   * 강제로 모든 버퍼를 플러시하는 함수
   */
  void ForceFlushAll();

  /**
   * 특정 버퍼를 강제로 플러시하는 함수
   * @param buffer 플러시할 버퍼
   * @param file 쓰기할 파일 스트림
   */
  static void ForceFlushBuffer(FastLogBuffer& buffer, ofstream& file);

  /**
   * 지정된 버퍼 인덱스의 버퍼를 플러시하는 함수
   * @param buffer 플러시할 버퍼
   * @param buffer_idx 버퍼 인덱스
   * @param file 쓰기할 파일 스트림
   */
  static void FlushBuffer(FastLogBuffer& buffer, size_t buffer_idx,
                          ofstream& file);

  // 정적 유틸리티 함수들
  /**
   * 빠른 메시지 포맷팅 함수
   * @param buffer 포맷팅 결과를 저장할 버퍼
   * @param level 로그 레벨
   * @param file 파일명
   * @param line 라인 번호
   * @param message 메시지
   * @return 포맷팅된 메시지 길이
   */
  static size_t FormatMessageFast(char* buffer, LogLevel level,
                                  const char* file, int line,
                                  const char* message);

  /**
   * 로그 레벨을 문자열로 변환하는 함수
   * @param level 로그 레벨
   * @return 레벨 문자열
   */
  static const char* GetLevelString(LogLevel level);

  /**
   * 파일 경로에서 파일명만 추출하는 함수
   * @param filepath 전체 파일 경로
   * @return 파일명
   */
  static const char* ExtractFilename(const char* filepath);

  /**
   * 콘솔에 로그 메시지를 출력하는 함수
   * @param level 로그 레벨 문자열
   * @param message 출력될 로그 메시지
   */
  static void ConsoleLog(const string& level, const string& message);
};

}  // namespace backtesting::logger
