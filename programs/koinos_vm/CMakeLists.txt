find_package(Boost CONFIG REQUIRED COMPONENTS program_options)

add_executable(koinos_vm driver.cpp)
target_link_libraries(koinos_vm PUBLIC Koinos::chain Boost::program_options)

