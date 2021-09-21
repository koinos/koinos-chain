#include <algorithm>
#include <filesystem>
#include <type_traits>
#include <vector>

#include <boost/test/unit_test.hpp>
#include <boost/filesystem.hpp>

#include <koinos/log.hpp>

#include <koinos/chain/controller.hpp>
#include <koinos/chain/apply_context.hpp>
#include <koinos/chain/constants.hpp>
#include <koinos/chain/exceptions.hpp>
#include <koinos/chain/host_api.hpp>
#include <koinos/chain/thunk_dispatcher.hpp>
#include <koinos/chain/system_calls.hpp>

#include <koinos/crypto/elliptic.hpp>

#include <koinos/vm_manager/exceptions.hpp>

#include <mira/database_configuration.hpp>

//#include <koinos/tests/koin.hpp>
//#include <koinos/tests/wasm/contract_return.hpp>
//#include <koinos/tests/wasm/forever.hpp>
//#include <koinos/tests/wasm/hello.hpp>
//#include <koinos/tests/wasm/koin.hpp>
//#include <koinos/tests/wasm/syscall_override.hpp>

using namespace koinos;
using namespace std::string_literals;

struct thunk_fixture
{
   thunk_fixture() :
      vm_backend( koinos::vm_manager::get_vm_backend() ),
      ctx( vm_backend ),
      host( ctx )
   {
      KOINOS_ASSERT( vm_backend, koinos::chain::unknown_backend_exception, "Couldn't get VM backend" );

      initialize_logging( "koinos_test", {}, "info" );

      temp = std::filesystem::temp_directory_path() / boost::filesystem::unique_path().string();
      std::filesystem::create_directory( temp );
      std::any cfg = mira::utilities::default_database_configuration();

      auto seed = "test seed"s;
      _signing_private_key = crypto::private_key::regenerate( crypto::hash( crypto::multicodec::sha2_256, seed ) );

      chain::genesis_data genesis_data;
      auto chain_id = crypto::hash( crypto::multicodec::sha2_256, _signing_private_key.get_public_key().to_address() );
      genesis_data[ { converter::as< state_db::object_space >( chain::database::space::kernel ), converter::as< state_db::object_key >( chain::database::key::chain_id ) } ] = converter::as< std::vector< std::byte > >( chain_id );

      db.open( temp, cfg, [&]( state_db::state_node_ptr root )
      {
         for ( const auto& entry : genesis_data )
         {
            state_db::put_object_args put_args;
            put_args.space = entry.first.first;
            put_args.key = entry.first.second;
            put_args.buf = entry.second.data();
            put_args.object_size = entry.second.size();

            state_db::put_object_result put_res;
            root->put_object( put_res, put_args );

            KOINOS_ASSERT(
               !put_res.object_existed,
               chain::unexpected_state,
               "encountered unexpected object in initial state"
            );
         }
      } );

      ctx.set_state_node( db.create_writable_node( db.get_head()->id(), crypto::hash( crypto::multicodec::sha2_256, 1 ) ) );
      ctx.push_frame( chain::stack_frame {
         .call = crypto::hash( crypto::multicodec::ripemd_160, "thunk_tests"s ).digest(),
         .call_privilege = chain::privilege::kernel_mode
      } );
      ctx.push_frame( chain::stack_frame {
         .call = crypto::hash( crypto::multicodec::ripemd_160, "thunk_tests"s ).digest(),
         .call_privilege = chain::privilege::kernel_mode
      } );

      vm_backend->initialize();
   }

   ~thunk_fixture()
   {
      db.close();
      std::filesystem::remove_all( temp );
   }

   void sign_transaction( protocol::transaction& transaction, crypto::private_key& transaction_signing_key )
   {
      // Signature is on the hash of the active data
      auto id_mh = crypto::hash( crypto::multicodec::sha2_256, transaction.active() );
      transaction.set_id( converter::as< std::string >( id_mh ) );
      transaction.set_signature_data( converter::as< std::string >( transaction_signing_key.sign_compact( id_mh ) ) );
   }

//   std::vector< uint8_t > get_hello_wasm()
//   {
//      return std::vector< uint8_t >( hello_wasm, hello_wasm + hello_wasm_len );
//   }
//
//   std::vector< uint8_t > get_contract_return_wasm()
//   {
//      return std::vector< uint8_t >( contract_return_wasm, contract_return_wasm + contract_return_wasm_len );
//   }
//
//   std::vector< uint8_t > get_syscall_override_wasm()
//   {
//      return std::vector< uint8_t >( syscall_override_wasm, syscall_override_wasm + syscall_override_wasm_len );
//   }
//
//   std::vector< uint8_t > get_koin_wasm()
//   {
//      return std::vector< uint8_t >( koin_wasm, koin_wasm + koin_wasm_len );
//   }
//
//   std::vector< uint8_t > get_forever_wasm()
//   {
//      return std::vector< uint8_t >( forever_wasm, forever_wasm + forever_wasm_len );
//   }

