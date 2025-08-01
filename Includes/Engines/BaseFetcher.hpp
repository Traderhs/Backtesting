#pragma once

// 표준 라이브러리
#include <future>

// 외부 라이브러리
#include "nlohmann/json_fwd.hpp"

// 전방 선언
namespace backtesting::logger {
class Logger;
}

// 네임 스페이스
using namespace std;
using namespace nlohmann;
using namespace backtesting::logger;

namespace backtesting::fetcher {

/**
 * 비동기와 HTTP를 사용하여 Fetch하는 함수를 제공하는 클래스
 */
class BaseFetcher {
 protected:
  BaseFetcher();
  ~BaseFetcher();

  /// =로 콘솔창을 분리하는 출력을 발생시키는 함수
  static void PrintSeparator();

  /**
   * 제공된 URL에서 주어진 파라미터를 사용하여 데이터를 가져오는 함수
   *
   * @param url 데이터를 가져올 대상 URL
   * @param params 요청에 포함될 파라미터의 unordered_map
   * @param need_signature 서명 필요 여부
   * @param sort_params 요청 정책에 파라미터 정렬이 필요한지 여부
   * @param header_msg 헤더 API 키 앞에 삽입할 문자열
   * @param api_key_env_var API 키를 저장한 환경 변수 이름
   * @param api_secret_env_var API 암호를 저장한 환경 변수 이름
   *
   * @return 가져온 데이터의 JSON 표현을 포함하는 future 객체
   */
  [[nodiscard]] static future<json> Fetch(
      const string& url, const unordered_map<string, string>& params = {},
      bool need_signature = false, bool sort_params = false,
      const string& header_msg = "", const string& api_key_env_var = "",
      const string& api_secret_env_var = "");

 private:
  static shared_ptr<Logger>& logger_;

  /**
   * 주어진 쿼리 매개변수를 사용하여 기본 URL에 전체 파라미터를 포함한 URL을
   * 구축하는 함수.
   *
   * need_signature가 true이면 서명을 포함한 URL을 구축하여 반환.
   *
   * @param base_url 데이터를 가져올 대상 URL
   * @param params 요청에 포함될 파라미터의 unordered_map
   * @param need_signature 서명 필요 여부
   * @param sort_params 요청 정책에 파라미터 정렬이 필요한지 여부
   * @param api_secret_env_var API 암호를 저장한 환경 변수 이름
   * @return 주어진 파라미터가 포함된 완전한 URL 문자열
   */
  static string BuildFullUrl(const string& base_url,
                             unordered_map<string, string> params,
                             bool need_signature, bool sort_params,
                             const string& api_secret_env_var);

  /// URL 파라미터를 정렬하여 쿼리 문자열을 생성하여 반환하는 함수
  template <typename T>
  static string EncodeUrl(const T& params);

  /// HMAC_SHA256으로 해싱하여 반환하는 함수
  static string HmacSha256(const string& data, const string& key);

  /**
   * HTTP 응답 데이터를 수신하는 콜백 함수
   *
   * @param contents 수신된 데이터의 포인터
   * @param size 각 데이터 블록의 크기
   * @param nmemb 데이터 블록의 개수
   * @param str 데이터를 추가할 문자열 객체에 대한 포인터
   * @return 실제로 추가된 데이터의 총 바이트 수
   */
  static size_t WriteCallback(void* contents, size_t size, size_t nmemb,
                              string* str);

  /// 헤더를 받을 콜백 함수
  static size_t HeaderCallback(void* ptr, size_t size, size_t nmemb,
                               string* data);
};

}  // namespace backtesting::fetcher