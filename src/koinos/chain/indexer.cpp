#include <koinos/chain/indexer.hpp>

#include <algorithm>

#include <koinos/chain/exceptions.hpp>
#include <koinos/exception.hpp>
#include <koinos/rpc/block_store/block_store_rpc.pb.h>
#include <koinos/util/services.hpp>

constexpr std::size_t request_queue_size = 100;
constexpr std::size_t block_queue_size   = 100;

namespace koinos::chain {

indexer::indexer( boost::asio::io_context& ioc, controller& c, std::shared_ptr< mq::client > mc, bool verify_blocks ):
    _ioc( ioc ),
    _controller( c ),
    _client( mc ),
    _verify_blocks( verify_blocks ),
    _signals( ioc ),
    _request_queue( request_queue_size ),
    _block_queue( block_queue_size )
{
  _signals.add( SIGINT );
  _signals.add( SIGTERM );
#if defined( SIGQUIT )
  _signals.add( SIGQUIT );
#endif // defined(SIGQUIT)

  _signals.async_wait(
    [ & ]( const boost::system::error_code& err, int num )
    {
      _stopped = true;

      if( _complete.has_value() )
      {
        _complete->set_value( false );
        _complete.reset();
      }

      _request_queue.close();
      _block_queue.close();
    } );
}

void indexer::handle_error( const std::string& msg )
{
  _stopped = true;

  if( _complete.has_value() )
  {
    _complete->set_exception( std::make_exception_ptr( indexer_failure_exception( msg ) ) );
    _complete.reset();
  }

  _request_queue.close();
  _block_queue.close();
}

std::future< bool > indexer::index()
{
  boost::asio::post( std::bind( &indexer::prepare_index, this ) );
  return _complete->get_future();
}

void indexer::prepare_index()
{
  try
  {
    if( _stopped )
      return;

    LOG( info ) << "Retrieving highest block from block store";
    rpc::block_store::block_store_request req;
    req.mutable_get_highest_block();

    std::shared_future< std::string > data;

    rpc::block_store::block_store_response resp;

    data = _client->rpc( util::service::block_store, req.SerializeAsString() );

    if( !resp.ParseFromString( data.get() ) )
      return handle_error( "could not get highest block from block store" );

    if( resp.has_error() )
      return handle_error( resp.error().message() );

    if( !resp.has_get_highest_block() )
      return handle_error( "unexpected block store response" );

    _target_head = resp.get_highest_block().topology();

    _start_head_info = _controller.get_head_info();

    if( _start_head_info.head_topology().height() < _target_head.height() )
    {
      LOG( info ) << "Indexing to target block - Height: " << _target_head.height()
                  << ", ID: " << util::to_hex( _target_head.id() );
      boost::asio::post( std::bind( &indexer::send_requests, this, _start_head_info.head_topology().height(), 50 ) );
      boost::asio::post( std::bind( &indexer::process_block, this ) );
    }
    else
    {
      LOG( info ) << "Chain state is synchronized with block store";
      _complete->set_value( true );
      _complete.reset();
    }
  }
  catch( const std::exception& e )
  {
    return handle_error( e.what() );
  }
  catch( const boost::exception& e )
  {
    return handle_error( boost::diagnostic_information( e ) );
  }
}

void indexer::send_requests( uint64_t last_height, uint64_t batch_size )
{
  try
  {
    if( _stopped )
      return;

    if( last_height <= _target_head.height() )
    {
      rpc::block_store::block_store_request req;
      auto* by_height_req = req.mutable_get_blocks_by_height();
      by_height_req->set_head_block_id( _target_head.id() );
      by_height_req->set_ancestor_start_height( last_height + 1 );
      by_height_req->set_num_blocks( uint32_t( batch_size ) );
      by_height_req->set_return_block( true );
      by_height_req->set_return_receipt( true );

      std::shared_future< std::string > data;

      data = _client->rpc( util::service::block_store, req.SerializeAsString(), std::chrono::milliseconds( 5'000 ) );

      _request_queue.push( std::move( data ) );
    }
    else
    {
      _requests_complete = true;
    }

    boost::asio::dispatch( std::bind( &indexer::process_requests, this, last_height, batch_size ) );
  }
  catch( boost::sync_queue_is_closed& )
  {
    LOG( warning ) << "Indexer synchronized queue has been closed";
  }
  catch( const std::exception& e )
  {
    return handle_error( e.what() );
  }
  catch( const boost::exception& e )
  {
    return handle_error( boost::diagnostic_information( e ) );
  }
}

void indexer::process_requests( uint64_t last_height, uint64_t batch_size )
{
  try
  {
    if( _stopped )
      return;

    if( _requests_complete && _request_queue.empty() )
    {
      _request_processing_complete = true;
      return;
    }

    auto fut = _request_queue.pull();

    rpc::block_store::block_store_response resp;
    rpc::block_store::get_blocks_by_height_response by_height_resp;

    if( !resp.ParseFromString( fut.get() ) )
      return handle_error( "could not parse block store response" );

    if( resp.has_error() )
      return handle_error( resp.error().message() );

    if( !resp.has_get_blocks_by_height() )
      return handle_error( "unexpected block store response" );

    for( uint64_t i = 0; i < resp.get_blocks_by_height().block_items_size(); i++ )
      _block_queue.push( std::move( *resp.mutable_get_blocks_by_height()->mutable_block_items( i ) ) );

    boost::asio::post( std::bind( &indexer::send_requests,
                                  this,
                                  last_height + batch_size,
                                  std::min( batch_size * 2, uint64_t( 1'000 ) ) ) );
  }
  catch( boost::sync_queue_is_closed& )
  {
    LOG( warning ) << "Indexer synchronized queue has been closed";
  }
  catch( const std::exception& e )
  {
    return handle_error( e.what() );
  }
  catch( const boost::exception& e )
  {
    return handle_error( boost::diagnostic_information( e ) );
  }
}

void indexer::process_block()
{
  try
  {
    if( _stopped )
      return;

    if( _request_processing_complete && _block_queue.empty() )
    {
      const auto new_head_info                       = _controller.get_head_info();
      const std::chrono::duration< double > duration = std::chrono::system_clock::now() - _start_time;
      LOG( info ) << "Finished indexing "
                  << new_head_info.head_topology().height() - _start_head_info.head_topology().height()
                  << " blocks, took " << duration.count() << " seconds";
      _complete->set_value( true );
      _complete.reset();
      return;
    }

    auto block_item = _block_queue.pull();

    if( _verify_blocks )
    {
      rpc::chain::submit_block_request submit_block;
      *submit_block.mutable_block() = block_item.block();
      _controller.submit_block( submit_block, _target_head.height() );
    }
    else
      _controller.apply_block_delta( block_item.block(), block_item.receipt(), _target_head.height() );

    boost::asio::post( std::bind( &indexer::process_block, this ) );
  }
  catch( boost::sync_queue_is_closed& )
  {
    LOG( warning ) << "Indexer synchronized queue has been closed";
  }
  catch( const std::exception& e )
  {
    return handle_error( e.what() );
  }
  catch( const boost::exception& e )
  {
    return handle_error( boost::diagnostic_information( e ) );
  }
}

} // namespace koinos::chain
