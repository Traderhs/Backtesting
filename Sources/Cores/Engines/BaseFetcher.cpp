// 표준 라이브러리
#include <format>
#include <iomanip>
#include <map>
#include <unordered_map>

// 외부 라이브러리
#include "curl/curl.h"
#include "nlohmann/json.hpp"
#include "openssl/evp.h"
#include "openssl/hmac.h"

// 파일 헤더
#include "Engines/BaseFetcher.hpp"

// 내부 헤더
#include "Engines/DataUtils.hpp"
#include "Engines/Logger.hpp"

// 네임 스페이스
using namespace std;
using namespace backtesting::utils;

namespace backtesting::fetcher {

BaseFetcher::BaseFetcher() = default;
BaseFetcher::~BaseFetcher() = default;

BACKTESTING_API shared_ptr<Logger>& BaseFetcher::logger_ = Logger::GetLogger();

future<json> BaseFetcher::Fetch(
    const string& url, const unordered_map<string, string>& params,
    const bool need_signature, const bool sort_params, const string& header_msg,
    const string& api_key_env_var, const string& api_secret_env_var) {
  return async(launch::async, [url, params, need_signature, sort_params,
                               header_msg, api_key_env_var,
                               api_secret_env_var] {
    CURL* curl = curl_easy_init();
    if (!curl) {
      throw runtime_error("CURL 초기화가 실패했습니다.");
    }

    string response_string;
    const string& full_url = BuildFullUrl(url, params, need_signature,
                                          sort_params, api_secret_env_var);

    curl_slist* headers = nullptr;
    string response_header;
    if (need_signature) {
      // API KEY 헤더 추가
      headers = curl_slist_append(
          headers, (header_msg + GetEnvVariable(api_key_env_var)).c_str());
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    curl_easy_setopt(curl, CURLOPT_URL, full_url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, HeaderCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response_header);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    const CURLcode& response = curl_easy_perform(curl);

    long response_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);  // 헤더 메모리 해제

    if (response != CURLE_OK || response_code != 200) {
      logger_->Log(ERROR_L, format("HTTP [{}] 응답이 실패했습니다.", full_url),
                   __FILE__, __LINE__, true);

      throw runtime_error(format("[{}] | [{}] | {}", response_header,
                                 response_code, response_string));
    }

    return json::parse(response_string);
  });
}

string BaseFetcher::BuildFullUrl(const string& base_url,
                                 unordered_map<string, string> params,
                                 const bool need_signature,
                                 const bool sort_params,
                                 const string& api_secret_env_var) {
  if (sort_params) {
    // 정렬된 맵 사용
    map sorted_params(params.begin(), params.end());

    if (need_signature) {
      sorted_params["signature"] = HmacSha256(
          EncodeUrl(sorted_params), GetEnvVariable(api_secret_env_var));
    }

    return sorted_params.empty() ? base_url
                                 : base_url + "?" + EncodeUrl(sorted_params);
  }

  // 정렬되지 않은 맵 사용
  if (need_signature) {
    // Binance에서 signature는 무조건 마지막이어야 함
    // (sort_params가 false면 마지막 유지)
    params["signature"] =
        HmacSha256(EncodeUrl(params), GetEnvVariable(api_secret_env_var));
  }

  return params.empty() ? base_url : base_url + "?" + EncodeUrl(params);
}

template <typename T>
string BaseFetcher::EncodeUrl(const T& params) {
  string encoded_str;
  for (const auto& [key, value] : params) {
    if (!encoded_str.empty()) {
      encoded_str += "&";
    }
    encoded_str += format("{}={}", key, value);
  }
  return encoded_str;
}

string BaseFetcher::HmacSha256(const string& data, const string& key) {
  array<unsigned char, 32> digest{};  // HMAC-SHA256 결과 저장용
  unsigned int md_len = 0;

  const unsigned char* result =
      HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()),
           reinterpret_cast<const unsigned char*>(data.data()),
           static_cast<int>(data.size()), digest.data(), &md_len);

  if (!result) {
    throw runtime_error("HMAC-SHA256 계산이 실패했습니다.");
  }

  ostringstream oss;
  oss << hex << setfill('0');
  for (const unsigned char byte : digest) {
    oss << setw(2) << static_cast<int>(byte);
  }

  return oss.str();
}

size_t BaseFetcher::WriteCallback(void* contents, const size_t size,
                                  const size_t nmemb, string* str) {
  const size_t new_length = size * nmemb;
  str->append(static_cast<char*>(contents), new_length);
  return new_length;
}

size_t BaseFetcher::HeaderCallback(void* ptr, const size_t size,
                                   const size_t nmemb, string* data) {
  const size_t total_size = size * nmemb;
  data->append(static_cast<char*>(ptr), total_size);
  return total_size;
}

}  // namespace backtesting::fetcher
