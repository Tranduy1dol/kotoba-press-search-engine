include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_Declare(
  lexbor
  GIT_REPOSITORY
  https://github.com/lexbor/lexbor.git
  GIT_TAG v2.3.0
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

set(LEXBOR_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LEXBOR_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)
FetchContent_MakeAvailable(lexbor)

enable_testing()

file(GLOB TEST_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
)

foreach(TEST_FILE ${TEST_SOURCES})
  get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
  add_executable(${TEST_NAME} ${TEST_FILE})
  target_link_libraries(${TEST_NAME} PRIVATE
    search_core_lib
    GTest::gtest_main
  )
  target_compile_definitions(${TEST_NAME} PRIVATE
    TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/data"
  )

  set_target_properties(${TEST_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
  )

  add_test(
    NAME ${TEST_NAME}
    COMMAND ${TEST_NAME}
  )
  
  message(STATUS "Add test: ${TEST_NAME}")
endforeach()

add_custom_target(run_test
  COMMAND ${CMAKE_CTEST_COMMAND} --outpyt-on-failure
  DEPENDS ${TEST_NAMES}
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  COMMENT "Running all unit tests"
)
