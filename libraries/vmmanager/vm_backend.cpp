
#include <koinos/vmmanager/vm_backend.hpp>

#include <koinos/vmmanager/eos/eos_vm_backend.hpp>

namespace koinos::vmmanager {

vm_backend::vm_backend() {}
vm_backend::~vm_backend() {}

std::vector< std::shared_ptr< vm_backend > > get_vm_backends( const std::string& name )
{
   std::vector< std::shared_ptr< vm_backend > > result;

   result.push_back( std::make_shared< vmmanager::eos::eos_vm_backend >() );
   return result;
}

}
