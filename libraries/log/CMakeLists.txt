file(GLOB HEADERS "include/koinos/*.hpp")

find_package(Boost CONFIG REQUIRED log log_setup filesystem)

add_library(koinos_log
  log.cpp
  ${HEADERS}
)

target_include_directories(koinos_log PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(koinos_log PUBLIC Boost::log_setup Boost::log Boost::filesystem)
add_library(Koinos::log ALIAS koinos_log)

