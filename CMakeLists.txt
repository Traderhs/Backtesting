# 전역 설정
cmake_minimum_required(VERSION 3.20)
project(Backtesting)

# C++ 표준 설정
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# vcpkg 툴 체인 설정
set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "vcpkg toolchain file")

# 빌드 타입별 설정
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Build types" FORCE)

# 디버그 빌드 설정
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Zi /Od /RTC1")
set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} /DEBUG")

# 패키지 임포트
find_package(Arrow CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(Parquet)
# ======================================================================================================================
# Backtesting 설정
file(GLOB HEADERS   # 해당 경로에서 hpp 파일들을 GLOB
        "Includes/Engines/*.hpp"
        "Includes/Indicators/*.hpp"
        "Includes/Strategies/*.hpp")
file(GLOB SOURCES  # 해당 경로에서 cpp 파일들을 GLOB
        "Sources/cpp/*.cpp"
        "Sources/cpp/Engines/*.cpp"
        "Sources/cpp/Indicators/*.cpp"
        "Sources/cpp/Strategies/*.cpp")

add_executable(Backtesting ${SOURCES}) # 실행 파일 Backtesting을 ${SOURCES} 소스 파일로 생성
target_include_directories(Backtesting PRIVATE # Backtesting 타겟에 포함 디렉터리 경로 추가
        Includes
        Sources/Cpp)
target_link_libraries(Backtesting PRIVATE # Backtesting 타겟에 외부 라이브러리 링크
        arrow_shared
        CURL::libcurl
        nlohmann_json::nlohmann_json
        parquet_shared)

# 디버그, 릴리즈 빌드별 추가 설정
target_compile_definitions(Backtesting PRIVATE
        $<$<CONFIG:Debug>:DEBUG_BUILD>
        $<$<CONFIG:Release>:RELEASE_BUILD>)

# MSVC 프로파일링 링크
target_link_options(Backtesting PRIVATE $<$<CONFIG:Release>:/PROFILE>)
# ======================================================================================================================
# Google Test 설정
file(GLOB TEST_HEADERS  # 해당 경로에서 hpp 파일들을 GLOB
        "Tests/*.hpp"
        "Includes/Engines/*.hpp"
        "Includes/Indicators/*.hpp"
        "Includes/Strategies/*.hpp")
file(GLOB TEST_SOURCES  # 해당 경로에서 cpp 파일들을 GLOB
        "Tests/*.cpp"
        "Sources/cpp/*.cpp"
        "Sources/cpp/Engines/*.cpp"
        "Sources/cpp/Indicators/*.cpp"
        "Sources/cpp/Strategies/*.cpp")

add_executable(Tests ${TEST_SOURCES}) # 실행 파일 Tests를 ${TEST_SOURCES} 소스 파일로 생성
target_include_directories(Tests PRIVATE # Tests 타겟에 포함 디렉터리 경로 추가
        Tests
        Includes
        Sources/Cpp)
target_link_libraries(Tests PRIVATE # Tests 타겟에 외부 라이브러리 연결
        arrow_shared
        CURL::libcurl
        GTest::gtest_main
        nlohmann_json::nlohmann_json
        parquet_shared)

# 디버그, 릴리즈 빌드별 추가 설정
target_compile_definitions(Tests PRIVATE
        $<$<CONFIG:Debug>:DEBUG_BUILD>
        $<$<CONFIG:Release>:RELEASE_BUILD>)

# Google Test
enable_testing()
add_test(NAME AllTests COMMAND Tests) # GTest 테스트 파일 추가
