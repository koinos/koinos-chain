#include <cstddef>
#include <iostream>

#include <boost/program_options.hpp>

#include <chainbase/chainbase.hpp>

#include <koinos/chain/system_calls.hpp>
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

      koinos::chain::register_syscalls();
      koinos::chain::wasm_allocator_type wa;
      std::vector< uint8_t > wasm_bin = koinos::chain::backend_type::read_wasm( vmap[ CONTRACT_OPTION ].as< std::string >() );
      koinos::chain::backend_type backend( wasm_bin, koinos::chain::registrar_type{} );

      backend.set_wasm_allocator( &wa );
      backend.initialize();

      chainbase::database db;
      auto tmp = boost::filesystem::current_path() / boost::filesystem::unique_path();

      db.open( tmp, 0, mira::utilities::default_database_configuration() );
      db.add_index< koinos::chain::table_id_multi_index >();
      db.add_index< koinos::chain::key_value_index >();
      db.add_index< koinos::chain::index64_index >();
      db.add_index< koinos::chain::index128_index >();
      db.add_index< koinos::chain::index256_index >();
      db.add_index< koinos::chain::index_double_index >();
      db.add_index< koinos::chain::index_long_double_index >();

      koinos::chain::system_call_table t;
      koinos::chain::apply_context ctx( db, t );
      ctx.receiver = koinos::chain::name( 0 );

      backend( &ctx, "env", "apply", (uint64_t)0, (uint64_t)0, (uint64_t)0 );

      std::cout << ctx.get_pending_console_output() << std::endl;
   }
   catch( const eosio::vm::exception& e )
   {
      std::cerr << e.what() << ": " << e.detail() << std::endl;
      return EXIT_FAILURE;
   }
   catch( const koinos::exception& e )
   {
      std::cerr << e.to_string() << std::endl;
      return EXIT_FAILURE;
   }
   catch (...)
   {
      std::cerr << "unknown error" << std::endl;
      return EXIT_FAILURE;
   }

   return EXIT_SUCCESS;
}
