#include <cstddef>
#include <fstream>
#include <iostream>

#include <boost/program_options.hpp>

#include <koinos/chain/host_api.hpp>
#include <koinos/chain/system_calls.hpp>
#include <koinos/chain/thunk_dispatcher.hpp>
#include <koinos/chain/types.hpp>
#include <koinos/exception.hpp>

#include <mira/database_configuration.hpp>

#define HELP_OPTION     "help"
#define CONTRACT_OPTION "contract"
#define VM_OPTION       "vm"
#define LIST_VM_OPTION  "list"
#define TICKS_OPTION    "ticks"

using namespace koinos;

int main( int argc, char** argv, char** envp )
{
   try
   {
      boost::program_options::options_description desc( "Koinos VM options" );
      desc.add_options()
        ( HELP_OPTION ",h", "print usage message" )
        ( CONTRACT_OPTION ",c", boost::program_options::value< std::string >(), "the contract to run" )
        ( VM_OPTION ",v", boost::program_options::value< std::string >()->default_value( "" ), "the VM backend to use" )
        ( TICKS_OPTION ",t", boost::program_options::value< int64_t >()->default_value( 10 * 1000 * 1000 ), "set maximum allowed ticks" )
        ( LIST_VM_OPTION ",l", "list available VM backends" )
        ;

      boost::program_options::variables_map vmap;
      boost::program_options::store( boost::program_options::parse_command_line( argc, argv, desc ), vmap );

      initialize_logging( "koinos_vm_drivier", {}, "info" );

      if ( vmap.count( HELP_OPTION ) )
      {
         std::cout << desc << std::endl;
         return EXIT_SUCCESS;
      }

      if ( vmap.count( LIST_VM_OPTION ) )
      {
         std::cout << "Available VM Backend(s):";

         std::vector< std::shared_ptr< vm_manager::vm_backend > > backends = vm_manager::get_vm_backends();
         for( auto b : backends )
         {
            std::cout << "   " << b->backend_name() << std::endl;
         }
         return EXIT_SUCCESS;
      }

      if ( !vmap.count( CONTRACT_OPTION ) )
      {
         std::cout << desc << std::endl;
         return EXIT_FAILURE;
      }

      std::filesystem::path contract_file{ vmap[ CONTRACT_OPTION ].as< std::string >() };
      if ( contract_file.is_relative() )
         contract_file = std::filesystem::current_path() / contract_file;

      std::ifstream ifs( contract_file );
      std::string contract_wasm( ( std::istreambuf_iterator< char >( ifs ) ), ( std::istreambuf_iterator< char >() ) );

      std::string vm_backend_name = vmap[ VM_OPTION ].as< std::string >();

      auto vm_backend = vm_manager::get_vm_backend( vm_backend_name );
      KOINOS_ASSERT( vm_backend, koinos::chain::unknown_backend_exception, "Couldn't get VM backend" );

      vm_backend->initialize();
      LOG(info) << "Initialized " << vm_backend->backend_name() << " VM backend";

      chain::apply_context ctx( vm_backend );

      auto rld = chain::system_call::get_resource_limits( ctx );
      rld.set_compute_bandwidth_limit( vmap[ TICKS_OPTION ].as< int64_t >() );
      ctx.resource_meter().set_resource_limit_data( rld );
      chain::host_api hapi( ctx );

      chain::contract_data cd;
      cd.set_wasm( std::move( contract_wasm ) );
      cd.set_hash( std::string() );

      vm_backend->run( hapi, cd );

      auto output = ctx.get_pending_console_output();

      if ( output.size() )
         LOG(info) << "Contract output:";
         LOG(info) << ctx.get_pending_console_output();
   }
   catch( const koinos::exception& e )
   {
      LOG(fatal) << boost::diagnostic_information( e );
      return EXIT_FAILURE;
   }
   catch (...)
   {
      LOG(fatal) << "unknown error";
      return EXIT_FAILURE;
   }

   return EXIT_SUCCESS;
}
