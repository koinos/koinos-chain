
#include <koinos/vm_manager/exceptions.hpp>
#include <koinos/vm_manager/vm_backend.hpp>

#include <koinos/vm_manager/fizzy/fizzy_vm_backend.hpp>

#include <cstdlib>

namespace koinos::vm_manager {

vm_backend::vm_backend() {}
vm_backend::~vm_backend() {}

std::vector< std::shared_ptr< vm_backend > > get_vm_backends()
{
   std::vector< std::shared_ptr< vm_backend > > result;

   result.push_back( std::make_shared< vm_manager::fizzy::fizzy_vm_backend >() );

   return result;
}

std::string get_default_vm_backend_name()
{
   const std::string default_vm_backend = "fizzy";
   return default_vm_backend;
}

std::shared_ptr< vm_backend > get_vm_backend( const std::string& name )
{
   std::vector< std::shared_ptr< vm_backend > > backends = get_vm_backends();
   for( std::shared_ptr< vm_backend > b : backends )
   {
      if( b->backend_name() == name )
         return b;
   }
   return std::shared_ptr< vm_backend >();
}

} // koinos::vm_manager
