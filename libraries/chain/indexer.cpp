#include <koinos/chain/indexer.hpp>

#include <algorithm>

#include <koinos/chain/exceptions.hpp>
#include <koinos/exception.hpp>
#include <koinos/rpc/block_store/block_store_rpc.pb.h>
#include <koinos/util/services.hpp>

namespace koinos::chain {

indexer::indexer( boost::asio::io_context& ioc, controller& c, std::shared_ptr< mq::client > mc ) :
   _ioc( ioc ),
   _controller( c ),
   _client( mc ),
   _signals( ioc )
{
   _signals.add( SIGINT );
   _signals.add( SIGTERM );
#if defined(SIGQUIT)
   _signals.add( SIGQUIT );
#endif // defined(SIGQUIT)

   _signals.async_wait( [&]( const boost::system::error_code& err, int num )
   {
      _stopped = true;

      if ( _complete.has_value() )
      {
         _complete->set_value( false );
         _complete.reset();
      }

      _request_queue.close();
      _block_queue.close();
   } );
}

std::future< bool > indexer::index()
{
   boost::asio::post( std::bind( &indexer::prepare_index, this ) );
   return _complete->get_future();
}

void indexer::prepare_index()
{
   if ( _stopped )
      return;

   LOG(info) << "Retrieving highest block from block store";
   rpc::block_store::block_store_request req;
   req.mutable_get_highest_block();
   auto future = _client->rpc( util::service::block_store, req.SerializeAsString() );

   rpc::block_store::block_store_response resp;

   if ( !resp.ParseFromString( future.get() ) )
      KOINOS_THROW( chain::parse_failure, "could not get highest block from block store" );

   switch( resp.response_case() )
   {
      case rpc::block_store::block_store_response::ResponseCase::kGetHighestBlock:
         _target_head = resp.get_highest_block().topology();
         break;
      case rpc::block_store::block_store_response::ResponseCase::kError:
         KOINOS_THROW( chain::rpc_failure, resp.error().message() );
         break;
      default:
         KOINOS_THROW( chain::rpc_failure, "unexpected block store response" );
         break;
   }

   _start_head_info = _controller.get_head_info();

   if ( _start_head_info.head_topology().height() < _target_head.height() )
   {
      LOG(info) << "Indexing to target block - Height: " << _target_head.height() << ", ID: 0x" << util::to_hex( _target_head.id() );
      boost::asio::post( std::bind( &indexer::send_requests, this, _start_head_info.head_topology().height(), 50 ) );
      boost::asio::post( std::bind( &indexer::process_block, this ) );
   }
   else
   {
      LOG(info) << "Chain state is synchronized with block store";
      _complete->set_value( true );
      _complete.reset();
   }
}

void indexer::send_requests( uint64_t last_height, uint64_t batch_size )
{
   if ( _stopped )
      return;

   try
   {
      if ( last_height <= _target_head.height() )
      {
         rpc::block_store::block_store_request req;
         auto* by_height_req = req.mutable_get_blocks_by_height();
         by_height_req->set_head_block_id( _target_head.id() );
         by_height_req->set_ancestor_start_height( last_height + 1 );
         by_height_req->set_num_blocks( batch_size );
         by_height_req->set_return_block( true );
         by_height_req->set_return_receipt( false );

         _request_queue.push( std::move( _client->rpc( util::service::block_store, req.SerializeAsString(), std::chrono::milliseconds( 10000 ) ) ) );
      }
      else
      {
         _requests_complete = true;
      }
   }
   catch ( boost::sync_queue_is_closed& )
   {
      LOG(info) << "Indexer synchronized queue has been closed";
   }

   boost::asio::dispatch( std::bind( &indexer::process_requests, this, last_height, batch_size ) );
}

void indexer::process_requests( uint64_t last_height, uint64_t batch_size )
{
   if ( _stopped )
      return;

   if ( _requests_complete && _request_queue.empty() )
   {
      _request_processing_complete = true;
      return;
   }

   try
   {
      auto fut = _request_queue.pull();

      rpc::block_store::block_store_response resp;
      rpc::block_store::get_blocks_by_height_response by_height_resp;

      if ( !resp.ParseFromString( fut.get() ) )
         KOINOS_THROW( chain::parse_failure, "could not parse block store response" );

      switch( resp.response_case() )
      {
         case rpc::block_store::block_store_response::ResponseCase::kGetBlocksByHeight:
            break;
         case rpc::block_store::block_store_response::ResponseCase::kError:
            KOINOS_THROW( chain::rpc_failure, resp.error().message() );
            break;
         default:
            KOINOS_THROW( chain::rpc_failure, "unexpected block store response" );
            break;
      }

      for ( auto& block_item : *resp.mutable_get_blocks_by_height()->mutable_block_items() )
         _block_queue.push( std::move( *block_item.mutable_block() ) );

   }
   catch ( boost::sync_queue_is_closed& )
   {
      LOG(info) << "Indexer synchronized queue has been closed";
   }

   boost::asio::post( std::bind( &indexer::send_requests, this, last_height + batch_size, std::min( batch_size * 2, uint64_t( 1000 ) ) ) );
}

void indexer::process_block()
{
   if ( _stopped )
      return;

   if ( _request_processing_complete && _block_queue.empty() )
   {
      const auto new_head_info = _controller.get_head_info();
      const std::chrono::duration< double > duration = std::chrono::system_clock::now() - _start_time;
      LOG(info) << "Finished indexing " << new_head_info.head_topology().height() - _start_head_info.head_topology().height() << " blocks, took " << duration.count() << " seconds";
      _complete->set_value( true );
      _complete.reset();
      return;
   }

   try
   {
      rpc::chain::submit_block_request submit_block;

      *submit_block.mutable_block() = _block_queue.pull();
      _controller.submit_block( submit_block, _target_head.height() );
   }
   catch ( const koinos::exception& e )
   {
      LOG(fatal) << "An unexpected error has occurred during index: " << e.what();
      _complete->set_value( false );
      _complete.reset();
   }
   catch ( boost::sync_queue_is_closed& )
   {
      LOG(info) << "Indexer synchronized queue has been closed";
   }

   boost::asio::post( std::bind( &indexer::process_block, this ) );
}

} // koinos::chain
