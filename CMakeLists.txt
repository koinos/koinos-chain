
cmake_minimum_required (VERSION 3.10.2)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
   set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
   set(CMAKE_XCODE_ATTRIBUTE_CC           "${CMAKE_SOURCE_DIR}/ci/ccache_clang")
   set(CMAKE_XCODE_ATTRIBUTE_CXX          "${CMAKE_SOURCE_DIR}/ci/ccache_clang++")
   set(CMAKE_XCODE_ATTRIBUTE_LD           "${CMAKE_SOURCE_DIR}/ci/ccache_clang")
   set(CMAKE_XCODE_ATTRIBUTE_LDPLUSPLUS   "${CMAKE_SOURCE_DIR}/ci/ccache_clang++")
endif()

project( koinos )

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror" )
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror" )

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unknown-pragmas" )
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unknown-pragmas" )

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")

if(APPLE)
   SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_GNU_SOURCE")
   SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_GNU_SOURCE")
endif()

set(Boost_USE_STATIC_LIBS ON)
set(Boost_NO_BOOST_CMAKE ON)
if( NOT( Boost_VERSION_MACRO LESS 106900 ) )
   SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
   SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")
endif()

if(${CMAKE_GENERATOR} MATCHES "Xcode")
  set(CMAKE_XCODE_GENERATE_SCHEME YES)
endif()

if (COVERAGE)
  include(CodeCoverage)
  append_coverage_compiler_flags()
  setup_target_for_coverage_lcov(
     NAME coverage
     LCOV_ARGS "--no-external"
     EXECUTABLE koinos_tests
     EXCLUDE "libraries/vendor/*" "build/libraries/vendor/*")
endif()

add_subdirectory(programs)
add_subdirectory(libraries)
add_subdirectory(tests)
