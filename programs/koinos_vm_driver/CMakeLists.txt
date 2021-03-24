find_package(Boost CONFIG REQUIRED COMPONENTS program_options)

add_executable(koinos_vm_driver main.cpp)
target_link_libraries(koinos_vm_driver PUBLIC Koinos::chain Boost::program_options)

