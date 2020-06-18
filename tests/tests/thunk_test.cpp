#include <boost/test/unit_test.hpp>

#include <koinos/log.hpp>

#include <koinos/chain/apply_context.hpp>
#include <koinos/chain/exceptions.hpp>
#include <koinos/chain/host.hpp>
#include <koinos/chain/thunk_dispatcher.hpp>

#include <koinos/pack/rt/binary.hpp>
#include <koinos/pack/rt/json.hpp>
#include <koinos/pack/classes.hpp>

#include <koinos/pack/rt/binary.hpp>

#include <mira/database_configuration.hpp>

#include "../test_fixtures/wasm/hello_wasm.hpp"

using namespace std::string_literals;

struct thunk_fixture
{
   thunk_fixture() :
      host_api( ctx )
   {
      temp = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
      boost::filesystem::create_directory( temp );
      boost::any cfg = mira::utilities::default_database_configuration();

      db.open( temp, cfg );
      ctx.set_state_node( db.create_writable_node( db.get_head()->id(), koinos::crypto::hash( CRYPTO_SHA2_256_ID, 1 ) ) );
      ctx.privilege_level = koinos::chain::privilege::kernel_mode;
      koinos::chain::register_host_functions();
   }

   ~thunk_fixture()
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
   koinos::chain::apply_context ctx;
   koinos::chain::host_api host_api;
};

BOOST_FIXTURE_TEST_SUITE( thunk_tests, thunk_fixture )

using namespace koinos::chain;

BOOST_AUTO_TEST_CASE( db_crud )
{ try {
   auto node = ctx.get_state_node();
   ctx.clear_state_node();

   BOOST_TEST_MESSAGE( "Test failure when apply context is not set to a state node" );

   koinos::protocol::vl_blob object_data;
   BOOST_REQUIRE_THROW( thunk::db_put_object( ctx, 0, 0, object_data ), koinos::chain::database_exception );
   BOOST_REQUIRE_THROW( thunk::db_get_object( ctx, 0, 0 ), koinos::chain::database_exception );
   BOOST_REQUIRE_THROW( thunk::db_get_next_object( ctx, 0, 0 ), koinos::chain::database_exception );
   BOOST_REQUIRE_THROW( thunk::db_get_prev_object( ctx, 0, 0 ), koinos::chain::database_exception );

   ctx.set_state_node( node );

   koinos::pack::to_vl_blob( object_data, "object1"s );

   BOOST_TEST_MESSAGE( "Test putting an object" );

   BOOST_REQUIRE( thunk::db_put_object( ctx, 0, 1, object_data ) == false );
   auto obj_blob = thunk::db_get_object( ctx, 0, 1 );
   BOOST_REQUIRE( koinos::pack::from_vl_blob< std::string >( obj_blob ) == "object1" );

   BOOST_TEST_MESSAGE( "Testing getting a non-existent object" );

   obj_blob = thunk::db_get_object( ctx, 0, 2 );
   BOOST_REQUIRE( obj_blob.data.size() == 0 );

   BOOST_TEST_MESSAGE( "Test iteration" );

   koinos::pack::to_vl_blob( object_data, "object2"s );
   thunk::db_put_object( ctx, 0, 2, object_data );
   koinos::pack::to_vl_blob( object_data, "object3"s );
   thunk::db_put_object( ctx, 0, 3, object_data );

   obj_blob = thunk::db_get_next_object( ctx, 0, 2, 8 );
   BOOST_REQUIRE( koinos::pack::from_vl_blob< std::string >( obj_blob ) == "object3" );

   obj_blob = thunk::db_get_prev_object( ctx, 0, 2, 8 );
   BOOST_REQUIRE( koinos::pack::from_vl_blob< std::string >( obj_blob ) == "object1" );

   BOOST_TEST_MESSAGE( "Test iterator overrun" );

   obj_blob = thunk::db_get_next_object( ctx, 0, 3 );
   BOOST_REQUIRE( obj_blob.data.size() == 0 );
   obj_blob = thunk::db_get_next_object( ctx, 0, 4 );
   BOOST_REQUIRE( obj_blob.data.size() == 0 );
   obj_blob = thunk::db_get_prev_object( ctx, 0, 1 );
   BOOST_REQUIRE( obj_blob.data.size() == 0 );
   obj_blob = thunk::db_get_prev_object( ctx, 0, 0 );
   BOOST_REQUIRE( obj_blob.data.size() == 0 );

   koinos::pack::to_vl_blob( object_data, "space1.object1"s );
   thunk::db_put_object( ctx, 1, 1, object_data );
   obj_blob = thunk::db_get_next_object( ctx, 0, 3 );
   BOOST_REQUIRE( obj_blob.data.size() == 0 );
   obj_blob = thunk::db_get_next_object( ctx, 1, 1 );
   BOOST_REQUIRE( obj_blob.data.size() == 0 );
   obj_blob = thunk::db_get_prev_object( ctx, 1, 1 );
   BOOST_REQUIRE( obj_blob.data.size() == 0 );

   BOOST_TEST_MESSAGE( "Test object modification" );
   koinos::pack::to_vl_blob( object_data, "object1.1"s );
   BOOST_REQUIRE( thunk::db_put_object( ctx, 0, 1, object_data ) == true );
   obj_blob = thunk::db_get_object( ctx, 0, 1, 10 );
   BOOST_REQUIRE( koinos::pack::from_vl_blob< std::string >( obj_blob ) == "object1.1" );

   BOOST_TEST_MESSAGE( "Test object deletion" );
   object_data.data.clear();
   BOOST_REQUIRE( thunk::db_put_object( ctx, 0, 1, object_data ) == true );
   obj_blob = thunk::db_get_object( ctx, 0, 1, 10 );
   BOOST_REQUIRE( obj_blob.data.size() == 0 );

} catch ( const koinos::exception& e ) { LOG(info) << e.to_string(); throw e; } }

