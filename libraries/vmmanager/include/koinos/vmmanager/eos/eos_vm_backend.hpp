#pragma once

#include <koinos/vmmanager/vm_backend.hpp>

namespace koinos::vmmanager::eos {

class eos_vm_backend : public vm_backend
{
   public:
      eos_vm_backend();
      virtual ~eos_vm_backend();

      virtual std::string backend_name();
      virtual void initialize();
};

}
