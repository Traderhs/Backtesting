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
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Od /Zi /W4")

# 릴리즈 빌드 설정 (MSVC)
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /Oi /GL /arch:AVX2 /DNDEBUG")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG /OPT:REF /OPT:ICF")

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
# Backtesting 설정
file(GLOB HEADERS   # 해당 경로에서 hpp 파일들을 GLOB
        "Includes/Backtesting.hpp"
        "Includes/Engines/*.hpp"
        "Includes/Indicators/*.hpp"
        "Includes/Strategies/*.hpp")
file(GLOB SOURCES  # 해당 경로에서 cpp 파일들을 GLOB
        "Sources/cpp/Backtesting.cpp"
        "Sources/cpp/Engines/*.cpp"
        "Sources/cpp/Indicators/*.cpp"
        "Sources/cpp/Strategies/*.cpp")

add_executable(Backtesting ${SOURCES}) # 실행 파일 Backtesting을 ${SOURCES} 소스 파일로 생성
target_include_directories(Backtesting PRIVATE # Backtesting 타겟에 포함 디렉터리 경로 추가
        Includes
        Sources/Cpp
        D:/vcpkg/installed/x64-windows/include)
target_link_directories(Backtesting PRIVATE
        D:/vcpkg/installed/x64-windows/lib)
target_link_libraries(Backtesting PRIVATE # Backtesting 타겟에 외부 라이브러리 링크
        arrow_shared
        CURL::libcurl
        nlohmann_json::nlohmann_json
        parquet_shared
        OpenSSL::SSL
        OpenSSL::Crypto
        ZLIB::ZLIB)

# 프로파일링을 위한 컴파일 옵션
target_compile_options(Backtesting PRIVATE
        /Zi          # 디버그 정보 생성
        /DEBUG:FULL  # 전체 디버그 정보
)

# 프로파일링을 위한 링커 옵션
target_link_options(Backtesting PRIVATE
        /PROFILE     # 프로파일링 활성화
        /DEBUG:FULL  # 전체 디버그 정보
)