#pragma once

#include <koinos/vmmanager/context.hpp>
#include <koinos/vmmanager/vm_backend.hpp>

#include <string>

namespace koinos::vmmanager::fizzy {

/**
 * Implementation of vm_backend for Fizzy.
 */
class fizzy_vm_backend : public vm_backend
{
   public:
      fizzy_vm_backend();
      virtual ~fizzy_vm_backend();

      virtual std::string backend_name();
      virtual void initialize();

      virtual void run( context& ctx, char* bytecode_data, size_t bytecode_size );
};

} // koinos::vmmanager::fizzy
