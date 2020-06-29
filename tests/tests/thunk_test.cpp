#include <boost/test/unit_test.hpp>

#include <koinos/log.hpp>

#include <koinos/chain/apply_context.hpp>
#include <koinos/chain/exceptions.hpp>
#include <koinos/chain/host.hpp>
#include <koinos/chain/thunk_dispatcher.hpp>
#include <koinos/chain/system_calls.hpp>

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

   koinos::types::variable_blob object_data;
   BOOST_REQUIRE_THROW( thunk::db_put_object( ctx, 0, 0, object_data ), koinos::chain::database_exception );
   BOOST_REQUIRE_THROW( thunk::db_get_object( ctx, 0, 0 ), koinos::chain::database_exception );
   BOOST_REQUIRE_THROW( thunk::db_get_next_object( ctx, 0, 0 ), koinos::chain::database_exception );
   BOOST_REQUIRE_THROW( thunk::db_get_prev_object( ctx, 0, 0 ), koinos::chain::database_exception );

   ctx.set_state_node( node );

   koinos::pack::to_variable_blob( object_data, "object1"s );

   BOOST_TEST_MESSAGE( "Test putting an object" );

   BOOST_REQUIRE( thunk::db_put_object( ctx, 0, 1, object_data ) == false );
   auto obj_blob = thunk::db_get_object( ctx, 0, 1 );
   BOOST_REQUIRE( koinos::pack::from_variable_blob< std::string >( obj_blob ) == "object1" );

   BOOST_TEST_MESSAGE( "Testing getting a non-existent object" );

   obj_blob = thunk::db_get_object( ctx, 0, 2 );
   BOOST_REQUIRE( obj_blob.size() == 0 );

   BOOST_TEST_MESSAGE( "Test iteration" );

   koinos::pack::to_variable_blob( object_data, "object2"s );
   thunk::db_put_object( ctx, 0, 2, object_data );
   koinos::pack::to_variable_blob( object_data, "object3"s );
   thunk::db_put_object( ctx, 0, 3, object_data );

   obj_blob = thunk::db_get_next_object( ctx, 0, 2, 8 );
   BOOST_REQUIRE( koinos::pack::from_variable_blob< std::string >( obj_blob ) == "object3" );

   obj_blob = thunk::db_get_prev_object( ctx, 0, 2, 8 );
   BOOST_REQUIRE( koinos::pack::from_variable_blob< std::string >( obj_blob ) == "object1" );

   BOOST_TEST_MESSAGE( "Test iterator overrun" );

   obj_blob = thunk::db_get_next_object( ctx, 0, 3 );
   BOOST_REQUIRE( obj_blob.size() == 0 );
   obj_blob = thunk::db_get_next_object( ctx, 0, 4 );
   BOOST_REQUIRE( obj_blob.size() == 0 );
   obj_blob = thunk::db_get_prev_object( ctx, 0, 1 );
   BOOST_REQUIRE( obj_blob.size() == 0 );
   obj_blob = thunk::db_get_prev_object( ctx, 0, 0 );
   BOOST_REQUIRE( obj_blob.size() == 0 );

   koinos::pack::to_variable_blob( object_data, "space1.object1"s );
   thunk::db_put_object( ctx, 1, 1, object_data );
   obj_blob = thunk::db_get_next_object( ctx, 0, 3 );
   BOOST_REQUIRE( obj_blob.size() == 0 );
   obj_blob = thunk::db_get_next_object( ctx, 1, 1 );
   BOOST_REQUIRE( obj_blob.size() == 0 );
   obj_blob = thunk::db_get_prev_object( ctx, 1, 1 );
   BOOST_REQUIRE( obj_blob.size() == 0 );

   BOOST_TEST_MESSAGE( "Test object modification" );
   koinos::pack::to_variable_blob( object_data, "object1.1"s );
   BOOST_REQUIRE( thunk::db_put_object( ctx, 0, 1, object_data ) == true );
   obj_blob = thunk::db_get_object( ctx, 0, 1, 10 );
   BOOST_REQUIRE( koinos::pack::from_variable_blob< std::string >( obj_blob ) == "object1.1" );

   BOOST_TEST_MESSAGE( "Test object deletion" );
   object_data.clear();
   BOOST_REQUIRE( thunk::db_put_object( ctx, 0, 1, object_data ) == true );
   obj_blob = thunk::db_get_object( ctx, 0, 1, 10 );
   BOOST_REQUIRE( obj_blob.size() == 0 );

} catch ( const koinos::exception& e ) { LOG(info) << e.to_string(); throw e; } }

