#include <boost/test/unit_test.hpp>
#include <boost/filesystem.hpp>
#include "../test_fixtures/vm_fixture.hpp"

#include <vector>

#include <chainbase/chainbase.hpp>

#include <koinos/chain/system_calls.hpp>
#include <koinos/exception.hpp>

#include <mira/database_configuration.hpp>

BOOST_FIXTURE_TEST_SUITE( vm_tests, vm_fixture )

BOOST_AUTO_TEST_CASE( vm_tests )
{
   koinos::chain::register_syscalls();
   koinos::chain::wasm_allocator_type wa;
   std::vector< uint8_t > wasm_bin = get_wasm();
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

   const char* expected_output =
R"OUTPUT(row [1]: {foo1, bar1, alice}
row [2]: {foo2, bar2, bob}
row [3]: {foo3, bar3, charlie}
)OUTPUT";

   BOOST_CHECK_EQUAL( expected_output, ctx.get_pending_console_output() );
}

BOOST_AUTO_TEST_SUITE_END()
