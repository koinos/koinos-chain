add_executable(koinosd main.cpp)
target_link_libraries(koinosd appbase Koinos::exception Koinos::plugins Koinos::crypto Koinos::chain_plugin Koinos::types Koinos::log Boost::program_options)
install(TARGETS
   koinosd
   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
