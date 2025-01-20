// 외부 라이브러리
#include <curl/curl.h>

// 파일 헤더
#include "Engines/BaseFetcher.hpp"

BaseFetcher::BaseFetcher() = default;
BaseFetcher::~BaseFetcher() = default;

shared_ptr<Logger>& BaseFetcher::logger_ = Logger::GetLogger();

future<json> BaseFetcher::Fetch(const string& url,
                                const unordered_map<string, string>& params) {
  return async(launch::async, [url, params]() {
    CURL* curl = curl_easy_init();
    if (!curl)
      Logger::LogAndThrowError("CURL 초기화가 실패했습니다.", __FILE__,
                               __LINE__);

    string response_string;
    const string& full_url = BuildFullUrl(url, params);

    curl_easy_setopt(curl, CURLOPT_URL, full_url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    const CURLcode& response = curl_easy_perform(curl);

    long response_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    curl_easy_cleanup(curl);

    if (response != CURLE_OK) {
      Logger::LogAndThrowError(
          "CURL 요청이 실패했습니다.: " + string(curl_easy_strerror(response)),
          __FILE__, __LINE__);
    }

    if (response_code != 200) {
      Logger::LogAndThrowError(
          "HTTP 응답이 실패했습니다.: " + to_string(response_code), __FILE__,
          __LINE__);
    }

    return json::parse(response_string);
  });
}

string BaseFetcher::BuildFullUrl(const string& base_url,
                                 const unordered_map<string, string>& params) {
  string full_url = base_url + "?";
  bool first = true;

  for (const auto& [key, value] : params) {
    if (!first) full_url += "&";

    full_url += key + "=" +
                curl_easy_escape(nullptr, value.c_str(),
                                 static_cast<int>(value.length()));
    first = false;
  }

  return full_url;
}

size_t BaseFetcher::WriteCallback(void* contents, const size_t size,
                                  const size_t nmemb, string* str) {
  const size_t new_length = size * nmemb;
  str->append(static_cast<char*>(contents), new_length);
  return new_length;
}