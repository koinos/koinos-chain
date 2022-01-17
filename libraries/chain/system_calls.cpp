#include <algorithm>
#include <string>

#include <google/protobuf/message.h>
#include <google/protobuf/util/message_differencer.h>

#include <koinos/bigint.hpp>
#include <koinos/chain/execution_context.hpp>
#include <koinos/chain/constants.hpp>
#include <koinos/chain/host_api.hpp>
#include <koinos/chain/state.hpp>
#include <koinos/chain/system_calls.hpp>
#include <koinos/chain/thunk_dispatcher.hpp>
#include <koinos/chain/session.hpp>
#include <koinos/crypto/multihash.hpp>
#include <koinos/log.hpp>
#include <koinos/util/base58.hpp>
#include <koinos/util/conversion.hpp>
#include <koinos/util/hex.hpp>

using namespace std::string_literals;

namespace koinos::chain {

void register_thunks( thunk_dispatcher& td )
{
   THUNK_REGISTER( td,
      (prints)
      (exit_contract)

      (process_block_signature)
      (verify_merkle_root)

      (apply_block)
      (apply_transaction)
      (apply_upload_contract_operation)
      (apply_call_contract_operation)
      (apply_set_system_call_operation)
      (apply_set_system_contract_operation)

      (put_object)
      (remove_object)
      (get_object)
      (get_next_object)
      (get_prev_object)

      (call_contract)

      (get_entry_point)
      (get_contract_arguments_size)
      (get_contract_arguments)
      (set_contract_result)

      (get_head_info)
      (hash)
      (recover_public_key)

      (get_transaction_payer)
      (get_transaction_rc_limit)

      (get_last_irreversible_block)

      (get_caller)
      (get_transaction_signature)
      (require_authority)

      (get_contract_id)

      (get_account_nonce)

      (get_account_rc)
      (consume_account_rc)
      (get_resource_limits)
      (consume_block_resources)

      (event)
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

THUNK_DEFINE_BEGIN();

THUNK_DEFINE( void, prints, ((const std::string&) str) )
{
   context.resource_meter().use_compute_bandwidth( compute_load::light );
   context.console_append( str );
}

THUNK_DEFINE( void, exit_contract, ((uint32_t) exit_code) )
{
   switch( exit_code )
   {
      case KOINOS_EXIT_SUCCESS:
          KOINOS_THROW( exit_success, "" );
      case KOINOS_EXIT_FAILURE:
          KOINOS_THROW( exit_failure, "" );
      default:
          KOINOS_THROW( unknown_exit_code, "contract specified unknown exit code" );
   }
}

THUNK_DEFINE( process_block_signature_result, process_block_signature, ((const std::string&) id, (const std::string&) active_data, (const std::string&) signature_data) )
{
   context.resource_meter().use_compute_bandwidth( compute_load::light );

   std::string genesis_addr;
   crypto::recoverable_signature sig;
   std::memcpy( sig.data(), signature_data.data(), std::min( sig.size(), signature_data.size() ) );
   crypto::multihash block_id = util::converter::to< crypto::multihash >( id );

   with_privilege(
      context,
      privilege::kernel_mode,
      [&]() {
         genesis_addr = system_call::get_object( context, state::space::metadata(), state::key::genesis_key ).value();
      }
   );

   process_block_signature_result ret;
   ret.set_value( genesis_addr == crypto::public_key::recover( sig, block_id ).to_address_bytes() );
   return ret;
}

THUNK_DEFINE( verify_merkle_root_result, verify_merkle_root, ((const std::string&) root, (const std::vector< std::string >&) hashes) )
{
   uint128_t compute_bandwidth = uint128_t( compute_load::light ) * hashes.size();
   compute_bandwidth = compute_bandwidth > std::numeric_limits< uint64_t >::max() ? std::numeric_limits< uint64_t >::max() : compute_bandwidth;
   context.resource_meter().use_compute_bandwidth( compute_bandwidth.convert_to< uint64_t >() );

   std::vector< crypto::multihash > leaves;

   leaves.resize( hashes.size() );
   std::transform( std::begin( hashes ), std::end( hashes ), std::begin( leaves ), []( const std::string& s ) { return util::converter::to< crypto::multihash >( s ); } );

   auto root_hash = util::converter::to< crypto::multihash >( root );
   auto mtree = crypto::merkle_tree( root_hash.code(), leaves );

   auto merkle_root = mtree.root()->hash();

   verify_merkle_root_result ret;
   ret.set_value( merkle_root == root_hash );
   return ret;
}

THUNK_DEFINE( void, apply_block, ((const protocol::block&) block) )
{
   KOINOS_ASSERT( context.get_privilege() == privilege::kernel_mode, insufficient_privileges, "calling privileged thunk from non-privileged code" );
   KOINOS_ASSERT( context.intent() == intent::block_application, unexpected_intent, "expected block application intent while applying block" );

   block_guard guard( context, block );

   context.receipt() = protocol::block_receipt();

   context.resource_meter().set_resource_limit_data( system_call::get_resource_limits( context ) );

   const crypto::multihash tx_root = util::converter::to< crypto::multihash >( block.header().transaction_merkle_root() );
   size_t tx_count = block.transactions_size();

   // Check transaction Merkle root
   std::vector< std::string > hashes;
   hashes.reserve( tx_count * 2 );

   std::size_t transactions_bytes_size = 0;
   for ( const auto& trx : block.transactions() )
   {
      transactions_bytes_size += trx.ByteSizeLong();
      hashes.emplace_back( util::converter::as< std::string >( crypto::hash( tx_root.code(), trx.header() ) ) );
      hashes.emplace_back( util::converter::as< std::string >( crypto::hash( tx_root.code(), trx.signature() ) ) );
   }

   context.resource_meter().use_network_bandwidth( block.ByteSizeLong() - transactions_bytes_size );

   KOINOS_ASSERT( system_call::verify_merkle_root( context, block.header().transaction_merkle_root(), hashes ), transaction_root_mismatch, "transaction merkle root does not match" );

   crypto::multihash block_hash = crypto::hash( tx_root.code(), block.header() );
   KOINOS_ASSERT(
      system_call::process_block_signature(
         context,
         util::converter::as< std::string >( block_hash ),
         util::converter::as< std::string >( block.header() ),
         block.signature()
      ),
      invalid_block_signature,
      "block signature does not match"
   );

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

   for ( const auto& [ within_session, event ] : context.event_recorder().events() )
      if ( !within_session )
         *receipt.add_events() = event;
}

THUNK_DEFINE( void, apply_transaction, ((const protocol::transaction&) trx) )
{
   KOINOS_ASSERT( context.get_privilege() == privilege::kernel_mode, insufficient_privileges, "calling privileged thunk from non-privileged code" );
   KOINOS_ASSERT( !context.read_only(), read_only_context, "unable to perform action while context is read only" );

   transaction_guard guard( context, trx );

   const crypto::multihash op_root = util::converter::to< crypto::multihash >( trx.header().operation_merkle_root() );
   size_t op_count = trx.operations_size();

   // Check operation merkle root
   std::vector< std::string > hashes;
   hashes.reserve( op_count );

   for ( const auto& op : trx.operations() )
      hashes.emplace_back( util::converter::as< std::string >( crypto::hash( op_root.code(), op ) ) );

   KOINOS_ASSERT( system_call::verify_merkle_root( context, trx.header().operation_merkle_root(), hashes ), operation_root_mismatch, "operation merkle root does not match" );

   protocol::transaction_receipt receipt;

   std::string payer = system_call::get_transaction_payer( context, trx );

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

   auto account_nonce = system_call::get_account_nonce( context, payer );
   KOINOS_ASSERT(
      account_nonce == trx.header().nonce(),
      chain::chain_exception,
      "mismatching transaction nonce - trx nonce: ${d}, expected: ${e}", ("d", trx.header().nonce())("e", account_nonce)
   );

   system_call::require_authority( context, payer );

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
      throw e;
   }
   catch ( const std::exception& e )
   {
      LOG(info) << "Transaction " << util::to_hex( trx.id() ) << " reverted with: " << e.what();
      transaction_reverted tr( e.what() );
      reverted_exception_ptr = std::make_exception_ptr( tr );
      receipt.set_reverted( true );
   }

   context.set_state_node( block_node );

   // Next nonce should be the current nonce + 1
   system_call::put_object( context, state::space::transaction_nonce(), payer, util::converter::as< std::string >( trx.header().nonce() + 1 ) );
   auto used_rc = payer_session->used_rc();

   receipt.set_rc_used( used_rc );
   receipt.set_disk_storage_used( context.resource_meter().disk_storage_used() - start_disk_used );
   receipt.set_network_bandwidth_used( context.resource_meter().network_bandwidth_used() - start_network_used );
   receipt.set_compute_bandwidth_used( context.resource_meter().compute_bandwidth_used() - start_compute_used );

   for ( auto& e : payer_session->events() )
      *receipt.add_events() = e;

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

   LOG(debug) << "(apply_transaction) transaction " << trx.id();
}

THUNK_DEFINE( void, apply_upload_contract_operation, ((const protocol::upload_contract_operation&) o) )
{
   KOINOS_ASSERT( context.get_privilege() == privilege::kernel_mode, insufficient_privileges, "calling privileged thunk from non-privileged code" );
   KOINOS_ASSERT( !context.read_only(), read_only_context, "unable to perform action while context is read only" );

   context.resource_meter().use_compute_bandwidth( compute_load::medium );

   auto tx_id       = crypto::hash( crypto::multicodec::sha2_256, context.get_transaction().header() );
   auto sig_account = system_call::recover_public_key( context, system_call::get_transaction_signature( context ), util::converter::as< std::string >( tx_id ) );

   KOINOS_ASSERT(
      sig_account == o.contract_id(),
      invalid_signature,
      "signature does not match: ${contract_id} != ${signer_hash}", ("contract_id", o.contract_id())("signer_hash", sig_account)
   );

   contract_metadata_object contract_meta;
   *contract_meta.mutable_hash() = util::converter::as< std::string >( crypto::hash( crypto::multicodec::sha2_256, o.bytecode() ) );

   system_call::put_object( context, state::space::contract_bytecode(), o.contract_id(), o.bytecode() );
   system_call::put_object( context, state::space::contract_metadata(), o.contract_id(), util::converter::as< std::string >( contract_meta ) );
}

THUNK_DEFINE( void, apply_call_contract_operation, ((const protocol::call_contract_operation&) o) )
{
   KOINOS_ASSERT( context.get_privilege() == privilege::kernel_mode, insufficient_privileges, "calling privileged thunk from non-privileged code" );
   KOINOS_ASSERT( !context.read_only(), read_only_context, "unable to perform action while context is read only" );

   context.resource_meter().use_compute_bandwidth( compute_load::light );

   // Drop to user mode
   with_stack_frame(
      context,
      stack_frame {
         .contract_id = "call_contract_operation"s,
         .system = false,
         .call_privilege = privilege::user_mode,
      },
      [&]() {
         system_call::call_contract( context, o.contract_id(), o.entry_point(), o.args() );
      }
   );
}

THUNK_DEFINE( void, apply_set_system_call_operation, ((const protocol::set_system_call_operation&) o) )
{
   KOINOS_ASSERT( context.get_privilege() == privilege::kernel_mode, insufficient_privileges, "calling privileged thunk from non-privileged code" );
   KOINOS_ASSERT( !context.read_only(), read_only_context, "unable to perform action while context is read only" );

   context.resource_meter().use_compute_bandwidth( compute_load::heavy );

   auto genesis_addr = system_call::get_object( context, state::space::metadata(), state::key::genesis_key ).value();

   const auto& tx = context.get_transaction();
   crypto::recoverable_signature sig;
   std::memcpy( sig.data(), tx.signature().data(), std::min( sig.size(), tx.signature().size() ) );

   KOINOS_ASSERT(
      genesis_addr == crypto::public_key::recover( sig, util::converter::to< crypto::multihash >( tx.id() ) ).to_address_bytes(),
      insufficient_privileges,
      "transaction does not have the required authority to override system calls"
   );

   if ( o.target().has_system_call_bundle() )
   {
      auto contract_object = system_call::get_object( context, state::space::contract_bytecode(), o.target().system_call_bundle().contract_id() );
      KOINOS_ASSERT( contract_object.exists(), invalid_contract, "contract does not exist" );
      auto contract_meta_object = system_call::get_object( context, state::space::contract_metadata(), o.target().system_call_bundle().contract_id() );
      KOINOS_ASSERT( contract_meta_object.exists(), invalid_contract, "contract metadata does not exist" );
      auto contract_meta = util::converter::to< contract_metadata_object >( contract_meta_object.value() );
      KOINOS_ASSERT( contract_meta.system(), invalid_contract, "contract is not a system contract" );
      KOINOS_ASSERT( ( o.call_id() != protocol::system_call_id::call_contract ), forbidden_override, "cannot override call_contract" );

      LOG(info) << "Overriding system call " << o.call_id() << " with contract " << util::to_base58( o.target().system_call_bundle().contract_id() ) << " at entry point " << o.target().system_call_bundle().entry_point();
   }
   else if ( o.target().thunk_id() )
   {
      KOINOS_ASSERT( thunk_dispatcher::instance().thunk_exists( o.target().thunk_id() ), thunk_not_found, "thunk ${tid} does not exist", ("tid", o.target().thunk_id()) );
      LOG(info) << "Overriding system call " << o.call_id() << " with thunk " << o.target().thunk_id();
   }
   else
   {
      KOINOS_THROW( unknown_system_call, "set_system_call invoked with unknown override" );
   }

   // Place the override in the database
   system_call::put_object( context, state::space::system_call_dispatch(), util::converter::as< std::string >( std::underlying_type_t< koinos::protocol::system_call_id >( o.call_id() ) ), util::converter::as< std::string >( o.target() ) );
}

THUNK_DEFINE( void, apply_set_system_contract_operation, ((const protocol::set_system_contract_operation&) o) )
{
   KOINOS_ASSERT( context.get_privilege() == privilege::kernel_mode, insufficient_privileges, "calling privileged thunk from non-privileged code" );
   KOINOS_ASSERT( !context.read_only(), read_only_context, "unable to perform action while context is read only" );

   context.resource_meter().use_compute_bandwidth( compute_load::heavy );

   auto genesis_addr = system_call::get_object( context, state::space::metadata(), state::key::genesis_key ).value();

   const auto& tx = context.get_transaction();
   crypto::recoverable_signature sig;
   std::memcpy( sig.data(), tx.signature().data(), std::min( sig.size(), tx.signature().size() ) );

   KOINOS_ASSERT(
      genesis_addr == crypto::public_key::recover( sig, util::converter::to< crypto::multihash >( tx.id() ) ).to_address_bytes(),
      insufficient_privileges,
      "transaction does not have the required authority to override system calls"
   );

   auto contract_object = system_call::get_object( context, state::space::contract_bytecode(), o.contract_id() );
   KOINOS_ASSERT( contract_object.exists(), invalid_contract, "contract does not exist" );
   auto contract_meta_object = system_call::get_object( context, state::space::contract_metadata(), o.contract_id() );
   KOINOS_ASSERT( contract_meta_object.exists(), invalid_contract, "contract does not exist" );
   auto contract_meta = util::converter::to< contract_metadata_object >( contract_meta_object.value() );
   KOINOS_ASSERT( contract_meta.hash().size(), invalid_contract, "contract hash does not exist" );

   contract_meta.set_system( o.system_contract() );
   system_call::put_object( context, state::space::contract_metadata(), o.contract_id(), util::converter::as< std::string >( contract_meta ) );
}

THUNK_DEFINE( put_object_result, put_object, ((const object_space&) space, (const std::string&) key, (const std::string&) obj) )
{
   KOINOS_ASSERT( !context.read_only(), read_only_context, "cannot put object during read only call" );

   context.resource_meter().use_compute_bandwidth( compute_load::medium );

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

   context.resource_meter().use_compute_bandwidth( compute_load::medium );

   state::assert_permissions( context, space );

   auto state = context.get_state_node();
   KOINOS_ASSERT( state, state_node_not_found, "current state node does not exist" );

   state->remove_object( space, key );
}

THUNK_DEFINE( get_object_result, get_object, ((const object_space&) space, (const std::string&) key) )
{
   context.resource_meter().use_compute_bandwidth( compute_load::medium );

   state::assert_permissions( context, space );

   abstract_state_node_ptr state = google::protobuf::util::MessageDifferencer::Equals( space, state::space::system_call_dispatch() ) ? context.get_parent_node() : context.get_state_node();

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
   context.resource_meter().use_compute_bandwidth( compute_load::medium );

   state::assert_permissions( context, space );

   abstract_state_node_ptr state = google::protobuf::util::MessageDifferencer::Equals( space, state::space::system_call_dispatch() ) ? context.get_parent_node() : context.get_state_node();
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
   context.resource_meter().use_compute_bandwidth( compute_load::medium );

   state::assert_permissions( context, space );

   abstract_state_node_ptr state = google::protobuf::util::MessageDifferencer::Equals( space, state::space::system_call_dispatch() ) ? context.get_parent_node() : context.get_state_node();
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
   context.resource_meter().use_compute_bandwidth( compute_load::medium );

   // We need to be in kernel mode to read the contract data
   database_object contract_object;
   contract_metadata_object contract_meta;
   with_privilege(
      context,
      privilege::kernel_mode,
      [&]()
      {
         contract_object = system_call::get_object( context, state::space::contract_bytecode(), contract_id );
         KOINOS_ASSERT( contract_object.exists(), invalid_contract, "contract does not exist" );
         auto contract_meta_object = system_call::get_object( context, state::space::contract_metadata(), contract_id );
         KOINOS_ASSERT( contract_meta_object.exists(), invalid_contract, "contract metadata does not exist" );
         contract_meta = util::converter::to< contract_metadata_object >( contract_meta_object.value() );
         KOINOS_ASSERT( contract_meta.hash().size(), invalid_contract, "contract hash does not exist" );
      }
   );

   context.push_frame( stack_frame{
      .contract_id = contract_id,
      .system = contract_meta.system(),
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
   context.resource_meter().use_compute_bandwidth( compute_load::light );

   get_entry_point_result ret;
   ret.set_value( context.get_contract_entry_point() );
   return ret;
}

THUNK_DEFINE_VOID( get_contract_arguments_size_result, get_contract_arguments_size )
{
   context.resource_meter().use_compute_bandwidth( compute_load::light );

   get_contract_arguments_size_result ret;
   ret.set_value( (uint32_t)context.get_contract_call_args().size() );
   return ret;
}

THUNK_DEFINE_VOID( get_contract_arguments_result, get_contract_arguments )
{
   context.resource_meter().use_compute_bandwidth( compute_load::light );

   get_contract_arguments_result ret;
   ret.set_value( context.get_contract_call_args() );
   return ret;
}

THUNK_DEFINE( void, set_contract_result, ((const std::string&) ret) )
{
   context.resource_meter().use_compute_bandwidth( compute_load::light );

   context.set_contract_return( ret );
}

THUNK_DEFINE_VOID( get_head_info_result, get_head_info )
{
   context.resource_meter().use_compute_bandwidth( compute_load::medium );

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
      with_privilege(
         context,
         privilege::kernel_mode,
         [&]() {
            auto head_info_object = system_call::get_object( context, state::space::metadata(), state::key::head_block_time );
            uint64_t time = head_info_object.exists() ? util::converter::to< uint64_t >( head_info_object.value() ) : 0;
            hi.set_head_block_time( time );
         }
      );
   }

   get_head_info_result ret;
   auto* v = ret.mutable_value();
   *v = hi;
   return ret;
}

THUNK_DEFINE( hash_result, hash, ((uint64_t) id, (const std::string&) obj, (uint64_t) size) )
{
   context.resource_meter().use_compute_bandwidth( compute_load::light );

   auto multicodec = static_cast< crypto::multicodec >( id );
   switch ( multicodec )
   {
      case crypto::multicodec::sha1:
         [[fallthrough]];
      case crypto::multicodec::sha2_256:
         [[fallthrough]];
      case crypto::multicodec::sha2_512:
         [[fallthrough]];
      case crypto::multicodec::ripemd_160:
         break;
      default:
         KOINOS_THROW( unknown_hash_code, "unknown_hash_code" );
   }
   auto hash = crypto::hash( multicodec, obj, crypto::digest_size( size ) );

   hash_result ret;
   ret.set_value( util::converter::as< std::string >( hash ) );
   return ret;
}

THUNK_DEFINE( recover_public_key_result, recover_public_key, ((const std::string&) signature_data, (const std::string&) digest) )
{
   context.resource_meter().use_compute_bandwidth( compute_load::light );

   KOINOS_ASSERT( signature_data.size() == 65, invalid_signature, "unexpected signature length" );
   crypto::recoverable_signature signature = util::converter::as< crypto::recoverable_signature >( signature_data );

   KOINOS_ASSERT( crypto::public_key::is_canonical( signature ), invalid_signature, "signature must be canonical" );

   auto pub_key = crypto::public_key::recover( signature, util::converter::to< crypto::multihash >( digest ) );
   KOINOS_ASSERT( pub_key.valid(), invalid_signature, "public key is invalid" );

   auto address = pub_key.to_address_bytes();
   recover_public_key_result ret;
   ret.set_value( address );
   return ret;
}

THUNK_DEFINE( get_transaction_payer_result, get_transaction_payer, ((const protocol::transaction&) transaction) )
{
   context.resource_meter().use_compute_bandwidth( compute_load::light );

   std::string account = system_call::recover_public_key( context, transaction.signature(), util::converter::as< std::string >( crypto::hash( crypto::multicodec::sha2_256, transaction.header() ) ) );

   LOG(debug) << "(get_transaction_payer) transaction: " << transaction;

   get_transaction_payer_result ret;
   ret.set_value( account );
   return ret;
}

THUNK_DEFINE( get_transaction_rc_limit_result, get_transaction_rc_limit, ((const protocol::transaction&) transaction) )
{
   context.resource_meter().use_compute_bandwidth( compute_load::light );

   get_transaction_rc_limit_result ret;
   ret.set_value( transaction.header().rc_limit() );
   return ret;
}

THUNK_DEFINE_VOID( get_last_irreversible_block_result, get_last_irreversible_block )
{
   context.resource_meter().use_compute_bandwidth( compute_load::light );

   auto head = context.get_state_node();

   get_last_irreversible_block_result ret;
   ret.set_value( head->revision() > default_irreversible_threshold ? head->revision() - default_irreversible_threshold : 0 );

   return ret;
}

THUNK_DEFINE_VOID( get_caller_result, get_caller )
{
   context.resource_meter().use_compute_bandwidth( compute_load::light );

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

THUNK_DEFINE_VOID( get_transaction_signature_result, get_transaction_signature )
{
   KOINOS_ASSERT( !context.read_only(), read_only_context, "unable to perform action while context is read only" );

   context.resource_meter().use_compute_bandwidth( compute_load::light );

   get_transaction_signature_result ret;
   ret.set_value( context.get_transaction().signature() );
   return ret;
}

THUNK_DEFINE( void, require_authority, ((const std::string&) account) )
{
   KOINOS_ASSERT( !context.read_only(), read_only_context, "unable to perform action while context is read only" );

   context.resource_meter().use_compute_bandwidth( compute_load::light );

   auto digest = crypto::hash( crypto::multicodec::sha2_256, context.get_transaction().header() );
   std::string sig_account = system_call::recover_public_key( context, system_call::get_transaction_signature( context ), util::converter::as< std::string >( digest ) );

   KOINOS_ASSERT(
      account == sig_account,
      invalid_signature,
      "signature does not match", ("account", account)("sig_account", sig_account)
   );
}

THUNK_DEFINE_VOID( get_contract_id_result, get_contract_id )
{
   context.resource_meter().use_compute_bandwidth( compute_load::light );

   get_contract_id_result ret;
   ret.set_value( context.get_contract_id() );
   return ret;
}

THUNK_DEFINE( get_account_nonce_result, get_account_nonce, ((const std::string&) account ) )
{
   context.resource_meter().use_compute_bandwidth( compute_load::light );

   auto obj = system_call::get_object( context, state::space::transaction_nonce(), account );

   get_account_nonce_result ret;

   if ( obj.exists() )
      ret.set_value( util::converter::to< uint64_t >( obj.value() ) );
   else
      ret.set_value( 0 );

   return ret;
}

THUNK_DEFINE( get_account_rc_result, get_account_rc, ((const std::string&) account) )
{
   context.resource_meter().use_compute_bandwidth( compute_load::light );

   uint64_t max_resources = 10'000'000;
   get_account_rc_result ret;
   ret.set_value( max_resources );
   return ret;
}

THUNK_DEFINE( consume_account_rc_result, consume_account_rc, ((const std::string&) account, (uint64_t) rc) )
{
   context.resource_meter().use_compute_bandwidth( compute_load::light );

   consume_account_rc_result ret;
   ret.set_value( true );
   return ret;
}

THUNK_DEFINE_VOID( get_resource_limits_result, get_resource_limits )
{
   context.resource_meter().use_compute_bandwidth( compute_load::light );

   resource_limit_data rd;

   rd.set_disk_storage_cost( 10 );
   rd.set_disk_storage_limit( 204'800 );

   rd.set_network_bandwidth_cost( 5 );
   rd.set_network_bandwidth_limit( 1'048'576 );

   rd.set_compute_bandwidth_cost( 1 );
   rd.set_compute_bandwidth_limit( 100'000'000 );

   get_resource_limits_result ret;
   *ret.mutable_value() = rd;
   return ret;
}

THUNK_DEFINE( consume_block_resources_result, consume_block_resources, ((uint64_t) disk, (uint64_t) network, (uint64_t) compute) )
{
   context.resource_meter().use_compute_bandwidth( compute_load::light );

   consume_block_resources_result ret;
   ret.set_value( true );
   return ret;
}

THUNK_DEFINE( void, event, ((const std::string&) name, (const std::string&) data, (const std::vector< std::string >&) impacted) )
{
   auto caller = context.get_caller();

   context.resource_meter().use_compute_bandwidth( compute_load::light );
   context.resource_meter().use_network_bandwidth( caller.size() + name.size() + data.size() );

   protocol::event_data ev;
   ev.set_source( caller );
   ev.set_name( name );
   ev.set_data( data );

   for ( auto& imp : impacted )
      *ev.add_impacted() = imp;

   context.event_recorder().push_event( std::move( ev ) );
}

THUNK_DEFINE_END();

} // koinos::chain
