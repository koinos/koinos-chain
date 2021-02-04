#pragma once

#include <koinos/mq/message_broker.hpp>
#include <koinos/log.hpp>
#include <koinos/util.hpp>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/thread/sync_bounded_queue.hpp>
#include <boost/tuple/tuple.hpp>

#include <memory>
#include <thread>
#include <unordered_map>

namespace koinos::mq {

struct rpc_call
{
   message req;
   message resp;
   error_code err;
};

using msg_handler_void_func = std::function< void( const std::string& ) >;
using msg_handler_string_func = std::function< std::string( const std::string& ) >;
using msg_handler_func = std::variant< msg_handler_void_func, msg_handler_string_func >;
using synced_msg_queue = boost::concurrent::sync_bounded_queue< std::shared_ptr< message > >;
using handler_verify_func = std::function< bool( const message& ) >;
using handler_pair = std::pair< handler_verify_func, msg_handler_func >;
using msg_routing_map = boost::container::flat_map< std::pair< std::string, std::string >, std::vector< handler_pair > >;

using prepare_func = std::function< error_code( message_broker& b ) >;

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

      error_code add_msg_handler(
         const std::string& exchange,
         const std::string& topic,
         bool exclusive,
         handler_verify_func,
         msg_handler_func );

      // The compiler should be able to implicitly convert msg_handler_string_func -> msg_handler_func
      // but doesn't. This explicit binding compiles. Very likely to break in the future as an
      // ambiguous function binding. If so, delete this function.
      error_code add_msg_handler(
         const std::string& exchange,
         const std::string& topic,
         bool exclusive,
         handler_verify_func,
         msg_handler_string_func );

   private:
      void consume( std::shared_ptr< message_broker > broker );
      void publisher( std::shared_ptr< message_broker > broker );

      std::unique_ptr< std::thread >    _consumer_thread;
      std::shared_ptr< message_broker > _consumer_broker;

      std::unique_ptr< std::thread >    _publisher_thread;
      std::shared_ptr< message_broker > _publisher_broker;

      std::set< std::pair< std::string, std::string > > _queue_bindings;
      msg_routing_map                   _handler_map;
      std::vector< std::thread >        _consumer_pool;

      synced_msg_queue                  _input_queue{ MAX_QUEUE_SIZE };
      synced_msg_queue                  _output_queue{ MAX_QUEUE_SIZE };
};

} // koinos::mq
