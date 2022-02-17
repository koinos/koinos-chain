#include <algorithm>
#include <string>
#include <stdexcept>

#include <google/protobuf/util/message_differencer.h>

#include <koinos/bigint.hpp>
#include <koinos/chain/execution_context.hpp>
#include <koinos/chain/constants.hpp>
#include <koinos/chain/host_api.hpp>
#include <koinos/chain/proto_utils.hpp>
#include <koinos/chain/state.hpp>
#include <koinos/chain/system_calls.hpp>
#include <koinos/chain/thunk_dispatcher.hpp>
#include <koinos/chain/session.hpp>
#include <koinos/crypto/multihash.hpp>

#include <koinos/log.hpp>

#include <koinos/util/base58.hpp>
#include <koinos/util/conversion.hpp>
#include <koinos/util/hex.hpp>

#include <koinos/chain/authority.pb.h>

using namespace std::string_literals;

namespace koinos::chain {

void register_thunks( thunk_dispatcher& td )
{
   THUNK_REGISTER( td,
      // General Blockchain Management
      (get_head_info)
      (apply_block)
      (apply_transaction)
      (apply_upload_contract_operation)
      (apply_call_contract_operation)
      (apply_set_system_call_operation)
      (apply_set_system_contract_operation)
      (pre_block_callback)
      (post_block_callback)
      (pre_transaction_callback)
      (post_transaction_callback)

      // System Helpers
      (process_block_signature)
      (get_transaction)
      (get_transaction_field)
      (get_block)
      (get_block_field)
      (get_last_irreversible_block)
      (get_account_nonce)
      (verify_account_nonce)
      (set_account_nonce)
      (require_system_authority)

      // Resource Subsystem
      (get_account_rc)
      (consume_account_rc)
      (get_resource_limits)
      (consume_block_resources)

      // Database
      (put_object)
      (remove_object)
      (get_object)
      (get_next_object)
      (get_prev_object)

      // Logging
      (log)
      (event)

      // Cryptography
      (hash)
      (recover_public_key)
      (verify_merkle_root)
      (verify_signature)

      // Contract Management
      (call_contract)
      (get_entry_point)
      (get_contract_arguments)
      (set_contract_result)
      (exit_contract)
      (get_contract_id)
      (get_caller)
      (require_authority)
   )
}

// RAII class to ensure apply context block state is consistent if there is an error applying
// the block.
struct block_guard
{
   block_guard( execution_context& context, const protocol::block& block ) :
      ctx( context )
   {
      ctx.set_block( block );
   }

   ~block_guard()
   {
      ctx.clear_block();
   }

   execution_context& ctx;
};

// RAII class to ensure apply context transaction state is consistent if there is an error applying
// the transaction.
struct transaction_guard
{
   transaction_guard( execution_context& context, const protocol::transaction& trx ) :
      ctx( context )
   {
      ctx.set_transaction( trx );
   }

   ~transaction_guard()
   {
      ctx.clear_transaction();
   }

