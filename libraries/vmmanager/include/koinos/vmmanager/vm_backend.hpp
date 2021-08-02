#pragma once

#include <koinos/vmmanager/context.hpp>

#include <memory>
#include <string>
#include <vector>

namespace koinos::vmmanager {

class vm_backend
{
   public:
      vm_backend();
      virtual ~vm_backend();

      virtual std::string backend_name() = 0;
      virtual void initialize() = 0;

      virtual void run( context&, char* bytecode_data, size_t bytecode_size ) = 0;
};

std::vector< std::shared_ptr< vm_backend > > get_vm_backends();
std::shared_ptr< vm_backend > get_vm_backend( const std::string& name );

}
