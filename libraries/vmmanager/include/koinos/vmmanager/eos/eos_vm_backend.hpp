#pragma once

#include <koinos/vmmanager/context.hpp>
#include <koinos/vmmanager/vm_backend.hpp>

#include <string>

namespace koinos::vmmanager::eos {

/**
 * Implementation of vm_backend for the EOS VM.
 */
class eos_vm_backend : public vm_backend
{
   public:
      eos_vm_backend();
      virtual ~eos_vm_backend();

      virtual std::string backend_name();
      virtual void initialize();

      virtual void run( context& ctx, char* bytecode_data, size_t bytecode_size );
};

} // koinos::vmmanager::eos
