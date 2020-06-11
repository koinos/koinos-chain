#include <boost/test/unit_test.hpp>

#include <koinos/chain/system_calls.hpp>
#include <koinos/chain/exceptions.hpp>
#include <koinos/log.hpp>
#include <koinos/pack/rt/binary.hpp>
#include <koinos/pack/rt/json.hpp>

#include <mira/database_configuration.hpp>

#include "../test_fixtures/wasm/hello_wasm.hpp"

using namespace std::string_literals;

struct system_fixture
{
   system_fixture() :
      ctx( t ), sys_api( ctx )
   {
      temp = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
      boost::filesystem::create_directory( temp );
      boost::any cfg = mira::utilities::default_database_configuration();

      db.open( temp, cfg );
      ctx.set_state_node( db.create_writable_node( db.get_head()->id(), koinos::crypto::hash( CRYPTO_SHA2_256_ID, 1 ) ) );
   }

   ~system_fixture()
   {
      db.close();
      boost::filesystem::remove_all( temp );
   }

   std::vector< uint8_t > get_hello_wasm()
   {
      return std::vector< uint8_t >( hello_wasm, hello_wasm + hello_wasm_len );
   }

   boost::filesystem::path temp;
   koinos::statedb::state_db db;
   koinos::chain::system_call_table t;
   koinos::chain::apply_context ctx;
   koinos::chain::system_api sys_api;
};

BOOST_FIXTURE_TEST_SUITE( system_tests, system_fixture )

BOOST_AUTO_TEST_CASE( system_tests )
{
   BOOST_TEST_MESSAGE( "basic system slot tests" );

   koinos::chain::system_call_table t;
   koinos::chain::apply_context ctx( t );
   koinos::chain::system_api sys_api( ctx );

   char print_str[] = "message";
   koinos::chain::null_terminated_ptr p_print_str(print_str);

   BOOST_TEST_MESSAGE( "call the public system slot" );
   // This should end up calling the private native implementation and throwing `abort_called`
   sys_api.prints( p_print_str );

   BOOST_TEST_MESSAGE( "call the private system slot in user mode" );
   // We should not be able to bypass the public system slot in user_mode
   BOOST_CHECK_THROW( sys_api.internal_prints( p_print_str ), koinos::chain::insufficient_privileges );

   BOOST_TEST_MESSAGE( "call the private system slot in kernel mode" );
   // If we are in kernel mode, we can call the private implementation and it should throw `abort_called`
   ctx.privilege_level = koinos::chain::privilege::kernel_mode;
   sys_api.internal_prints( p_print_str );
}

