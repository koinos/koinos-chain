#include <algorithm>
#include <filesystem>
#include <type_traits>
#include <vector>

#include <boost/test/unit_test.hpp>
#include <boost/filesystem.hpp>

#include <koinos/log.hpp>

#include <koinos/chain/apply_context.hpp>
#include <koinos/chain/constants.hpp>
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
#include <koinos/tests/wasm/koin.hpp>
#include <koinos/tests/wasm/syscall_override.hpp>

using namespace std::string_literals;

struct transfer_args
{
   std::string from;
   std::string to;
   uint64_t    value;
};

KOINOS_REFLECT( transfer_args, (from)(to)(value) );

struct mint_args
{
   std::string to;
   uint64_t    value;
};

KOINOS_REFLECT( mint_args, (to)(value) );

struct thunk_fixture
{
   thunk_fixture() :
      host_api( ctx )
   {
      temp = std::filesystem::temp_directory_path() / boost::filesystem::unique_path().string();
      std::filesystem::create_directory( temp );
      std::any cfg = mira::utilities::default_database_configuration();

      db.open( temp, cfg );
      ctx.set_state_node( db.create_writable_node( db.get_head()->id(), koinos::crypto::hash( CRYPTO_SHA2_256_ID, 1 ) ) );
      ctx.push_frame( koinos::chain::stack_frame {
         .call = koinos::crypto::hash( CRYPTO_RIPEMD160_ID, "thunk_tests"s ).digest,
         .call_privilege = koinos::chain::privilege::kernel_mode
      } );
      ctx.push_frame( koinos::chain::stack_frame {
         .call = koinos::crypto::hash( CRYPTO_RIPEMD160_ID, "thunk_tests"s ).digest,
         .call_privilege = koinos::chain::privilege::kernel_mode
      } );

      koinos::chain::register_host_functions();
   }

