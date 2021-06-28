add_executable(koinos_contract_uploader main.cpp)
target_link_libraries(koinos_contract_uploader Koinos::exception Koinos::mq Koinos::crypto Koinos::types Koinos::util Boost::program_options)
install(TARGETS
   koinos_contract_uploader
   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
