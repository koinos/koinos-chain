#pragma once

#include <koinos/vm_manager/context.hpp>

#include <memory>
#include <string>
#include <vector>

namespace koinos::vm_manager {

/**
 * Abstract class for WebAssembly virtual machines.
 *
 * To add a new WebAssembly VM, you need to implement this class
 * and return it in get_vm_backends().
 */
class vm_backend
{
   public:
      vm_backend();
      virtual ~vm_backend();

      virtual std::string backend_name() = 0;

      /**
       * Initialize the backend.  Should only be called once.
       */
      virtual void initialize() = 0;

      /**
       * Run some bytecode.
       */
      virtual void run( context&, char* bytecode_data, size_t bytecode_size ) = 0;
};

/**
 * Get a list of available VM backends.
 */
std::vector< std::shared_ptr< vm_backend > > get_vm_backends();

std::string get_default_vm_backend_name();

/**
 * Get a shared_ptr to the named VM backend.
 */
std::shared_ptr< vm_backend > get_vm_backend( const std::string& name = get_default_vm_backend_name() );

} // koinos::vm_manager
