add_subdirectory(chainbase)
add_subdirectory(eos-vm)
add_subdirectory(fc)
add_subdirectory(nlohmann_json)
add_subdirectory(mira)

#Configure needed build options of RocksDB.
SET(WITH_TESTS OFF CACHE BOOL "build with tests")
#SET(WITH_SNAPPY ON CACHE BOOL "build with SNAPPY")
#SET(WITH_ZLIB OFF CACHE BOOL "build with ZLIB")
#SET(WITH_BZ2 OFF CACHE BOOL "build with BZ2")
SET(WITH_BENCHMARK_TOOLS OFF CACHE BOOL "build with BENCHMARKS")
#SET(WITH_GFLAGS OFF CACHE BOOL "build with GFlags")
#SET(USE_RTTI ON CACHE BOOL "build with RTTI")
#SET(PORTABLE ON CACHE BOOL "build a portable binary")
SET(WITH_CORE_TOOLS OFF CACHE BOOL "build with tools")
SET(WITH_TOOLS OFF CACHE BOOL "build with tools")

# If we don't have CMake variables defined, try to get them from the environment
#if (NOT DEFINED ZLIB_ROOT_DIR)
#   if (EXISTS "$ENV{ZLIB_ROOT_DIR}")
#      set(ZLIB_ROOT_DIR $ENV{ZLIB_ROOT_DIR})
#   endif()
#endif()
#if (NOT DEFINED BZIP2_ROOT_DIR)
#   if (EXISTS "$ENV{BZIP2_ROOT_DIR}")
#      set(BZIP2_ROOT_DIR $ENV{BZIP2_ROOT_DIR})
#   endif()
#endif()

add_subdirectory(rocksdb)

# Configure secp256k1
include(ExternalProject)
ExternalProject_Add( project_secp256k1
   PREFIX ${CMAKE_CURRENT_BINARY_DIR}/secp256k1
   SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/secp256k1
   CONFIGURE_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/secp256k1/configure --prefix=${CMAKE_CURRENT_BINARY_DIR}/secp256k1 --with-bignum=no
   BUILD_COMMAND make
   INSTALL_COMMAND true
   BUILD_BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/secp256k1/src/project_secp256k1-build/.libs/libsecp256k1.a
)
ExternalProject_Add_Step(project_secp256k1 autogen
   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/secp256k1
   COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/secp256k1/autogen.sh
   DEPENDERS configure
)
ExternalProject_Get_Property(project_secp256k1 binary_dir)

add_library(secp256k1 STATIC IMPORTED)
set_property(TARGET secp256k1 PROPERTY IMPORTED_LOCATION ${binary_dir}/.libs/libsecp256k1${CMAKE_STATIC_LIBRARY_SUFFIX})
set_property(TARGET secp256k1 PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/secp256k1/include)
add_dependencies(secp256k1 project_secp256k1)
# End configure secp256k1
