
#include <koinos/vmmanager/exceptions.hpp>
#include <koinos/vmmanager/vm_backend.hpp>

#include <koinos/vmmanager/eos/eos_vm_backend.hpp>
#include <koinos/vmmanager/fizzy/fizzy_vm_backend.hpp>

#include <cstdlib>

namespace koinos::vmmanager {

vm_backend::vm_backend() {}
vm_backend::~vm_backend() {}

std::vector< std::shared_ptr< vm_backend > > get_vm_backends()
{
   std::vector< std::shared_ptr< vm_backend > > result;

   result.push_back( std::make_shared< vmmanager::eos::eos_vm_backend >() );
   result.push_back( std::make_shared< vmmanager::fizzy::fizzy_vm_backend >() );

   return result;
}

std::string default_vm_backend = "fizzy";

std::string get_default_vm_backend_name()
{
   const char* env_backend = std::getenv("KOINOS_VM_BACKEND");
   if( env_backend == nullptr )
      return default_vm_backend;

   std::string env_backend_str = env_backend;

   if( env_backend_str.length() == 0 )
      return default_vm_backend;

   return env_backend_str;
}

std::shared_ptr< vm_backend > get_vm_backend( const std::string& name )
{
   const std::string& target_name = (name != "") ? name : get_default_vm_backend_name();

   std::vector< std::shared_ptr< vm_backend > > backends = get_vm_backends();
   for( std::shared_ptr< vm_backend > b : backends )
   {
      if( b->backend_name() == target_name )
         return b;
   }
   KOINOS_THROW( unknown_backend_exception, "Could not find backend ${name}", ("name", target_name) );
}

} // koinos::vmmanager
