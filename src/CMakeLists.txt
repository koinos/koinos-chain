add_library(chain
  koinos/chain/chronicler.cpp
  koinos/chain/controller.cpp
  koinos/chain/execution_context.cpp
  koinos/chain/host_api.cpp
  koinos/chain/indexer.cpp
  koinos/chain/proto_utils.cpp
  koinos/chain/resource_meter.cpp
  koinos/chain/session.cpp
  koinos/chain/state.cpp
  koinos/chain/system_calls.cpp
  koinos/chain/thunk_dispatcher.cpp

  koinos/chain/chronicler.hpp
  koinos/chain/constants.hpp
  koinos/chain/controller.hpp
  koinos/chain/exceptions.hpp
  koinos/chain/execution_context.hpp
  koinos/chain/host_api.hpp
  koinos/chain/indexer.hpp
  koinos/chain/proto_utils.hpp
  koinos/chain/resource_meter.hpp
  koinos/chain/session.hpp
  koinos/chain/state.hpp
  koinos/chain/system_calls.hpp
  koinos/chain/thunk_dispatcher.hpp
  koinos/chain/thunk_utils.hpp
  koinos/chain/types.hpp)

target_link_libraries(
  chain
    PUBLIC
      Koinos::state_db
      Koinos::exception
      Koinos::crypto
      Koinos::log
      Koinos::util
      Koinos::mq
      vm_manager)

target_include_directories(
  chain
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

koinos_add_format(TARGET chain)

add_library(vm_manager
  koinos/vm_manager/host_api.cpp
  koinos/vm_manager/vm_backend.cpp
  koinos/vm_manager/fizzy/fizzy_vm_backend.cpp
  koinos/vm_manager/fizzy/module_cache.cpp

  koinos/vm_manager/fizzy/exceptions.hpp
  koinos/vm_manager/fizzy/fizzy_vm_backend.hpp
  koinos/vm_manager/fizzy/module_cache.hpp)

target_link_libraries(
  vm_manager
    PUBLIC
      Koinos::exception
      Koinos::log
      Koinos::util
      fizzy::fizzy)

target_include_directories(
  vm_manager
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

koinos_add_format(TARGET vm_manager)

add_executable(koinos_chain koinos_chain.cpp)
target_link_libraries(
  koinos_chain
    PRIVATE
      chain
      Koinos::git
      Boost::program_options)

koinos_add_format(TARGET koinos_chain)

add_executable(koinos_vm_driver koinos_vm_driver.cpp)
target_link_libraries(
  koinos_vm_driver
    PRIVATE
      chain
      Boost::program_options)

koinos_add_format(TARGET koinos_vm_driver)

koinos_install(TARGETS koinos_chain koinos_vm_driver)
