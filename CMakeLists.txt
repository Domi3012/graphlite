cmake_minimum_required(VERSION 3.10)

project(GraphLiteEngine VERSION 0.1)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

file(GLOB_RECURSE GRAPHLITE_SOURCES "src/engine/*.cpp" "src/graph/*.cpp" "src/utils/*.cpp")

add_library(graphlite STATIC ${GRAPHLITE_SOURCES})

set_target_properties(graphlite PROPERTIES OUTPUT_NAME "graphlite-${PROJECT_VERSION}")

target_include_directories(graphlite PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(graphlite PRIVATE -O3)