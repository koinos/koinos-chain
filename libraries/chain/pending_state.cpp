#include <koinos/chain/pending_state.hpp>

#include <koinos/chain/exceptions.hpp>
#include <koinos/chain/execution_context.hpp>
#include <koinos/chain/system_calls.hpp>
#include <koinos/rpc/mempool/mempool_rpc.pb.h>
#include <koinos/broadcast/broadcast.pb.h>
#include <koinos/util/services.hpp>

namespace koinos::chain {

state_db::anonymous_state_node_ptr pending_state::get_state_node()
{
   return _pending_state;
}

void pending_state::set_backend( std::shared_ptr< vm_manager::vm_backend > backend )
{
   _backend = backend;
}

void pending_state::set_client( std::shared_ptr< mq::client > client )
{
   _client = client;
}

void pending_state::rebuild( state_db::state_node_ptr head )
{
   _pending_state = head->create_anonymous_node();

   if ( _client && _client->ready() )
   {
      LOG(debug) << "Rebuilding pending state";

      rpc::mempool::mempool_request req;
      req.mutable_get_pending_transactions();

      auto future = _client->rpc( util::service::mempool, util::converter::as< std::string >( req ) );

      execution_context ctx( _backend, intent::transaction_application );

      ctx.set_state_node( _pending_state );
      ctx.build_cache();

      ctx.push_frame( stack_frame {
         .call_privilege = privilege::kernel_mode
      } );

      rpc::mempool::mempool_response resp;
      resp.ParseFromString( future.get() );

      KOINOS_ASSERT( !resp.has_error(), rpc_failure, "received error from mempool: ${e}", ("e", resp.error()) );
      KOINOS_ASSERT( resp.has_get_pending_transactions(), rpc_failure, "unexpected response when requesting pending transactions: ${r}", ("r", resp) );

      const auto& pending_trxs = resp.get_pending_transactions();

      LOG(debug) << "Retrieved " << pending_trxs.pending_transactions_size() << " transaction(s) from the mempool for reapplication";

      auto resource_limits = system_call::get_resource_limits( ctx );

      for ( const auto& ptransaction : pending_trxs.pending_transactions() )
      {
         ctx.resource_meter().set_resource_limit_data( resource_limits );

         try
         {
            system_call::apply_transaction( ctx, ptransaction.transaction() );
         }
         catch ( const std::exception& e )
         {
            broadcast::transaction_failed ptf;
            ptf.set_id( ptransaction.transaction().id() );
            _client->broadcast( "koinos.transaction.fail", util::converter::as< std::string >( ptf ) );
         }
      }
   }
}

} // koinos::chain
