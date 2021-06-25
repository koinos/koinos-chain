add_executable(koinos_get_dev_key main.cpp)
target_link_libraries(koinos_get_dev_key Koinos::exception Koinos::crypto Koinos::types Koinos::util Boost::program_options)
install(TARGETS
   koinos_get_dev_key
   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
