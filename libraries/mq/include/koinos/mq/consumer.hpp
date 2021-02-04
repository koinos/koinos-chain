#pragma once

#include <koinos/mq/message_broker.hpp>
#include <koinos/log.hpp>
#include <koinos/util.hpp>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/thread/sync_bounded_queue.hpp>

#include <memory>
#include <thread>

namespace koinos::mq {

struct rpc_call
{
   message req;
   message resp;
   error_code err;
};

typedef std::function< std::string( const std::string& ) > rpc_handler_func;

struct handler_table
{
   // Map (content_type, rpc_type) -> handler
   boost::container::flat_map< std::pair< std::string, std::string >, rpc_handler_func > _rpc_handler_map;

   void handle_rpc_call( rpc_call& call );
};

constexpr std::size_t MAX_QUEUE_SIZE = 1024;

class consumer : public std::enable_shared_from_this< consumer >
{
   public:
      consumer( const std::string& amqp_url );
      virtual ~consumer();

      void start();
      void add_rpc_handler( const std::string& content_type, const std::string& rpc_type, rpc_handler_func handler );


   private:
      void connect_loop();
      void consume( std::shared_ptr< message_broker > broker, std::shared_ptr< handler_table > handlers );
      std::pair< mq::error_code, std::shared_ptr< std::thread > > connect();

      std::string                      _amqp_url;
      std::unique_ptr< std::thread >   _connect_thread;
      std::shared_ptr< handler_table > _handlers;

      boost::concurrent::sync_bounded_queue< std::shared_ptr< message > > _input_queue{ MAX_QUEUE_SIZE };
      boost::concurrent::sync_bounded_queue< std::shared_ptr< message > > _output_queue{ MAX_QUEUE_SIZE };
};

} // koinos::mq
