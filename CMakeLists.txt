cmake_minimum_required(VERSION 3.19.0)

cmake_policy(VERSION 3.19.0...3.27.4)

include(FetchContent)
FetchContent_Declare(
  koinos_cmake
  GIT_REPOSITORY https://github.com/koinos/koinos-cmake.git
  GIT_TAG        v1.0.7)
FetchContent_MakeAvailable(koinos_cmake)

include("${koinos_cmake_SOURCE_DIR}/Koinos.cmake")

project(koinos_chain
  VERSION 1.4.1
  DESCRIPTION "Koinos Chain microservice"
  LANGUAGES CXX C)

koinos_define_version()

koinos_add_package(Boost CONFIG REQUIRED
  ADD_COMPONENTS system log thread date_time filesystem chrono test program_options exception
  FIND_COMPONENTS system log log_setup thread date_time filesystem chrono program_options exception)

koinos_add_package(ethash CONFIG REQUIRED)
koinos_add_package(libsecp256k1-vrf CONFIG REQUIRED)
koinos_add_package(nlohmann_json CONFIG REQUIRED)
koinos_add_package(OpenSSL REQUIRED)
koinos_add_package(rabbitmq-c CONFIG REQUIRED)
koinos_add_package(rocksdb NAME RocksDB CONFIG REQUIRED)
koinos_add_package(yaml-cpp CONFIG REQUIRED)
koinos_add_package(Protobuf CONFIG REQUIRED)
koinos_add_package(fizzy CONFIG REQUIRED)
koinos_add_package(gRPC CONFIG REQUIRED)
koinos_add_package(abseil NAME absl CONFIG REQUIRED)
koinos_add_package(re2 CONFIG REQUIRED)
koinos_add_package(c-ares CONFIG REQUIRED)
koinos_add_package(ZLIB CONFIG REQUIRED)

koinos_add_package(koinos_log CONFIG REQUIRED)
koinos_add_package(koinos_util CONFIG REQUIRED)
koinos_add_package(koinos_proto CONFIG REQUIRED)
koinos_add_package(koinos_exception CONFIG REQUIRED)
koinos_add_package(koinos_crypto CONFIG REQUIRED)
koinos_add_package(koinos_mq CONFIG REQUIRED)
koinos_add_package(koinos_state_db CONFIG REQUIRED)

add_subdirectory(src)
add_subdirectory(tests)
