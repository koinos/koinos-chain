add_executable(koinos_transaction_signer main.cpp)
target_link_libraries(koinos_transaction_signer Koinos::exception Koinos::crypto Koinos::types Koinos::log Boost::program_options)
install(TARGETS
   koinos_transaction_signer
   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
