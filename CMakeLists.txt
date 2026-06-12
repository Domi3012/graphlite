cmake_minimum_required(VERSION 3.14)
project(GraphLiteEngine VERSION 1.0 LANGUAGES CXX)

# ============================================================
# USER OPTIONS
# ============================================================

option(BUILD_SHARED_LIBS          "Build shared libraries"                        OFF)
option(GRAPHLITE_BUILD_TESTS      "Build test suite"                              ON)
option(GRAPHLITE_BUILD_BENCHMARKS "Build performance benchmarks"                  OFF)

# ============================================================
# LIBRARY TARGET
# ============================================================

file(GLOB_RECURSE GRAPHLITE_SOURCES "src/*.cpp")

add_library(graphlite ${GRAPHLITE_SOURCES})

target_include_directories(graphlite
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(graphlite PUBLIC cxx_std_17)

# Platform-specific compile flags
if(UNIX)
    target_compile_options(graphlite PRIVATE -O3 -Wall -Wextra -Wpedantic)
elseif(MSVC)
    target_compile_options(graphlite PRIVATE /O2 /W4)
endif()

# ============================================================
# TESTS
# ============================================================

if(GRAPHLITE_BUILD_TESTS)
    # Phase 1 test (platform, mmap, utilities)
    add_executable(test_phase1 tests/test_phase1.cpp)
    target_link_libraries(test_phase1 PRIVATE graphlite)

    # Phase 2 test (NodeStore, PageManager, persistence)
    add_executable(test_phase2 tests/test_phase2.cpp)
    target_link_libraries(test_phase2 PRIVATE graphlite)
endif()

# ============================================================
# BENCHMARKS
# ============================================================

if(GRAPHLITE_BUILD_BENCHMARKS)
    add_executable(stress_test benchmarks/stress_test.cpp)
    target_link_libraries(stress_test PRIVATE graphlite)
endif()