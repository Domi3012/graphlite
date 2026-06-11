cmake_minimum_required(VERSION 3.14)
project(GraphLiteEngine VERSION 1.0 LANGUAGES CXX)

# ============================================================
# USER OPTIONS
# ============================================================

option(GRAPHLITE_HEADER_ONLY      "Build as header-only (no library compilation)" OFF)
option(GRAPHLITE_BUILD_TESTS      "Build test suite"                              ON)
option(GRAPHLITE_BUILD_BENCHMARKS "Build performance benchmarks"                  OFF)

# ============================================================
# LIBRARY TARGET
# ============================================================

if(GRAPHLITE_HEADER_ONLY)
    # --- HEADER-ONLY MODE ---
    # Không compile gì — user include headers trực tiếp
    add_library(graphlite INTERFACE)
    target_include_directories(graphlite INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
    target_compile_definitions(graphlite INTERFACE GRAPHLITE_HEADER_ONLY)
    target_compile_features(graphlite INTERFACE cxx_std_17)
else()
    # --- COMPILED MODE (default, recommended) ---
    # Compile 1 lần thành libgraphlite.a, user link vào
    add_library(graphlite STATIC src/graphlite.cpp)
    target_include_directories(graphlite PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
    target_compile_features(graphlite PUBLIC cxx_std_17)

    # Platform-specific compile flags
    if(UNIX)
        target_compile_options(graphlite PRIVATE -O3 -Wall -Wextra -Wpedantic)
    elseif(MSVC)
        target_compile_options(graphlite PRIVATE /O2 /W4)
    endif()
endif()

# ============================================================
# TESTS
# ============================================================

if(GRAPHLITE_BUILD_TESTS)
    # Phase 1 test (platform, mmap, utilities)
    add_executable(test_phase1 tests/test_phase1.cpp)
    target_link_libraries(test_phase1 PRIVATE graphlite)

    # Full integration test (sẽ cập nhật ở Phase 3+)
    # add_executable(run_test tests/main.cpp)
    # target_link_libraries(run_test PRIVATE graphlite)
endif()

# ============================================================
# BENCHMARKS
# ============================================================

if(GRAPHLITE_BUILD_BENCHMARKS)
    add_executable(stress_test benchmarks/stress_test.cpp)
    target_link_libraries(stress_test PRIVATE graphlite)
endif()