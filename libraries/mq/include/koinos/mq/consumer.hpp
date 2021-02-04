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

typedef std::function< std::optional< std::string >( const std::string&, const std::string& ) > msg_handler_func;

using prepare_func = std::function< error_code( message_broker& b ) >;

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
      consumer();
      virtual ~consumer();

      void start();
      void stop();
      error_code connect( const std::string& amqp_url );
      error_code prepare( prepare_func f );

      void add_msg_handler( const std::string& exchange, const std::string& topic, bool exclusive, msg_handler_func );

   private:
      typedef boost::concurrent::sync_bounded_queue< std::shared_ptr< message > > synced_msg_queue_t;
      typedef std::unordered_map< std::pair< std::string, std::string >, msg_handler_func > msg_routing_map_t;

      void consume( std::shared_ptr< message_broker > broker );
      void publisher( std::shared_ptr< message_broker > broker );

      std::unique_ptr< std::thread >    _consumer_thread;
      std::shared_ptr< message_broker > _consumer_broker;

      std::unique_ptr< std::thread >    _publisher_thread;
      std::shared_ptr< message_broker > _publisher_broker;

      msg_routing_map_t                 _handler_map;
      std::vector< std::thread >        _consumer_pool;

      synced_msg_queue_t                _input_queue{ MAX_QUEUE_SIZE };
      synced_msg_queue_t                _output_queue{ MAX_QUEUE_SIZE };
};

} // koinos::mq
