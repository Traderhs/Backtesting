#pragma once

// 표준 라이브러리
#include <fstream>
#include <memory>
#include <mutex>
#include <string>

// 네임 스페이스
using namespace std;

// 로그 레벨을 지정하는 열거형 클래스
enum class LogLevel { DEBUG_L, INFO_L, WARNING_L, ERROR_L };

/// 시스템 로깅을 담당하는 클래스
class Logger final {
 public:
  // 싱글톤 특성 유지
  Logger(const Logger&) = delete;             // 복사 생성자 삭제
  Logger& operator=(const Logger&) = delete;  // 대입 연산자 삭제

  /**
   * Logger의 싱글톤 인스턴스를 반환하는 함수
   * 로그 수준에 따라 서로 다른 파일에 로그를 관리
   *
   * @param log_directory 로그 파일이 저장될 디렉터리
   * @param debug_log_name 디버그 수준 로그를 저장할 파일 이름
   * @param info_log_name 정보 수준 로그를 저장할 파일 이름
   * @param warning_log_name 경고 수준 로그를 저장할 파일 이름
   * @param error_log_name 오류 수준 로그를 저장할 파일 이름
   * @return 싱글톤 Logger 인스턴스에 대한 참조
   */
  static shared_ptr<Logger>& GetLogger(
      const string& log_directory = "../../Logs",
      const string& debug_log_name = "debug.log",
      const string& info_log_name = "info.log",
      const string& warning_log_name = "warning.log",
      const string& error_log_name = "error.log");

  /**
   * 지정된 로그 레벨, 파일, 및 라인 정보를 사용하여 메시지를 기록하는 함수
   *
   * @param log_level 로그 메시지의 레벨
   * @param message 기록할 로그 메시지
   * @param file 로그가 생성된 파일의 이름. __FILE__로 지정
   * @param line 로그 명령문이 발생한 파일의 라인 번호. __LINE__으로 지정
   */
  void Log(const LogLevel& log_level, const string& message, const string& file,
           int line);

  /**
   * 에러를 로깅하고 Throw하는 함수
   *
   * @param message 오류에 대한 설명 메시지
   * @param file
   * @param line
   */
  static void LogAndThrowError(const string& message, const string& file,
                               int line);

 private:
  // 싱글톤 인스턴스 관리
  Logger(const string& log_directory, const string& debug_log_name,
         const string& info_log_name, const string& warning_log_name,
         const string& error_log_name);
  class Deleter {
   public:
    void operator()(Logger* p) const;
  };

  static mutex mutex_;
  static shared_ptr<Logger> instance_;

  // 로그 파일
  ofstream debug_file_;
  ofstream info_file_;
  ofstream warning_file_;
  ofstream error_file_;

  /**
   * 콘솔에 로그 메시지를 출력하는 함수
   *
   * @param level 로그 레벨
   *              (예: "DEBUG_L", "INFO_L", "WARNING_L", "ERROR_L")
   * @param message 출력될 로그 메시지
   */
  static void ConsoleLog(const string& level, const string& message);

  /**
   * 지정된 파일 스트림에 로그 메시지를 기록하는 함수
   *
   * @param file 메시지를 기록할 출력 파일 스트림
   * @param message 파일에 기록할 로그 메시지
   */
  static void WriteToFile(ofstream& file, const string& message);
};
