cmake_minimum_required(VERSION 3.25) # Minimum CMake Version required
project(sta_engine VERSION 0.1.0 LANGUAGES CXX) # Identity of the project, Only set up a C++ toolchain

set(CMAKE_CXX_STANDARD 20) # Target C++ 20 
set(CMAKE_CXX_STANDARD_REQUIRED ON) # Fail the build if compiler cannot do C++ 20, dont silently fall back on old standard
set(CMAKE_CXX_EXTENSIONS OFF) # use strict ISO C++ 20, not GNU's extended dialect gnu++20, needed for portability

find_program(CCACHE_PROGRAM ccache)
if (CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif ()

include(cmake/CompilerWarnings.cmake)

option(BUILD_TESTING "Build tests" ON)
if (BUILD_TESTING)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)
    add_executable(sta_tests
            tests/test_timing_graph.cpp
            tests/test_analyzer.cpp
            tests/test_parser.cpp)
    target_link_libraries(sta_tests PRIVATE sta_core GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main)
    include(GoogleTest)
    gtest_discover_tests(sta_tests)
endif ()

add_executable(sta_app
        src/main.cpp)

add_library(sta_core
        src/TimingGraph.cpp
        src/BenchParser.cpp
        include/sta/BenchParser.h
        src/TimingAnalyzer.cpp
        include/sta/TimingAnalyzer.h)

target_include_directories(sta_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
set_project_warnings(sta_core)

target_link_libraries(sta_app PRIVATE sta_core)

find_package(benchmark CONFIG REQUIRED)
add_executable(sta_benchmarks
        benchmarks/benchmark_sta.cpp)
target_link_libraries(sta_benchmarks PRIVATE sta_core benchmark::benchmark)

find_package(Taskflow CONFIG REQUIRED)
target_link_libraries(sta_core PUBLIC Taskflow::Taskflow)