   ~thunk_fixture()
   {
      db.close();
      std::filesystem::remove_all( temp );
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

   std::vector< uint8_t > get_koin_wasm()
   {
      return std::vector< uint8_t >( koin_wasm, koin_wasm + koin_wasm_len );
   }

   std::filesystem::path temp;
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
   BOOST_REQUIRE_THROW( system_call::db_put_object( ctx, KERNEL_SPACE_ID, 0, object_data ), koinos::chain::state_node_not_found );
   BOOST_REQUIRE_THROW( system_call::db_get_object( ctx, KERNEL_SPACE_ID, 0 ), koinos::chain::state_node_not_found );
   BOOST_REQUIRE_THROW( system_call::db_get_next_object( ctx, KERNEL_SPACE_ID, 0 ), koinos::chain::state_node_not_found );
   BOOST_REQUIRE_THROW( system_call::db_get_prev_object( ctx, KERNEL_SPACE_ID, 0 ), koinos::chain::state_node_not_found );

   ctx.set_state_node( node );

   koinos::pack::to_variable_blob( object_data, "object1"s );

   BOOST_TEST_MESSAGE( "Test putting an object" );

   BOOST_REQUIRE( system_call::db_put_object( ctx, KERNEL_SPACE_ID, 1, object_data ) == false );
   auto obj_blob = system_call::db_get_object( ctx, KERNEL_SPACE_ID, 1 );
   BOOST_REQUIRE( koinos::pack::from_variable_blob< std::string >( obj_blob ) == "object1" );

   BOOST_TEST_MESSAGE( "Testing getting a non-existent object" );

   obj_blob = system_call::db_get_object( ctx, KERNEL_SPACE_ID, 2 );
   BOOST_REQUIRE( obj_blob.size() == 0 );

   BOOST_TEST_MESSAGE( "Test iteration" );

   koinos::pack::to_variable_blob( object_data, "object2"s );
   system_call::db_put_object( ctx, KERNEL_SPACE_ID, 2, object_data );
   koinos::pack::to_variable_blob( object_data, "object3"s );
   system_call::db_put_object( ctx, KERNEL_SPACE_ID, 3, object_data );

   obj_blob = system_call::db_get_next_object( ctx, KERNEL_SPACE_ID, 2, 8 );
   BOOST_REQUIRE( koinos::pack::from_variable_blob< std::string >( obj_blob ) == "object3" );

   obj_blob = system_call::db_get_prev_object( ctx, KERNEL_SPACE_ID, 2, 8 );
   BOOST_REQUIRE( koinos::pack::from_variable_blob< std::string >( obj_blob ) == "object1" );

   BOOST_TEST_MESSAGE( "Test iterator overrun" );

   obj_blob = system_call::db_get_next_object( ctx, KERNEL_SPACE_ID, 3 );
   BOOST_REQUIRE( obj_blob.size() == 0 );
   obj_blob = system_call::db_get_next_object( ctx, KERNEL_SPACE_ID, 4 );
   BOOST_REQUIRE( obj_blob.size() == 0 );
   obj_blob = system_call::db_get_prev_object( ctx, KERNEL_SPACE_ID, 1 );
   BOOST_REQUIRE( obj_blob.size() == 0 );
   obj_blob = system_call::db_get_prev_object( ctx, KERNEL_SPACE_ID, 0 );
   BOOST_REQUIRE( obj_blob.size() == 0 );

   koinos::pack::to_variable_blob( object_data, "space1.object1"s );
   system_call::db_put_object( ctx, CONTRACT_SPACE_ID, 1, object_data );
   obj_blob = system_call::db_get_next_object( ctx, KERNEL_SPACE_ID, 3 );
   BOOST_REQUIRE( obj_blob.size() == 0 );
   obj_blob = system_call::db_get_next_object( ctx, CONTRACT_SPACE_ID, 1 );
   BOOST_REQUIRE( obj_blob.size() == 0 );
   obj_blob = system_call::db_get_prev_object( ctx, CONTRACT_SPACE_ID, 1 );
   BOOST_REQUIRE( obj_blob.size() == 0 );

   BOOST_TEST_MESSAGE( "Test object modification" );
   koinos::pack::to_variable_blob( object_data, "object1.1"s );
   BOOST_REQUIRE( system_call::db_put_object( ctx, KERNEL_SPACE_ID, 1, object_data ) == true );
   obj_blob = system_call::db_get_object( ctx, KERNEL_SPACE_ID, 1, 10 );
   BOOST_REQUIRE( koinos::pack::from_variable_blob< std::string >( obj_blob ) == "object1.1" );

   BOOST_TEST_MESSAGE( "Test object deletion" );
   object_data.clear();
   BOOST_REQUIRE( system_call::db_put_object( ctx, KERNEL_SPACE_ID, 1, object_data ) == true );
   obj_blob = system_call::db_get_object( ctx, KERNEL_SPACE_ID, 1, 10 );
   BOOST_REQUIRE( obj_blob.size() == 0 );

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( contract_tests )
{ try {
   BOOST_TEST_MESSAGE( "Test uploading a contract" );

   koinos::protocol::create_system_contract_operation op;
   auto id = koinos::crypto::hash( CRYPTO_RIPEMD160_ID, 1 );
   std::memcpy( op.contract_id.data(), id.digest.data(), op.contract_id.size() );
   auto bytecode = get_hello_wasm();
   op.bytecode.insert( op.bytecode.end(), bytecode.begin(), bytecode.end() );

   system_call::apply_upload_contract_operation( ctx, op );

   koinos::uint256 contract_key = koinos::pack::from_fixed_blob< koinos::uint160_t >( op.contract_id );
   auto stored_bytecode = system_call::db_get_object( ctx, CONTRACT_SPACE_ID, contract_key, bytecode.size() );

   BOOST_REQUIRE( stored_bytecode.size() == bytecode.size() );
   BOOST_REQUIRE( std::memcmp( stored_bytecode.data(), bytecode.data(), bytecode.size() ) == 0 );

   BOOST_TEST_MESSAGE( "Test executing a contract" );

   koinos::protocol::call_contract_operation op2;
   std::memcpy( op2.contract_id.data(), id.digest.data(), op2.contract_id.size() );
   system_call::apply_execute_contract_operation( ctx, op2 );
   BOOST_REQUIRE( "Greetings from koinos vm" == ctx.get_pending_console_output() );

   BOOST_REQUIRE_THROW( system_call::apply_reserved_operation( ctx, koinos::protocol::reserved_operation() ), reserved_operation_exception );

   BOOST_TEST_MESSAGE( "Test contract return" );

   // Upload the return test contract
   koinos::pack::protocol::create_system_contract_operation contract_op;
   auto return_bytecode = get_contract_return_wasm();
   auto return_id = koinos::crypto::hash( CRYPTO_RIPEMD160_ID, return_bytecode );
   std::memcpy( contract_op.contract_id.data(), return_id.digest.data(), contract_op.contract_id.size() );
   contract_op.bytecode.insert( contract_op.bytecode.end(), return_bytecode.begin(), return_bytecode.end() );
   system_call::apply_upload_contract_operation( ctx, contract_op );

   std::string arg_str = "echo";
   koinos::variable_blob args = koinos::pack::to_variable_blob( arg_str );
   auto contract_ret = system_call::execute_contract(ctx, contract_op.contract_id, 0, args);
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
   std::memcpy( contract_op.contract_id.data(), id.digest.data(), contract_op.contract_id.size() );

   contract_op.bytecode.insert( contract_op.bytecode.end(), bytecode.begin(), bytecode.end() );
   system_call::apply_upload_contract_operation( ctx, contract_op );

   // Set the system call
   koinos::protocol::set_system_call_operation call_op;
   koinos::chain::contract_call_bundle bundle;
   bundle.contract_id = contract_op.contract_id;
   bundle.entry_point = 0;
   call_op.call_id = 11675754;
   call_op.target = bundle;
   system_call::apply_set_system_call_operation( ctx, call_op );

   // Fetch the created call bundle from the database and check it
   auto call_target = koinos::pack::from_variable_blob< koinos::chain::system_call_target >( system_call::db_get_object( ctx, SYS_CALL_DISPATCH_TABLE_SPACE_ID, call_op.call_id ) );
   auto call_bundle = std::get< koinos::chain::contract_call_bundle >( call_target );
   BOOST_REQUIRE( call_bundle.contract_id == bundle.contract_id );
   BOOST_REQUIRE( call_bundle.entry_point == bundle.entry_point );

   // Ensure exception thrown on invalid contract
   auto false_id = koinos::crypto::hash( CRYPTO_RIPEMD160_ID, 1234 );
   std::memcpy( bundle.contract_id.data(), false_id.digest.data(), bundle.contract_id.size() );
   call_op.target = bundle;
   BOOST_REQUIRE_THROW( system_call::apply_set_system_call_operation( ctx, call_op ), invalid_contract );

   // Test invoking the overridden system call
   koinos::variable_blob vl_args, vl_ret;
   host_api.invoke_system_call( 11675754, vl_ret.data(), vl_ret.size(), vl_args.data(), vl_args.size() );
   BOOST_REQUIRE( "Greetings from koinos vm" == host_api.context.get_pending_console_output() );

   // Call stock prints and save the message
   koinos::chain::prints_args args;
   args.message = "Hello World";
   koinos::variable_blob vl_args2, vl_ret2;
   koinos::pack::to_variable_blob( vl_args2, args );
   host_api.invoke_system_call(
      system_call_id_type( koinos::chain::system_call_id::prints ),
      vl_ret2.data(),
      vl_ret2.size(),
      vl_args2.data(),
      vl_args2.size() );
   auto original_message = host_api.context.get_pending_console_output();

   // Override prints with a contract that prepends a message before printing
   koinos::protocol::create_system_contract_operation contract_op2;
   auto bytecode2 = get_syscall_override_wasm();
   auto id2 = koinos::crypto::hash( CRYPTO_RIPEMD160_ID, bytecode2 );
   std::memcpy( contract_op2.contract_id.data(), id2.digest.data(), contract_op2.contract_id.size() );
   contract_op2.bytecode.insert( contract_op2.bytecode.end(), bytecode2.begin(), bytecode2.end() );
   system_call::apply_upload_contract_operation( ctx, contract_op2 );

   koinos::protocol::set_system_call_operation call_op2;
   koinos::chain::contract_call_bundle bundle2;
   bundle2.contract_id = contract_op2.contract_id;
   bundle2.entry_point = 0;
   call_op2.call_id = system_call_id_type( koinos::chain::system_call_id::prints );
   call_op2.target = bundle2;
   system_call::apply_set_system_call_operation( ctx, call_op2 );

   // Now test that the message has been modified
   host_api.invoke_system_call(
      system_call_id_type( koinos::chain::system_call_id::prints ),
      vl_ret2.data(),
      vl_ret2.size(),
      vl_args2.data(),
      vl_args2.size() );
   auto new_message = host_api.context.get_pending_console_output();
   BOOST_REQUIRE( original_message != new_message );
   BOOST_REQUIRE_EQUAL( "test: Hello World", new_message );

   system_call::prints( host_api.context, original_message );
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
   host_api.invoke_thunk(
      thunk_id_type( koinos::chain::thunk_id::prints ),
      vl_ret.data(),
      vl_ret.size(),
      vl_args.data(),
      vl_args.size() );

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
   host_api.invoke_system_call(
      system_call_id_type( koinos::chain::system_call_id::prints ),
      vl_ret.data(),
      vl_ret.size(),
      vl_args.data(),
      vl_args.size() );

   BOOST_CHECK_EQUAL( vl_ret.size(), 0 );
   BOOST_REQUIRE_EQUAL( "Hello World", ctx.get_pending_console_output() );
} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( chain_thunks_test )
{ try {
   BOOST_TEST_MESSAGE( "get_head_info test" );

   auto info = koinos::chain::system_call::get_head_info( ctx );
   BOOST_CHECK_EQUAL( info.head_topology.height, 1 );
   // Test exception when null state pointer is passed
   ctx.set_state_node( nullptr );
   BOOST_REQUIRE_THROW( system_call::get_head_info( ctx ), koinos::chain::database_exception );

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( hash_thunk_test )
{ try {
   BOOST_TEST_MESSAGE( "hash thunk test" );

   std::string test_string = "hash::string";
   koinos::variable_blob blob;
   koinos::pack::to_variable_blob( blob, test_string );

   auto thunk_hash  = system_call::hash( ctx, CRYPTO_SHA2_256_ID, blob );
   auto native_hash = koinos::crypto::hash( CRYPTO_SHA2_256_ID, test_string );

   BOOST_CHECK_EQUAL( thunk_hash, native_hash );

   koinos::block_topology block_topology;
   block_topology.height = 100;
   block_topology.id = koinos::crypto::hash( CRYPTO_SHA2_256_ID, "random::id"s );
   block_topology.previous = koinos::crypto::hash( CRYPTO_SHA2_256_ID, "random::previous"s );

   koinos::pack::to_variable_blob( blob, block_topology );
   thunk_hash = system_call::hash( ctx, CRYPTO_RIPEMD160_ID, blob );
   native_hash = koinos::crypto::hash( CRYPTO_RIPEMD160_ID, block_topology );

   BOOST_CHECK_EQUAL( thunk_hash, native_hash );

   BOOST_REQUIRE_THROW( system_call::hash( ctx, 0xDEADBEEF /* unknown code */, blob ), koinos::chain::unknown_hash_code );

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( privileged_calls )
{
   ctx.set_in_user_code( true );
   BOOST_REQUIRE_THROW( system_call::apply_block( ctx, koinos::protocol::block{}, false, false, false ), koinos::chain::insufficient_privileges );
   BOOST_REQUIRE_THROW( system_call::apply_transaction( ctx, koinos::protocol::transaction() ), koinos::chain::insufficient_privileges );
   BOOST_REQUIRE_THROW( system_call::apply_reserved_operation( ctx, koinos::protocol::reserved_operation{} ), koinos::chain::insufficient_privileges );
   BOOST_REQUIRE_THROW( system_call::apply_upload_contract_operation( ctx, koinos::protocol::create_system_contract_operation{} ), koinos::chain::insufficient_privileges );
   BOOST_REQUIRE_THROW( system_call::apply_execute_contract_operation( ctx, koinos::protocol::call_contract_operation{} ), koinos::chain::insufficient_privileges );
   BOOST_REQUIRE_THROW( system_call::apply_set_system_call_operation( ctx, koinos::protocol::set_system_call_operation{} ), koinos::chain::insufficient_privileges );
}

BOOST_AUTO_TEST_CASE( last_irreversible_block_test )
{ try {

   BOOST_TEST_MESSAGE( "last irreversible block test" );
   const int last_irreversible_threshold = 6;

   for( int i = 0; i < last_irreversible_threshold; i++ )
   {
      auto lib = system_call::get_last_irreversible_block( ctx );
      BOOST_REQUIRE_EQUAL( lib, 0 );

      db.finalize_node( ctx.get_state_node()->id() );
      ctx.set_state_node( db.create_writable_node( ctx.get_state_node()->id(), koinos::crypto::hash( CRYPTO_RIPEMD160_ID, i ) ) );
   }

   auto lib = system_call::get_last_irreversible_block( ctx );
   BOOST_REQUIRE_EQUAL( lib, 1 );

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( stack_tests )
{ try {
   BOOST_TEST_MESSAGE( "apply context stack tests" );
   ctx.pop_frame();
   ctx.pop_frame();

   BOOST_REQUIRE_THROW( ctx.pop_frame(), koinos::chain::stack_exception );

   auto call1_vb = koinos::crypto::hash( CRYPTO_RIPEMD160_ID, "call1"s ).digest;
   ctx.push_frame( koinos::chain::stack_frame{ .call = call1_vb } );
   BOOST_REQUIRE_THROW( ctx.get_caller(), koinos::chain::stack_exception );

   auto call2_vb = koinos::crypto::hash( CRYPTO_RIPEMD160_ID, "call2"s ).digest;
   ctx.push_frame( koinos::chain::stack_frame{ .call = call2_vb } );
   BOOST_REQUIRE( std::equal( call1_vb.begin(), call1_vb.end(), ctx.get_caller().begin() ) );

   auto last_frame = ctx.pop_frame();
   BOOST_REQUIRE( std::equal( call2_vb.begin(), call2_vb.end(), last_frame.call.begin() ) );

   for( int i = 2; i <= APPLY_CONTEXT_STACK_LIMIT; i++ )
   {
      ctx.push_frame( koinos::chain::stack_frame{ .call = koinos::crypto::hash( CRYPTO_RIPEMD160_ID, "call"s + std::to_string(i) ).digest } );
   }

   BOOST_REQUIRE_THROW( ctx.push_frame( koinos::chain::stack_frame{} ), koinos::chain::stack_overflow );

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( require_authority )
{ try {
   auto foo_key = koinos::crypto::private_key::regenerate( koinos::crypto::hash( CRYPTO_SHA2_256_ID, "foo"s ) );
   auto foo_account_string = foo_key.get_public_key().to_address();
   koinos::variable_blob foo_account( foo_account_string.begin(), foo_account_string.end() );
   auto bar_key = koinos::crypto::private_key::regenerate( koinos::crypto::hash( CRYPTO_SHA2_256_ID, "bar"s ) );
   auto bar_account_string = bar_key.get_public_key().to_address();
   koinos::variable_blob bar_account( bar_account_string.begin(), bar_account_string.end() );

   koinos::protocol::transaction trx;
   trx.active_data = koinos::protocol::active_transaction_data{};
   ctx.set_transaction( trx );
   BOOST_REQUIRE_THROW( system_call::require_authority( ctx, foo_account ), koinos::chain::invalid_signature );

   auto signature = foo_key.sign_compact( koinos::crypto::hash( CRYPTO_SHA2_256_ID, trx.active_data ) );
   trx.signature_data = koinos::pack::variable_blob( signature.begin(), signature.end() );
   ctx.set_transaction( trx );

   system_call::require_authority( ctx, foo_account );

   BOOST_REQUIRE_THROW( system_call::require_authority( ctx, bar_account ), koinos::chain::invalid_signature );

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( transaction_nonce_test )
{ try {
   using namespace koinos;

   BOOST_TEST_MESSAGE( "Test transaction nonce" );

   variable_blob obj_blob;
   crypto::private_key key;
   std::string seed = "alpha bravo charlie delta";

   key = crypto::private_key::regenerate( crypto::hash_str( CRYPTO_SHA2_256_ID, seed.c_str(), seed.size() ) );

   protocol::transaction transaction;
   transaction.active_data.make_mutable();
   transaction.active_data->operations.push_back( protocol::nop_operation() );
   transaction.active_data->resource_limit = 20;
   transaction.active_data->nonce = 0;
   transaction.id = crypto::hash( CRYPTO_SHA2_256_ID, transaction.active_data );
   auto signature = key.sign_compact( transaction.id );
   transaction.signature_data = variable_blob( signature.begin(), signature.end() );

   system_call::apply_transaction( ctx, transaction );

   variable_blob vkey;
   pack::to_variable_blob( vkey, system_call::get_transaction_payer( ctx, transaction ) );
   pack::to_variable_blob( vkey, std::string{ KOINOS_TRANSACTION_NONCE_KEY }, true );

   statedb::object_key nonce_key;
   nonce_key = pack::from_variable_blob< statedb::object_key >( vkey );

   obj_blob = system_call::db_get_object( ctx, KERNEL_SPACE_ID, nonce_key );
   BOOST_REQUIRE( obj_blob.size() );
   BOOST_REQUIRE( pack::from_variable_blob< uint64 >( obj_blob ) == 0 );

   BOOST_TEST_MESSAGE( "Test duplicate transaction nonce" );
   transaction.active_data.make_mutable();
   transaction.active_data->resource_limit = 25;
   transaction.active_data->nonce = 0;
   transaction.id = crypto::hash( CRYPTO_SHA2_256_ID, transaction.active_data );
   signature = key.sign_compact( transaction.id );
   transaction.signature_data = variable_blob( signature.begin(), signature.end() );

   BOOST_REQUIRE_THROW( system_call::apply_transaction( ctx, transaction ), chain::chain_exception );

   obj_blob = system_call::db_get_object( ctx, KERNEL_SPACE_ID, nonce_key );
   BOOST_REQUIRE( obj_blob.size() );
   BOOST_REQUIRE( pack::from_variable_blob< uint64 >( obj_blob ) == 0 );

   BOOST_TEST_MESSAGE( "Test next transaction nonce" );
   transaction.active_data.make_mutable();
   transaction.active_data->nonce = 1;
   transaction.id = crypto::hash( CRYPTO_SHA2_256_ID, transaction.active_data );
   signature = key.sign_compact( transaction.id );
   transaction.signature_data = variable_blob( signature.begin(), signature.end() );

   system_call::apply_transaction( ctx, transaction );

   obj_blob = system_call::db_get_object( ctx, KERNEL_SPACE_ID, nonce_key );
   BOOST_REQUIRE( obj_blob.size() );
   BOOST_REQUIRE( pack::from_variable_blob< uint64 >( obj_blob ) == 1 );

   BOOST_TEST_MESSAGE( "Test duplicate transaction nonce" );
   transaction.active_data.make_mutable();
   transaction.active_data->resource_limit = 30;
   transaction.id = crypto::hash( CRYPTO_SHA2_256_ID, transaction.active_data );
   signature = key.sign_compact( transaction.id );
   transaction.signature_data = variable_blob( signature.begin(), signature.end() );

   BOOST_REQUIRE_THROW( system_call::apply_transaction( ctx, transaction ), chain::chain_exception );

   obj_blob = system_call::db_get_object( ctx, KERNEL_SPACE_ID, nonce_key );
   BOOST_REQUIRE( obj_blob.size() );
   BOOST_REQUIRE( pack::from_variable_blob< uint64 >( obj_blob ) == 1 );

   BOOST_TEST_MESSAGE( "Test next transaction nonce" );
   transaction.active_data.make_mutable();
   transaction.active_data->nonce = 2;
   transaction.id = crypto::hash( CRYPTO_SHA2_256_ID, transaction.active_data );
   signature = key.sign_compact( transaction.id );
   transaction.signature_data = variable_blob( signature.begin(), signature.end() );

   system_call::apply_transaction( ctx, transaction );

   obj_blob = system_call::db_get_object( ctx, KERNEL_SPACE_ID, nonce_key );
   BOOST_REQUIRE( obj_blob.size() );
   BOOST_REQUIRE( pack::from_variable_blob< uint64 >( obj_blob ) == 2 );
} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( get_contract_id_test )
{ try {
   auto contract_id = koinos::crypto::hash( CRYPTO_RIPEMD160_ID, "get_contract_id_test"s ).digest;

   ctx.push_frame( koinos::chain::stack_frame {
      .call = contract_id,
      .call_privilege = koinos::chain::privilege::kernel_mode
   } );

   auto id = system_call::get_contract_id( ctx );

   BOOST_REQUIRE( contract_id.size() == id.size() );
   BOOST_REQUIRE( std::equal( contract_id.begin(), contract_id.end(), id.begin() ) );
} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( token_tests )
{ try {
   using namespace koinos;

   koinos::protocol::create_system_contract_operation op;
   auto id = koinos::crypto::zero_hash( CRYPTO_RIPEMD160_ID );
   std::memcpy( op.contract_id.data(), id.digest.data(), op.contract_id.size() );
   auto bytecode = get_koin_wasm();
   op.bytecode.insert( op.bytecode.end(), bytecode.begin(), bytecode.end() );

   system_call::apply_upload_contract_operation( ctx, op );

   BOOST_TEST_MESSAGE( "Test executing a contract" );

   ctx.set_privilege( chain::privilege::user_mode );

   contract_id_type contract_id;
   std::memcpy( contract_id.data(), id.digest.data(), contract_id.size() );
   auto response = system_call::execute_contract( ctx, contract_id, 0x76ea4297, variable_blob() );

   auto name = pack::from_variable_blob< std::string >( response );
   LOG(info) << name;

   response.clear();
   response = system_call::execute_contract( ctx, contract_id, 0x7e794b24, variable_blob() );
   auto symbol = pack::from_variable_blob< std::string >( response );
   LOG(info) << symbol;

   response.clear();
   response = system_call::execute_contract( ctx, contract_id, 0x59dc15ce, variable_blob() );
   auto decimals = pack::from_variable_blob< uint8_t >( response );
   LOG(info) << (uint32_t)decimals;

   response.clear();
   response = system_call::execute_contract( ctx, contract_id, 0xcf2e8212, variable_blob() );
   auto supply = pack::from_variable_blob< uint64_t >( response );
   LOG(info) << "KOIN supply: " << supply;

   auto alice_private_key = crypto::private_key::regenerate( crypto::hash( CRYPTO_SHA2_256_ID, "alice"s ) );
   auto alice_address = alice_private_key.get_public_key().to_address();

   auto bob_private_key = crypto::private_key::regenerate( crypto::hash( CRYPTO_SHA2_256_ID, "bob"s ) );
   auto bob_address = bob_private_key.get_public_key().to_address();

   response.clear();
   response = system_call::execute_contract( ctx, contract_id, 0x15619248, pack::to_variable_blob( alice_address ) );
   auto balance = pack::from_variable_blob< uint64_t >( response );

   LOG(info) << "'alice' balance: " << balance;

   response.clear();
   response = system_call::execute_contract( ctx, contract_id, 0x15619248, pack::to_variable_blob( bob_address ) );
   balance = pack::from_variable_blob< uint64_t >( response );

   LOG(info) << "'bob' balance: " << balance;

   ctx.get_pending_console_output();

   LOG(info) << "Mint to 'alice'";
   auto m_args = mint_args{ .to = alice_address, .value = 100 };
   response.clear();
   response = system_call::execute_contract( ctx, contract_id, 0xc2f82bdc, pack::to_variable_blob( m_args ) );
   auto success = pack::from_variable_blob< bool >( response );
   BOOST_REQUIRE( !success );

   ctx.set_privilege( chain::privilege::kernel_mode );
   response.clear();
   response = system_call::execute_contract( ctx, contract_id, 0xc2f82bdc, pack::to_variable_blob( m_args ) );
   success = pack::from_variable_blob< bool >( response );
   BOOST_REQUIRE( success );

   response.clear();
   response = system_call::execute_contract( ctx, contract_id, 0x15619248, pack::to_variable_blob( alice_address ) );
   balance = pack::from_variable_blob< uint64_t >( response );

   LOG(info) << "'alice' balance: " << balance;

   response.clear();
   response = system_call::execute_contract( ctx, contract_id, 0x15619248, pack::to_variable_blob( bob_address ) );
   balance = pack::from_variable_blob< uint64_t >( response );

   LOG(info) << "'bob' balance: " << balance;

   response.clear();
   response = system_call::execute_contract( ctx, contract_id, 0xcf2e8212, variable_blob() );
   supply = pack::from_variable_blob< uint64_t >( response );
   LOG(info) << "KOIN supply: " << supply;

   LOG(info) << "Transfer from 'alice' to 'bob'";
   auto t_args = transfer_args{ .from = alice_address, .to = bob_address, .value = 25 };
   koinos::protocol::transaction trx;
   trx.active_data = koinos::protocol::active_transaction_data{};
   ctx.set_transaction( trx );
   response.clear();
   try
   {
      system_call::execute_contract( ctx, contract_id, 0x62efa292, pack::to_variable_blob( t_args ) );
      BOOST_FAIL( "Expected invalid signature exception" );
   }
   catch ( const koinos::chain::invalid_signature& ) {}

   auto signature = bob_private_key.sign_compact( koinos::crypto::hash( CRYPTO_SHA2_256_ID, trx.active_data ) );
   trx.signature_data = koinos::pack::variable_blob( signature.begin(), signature.end() );
   ctx.set_transaction( trx );

   response.clear();
   try
   {
      system_call::execute_contract( ctx, contract_id, 0x62efa292, pack::to_variable_blob( t_args ) );
      BOOST_FAIL( "Expected invalid signature exception" );
   }
   catch ( const koinos::chain::invalid_signature& ) {}

   signature = alice_private_key.sign_compact( koinos::crypto::hash( CRYPTO_SHA2_256_ID, trx.active_data ) );
   trx.signature_data = koinos::pack::variable_blob( signature.begin(), signature.end() );
   ctx.set_transaction( trx );

   response.clear();
   response = system_call::execute_contract( ctx, contract_id, 0x62efa292, pack::to_variable_blob( t_args ) );
   success = pack::from_variable_blob< bool >( response );
   BOOST_REQUIRE( success );

   response.clear();
   response = system_call::execute_contract( ctx, contract_id, 0x15619248, pack::to_variable_blob( alice_address ) );
   balance = pack::from_variable_blob< uint64_t >( response );

   LOG(info) << "'alice' balance: " << balance;

   response.clear();
   response = system_call::execute_contract( ctx, contract_id, 0x15619248, pack::to_variable_blob( bob_address ) );
   balance = pack::from_variable_blob< uint64_t >( response );

   LOG(info) << "'bob' balance: " << balance;

   response.clear();
   response = system_call::execute_contract( ctx, contract_id, 0xcf2e8212, variable_blob() );
   supply = pack::from_variable_blob< uint64_t >( response );
   LOG(info) << "KOIN supply: " << supply;

}
catch( const eosio::vm::exception& e )
{
   LOG(info) << e.what() << ": " << e.detail();
   BOOST_FAIL("EOSIO VM Exception");
}
KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( get_head_block_time )
{ try {
   using namespace koinos;
   koinos::protocol::block block;
   block.header.timestamp = 1000;
   ctx.set_block( block );

   BOOST_REQUIRE( system_call::get_head_block_time( ctx ) == block.header.timestamp );

   ctx.clear_block();

   auto vkey = pack::to_variable_blob( std::string{ KOINOS_HEAD_BLOCK_TIME_KEY } );
   vkey.resize( 32, char(0) );
   auto key = pack::from_variable_blob< statedb::object_key >( vkey );
   system_call::db_put_object( ctx, KERNEL_SPACE_ID, key, pack::to_variable_blob( block.header.timestamp ) );

   BOOST_REQUIRE( system_call::get_head_block_time( ctx ) == block.header.timestamp );
} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( pow_read_dump )
{
   koinos::rpc::chain::chain_rpc_request req = koinos::rpc::chain::read_contract_request{
      .contract_id = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 },
      .entry_point = 0x4a758831,
      .args = koinos::variable_blob()
   };
   LOG(info) << req;
}

BOOST_AUTO_TEST_SUITE_END()
