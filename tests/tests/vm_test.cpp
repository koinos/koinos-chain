#include <boost/test/unit_test.hpp>
#include <boost/filesystem.hpp>
#include "../test_fixtures/vm_fixture.hpp"

#include <vector>

#include <koinos/chain/system_calls.hpp>
#include <koinos/exception.hpp>

#include <mira/database_configuration.hpp>

BOOST_FIXTURE_TEST_SUITE( vm_tests, vm_fixture )

BOOST_AUTO_TEST_CASE( vm_tests )
{
   koinos::chain::register_host_functions();
   koinos::chain::wasm_allocator_type wa;
   std::vector< uint8_t > wasm_bin = get_hello_wasm();
   koinos::chain::backend_type bkend( wasm_bin, koinos::chain::registrar_type{} );

   bkend.set_wasm_allocator(&wa);
   bkend.initialize();

   koinos::chain::apply_context ctx;
   bkend(&ctx, "env", "apply", (uint64_t)0, (uint64_t)0, (uint64_t)0);

   const char* expected_output =
R"OUTPUT(Greetings from koinos vm)OUTPUT";

   BOOST_CHECK_EQUAL( expected_output, ctx.get_pending_console_output() );
}

BOOST_AUTO_TEST_SUITE_END()