BOOST_AUTO_TEST_CASE( contract_tests )
{ try {
   BOOST_TEST_MESSAGE( "Test uploading a contract" );

   koinos::protocol::create_system_contract_operation op;
   auto id = koinos::crypto::hash( CRYPTO_RIPEMD160_ID, 1 );
   memcpy( op.contract_id.data.data(), id.digest.data.data(), op.contract_id.data.size() );
   auto bytecode = get_hello_wasm();
   op.bytecode.data.insert( op.bytecode.data.end(), bytecode.begin(), bytecode.end() );

   thunk::apply_upload_contract_operation( ctx, op );

   koinos::protocol::uint256_t contract_key = koinos::pack::from_fl_blob< koinos::protocol::uint160_t >( op.contract_id );
   auto stored_bytecode = thunk::db_get_object( ctx, 0, contract_key, bytecode.size() );

   BOOST_REQUIRE( stored_bytecode.data.size() == bytecode.size() );
   BOOST_REQUIRE( memcmp( stored_bytecode.data.data(), bytecode.data(), bytecode.size() ) == 0 );

   BOOST_TEST_MESSAGE( "Test executing a contract" );

   koinos::protocol::contract_call_operation op2;
   memcpy( op2.contract_id.data.data(), id.digest.data.data(), op2.contract_id.data.size() );
   thunk::apply_execute_contract_operation( ctx, op2 );
   BOOST_REQUIRE( "Greetings from koinos vm" == ctx.get_pending_console_output() );

} catch ( const koinos::exception& e ) { LOG(info) << e.to_string(); throw e; } }

BOOST_AUTO_TEST_CASE( thunk_test )
{ try {
   BOOST_TEST_MESSAGE( "thunk test" );

   using namespace koinos::chain;
   using koinos::protocol::vl_blob;

   prints_args args;

   args.message = "Hello World";

   vl_blob vl_args, vl_ret;
   koinos::pack::to_vl_blob( vl_args, args );
   host_api.invoke_thunk( prints_thunk_id, vl_ret.data.data(), vl_ret.data.size(), vl_args.data.data(), vl_args.data.size() );

   BOOST_CHECK_EQUAL( vl_ret.data.size(), 0 );
   BOOST_REQUIRE_EQUAL( "Hello World", ctx.get_pending_console_output() );
} catch ( const koinos::exception& e ) { LOG(info) << e.to_string(); throw e; } }

BOOST_AUTO_TEST_CASE( xcall_test )
{ try {
   BOOST_TEST_MESSAGE( "xcall test" );

   using namespace koinos::chain;
   using koinos::protocol::vl_blob;

   prints_args args;

   args.message = "Hello World";

   vl_blob vl_args, vl_ret;
   koinos::pack::to_vl_blob( vl_args, args );
   host_api.invoke_xcall( prints_thunk_id, vl_ret.data.data(), vl_ret.data.size(), vl_args.data.data(), vl_args.data.size() );

   BOOST_CHECK_EQUAL( vl_ret.data.size(), 0 );
   BOOST_REQUIRE_EQUAL( "Hello World", ctx.get_pending_console_output() );
} catch ( const koinos::exception& e ) { LOG(info) << e.to_string(); throw e; } }

BOOST_AUTO_TEST_SUITE_END()
