
#include <filesystem>

#include <boost/test/unit_test.hpp>
#include <boost/filesystem.hpp>

#include <koinos/chain/controller.hpp>
#include <koinos/chain/execution_context.hpp>
#include <koinos/chain/host_api.hpp>
#include <koinos/chain/state.hpp>
#include <koinos/chain/system_calls.hpp>
#include <koinos/chain/thunk_dispatcher.hpp>
#include <koinos/crypto/elliptic.hpp>

#include <koinos/tests/wasm/stack/call_contract.hpp>
#include <koinos/tests/wasm/stack/call_system_call.hpp>
#include <koinos/tests/wasm/stack/call_system_call2.hpp>
#include <koinos/tests/wasm/stack/stack_assertion.hpp>
#include <koinos/tests/wasm/stack/system_from_user.hpp>
#include <koinos/tests/wasm/stack/system_from_system.hpp>
#include <koinos/tests/wasm/stack/user_from_user.hpp>

#include <koinos/util/hex.hpp>

using namespace koinos;
using namespace std::string_literals;

/**
 * For these tests, we sometimes need to override system calls.
 * The ones chosen for this are set_contract_result and event because both have void return types
 * and are not needed for these tests.
 *
 * log also has a void return type, but is used for logging error messages by the contracts
 */

