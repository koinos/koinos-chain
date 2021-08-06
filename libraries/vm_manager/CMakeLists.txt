file(GLOB HEADERS "include/koinos/vm_manager/*.hpp" "include/koinos/vm_manager")
add_library(koinos_vm_manager_lib

            host_api.cpp
            vm_backend.cpp

            fizzy/fizzy_vm_backend.cpp

            ${HEADERS})
target_link_libraries(koinos_vm_manager_lib Koinos::exception Koinos::log Koinos::util fizzy)
target_include_directories(koinos_vm_manager_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_library(Koinos::vm_manager ALIAS koinos_vm_manager_lib)
