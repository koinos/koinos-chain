find_package(Boost CONFIG REQUIRED program_options log log_setup filesystem headers)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(rabbitmq-c CONFIG REQUIRED)

find_package(koinos_log CONFIG REQUIRED)
find_package(koinos_util CONFIG REQUIRED)
find_package(koinos_types CONFIG REQUIRED)
find_package(koinos_exception CONFIG REQUIRED)
find_package(koinos_crypto CONFIG REQUIRED)
find_package(koinos_mq CONFIG REQUIRED)

add_executable(koinosd main.cpp)
target_link_libraries(koinosd appbase Koinos::exception Koinos::plugins Koinos::crypto Koinos::chain_plugin Koinos::types Koinos::log Boost::program_options)
install(TARGETS
   koinosd
   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
