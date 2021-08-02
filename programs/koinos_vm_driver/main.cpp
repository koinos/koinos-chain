#include <cstddef>
#include <fstream>
#include <iostream>

#include <boost/program_options.hpp>

#include <koinos/chain/koinos_host_api.hpp>
#include <koinos/chain/system_calls.hpp>
#include <koinos/chain/thunk_dispatcher.hpp>
#include <koinos/chain/types.hpp>
#include <koinos/exception.hpp>
#include <koinos/pack/classes.hpp>
#include <koinos/vmmanager/chain_host_api.hpp>
#include <koinos/vmmanager/context.hpp>

#include <mira/database_configuration.hpp>

#define HELP_OPTION     "help"
#define CONTRACT_OPTION "contract"
#define VM_OPTION       "vm"
#define LIST_VM_OPTION  "listvm"
#define TICKS_OPTION    "ticks"

std::vector< char > read_file( const std::string& path )
{
   std::ifstream in;
   in.exceptions( std::ifstream::eofbit | std::ifstream::failbit | std::ifstream::badbit );

   in.open( path, std::ifstream::binary );
   in.seekg( 0, std::ifstream::end );
   size_t size = in.tellg();
   in.seekg( 0, std::ifstream::beg );
   std::vector<char> result(size);
   in.read( result.data(), size );
   return result;
}

using namespace koinos::vmmanager;

int main( int argc, char** argv, char** envp )
{
   try
   {
      boost::program_options::options_description desc( "Koinos VM options" );
      desc.add_options()
        ( HELP_OPTION ",h", "print usage message" )
        ( CONTRACT_OPTION ",c", boost::program_options::value< std::string >(), "the contract to run" )
        ( VM_OPTION ",v", boost::program_options::value< std::string >()->default_value( "eos" ), "the VM backend to use" )
        ( TICKS_OPTION ",t", boost::program_options::value< int64_t >()->default_value( 10 * 1000 * 1000 ), "set maximum allowed ticks" )
        ( LIST_VM_OPTION ",l", "list available VM backends" )
        ;

      boost::program_options::variables_map vmap;
      boost::program_options::store( boost::program_options::parse_command_line( argc, argv, desc ), vmap );

      if ( vmap.count( HELP_OPTION ) )
      {
         std::cout << desc << std::endl;
         return EXIT_SUCCESS;
      }

      if ( vmap.count( LIST_VM_OPTION ) )
      {
         std::vector< std::shared_ptr< vm_backend > > backends = get_vm_backends();
         for( std::shared_ptr< vm_backend > b : backends )
         {
            std::cout << b->backend_name() << std::endl;
         }
         return EXIT_SUCCESS;
      }

      if ( !vmap.count( CONTRACT_OPTION ) )
      {
         std::cout << desc << std::endl;
         return EXIT_FAILURE;
      }

      std::string vm_backend_name = vmap[ VM_OPTION ].as< std::string >();

      std::shared_ptr< vm_backend > vm_backend = get_vm_backend( vm_backend_name );
      LOG(info) << "Initialized " << vm_backend->backend_name() << " VM backend";

      vm_backend->initialize();

      std::vector< char > wasm_bin = read_file( vmap[ CONTRACT_OPTION ].as< std::string >() );

      int64_t ticks = vmap[ TICKS_OPTION ].as< int64_t >();
      koinos::chain::apply_context ctx( vm_backend );
      koinos::chain::koinos_host_api hapi( ctx );
      koinos::vmmanager::context vm_ctx( hapi, ticks );
      vm_backend->run( vm_ctx, wasm_bin.data(), wasm_bin.size() );

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
