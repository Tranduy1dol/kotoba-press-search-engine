cmake_minimum_required(VERSION 3.16)
project(mini-search-engine VERSION 0.1.0 LANGUAGES CXX)
set(DCMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(
  -Wall
  -Wextra
  -Wpedantic
  -Wshadow
  -Wno-unused-parameter
)

include(FetchContent)
FetchContent_Declare(
  lexbor
  GIT_REPOSITORY https://github.com/lexbor/lexbor.git
  GIT_TAG v2.3.0
)
set(LEXBOR_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LEXBOR_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(lexbor)

file(GLOB_RECURSE SEARCH_CORE_SOURCES
  "${CMAKE_SOURCE_DIR}/src/*.cpp"
)
list(FILTER SEARCH_CORE_SOURCES EXCLUDE REGEX ".*/server/.*")

add_library(search_core_lib STATIC ${SEARCH_CORE_SOURCES})

find_package(Protobuf REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GRPC REQUIRED grpc++ grpc)

add_library(grpc_service STATIC
  generated/grpc_service/v1/search.pb.cc
  generated/grpc_service/v1/search.grpc.pb.cc
)
target_include_directories(grpc_service PUBLIC "${CMAKE_SOURCE_DIR}/generated")
target_link_libraries(grpc_service PUBLIC
  protobuf::libprotobuf
  ${GRPC_LIBRARIES}
)
target_include_directories(grpc_service PUBLIC ${GRPC_INCLUDE_DIRS})

add_library(search_lib STATIC
  "${CMAKE_SOURCE_DIR}/src/server/search_service.cpp"
)
target_link_libraries(search_lib PUBLIC search_core_lib grpc_service)

add_executable(search_server app/server_main.cpp)
target_link_libraries(search_server PRIVATE search_lib)
target_include_directories(search_server PRIVATE
  "${CMAKE_SOURCE_DIR}/include"
  "${CMAKE_SOURCE_DIR}/generated"
)

find_package(CURL REQUIRED)
target_link_libraries(search_core_lib
  PRIVATE
    CURL::libcurl
    lexbor_static
    mecab
)

target_include_directories(search_core_lib PUBLIC
  "${CMAKE_SOURCE_DIR}/include"
)

target_compile_features(search_core_lib PUBLIC cxx_std_17)

foreach(APP crawl indexer search)
  if(APP STREQUAL "indexer")
    set(APP_SOURCE "${CMAKE_SOURCE_DIR}/app/index_main.cpp")
  else()
    set(APP_SOURCE "${CMAKE_SOURCE_DIR}/app/search_main.cpp")
  endif()

  add_executable(${APP} ${APP_SOURCE})
  target_link_libraries(${APP} PRIVATE search_lib)
  target_include_directories(${APP} PRIVATE "${CMAKE_SOURCE_DIR}/include")
endforeach()

enable_testing()
add_subdirectory(tests)

option(BUILD_BENCHMARKS "Build benchmark suite" ON)
if(BUILD_BENCHMARKS)
  add_subdirectory(benchmarks)
endif()
