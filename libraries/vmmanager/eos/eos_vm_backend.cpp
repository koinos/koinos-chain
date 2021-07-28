
#include <koinos/vmmanager/eos/eos_host.hpp>
#include <koinos/vmmanager/eos/eos_vm_backend.hpp>

namespace koinos::vmmanager::eos {

eos_vm_backend::eos_vm_backend() {}
eos_vm_backend::~eos_vm_backend() {}

std::string eos_vm_backend::backend_name()
{
   return "eos";
}

void eos_vm_backend::initialize()
{
   register_host_functions();
}

}
