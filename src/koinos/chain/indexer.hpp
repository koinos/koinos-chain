#pragma once

#include <atomic>
#include <chrono>
#include <future>
#include <optional>

#include <boost/asio.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/thread/sync_bounded_queue.hpp>

#include <koinos/block_store/block_store.pb.h>
#include <koinos/chain/controller.hpp>
#include <koinos/chain/exceptions.hpp>
#include <koinos/mq/client.hpp>

namespace koinos::chain {

class indexer final
{
public:
  indexer( boost::asio::io_context& ioc, controller& c, std::shared_ptr< mq::client > mc, bool verify_blocks );

  std::future< bool > index();

private:
  void prepare_index();
  void send_requests( uint64_t last_height, uint64_t batch_size );
  void process_requests( uint64_t last_height, uint64_t batch_size );
  void process_block();

  void handle_error( const std::string& msg );

  boost::asio::io_context& _ioc;
  controller& _controller;
  std::shared_ptr< mq::client > _client;
  bool _verify_blocks = false;

  boost::asio::signal_set _signals;
  std::atomic_bool _stopped = false;

  boost::concurrent::sync_bounded_queue< std::shared_future< std::string > > _request_queue;
  std::atomic< bool > _requests_complete           = false;
  std::atomic< bool > _request_processing_complete = false;

  boost::concurrent::sync_bounded_queue< block_store::block_item > _block_queue;

  block_topology _target_head;
  rpc::chain::get_head_info_response _start_head_info;
  const std::chrono::time_point< std::chrono::system_clock > _start_time = std::chrono::system_clock::now();

  std::optional< std::promise< bool > > _complete = std::promise< bool >();
};

} // namespace koinos::chain
