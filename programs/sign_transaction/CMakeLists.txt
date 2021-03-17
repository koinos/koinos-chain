add_executable(sign_transaction main.cpp)
target_link_libraries(sign_transaction Koinos::exception Koinos::crypto Koinos::types Koinos::log Boost::program_options)
install(TARGETS
   sign_transaction
   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
