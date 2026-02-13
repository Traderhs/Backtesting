#pragma once

// BACKTESTING_EXPORTS가 정의된 빌드에서는 심볼을 DLL로 익스포트하기 위해
// __declspec(dllexport)를 사용
//
// 커스텀 전략/지표가 DLL에서 로드되도록 하려면 클래스 선언에
// BACKTESTING_API를 명시해야 함
#ifndef BACKTESTING_API
#ifdef BACKTESTING_EXPORTS
#define BACKTESTING_API __declspec(dllexport)
#else
#define BACKTESTING_API __declspec(dllimport)
#endif
#endif
