find_package( Gperftools QUIET )
if( GPERFTOOLS_FOUND )
    message( STATUS "Found gperftools; compiling tests with TCMalloc")
    list( APPEND PLATFORM_SPECIFIC_LIBS tcmalloc )
endif()

INCLUDE(CTest)

ENABLE_TESTING()

file(GLOB UNIT_TESTS "tests/*.cpp")
file(GLOB_RECURSE TEST_FIXTURES "include/*.hpp")

include(BoostTestTargets.cmake)

function(parse_unit_tests RESULT)
   set(SOURCES)
	foreach(_element ${ARGN})
      list(APPEND SOURCES "${_element}")
	endforeach()

   set(tests)

   foreach(src ${SOURCES})
      file(READ ${src} thefile)
      #string(REGEX MATCH "" test_suite "${thefile}" )
      string(REGEX MATCH "BOOST_FIXTURE_TEST_SUITE\\([A-Za-z0-9_, ]*\\)" test_suite "${thefile}" )

      if( NOT (test_suite STREQUAL "") )
         string(SUBSTRING "${test_suite}" 25 -1 test_suite)
         string(FIND "${test_suite}" "," comma_loc )
         string(SUBSTRING "${test_suite}" 0 ${comma_loc} test_suite)
         string(STRIP "${test_suite}" test_suite)

         string( REGEX MATCHALL "BOOST_AUTO_TEST_CASE\\([A-Za-z0-9_, ]*\\)" cases "${thefile}" )

         foreach( test_case ${cases} )
            string(SUBSTRING "${test_case}" 21 -1 test_case)
            string(FIND "${test_case}" ")" paren_loc )
            string(SUBSTRING "${test_case}" 0 ${paren_loc} test_case)
            string(STRIP "${test_case}" test_case)

            list(APPEND tests "${test_suite}/${test_case}")
         endforeach()
      endif()
   endforeach()

   set(${RESULT} ${tests} PARENT_SCOPE)
endfunction()

parse_unit_tests(TEST_CASES ${UNIT_TESTS})

add_boost_test( koinos_tests
   SOURCES ${UNIT_TESTS} ${TEST_FIXTURES}
   TESTS ${TEST_CASES}
)

target_link_libraries(koinos_tests mira koinos_types koinos_chain koinos_crypto koinos_statedb koinos_log koinos_util koinos_exception ${PLATFORM_SPECIFIC_LIBS})
target_include_directories(koinos_tests PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>  # <prefix>/include/mylib
)
