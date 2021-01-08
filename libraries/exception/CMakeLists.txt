file(GLOB HEADERS "include/koinos/*.hpp")

find_package(nlohmann_json CONFIG REQUIRED)

add_library(koinos_exception ${HEADERS} exception.cpp)
target_link_libraries(koinos_exception Koinos::types Koinos::log nlohmann_json::nlohmann_json)
target_include_directories(koinos_exception PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_library(Koinos::exception ALIAS koinos_exception)
