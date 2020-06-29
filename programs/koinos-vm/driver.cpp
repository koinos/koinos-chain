#include <cstddef>
#include <iostream>

#include <boost/program_options.hpp>

#include <koinos/chain/register_thunks.hpp>
#include <koinos/chain/system_calls.hpp>
#include <koinos/chain/thunk_dispatcher.hpp>
#include <koinos/chain/types.hpp>
#include <koinos/exception.hpp>

#include <mira/database_configuration.hpp>

#define HELP_OPTION     "help"
#define CONTRACT_OPTION "contract"

int main( int argc, char** argv, char** envp )
{
   try
   {
      boost::program_options::options_description desc( "Koinos VM options" );
      desc.add_options()
        ( HELP_OPTION ",h", "print usage message" )
        ( CONTRACT_OPTION ",c", boost::program_options::value< std::string >(), "the contract to run" )
        ;

      boost::program_options::variables_map vmap;
      boost::program_options::store( boost::program_options::parse_command_line( argc, argv, desc ), vmap );

      if ( vmap.count( HELP_OPTION ) )
      {
         std::cout << desc << std::endl;
         return EXIT_SUCCESS;
      }

      if ( !vmap.count( CONTRACT_OPTION ) )
      {
         std::cout << desc << std::endl;
         return EXIT_FAILURE;
      }

      koinos::chain::wasm_allocator_type wa;
      std::vector< uint8_t > wasm_bin = koinos::chain::backend_type::read_wasm( vmap[ CONTRACT_OPTION ].as< std::string >() );
      koinos::chain::backend_type backend( wasm_bin, koinos::chain::registrar_type{} );

      backend.set_wasm_allocator( &wa );
      backend.initialize();

      koinos::chain::apply_context ctx;

      backend( &ctx, "env", "apply", (uint64_t)0, (uint64_t)0, (uint64_t)0 );

      LOG(info) << ctx.get_pending_console_output();
   }
   catch( const eosio::vm::exception& e )
   {
      LOG(fatal) << e.what() << ": " << e.detail();
      return EXIT_FAILURE;
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