struct stack_fixture
{
   stack_fixture() :
      vm_backend( koinos::vm_manager::get_vm_backend() ),
      ctx( vm_backend, chain::intent::transaction_application ),
      host( ctx )
   {
      KOINOS_ASSERT( vm_backend, koinos::chain::unknown_backend_exception, "Couldn't get VM backend" );

      initialize_logging( "koinos_test", {}, "info" );

      temp = std::filesystem::temp_directory_path() / boost::filesystem::unique_path().string();
      std::filesystem::create_directory( temp );

      auto seed = "test seed"s;
      _genesis_private_key = crypto::private_key::regenerate( crypto::hash( crypto::multicodec::sha2_256, seed ) );

      auto entry = _genesis_data.add_entries();
      entry->set_key( chain::state::key::genesis_key );
      entry->set_value( _genesis_private_key.get_public_key().to_address_bytes() );
      *entry->mutable_space() = chain::state::space::metadata();

      koinos::chain::resource_limit_data rd;

      rd.set_disk_storage_cost( 10 );
      rd.set_disk_storage_limit( 204'800 );

      rd.set_network_bandwidth_cost( 5 );
      rd.set_network_bandwidth_limit( 1'048'576 );

      rd.set_compute_bandwidth_cost( 1 );
      rd.set_compute_bandwidth_limit( 100'000'000 );

      entry = _genesis_data.add_entries();
      entry->set_key( chain::state::key::resource_limit_data );
      entry->set_value( util::converter::as< std::string >( rd ) );
      *entry->mutable_space() = chain::state::space::metadata();

      koinos::chain::max_account_resources mar;

      mar.set_value( 10'000'000 );

      entry = _genesis_data.add_entries();
      entry->set_key( chain::state::key::max_account_resources );
      entry->set_value( util::converter::as< std::string >( mar ) );
      *entry->mutable_space() = chain::state::space::metadata();

      db.open( temp, [&]( state_db::state_node_ptr root )
      {
         // Write genesis objects into the database
         for ( const auto& entry : _genesis_data.entries() )
         {
            KOINOS_ASSERT(
               root->put_object( entry.space(), entry.key(), &entry.value() ) == entry.value().size(),
               koinos::chain::unexpected_state,
               "encountered unexpected object in initial state"
            );
         }
         LOG(info) << "Wrote " << _genesis_data.entries().size() << " genesis objects into new database";

         // Read genesis public key from the database, assert its existence at the correct location
         KOINOS_ASSERT(
            root->get_object( chain::state::space::metadata(), chain::state::key::genesis_key ),
            koinos::chain::unexpected_state,
            "could not find genesis public key in database"
         );

         // Calculate and write the chain ID into the database
         auto chain_id = crypto::hash( koinos::crypto::multicodec::sha2_256, _genesis_data );
         LOG(info) << "Calculated chain ID: " << chain_id;
         auto chain_id_str = util::converter::as< std::string >( chain_id );
         KOINOS_ASSERT(
            root->put_object( chain::state::space::metadata(), chain::state::key::chain_id, &chain_id_str ) == chain_id_str.size(),
            koinos::chain::unexpected_state,
            "encountered unexpected chain id in initial state"
         );
         LOG(info) << "Wrote chain ID into new database";
      } );

      ctx.set_state_node( db.create_writable_node( db.get_head()->id(), crypto::hash( crypto::multicodec::sha2_256, 1 ) ) );
      ctx.push_frame( chain::stack_frame {
         .contract_id = "stack_tests"s,
         .call_privilege = chain::privilege::kernel_mode
      } );

      ctx.resource_meter().set_resource_limit_data( chain::system_call::get_resource_limits( ctx ) );

      vm_backend->initialize();


      _stack_assertion_private_key = crypto::private_key::regenerate( crypto::hash( crypto::multicodec::sha2_256, "stack_assertion"s ) );
      koinos::protocol::upload_contract_operation op;
      op.set_contract_id( util::converter::as< std::string >( _stack_assertion_private_key.get_public_key().to_address_bytes() ) );
      op.set_bytecode( std::string( (const char*)stack_assertion_wasm, stack_assertion_wasm_len ) );

      koinos::protocol::transaction trx;
      sign_transaction( trx, _stack_assertion_private_key );
      ctx.set_transaction( trx );

      koinos::chain::system_call::apply_upload_contract_operation( ctx, op );
   }

   ~stack_fixture()
   {
      boost::log::core::get()->remove_all_sinks();
      db.close();
      std::filesystem::remove_all( temp );
   }

   void set_transaction_merkle_roots( protocol::transaction& transaction, crypto::multicodec code, crypto::digest_size size = crypto::digest_size( 0 ) )
   {
      std::vector< crypto::multihash > operations;
      operations.reserve( transaction.operations().size() );

      for ( const auto& op : transaction.operations() )
      {
         operations.emplace_back( crypto::hash( code, op, size ) );
      }

      auto operation_merkle_tree = crypto::merkle_tree( code, operations );
      transaction.mutable_header()->set_operation_merkle_root( util::converter::as< std::string >( operation_merkle_tree.root()->hash() ) );
   }

   void sign_transaction( protocol::transaction& transaction, crypto::private_key& transaction_signing_key );

   std::filesystem::path temp;
   koinos::state_db::database db;
   std::shared_ptr< koinos::vm_manager::vm_backend > vm_backend;
   koinos::chain::execution_context ctx;
   koinos::chain::host_api host;
   koinos::crypto::private_key _genesis_private_key;
   koinos::crypto::private_key _stack_assertion_private_key;
   chain::genesis_data _genesis_data;
};

void stack_fixture::sign_transaction( protocol::transaction& transaction, crypto::private_key& transaction_signing_key )
{
   // Signature is on the hash of the active data
   auto id_mh = crypto::hash( crypto::multicodec::sha2_256, transaction.header() );
   transaction.set_id( util::converter::as< std::string >( id_mh ) );
//   transaction.set_signature( util::converter::as< std::string >( transaction_signing_key.sign_compact( id_mh ) ) );
}

namespace koinos::chain::thunk {

void dummy_thunk( execution_context& ctx, const std::string& )
{
   system_call::event( ctx, "foo", "bar", std::vector< std::string >() );
}

} // koinos::chain::thunk

BOOST_FIXTURE_TEST_SUITE( stack_tests, stack_fixture )

BOOST_AUTO_TEST_CASE( simple_user_contract )
{ try {
   /**
    * Top User Contract (User Mode, read from DB)
    * |  Call Contract (Kernel Mode)
    * |  Apply Call Contract Operation (Drop to User Mode)
    * V  Apply Transaction (Kernel Mode)
    */

   // User contract checks caller is in user mode (apply_transaction dropping to user)
   // And then asserts it is in user mode

   auto user_key = crypto::private_key::regenerate( crypto::hash( crypto::multicodec::sha2_256, "contract_key"s ) );
   koinos::protocol::transaction trx;
   protocol::upload_contract_operation upload_op;
   upload_op.set_contract_id( util::converter::as< std::string >( user_key.get_public_key().to_address_bytes() ) );
   upload_op.set_bytecode( std::string( (const char*)user_from_user_wasm, user_from_user_wasm_len ) );
   sign_transaction( trx, user_key );
   ctx.set_transaction( trx );
   chain::system_call::apply_upload_contract_operation( ctx, upload_op );

   trx.mutable_header()->set_rc_limit( 100'000 );
   trx.mutable_header()->set_nonce( 0 );
   auto call_op = trx.add_operations()->mutable_call_contract();
   call_op->set_contract_id( upload_op.contract_id() );
   set_transaction_merkle_roots( trx, koinos::crypto::multicodec::sha2_256 );
   sign_transaction( trx, user_key );

   ctx.set_transaction( trx );
   try
   {
      chain::system_call::apply_transaction( ctx, trx );
   }
   catch ( ... )
   {
      BOOST_FAIL( ctx.get_pending_console_output() );
   }

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( syscall_from_user )
{ try {
   /**
    * Top System Call (Kernel Mode, read from DB)
    * |  Call Contract (Kernel Mode)
    * |  User Code (User Mode, read from DB)
    * |  Call Contract (Kernel Mode)
    * |  Apply Call Contract Operation (Drop to User Mode)
    * V  Apply Transaction (Kernel Mode)
    */

   // Syscall override checks caller is in user mode (user contract calling to syscall)
   // And then asserts it is in kernel mode

   auto override_key = crypto::private_key::regenerate( crypto::hash( crypto::multicodec::sha2_256, "override_key"s ) );
   protocol::transaction trx;
   protocol::upload_contract_operation upload_op;
   upload_op.set_contract_id( util::converter::as< std::string >( override_key.get_public_key().to_address_bytes() ) );
   upload_op.set_bytecode( std::string( (const char*)system_from_user_wasm, system_from_user_wasm_len ) );
   sign_transaction( trx, override_key );
   ctx.set_transaction( trx );
   chain::system_call::apply_upload_contract_operation( ctx, upload_op );

   protocol::set_system_contract_operation set_system_op;
   set_system_op.set_contract_id( upload_op.contract_id() );
   set_system_op.set_system_contract( true );

   sign_transaction( trx, _genesis_private_key );
   ctx.set_transaction( trx );
   chain::system_call::apply_set_system_contract_operation( ctx, set_system_op );

   protocol::set_system_call_operation set_syscall_op;
   set_syscall_op.set_call_id( std::underlying_type_t< chain::system_call_id >( chain::system_call_id::set_contract_result ) );
   set_syscall_op.mutable_target()->mutable_system_call_bundle()->set_contract_id( upload_op.contract_id() );
   set_syscall_op.mutable_target()->mutable_system_call_bundle()->set_entry_point( 0 );
   chain::system_call::apply_set_system_call_operation( ctx, set_syscall_op );

   auto user_key = crypto::private_key::regenerate( crypto::hash( crypto::multicodec::sha2_256, "contract_key"s ) );
   upload_op.set_contract_id( util::converter::as< std::string >( user_key.get_public_key().to_address_bytes() ) );
   upload_op.set_bytecode( std::string( (const char*)call_system_call_wasm, call_system_call_wasm_len ) );
   sign_transaction( trx, user_key );
   chain::system_call::apply_upload_contract_operation( ctx, upload_op );

   // We need to update the state node after a system call override
   ctx.set_state_node( ctx.get_state_node()->create_anonymous_node() );

   trx.mutable_header()->set_rc_limit( 100'000 );
   trx.mutable_header()->set_nonce( 0 );
   auto call_op = trx.add_operations()->mutable_call_contract();
   call_op->set_contract_id( upload_op.contract_id() );
   set_transaction_merkle_roots( trx, koinos::crypto::multicodec::sha2_256 );
   sign_transaction( trx, user_key );

   ctx.set_transaction( trx );
   try
   {
      chain::system_call::apply_transaction( ctx, trx );
   }
   catch ( ... )
   {
      BOOST_FAIL( ctx.get_pending_console_output() );
   }

   set_system_op.set_contract_id( call_op->contract_id() );
   sign_transaction( trx, _genesis_private_key );
   ctx.set_transaction( trx );
   chain::system_call::apply_set_system_contract_operation( ctx, set_system_op );

   trx.mutable_header()->set_nonce( 1 );
   set_transaction_merkle_roots( trx, koinos::crypto::multicodec::sha2_256 );
   sign_transaction( trx, user_key );

   ctx.set_transaction( trx );
   try
   {
      chain::system_call::apply_transaction( ctx, trx );
      BOOST_FAIL( "no reversion when called from system context" );
   }
   catch ( ... ) { /* do nothing, success */ }

} KOINOS_CATCH_LOG_AND_RETHROW(info) }

BOOST_AUTO_TEST_CASE( user_from_user )
{
   /**
    * Top User Code (User Mode, read from DB)
    * |  Call Contract (Kernel Mode)
    * |  User Code (User Mode, read from DB)
    * |  Call Contract (Kernel Mode)
    * |  Apply Call Contract Operation (Drop to User Mode)
    * V  Apply Transaction (Kernel Mode)
    */
   // User contract checks if being called from user mode then asserts it is in user mode

   auto user_key = crypto::private_key::regenerate( crypto::hash( crypto::multicodec::sha2_256, "contract_key"s ) );
   koinos::protocol::transaction trx;
   protocol::upload_contract_operation upload_op;
   upload_op.set_contract_id( util::converter::as< std::string >( user_key.get_public_key().to_address_bytes() ) );
   upload_op.set_bytecode( std::string( (const char*)user_from_user_wasm, user_from_user_wasm_len ) );
   sign_transaction( trx, user_key );
   ctx.set_transaction( trx );
   chain::system_call::apply_upload_contract_operation( ctx, upload_op );

   auto calling_key = crypto::private_key::regenerate( crypto::hash( crypto::multicodec::sha2_256, "calling_key"s ) );
   upload_op.set_contract_id( util::converter::as< std::string >( calling_key.get_public_key().to_address_bytes() ) );
   upload_op.set_bytecode( std::string( (const char*)call_contract_wasm, call_contract_wasm_len ) );
   sign_transaction( trx, calling_key );
   ctx.set_transaction( trx );
   chain::system_call::apply_upload_contract_operation( ctx, upload_op );

   trx.mutable_header()->set_rc_limit( 100'000 );
   trx.mutable_header()->set_nonce( 0 );
   auto call_op = trx.add_operations()->mutable_call_contract();
   call_op->set_contract_id( upload_op.contract_id() );
   set_transaction_merkle_roots( trx, koinos::crypto::multicodec::sha2_256 );
   sign_transaction( trx, calling_key );

   ctx.set_transaction( trx );
   try
   {
      chain::system_call::apply_transaction( ctx, trx );
   }
   catch ( ... )
   {
      BOOST_FAIL( ctx.get_pending_console_output() );
   }

   protocol::set_system_contract_operation set_system_op;
   set_system_op.set_contract_id( upload_op.contract_id() );
   set_system_op.set_system_contract( true );

   sign_transaction( trx, _genesis_private_key );
   ctx.set_transaction( trx );
   chain::system_call::apply_set_system_contract_operation( ctx, set_system_op );

   trx.mutable_header()->set_nonce( 1 );
   set_transaction_merkle_roots( trx, koinos::crypto::multicodec::sha2_256 );
   sign_transaction( trx, user_key );

   ctx.set_transaction( trx );
   try
   {
      chain::system_call::apply_transaction( ctx, trx );
      BOOST_FAIL( "no reversion when called from system context" );
   }
   catch ( ... ) { /* do nothing, success */ }
}

BOOST_AUTO_TEST_CASE( syscall_override_from_thunk )
{
   /**
    * Top System Call (Kernel Mode, read from DB )
    * |  Call Contract (Kernel Mode)
    * |  System Call (Kernel Mode)
    * |  User Code (User Mode, read from DB)
    * |  Call Contract (Kernel Mode)
    * |  Apply Call Contract Operation (Drop to User Mode)
    * V  Apply Transaction (Kernel Mode)
    */
   const_cast< chain::thunk_dispatcher& >( chain::thunk_dispatcher::instance() ).register_thunk< chain::log_arguments, chain::log_result >( 99, chain::thunk::dummy_thunk );

   // Upload and override event
   auto override_key = crypto::private_key::regenerate( crypto::hash( crypto::multicodec::sha2_256, "override_key"s ) );
   protocol::transaction trx;
   protocol::upload_contract_operation upload_op;
   upload_op.set_contract_id( util::converter::as< std::string >( override_key.get_public_key().to_address_bytes() ) );
   upload_op.set_bytecode( std::string( (const char*)system_from_system_wasm, system_from_system_wasm_len ) );
   sign_transaction( trx, override_key );
   ctx.set_transaction( trx );
   chain::system_call::apply_upload_contract_operation( ctx, upload_op );

   protocol::set_system_contract_operation set_system_op;
   set_system_op.set_contract_id( upload_op.contract_id() );
   set_system_op.set_system_contract( true );

   sign_transaction( trx, _genesis_private_key );
   ctx.set_transaction( trx );
   chain::system_call::apply_set_system_contract_operation( ctx, set_system_op );

   protocol::set_system_call_operation set_syscall_op;
   set_syscall_op.set_call_id( std::underlying_type_t< chain::system_call_id >( chain::system_call_id::event ) );
   set_syscall_op.mutable_target()->mutable_system_call_bundle()->set_contract_id( upload_op.contract_id() );
   set_syscall_op.mutable_target()->mutable_system_call_bundle()->set_entry_point( 0 );
   chain::system_call::apply_set_system_call_operation( ctx, set_syscall_op );

   // Override set_contract_result with dummy_thunk
   set_syscall_op.set_call_id( std::underlying_type_t< chain::system_call_id >( chain::system_call_id::set_contract_result ) );
   set_syscall_op.mutable_target()->set_thunk_id( 99 );
   chain::system_call::apply_set_system_call_operation( ctx, set_syscall_op );

   // Upload user contract
   auto user_key = crypto::private_key::regenerate( crypto::hash( crypto::multicodec::sha2_256, "contract_key"s ) );
   upload_op.set_contract_id( util::converter::as< std::string >( user_key.get_public_key().to_address_bytes() ) );
   upload_op.set_bytecode( std::string( (const char*)call_system_call_wasm, call_system_call_wasm_len ) );
   sign_transaction( trx, user_key );
   chain::system_call::apply_upload_contract_operation( ctx, upload_op );

   // Call user contract
   // We need to update the state node after a system call override
   ctx.set_state_node( ctx.get_state_node()->create_anonymous_node() );

   trx.mutable_header()->set_rc_limit( 100'000 );
   trx.mutable_header()->set_nonce( 0 );
   auto call_op = trx.add_operations()->mutable_call_contract();
   call_op->set_contract_id( upload_op.contract_id() );
   set_transaction_merkle_roots( trx, koinos::crypto::multicodec::sha2_256 );
   sign_transaction( trx, user_key );

   ctx.set_transaction( trx );
   try
   {
      chain::system_call::apply_transaction( ctx, trx );
   }
   catch ( ... )
   {
      BOOST_FAIL( ctx.get_pending_console_output() );
   }
}

BOOST_AUTO_TEST_CASE( syscall_override_from_syscall_override )
{
   /**
    * Top System Call (Kernel Mode, read from DB )
    * |  Call Contract (Kernel Mode)
    * |  System Call (Kernel Mode, read from DB)
    * |  Call Contract (Kernel Mode)
    * |  User Code (User Mode, read from DB)
    * |  Call Contract (Kernel Mode)
    * |  Apply Call Contract Operation (Drop to User Mode)
    * V  Apply Transaction (Kernel Mode)
    */

   // Upload and override event
   auto override_key = crypto::private_key::regenerate( crypto::hash( crypto::multicodec::sha2_256, "override_key"s ) );
   protocol::transaction trx;
   protocol::upload_contract_operation upload_op;
   upload_op.set_contract_id( util::converter::as< std::string >( override_key.get_public_key().to_address_bytes() ) );
   upload_op.set_bytecode( std::string( (const char*)system_from_system_wasm, system_from_system_wasm_len ) );
   sign_transaction( trx, override_key );
   ctx.set_transaction( trx );
   chain::system_call::apply_upload_contract_operation( ctx, upload_op );

   protocol::set_system_contract_operation set_system_op;
   set_system_op.set_contract_id( upload_op.contract_id() );
   set_system_op.set_system_contract( true );

   sign_transaction( trx, _genesis_private_key );
   ctx.set_transaction( trx );
   chain::system_call::apply_set_system_contract_operation( ctx, set_system_op );

   protocol::set_system_call_operation set_syscall_op;
   set_syscall_op.set_call_id( std::underlying_type_t< chain::system_call_id >( chain::system_call_id::event ) );
   set_syscall_op.mutable_target()->mutable_system_call_bundle()->set_contract_id( upload_op.contract_id() );
   set_syscall_op.mutable_target()->mutable_system_call_bundle()->set_entry_point( 0 );
   chain::system_call::apply_set_system_call_operation( ctx, set_syscall_op );

   // Upload and override set_contract_result
   auto override_key2 = crypto::private_key::regenerate( crypto::hash( crypto::multicodec::sha2_256, "override_key2"s ) );
   upload_op.set_contract_id( util::converter::as< std::string >( override_key2.get_public_key().to_address_bytes() ) );
   upload_op.set_bytecode( std::string( (const char*)call_system_call2_wasm, call_system_call2_wasm_len ) );
   sign_transaction( trx, override_key2 );
   ctx.set_transaction( trx );
   chain::system_call::apply_upload_contract_operation( ctx, upload_op );

   set_system_op.set_contract_id( upload_op.contract_id() );
   set_system_op.set_system_contract( true );

   sign_transaction( trx, _genesis_private_key );
   ctx.set_transaction( trx );
   chain::system_call::apply_set_system_contract_operation( ctx, set_system_op );

   set_syscall_op.set_call_id( std::underlying_type_t< chain::system_call_id >( chain::system_call_id::set_contract_result ) );
   set_syscall_op.mutable_target()->mutable_system_call_bundle()->set_contract_id( upload_op.contract_id() );
   set_syscall_op.mutable_target()->mutable_system_call_bundle()->set_entry_point( 0 );
   chain::system_call::apply_set_system_call_operation( ctx, set_syscall_op );

   // Upload user contract
   auto user_key = crypto::private_key::regenerate( crypto::hash( crypto::multicodec::sha2_256, "contract_key"s ) );
   upload_op.set_contract_id( util::converter::as< std::string >( user_key.get_public_key().to_address_bytes() ) );
   upload_op.set_bytecode( std::string( (const char*)call_system_call_wasm, call_system_call_wasm_len ) );
   sign_transaction( trx, user_key );
   chain::system_call::apply_upload_contract_operation( ctx, upload_op );

   // Call user contract
   // We need to update the state node after a system call override
   ctx.set_state_node( ctx.get_state_node()->create_anonymous_node() );

   trx.mutable_header()->set_rc_limit( 100'000 );
   trx.mutable_header()->set_nonce( 0 );
   auto call_op = trx.add_operations()->mutable_call_contract();
   call_op->set_contract_id( upload_op.contract_id() );
   set_transaction_merkle_roots( trx, koinos::crypto::multicodec::sha2_256 );
   sign_transaction( trx, user_key );

   ctx.set_transaction( trx );
   try
   {
      chain::system_call::apply_transaction( ctx, trx );
   }
   catch ( ... )
   {
      BOOST_FAIL( ctx.get_pending_console_output() );
   }
}

BOOST_AUTO_TEST_CASE( system_contract_from_syscall_override )
{
   /**
    * Top System Contract (Kernel Mode, read from DB )
    * |  Call Contract (Kernel Mode)
    * |  System Call (Kernel Mode, read from DB)
    * |  Call Contract (Kernel Mode)
    * |  User Code (User Mode, read from DB)
    * |  Call Contract (Kernel Mode)
    * |  Apply Call Contract Operation (Drop to User Mode)
    * V  Apply Transaction (Kernel Mode)
    */

   // Upload and override set_contract_result
   auto override_key = crypto::private_key::regenerate( crypto::hash( crypto::multicodec::sha2_256, "override_key"s ) );
   protocol::transaction trx;
   protocol::upload_contract_operation upload_op;
   upload_op.set_contract_id( util::converter::as< std::string >( override_key.get_public_key().to_address_bytes() ) );
   upload_op.set_bytecode( std::string( (const char*)call_contract_wasm, call_contract_wasm_len ) );
   sign_transaction( trx, override_key );
   ctx.set_transaction( trx );
   chain::system_call::apply_upload_contract_operation( ctx, upload_op );

   protocol::set_system_contract_operation set_system_op;
   set_system_op.set_contract_id( upload_op.contract_id() );
   set_system_op.set_system_contract( true );

   sign_transaction( trx, _genesis_private_key );
   ctx.set_transaction( trx );
   chain::system_call::apply_set_system_contract_operation( ctx, set_system_op );

   protocol::set_system_call_operation set_syscall_op;
   set_syscall_op.set_call_id( std::underlying_type_t< chain::system_call_id >( chain::system_call_id::set_contract_result ) );
   set_syscall_op.mutable_target()->mutable_system_call_bundle()->set_contract_id( upload_op.contract_id() );
   set_syscall_op.mutable_target()->mutable_system_call_bundle()->set_entry_point( 0 );
   chain::system_call::apply_set_system_call_operation( ctx, set_syscall_op );

   // Upload system contract
   auto system_contract = crypto::private_key::regenerate( crypto::hash( crypto::multicodec::sha2_256, "contract_key"s ) );
   upload_op.set_contract_id( util::converter::as< std::string >( system_contract.get_public_key().to_address_bytes() ) );
   upload_op.set_bytecode( std::string( (const char*)system_from_system_wasm, system_from_system_wasm_len ) );
   sign_transaction( trx, system_contract );
   ctx.set_transaction( trx );
   chain::system_call::apply_upload_contract_operation( ctx, upload_op );

   set_system_op.set_contract_id( upload_op.contract_id() );
   set_system_op.set_system_contract( true );

   sign_transaction( trx, _genesis_private_key );
   ctx.set_transaction( trx );
   chain::system_call::apply_set_system_contract_operation( ctx, set_system_op );

   // Upload user contract
   auto user_key = crypto::private_key::regenerate( crypto::hash( crypto::multicodec::sha2_256, "user_key"s ) );
   upload_op.set_contract_id( util::converter::as< std::string >( user_key.get_public_key().to_address_bytes() ) );
   upload_op.set_bytecode( std::string( (const char*)call_system_call_wasm, call_system_call_wasm_len ) );
   sign_transaction( trx, user_key );
   chain::system_call::apply_upload_contract_operation( ctx, upload_op );

   // Call user contract
   // We need to update the state node after a system call override
   ctx.set_state_node( ctx.get_state_node()->create_anonymous_node() );

   trx.mutable_header()->set_rc_limit( 100'000 );
   trx.mutable_header()->set_nonce( 0 );
   auto call_op = trx.add_operations()->mutable_call_contract();
   call_op->set_contract_id( upload_op.contract_id() );
   set_transaction_merkle_roots( trx, koinos::crypto::multicodec::sha2_256 );
   sign_transaction( trx, user_key );

   ctx.set_transaction( trx );
   try
   {
      chain::system_call::apply_transaction( ctx, trx );
   }
   catch ( ... )
   {
      BOOST_FAIL( ctx.get_pending_console_output() );
   }
}

BOOST_AUTO_TEST_SUITE_END()
