# Defines MIRA library target.
#project(MIRA)
#cmake_minimum_required(VERSION 2.8.12)

file(GLOB HEADERS "include/mira/*.hpp" "include/mira/detail/*.hpp")

set(CMAKE_EXPORT_COMPILE_COMMANDS "ON")

find_package(Boost CONFIG REQUIRED thread date_time system filesystem chrono unit_test_framework locale)
find_package(RocksDB CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
set(Boost_USE_STATIC_LIBS ON CACHE STRING "ON or OFF")

if( APPLE )
  # Apple Specific Options Here
  message( STATUS "Configuring MIRA on OS X" )
  set( CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++14 -stdlib=libc++ -Wall" )
else( APPLE )
  # Linux Specific Options Here
  message( STATUS "Configuring MIRA on Linux" )
  set( CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++14 -Wall" )
  set( rt_library rt )
  set( pthread_library pthread)
  if ( FULL_STATIC_BUILD )
    set( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ -static-libgcc")
  endif ( FULL_STATIC_BUILD )
endif( APPLE )

if( "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" )
  set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-builtin-memcmp" )
endif()

if(ENABLE_COVERAGE_TESTING)
    SET(CMAKE_CXX_FLAGS "--coverage ${CMAKE_CXX_FLAGS}")
endif()

add_library( mira
             src/configuration.cpp
             src/database_configuration.cpp
             ${HEADERS}
           )

if( CLANG_TIDY_EXE )
   set_target_properties(
      mira PROPERTIES
      CXX_CLANG_TIDY "${DO_CLANG_TIDY}"
   )
endif( CLANG_TIDY_EXE )

target_link_libraries(mira PUBLIC Boost::boost Boost::filesystem Boost::thread Boost::system Boost::chrono Boost::date_time Boost::locale RocksDB::rocksdb nlohmann_json::nlohmann_json)
target_include_directories(mira PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_subdirectory(test)

install(TARGETS
   mira
   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
install(FILES ${HEADERS} DESTINATION "include/mira")
