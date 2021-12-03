#include <algorithm>
#include <filesystem>
#include <type_traits>
#include <vector>

#include <boost/test/unit_test.hpp>
#include <boost/filesystem.hpp>

#include <koinos/log.hpp>

#include <koinos/chain/controller.hpp>
#include <koinos/chain/execution_context.hpp>
#include <koinos/chain/constants.hpp>
#include <koinos/chain/exceptions.hpp>
#include <koinos/chain/host_api.hpp>
#include <koinos/chain/thunk_dispatcher.hpp>
#include <koinos/chain/session.hpp>
#include <koinos/chain/state.hpp>
#include <koinos/chain/system_calls.hpp>

#include <koinos/crypto/elliptic.hpp>

#include <koinos/vm_manager/exceptions.hpp>

#include <koinos/contracts/token/token.pb.h>

#include <koinos/tests/wasm/contract_return.hpp>
#include <koinos/tests/wasm/forever.hpp>
#include <koinos/tests/wasm/hello.hpp>
#include <koinos/tests/wasm/koin.hpp>
#include <koinos/tests/wasm/syscall_override.hpp>

#include <koinos/util/hex.hpp>

using namespace koinos;
using namespace std::string_literals;

struct thunk_fixture
{
   thunk_fixture() :
      vm_backend( koinos::vm_manager::get_vm_backend() ),
      ctx( vm_backend, chain::intent::block_application ),
      host( ctx )
   {
      KOINOS_ASSERT( vm_backend, koinos::chain::unknown_backend_exception, "Couldn't get VM backend" );

      initialize_logging( "koinos_test", {}, "info" );

      temp = std::filesystem::temp_directory_path() / boost::filesystem::unique_path().string();
      std::filesystem::create_directory( temp );

      auto seed = "test seed"s;
      _signing_private_key = crypto::private_key::regenerate( crypto::hash( crypto::multicodec::sha2_256, seed ) );

      chain::genesis_data genesis_data;
      auto chain_id = crypto::hash( crypto::multicodec::sha2_256, _signing_private_key.get_public_key().to_address_bytes() );
      genesis_data[ { chain::state::space::metadata(), chain::state::key::chain_id } ] = util::converter::as< std::string >( chain_id );

      db.open( temp, [&]( state_db::state_node_ptr root )
      {
         for ( const auto& entry : genesis_data )
         {
            auto value = util::converter::as< state_db::object_value >( entry.second );
            auto res = root->put_object( entry.first.first, entry.first.second, &value );

            KOINOS_ASSERT(
               res == value.size(),
               chain::unexpected_state,
               "encountered unexpected object in initial state"
            );
         }
      } );

      ctx.set_state_node( db.create_writable_node( db.get_head()->id(), crypto::hash( crypto::multicodec::sha2_256, 1 ) ) );
      ctx.push_frame( chain::stack_frame {
         .contract_id = "thunk_tests"s,
         .system = true,
         .call_privilege = chain::privilege::kernel_mode
      } );

      ctx.resource_meter().set_resource_limit_data( chain::system_call::get_resource_limits( ctx ) );

      vm_backend->initialize();
   }

   ~thunk_fixture()
   {
      boost::log::core::get()->remove_all_sinks();
      db.close();
      std::filesystem::remove_all( temp );
   }

