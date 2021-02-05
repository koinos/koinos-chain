#include <koinos/mq/request_handler.hpp>

#include <koinos/util.hpp>

#include <chrono>
#include <cstdlib>

namespace koinos::mq {

void consumer_thread_main( synced_msg_queue& input_queue, synced_msg_queue& output_queue, const msg_routing_map& routing_map )
{
   while ( true )
   {
      std::shared_ptr< message > msg;
      try
      {
         input_queue.pull_front( msg );
      }
      catch ( const boost::concurrent::sync_queue_is_closed& )
      {
         break;
      }

      auto reply = std::make_shared< message >();
      auto routing_itr = routing_map.find( std::make_pair( msg->exchange, msg->routing_key ) );

      if ( routing_itr == routing_map.end() )
      {
         LOG(error) << "Did not find route: " << msg->exchange << ":" << msg->routing_key ;
      }
      else
      {
         for ( const auto& h_pair : routing_itr->second )
         {
            if ( !h_pair.first( msg->content_type ) )
               continue;

            std::visit(
               koinos::overloaded {
                  [&]( const msg_handler_string_func& f )
                  {
                     auto resp = f( msg->data );
                     if ( msg->reply_to.has_value() && msg->correlation_id.has_value() )
                     {
                        reply->exchange = "";
                        reply->routing_key = *msg->reply_to;
                        reply->content_type = msg->content_type;
                        reply->correlation_id = *msg->correlation_id;
                        reply->data = resp;

                        output_queue.push_back( reply );
                     }
                  },
                  [&]( const msg_handler_void_func& f )
                  {
                     f( msg->data );
                  },
                  [&]( const auto& ) {}
               }, h_pair.second );
            break;
         }
      }
   }
}

request_handler::request_handler() :
   _publisher_broker( std::make_shared< message_broker >() ),
   _consumer_broker( std::make_shared< message_broker >() ) {}

request_handler::~request_handler() = default;

void request_handler::start()
{
   _consumer_thread = std::make_unique< std::thread >( [&]()
   {
      consumer( _consumer_broker );
   } );

   _publisher_thread = std::make_unique< std::thread >( [&]()
   {
      publisher( _publisher_broker );
   } );

   std::size_t num_threads = std::thread::hardware_concurrency() + 1;
   for ( std::size_t i = 0; i < num_threads; i++ )
   {
      _consumer_pool.emplace_back( [&]()
      {
         consumer_thread_main( _input_queue, _output_queue, _handler_map );
      } );
   }
}

void request_handler::stop()
{
   _input_queue.close();
   if ( _consumer_thread )
      _consumer_thread->join();

   for( auto& c : _consumer_pool )
      c.join();
   _consumer_pool.clear();

   _output_queue.close();
   if ( _publisher_thread )
      _publisher_thread->join();
}

error_code request_handler::connect( const std::string& amqp_url )
{
   error_code ec;

   ec = _publisher_broker->connect( amqp_url );
   if ( ec != error_code::success )
      return ec;

   ec = _consumer_broker->connect( amqp_url );
   if ( ec != error_code::success )
      return ec;

   return error_code::success;
}

error_code request_handler::add_msg_handler(
   const std::string& exchange,
   const std::string& routing_key,
   bool competing_consumer,
   handler_verify_func verify,
   msg_handler_func handler )
{
   std::string queue_name;
   auto binding = std::make_pair( exchange, routing_key );
   auto binding_itr = _queue_bindings.find( binding );
   error_code ec = error_code::success;

   if ( binding_itr == _queue_bindings.end() )
   {
      ec = _consumer_broker->declare_exchange(
         exchange,
         competing_consumer ? "direct" : "topic",
         false, // Passive
         true,  // Durable
         false, // Auto Delete
         false  // Internal
      );
      if ( ec != error_code::success )
         return ec;

      auto queue_res = _consumer_broker->declare_queue(
         competing_consumer ? routing_key : "",
         false,               // Passive
         competing_consumer,  // Durable
         !competing_consumer, // Exclusive
         false                // Internal
      );
      if ( queue_res.first != error_code::success )
         return queue_res.first;

      ec = _consumer_broker->bind_queue( queue_res.second, exchange, routing_key );
      if ( ec != error_code::success )
         return ec;

      queue_name = queue_res.second;
      _queue_bindings.emplace( binding, queue_name );
   }
   else
   {
      queue_name = binding_itr->second;
   }

   // Valid routes are:
   //    exchange, routing_key
   //    "", queue_name
   auto default_binding = std::make_pair( "", queue_name );
   auto handler_itr = _handler_map.find( binding );

   if ( handler_itr == _handler_map.end() )
   {
      _handler_map.emplace( binding,         std::vector< handler_pair >{ std::make_pair( verify, handler ) } );
      _handler_map.emplace( default_binding, std::vector< handler_pair >{ std::make_pair( verify, handler ) } );
   }
   else
   {
      handler_itr->second.emplace_back( std::make_pair( verify, handler ) );

      handler_itr = _handler_map.find( default_binding );
      if ( handler_itr == _handler_map.end() )
      {
         _handler_map[ binding ].pop_back();
         LOG(error) << "Default binding route not found in handler map";
         ec = error_code::failure;
      }
      else
      {
         handler_itr->second.emplace_back( std::make_pair( verify, handler ) );
      }
   }

   return ec;
}

error_code request_handler::add_msg_handler(
   const std::string& exchange,
   const std::string& routing_key,
   bool exclusive,
   handler_verify_func verify,
   msg_handler_string_func handler )
{
   return add_msg_handler(
      exchange,
      routing_key,
      exclusive,
      verify,
      msg_handler_func( handler )
   );
}

void request_handler::publisher( std::shared_ptr< message_broker > broker )
{
   while ( true )
   {
      std::shared_ptr< message > m;

      try
      {
         _output_queue.pull_front( m );
      }
      catch ( const boost::sync_queue_is_closed& )
      {
         break;
      }

      auto r = broker->publish( *m );

      if ( r != error_code::success )
      {
         LOG(error) << "an error has occurred while publishing message";
      }
   }
}

void request_handler::consumer( std::shared_ptr< message_broker > broker )
{
   while ( true )
   {
      auto result = broker->consume();

      if ( result.first == error_code::time_out )
      {
         if ( _input_queue.closed() )
            break;
         else
            continue;
      }

      if ( result.first != error_code::success )
      {
         LOG(error) << "failed to consume message";
         continue;
      }

      if ( !result.second )
      {
         LOG(error) << "consumption succeeded but resulted in an empty message";
         continue;
      }

      try
      {
         _input_queue.push_back( result.second );
      }
      catch ( const boost::sync_queue_is_closed& )
      {
         break;
      }
   }
}

} // koinos::mq
