#pragma once

// 표준 라이브러리
#include <cstdint>

// 내부 헤더
#include "BarDataManager.hpp"
#include "Engine.hpp"

// 네임 스페이스
using namespace std;

/**
 * 콘솔 출력을 쉽게하기 위한 네임스페이스
 */
namespace Print {
/**
 * Bar Data를 콘솔 출력하는 함수
 */
void PrintBarData(const BarDataManager::bar_data& bar_data);

/**
 * 주어진 `bar_data_vector`에서 지정된 길이의 바 데이터 벡터를 출력하는 함수
 *
 * @param bar_data_vector 출력할 바 데이터가 저장된 벡터
 * @param length 출력할 데이터의 길이
 * @param is_reverse `is_reversed`가 `false`이면 벡터의 처음부터 지정된
 * `length`만큼 출력하고, `is_reversed`가 `true`이면 벡터의 끝에서부터 지정된 `length`만큼 출력
 */
void PrintBarDataVector(const vector<BarDataManager::bar_data>& bar_data_vector,
                        int64_t length, bool is_reverse);
};