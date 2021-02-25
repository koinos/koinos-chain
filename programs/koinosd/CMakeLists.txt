find_package(Boost CONFIG REQUIRED program_options log log_setup filesystem)
find_package(koinos_log CONFIG REQUIRED)

add_executable(koinosd main.cpp)
target_link_libraries(koinosd appbase Koinos::exception Koinos::plugins Koinos::crypto Koinos::chain_plugin Koinos::koinos_log Boost::program_options)
install(TARGETS
   koinosd
   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
