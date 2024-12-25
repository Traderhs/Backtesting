#pragma once

// 표준 라이브러리
#include <future>

// 외부 라이브러리
#include <nlohmann/json.hpp>

// 내부 헤더
#include "Logger.hpp"

// 네임 스페이스
using namespace std;
using namespace nlohmann;

class FetchManager {
 public:
  FetchManager();
  ~FetchManager();

 protected:
  /**
   * 제공된 URL에서 주어진 파라미터를 사용하여 데이터를 가져오는 함수
   *
   * @param url 데이터를 가져올 대상 URL
   * @param params 요청에 포함될 파라미터의 unordered_map
   * @return 가져온 데이터의 JSON 표현을 포함하는 future 객체
   */
  static future<json> FetchData(const string& url,
                                const unordered_map<string, string>& params);

 private:
  static Logger& logger;

  /**
   * 주어진 쿼리 매개변수를 사용하여 기본 URL에 전체 파라미터를 포함한 URL을
   * 구축하는 함수
   *
   * @param base_url 데이터를 가져올 대상 URL
   * @param params 요청에 포함될 파라미터의 unordered_map
   * @return 주어진 파라미터가 포함된 완전한 URL 문자열
   */
  static string BuildFullUrlWithParams(
      const string& base_url, const unordered_map<string, string>& params);

  /**
   * HTTP 응답 데이터를 수신하는 콜백 함수
   *
   * @param contents 수신된 데이터의 포인터
   * @param size 각 데이터 블록의 크기
   * @param nmemb 데이터 블록의 개수
   * @param str 데이터를 추가할 문자열 객체에 대한 포인터
   * @return 실제로 추가된 데이터의 총 바이트 수
   */
  static size_t WriteCallback(void* contents, size_t size, size_t nmemb, string* str);

};