   std::filesystem::path temp;
   koinos::state_db::database db;
   std::shared_ptr< koinos::vm_manager::vm_backend > vm_backend;
   koinos::chain::apply_context ctx;
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
      chain::system_call::db_put_object(
         ctx,
         chain::database::space::kernel,
         chain::database::key::chain_id,
         object_data
      ).value() == true
   );

   auto node = std::dynamic_pointer_cast< koinos::state_db::state_node >( ctx.get_state_node() );
   ctx.clear_state_node();

   BOOST_TEST_MESSAGE( "Test failure when apply context is not set to a state node" );

   BOOST_REQUIRE_THROW( chain::system_call::db_put_object( ctx, chain::database::space::kernel, converter::as< std::string >( uint256_t( 0 ) ), object_data ), chain::state_node_not_found );
   BOOST_REQUIRE_THROW( chain::system_call::db_get_object( ctx, chain::database::space::kernel, converter::as< std::string >( 0 ) ), chain::state_node_not_found );
   BOOST_REQUIRE_THROW( chain::system_call::db_get_next_object( ctx, chain::database::space::kernel, converter::as< std::string >( 0 ) ), chain::state_node_not_found );
   BOOST_REQUIRE_THROW( chain::system_call::db_get_prev_object( ctx, chain::database::space::kernel, converter::as< std::string >( 0 ) ), chain::state_node_not_found );

   ctx.set_state_node( node );

   object_data = "object1"s;

   BOOST_TEST_MESSAGE( "Test putting an object" );

   BOOST_REQUIRE( chain::system_call::db_put_object( ctx, chain::database::space::kernel, converter::as< std::string >( 1 ), object_data ).value() == false );
   auto obj_blob = chain::system_call::db_get_object( ctx, chain::database::space::kernel, converter::as< std::string >( 1 ) ).value();
   BOOST_REQUIRE( object_data == "object1" );

   BOOST_TEST_MESSAGE( "Testing getting a non-existent object" );

   obj_blob = chain::system_call::db_get_object( ctx, chain::database::space::kernel, converter::as< std::string >( 2 ) ).value();
   BOOST_REQUIRE( obj_blob.size() == 0 );

   BOOST_TEST_MESSAGE( "Test iteration" );

   object_data = "object2"s;
   chain::system_call::db_put_object( ctx, chain::database::space::kernel, converter::as< std::string >( 2 ), object_data );
   object_data = "object3"s;
   chain::system_call::db_put_object( ctx, chain::database::space::kernel, converter::as< std::string >( 3 ), object_data );

   obj_blob = chain::system_call::db_get_next_object( ctx, chain::database::space::kernel, converter::as< std::string >( 2 ), 8 ).value();
   BOOST_REQUIRE( obj_blob == "object3" );

   obj_blob = chain::system_call::db_get_prev_object( ctx, chain::database::space::kernel, converter::as< std::string >( 2 ), 8 ).value();
   BOOST_REQUIRE( obj_blob == "object1" );

   BOOST_TEST_MESSAGE( "Test iterator overrun" );

   obj_blob = chain::system_call::db_get_next_object( ctx, chain::database::space::kernel, converter::as< std::string >( 3 ) ).value();
   BOOST_REQUIRE( obj_blob.size() == 0 );
   obj_blob = chain::system_call::db_get_next_object( ctx, chain::database::space::kernel, converter::as< std::string >( 4 ) ).value();
   BOOST_REQUIRE( obj_blob.size() == 0 );
   obj_blob = chain::system_call::db_get_prev_object( ctx, chain::database::space::kernel, converter::as< std::string >( 1 ) ).value();
   BOOST_REQUIRE( obj_blob.size() == 0 );
   obj_blob = chain::system_call::db_get_prev_object( ctx, chain::database::space::kernel, converter::as< std::string >( 0 ) ).value();
   BOOST_REQUIRE( obj_blob.size() == 0 );

   object_data = "space1.object1"s;
   chain::system_call::db_put_object( ctx, chain::database::space::contract, converter::as< std::string >( 1 ), object_data );
   obj_blob = chain::system_call::db_get_next_object( ctx, chain::database::space::kernel, converter::as< std::string >( 3 ) ).value();
   BOOST_REQUIRE( obj_blob.size() == 0 );
   obj_blob = chain::system_call::db_get_next_object( ctx, chain::database::space::contract, converter::as< std::string >( 1 ) ).value();
   BOOST_REQUIRE( obj_blob.size() == 0 );
   obj_blob = chain::system_call::db_get_prev_object( ctx, chain::database::space::contract, converter::as< std::string >( 1 ) ).value();
   BOOST_REQUIRE( obj_blob.size() == 0 );

   BOOST_TEST_MESSAGE( "Test object modification" );
   object_data = "object1.1"s;
   BOOST_REQUIRE( chain::system_call::db_put_object( ctx, chain::database::space::kernel, converter::as< std::string >( 1 ), object_data ).value() == true );
   obj_blob = chain::system_call::db_get_object( ctx, chain::database::space::kernel, converter::as< std::string >( 1 ), 10 ).value();
   BOOST_REQUIRE( obj_blob == "object1.1" );

   BOOST_TEST_MESSAGE( "Test object deletion" );
   object_data.clear();
   BOOST_REQUIRE( chain::system_call::db_put_object( ctx, chain::database::space::kernel, converter::as< std::string >( 1 ), object_data ).value() == true );
   obj_blob = chain::system_call::db_get_object( ctx, chain::database::space::kernel, converter::as< std::string >( 1 ), 10 ).value();
   BOOST_REQUIRE( obj_blob.size() == 0 );

} KOINOS_CATCH_LOG_AND_RETHROW(info) }
#if 0
BOOST_AUTO_TEST_CAS E( contract_tests )
{ try {
   BOOST_TEST_MESSAGE( "Test uploading a contract" );

   auto contract_private_key = koinos::crypto::private_key::regenerate( koinos::crypto::hash( CRYPTO_SHA2_256_ID, "contract"s ) );
   auto contract_address = contract_private_key.get_public_key().to_address();
   koinos::protocol::transaction trx;
   sign_transaction( trx, contract_private_key );
   ctx.set_transaction( trx );

   auto contract_id = koinos::crypto::hash( CRYPTO_RIPEMD160_ID, contract_address );

   koinos::protocol::upload_contract_operation op;
   memcpy( op.contract_id.data(), contract_id.digest.data(), op.contract_id.size() );
   auto bytecode = get_hello_wasm();
   op.bytecode.insert( op.bytecode.end(), bytecode.begin(), bytecode.end() );

   system_call::apply_upload_contract_operation( ctx, op );

   koinos::uint256 contract_key = koinos::pack::from_fixed_blob< koinos::uint160_t >( op.contract_id );
   auto stored_bytecode = system_call::db_get_object( ctx, database::contract_space, contract_key, bytecode.size() );

   BOOST_REQUIRE( stored_bytecode.size() == bytecode.size() );
   BOOST_REQUIRE( std::memcmp( stored_bytecode.data(), bytecode.data(), bytecode.size() ) == 0 );

   BOOST_TEST_MESSAGE( "Test executing a contract" );

   ctx.set_meter_ticks(KOINOS_MAX_METER_TICKS);
   koinos::protocol::call_contract_operation op2;
   std::memcpy( op2.contract_id.data(), contract_id.digest.data(), op2.contract_id.size() );
   system_call::apply_execute_contract_operation( ctx, op2 );
   BOOST_REQUIRE( "Greetings from koinos vm" == ctx.get_pending_console_output() );

   LOG(info) << "hello.wasm opcode count: " << ctx.get_used_meter_ticks();

   BOOST_REQUIRE_THROW( system_call::apply_reserved_operation( ctx, koinos::protocol::reserved_operation() ), reserved_operation_exception );

   BOOST_TEST_MESSAGE( "Test contract return" );

   // Upload the return test contract
   koinos::pack::protocol::upload_contract_operation contract_op;
   auto return_bytecode = get_contract_return_wasm();
   //auto return_id = koinos::crypto::hash( CRYPTO_RIPEMD160_ID, return_bytecode );
   std::memcpy( contract_op.contract_id.data(), contract_id.digest.data(), contract_op.contract_id.size() );
   contract_op.bytecode.insert( contract_op.bytecode.end(), return_bytecode.begin(), return_bytecode.end() );
   system_call::apply_upload_contract_operation( ctx, contract_op );

   ctx.set_meter_ticks(KOINOS_MAX_METER_TICKS);
   std::string arg_str = "echo";
   koinos::variable_blob args = koinos::pack::to_variable_blob( arg_str );
   auto contract_ret = system_call::execute_contract(ctx, contract_op.contract_id, 0, args);
   auto return_str = koinos::pack::from_variable_blob< std::string >( contract_ret );
   BOOST_REQUIRE_EQUAL( arg_str, return_str );
   LOG(info) << "echo opcode count: " << ctx.get_used_meter_ticks();

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CAS E( override_tests )
{ try {
   BOOST_TEST_MESSAGE( "Test set system call operation" );

   auto seed = "non-genesis key"s;
   auto random_private_key = koinos::crypto::private_key::regenerate( koinos::crypto::hash_str( CRYPTO_SHA2_256_ID, seed.c_str(), seed.size() ) );

   koinos::protocol::transaction tx;
   sign_transaction( tx, random_private_key );
   ctx.set_transaction( tx );

   // Upload a test contract to use as override
   koinos::protocol::upload_contract_operation contract_op;
   auto bytecode = get_hello_wasm();
   auto contract_address = random_private_key.get_public_key().to_address();
   auto id = koinos::crypto::hash( CRYPTO_RIPEMD160_ID, contract_address );
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

   BOOST_TEST_MESSAGE( "Test failure to override system call without genesis key" );

   BOOST_REQUIRE_THROW( system_call::apply_set_system_call_operation( ctx, call_op ), insufficient_privileges );

   BOOST_TEST_MESSAGE( "Test success overriding a system call with the genesis key" );

   // After signing the transaction with the genesis key, we may override system calls
   sign_transaction( tx, _signing_private_key );
   ctx.set_transaction( tx );

   system_call::apply_set_system_call_operation( ctx, call_op );

   // Fetch the created call bundle from the database and check it
   auto call_target = koinos::pack::from_variable_blob< koinos::chain::system_call_target >( system_call::db_get_object( ctx, database::system_call_dispatch_space, call_op.call_id ) );
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
   ctx.set_meter_ticks(KOINOS_MAX_METER_TICKS);
   host.invoke_system_call( 11675754, vl_ret.data(), vl_ret.size(), vl_args.data(), vl_args.size() );
   BOOST_REQUIRE( "Greetings from koinos vm" == host.context.get_pending_console_output() );

   // Call stock prints and save the message
   koinos::chain::prints_args args;
   args.message = "Hello World";
   koinos::variable_blob vl_args2, vl_ret2;
   koinos::pack::to_variable_blob( vl_args2, args );
   ctx.set_meter_ticks(KOINOS_MAX_METER_TICKS);
   host.invoke_system_call(
      system_call_id_type( koinos::chain::system_call_id::prints ),
      vl_ret2.data(),
      vl_ret2.size(),
      vl_args2.data(),
      vl_args2.size() );
   auto original_message = host.context.get_pending_console_output();

   auto random_private_key2 = koinos::crypto::private_key::regenerate( koinos::crypto::hash( CRYPTO_SHA2_256_ID, "key2"s ) );
   sign_transaction( tx, random_private_key2 );
   ctx.set_transaction( tx );

   // Override prints with a contract that prepends a message before printing
   koinos::protocol::upload_contract_operation contract_op2;
   auto bytecode2 = get_syscall_override_wasm();
   auto contract_address2 = random_private_key2.get_public_key().to_address();
   auto id2 = koinos::crypto::hash( CRYPTO_RIPEMD160_ID, contract_address2 );
   std::memcpy( contract_op2.contract_id.data(), id2.digest.data(), contract_op2.contract_id.size() );
   contract_op2.bytecode.insert( contract_op2.bytecode.end(), bytecode2.begin(), bytecode2.end() );
   system_call::apply_upload_contract_operation( ctx, contract_op2 );

   sign_transaction( tx, _signing_private_key );
   ctx.set_transaction( tx );

   koinos::protocol::set_system_call_operation call_op2;
   koinos::chain::contract_call_bundle bundle2;
   bundle2.contract_id = contract_op2.contract_id;
   bundle2.entry_point = 0;
   call_op2.call_id = system_call_id_type( koinos::chain::system_call_id::prints );
   call_op2.target = bundle2;
   system_call::apply_set_system_call_operation( ctx, call_op2 );

   // Now test that the message has been modified
   ctx.set_meter_ticks(KOINOS_MAX_METER_TICKS);
   host.invoke_system_call(
      system_call_id_type( koinos::chain::system_call_id::prints ),
      vl_ret2.data(),
      vl_ret2.size(),
      vl_args2.data(),
      vl_args2.size() );
   auto new_message = host.context.get_pending_console_output();
   BOOST_REQUIRE( original_message != new_message );
   BOOST_REQUIRE_EQUAL( "test: Hello World", new_message );

   system_call::prints( host.context, original_message );
   new_message = host.context.get_pending_console_output();
   BOOST_REQUIRE( original_message != new_message );
   BOOST_REQUIRE_EQUAL( "test: Hello World", new_message );

   ctx.clear_transaction();

} KOINOS_CATCH_LOG_AND_RETHROW(info) }
#endif
BOOST_AUTO_TEST_CASE( thunk_test )
{ try {
   BOOST_TEST_MESSAGE( "thunk test" );

   chain::prints_args args;

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
} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( system_call_test )
{ try {
   BOOST_TEST_MESSAGE( "system call test" );

   chain::prints_args args;

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

   BOOST_CHECK_EQUAL( chain::system_call::get_head_info( ctx ).value().head_topology().height(), 1 );

   koinos::protocol::block block;
   block.mutable_header()->set_timestamp( 1000 );
   ctx.set_block( block );

   BOOST_REQUIRE( chain::system_call::get_head_info( ctx ).value().head_block_time() == block.header().timestamp() );

   ctx.clear_block();

   chain::system_call::db_put_object( ctx, chain::database::space::kernel, chain::database::key::head_block_time, converter::as< std::string >( block.header().timestamp() ) );

   BOOST_REQUIRE( chain::system_call::get_head_info( ctx ).value().head_block_time() == block.header().timestamp() );

   // Test exception when null state pointer is passed
   ctx.set_state_node( std::shared_ptr< chain::abstract_state_node >() );
   BOOST_REQUIRE_THROW( chain::system_call::get_head_info( ctx ), koinos::chain::database_exception );

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( hash_thunk_test )
{ try {
   BOOST_TEST_MESSAGE( "hash thunk test" );

   std::string test_string = "hash::string";

   auto thunk_hash  = converter::to< crypto::multihash >( chain::system_call::hash( ctx, static_cast< uint64_t >( crypto::multicodec::sha2_256 ), test_string ).value() );
   auto native_hash = crypto::hash( crypto::multicodec::sha2_256, test_string );

   BOOST_CHECK_EQUAL( thunk_hash, native_hash );

   koinos::block_topology block_topology;
   block_topology.set_height( 100 );
   block_topology.set_id( converter::as< std::string >( crypto::hash( crypto::multicodec::sha2_256, "random::id"s ) ) );
   block_topology.set_previous( converter::as< std::string >( crypto::hash( crypto::multicodec::sha2_256, "random::previous"s ) ) );

   std::string blob;
   block_topology.SerializeToString( &blob );
   thunk_hash = converter::to< crypto::multihash >( chain::system_call::hash( ctx, static_cast< uint64_t >( crypto::multicodec::sha2_256 ), blob ).value() );
   native_hash = crypto::hash( crypto::multicodec::sha2_256, block_topology );

   BOOST_CHECK_EQUAL( thunk_hash, native_hash );

   BOOST_REQUIRE_THROW( chain::system_call::hash( ctx, 0xDEADBEEF /* unknown code */, blob ), koinos::chain::unknown_hash_code );

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( privileged_calls )
{
   ctx.set_in_user_code( true );
   BOOST_REQUIRE_THROW( chain::system_call::apply_block( ctx, protocol::block{}, false, false, false ), koinos::chain::insufficient_privileges );
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
      auto lib = chain::system_call::get_last_irreversible_block( ctx ).value();
      BOOST_REQUIRE_EQUAL( lib, 0 );

      db.finalize_node( ctx.get_state_node()->id() );
      ctx.set_state_node( db.create_writable_node( ctx.get_state_node()->id(), crypto::hash( crypto::multicodec::sha2_256, i ) ) );
   }

   auto lib = chain::system_call::get_last_irreversible_block( ctx ).value();
   BOOST_REQUIRE_EQUAL( lib, 1 );

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( stack_tests )
{ try {
   BOOST_TEST_MESSAGE( "apply context stack tests" );
   ctx.pop_frame();
   ctx.pop_frame();

   BOOST_REQUIRE_THROW( ctx.pop_frame(), chain::stack_exception );

   auto call1_vb = crypto::hash( crypto::multicodec::ripemd_160, "call1"s ).digest();
   ctx.push_frame( chain::stack_frame{ .call = call1_vb } );
   BOOST_REQUIRE_THROW( ctx.get_caller(), chain::stack_exception );

   auto call2_vb = crypto::hash( crypto::multicodec::ripemd_160, "call2"s ).digest();
   ctx.push_frame( chain::stack_frame{ .call = call2_vb } );
   BOOST_REQUIRE( std::equal( call1_vb.begin(), call1_vb.end(), ctx.get_caller().begin() ) );

   auto last_frame = ctx.pop_frame();
   BOOST_REQUIRE( std::equal( call2_vb.begin(), call2_vb.end(), last_frame.call.begin() ) );

   for ( int i = 2; i <= APPLY_CONTEXT_STACK_LIMIT; i++ )
   {
      ctx.push_frame( chain::stack_frame{ .call = crypto::hash( crypto::multicodec::ripemd_160, "call"s + std::to_string( i ) ).digest() } );
   }

   BOOST_REQUIRE_THROW( ctx.push_frame( chain::stack_frame{} ), chain::stack_overflow );

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( require_authority )
{ try {
   auto foo_key = crypto::private_key::regenerate( crypto::hash( crypto::multicodec::sha2_256, "foo"s ) );
   auto foo_account_string = foo_key.get_public_key().to_address();

   auto bar_key = crypto::private_key::regenerate( crypto::hash( crypto::multicodec::sha2_256, "bar"s ) );
   auto bar_account_string = bar_key.get_public_key().to_address();

   protocol::transaction trx;
   protocol::active_transaction_data active_data;
   trx.set_active( converter::as< std::string >( active_data ) );

   ctx.set_transaction( trx );
   BOOST_REQUIRE_THROW( chain::system_call::require_authority( ctx, foo_account_string ), chain::invalid_signature );

   trx.set_signature_data( converter::as< std::string >( foo_key.sign_compact( crypto::hash( crypto::multicodec::sha2_256, trx.active() ) ) ) );
   ctx.set_transaction( trx );

   chain::system_call::require_authority( ctx, foo_account_string );

   BOOST_REQUIRE_THROW( chain::system_call::require_authority( ctx, bar_account_string ), chain::invalid_signature );

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( transaction_nonce_test )
{ try {
   using namespace koinos;

   BOOST_TEST_MESSAGE( "Test transaction nonce" );

   auto key = crypto::private_key::regenerate( crypto::hash( crypto::multicodec::sha2_256, "alpha bravo charlie delta"s ) );

   protocol::transaction transaction;
   protocol::active_transaction_data active;
   active.set_resource_limit( 20 );
   active.set_nonce( 0 );
   transaction.set_active( converter::as< std::string >( active ) );
   transaction.set_id( converter::as< std::string >( crypto::hash( crypto::multicodec::sha2_256, active ) ) );
   transaction.set_signature_data( converter::as< std::string >( key.sign_compact( converter::to< crypto::multihash >( transaction.id() ) ) ) );

   chain::system_call::apply_transaction( ctx, transaction );
   auto payer = chain::system_call::get_transaction_payer( ctx, transaction ).value();
   auto next_nonce = chain::system_call::get_account_nonce( ctx, payer ).value();
   BOOST_REQUIRE_EQUAL( next_nonce, 1 );

   BOOST_TEST_MESSAGE( "Test duplicate transaction nonce" );
   active.set_resource_limit( 25 );
   active.set_nonce( 0 );
   transaction.set_active( converter::as< std::string >( active ) );
   transaction.set_id( converter::as< std::string >( crypto::hash( crypto::multicodec::sha2_256, transaction.active() ) ) );
   transaction.set_signature_data( converter::as< std::string >( key.sign_compact( converter::to< crypto::multihash >( transaction.id() ) ) ) );

   BOOST_REQUIRE_THROW( chain::system_call::apply_transaction( ctx, transaction ), chain::chain_exception );

   next_nonce = chain::system_call::get_account_nonce( ctx, payer ).value();
   BOOST_REQUIRE_EQUAL( next_nonce, 1 );

   BOOST_TEST_MESSAGE( "Test next transaction nonce" );
   active.set_nonce( 1 );
   transaction.set_active( converter::as< std::string >( active ) );
   transaction.set_id( converter::as< std::string >( crypto::hash( crypto::multicodec::sha2_256, active ) ) );
   transaction.set_signature_data( converter::as< std::string >( key.sign_compact( converter::to< crypto::multihash >( transaction.id() ) ) ) );

   chain::system_call::apply_transaction( ctx, transaction );

   next_nonce = chain::system_call::get_account_nonce( ctx, payer ).value();
   BOOST_REQUIRE_EQUAL( next_nonce, 2 );

   BOOST_TEST_MESSAGE( "Test duplicate transaction nonce" );
   active.set_resource_limit( 30 );
   transaction.set_active( converter::as< std::string >( active ) );
   transaction.set_id( converter::as< std::string >( crypto::hash( crypto::multicodec::sha2_256, active ) ) );
   transaction.set_signature_data( converter::as< std::string >( key.sign_compact( converter::to< crypto::multihash >( transaction.id() ) ) ) );

   BOOST_REQUIRE_THROW( chain::system_call::apply_transaction( ctx, transaction ), chain::chain_exception );

   next_nonce = chain::system_call::get_account_nonce( ctx, payer ).value();
   BOOST_REQUIRE_EQUAL( next_nonce, 2 );

   BOOST_TEST_MESSAGE( "Test next transaction nonce" );
   active.set_nonce( 2 );
   transaction.set_active( converter::as< std::string >( active ) );
   transaction.set_id( converter::as< std::string >( crypto::hash( crypto::multicodec::sha2_256, active ) ) );
   transaction.set_signature_data( converter::as< std::string >( key.sign_compact( converter::to< crypto::multihash >( transaction.id() ) ) ) );

   chain::system_call::apply_transaction( ctx, transaction );

   next_nonce = chain::system_call::get_account_nonce( ctx, payer ).value();
   BOOST_REQUIRE_EQUAL( next_nonce, 3 );
} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( get_contract_id_test )
{ try {
   auto contract_id = crypto::hash( crypto::multicodec::ripemd_160, "get_contract_id_test"s ).digest();

   ctx.push_frame( chain::stack_frame {
      .call = contract_id,
      .call_privilege = chain::privilege::kernel_mode
   } );

   auto id = chain::system_call::get_contract_id( ctx );

   BOOST_REQUIRE( contract_id.size() == id.value().size() );
   auto id_bytes = converter::as< std::vector< std::byte > >( id.value() );
   BOOST_REQUIRE( std::equal( contract_id.begin(), contract_id.end(), id_bytes.begin() ) );
} KOINOS_CATCH_LOG_AND_RETHROW(info) }
#if 0
BOOST_AUTO_TEST_CAS E( token_tests )
{ try {
   using namespace koinos;

   auto contract_private_key = crypto::private_key::regenerate( crypto::hash( CRYPTO_SHA2_256_ID, "token_contract"s ) );
   auto contract_address = contract_private_key.get_public_key().to_address();
   protocol::transaction trx;
   sign_transaction( trx, contract_private_key );
   ctx.set_transaction( trx );

   koinos::protocol::upload_contract_operation op;
   auto id = koinos::crypto::hash( CRYPTO_RIPEMD160_ID, contract_address );
   std::memcpy( op.contract_id.data(), id.digest.data(), op.contract_id.size() );
   auto bytecode = get_koin_wasm();
   op.bytecode.insert( op.bytecode.end(), bytecode.begin(), bytecode.end() );

   system_call::apply_upload_contract_operation( ctx, op );

   BOOST_TEST_MESSAGE( "Test executing a contract" );

   ctx.set_privilege( chain::privilege::user_mode );

   contract_id_type contract_id;
   std::memcpy( contract_id.data(), id.digest.data(), contract_id.size() );

   ctx.set_meter_ticks(KOINOS_MAX_METER_TICKS);
   auto response = system_call::execute_contract( ctx, contract_id, 0x76ea4297, variable_blob() );
   auto name = pack::from_variable_blob< std::string >( response );
   LOG(info) << name;
   LOG(info) << "koin.name() opcode count: " << ctx.get_used_meter_ticks();

   response.clear();
   ctx.set_meter_ticks(KOINOS_MAX_METER_TICKS);
   response = system_call::execute_contract( ctx, contract_id, 0x7e794b24, variable_blob() );
   auto symbol = pack::from_variable_blob< std::string >( response );
   LOG(info) << symbol;
   LOG(info) << "koin.symbol() opcode count: " << ctx.get_used_meter_ticks();

   response.clear();
   ctx.set_meter_ticks(KOINOS_MAX_METER_TICKS);
   response = system_call::execute_contract( ctx, contract_id, 0x59dc15ce, variable_blob() );
   auto decimals = pack::from_variable_blob< uint8_t >( response );
   LOG(info) << (uint32_t)decimals;
   LOG(info) << "koin.decimals() opcode count: " << ctx.get_used_meter_ticks();

   response.clear();
   ctx.set_meter_ticks(KOINOS_MAX_METER_TICKS);
   response = system_call::execute_contract( ctx, contract_id, 0xcf2e8212, variable_blob() );
   auto supply = pack::from_variable_blob< uint64_t >( response );
   LOG(info) << "KOIN supply: " << supply;
   LOG(info) << "koin.total_supply() opcode count: " << ctx.get_used_meter_ticks();

   auto alice_private_key = crypto::private_key::regenerate( crypto::hash( CRYPTO_SHA2_256_ID, "alice"s ) );
   auto alice_address = alice_private_key.get_public_key().to_address();

   auto bob_private_key = crypto::private_key::regenerate( crypto::hash( CRYPTO_SHA2_256_ID, "bob"s ) );
   auto bob_address = bob_private_key.get_public_key().to_address();

   response.clear();
   ctx.set_meter_ticks(KOINOS_MAX_METER_TICKS);
   response = system_call::execute_contract( ctx, contract_id, 0x15619248, pack::to_variable_blob( alice_address ) );
   auto balance = pack::from_variable_blob< uint64_t >( response );
   LOG(info) << "koin.balance_of(alice) opcode count: " << ctx.get_used_meter_ticks();

   LOG(info) << "'alice' balance: " << balance;

   response.clear();
   ctx.set_meter_ticks(KOINOS_MAX_METER_TICKS);
   response = system_call::execute_contract( ctx, contract_id, 0x15619248, pack::to_variable_blob( bob_address ) );
   balance = pack::from_variable_blob< uint64_t >( response );
   LOG(info) << "koin.balance_of(bob) opcode count: " << ctx.get_used_meter_ticks();

   LOG(info) << "'bob' balance: " << balance;

   ctx.get_pending_console_output();

   LOG(info) << "Mint to 'alice'";
   auto m_args = mint_args{ .to = alice_address, .value = 100 };
   response.clear();
   ctx.set_meter_ticks(KOINOS_MAX_METER_TICKS);
   response = system_call::execute_contract( ctx, contract_id, 0xc2f82bdc, pack::to_variable_blob( m_args ) );
   auto success = pack::from_variable_blob< bool >( response );
   BOOST_REQUIRE( !success );
   LOG(info) << "koin.mint() opcode count (failure case): " << ctx.get_used_meter_ticks();

   ctx.set_privilege( chain::privilege::kernel_mode );
   response.clear();
   ctx.set_meter_ticks(KOINOS_MAX_METER_TICKS);
   response = system_call::execute_contract( ctx, contract_id, 0xc2f82bdc, pack::to_variable_blob( m_args ) );
   success = pack::from_variable_blob< bool >( response );
   BOOST_REQUIRE( success );
   LOG(info) << "koin.mint() opcode count (success case): " << ctx.get_used_meter_ticks();

   response.clear();
   ctx.set_meter_ticks(KOINOS_MAX_METER_TICKS);
   response = system_call::execute_contract( ctx, contract_id, 0x15619248, pack::to_variable_blob( alice_address ) );
   balance = pack::from_variable_blob< uint64_t >( response );
   LOG(info) << "koin.balance_of(alice) opcode count: " << ctx.get_used_meter_ticks();

   LOG(info) << "'alice' balance: " << balance;

   response.clear();
   ctx.set_meter_ticks(KOINOS_MAX_METER_TICKS);
   response = system_call::execute_contract( ctx, contract_id, 0x15619248, pack::to_variable_blob( bob_address ) );
   balance = pack::from_variable_blob< uint64_t >( response );
   LOG(info) << "koin.balance_of(bob) opcode count: " << ctx.get_used_meter_ticks();

   LOG(info) << "'bob' balance: " << balance;

   response.clear();
   ctx.set_meter_ticks(KOINOS_MAX_METER_TICKS);
   response = system_call::execute_contract( ctx, contract_id, 0xcf2e8212, variable_blob() );
   supply = pack::from_variable_blob< uint64_t >( response );
   LOG(info) << "KOIN supply: " << supply;
   LOG(info) << "koin.total_supply() opcode count: " << ctx.get_used_meter_ticks();

   LOG(info) << "Transfer from 'alice' to 'bob'";
   auto t_args = transfer_args{ .from = alice_address, .to = bob_address, .value = 25 };
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
   ctx.set_meter_ticks(KOINOS_MAX_METER_TICKS);
   response = system_call::execute_contract( ctx, contract_id, 0x62efa292, pack::to_variable_blob( t_args ) );
   success = pack::from_variable_blob< bool >( response );
   BOOST_REQUIRE( success );
   LOG(info) << "koin.transfer() opcode count: " << ctx.get_used_meter_ticks();

   response.clear();
   ctx.set_meter_ticks(KOINOS_MAX_METER_TICKS);
   response = system_call::execute_contract( ctx, contract_id, 0x15619248, pack::to_variable_blob( alice_address ) );
   balance = pack::from_variable_blob< uint64_t >( response );

   LOG(info) << "'alice' balance: " << balance;

   response.clear();
   ctx.set_meter_ticks(KOINOS_MAX_METER_TICKS);
   response = system_call::execute_contract( ctx, contract_id, 0x15619248, pack::to_variable_blob( bob_address ) );
   balance = pack::from_variable_blob< uint64_t >( response );

   LOG(info) << "'bob' balance: " << balance;

   response.clear();
   ctx.set_meter_ticks(KOINOS_MAX_METER_TICKS);
   response = system_call::execute_contract( ctx, contract_id, 0xcf2e8212, variable_blob() );
   supply = pack::from_variable_blob< uint64_t >( response );
   LOG(info) << "KOIN supply: " << supply;

}
catch( const koinos::vm_manager::vm_exception& e )
{
   LOG(info) << e.what();
   BOOST_FAIL("VM Exception");
}
KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CAS E( tick_limit )
{ try {
   using namespace koinos;
   BOOST_TEST_MESSAGE( "Upload forever contract" );

   auto contract_private_key = koinos::crypto::private_key::regenerate( koinos::crypto::hash( CRYPTO_SHA2_256_ID, "contract"s ) );
   protocol::transaction trx;
   sign_transaction( trx, contract_private_key );
   ctx.set_transaction( trx );

   protocol::upload_contract_operation op;
   auto id = crypto::hash( CRYPTO_RIPEMD160_ID, contract_private_key.get_public_key().to_address() );
   std::memcpy( op.contract_id.data(), id.digest.data(), op.contract_id.size() );
   auto bytecode = get_forever_wasm();
   op.bytecode.insert( op.bytecode.end(), bytecode.begin(), bytecode.end() );

   system_call::apply_upload_contract_operation( ctx, op );

   koinos::uint256 contract_key = koinos::pack::from_fixed_blob< koinos::uint160_t >( op.contract_id );
   auto stored_bytecode = system_call::db_get_object( ctx, koinos::chain::database::contract_space, contract_key, bytecode.size() );

   BOOST_REQUIRE( stored_bytecode.size() == bytecode.size() );
   BOOST_REQUIRE( std::memcmp( stored_bytecode.data(), bytecode.data(), bytecode.size() ) == 0 );

   BOOST_TEST_MESSAGE( "Execute forever contract" );

   ctx.set_meter_ticks(KOINOS_MAX_METER_TICKS);
   koinos::protocol::call_contract_operation op2;
   std::memcpy( op2.contract_id.data(), id.digest.data(), op2.contract_id.size() );
   BOOST_REQUIRE_THROW( system_call::apply_execute_contract_operation( ctx, op2 ), koinos::vm_manager::tick_meter_exception );

} KOINOS_CATCH_LOG_AND_RETHROW(info) }
#endif
BOOST_AUTO_TEST_SUITE_END()