BOOST_AUTO_TEST_CASE( contract_tests )
{ try {
   BOOST_TEST_MESSAGE( "Test uploading a contract" );

   koinos::types::protocol::create_system_contract_operation op;
   auto id = koinos::crypto::hash( CRYPTO_RIPEMD160_ID, 1 );
   memcpy( op.contract_id.data(), id.digest.data(), op.contract_id.size() );
   auto bytecode = get_hello_wasm();
   op.bytecode.insert( op.bytecode.end(), bytecode.begin(), bytecode.end() );

   thunk::apply_upload_contract_operation( ctx, op );

   koinos::types::uint256_t contract_key = koinos::pack::from_fixed_blob< koinos::types::uint160_t >( op.contract_id );
   auto stored_bytecode = thunk::db_get_object( ctx, CONTRACT_SPACE_ID, contract_key, bytecode.size() );

   BOOST_REQUIRE( stored_bytecode.size() == bytecode.size() );
   BOOST_REQUIRE( memcmp( stored_bytecode.data(), bytecode.data(), bytecode.size() ) == 0 );

   BOOST_TEST_MESSAGE( "Test executing a contract" );

   koinos::types::protocol::contract_call_operation op2;
   memcpy( op2.contract_id.data(), id.digest.data(), op2.contract_id.size() );
   thunk::apply_execute_contract_operation( ctx, op2 );
   BOOST_REQUIRE( "Greetings from koinos vm" == ctx.get_pending_console_output() );

   BOOST_REQUIRE_THROW( thunk::apply_reserved_operation( ctx, koinos::types::protocol::reserved_operation() ), reserved_operation_exception );

} catch ( const koinos::exception& e ) { LOG(info) << e.to_string(); throw e; } }

BOOST_AUTO_TEST_CASE( override_tests )
{ try {
   BOOST_TEST_MESSAGE( "Test set system call operation" );
   using namespace koinos::types;

   // Upload a test contract to use as override
   protocol::create_system_contract_operation op;
   auto id = koinos::crypto::hash( CRYPTO_RIPEMD160_ID, 1 );
   memcpy( op.contract_id.data(), id.digest.data(), op.contract_id.size() );
   auto bytecode = get_hello_wasm();
   op.bytecode.insert( op.bytecode.end(), bytecode.begin(), bytecode.end() );
   thunk::apply_upload_contract_operation( ctx, op );

   // Set the system call
   protocol::set_system_call_operation call_op;
   call_op.call_id = 11675754;
   call_op.contract_id = op.contract_id;
   call_op.entry_point = 0;
   thunk::apply_set_system_call_operation( ctx, call_op );

   // Fetch the created call bundle from the database and check it
   auto call_target = koinos::pack::from_variable_blob< system::system_call_target >( thunk::db_get_object( ctx, SYS_CALL_DISPATCH_TABLE_SPACE_ID, call_op.call_id ) );
   auto call_bundle = std::get< system::system_call_bundle >( call_target );
   BOOST_REQUIRE( call_bundle.contract_id == call_op.contract_id );
   BOOST_REQUIRE( call_bundle.entry_point == call_op.entry_point );

   // Ensure exception thrown on invalid contract
   auto false_id = koinos::crypto::hash( CRYPTO_RIPEMD160_ID, 1234 );
   memcpy( call_op.contract_id.data(), false_id.digest.data(), call_op.contract_id.size() );
   BOOST_REQUIRE_THROW( thunk::apply_set_system_call_operation( ctx, call_op ), invalid_contract );

   // Test invoking the overridden system call
   variable_blob vl_args, vl_ret;
   host_api.invoke_system_call( 11675754, vl_ret.data(), vl_ret.size(), vl_args.data(), vl_args.size() );
   BOOST_REQUIRE( "Greetings from koinos vm" == host_api.context.get_pending_console_output() );

} catch ( const koinos::exception& e ) { LOG(info) << e.to_string(); throw e; } }

BOOST_AUTO_TEST_CASE( thunk_test )
{ try {
   BOOST_TEST_MESSAGE( "thunk test" );

   using namespace koinos::chain;
   using namespace koinos::types;

   thunks::prints_args args;

   args.message = "Hello World";

   variable_blob vl_args, vl_ret;
   koinos::pack::to_variable_blob( vl_args, args );
   host_api.invoke_thunk( static_cast< system::thunk_id_type >( thunks::thunk_id::prints ), vl_ret.data(), vl_ret.size(), vl_args.data(), vl_args.size() );

   BOOST_CHECK_EQUAL( vl_ret.size(), 0 );
   BOOST_REQUIRE_EQUAL( "Hello World", ctx.get_pending_console_output() );
} catch ( const koinos::exception& e ) { LOG(info) << e.to_string(); throw e; } }

BOOST_AUTO_TEST_CASE( system_call_test )
{ try {
   BOOST_TEST_MESSAGE( "system call test" );

   using namespace koinos::chain;
   using namespace koinos::types;

   thunks::prints_args args;

   args.message = "Hello World";

   variable_blob vl_args, vl_ret;
   koinos::pack::to_variable_blob( vl_args, args );
   host_api.invoke_system_call( static_cast< system::thunk_id_type >( thunks::thunk_id::prints ), vl_ret.data(), vl_ret.size(), vl_args.data(), vl_args.size() );

   BOOST_CHECK_EQUAL( vl_ret.size(), 0 );
   BOOST_REQUIRE_EQUAL( "Hello World", ctx.get_pending_console_output() );
} catch ( const koinos::exception& e ) { LOG(info) << e.to_string(); throw e; } }


BOOST_AUTO_TEST_SUITE_END()