   execution_context& ctx;
};

void validate_hash_code( crypto::multicodec id )
{
   switch ( id )
   {
      case crypto::multicodec::sha1:
         [[fallthrough]];
      case crypto::multicodec::sha2_256:
         [[fallthrough]];
      case crypto::multicodec::sha2_512:
         [[fallthrough]];
      case crypto::multicodec::keccak_256:
         [[fallthrough]];
      case crypto::multicodec::ripemd_160:
         break;
      default:
         KOINOS_THROW( unknown_hash_code, "unknown hash code" );
   }
}

THUNK_DEFINE_BEGIN();

THUNK_DEFINE( void, log, ((const std::string&) msg) )
{
   context.chronicler().push_log( msg );
}

THUNK_DEFINE( void, exit_contract, ((uint32_t) exit_code) )
{
   switch( exit_code )
   {
      case exit_code::success:
          KOINOS_THROW( exit_success, "" );
      case exit_code::failure:
          KOINOS_THROW( exit_failure, "" );
      default:
          KOINOS_THROW( unknown_exit_code, "contract specified unknown exit code" );
   }
}

THUNK_DEFINE( process_block_signature_result, process_block_signature, ((const std::string&) id, (const protocol::block_header&) header, (const std::string&) signature_data) )
{
   auto genesis_addr = system_call::get_object( context, state::space::metadata(), state::key::genesis_key ).value();

   process_block_signature_result ret;
   ret.set_value( genesis_addr == util::converter::to< crypto::public_key >( system_call::recover_public_key( context, ecdsa_secp256k1, signature_data, id ) ).to_address_bytes() );
   return ret;
}

THUNK_DEFINE( verify_merkle_root_result, verify_merkle_root, ((const std::string&) root, (const std::vector< std::string >&) hashes) )
{
   auto root_hash = util::converter::to< crypto::multihash >( root );
   validate_hash_code( root_hash.code() );

   std::vector< crypto::multihash > leaves;

   leaves.resize( hashes.size() );
   std::transform( std::begin( hashes ), std::end( hashes ), std::begin( leaves ), []( const std::string& s ) { return util::converter::to< crypto::multihash >( s ); } );

   auto mtree = crypto::merkle_tree( root_hash.code(), leaves );

   auto merkle_root = mtree.root()->hash();

   verify_merkle_root_result ret;
   ret.set_value( merkle_root == root_hash );
   return ret;
}

THUNK_DEFINE_VOID( void, pre_block_callback ) { }

THUNK_DEFINE_VOID( void, pre_transaction_callback ) { }

THUNK_DEFINE( void, apply_block, ((const protocol::block&) block) )
{
   KOINOS_ASSERT( context.get_caller_privilege() == privilege::kernel_mode, insufficient_privileges, "calling privileged thunk from non-privileged code" );
   KOINOS_ASSERT( context.intent() == intent::block_application, unexpected_intent, "expected block application intent while applying block" );

   block_guard guard( context, block );

   context.receipt() = protocol::block_receipt();

   context.resource_meter().set_resource_limit_data( system_call::get_resource_limits( context ) );

   KOINOS_ASSERT(
      system_call::hash( context, std::underlying_type_t< crypto::multicodec >( context.block_hash_code() ), util::converter::as< std::string >( block.header() ) ) == block.id(),
      block_id_mismatch,
      "block contains an invalid block id"
   );

   const crypto::multihash tx_root = util::converter::to< crypto::multihash >( block.header().transaction_merkle_root() );
   KOINOS_ASSERT(
      tx_root.code() == context.block_hash_code(),
      hash_code_mismatch,
      "unexpected transaction merkle root hash code - was: ${t}, expected: ${e}",
      ("t", std::underlying_type_t< crypto::multicodec >( tx_root.code() ))("e", std::underlying_type_t< crypto::multicodec >( context.block_hash_code() ))
   );

   // Check transaction Merkle root
   std::vector< std::string > hashes;
   hashes.reserve( block.transactions_size() * 2 );

   std::size_t transactions_bytes_size = 0;
   for ( const auto& trx : block.transactions() )
   {
      transactions_bytes_size += trx.ByteSizeLong();
      hashes.emplace_back( system_call::hash( context, std::underlying_type_t< crypto::multicodec >( context.block_hash_code() ), util::converter::as< std::string >( trx.header() ) ) );
      std::stringstream ss;

      for ( const auto& sig : trx.signatures() )
      {
         ss << sig;
      }

      hashes.emplace_back( system_call::hash( context, std::underlying_type_t< crypto::multicodec >( context.block_hash_code() ), ss.str() ) );
   }

   context.resource_meter().use_network_bandwidth( block.ByteSizeLong() - transactions_bytes_size );

   KOINOS_ASSERT( system_call::verify_merkle_root( context, block.header().transaction_merkle_root(), hashes ), transaction_root_mismatch, "transaction merkle root does not match" );

   auto block_hash = util::converter::to< crypto::multihash >( system_call::hash( context, std::underlying_type_t< crypto::multicodec >( context.block_hash_code() ), util::converter::as< std::string >( block.header() ) ) );
   KOINOS_ASSERT(
      system_call::process_block_signature(
         context,
         util::converter::as< std::string >( block_hash ),
         block.header(),
         block.signature()
      ),
      invalid_block_signature,
      "block signature does not match"
   );

   system_call::pre_block_callback( context );

   system_call::put_object( context, state::space::metadata(), state::key::head_block_time, util::converter::as< std::string >( block.header().timestamp() ) );

   for ( const auto& tx : block.transactions() )
   {
      try
      {
         system_call::apply_transaction( context, tx );
      }
      catch( const transaction_reverted& ) {} /* do nothing */
      KOINOS_CAPTURE_CATCH_AND_RETHROW( ("transaction_id", util::to_hex( tx.id() )) )
   }

   system_call::post_block_callback( context );

   KOINOS_ASSERT(
      system_call::consume_block_resources(
         context,
         context.resource_meter().disk_storage_used(),
         context.resource_meter().network_bandwidth_used(),
         context.resource_meter().compute_bandwidth_used()
      ),
      unable_to_consume_resources,
      "unable to consume block resources"
   );

   auto& receipt = std::get< protocol::block_receipt >( context.receipt() );

   receipt.set_id( block.id() );
   receipt.set_disk_storage_used( context.resource_meter().disk_storage_used() );
   receipt.set_compute_bandwidth_used( context.resource_meter().compute_bandwidth_used() );
   receipt.set_network_bandwidth_used( context.resource_meter().network_bandwidth_used() );

   for ( const auto& [ within_session, event ] : context.chronicler().events() )
      if ( !within_session )
         *receipt.add_events() = event;

   for ( const auto& message : context.chronicler().logs() )
      *receipt.add_logs() = message;
}

THUNK_DEFINE( void, apply_transaction, ((const protocol::transaction&) trx) )
{
   KOINOS_ASSERT( context.get_caller_privilege() == privilege::kernel_mode, insufficient_privileges, "calling privileged thunk from non-privileged code" );
   KOINOS_ASSERT( !context.read_only(), read_only_context, "unable to perform action while context is read only" );

   transaction_guard guard( context, trx );

   protocol::transaction_receipt receipt;

   const auto& payer = trx.header().payer();

   auto payer_rc = system_call::get_account_rc( context, payer );
   KOINOS_ASSERT( payer_rc >= trx.header().rc_limit(), insufficient_rc, "payer does not have the rc to cover transaction rc limit" );

   auto start_disk_used    = context.resource_meter().disk_storage_used();
   auto start_network_used = context.resource_meter().network_bandwidth_used();
   auto start_compute_used = context.resource_meter().compute_bandwidth_used();

   /**
    * While a reference to the payer_session remains alive, resource usage will be tallied
    * and charged to the current payer.
    */
   auto payer_session = context.make_session( trx.header().rc_limit() );

   auto chain_id = system_call::get_object( context, state::space::metadata(), state::key::chain_id );
   KOINOS_ASSERT( chain_id.exists(), unexpected_state, "chain id does not exist" );
   KOINOS_ASSERT( trx.header().chain_id() == chain_id.value(), chain_id_mismatch, "chain id mismatch" );

   KOINOS_ASSERT(
      system_call::hash( context, std::underlying_type_t< crypto::multicodec >( context.block_hash_code() ), util::converter::as< std::string >( trx.header() ) ) == trx.id(),
      transaction_id_mismatch,
      "transaction contains an invalid transaction id"
   );

   const crypto::multihash op_root = util::converter::to< crypto::multihash >( trx.header().operation_merkle_root() );
   KOINOS_ASSERT(
      op_root.code() == context.block_hash_code(),
      hash_code_mismatch,
      "unexpected operation merkle root hash code - was: ${o}, expected: ${e}",
      ("o", std::underlying_type_t< crypto::multicodec >( op_root.code() ))("e", std::underlying_type_t< crypto::multicodec >( context.block_hash_code() ))
   );

   // Check operation merkle root
   std::vector< std::string > hashes;
   hashes.reserve( trx.operations_size() );

   for ( const auto& op : trx.operations() )
      hashes.emplace_back( system_call::hash( context, std::underlying_type_t< crypto::multicodec >( context.block_hash_code() ), util::converter::as< std::string >( op ) ) );

   KOINOS_ASSERT( system_call::verify_merkle_root( context, trx.header().operation_merkle_root(), hashes ), operation_root_mismatch, "operation merkle root does not match" );

   system_call::require_authority( context, rc_use, payer );

   const auto& payee = trx.header().payee();

   // If the payee is set and not the payer, then the payee account's nonce is used.
   bool use_payee_nonce = payee.size() && payee != payer;
   const auto& nonce_account = use_payee_nonce ? payee : payer;

   // If we are using the payee account's nonce, we also need to ensure they signed the transaction as well
   if ( use_payee_nonce )
   {
      system_call::require_authority( context, signature_exists, payee );
   }

   KOINOS_ASSERT(
      system_call::verify_account_nonce( context, nonce_account, trx.header().nonce() ),
      invalid_nonce,
      "invalid transaction nonce - account: ${a}, nonce: ${n}, current nonce: ${c}",
      ("a", util::to_base58( nonce_account ))("n", util::to_hex( trx.header().nonce() ))("c", util::to_hex( system_call::get_account_nonce( context, nonce_account) ))
   );

   system_call::pre_transaction_callback( context );

   // The anonymous node must be created after requiring authority and the pre transaction callback
   // because those calls might write to database and those writes must persist regardless of whether
   // the rest of the transaction is reverted or not.
   auto block_node = context.get_state_node();
   auto trx_node = block_node->create_anonymous_node();
   context.set_state_node( trx_node, block_node->get_parent() );

   std::exception_ptr reverted_exception_ptr;

   receipt.set_id( trx.id() );
   receipt.set_payer( payer );
   receipt.set_max_payer_rc( payer_rc );
   receipt.set_rc_limit( trx.header().rc_limit() );
   receipt.set_reverted( false );

   try
   {
      context.resource_meter().use_network_bandwidth( trx.ByteSizeLong() );

      for ( const auto& o : trx.operations() )
      {
         if ( o.has_upload_contract() )
            system_call::apply_upload_contract_operation( context, o.upload_contract() );
         else if ( o.has_call_contract() )
            system_call::apply_call_contract_operation( context, o.call_contract() );
         else if ( o.has_set_system_call() )
            system_call::apply_set_system_call_operation( context, o.set_system_call() );
         else if ( o.has_set_system_contract() )
            system_call::apply_set_system_contract_operation( context, o.set_system_contract() );
         else
            KOINOS_THROW( unknown_operation, "unknown operation" );
      }

      trx_node->commit();
   }
   catch ( const block_resource_limit_exception& e )
   {
      // If the block resources are exceeded, the exception propagates, failing the block
      throw e;
   }
   catch ( const std::exception& e )
   {
      // If the transaction fails for any other reason within the operations, it is reverted.
      // Mana is still charged, but the block does not fail
      LOG(info) << "Transaction " << util::to_hex( trx.id() ) << " reverted with: " << e.what();
      transaction_reverted tr( e.what() );
      reverted_exception_ptr = std::make_exception_ptr( tr );
      receipt.set_reverted( true );
   }

   context.set_state_node( block_node );

   system_call::set_account_nonce( context, nonce_account, trx.header().nonce() );

   system_call::post_transaction_callback( context );

   auto used_rc = payer_session->used_rc();

   receipt.set_rc_used( used_rc );
   receipt.set_disk_storage_used( context.resource_meter().disk_storage_used() - start_disk_used );
   receipt.set_network_bandwidth_used( context.resource_meter().network_bandwidth_used() - start_network_used );
   receipt.set_compute_bandwidth_used( context.resource_meter().compute_bandwidth_used() - start_compute_used );

   for ( const auto& e : payer_session->events() )
      *receipt.add_events() = e;

   for ( const auto& message : payer_session->logs() )
      *receipt.add_logs() = message;

   payer_session.reset();

   KOINOS_ASSERT(
      system_call::consume_account_rc( context, payer, used_rc ),
      unable_to_consume_resources,
      "unable to consume rc for payer: ${p}", ("p", util::to_base58( payer ) );
   );

   switch ( context.intent() )
   {
   case intent::block_application:
      KOINOS_ASSERT( std::holds_alternative< protocol::block_receipt >( context.receipt() ), unexpected_receipt, "expected block receipt with block application intent" );
      *std::get< protocol::block_receipt >( context.receipt() ).add_transaction_receipts() = receipt;
      break;
   case intent::transaction_application:
      context.receipt() = receipt;
      break;
   default:
      KOINOS_THROW( unexpected_intent, "transactions cannot be applied outside of the block application or transaction application intent" );
      break;
   }

   if ( reverted_exception_ptr )
   {
      std::rethrow_exception( reverted_exception_ptr );
   }
}

THUNK_DEFINE( void, apply_upload_contract_operation, ((const protocol::upload_contract_operation&) o) )
{
   KOINOS_ASSERT( context.get_caller_privilege() == privilege::kernel_mode, insufficient_privileges, "calling privileged thunk from non-privileged code" );
   KOINOS_ASSERT( !context.read_only(), read_only_context, "unable to perform action while context is read only" );

   system_call::require_authority( context, contract_upload, o.contract_id() );

   contract_metadata_object contract_meta;
   *contract_meta.mutable_hash() = system_call::hash( context, std::underlying_type_t< crypto::multicodec >( context.block_hash_code() ), o.bytecode() );
   contract_meta.set_authorizes_call_contract( o.authorizes_call_contract() );
   contract_meta.set_authorizes_use_rc( o.authorizes_use_rc() );
   contract_meta.set_authorizes_upload_contract( o.authorizes_upload_contract() );

   system_call::put_object( context, state::space::contract_bytecode(), o.contract_id(), o.bytecode() );
   system_call::put_object( context, state::space::contract_metadata(), o.contract_id(), util::converter::as< std::string >( contract_meta ) );
}

THUNK_DEFINE( void, apply_call_contract_operation, ((const protocol::call_contract_operation&) o) )
{
   KOINOS_ASSERT( context.get_caller_privilege() == privilege::kernel_mode, insufficient_privileges, "calling privileged thunk from non-privileged code" );
   KOINOS_ASSERT( !context.read_only(), read_only_context, "unable to perform action while context is read only" );

   // Drop to user mode
   with_privilege(
      context,
      privilege::user_mode,
      [&]() {
         system_call::call_contract( context, o.contract_id(), o.entry_point(), o.args() );
      }
   );
}

THUNK_DEFINE( void, apply_set_system_call_operation, ((const protocol::set_system_call_operation&) o) )
{
   KOINOS_ASSERT( context.get_caller_privilege() == privilege::kernel_mode, insufficient_privileges, "calling privileged thunk from non-privileged code" );
   KOINOS_ASSERT( !context.read_only(), read_only_context, "unable to perform action while context is read only" );

   system_call::require_system_authority( context, set_system_call );

   if ( o.target().has_system_call_bundle() )
   {
      auto contract_object = system_call::get_object( context, state::space::contract_bytecode(), o.target().system_call_bundle().contract_id() );
      KOINOS_ASSERT( contract_object.exists(), invalid_contract, "contract does not exist" );
      auto contract_meta_object = system_call::get_object( context, state::space::contract_metadata(), o.target().system_call_bundle().contract_id() );
      KOINOS_ASSERT( contract_meta_object.exists(), invalid_contract, "contract metadata does not exist" );
      auto contract_meta = util::converter::to< contract_metadata_object >( contract_meta_object.value() );
      KOINOS_ASSERT( contract_meta.system(), invalid_contract, "contract is not a system contract" );
      KOINOS_ASSERT( ( o.call_id() != chain::system_call_id::call_contract ), forbidden_override, "cannot override call_contract" );

      //LOG(info) << "Overriding system call " << o.call_id() << " with contract " << util::to_base58( o.target().system_call_bundle().contract_id() ) << " at entry point " << o.target().system_call_bundle().entry_point();
   }
   else
   {
      KOINOS_ASSERT( thunk_dispatcher::instance().thunk_exists( o.target().thunk_id() ), thunk_not_found, "thunk ${tid} does not exist", ("tid", o.target().thunk_id()) );
      LOG(info) << "Overriding system call " << o.call_id() << " with thunk " << o.target().thunk_id();
   }

   // Place the override in the database
   system_call::put_object( context, state::space::system_call_dispatch(), util::converter::as< std::string >( std::underlying_type_t< koinos::chain::system_call_id >( o.call_id() ) ), util::converter::as< std::string >( o.target() ) );

   // Emit an event
   set_system_call_event event;
   event.set_call_id( o.call_id() );
   event.mutable_target()->CopyFrom( o.target() );
   std::string str;
   event.SerializeToString( &str );
   system_call::event( context, "set_system_call_event", str, {} );
}

THUNK_DEFINE( void, apply_set_system_contract_operation, ((const protocol::set_system_contract_operation&) o) )
{
   KOINOS_ASSERT( context.get_caller_privilege() == privilege::kernel_mode, insufficient_privileges, "calling privileged thunk from non-privileged code" );
   KOINOS_ASSERT( !context.read_only(), read_only_context, "unable to perform action while context is read only" );

   system_call::require_system_authority( context, set_system_contract );

   auto contract_object = system_call::get_object( context, state::space::contract_bytecode(), o.contract_id() );
   KOINOS_ASSERT( contract_object.exists(), invalid_contract, "contract does not exist" );
   auto contract_meta_object = system_call::get_object( context, state::space::contract_metadata(), o.contract_id() );
   KOINOS_ASSERT( contract_meta_object.exists(), invalid_contract, "contract does not exist" );
   auto contract_meta = util::converter::to< contract_metadata_object >( contract_meta_object.value() );
   KOINOS_ASSERT( contract_meta.hash().size(), invalid_contract, "contract hash does not exist" );

   contract_meta.set_system( o.system_contract() );
   system_call::put_object( context, state::space::contract_metadata(), o.contract_id(), util::converter::as< std::string >( contract_meta ) );

   // Emit an event
   set_system_contract_event event;
   event.set_contract_id( o.contract_id() );
   event.set_system_contract( o.system_contract() );
   std::string str;
   event.SerializeToString( &str );
   system_call::event( context, "set_system_contract_event", str, {} );
}

THUNK_DEFINE_VOID( void, post_block_callback ) { }

THUNK_DEFINE_VOID( void, post_transaction_callback ) { }

THUNK_DEFINE( put_object_result, put_object, ((const object_space&) space, (const std::string&) key, (const std::string&) obj) )
{
   KOINOS_ASSERT( !context.read_only(), read_only_context, "cannot put object during read only call" );

   state::assert_permissions( context, space );

   auto state = context.get_state_node();
   KOINOS_ASSERT( state, state_node_not_found, "current state node does not exist" );
   auto val = util::converter::as< state_db::object_value >( obj );

   auto bytes_used = state->put_object( space, key, &val );

   if ( bytes_used > 0 )
      context.resource_meter().use_disk_storage( bytes_used );

   put_object_result ret;
   ret.set_value( bytes_used );

   return ret;
}

THUNK_DEFINE( void, remove_object, ((const object_space&) space, (const std::string&) key) )
{
   KOINOS_ASSERT( !context.read_only(), read_only_context, "cannot remove object during read only call" );

   state::assert_permissions( context, space );

   auto state = context.get_state_node();
   KOINOS_ASSERT( state, state_node_not_found, "current state node does not exist" );

   state->remove_object( space, key );
}

THUNK_DEFINE( get_object_result, get_object, ((const object_space&) space, (const std::string&) key) )
{
   state::assert_permissions( context, space );

   abstract_state_node_ptr state = context.get_state_node();

   KOINOS_ASSERT( state, state_node_not_found, "current state node does not exist" );

   const auto result = state->get_object( space, key );

   get_object_result ret;

   if( result )
   {
      ret.mutable_value()->set_exists( true );
      ret.mutable_value()->set_value( result->data(), result->size() );
   }

   return ret;
}

THUNK_DEFINE( get_next_object_result, get_next_object, ((const object_space&) space, (const std::string&) key) )
{
   state::assert_permissions( context, space );

   abstract_state_node_ptr state = context.get_state_node();
   KOINOS_ASSERT( state, state_node_not_found, "current state node does not exist" );

   const auto [result, next_key] = state->get_next_object( space, key );

   get_next_object_result ret;

   if( result )
   {
      ret.mutable_value()->set_exists( true );
      ret.mutable_value()->set_value( result->data(), result->size() );
      ret.mutable_value()->set_key( next_key );
   }

   return ret;
}

THUNK_DEFINE( get_prev_object_result, get_prev_object, ((const object_space&) space, (const std::string&) key) )
{
   state::assert_permissions( context, space );

   abstract_state_node_ptr state = context.get_state_node();
   KOINOS_ASSERT( state, state_node_not_found, "current state node does not exist" );

   const auto [result, next_key] = state->get_prev_object( space, key );

   get_prev_object_result ret;

   if( result )
   {
      ret.mutable_value()->set_exists( true );
      ret.mutable_value()->set_value( result->data(), result->size() );
      ret.mutable_value()->set_key( next_key );
   }

   return ret;
}

THUNK_DEFINE( call_contract_result, call_contract, ((const std::string&) contract_id, (uint32_t) entry_point, (const std::string&) args) )
{
   // We need to be in kernel mode to read the contract data
   auto contract_object = system_call::get_object( context, state::space::contract_bytecode(), contract_id );
   KOINOS_ASSERT( contract_object.exists(), invalid_contract, "contract does not exist" );
   auto contract_meta_object = system_call::get_object( context, state::space::contract_metadata(), contract_id );
   KOINOS_ASSERT( contract_meta_object.exists(), invalid_contract, "contract metadata does not exist" );
   auto contract_meta = util::converter::to< contract_metadata_object >( contract_meta_object.value() );
   KOINOS_ASSERT( contract_meta.hash().size(), invalid_contract, "contract hash does not exist" );

   context.push_frame( stack_frame{
      .contract_id = contract_id,
      .call_privilege = contract_meta.system() ? privilege::kernel_mode : privilege::user_mode,
      .call_args = args,
      .entry_point = entry_point
   } );

   try
   {
      chain::host_api hapi( context );
      context.get_backend()->run( hapi, contract_object.value(), contract_meta.hash() );
   }
   catch( const exit_success& ) {}
   catch( ... ) {
      context.pop_frame();
      throw;
   }

   call_contract_result ret;
   ret.set_value( context.pop_frame().call_return );
   return ret;
}

THUNK_DEFINE_VOID( get_entry_point_result, get_entry_point )
{
   get_entry_point_result ret;
   ret.set_value( context.get_contract_entry_point() );
   return ret;
}

THUNK_DEFINE_VOID( get_contract_arguments_result, get_contract_arguments )
{
   get_contract_arguments_result ret;
   ret.set_value( context.get_contract_call_args() );
   return ret;
}

THUNK_DEFINE( void, set_contract_result, ((const std::string&) ret) )
{
   context.set_contract_return( ret );
}

THUNK_DEFINE_VOID( get_head_info_result, get_head_info )
{
   auto head = context.get_state_node();

   chain::head_info hi;
   hi.mutable_head_topology()->set_id( util::converter::as< std::string >( head->id() ) );
   hi.mutable_head_topology()->set_previous( util::converter::as< std::string >( head->parent_id() ) );
   hi.mutable_head_topology()->set_height( head->revision() );
   hi.set_last_irreversible_block( system_call::get_last_irreversible_block( context ) );

   if ( const auto* block = context.get_block(); block != nullptr )
   {
      hi.set_head_block_time( block->header().timestamp() );
   }
   else
   {
      auto head_info_object = system_call::get_object( context, state::space::metadata(), state::key::head_block_time );
      uint64_t time = head_info_object.exists() ? util::converter::to< uint64_t >( head_info_object.value() ) : 0;
      hi.set_head_block_time( time );
   }

   get_head_info_result ret;
   auto* v = ret.mutable_value();
   *v = hi;
   return ret;
}

THUNK_DEFINE( hash_result, hash, ((uint64_t) id, (const std::string&) obj, (uint64_t) size) )
{
   auto multicodec = static_cast< crypto::multicodec >( id );
   validate_hash_code( multicodec );

   auto hash = crypto::hash( multicodec, obj, crypto::digest_size( size ) );

   hash_result ret;
   ret.set_value( util::converter::as< std::string >( hash ) );
   return ret;
}

THUNK_DEFINE( recover_public_key_result, recover_public_key, ((dsa) type, (const std::string&) signature_data, (const std::string&) digest) )
{
   KOINOS_ASSERT( type == ecdsa_secp256k1, invalid_dsa, "unexpected dsa" );

   KOINOS_ASSERT( signature_data.size() == 65, invalid_signature, "unexpected signature length" );
   crypto::recoverable_signature signature = util::converter::as< crypto::recoverable_signature >( signature_data );

   KOINOS_ASSERT( crypto::public_key::is_canonical( signature ), invalid_signature, "signature must be canonical" );

   auto pub_key = crypto::public_key::recover( signature, util::converter::to< crypto::multihash >( digest ) );
   KOINOS_ASSERT( pub_key.valid(), invalid_signature, "public key is invalid" );

   recover_public_key_result ret;
   ret.set_value( util::converter::as< std::string >( pub_key ) );
   return ret;
}

THUNK_DEFINE_VOID( get_last_irreversible_block_result, get_last_irreversible_block )
{
   auto head = context.get_state_node();

   get_last_irreversible_block_result ret;
   ret.set_value( head->revision() > default_irreversible_threshold ? head->revision() - default_irreversible_threshold : 0 );

   return ret;
}

THUNK_DEFINE_VOID( get_caller_result, get_caller )
{
   get_caller_result ret;
   auto frame0 = context.pop_frame(); // get_caller frame
   auto frame1 = context.pop_frame(); // contract frame
   std::exception_ptr e;

   try
   {
      ret.mutable_value()->set_caller( context.get_caller() );
      ret.mutable_value()->set_caller_privilege( context.get_caller_privilege() );
   }
   catch( ... )
   {
      e = std::current_exception();
   }

   context.push_frame( std::move( frame1 ) );
   context.push_frame( std::move( frame0 ) );

   if ( e )
   {
      std::rethrow_exception( e );
   }
   return ret;
}

THUNK_DEFINE( void, require_authority, ((authorization_type) type, (const std::string&) account) )
{
   KOINOS_ASSERT( !context.read_only(), read_only_context, "unable to perform action while context is read only" );

   auto account_contract_meta_object = system_call::get_object( context, state::space::contract_metadata(), account );
   bool authorize_override = false;

   if ( account_contract_meta_object.exists() )
   {
      auto account_contract_meta = util::converter::to< contract_metadata_object >( account_contract_meta_object.value() );

      switch ( type )
      {
         case contract_call:
            authorize_override = account_contract_meta.authorizes_call_contract();
            break;
         case rc_use:
            authorize_override = account_contract_meta.authorizes_use_rc();
            break;
         case contract_upload:
            authorize_override = account_contract_meta.authorizes_upload_contract();
            break;
         default:;
      }
   }

   bool authorized = false;

   if ( authorize_override )
   {
      authorize_arguments args;
      args.set_type( type );

      if ( type == contract_call )
      {
         args.mutable_call()->set_contract_id( context.get_caller() );
         args.mutable_call()->set_entry_point( context.get_caller_entry_point() );
      }

      authorized = util::converter::to< authorize_result >( system_call::call_contract( context, account, authorize_entrypoint, util::converter::as< std::string >( args ) ) ).value();
   }
   else
   {
      const auto& trx = context.get_transaction();

      for ( const auto& sig : trx.signatures() )
      {
         auto signer_address = util::converter::to< crypto::public_key >( system_call::recover_public_key( context, ecdsa_secp256k1, sig, trx.id() ) ).to_address_bytes();
         authorized = ( signer_address == account );
         if ( authorized )
            break;
      }
   }

   KOINOS_ASSERT( authorized, authorization_failed, "account ${account} has not authorized action", ("account", util::to_base58( account )) );
}

THUNK_DEFINE_VOID( get_contract_id_result, get_contract_id )
{
   get_contract_id_result ret;
   ret.set_value( context.get_contract_id() );
   return ret;
}

THUNK_DEFINE( get_account_nonce_result, get_account_nonce, ((const std::string&) account ) )
{
   auto obj = system_call::get_object( context, state::space::transaction_nonce(), account );

   get_account_nonce_result ret;

   if ( obj.exists() )
   {
      ret.set_value( obj.value() );
   }
   else
   {
      value_type nonce_value;
      nonce_value.set_uint64_value( 0 );
      ret.set_value( util::converter::as< std::string >( nonce_value ) );
   }

   return ret;
}

THUNK_DEFINE( verify_account_nonce_result, verify_account_nonce, ((const std::string&) account, (const std::string&) nonce) )
{
   auto nonce_value = util::converter::to< value_type >( nonce );
   KOINOS_ASSERT(
      nonce_value.has_uint64_value(),
      invalid_nonce,
      "nonce did not contain uint64 value - nonce: ${n}, account: ${a}",
      ("n", util::to_hex( nonce ))("a", util::to_base58( account ))
   );

   auto current_nonce_value = util::converter::to< value_type >( system_call::get_account_nonce( context, account ) );
   KOINOS_ASSERT(
      current_nonce_value.has_uint64_value(),
      unexpected_state,
      "current nonce did not contain uint64 value - nonce: ${n}, account: ${a}",
      ("n", util::to_hex( current_nonce_value ))("a", util::to_base58( account ))
   );

   verify_account_nonce_result res;
   res.set_value( nonce_value.uint64_value() > current_nonce_value.uint64_value() );
   return res;
}

THUNK_DEFINE( void, set_account_nonce, ((const std::string&) account, (const std::string&) nonce) )
{
   auto nonce_value = util::converter::to< value_type >( nonce );
   KOINOS_ASSERT(
      nonce_value.has_uint64_value(),
      invalid_nonce,
      "set nonce did not contain uint64 value - nonce: ${n}, account: ${a}",
      ("n", util::to_hex( nonce ))("a", util::to_base58( account )) );

   system_call::put_object( context, state::space::transaction_nonce(), account, nonce );
}

THUNK_DEFINE( get_account_rc_result, get_account_rc, ((const std::string&) account) )
{
   auto obj = system_call::get_object( context, state::space::metadata(), state::key::max_account_resources );
   KOINOS_ASSERT( obj.exists(), unexpected_state, "max_account_resources does not exist" );

   get_account_rc_result ret;
   ret.set_value( util::converter::to< chain::max_account_resources >( obj.value() ).value() );

   return ret;
}

THUNK_DEFINE( consume_account_rc_result, consume_account_rc, ((const std::string&) account, (uint64_t) rc) )
{
   consume_account_rc_result ret;
   ret.set_value( true );
   return ret;
}

THUNK_DEFINE_VOID( get_resource_limits_result, get_resource_limits )
{
   resource_limit_data rd;

   auto obj = system_call::get_object( context, state::space::metadata(), state::key::resource_limit_data );
   KOINOS_ASSERT( obj.exists(), unexpected_state, "resource_limit_data does not exist" );

   get_resource_limits_result ret;
   *ret.mutable_value() = util::converter::to< resource_limit_data >( obj.value() );
   return ret;
}

THUNK_DEFINE( consume_block_resources_result, consume_block_resources, ((uint64_t) disk, (uint64_t) network, (uint64_t) compute) )
{
   consume_block_resources_result ret;
   ret.set_value( true );
   return ret;
}

THUNK_DEFINE( void, event, ((const std::string&) name, (const std::string&) data, (const std::vector< std::string >&) impacted) )
{
   const auto& caller = context.get_caller();

   context.resource_meter().use_network_bandwidth( caller.size() + name.size() + data.size() );

   protocol::event_data ev;
   ev.set_source( caller );
   ev.set_name( name );
   ev.set_data( data );

   for ( auto& imp : impacted )
      *ev.add_impacted() = imp;

   context.chronicler().push_event( std::move( ev ) );
}

THUNK_DEFINE_VOID( get_transaction_result, get_transaction )
{
   get_transaction_result ret;

   *ret.mutable_value() = context.get_transaction();

   return ret;
}

THUNK_DEFINE( get_transaction_field_result, get_transaction_field, ((const std::string&) field) )
{
   get_transaction_field_result ret;

   *ret.mutable_value() = get_nested_field_value( context, context.get_transaction(), field );

   return ret;
}

THUNK_DEFINE_VOID( get_block_result, get_block )
{
   get_block_result ret;

   const auto* block = context.get_block();
   KOINOS_ASSERT( block != nullptr, unexpected_access, "block does not exist" );

   *ret.mutable_value() = *block;

   return ret;
}

THUNK_DEFINE( get_block_field_result, get_block_field, ((const std::string&) field) )
{
   get_block_field_result ret;

   const auto* block = context.get_block();
   KOINOS_ASSERT( block != nullptr, unexpected_access, "block does not exist" );

   *ret.mutable_value() = get_nested_field_value( context, *block, field );

   return ret;
}

THUNK_DEFINE( void, require_system_authority, ((system_authorization_type) type) )
{
   auto genesis_addr = system_call::get_object( context, state::space::metadata(), state::key::genesis_key ).value();

   const auto& trx = context.get_transaction();

   bool authorized = false;

   for ( const auto& sig : trx.signatures() )
   {
      auto addr = util::converter::to< crypto::public_key >( system_call::recover_public_key( context, ecdsa_secp256k1, sig, trx.id() ) ).to_address_bytes();
      authorized = ( addr == genesis_addr );
      if ( authorized )
         break;
   }

   KOINOS_ASSERT( authorized, authorization_failed, "system authority required" );
}

THUNK_DEFINE( verify_signature_result, verify_signature, ((dsa) type, (const std::string&) public_key, (const std::string&) signature, (const std::string&) digest) )
{
   KOINOS_ASSERT( type == ecdsa_secp256k1, invalid_dsa, "unexpected dsa" );

   verify_signature_result ret;
   ret.set_value( system_call::recover_public_key( context, type, signature, digest ) == public_key );
   return ret;
}

THUNK_DEFINE_END();

} // koinos::chain
