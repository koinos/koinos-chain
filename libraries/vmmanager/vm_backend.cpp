
#include <koinos/vmmanager/exceptions.hpp>
#include <koinos/vmmanager/vm_backend.hpp>

#include <koinos/vmmanager/eos/eos_vm_backend.hpp>

namespace koinos::vmmanager {

vm_backend::vm_backend() {}
vm_backend::~vm_backend() {}

std::vector< std::shared_ptr< vm_backend > > get_vm_backends()
{
   std::vector< std::shared_ptr< vm_backend > > result;

   result.push_back( std::make_shared< vmmanager::eos::eos_vm_backend >() );
   return result;
}

std::shared_ptr< vm_backend > get_vm_backend( const std::string& name )
{
   std::vector< std::shared_ptr< vm_backend > > backends = get_vm_backends();
   for( std::shared_ptr< vm_backend > b : backends )
   {
      if( b->backend_name() == name )
         return b;
   }
   KOINOS_THROW( unknown_backend_exception, "Could not find backend ${name}", ("name", name) );
}

}
