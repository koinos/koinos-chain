#pragma once

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
};

std::vector< std::shared_ptr< vm_backend > > get_vm_backends();

}
