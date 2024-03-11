if (NOT BUILD_TESTING)
  return()
endif()

include(CTest)

koinos_add_test(
  chain_tests
    SOURCES
      main.cpp
      contracts.cpp
      controller_test.cpp
      stack_test.cpp
      thunk_test.cpp)

target_link_libraries(
  chain_tests
    PRIVATE
      chain
      Koinos::proto
      Koinos::crypto
      Koinos::state_db
      Koinos::log
      Koinos::util
      Koinos::exception)

target_include_directories(
  chain_tests
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
      $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
      $<INSTALL_INTERFACE:include>)

koinos_add_format(TARGET chain_tests)

koinos_coverage(
  EXECUTABLE
    chain_tests
  EXCLUDE
    "tests/*")
