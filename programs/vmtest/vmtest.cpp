
#include <eosio/vm/backend.hpp>
#include <eosio/vm/error_codes.hpp>
#include <eosio/vm/host_function.hpp>
#include <eosio/vm/exceptions.hpp>
#include <koinos/chain/apply_context.hpp>
#include <koinos/chain/wasm_interface.hpp>
#include <koinos/chain/wasm/type_conversion.hpp>
#include <chainbase/chainbase.hpp>
#include <mira/database_configuration.hpp>
#include <koinos/exception.hpp>

#include <iostream>
#include <chrono>
#include <thread>
#include <cstddef>

int main(int argc, char** argv, char** envp)
{
   using namespace std::this_thread;     // sleep_for, sleep_until
   using namespace std::chrono_literals; // ns, us, ms, s, h, etc.
   using namespace koinos::chain;
   using eosio::vm::wasm_allocator;

   sleep_for(10s);

   // Thread specific `allocator` used for wasm linear memory.
   wasm_allocator wa;

   // Specific the backend with example_host_methods for host functions.
   using backend_t = eosio::vm::backend< apply_context >;
   using rhf_t     = eosio::vm::registered_host_functions< apply_context >;

   rhf_t::add< compiler_builtins, &compiler_builtins::__ashlti3, wasm_allocator >( "env", "__ashlti3" );
   rhf_t::add< compiler_builtins, &compiler_builtins::__ashrti3, wasm_allocator >( "env", "__ashrti3" );
   rhf_t::add< compiler_builtins, &compiler_builtins::__lshlti3, wasm_allocator >( "env", "__lshlti3" );
   rhf_t::add< compiler_builtins, &compiler_builtins::__lshrti3, wasm_allocator >( "env", "__lshrti3" );
   rhf_t::add< compiler_builtins, &compiler_builtins::__divti3, wasm_allocator >( "env", "__divti3" );
   rhf_t::add< compiler_builtins, &compiler_builtins::__udivti3, wasm_allocator >( "env", "__udivti3" );
   rhf_t::add< compiler_builtins, &compiler_builtins::__multi3, wasm_allocator >( "env", "__multi3" );
   rhf_t::add< compiler_builtins, &compiler_builtins::__modti3, wasm_allocator >( "env", "__modti3" );
   rhf_t::add< compiler_builtins, &compiler_builtins::__umodti3, wasm_allocator >( "env", "__umodti3" );
   rhf_t::add< compiler_builtins, &compiler_builtins::__addtf3, wasm_allocator >( "env", "__addtf3" );
   rhf_t::add< compiler_builtins, &compiler_builtins::__subtf3, wasm_allocator >( "env", "__subtf3" );
   rhf_t::add< compiler_builtins, &compiler_builtins::__multf3, wasm_allocator >( "env", "__multf3" );
   rhf_t::add< compiler_builtins, &compiler_builtins::__divtf3, wasm_allocator >( "env", "__divtf3" );
   rhf_t::add< compiler_builtins, &compiler_builtins::__negtf2, wasm_allocator >( "env", "__negtf2" );
   rhf_t::add< compiler_builtins, &compiler_builtins::__extendsftf2, wasm_allocator >( "env", "__extendsftf2" );
   rhf_t::add< compiler_builtins, &compiler_builtins::__extenddftf2, wasm_allocator >( "env", "__extenddftf2" );
   rhf_t::add< compiler_builtins, &compiler_builtins::__trunctfdf2, wasm_allocator >( "env", "__trunctfdf2" );
   rhf_t::add< compiler_builtins, &compiler_builtins::__trunctfsf2, wasm_allocator >( "env", "__trunctfsf2" );
   rhf_t::add< compiler_builtins, &compiler_builtins::__fixtfsi, wasm_allocator >( "env", "__fixtfsi" );
   rhf_t::add< compiler_builtins, &compiler_builtins::__fixtfdi, wasm_allocator >( "env", "__fixtfdi" );
   rhf_t::add< compiler_builtins, &compiler_builtins::__fixtfti, wasm_allocator >( "env", "__fixtfti" );
   rhf_t::add< compiler_builtins, &compiler_builtins::__fixunstfsi, wasm_allocator >( "env", "__fixunstfsi" );
   rhf_t::add< compiler_builtins, &compiler_builtins::__fixunstfdi, wasm_allocator >( "env", "__fixunstfdi" );
   rhf_t::add< compiler_builtins, &compiler_builtins::__fixunstfti, wasm_allocator >( "env", "__fixunstfti" );
   rhf_t::add< compiler_builtins, &compiler_builtins::__fixsfti, wasm_allocator >( "env", "__fixsfti" );
   rhf_t::add< compiler_builtins, &compiler_builtins::__fixdfti, wasm_allocator >( "env", "__fixdfti" );
   rhf_t::add< compiler_builtins, &compiler_builtins::__fixunssfti, wasm_allocator >( "env", "__fixunssfti" );
   rhf_t::add< compiler_builtins, &compiler_builtins::__fixunsdfti, wasm_allocator >( "env", "__fixunsdfti" );
   rhf_t::add< compiler_builtins, &compiler_builtins::__floatsidf, wasm_allocator >( "env", "__floatsidf" );
   rhf_t::add< compiler_builtins, &compiler_builtins::__floatsitf, wasm_allocator >( "env", "__floatsitf" );
   rhf_t::add< compiler_builtins, &compiler_builtins::__floatditf, wasm_allocator >( "env", "__floatditf" );
   rhf_t::add< compiler_builtins, &compiler_builtins::__floatunsitf, wasm_allocator >( "env", "__floatunsitf" );
   rhf_t::add< compiler_builtins, &compiler_builtins::__floatunditf, wasm_allocator >( "env", "__floatunditf" );
   rhf_t::add< compiler_builtins, &compiler_builtins::__floattidf, wasm_allocator >( "env", "__floattidf" );
   rhf_t::add< compiler_builtins, &compiler_builtins::__floatuntidf, wasm_allocator >( "env", "__floatuntidf" );
   rhf_t::add< compiler_builtins, &compiler_builtins::___cmptf2, wasm_allocator >( "env", "___cmptf2" );
   rhf_t::add< compiler_builtins, &compiler_builtins::__eqtf2, wasm_allocator >( "env", "__eqtf2" );
   rhf_t::add< compiler_builtins, &compiler_builtins::__netf2, wasm_allocator >( "env", "__netf2" );
   rhf_t::add< compiler_builtins, &compiler_builtins::__getf2, wasm_allocator >( "env", "__getf2" );
   rhf_t::add< compiler_builtins, &compiler_builtins::__gttf2, wasm_allocator >( "env", "__gttf2" );
   rhf_t::add< compiler_builtins, &compiler_builtins::__letf2, wasm_allocator >( "env", "__letf2" );
   rhf_t::add< compiler_builtins, &compiler_builtins::__lttf2, wasm_allocator >( "env", "__lttf2" );
   rhf_t::add< compiler_builtins, &compiler_builtins::__cmptf2, wasm_allocator >( "env", "__cmptf2" );
   rhf_t::add< compiler_builtins, &compiler_builtins::__unordtf2, wasm_allocator >( "env", "__unordtf2" );

   rhf_t::add< console_api, &console_api::prints, wasm_allocator >( "env", "prints" );
   rhf_t::add< console_api, &console_api::prints_l, wasm_allocator >( "env", "prints_l" );
   rhf_t::add< console_api, &console_api::printi, wasm_allocator >( "env", "printi" );
   rhf_t::add< console_api, &console_api::printui, wasm_allocator >( "env", "printui" );
   rhf_t::add< console_api, &console_api::printi128, wasm_allocator >( "env", "printi128" );
   rhf_t::add< console_api, &console_api::printui128, wasm_allocator >( "env", "printui128" );
   rhf_t::add< console_api, &console_api::printsf, wasm_allocator >( "env", "printsf" );
   rhf_t::add< console_api, &console_api::printdf, wasm_allocator >( "env", "printdf" );
   rhf_t::add< console_api, &console_api::printqf, wasm_allocator >( "env", "printqf" );
   rhf_t::add< console_api, &console_api::printn, wasm_allocator >( "env", "printn" );
   rhf_t::add< console_api, &console_api::printhex, wasm_allocator >( "env", "printhex" );

   rhf_t::add< memory_api, &memory_api::memset, wasm_allocator >( "env", "memset" );
   rhf_t::add< memory_api, &memory_api::memcmp, wasm_allocator >( "env", "memcmp" );
   rhf_t::add< memory_api, &memory_api::memmove, wasm_allocator >( "env", "memmove" );
   rhf_t::add< memory_api, &memory_api::memcpy, wasm_allocator >( "env", "memcpy" );

   rhf_t::add< action_api, &action_api::current_receiver, wasm_allocator >( "env", "current_receiver" );
   rhf_t::add< action_api, &action_api::action_data_size, wasm_allocator >( "env", "action_data_size" );
   rhf_t::add< action_api, &action_api::read_action_data, wasm_allocator >( "env", "read_action_data" );

   rhf_t::add< context_free_system_api, &context_free_system_api::eosio_assert, wasm_allocator >( "env", "eosio_assert" );
   rhf_t::add< context_free_system_api, &context_free_system_api::eosio_assert_message, wasm_allocator >( "env", "eosio_assert_message" );
   rhf_t::add< context_free_system_api, &context_free_system_api::eosio_assert_code, wasm_allocator >( "env", "eosio_assert_code" );
   rhf_t::add< context_free_system_api, &context_free_system_api::eosio_exit, wasm_allocator >( "env", "eosio_exit" );
   rhf_t::add< context_free_system_api, &context_free_system_api::abort, wasm_allocator >( "env", "abort" );

   rhf_t::add< database_api, &database_api::db_store_i64, wasm_allocator >( "env", "db_store_i64" );
   rhf_t::add< database_api, &database_api::db_update_i64, wasm_allocator >( "env", "db_update_i64" );
   rhf_t::add< database_api, &database_api::db_remove_i64, wasm_allocator >( "env", "db_remove_i64" );
   rhf_t::add< database_api, &database_api::db_get_i64, wasm_allocator >( "env", "db_get_i64" );
   rhf_t::add< database_api, &database_api::db_next_i64, wasm_allocator >( "env", "db_next_i64" );
   rhf_t::add< database_api, &database_api::db_previous_i64, wasm_allocator >( "env", "db_previous_i64" );
   rhf_t::add< database_api, &database_api::db_find_i64, wasm_allocator >( "env", "db_find_i64" );
   rhf_t::add< database_api, &database_api::db_lowerbound_i64, wasm_allocator >( "env", "db_lowerbound_i64" );
   rhf_t::add< database_api, &database_api::db_upperbound_i64, wasm_allocator >( "env", "db_upperbound_i64" );
   rhf_t::add< database_api, &database_api::db_end_i64, wasm_allocator >( "env", "db_end_i64" );


   rhf_t::add< database_api, &database_api::db_idx64_store, wasm_allocator >( "env", "db_idx64_store" );
   rhf_t::add< database_api, &database_api::db_idx64_update, wasm_allocator >( "env", "db_idx64_update" );
   rhf_t::add< database_api, &database_api::db_idx64_remove, wasm_allocator >( "env", "db_idx64_remove" );
   rhf_t::add< database_api, &database_api::db_idx64_next, wasm_allocator >( "env", "db_idx64_next" );
   rhf_t::add< database_api, &database_api::db_idx64_previous, wasm_allocator >( "env", "db_idx64_previous" );
   rhf_t::add< database_api, &database_api::db_idx64_find_primary, wasm_allocator >( "env", "db_idx64_find_primary" );
   rhf_t::add< database_api, &database_api::db_idx64_find_secondary, wasm_allocator >( "env", "db_idx64_find_secondary" );
   rhf_t::add< database_api, &database_api::db_idx64_lowerbound, wasm_allocator >( "env", "db_idx64_lowerbound" );
   rhf_t::add< database_api, &database_api::db_idx64_upperbound, wasm_allocator >( "env", "db_idx64_upperbound" );
   rhf_t::add< database_api, &database_api::db_idx64_end, wasm_allocator >( "env", "db_idx64_end" );


   rhf_t::add< database_api, &database_api::db_idx128_store, wasm_allocator >( "env", "db_idx128_store" );
   rhf_t::add< database_api, &database_api::db_idx128_update, wasm_allocator >( "env", "db_idx128_update" );
   rhf_t::add< database_api, &database_api::db_idx128_remove, wasm_allocator >( "env", "db_idx128_remove" );
   rhf_t::add< database_api, &database_api::db_idx128_next, wasm_allocator >( "env", "db_idx128_next" );
   rhf_t::add< database_api, &database_api::db_idx128_previous, wasm_allocator >( "env", "db_idx128_previous" );
   rhf_t::add< database_api, &database_api::db_idx128_find_primary, wasm_allocator >( "env", "db_idx128_find_primary" );
   rhf_t::add< database_api, &database_api::db_idx128_find_secondary, wasm_allocator >( "env", "db_idx128_find_secondary" );
   rhf_t::add< database_api, &database_api::db_idx128_lowerbound, wasm_allocator >( "env", "db_idx128_lowerbound" );
   rhf_t::add< database_api, &database_api::db_idx128_upperbound, wasm_allocator >( "env", "db_idx128_upperbound" );
   rhf_t::add< database_api, &database_api::db_idx128_end, wasm_allocator >( "env", "db_idx128_end" );

   rhf_t::add< database_api, &database_api::db_idx256_store, wasm_allocator >( "env", "db_idx256_store" );
   rhf_t::add< database_api, &database_api::db_idx256_update, wasm_allocator >( "env", "db_idx256_update" );
   rhf_t::add< database_api, &database_api::db_idx256_remove, wasm_allocator >( "env", "db_idx256_remove" );
   rhf_t::add< database_api, &database_api::db_idx256_next, wasm_allocator >( "env", "db_idx256_next" );
   rhf_t::add< database_api, &database_api::db_idx256_previous, wasm_allocator >( "env", "db_idx256_previous" );
   rhf_t::add< database_api, &database_api::db_idx256_find_primary, wasm_allocator >( "env", "db_idx256_find_primary" );
   rhf_t::add< database_api, &database_api::db_idx256_find_secondary, wasm_allocator >( "env", "db_idx256_find_secondary" );
   rhf_t::add< database_api, &database_api::db_idx256_lowerbound, wasm_allocator >( "env", "db_idx256_lowerbound" );
   rhf_t::add< database_api, &database_api::db_idx256_upperbound, wasm_allocator >( "env", "db_idx256_upperbound" );
   rhf_t::add< database_api, &database_api::db_idx256_end, wasm_allocator >( "env", "db_idx256_end" );

   rhf_t::add< database_api, &database_api::db_idx_double_store, wasm_allocator >( "env", "db_idx_double_store" );
   rhf_t::add< database_api, &database_api::db_idx_double_update, wasm_allocator >( "env", "db_idx_double_update" );
   rhf_t::add< database_api, &database_api::db_idx_double_remove, wasm_allocator >( "env", "db_idx_double_remove" );
   rhf_t::add< database_api, &database_api::db_idx_double_next, wasm_allocator >( "env", "db_idx_double_next" );
   rhf_t::add< database_api, &database_api::db_idx_double_previous, wasm_allocator >( "env", "db_idx_double_previous" );
   rhf_t::add< database_api, &database_api::db_idx_double_find_primary, wasm_allocator >( "env", "db_idx_double_find_primary" );
   rhf_t::add< database_api, &database_api::db_idx_double_find_secondary, wasm_allocator >( "env", "db_idx_double_find_secondary" );
   rhf_t::add< database_api, &database_api::db_idx_double_lowerbound, wasm_allocator >( "env", "db_idx_double_lowerbound" );
   rhf_t::add< database_api, &database_api::db_idx_double_upperbound, wasm_allocator >( "env", "db_idx_double_upperbound" );
   rhf_t::add< database_api, &database_api::db_idx_double_end, wasm_allocator >( "env", "db_idx_double_end" );

   rhf_t::add< database_api, &database_api::db_idx_long_double_store, wasm_allocator >( "env", "db_idx_long_double_store" );
   rhf_t::add< database_api, &database_api::db_idx_long_double_update, wasm_allocator >( "env", "db_idx_long_double_update" );
   rhf_t::add< database_api, &database_api::db_idx_long_double_remove, wasm_allocator >( "env", "db_idx_long_double_remove" );
   rhf_t::add< database_api, &database_api::db_idx_long_double_next, wasm_allocator >( "env", "db_idx_long_double_next" );
   rhf_t::add< database_api, &database_api::db_idx_long_double_previous, wasm_allocator >( "env", "db_idx_long_double_previous" );
   rhf_t::add< database_api, &database_api::db_idx_long_double_find_primary, wasm_allocator >( "env", "db_idx_long_double_find_primary" );
   rhf_t::add< database_api, &database_api::db_idx_long_double_find_secondary, wasm_allocator >( "env", "db_idx_long_double_find_secondary" );
   rhf_t::add< database_api, &database_api::db_idx_long_double_lowerbound, wasm_allocator >( "env", "db_idx_long_double_lowerbound" );
   rhf_t::add< database_api, &database_api::db_idx_long_double_upperbound, wasm_allocator >( "env", "db_idx_long_double_upperbound" );
   rhf_t::add< database_api, &database_api::db_idx_long_double_end, wasm_allocator >( "env", "db_idx_long_double_end" );

   try
   {
      std::vector< uint8_t > wasm_bin = backend_t::read_wasm("hello.wasm");

      // Instaniate a new backend using the wasm provided.
      backend_t bkend(wasm_bin, rhf_t{});

      // Point the backend to the allocator you want it to use.
      bkend.set_wasm_allocator(&wa);
      bkend.initialize();

      chainbase::database db;
      auto tmp = boost::filesystem::current_path() / boost::filesystem::unique_path();

      db.open( tmp, 0, mira::utilities::default_database_configuration() );
      db.add_index< table_id_multi_index >();
      db.add_index< key_value_index >();
      db.add_index< index64_index >();
      db.add_index< index128_index >();
      db.add_index< index256_index >();
      db.add_index< index_double_index >();
      db.add_index< index_long_double_index >();

      koinos::chain::apply_context ctx( db );
      ctx.receiver = koinos::chain::name(0);
      bkend(&ctx, "env", "apply", (uint64_t)0, (uint64_t)0, (uint64_t)0);
      std::cout << ctx.get_pending_console_output() << std::endl;
   }
   catch( const eosio::vm::exception& e )
   {
      std::cerr << e.what() << ": " << e.detail() << std::endl;
   }
   catch( const koinos::exception::koinos_exception& e )
   {
      std::cerr << e.to_string() << std::endl;
   }
   catch (...)
   {
      std::cerr << "unknown error" << std::endl;
   }

   return 0;
}
