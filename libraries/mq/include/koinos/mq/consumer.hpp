#pragma once

#include <koinos/mq/message_broker.hpp>
#include <koinos/log.hpp>
#include <koinos/util.hpp>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/container/flat_map.hpp>

#include <memory>
#include <thread>

namespace koinos::mq {

struct rpc_call
{
   mq::message req;
   mq::message resp;
   mq::error_code err;
};

typedef std::function< std::string( const std::string& ) > rpc_handler_func;

struct handler_table
{
   // Map (content_type, rpc_type) -> handler
   boost::container::flat_map< std::pair< std::string, std::string >, rpc_handler_func > _rpc_handler_map;

   void handle_rpc_call( rpc_call& call );
};

class rpc_mq_consumer : public std::enable_shared_from_this< rpc_mq_consumer >
{
   public:
      rpc_mq_consumer( const std::string& amqp_url );
      virtual ~rpc_mq_consumer();

      void start();
      void add_rpc_handler( const std::string& content_type, const std::string& rpc_type, rpc_handler_func handler );
      void connect_loop();
      void consume_rpc_loop( std::shared_ptr< mq::message_broker > broker, std::shared_ptr< handler_table > handlers );
      std::pair< mq::error_code, std::shared_ptr< std::thread > > connect();

      std::string                      _amqp_url;
      std::unique_ptr< std::thread >   _connect_thread;
      std::shared_ptr< handler_table > _handlers;
};

typedef std::function< std::string( const std::string& ) > handler_func_t;

/**
 * rpc_manager interfaces the generic rpc_mq_consumer with the specific request/response handlers for known serializations
 * (right now, just JSON)
 */
class rpc_manager
{
   public:
      rpc_manager( std::shared_ptr< rpc_mq_consumer > consumer );
      virtual ~rpc_manager();

      void add_rpc_handler( const std::string& rpc_type, handler_func_t handler );

      std::shared_ptr< rpc_mq_consumer > _consumer;
};

} // koinos::mq
