add_executable(koinos_chain main.cpp)
target_link_libraries(koinos_chain mira Koinos::exception Koinos::crypto Koinos::types Koinos::log Koinos::mq Koinos::util Koinos::chain Boost::program_options yaml-cpp)
install(TARGETS
   koinos_chain
   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
