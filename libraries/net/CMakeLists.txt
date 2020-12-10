file(GLOB HEADERS "include/koinos/net/jsonrpc/*.hpp")

find_package(Boost REQUIRED COMPONENTS system)

add_library(koinos_net ${HEADERS} server.cpp)

target_include_directories(koinos_net PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(koinos_net PUBLIC Boost::system koinos_log koinos_util nlohmann_json )