   void sign_transaction( protocol::transaction& transaction, crypto::private_key& transaction_signing_key )
   {
      // Signature is on the hash of the active data
      auto id_mh = crypto::hash( crypto::multicodec::sha2_256, transaction.active() );
      transaction.set_id( util::converter::as< std::string >( id_mh ) );
      transaction.set_signature_data( util::converter::as< std::string >( transaction_signing_key.sign_compact( id_mh ) ) );
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

   std::vector< uint8_t > get_forever_wasm()
   {
      return std::vector< uint8_t >( forever_wasm, forever_wasm + forever_wasm_len );
   }

   std::filesystem::path temp;
   koinos::state_db::database db;
   std::shared_ptr< koinos::vm_manager::vm_backend > vm_backend;
   koinos::chain::execution_context ctx;
   koinos::chain::host_api host;
   koinos::crypto::private_key _signing_private_key;
};

BOOST_FIXTURE_TEST_SUITE( thunk_tests, thunk_fixture )

BOOST_AUTO_TEST_CASE( db_crud )
{ try {
   std::string object_data;

   // This test expects chain ID not to be present in the database, so
   // we begin by removing it
   BOOST_REQUIRE(
      chain::system_call::put_object(
         ctx,
         chain::state::space::metadata(),
         chain::state::key::chain_id,
         object_data
      ) == true
   );

   auto node = std::dynamic_pointer_cast< koinos::state_db::state_node >( ctx.get_state_node() );
   ctx.clear_state_node();

   BOOST_TEST_MESSAGE( "Test failure when apply context is not set to a state node" );

   BOOST_REQUIRE_THROW( chain::system_call::put_object( ctx, chain::state::space::metadata(), util::converter::as< std::string >( uint256_t( 0 ) ), object_data ), chain::state_node_not_found );
   BOOST_REQUIRE_THROW( chain::system_call::get_object( ctx, chain::state::space::metadata(), util::converter::as< std::string >( 0 ) ), chain::state_node_not_found );
   BOOST_REQUIRE_THROW( chain::system_call::get_next_object( ctx, chain::state::space::metadata(), util::converter::as< std::string >( 0 ) ), chain::state_node_not_found );
   BOOST_REQUIRE_THROW( chain::system_call::get_prev_object( ctx, chain::state::space::metadata(), util::converter::as< std::string >( 0 ) ), chain::state_node_not_found );

   ctx.set_state_node( node );

   object_data = "object1"s;

   BOOST_TEST_MESSAGE( "Test putting an object" );

   BOOST_REQUIRE( chain::system_call::put_object( ctx, chain::state::space::metadata(), util::converter::as< std::string >( 1 ), object_data ) == false );
   auto obj_blob = chain::system_call::get_object( ctx, chain::state::space::metadata(), util::converter::as< std::string >( 1 ) );
   BOOST_REQUIRE( object_data == "object1" );

   BOOST_TEST_MESSAGE( "Testing getting a non-existent object" );

   obj_blob = chain::system_call::get_object( ctx, chain::state::space::metadata(), util::converter::as< std::string >( 2 ) );
   BOOST_REQUIRE( obj_blob.size() == 0 );

   BOOST_TEST_MESSAGE( "Test iteration" );

   object_data = "object2"s;
   chain::system_call::put_object( ctx, chain::state::space::metadata(), util::converter::as< std::string >( 2 ), object_data );
   object_data = "object3"s;
   chain::system_call::put_object( ctx, chain::state::space::metadata(), util::converter::as< std::string >( 3 ), object_data );

   obj_blob = chain::system_call::get_next_object( ctx, chain::state::space::metadata(), util::converter::as< std::string >( 2 ), 8 );
   BOOST_REQUIRE( obj_blob == "object3" );

   obj_blob = chain::system_call::get_prev_object( ctx, chain::state::space::metadata(), util::converter::as< std::string >( 2 ), 8 );
   BOOST_REQUIRE( obj_blob == "object1" );

   BOOST_TEST_MESSAGE( "Test iterator overrun" );

   obj_blob = chain::system_call::get_next_object( ctx, chain::state::space::metadata(), util::converter::as< std::string >( 3 ) );
   BOOST_REQUIRE( obj_blob.size() == 0 );
   obj_blob = chain::system_call::get_next_object( ctx, chain::state::space::metadata(), util::converter::as< std::string >( 4 ) );
   BOOST_REQUIRE( obj_blob.size() == 0 );
   obj_blob = chain::system_call::get_prev_object( ctx, chain::state::space::metadata(), util::converter::as< std::string >( 1 ) );
   BOOST_REQUIRE( obj_blob.size() == 0 );
   obj_blob = chain::system_call::get_prev_object( ctx, chain::state::space::metadata(), util::converter::as< std::string >( 0 ) );
   BOOST_REQUIRE( obj_blob.size() == 0 );

   object_data = "space1.object1"s;
   chain::system_call::put_object( ctx, chain::state::space::contract_bytecode(), util::converter::as< std::string >( 1 ), object_data );
   obj_blob = chain::system_call::get_next_object( ctx, chain::state::space::metadata(), util::converter::as< std::string >( 3 ) );
   BOOST_REQUIRE( obj_blob.size() == 0 );
   obj_blob = chain::system_call::get_next_object( ctx, chain::state::space::contract_bytecode(), util::converter::as< std::string >( 1 ) );
   BOOST_REQUIRE( obj_blob.size() == 0 );
   obj_blob = chain::system_call::get_prev_object( ctx, chain::state::space::contract_bytecode(), util::converter::as< std::string >( 1 ) );
   BOOST_REQUIRE( obj_blob.size() == 0 );

   BOOST_TEST_MESSAGE( "Test object modification" );
   object_data = "object1.1"s;
   BOOST_REQUIRE( chain::system_call::put_object( ctx, chain::state::space::metadata(), util::converter::as< std::string >( 1 ), object_data ) == true );
   obj_blob = chain::system_call::get_object( ctx, chain::state::space::metadata(), util::converter::as< std::string >( 1 ), 10 );
   BOOST_REQUIRE( obj_blob == "object1.1" );

   BOOST_TEST_MESSAGE( "Test object deletion" );
   object_data.clear();
   BOOST_REQUIRE( chain::system_call::put_object( ctx, chain::state::space::metadata(), util::converter::as< std::string >( 1 ), object_data ) == true );
   obj_blob = chain::system_call::get_object( ctx, chain::state::space::metadata(), util::converter::as< std::string >( 1 ), 10 );
   BOOST_REQUIRE( obj_blob.size() == 0 );

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( contract_tests )
{ try {
   BOOST_TEST_MESSAGE( "Test uploading a contract" );

   auto contract_private_key = koinos::crypto::private_key::regenerate( koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, "contract"s ) );
   auto contract_address = contract_private_key.get_public_key().to_address_bytes();
   koinos::protocol::transaction trx;
   sign_transaction( trx, contract_private_key );
   ctx.set_transaction( trx );

   koinos::protocol::upload_contract_operation op;
   op.set_contract_id( util::converter::as< std::string >( contract_address ) );
   op.set_bytecode( std::string( (const char*)hello_wasm, hello_wasm_len ) );

   koinos::chain::system_call::apply_upload_contract_operation( ctx, op );

   auto bytecode = koinos::chain::system_call::get_object( ctx, koinos::chain::state::space::contract_bytecode(), op.contract_id() );
   auto meta = util::converter::to< koinos::chain::contract_metadata >( koinos::chain::system_call::get_object( ctx, koinos::chain::state::space::contract_metadata(), op.contract_id() ) );

   BOOST_REQUIRE( bytecode.size() == op.bytecode().size() );
   BOOST_REQUIRE( std::memcmp( bytecode.c_str(), op.bytecode().c_str(), op.bytecode().size() ) == 0 );
   BOOST_REQUIRE( meta.hash() == util::converter::as< std::string >( koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, bytecode ) ) );

   BOOST_TEST_MESSAGE( "Test executing a contract" );

   koinos::protocol::call_contract_operation op2;
   op2.set_contract_id( op.contract_id() );
   koinos::chain::system_call::apply_call_contract_operation( ctx, op2 );
   BOOST_REQUIRE_EQUAL( "Greetings from koinos vm", ctx.get_pending_console_output() );

   BOOST_TEST_MESSAGE( "Test contract return" );

   // Upload the return test contract
   op.set_bytecode( std::string( (const char*)contract_return_wasm, contract_return_wasm_len ) );
   koinos::chain::system_call::apply_upload_contract_operation( ctx, op );

   auto contract_ret = koinos::chain::system_call::call_contract(ctx, op.contract_id(), 0, "echo");

   BOOST_REQUIRE_EQUAL( contract_ret, "echo" );

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( override_tests )
{ try {
   BOOST_TEST_MESSAGE( "Test set system call operation" );

   auto seed = "non-genesis key"s;
   auto random_private_key = koinos::crypto::private_key::regenerate( koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, seed ) );

   koinos::protocol::transaction tx;
   sign_transaction( tx, random_private_key );
   ctx.set_transaction( tx );

   // Upload a test contract to use as override
   auto contract_address = random_private_key.get_public_key().to_address_bytes();

   koinos::protocol::upload_contract_operation contract_op;
   contract_op.set_contract_id( util::converter::as< std::string >( contract_address ) );
   contract_op.set_bytecode( std::string( (const char*)hello_wasm, hello_wasm_len ) );

   koinos::chain::system_call::apply_upload_contract_operation( ctx, contract_op );

   koinos::protocol::call_contract_operation call_op;
   call_op.set_contract_id( contract_op.contract_id() );
   koinos::chain::system_call::apply_call_contract_operation( ctx, call_op );
   auto original_message = host._ctx.get_pending_console_output();
   BOOST_REQUIRE_EQUAL( "Greetings from koinos vm", original_message );

   // Override prints with a contract that prepends a message before printing
   auto random_private_key2 = koinos::crypto::private_key::regenerate( koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, "key2"s ) );
   auto contract_address2 = random_private_key2.get_public_key().to_address_bytes();

   koinos::protocol::upload_contract_operation contract_op2;
   contract_op2.set_contract_id( util::converter::as< std::string >( contract_address2 ) );
   contract_op2.set_bytecode( std::string( (const char*)syscall_override_wasm, syscall_override_wasm_len ) );

   sign_transaction( tx, random_private_key2 );
   ctx.set_transaction( tx );

   koinos::chain::system_call::apply_upload_contract_operation( ctx, contract_op2 );

   // Set the system call
   koinos::protocol::set_system_call_operation set_op;
   set_op.set_call_id( std::underlying_type_t< protocol::system_call_id >( protocol::system_call_id::prints ) );
   set_op.mutable_target()->mutable_system_call_bundle()->set_contract_id( contract_op2.contract_id() );
   set_op.mutable_target()->mutable_system_call_bundle()->set_entry_point( 0 );

   BOOST_TEST_MESSAGE( "Test failure to override system call without genesis key" );

   BOOST_REQUIRE_THROW( koinos::chain::system_call::apply_set_system_call_operation( ctx, set_op ), koinos::chain::insufficient_privileges );

   BOOST_TEST_MESSAGE( "Test failure to set system contract without genesis key" );

   koinos::protocol::set_system_contract_operation system_contract_op;
   system_contract_op.set_contract_id( contract_op2.contract_id() );
   system_contract_op.set_system_contract( true );

   BOOST_REQUIRE_THROW( koinos::chain::system_call::apply_set_system_contract_operation( ctx, system_contract_op ), koinos::chain::insufficient_privileges );

   BOOST_TEST_MESSAGE( "Test failure to override system call without system contract" );

   // Overriding system calls requires the genesis key
   sign_transaction( tx, _signing_private_key );
   ctx.set_transaction( tx );

   BOOST_REQUIRE_THROW( koinos::chain::system_call::apply_set_system_call_operation( ctx, set_op ), koinos::chain::invalid_contract );

   BOOST_TEST_MESSAGE( "Test success overriding a system call with the genesis key" );

   koinos::chain::system_call::apply_set_system_contract_operation( ctx, system_contract_op );
   koinos::chain::system_call::apply_set_system_call_operation( ctx, set_op );

   // Fetch the created call bundle from the database and check it
   // The call bundle should be read from the parent context
   auto call_target = koinos::util::converter::to< koinos::protocol::system_call_target >( koinos::chain::system_call::get_object( ctx, koinos::chain::state::space::system_call_dispatch(), util::converter::as< std::string >( set_op.call_id() ) ) );
   BOOST_REQUIRE( !call_target.has_system_call_bundle() );
   ctx.set_state_node( ctx.get_state_node()->create_anonymous_node() );
   call_target = koinos::util::converter::to< koinos::protocol::system_call_target >( koinos::chain::system_call::get_object( ctx, koinos::chain::state::space::system_call_dispatch(), util::converter::as< std::string >( set_op.call_id() ) ) );
   BOOST_REQUIRE( call_target.has_system_call_bundle() );
   BOOST_REQUIRE( call_target.system_call_bundle().contract_id() == set_op.target().system_call_bundle().contract_id() );
   BOOST_REQUIRE( call_target.system_call_bundle().entry_point() == set_op.target().system_call_bundle().entry_point() );

   // Ensure exception thrown on invalid contract
   auto false_id = koinos::crypto::hash( koinos::crypto::multicodec::ripemd_160, 1234 );
   set_op.mutable_target()->mutable_system_call_bundle()->set_contract_id( util::converter::as< std::string >( false_id ) );
   BOOST_REQUIRE_THROW( koinos::chain::system_call::apply_set_system_call_operation( ctx, set_op ), koinos::chain::invalid_contract );

   // Test invoking the overridden system call
   koinos::chain::system_call::apply_call_contract_operation( ctx, call_op );
   BOOST_REQUIRE_EQUAL( "test: " + original_message, ctx.get_pending_console_output() );

   koinos::chain::system_call::prints( host._ctx, "Hello World" );
   BOOST_REQUIRE_EQUAL( "test: Hello World", host._ctx.get_pending_console_output() );

   ctx.clear_transaction();

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( thunk_test )
{ try {
   BOOST_TEST_MESSAGE( "thunk test" );

   chain::prints_arguments args;

   std::string arg;
   std::string ret;

   args.set_message( "Hello World" );
   args.SerializeToString( &arg );

   host.invoke_thunk(
      protocol::system_call_id::prints,
      ret.data(),
      ret.size(),
      arg.data(),
      arg.size()
   );

   BOOST_CHECK_EQUAL( ret.size(), 0 );
   BOOST_REQUIRE_EQUAL( "Hello World", ctx.get_pending_console_output() );

   ctx.push_frame( chain::stack_frame{ .contract_id = "user_contract", .system = false, .call_privilege = chain::user_mode } );
   BOOST_REQUIRE_THROW( host.invoke_thunk( protocol::system_call_id::prints, ret.data(), ret.size(), arg.data(), arg.size() ), chain::insufficient_privileges );
} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( system_call_test )
{ try {
   BOOST_TEST_MESSAGE( "system call test" );

   chain::prints_arguments args;

   std::string arg;
   std::string ret;

   args.set_message( "Hello World" );
   args.SerializeToString( &arg );

   host.invoke_system_call(
      protocol::system_call_id::prints,
      ret.data(),
      ret.size(),
      arg.data(),
      arg.size()
   );

   BOOST_CHECK_EQUAL( ret.size(), 0 );
   BOOST_REQUIRE_EQUAL( "Hello World", ctx.get_pending_console_output() );
} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( get_head_info_thunk_test )
{ try {
   BOOST_TEST_MESSAGE( "get_head_info thunk test" );

   BOOST_CHECK_EQUAL( chain::system_call::get_head_info( ctx ).head_topology().height(), 1 );

   koinos::protocol::block block;
   block.mutable_header()->set_timestamp( 1000 );
   ctx.set_block( block );

   BOOST_REQUIRE( chain::system_call::get_head_info( ctx ).head_block_time() == block.header().timestamp() );

   ctx.clear_block();

   chain::system_call::put_object( ctx, chain::state::space::metadata(), chain::state::key::head_block_time, util::converter::as< std::string >( block.header().timestamp() ) );

   BOOST_REQUIRE( chain::system_call::get_head_info( ctx ).head_block_time() == block.header().timestamp() );

   // Test exception when null state pointer is passed
   ctx.set_state_node( std::shared_ptr< chain::abstract_state_node >() );
   BOOST_REQUIRE_THROW( chain::system_call::get_head_info( ctx ), koinos::chain::database_exception );

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( hash_thunk_test )
{ try {
   BOOST_TEST_MESSAGE( "hash thunk test" );

   std::string test_string = "hash::string";

   auto thunk_hash  = util::converter::to< crypto::multihash >( chain::system_call::hash( ctx, static_cast< uint64_t >( crypto::multicodec::sha2_256 ), test_string ) );
   auto native_hash = crypto::hash( crypto::multicodec::sha2_256, test_string );

   BOOST_CHECK_EQUAL( thunk_hash, native_hash );

   koinos::block_topology block_topology;
   block_topology.set_height( 100 );
   block_topology.set_id( util::converter::as< std::string >( crypto::hash( crypto::multicodec::sha2_256, "random::id"s ) ) );
   block_topology.set_previous( util::converter::as< std::string >( crypto::hash( crypto::multicodec::sha2_256, "random::previous"s ) ) );

   std::string blob;
   block_topology.SerializeToString( &blob );
   thunk_hash = util::converter::to< crypto::multihash >( chain::system_call::hash( ctx, static_cast< uint64_t >( crypto::multicodec::sha2_256 ), blob ) );
   native_hash = crypto::hash( crypto::multicodec::sha2_256, block_topology );

   BOOST_CHECK_EQUAL( thunk_hash, native_hash );

   BOOST_REQUIRE_THROW( chain::system_call::hash( ctx, 0xDEADBEEF /* unknown code */, blob ), koinos::chain::unknown_hash_code );

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( privileged_calls )
{
   ctx.set_privilege( chain::privilege::user_mode );
   BOOST_REQUIRE_THROW( chain::system_call::apply_block( ctx, protocol::block{} ), koinos::chain::insufficient_privileges );
   BOOST_REQUIRE_THROW( chain::system_call::apply_transaction( ctx, protocol::transaction() ), koinos::chain::insufficient_privileges );
   BOOST_REQUIRE_THROW( chain::system_call::apply_upload_contract_operation( ctx, protocol::upload_contract_operation{} ), koinos::chain::insufficient_privileges );
   BOOST_REQUIRE_THROW( chain::system_call::apply_call_contract_operation( ctx, protocol::call_contract_operation{} ), koinos::chain::insufficient_privileges );
   BOOST_REQUIRE_THROW( chain::system_call::apply_set_system_call_operation( ctx, protocol::set_system_call_operation{} ), koinos::chain::insufficient_privileges );
}

BOOST_AUTO_TEST_CASE( last_irreversible_block_test )
{ try {

   BOOST_TEST_MESSAGE( "last irreversible block test" );

   for( uint64_t i = 0; i < chain::default_irreversible_threshold; i++ )
   {
      auto lib = chain::system_call::get_last_irreversible_block( ctx );
      BOOST_REQUIRE_EQUAL( lib, 0 );

      db.finalize_node( ctx.get_state_node()->id() );
      ctx.set_state_node( db.create_writable_node( ctx.get_state_node()->id(), crypto::hash( crypto::multicodec::sha2_256, i ) ) );
   }

   auto lib = chain::system_call::get_last_irreversible_block( ctx );
   BOOST_REQUIRE_EQUAL( lib, 1 );

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( stack_tests )
{ try {
   BOOST_TEST_MESSAGE( "apply context stack tests" );
   ctx.pop_frame();

   BOOST_REQUIRE_THROW( ctx.pop_frame(), chain::stack_exception );

   auto call1 = util::converter::as< std::string >( crypto::hash( crypto::multicodec::ripemd_160, "call1"s ) );
   ctx.push_frame( chain::stack_frame{ .contract_id = call1, .system = false } );
   BOOST_CHECK_EQUAL( call1, ctx.get_caller() );
   BOOST_CHECK_EQUAL( call1, ctx.get_contract_id() );

   auto call2 = util::converter::as< std::string >( crypto::hash( crypto::multicodec::ripemd_160, "call2"s ) );
   ctx.push_frame( chain::stack_frame{ .contract_id = call2, .system = true } );

   BOOST_CHECK_EQUAL( call1, ctx.get_caller() );
   BOOST_CHECK_EQUAL( call2, ctx.get_contract_id() );

   auto last_frame = ctx.pop_frame();
   BOOST_CHECK_EQUAL( call2, last_frame.contract_id );
   BOOST_CHECK_EQUAL( call1, ctx.get_caller() );

   for ( int i = 2; i <= chain::execution_context::stack_limit; i++ )
   {
      ctx.push_frame( chain::stack_frame{ .system = true } );
      BOOST_REQUIRE_EQUAL( call1, ctx.get_caller() );
   }

   BOOST_REQUIRE_THROW( ctx.push_frame( chain::stack_frame{} ), chain::stack_overflow );

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( require_authority )
{ try {
   auto foo_key = crypto::private_key::regenerate( crypto::hash( crypto::multicodec::sha2_256, "foo"s ) );
   auto foo_account_string = foo_key.get_public_key().to_address_bytes();

   auto bar_key = crypto::private_key::regenerate( crypto::hash( crypto::multicodec::sha2_256, "bar"s ) );
   auto bar_account_string = bar_key.get_public_key().to_address_bytes();

   protocol::transaction trx;
   protocol::active_transaction_data active_data;
   trx.set_active( util::converter::as< std::string >( active_data ) );

   ctx.set_transaction( trx );
   BOOST_REQUIRE_THROW( chain::system_call::require_authority( ctx, foo_account_string ), chain::invalid_signature );

   trx.set_signature_data( util::converter::as< std::string >( foo_key.sign_compact( crypto::hash( crypto::multicodec::sha2_256, trx.active() ) ) ) );
   ctx.set_transaction( trx );

   chain::system_call::require_authority( ctx, foo_account_string );

   BOOST_REQUIRE_THROW( chain::system_call::require_authority( ctx, bar_account_string ), chain::invalid_signature );

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( transaction_nonce_test )
{ try {
   using namespace koinos;

   ctx.set_intent( chain::intent::transaction_application );

   BOOST_TEST_MESSAGE( "Test transaction nonce" );

   auto key = crypto::private_key::regenerate( crypto::hash( crypto::multicodec::sha2_256, "alpha bravo charlie delta"s ) );

   protocol::transaction transaction;
   protocol::active_transaction_data active;
   active.set_rc_limit( 10'000 );
   active.set_nonce( 0 );
   transaction.set_active( util::converter::as< std::string >( active ) );
   transaction.set_id( util::converter::as< std::string >( crypto::hash( crypto::multicodec::sha2_256, active ) ) );
   transaction.set_signature_data( util::converter::as< std::string >( key.sign_compact( util::converter::to< crypto::multihash >( transaction.id() ) ) ) );

   chain::system_call::apply_transaction( ctx, transaction );
   auto payer = chain::system_call::get_transaction_payer( ctx, transaction );
   auto next_nonce = chain::system_call::get_account_nonce( ctx, payer );
   BOOST_REQUIRE_EQUAL( next_nonce, 1 );

   BOOST_TEST_MESSAGE( "Test duplicate transaction nonce" );
   active.set_rc_limit( 20'000 );
   active.set_nonce( 0 );
   transaction.set_active( util::converter::as< std::string >( active ) );
   transaction.set_id( util::converter::as< std::string >( crypto::hash( crypto::multicodec::sha2_256, transaction.active() ) ) );
   transaction.set_signature_data( util::converter::as< std::string >( key.sign_compact( util::converter::to< crypto::multihash >( transaction.id() ) ) ) );

   BOOST_REQUIRE_THROW( chain::system_call::apply_transaction( ctx, transaction ), chain::chain_exception );

   next_nonce = chain::system_call::get_account_nonce( ctx, payer );
   BOOST_REQUIRE_EQUAL( next_nonce, 1 );

   BOOST_TEST_MESSAGE( "Test next transaction nonce" );
   active.set_nonce( 1 );
   transaction.set_active( util::converter::as< std::string >( active ) );
   transaction.set_id( util::converter::as< std::string >( crypto::hash( crypto::multicodec::sha2_256, active ) ) );
   transaction.set_signature_data( util::converter::as< std::string >( key.sign_compact( util::converter::to< crypto::multihash >( transaction.id() ) ) ) );

   chain::system_call::apply_transaction( ctx, transaction );

   next_nonce = chain::system_call::get_account_nonce( ctx, payer );
   BOOST_REQUIRE_EQUAL( next_nonce, 2 );

   BOOST_TEST_MESSAGE( "Test duplicate transaction nonce" );
   active.set_rc_limit( 30'000 );
   transaction.set_active( util::converter::as< std::string >( active ) );
   transaction.set_id( util::converter::as< std::string >( crypto::hash( crypto::multicodec::sha2_256, active ) ) );
   transaction.set_signature_data( util::converter::as< std::string >( key.sign_compact( util::converter::to< crypto::multihash >( transaction.id() ) ) ) );

   BOOST_REQUIRE_THROW( chain::system_call::apply_transaction( ctx, transaction ), chain::chain_exception );

   next_nonce = chain::system_call::get_account_nonce( ctx, payer );
   BOOST_REQUIRE_EQUAL( next_nonce, 2 );

   BOOST_TEST_MESSAGE( "Test next transaction nonce" );
   active.set_nonce( 2 );
   transaction.set_active( util::converter::as< std::string >( active ) );
   transaction.set_id( util::converter::as< std::string >( crypto::hash( crypto::multicodec::sha2_256, active ) ) );
   transaction.set_signature_data( util::converter::as< std::string >( key.sign_compact( util::converter::to< crypto::multihash >( transaction.id() ) ) ) );

   chain::system_call::apply_transaction( ctx, transaction );

   next_nonce = chain::system_call::get_account_nonce( ctx, payer );
   BOOST_REQUIRE_EQUAL( next_nonce, 3 );
} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( get_contract_id_test )
{ try {
   auto contract_id = util::converter::as< std::string >( crypto::hash( crypto::multicodec::ripemd_160, "get_contract_id_test"s ) );

   ctx.push_frame( chain::stack_frame {
      .contract_id = contract_id,
      .call_privilege = chain::privilege::kernel_mode
   } );

   auto id = chain::system_call::get_contract_id( ctx );

   BOOST_REQUIRE_EQUAL( contract_id, id );
   //BOOST_REQUIRE( contract_id.size() == id.size() );
   //auto id_bytes = util::converter::as< std::vector< std::byte > >( id );
   //BOOST_REQUIRE( std::equal( contract_id.begin(), contract_id.end(), id_bytes.begin() ) );
} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( token_tests )
{ try {
   using namespace koinos;

   auto contract_private_key = crypto::private_key::regenerate( crypto::hash( koinos::crypto::multicodec::sha2_256, "token_contract"s ) );
   auto contract_address = contract_private_key.get_public_key().to_address_bytes();
   protocol::transaction trx;
   sign_transaction( trx, contract_private_key );
   ctx.set_transaction( trx );

   koinos::protocol::upload_contract_operation op;
   op.set_contract_id( util::converter::as< std::string >( contract_address ) );
   op.set_bytecode( std::string( (const char*)koin_wasm, koin_wasm_len ) );

   koinos::chain::system_call::apply_upload_contract_operation( ctx, op );

   BOOST_TEST_MESSAGE( "Test executing a contract" );

   ctx.push_frame( chain::stack_frame{ .contract_id = "token_tests"s, .system = false, .call_privilege = chain::user_mode } );

   auto response = koinos::chain::system_call::call_contract( ctx, op.contract_id(), 0x76ea4297, "" );
   auto name = util::converter::to< koinos::contracts::token::name_result >( response );
   LOG(info) << name.value();

   response = koinos::chain::system_call::call_contract( ctx, op.contract_id(), 0x7e794b24, "" );
   auto symbol = util::converter::to< koinos::contracts::token::symbol_result >( response );
   LOG(info) << symbol.value();

   response = koinos::chain::system_call::call_contract( ctx, op.contract_id(), 0x59dc15ce, "" );
   auto decimals = util::converter::to< koinos::contracts::token::decimals_result >( response );
   LOG(info) << decimals.value();

   response = koinos::chain::system_call::call_contract( ctx, op.contract_id(), 0xcf2e8212, "" );
   auto supply = util::converter::to< koinos::contracts::token::total_supply_result >( response );
   LOG(info) << "KOIN supply: " << supply.value();

   auto alice_private_key = crypto::private_key::regenerate( koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, "alice"s ) );
   auto alice_address = alice_private_key.get_public_key().to_address_bytes();

   auto bob_private_key = crypto::private_key::regenerate( koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, "bob"s ) );
   auto bob_address = bob_private_key.get_public_key().to_address_bytes();

   koinos::contracts::token::balance_of_arguments balance_of_args;
   balance_of_args.set_owner( util::converter::as< std::string >( alice_address ) );
   response = koinos::chain::system_call::call_contract( ctx, op.contract_id(), 0x15619248, util::converter::as< std::string >( balance_of_args ) );
   auto balance = util::converter::to< koinos::contracts::token::balance_of_result >( response );
   LOG(info) << "'alice' balance: " << balance.value();

   balance_of_args.set_owner( util::converter::as< std::string >( bob_address ) );
   response = koinos::chain::system_call::call_contract( ctx, op.contract_id(), 0x15619248, util::converter::as< std::string >( balance_of_args ) );
   balance = util::converter::to< koinos::contracts::token::balance_of_result >( response );
   LOG(info) << "'bob' balance: " << balance.value();

   ctx.get_pending_console_output();

   auto session = ctx.make_session( 1'000'000 );

   LOG(info) << "Mint to 'alice'";
   koinos::contracts::token::mint_arguments mint_args;
   mint_args.set_to( util::converter::as< std::string >( alice_address ) );
   mint_args.set_value( 100 );

   response = koinos::chain::system_call::call_contract( ctx, op.contract_id(), 0xc2f82bdc, util::converter::as< std::string >( mint_args ) );
   auto success = util::converter::to< koinos::contracts::token::mint_result >( response );
   BOOST_REQUIRE( !success.value() );
   BOOST_CHECK_EQUAL( session->events().size(), 0 );

   session = ctx.make_session( 1'000'000 );

   ctx.set_privilege( chain::privilege::kernel_mode );
   response = koinos::chain::system_call::call_contract( ctx, op.contract_id(), 0xc2f82bdc, util::converter::as< std::string >( mint_args ) );
   success = util::converter::to< koinos::contracts::token::mint_result >( response );
   BOOST_REQUIRE( success.value() );

   BOOST_REQUIRE_EQUAL( session->events().size(), 1 );
   {
      const auto& event = session->events()[0];
      BOOST_CHECK_EQUAL( event.source(), op.contract_id() );
      BOOST_CHECK_EQUAL( event.name(), "koin.mint" );
      BOOST_CHECK_EQUAL( event.impacted().size(), 1 );
      BOOST_CHECK_EQUAL( event.impacted()[0], mint_args.to() );

      auto mint_event = util::converter::to< koinos::contracts::token::mint_event >( event.data() );
      BOOST_CHECK_EQUAL( mint_event.to(), mint_args.to() );
      BOOST_CHECK_EQUAL( mint_event.value(), mint_args.value() );
   }

   ctx.set_privilege( chain::privilege::user_mode );
   balance_of_args.set_owner( util::converter::as< std::string >( alice_address ) );
   response = koinos::chain::system_call::call_contract( ctx, op.contract_id(), 0x15619248, util::converter::as< std::string >( balance_of_args ) );
   balance = util::converter::to< koinos::contracts::token::balance_of_result >( response );

   LOG(info) << "'alice' balance: " << balance.value();

   balance_of_args.set_owner( util::converter::as< std::string >( bob_address ) );
   response = koinos::chain::system_call::call_contract( ctx, op.contract_id(), 0x15619248, util::converter::as< std::string >( balance_of_args ) );
   balance = util::converter::to< koinos::contracts::token::balance_of_result >( response );

   LOG(info) << "'bob' balance: " << balance.value();

   response = koinos::chain::system_call::call_contract( ctx, op.contract_id(), 0xcf2e8212, "" );
   supply = util::converter::to< koinos::contracts::token::total_supply_result >( response );
   LOG(info) << "KOIN supply: " << supply.value();

   LOG(info) << "Transfer from 'alice' to 'bob'";
   koinos::contracts::token::transfer_arguments transfer_args;
   transfer_args.set_from( util::converter::as< std::string >( alice_address ) );
   transfer_args.set_to( util::converter::as< std::string >( bob_address ) );
   transfer_args.set_value( 25 );

   ctx.set_transaction( trx );
   try
   {
      koinos::chain::system_call::call_contract( ctx, op.contract_id(), 0x62efa292, util::converter::as< std::string >( transfer_args ) );
      BOOST_FAIL( "Expected invalid signature exception" );
   }
   catch ( const koinos::chain::invalid_signature& ) {}

   auto signature = bob_private_key.sign_compact( koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, trx.active() ) );
   trx.set_signature_data( util::converter::as< std::string >( signature ) );
   ctx.set_transaction( trx );

   try
   {
      koinos::chain::system_call::call_contract( ctx, op.contract_id(), 0x62efa292, util::converter::as< std::string >( transfer_args ) );
      BOOST_FAIL( "Expected invalid signature exception" );
   }
   catch ( const koinos::chain::invalid_signature& ) {}

   signature = alice_private_key.sign_compact( koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, trx.active() ) );
   trx.set_signature_data( util::converter::as< std::string >( signature ) );
   ctx.set_transaction( trx );

   response = koinos::chain::system_call::call_contract( ctx, op.contract_id(), 0x62efa292, util::converter::as< std::string >( transfer_args ) );
   auto xfer_result = util::converter::to< koinos::contracts::token::transfer_result >( response );
   BOOST_REQUIRE( xfer_result.value() );

   balance_of_args.set_owner( util::converter::as< std::string >( alice_address ) );
   response = koinos::chain::system_call::call_contract( ctx, op.contract_id(), 0x15619248, util::converter::as< std::string >( balance_of_args ) );
   balance = util::converter::to< koinos::contracts::token::balance_of_result >( response );

   LOG(info) << "'alice' balance: " << balance.value();

   balance_of_args.set_owner( util::converter::as< std::string >( bob_address ) );
   response = koinos::chain::system_call::call_contract( ctx, op.contract_id(), 0x15619248, util::converter::as< std::string >( balance_of_args ) );
   balance = util::converter::to< koinos::contracts::token::balance_of_result >( response );

   LOG(info) << "'bob' balance: " << balance.value();

   response = koinos::chain::system_call::call_contract( ctx, op.contract_id(), 0xcf2e8212, "" );
   supply = util::converter::to< koinos::contracts::token::total_supply_result >( response );
   LOG(info) << "KOIN supply: " << supply.value();
}
catch( const koinos::vm_manager::vm_exception& e )
{
   LOG(info) << e.what();
   BOOST_FAIL("VM Exception");
}
KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( tick_limit )
{ try {
   using namespace koinos;
   BOOST_TEST_MESSAGE( "Upload forever contract" );

   auto contract_private_key = koinos::crypto::private_key::regenerate( koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, "contract"s ) );
   protocol::transaction trx;
   sign_transaction( trx, contract_private_key );
   ctx.set_transaction( trx );

   protocol::upload_contract_operation op;
   op.set_contract_id( util::converter::as< std::string >( contract_private_key.get_public_key().to_address_bytes() ) );
   op.set_bytecode( util::converter::as< std::string >( get_forever_wasm() ) );

   chain::system_call::apply_upload_contract_operation( ctx, op );

   auto bytecode = koinos::chain::system_call::get_object( ctx, koinos::chain::state::space::contract_bytecode(), op.contract_id() );
   auto meta = util::converter::to< koinos::chain::contract_metadata >( koinos::chain::system_call::get_object( ctx, koinos::chain::state::space::contract_metadata(), op.contract_id() ) );

   BOOST_REQUIRE( bytecode.size() == op.bytecode().size() );
   BOOST_REQUIRE( std::memcmp( bytecode.c_str(), op.bytecode().c_str(), op.bytecode().size() ) == 0 );
   BOOST_REQUIRE( meta.hash() == util::converter::as< std::string >( koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, bytecode ) ) );

   koinos::protocol::call_contract_operation op2;
   op2.set_contract_id( op.contract_id() );

   BOOST_TEST_MESSAGE( "Execute forever contract inside a session" );

   auto compute_bandwidth_remaining = ctx.resource_meter().compute_bandwidth_remaining();

   auto session = ctx.make_session( 1'000'000 );
   BOOST_REQUIRE_THROW( chain::system_call::apply_call_contract_operation( ctx, op2 ), chain::insufficient_rc );
   BOOST_REQUIRE_EQUAL( session->used_rc(), 1'000'000 );
   BOOST_REQUIRE_EQUAL( session->remaining_rc(), 0 );
   session.reset();

   BOOST_REQUIRE_EQUAL( ctx.resource_meter().compute_bandwidth_remaining(), compute_bandwidth_remaining - 1'000'000 );

   // We lower the compute bandwidth block-wide so the test doesn't take long
   auto rl = chain::system_call::get_resource_limits( ctx );
   rl.set_compute_bandwidth_limit( 1'000'000 );
   ctx.resource_meter().set_resource_limit_data( rl );

   BOOST_TEST_MESSAGE( "Execute forever contract outside a session" );
   BOOST_REQUIRE_THROW( chain::system_call::apply_call_contract_operation( ctx, op2 ), chain::compute_bandwidth_limit_exceeded );

   BOOST_REQUIRE_EQUAL( ctx.resource_meter().compute_bandwidth_remaining(), 0 );

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_SUITE_END()
