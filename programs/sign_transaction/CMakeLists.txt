add_executable(sign_transaction main.cpp)
target_link_libraries(sign_transaction appbase Koinos::exception Koinos::plugins Koinos::crypto Koinos::chain_plugin Koinos::types Koinos::log Boost::program_options)
install(TARGETS
   sign_transaction
   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