BOOST_AUTO_TEST_CASE( db_crud )
{ try {
   auto node = ctx.get_state_node();
   ctx.clear_state_node();

   BOOST_TEST_MESSAGE( "Test failure when apply context is not set to a state node" );

   koinos::protocol::vl_blob object_data;
   BOOST_REQUIRE_THROW( sys_api.db_put_object( 0, 0, object_data ), koinos::chain::database_exception );
   BOOST_REQUIRE_THROW( sys_api.db_get_object( 0, 0 ), koinos::chain::database_exception );
   BOOST_REQUIRE_THROW( sys_api.db_get_next_object( 0, 0 ), koinos::chain::database_exception );
   BOOST_REQUIRE_THROW( sys_api.db_get_prev_object( 0, 0 ), koinos::chain::database_exception );

   ctx.set_state_node( node );

   koinos::pack::to_vl_blob( object_data, "object1"s );

   BOOST_TEST_MESSAGE( "Test putting an object" );

   BOOST_REQUIRE( sys_api.db_put_object( 0, 1, object_data ) == false );
   auto obj_blob = sys_api.db_get_object( 0, 1 );
   BOOST_REQUIRE( koinos::pack::from_vl_blob< std::string >( obj_blob ) == "object1" );

   BOOST_TEST_MESSAGE( "Testing getting a non-existent object" );

   obj_blob = sys_api.db_get_object( 0, 2 );
   BOOST_REQUIRE( obj_blob.data.size() == 0 );

   BOOST_TEST_MESSAGE( "Test iteration" );

   koinos::pack::to_vl_blob( object_data, "object2"s );
   sys_api.db_put_object( 0, 2, object_data );
   koinos::pack::to_vl_blob( object_data, "object3"s );
   sys_api.db_put_object( 0, 3, object_data );

   obj_blob = sys_api.db_get_next_object( 0, 2, 8 );
   BOOST_REQUIRE( koinos::pack::from_vl_blob< std::string >( obj_blob ) == "object3" );

   obj_blob = sys_api.db_get_prev_object( 0, 2, 8 );
   BOOST_REQUIRE( koinos::pack::from_vl_blob< std::string >( obj_blob ) == "object1" );

   BOOST_TEST_MESSAGE( "Test iterator overrun" );

   obj_blob = sys_api.db_get_next_object( 0, 3 );
   BOOST_REQUIRE( obj_blob.data.size() == 0 );
   obj_blob = sys_api.db_get_next_object( 0, 4 );
   BOOST_REQUIRE( obj_blob.data.size() == 0 );
   obj_blob = sys_api.db_get_prev_object( 0, 1 );
   BOOST_REQUIRE( obj_blob.data.size() == 0 );
   obj_blob = sys_api.db_get_prev_object( 0, 0 );
   BOOST_REQUIRE( obj_blob.data.size() == 0 );

   koinos::pack::to_vl_blob( object_data, "space1.object1"s );
   sys_api.db_put_object( 1, 1, object_data );
   obj_blob = sys_api.db_get_next_object( 0, 3 );
   BOOST_REQUIRE( obj_blob.data.size() == 0 );
   obj_blob = sys_api.db_get_next_object( 1, 1 );
   BOOST_REQUIRE( obj_blob.data.size() == 0 );
   obj_blob = sys_api.db_get_prev_object( 1, 1 );
   BOOST_REQUIRE( obj_blob.data.size() == 0 );

   BOOST_TEST_MESSAGE( "Test object modification" );
   koinos::pack::to_vl_blob( object_data, "object1.1"s );
   BOOST_REQUIRE( sys_api.db_put_object( 0, 1, object_data ) == true );
   obj_blob = sys_api.db_get_object( 0, 1, 10 );
   BOOST_REQUIRE( koinos::pack::from_vl_blob< std::string >( obj_blob ) == "object1.1" );

   BOOST_TEST_MESSAGE( "Test object deletion" );
   object_data.data.clear();
   BOOST_REQUIRE( sys_api.db_put_object( 0, 1, object_data ) == true );
   obj_blob = sys_api.db_get_object( 0, 1, 10 );
   BOOST_REQUIRE( obj_blob.data.size() == 0 );

} catch ( const koinos::exception& e ) { LOG(info) << e.to_string(); throw e; } }

BOOST_AUTO_TEST_CASE( upload_contract )
{ try {
   BOOST_TEST_MESSAGE( "Test uploading a contract" );

   koinos::protocol::create_system_contract_operation op;
   auto id = koinos::crypto::hash( CRYPTO_RIPEMD160_ID, 1 );
   memcpy( op.contract_id.data.data(), id.digest.data.data(), op.contract_id.data.size() );
   auto bytecode = get_hello_wasm();
   op.bytecode.data.insert( op.bytecode.data.end(), bytecode.begin(), bytecode.end() );

   sys_api.apply_upload_contract_operation( op );

   koinos::protocol::uint256_t contract_key = koinos::pack::from_fl_blob< koinos::protocol::uint160_t >( op.contract_id );
   auto stored_bytecode = sys_api.db_get_object( 0, contract_key, bytecode.size() );

   BOOST_REQUIRE( stored_bytecode.data.size() == bytecode.size() );
   BOOST_REQUIRE( memcmp( stored_bytecode.data.data(), bytecode.data(), bytecode.size() ) == 0 );

} catch ( const koinos::exception& e ) { LOG(info) << e.to_string(); throw e; } }

BOOST_AUTO_TEST_SUITE_END()
