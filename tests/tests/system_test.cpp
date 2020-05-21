#include <boost/test/unit_test.hpp>

#include <chainbase/chainbase.hpp>
#include <koinos/chain/system_calls.hpp>
#include <koinos/chain/exceptions.hpp>
#include <mira/database_configuration.hpp>

struct system_fixture {};

BOOST_FIXTURE_TEST_SUITE( system_tests, system_fixture )

BOOST_AUTO_TEST_CASE( system_tests )
{
   BOOST_TEST_MESSAGE( "basic system slot tests" );
   chainbase::database db;
      auto tmp = boost::filesystem::current_path() / boost::filesystem::unique_path();

   db.open( tmp, 0, mira::utilities::default_database_configuration() );

   koinos::chain::system_call_table t;
   koinos::chain::apply_context ctx( db, t );
   koinos::chain::system_api sys_api( ctx );

   BOOST_TEST_MESSAGE( "call the public system slot" );
   // This should end up calling the private native implementation and throwing `abort_called`
   BOOST_CHECK_THROW( sys_api.abort(), koinos::chain::abort_called );

   BOOST_TEST_MESSAGE( "call the private system slot in user mode" );
   // We should not be able to bypass the public system slot in user_mode
   BOOST_CHECK_THROW( sys_api.internal_abort(), koinos::chain::insufficient_privileges );

   BOOST_TEST_MESSAGE( "call the private system slot in kernel mode" );
   // If we are in kernel mode, we can call the private implementation and it should throw `abort_called`
   ctx.privilege_level = koinos::chain::privilege::kernel_mode;
   BOOST_CHECK_THROW( sys_api.internal_abort(), koinos::chain::abort_called );
}

BOOST_AUTO_TEST_SUITE_END()
