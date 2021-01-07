
cmake_minimum_required(VERSION 3.10.2)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
   set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
   set(CMAKE_XCODE_ATTRIBUTE_CC           "${CMAKE_SOURCE_DIR}/ci/ccache_clang")
   set(CMAKE_XCODE_ATTRIBUTE_CXX          "${CMAKE_SOURCE_DIR}/ci/ccache_clang++")
   set(CMAKE_XCODE_ATTRIBUTE_LD           "${CMAKE_SOURCE_DIR}/ci/ccache_clang")
   set(CMAKE_XCODE_ATTRIBUTE_LDPLUSPLUS   "${CMAKE_SOURCE_DIR}/ci/ccache_clang++")
endif()

include("cmake/HunterGate.cmake")

option(HUNTER_RUN_UPLOAD "Upload cache binaries" OFF)

HunterGate(
   URL "https://github.com/koinos/hunter/archive/rocksdb.tar.gz"
   SHA1 "b5dc3af780ba255ee6103bfc306403738735f420"
   LOCAL
)

#HunterGate(
#   URL "https://github.com/cpp-pm/hunter/archive/v0.23.287.tar.gz"
#   SHA1 "69371a638d697bbf09f4f08dd0aad996efcc79d9"
#   LOCAL
#)

set(
    HUNTER_PASSWORDS_PATH
    "${CMAKE_CURRENT_LIST_DIR}/cmake/Hunter/passwords.cmake"
    CACHE
    FILEPATH
    "Hunter passwords"
)

project(koinos)

option(FORCE_COLORED_OUTPUT "Always produce ANSI-colored output (GNU/Clang only)." OFF)

# This is to force color output when using ccache with Unix Makefiles
if(${FORCE_COLORED_OUTPUT})
    if( "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" )
       add_compile_options (-fdiagnostics-color=always)
    elseif( "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang" )
       add_compile_options (-fcolor-diagnostics)
    endif ()
endif ()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror" )
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror" )

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unknown-pragmas" )
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unknown-pragmas" )

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")

if(APPLE)
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_GNU_SOURCE")
   set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_GNU_SOURCE")
endif()

set(Boost_USE_STATIC_LIBS ON)
set(Boost_NO_BOOST_CMAKE ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")

if(${CMAKE_GENERATOR} MATCHES "Xcode")
  set(CMAKE_XCODE_GENERATE_SCHEME YES)
endif()

if(COVERAGE)
  include(CodeCoverage)
  append_coverage_compiler_flags()
  setup_target_for_coverage_lcov(
     NAME coverage
     LCOV_ARGS "--quiet" "--no-external"
     EXECUTABLE koinos_tests
     EXCLUDE "libraries/vendor/*" "build/generated/*")
endif()

hunter_add_package(rocksdb)
hunter_add_package(Boost COMPONENTS system log thread date_time filesystem chrono locale test program_options)
hunter_add_package(nlohmann_json)
hunter_add_package(OpenSSL)

add_subdirectory(programs)
add_subdirectory(libraries)
add_subdirectory(tests)

