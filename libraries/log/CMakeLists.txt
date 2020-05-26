file(GLOB HEADERS "include/koinos/log/*.hpp")

find_package(Boost REQUIRED COMPONENTS log log_setup filesystem)

add_library(log
  log.cpp
  ${HEADERS}
)

target_include_directories(log PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(log PUBLIC Boost::log_setup Boost::log Boost::filesystem)
#target_compile_definitions(log PUBLIC BOOST_LOG_DYN_LINK)

