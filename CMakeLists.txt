# 전역 설정
cmake_minimum_required(VERSION 3.20)
project(Backtesting)

# C++ 표준 설정
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# vcpkg 툴 체인 설정
set(CMAKE_TOOLCHAIN_FILE "D:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "vcpkg toolchain file")

# 빌드 타입별 설정
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Build types" FORCE)

# 디버그 빌드 설정 (MSVC)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Od /Zi /W4 /MDd /MP")

# 릴리즈 빌드 설정 (MSVC)
# /O2: 속도 최적화, /Oi: 내장 함수 사용, /Ot: 빠른 코드 우선
# /GL: 전체 프로그램 최적화, /arch:AVX2: AVX2 SIMD 명령어
# /fp:strict: 부동소수점 정밀도, /GS-: 버퍼 보안 검사 비활성화
# /Gy: 함수 수준 링킹, /favor:AMD64: x64 최적화
# /MD: 멀티스레드 DLL 런타임
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /Oi /Ot /GL /arch:AVX2 /fp:strict /DNDEBUG /GS- /Gy /favor:AMD64 /MD /MP /bigobj /Zc:inline")

# /LTCG: 링크 타임 코드 생성, /OPT:REF: 미사용 함수 제거, /OPT:ICF: 동일 함수 병합
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG:INCREMENTAL /OPT:REF /OPT:ICF /RELEASE")

# 빌드 결과물 경로 설정
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release)

# 패키지 임포트
find_package(Arrow CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(Parquet CONFIG REQUIRED)
find_package(OpenSSL CONFIG REQUIRED)
find_package(ZLIB REQUIRED)

# ======================================================================================================================
# 절대 경로 변수
set(PRIVATE_STRATEGY_INCLUDES "D:/Programming/Backtesting-Strategies/Includes")
set(PRIVATE_STRATEGY_SOURCES "D:/Programming/Backtesting-Strategies/Sources")

# Backtesting 설정
file(GLOB HEADERS   # 해당 경로에서 hpp 파일들을 GLOB (엔진, 전략, 지표 헤더 파일들)
        "Includes/**/*.hpp"
        "${PRIVATE_STRATEGY_INCLUDES}/Strategies/*.hpp")
file(GLOB SOURCES  # 해당 경로에서 cpp 파일들을 GLOB (엔진, 전략, 지표 소스 파일들)
        "Sources/Cores/**/*.cpp"
        "${PRIVATE_STRATEGY_SOURCES}/Strategies/*.cpp")

# 실행 파일 용 Main.cpp 제외
list(FILTER SOURCES EXCLUDE REGEX "Main.cpp$")

add_library(BacktestingCore SHARED ${SOURCES})

target_compile_definitions(BacktestingCore PRIVATE BACKTESTING_EXPORTS)

target_include_directories(BacktestingCore PRIVATE # Backtesting 타겟에 포함 디렉터리 경로 추가
        ${CMAKE_SOURCE_DIR}/Includes
        ${PRIVATE_STRATEGY_INCLUDES}
        D:/vcpkg/installed/x64-windows/include)

target_link_directories(BacktestingCore PRIVATE
        D:/vcpkg/installed/x64-windows/lib)

target_link_libraries(BacktestingCore PRIVATE # Backtesting 타겟에 외부 라이브러리 링크
        arrow_shared
        CURL::libcurl
        nlohmann_json::nlohmann_json
        parquet_shared
        OpenSSL::SSL
        OpenSSL::Crypto
        ZLIB::ZLIB)

# 실행 파일 생성
add_executable(Backtesting "Sources/Cores/Engines/Main.cpp")

target_include_directories(Backtesting PRIVATE
        ${CMAKE_SOURCE_DIR}/Includes
        ${PRIVATE_STRATEGY_INCLUDES}
        D:/vcpkg/installed/x64-windows/include)

target_link_libraries(Backtesting PRIVATE
        BacktestingCore)

# 프로파일링을 위한 컴파일 옵션
#target_compile_options(Backtesting PRIVATE
#        /Zi          # 디버그 정보 생성
#        /DEBUG:FULL  # 전체 디버그 정보
#)

# 프로파일링을 위한 링커 옵션
#target_link_options(Backtesting PRIVATE
#        /PROFILE     # 프로파일링 활성화
#        /DEBUG:FULL  # 전체 디버그 정보
#)

# BacktestingCore.lib를 BackBoard/Cores 폴더로 복사 (개발 중 전략 빌드용)
add_custom_command(TARGET BacktestingCore POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/BackBoard/Cores"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_LINKER_FILE:BacktestingCore>
        "${CMAKE_SOURCE_DIR}/BackBoard/Cores/BacktestingCore.lib"
        COMMENT "Copying BacktestingCore.lib to BackBoard/Cores directory"
)
