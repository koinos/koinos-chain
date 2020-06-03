#include <koinos/chain/system_calls.hpp>
#include <chainbase/chainbase.hpp>
#include <mira/database_configuration.hpp>
#include <koinos/exception.hpp>

#include <iostream>
#include <cstddef>

int main(int argc, char** argv, char** envp)
{
   try
   {
      koinos::chain::register_syscalls();
      koinos::chain::wasm_allocator_type wa;
      std::vector< uint8_t > wasm_bin = koinos::chain::backend_type::read_wasm("hello.wasm");
      koinos::chain::backend_type bkend( wasm_bin, koinos::chain::registrar_type{} );

      bkend.set_wasm_allocator(&wa);
      bkend.initialize();

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
      ctx.receiver = koinos::chain::name(0);
      bkend(&ctx, "env", "apply", (uint64_t)0, (uint64_t)0, (uint64_t)0);
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
