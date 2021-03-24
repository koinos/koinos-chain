#include <vector>
#include <algorithm>

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

#include <koinos/tests/wasm/contract_return.hpp>
#include <koinos/tests/wasm/hello.hpp>
#include <koinos/tests/wasm/syscall_override.hpp>

using namespace std::string_literals;

struct thunk_fixture
{
   thunk_fixture() :
      host_api( ctx )
   {
      temp = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
      boost::filesystem::create_directory( temp );
      std::any cfg = mira::utilities::default_database_configuration();

      db.open( temp, cfg );
      ctx.set_state_node( db.create_writable_node( db.get_head()->id(), koinos::crypto::hash( CRYPTO_SHA2_256_ID, 1 ) ) );
      ctx.push_frame( koinos::chain::stack_frame {
         .call = koinos::pack::to_variable_blob( std::string( "thunk_tests" ) ),
         .call_privilege = koinos::chain::privilege::kernel_mode
      } );

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

   std::vector< uint8_t > get_contract_return_wasm()
   {
      return std::vector< uint8_t >( contract_return_wasm, contract_return_wasm + contract_return_wasm_len );
   }

   std::vector< uint8_t > get_syscall_override_wasm()
   {
      return std::vector< uint8_t >( syscall_override_wasm, syscall_override_wasm + syscall_override_wasm_len );
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

   koinos::variable_blob object_data;
   BOOST_REQUIRE_THROW( thunk::db_put_object( ctx, KERNEL_SPACE_ID, 0, object_data ), koinos::chain::database_exception );
   BOOST_REQUIRE_THROW( thunk::db_get_object( ctx, KERNEL_SPACE_ID, 0 ), koinos::chain::database_exception );
   BOOST_REQUIRE_THROW( thunk::db_get_next_object( ctx, KERNEL_SPACE_ID, 0 ), koinos::chain::database_exception );
   BOOST_REQUIRE_THROW( thunk::db_get_prev_object( ctx, KERNEL_SPACE_ID, 0 ), koinos::chain::database_exception );

   ctx.set_state_node( node );

   koinos::pack::to_variable_blob( object_data, "object1"s );

   BOOST_TEST_MESSAGE( "Test putting an object" );

   BOOST_REQUIRE( thunk::db_put_object( ctx, KERNEL_SPACE_ID, 1, object_data ) == false );
   auto obj_blob = thunk::db_get_object( ctx, KERNEL_SPACE_ID, 1 );
   BOOST_REQUIRE( koinos::pack::from_variable_blob< std::string >( obj_blob ) == "object1" );

   BOOST_TEST_MESSAGE( "Testing getting a non-existent object" );

   obj_blob = thunk::db_get_object( ctx, KERNEL_SPACE_ID, 2 );
   BOOST_REQUIRE( obj_blob.size() == 0 );

   BOOST_TEST_MESSAGE( "Test iteration" );

   koinos::pack::to_variable_blob( object_data, "object2"s );
   thunk::db_put_object( ctx, KERNEL_SPACE_ID, 2, object_data );
   koinos::pack::to_variable_blob( object_data, "object3"s );
   thunk::db_put_object( ctx, KERNEL_SPACE_ID, 3, object_data );

   obj_blob = thunk::db_get_next_object( ctx, KERNEL_SPACE_ID, 2, 8 );
   BOOST_REQUIRE( koinos::pack::from_variable_blob< std::string >( obj_blob ) == "object3" );

   obj_blob = thunk::db_get_prev_object( ctx, KERNEL_SPACE_ID, 2, 8 );
   BOOST_REQUIRE( koinos::pack::from_variable_blob< std::string >( obj_blob ) == "object1" );

   BOOST_TEST_MESSAGE( "Test iterator overrun" );

   obj_blob = thunk::db_get_next_object( ctx, KERNEL_SPACE_ID, 3 );
   BOOST_REQUIRE( obj_blob.size() == 0 );
   obj_blob = thunk::db_get_next_object( ctx, KERNEL_SPACE_ID, 4 );
   BOOST_REQUIRE( obj_blob.size() == 0 );
   obj_blob = thunk::db_get_prev_object( ctx, KERNEL_SPACE_ID, 1 );
   BOOST_REQUIRE( obj_blob.size() == 0 );
   obj_blob = thunk::db_get_prev_object( ctx, KERNEL_SPACE_ID, 0 );
   BOOST_REQUIRE( obj_blob.size() == 0 );

   koinos::pack::to_variable_blob( object_data, "space1.object1"s );
   thunk::db_put_object( ctx, CONTRACT_SPACE_ID, 1, object_data );
   obj_blob = thunk::db_get_next_object( ctx, KERNEL_SPACE_ID, 3 );
   BOOST_REQUIRE( obj_blob.size() == 0 );
   obj_blob = thunk::db_get_next_object( ctx, CONTRACT_SPACE_ID, 1 );
   BOOST_REQUIRE( obj_blob.size() == 0 );
   obj_blob = thunk::db_get_prev_object( ctx, CONTRACT_SPACE_ID, 1 );
   BOOST_REQUIRE( obj_blob.size() == 0 );

   BOOST_TEST_MESSAGE( "Test object modification" );
   koinos::pack::to_variable_blob( object_data, "object1.1"s );
   BOOST_REQUIRE( thunk::db_put_object( ctx, KERNEL_SPACE_ID, 1, object_data ) == true );
   obj_blob = thunk::db_get_object( ctx, KERNEL_SPACE_ID, 1, 10 );
   BOOST_REQUIRE( koinos::pack::from_variable_blob< std::string >( obj_blob ) == "object1.1" );

   BOOST_TEST_MESSAGE( "Test object deletion" );
   object_data.clear();
   BOOST_REQUIRE( thunk::db_put_object( ctx, KERNEL_SPACE_ID, 1, object_data ) == true );
   obj_blob = thunk::db_get_object( ctx, KERNEL_SPACE_ID, 1, 10 );
   BOOST_REQUIRE( obj_blob.size() == 0 );

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( contract_tests )
{ try {
   BOOST_TEST_MESSAGE( "Test uploading a contract" );

   koinos::protocol::create_system_contract_operation op;
   auto id = koinos::crypto::hash( CRYPTO_RIPEMD160_ID, 1 );
   memcpy( op.contract_id.data(), id.digest.data(), op.contract_id.size() );
   auto bytecode = get_hello_wasm();
   op.bytecode.insert( op.bytecode.end(), bytecode.begin(), bytecode.end() );

   thunk::apply_upload_contract_operation( ctx, op );

   koinos::uint256 contract_key = koinos::pack::from_fixed_blob< koinos::uint160_t >( op.contract_id );
   auto stored_bytecode = thunk::db_get_object( ctx, CONTRACT_SPACE_ID, contract_key, bytecode.size() );

   BOOST_REQUIRE( stored_bytecode.size() == bytecode.size() );
   BOOST_REQUIRE( memcmp( stored_bytecode.data(), bytecode.data(), bytecode.size() ) == 0 );

   BOOST_TEST_MESSAGE( "Test executing a contract" );

   koinos::protocol::contract_call_operation op2;
   memcpy( op2.contract_id.data(), id.digest.data(), op2.contract_id.size() );
   thunk::apply_execute_contract_operation( ctx, op2 );
   BOOST_REQUIRE( "Greetings from koinos vm" == ctx.get_pending_console_output() );

   BOOST_REQUIRE_THROW( thunk::apply_reserved_operation( ctx, koinos::protocol::reserved_operation() ), reserved_operation_exception );

   BOOST_TEST_MESSAGE( "Test contract return" );

   // Upload the return test contract
   koinos::pack::protocol::create_system_contract_operation contract_op;
   auto return_bytecode = get_contract_return_wasm();
   auto return_id = koinos::crypto::hash( CRYPTO_RIPEMD160_ID, return_bytecode );
   memcpy( contract_op.contract_id.data(), return_id.digest.data(), contract_op.contract_id.size() );
   contract_op.bytecode.insert( contract_op.bytecode.end(), return_bytecode.begin(), return_bytecode.end() );
   thunk::apply_upload_contract_operation( ctx, contract_op );

   std::string arg_str = "echo";
   koinos::variable_blob args = koinos::pack::to_variable_blob( arg_str );
   auto contract_ret = thunk::execute_contract(ctx, contract_op.contract_id, 0, args);
   auto return_str = koinos::pack::from_variable_blob< std::string >( contract_ret );
   BOOST_REQUIRE_EQUAL( arg_str, return_str );

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( override_tests )
{ try {
   BOOST_TEST_MESSAGE( "Test set system call operation" );

   // Upload a test contract to use as override
   koinos::protocol::create_system_contract_operation contract_op;
   auto bytecode = get_hello_wasm();
   auto id = koinos::crypto::hash( CRYPTO_RIPEMD160_ID, bytecode );
   memcpy( contract_op.contract_id.data(), id.digest.data(), contract_op.contract_id.size() );

   contract_op.bytecode.insert( contract_op.bytecode.end(), bytecode.begin(), bytecode.end() );
   thunk::apply_upload_contract_operation( ctx, contract_op );

   // Set the system call
   koinos::protocol::set_system_call_operation call_op;
   koinos::chain::contract_call_bundle bundle;
   bundle.contract_id = contract_op.contract_id;
   bundle.entry_point = 0;
   call_op.call_id = 11675754;
   call_op.target = bundle;
   thunk::apply_set_system_call_operation( ctx, call_op );

   // Fetch the created call bundle from the database and check it
   auto call_target = koinos::pack::from_variable_blob< koinos::chain::system_call_target >( thunk::db_get_object( ctx, SYS_CALL_DISPATCH_TABLE_SPACE_ID, call_op.call_id ) );
   auto call_bundle = std::get< koinos::chain::contract_call_bundle >( call_target );
   BOOST_REQUIRE( call_bundle.contract_id == bundle.contract_id );
   BOOST_REQUIRE( call_bundle.entry_point == bundle.entry_point );

   // Ensure exception thrown on invalid contract
   auto false_id = koinos::crypto::hash( CRYPTO_RIPEMD160_ID, 1234 );
   memcpy( bundle.contract_id.data(), false_id.digest.data(), bundle.contract_id.size() );
   call_op.target = bundle;
   BOOST_REQUIRE_THROW( thunk::apply_set_system_call_operation( ctx, call_op ), invalid_contract );

   // Test invoking the overridden system call
   koinos::variable_blob vl_args, vl_ret;
   host_api.invoke_system_call( 11675754, vl_ret.data(), vl_ret.size(), vl_args.data(), vl_args.size() );
   BOOST_REQUIRE( "Greetings from koinos vm" == host_api.context.get_pending_console_output() );

   // Call stock prints and save the message
   koinos::chain::prints_args args;
   args.message = "Hello World";
   koinos::variable_blob vl_args2, vl_ret2;
   koinos::pack::to_variable_blob( vl_args2, args );
   host_api.invoke_system_call( uint32_t( koinos::chain::system_call_id::prints ), vl_ret2.data(), vl_ret2.size(), vl_args2.data(), vl_args2.size() );
   auto original_message = host_api.context.get_pending_console_output();

   // Override prints with a contract that prepends a message before printing
   koinos::protocol::create_system_contract_operation contract_op2;
   auto bytecode2 = get_syscall_override_wasm();
   auto id2 = koinos::crypto::hash( CRYPTO_RIPEMD160_ID, bytecode2 );
   memcpy( contract_op2.contract_id.data(), id2.digest.data(), contract_op2.contract_id.size() );
   contract_op2.bytecode.insert( contract_op2.bytecode.end(), bytecode2.begin(), bytecode2.end() );
   thunk::apply_upload_contract_operation( ctx, contract_op2 );

   koinos::protocol::set_system_call_operation call_op2;
   koinos::chain::contract_call_bundle bundle2;
   bundle2.contract_id = contract_op2.contract_id;
   bundle2.entry_point = 0;
   call_op2.call_id = uint32_t( koinos::chain::system_call_id::prints );
   call_op2.target = bundle2;
   thunk::apply_set_system_call_operation( ctx, call_op2 );

   // Now test that the message has been modified
   host_api.invoke_system_call( uint32_t( koinos::chain::system_call_id::prints ), vl_ret2.data(), vl_ret2.size(), vl_args2.data(), vl_args2.size() );
   auto new_message = host_api.context.get_pending_console_output();
   BOOST_REQUIRE( original_message != new_message );
   BOOST_REQUIRE_EQUAL( "test: Hello World", new_message );

   thunk::prints( host_api.context, original_message );
   new_message = host_api.context.get_pending_console_output();
   BOOST_REQUIRE( original_message != new_message );
   BOOST_REQUIRE_EQUAL( "test: Hello World", new_message );

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( thunk_test )
{ try {
   BOOST_TEST_MESSAGE( "thunk test" );

   using namespace koinos::chain;

   koinos::chain::prints_args args;

   args.message = "Hello World";

   koinos::variable_blob vl_args, vl_ret;
   koinos::pack::to_variable_blob( vl_args, args );
   host_api.invoke_thunk( uint32_t( koinos::chain::thunk_id::prints ), vl_ret.data(), vl_ret.size(), vl_args.data(), vl_args.size() );

   BOOST_CHECK_EQUAL( vl_ret.size(), 0 );
   BOOST_REQUIRE_EQUAL( "Hello World", ctx.get_pending_console_output() );
} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( system_call_test )
{ try {
   BOOST_TEST_MESSAGE( "system call test" );

   using namespace koinos::chain;

   koinos::chain::prints_args args;

   args.message = "Hello World";

   koinos::variable_blob vl_args, vl_ret;
   koinos::pack::to_variable_blob( vl_args, args );
   host_api.invoke_system_call( uint32_t( koinos::chain::system_call_id::prints ), vl_ret.data(), vl_ret.size(), vl_args.data(), vl_args.size() );

   BOOST_CHECK_EQUAL( vl_ret.size(), 0 );
   BOOST_REQUIRE_EQUAL( "Hello World", ctx.get_pending_console_output() );
} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( chain_thunks_test )
{ try {
   BOOST_TEST_MESSAGE( "get_head_info test" );

   auto info = koinos::chain::thunk::get_head_info( ctx );
   BOOST_CHECK_EQUAL( info.head_topology.height, 1 );
   // Test exception when null state pointer is passed
   ctx.set_state_node( nullptr );
   BOOST_REQUIRE_THROW( thunk::get_head_info( ctx ), koinos::chain::database_exception );

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( hash_thunk_test )
{ try {
   BOOST_TEST_MESSAGE( "hash thunk test" );

   std::string test_string = "hash::string";
   koinos::variable_blob blob;
   koinos::pack::to_variable_blob( blob, test_string );

   auto thunk_hash  = thunk::hash( ctx, CRYPTO_SHA2_256_ID, blob );
   auto native_hash = koinos::crypto::hash( CRYPTO_SHA2_256_ID, test_string );

   BOOST_CHECK_EQUAL( thunk_hash, native_hash );

   koinos::block_topology block_topology;
   block_topology.height = 100;
   block_topology.id = koinos::crypto::hash( CRYPTO_SHA2_256_ID, "random::id"s );
   block_topology.previous = koinos::crypto::hash( CRYPTO_SHA2_256_ID, "random::previous"s );

   koinos::pack::to_variable_blob( blob, block_topology );
   thunk_hash = thunk::hash( ctx, CRYPTO_RIPEMD160_ID, blob );
   native_hash = koinos::crypto::hash( CRYPTO_RIPEMD160_ID, block_topology );

   BOOST_CHECK_EQUAL( thunk_hash, native_hash );

   BOOST_REQUIRE_THROW( thunk::hash( ctx, 0xDEADBEEF /* unknown code */, blob ), koinos::chain::thunk::unknown_hash_code );

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( privileged_calls )
{
   ctx.set_in_user_code( true );
   BOOST_REQUIRE_THROW( thunk::apply_block( ctx, koinos::protocol::block{}, false, false, false ), koinos::chain::thunk::thunk_privilege_error );
   BOOST_REQUIRE_THROW( thunk::apply_transaction( ctx, koinos::protocol::transaction() ), koinos::chain::thunk::thunk_privilege_error );
   BOOST_REQUIRE_THROW( thunk::apply_reserved_operation( ctx, koinos::protocol::reserved_operation{} ), koinos::chain::thunk::thunk_privilege_error );
   BOOST_REQUIRE_THROW( thunk::apply_upload_contract_operation( ctx, koinos::protocol::create_system_contract_operation{} ), koinos::chain::thunk::thunk_privilege_error );
   BOOST_REQUIRE_THROW( thunk::apply_execute_contract_operation( ctx, koinos::protocol::contract_call_operation{} ), koinos::chain::thunk::thunk_privilege_error );
   BOOST_REQUIRE_THROW( thunk::apply_set_system_call_operation( ctx, koinos::protocol::set_system_call_operation{} ), koinos::chain::thunk::thunk_privilege_error );
}

BOOST_AUTO_TEST_CASE( last_irreversible_block_test )
{ try {

   BOOST_TEST_MESSAGE( "last irreversible block test" );
   const int last_irreversible_threshold = 6;

   for( int i = 0; i < last_irreversible_threshold; i++ )
   {
      auto lib = thunk::get_last_irreversible_block( ctx );
      BOOST_REQUIRE_EQUAL( lib, 0 );

      db.finalize_node( ctx.get_state_node()->id() );
      ctx.set_state_node( db.create_writable_node( ctx.get_state_node()->id(), koinos::crypto::hash( CRYPTO_RIPEMD160_ID, i ) ) );
   }

   auto lib = thunk::get_last_irreversible_block( ctx );
   BOOST_REQUIRE_EQUAL( lib, 1 );

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( stack_tests )
{ try {
   BOOST_TEST_MESSAGE( "apply context stack tests" );
   ctx.pop_frame();

   BOOST_REQUIRE_THROW( ctx.pop_frame(), koinos::chain::stack_exception );

   auto call1_vb = koinos::pack::to_variable_blob( "call1"s );
   ctx.push_frame( koinos::chain::stack_frame{ .call = call1_vb } );
   BOOST_REQUIRE_THROW( ctx.get_caller(), koinos::chain::stack_exception );

   auto call2_vb = koinos::pack::to_variable_blob( "call2"s );
   ctx.push_frame( koinos::chain::stack_frame{ .call = call2_vb } );
   BOOST_REQUIRE( std::equal( call1_vb.begin(), call1_vb.end(), ctx.get_caller().begin() ) );
   BOOST_REQUIRE( std::equal( call1_vb.begin(), call1_vb.end(), thunk::get_caller(ctx).begin() ) );

   auto last_frame = ctx.pop_frame();
   BOOST_REQUIRE( std::equal( call2_vb.begin(), call2_vb.end(), last_frame.call.begin() ) );

   for( int i = 2; i <= APPLY_CONTEXT_STACK_LIMIT; i++ )
   {
      ctx.push_frame( koinos::chain::stack_frame{ .call = koinos::pack::to_variable_blob( "call"s + std::to_string(i) ) } );
   }

   BOOST_REQUIRE_THROW( ctx.push_frame( koinos::chain::stack_frame{} ), koinos::chain::stack_overflow );

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( require_authority )
{ try {
   auto foo_key = koinos::crypto::private_key::regenerate( koinos::crypto::hash( CRYPTO_SHA2_256_ID, "foo"s ) );
   auto bar_key = koinos::crypto::private_key::regenerate( koinos::crypto::hash( CRYPTO_SHA2_256_ID, "bar"s ) );

   koinos::protocol::transaction trx;
   auto signature = foo_key.sign_compact( koinos::crypto::hash( CRYPTO_SHA2_256_ID, trx.active_data.get_const_native() ) );
   trx.signature_data = koinos::pack::variable_blob( signature.begin(), signature.end() );
   ctx.set_transaction( trx );

   auto foo_account = koinos::pack::to_variable_blob( foo_key.get_public_key().to_address() );
   auto bar_account = koinos::pack::to_variable_blob( bar_key.get_public_key().to_address() );
   thunk::require_authority( ctx, foo_account );

   BOOST_REQUIRE_THROW( thunk::require_authority( ctx, bar_account ), koinos::chain::thunk::invalid_signature );

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_SUITE_END()
