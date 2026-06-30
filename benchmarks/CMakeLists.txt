cmake_minimum_required(VERSION 3.16)

option(BUILD_BENCHMARKS "Build benchmark suite" ON)

if(NOT BUILD_BENCHMARKS)
  return()
endif()

include(FetchContent)
FetchContent_Declare(
  nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

find_package(Threads REQUIRED)

file(GLOB BENCHMARK_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_executable(benchmark_suite ${BENCHMARK_SOURCES})

target_include_directories(benchmark_suite PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_SOURCE_DIR}/include"
)

target_link_libraries(benchmark_suite PRIVATE
  search_core_lib
  nlohmann_json::nlohmann_json
  Threads::Threads
)

target_compile_definitions(benchmark_suite PRIVATE
  BENCHMARK_DATA_DIR="${CMAKE_SOURCE_DIR}/data"
  BENCHMARK_CONFIG_DIR="${CMAKE_CURRENT_SOURCE_DIR}/config"
)

execute_process(
  COMMAND git rev-parse --short HEAD
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_COMMIT_HASH
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)
if(NOT GIT_COMMIT_HASH)
  set(GIT_COMMIT_HASH "unknown")
endif()
target_compile_definitions(benchmark_suite PRIVATE
  BENCHMARK_GIT_COMMIT="${GIT_COMMIT_HASH}"
  BENCHMARK_CXX_FLAGS="${CMAKE_CXX_FLAGS} $<$<CONFIG:Release>:${CMAKE_CXX_FLAGS_RELEASE}> $<$<CONFIG:Debug>:${CMAKE_CXX_FLAGS_DEBUG}>"
)

target_compile_features(benchmark_suite PRIVATE cxx_std_17)

set_target_properties(benchmark_suite PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/benchmarks"
)
