#include <koinos/mq/consumer.hpp>

#include <chrono>
#include <cstdlib>

namespace koinos::mq {

std::string rand_str( int len )
{
   static const char characters[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
   string tmp;
   tmp.reserve( len );

   for ( int i = 0; i < len; i++ )
   {
      tmp += characters[rand() % (sizeof(characters) - 1)];
   }

   return tmp;
}

void consumer_thread_main( synced_msg_queue_t& input_queue, synced_msg_queue_t& output_queue, const msg_routing_map_t& routing_map )
{
   while( true )
   {
      std::shared_ptr< message > msg;
      try
      {
         input_queue.pull_front( msg );
      }
      catch( const boost::concurrent::sync_queue_is_closed& )
      {
         break;
      }

      std::shared_ptr< message > reply;
      auto routing_itr = routing_map.find( msg->exchange + msg->routing_key );

      if ( routing_itr == routing_map.end() )
      {
         LOG(error) << "Did not find route: " << msg->exchange << ":" << msg->routing_key;
      }
      else
      {
         auto resp = routing_itr->second( msg.data, msg.content_type );

         if ( resp.hasValue() && msg.reply_to.has_value() && msg.correlation_id.has_value() )
         {
            reply->exchange = "";
            reply->reply_to = *msg->reply_to;
            reply->content_type = msg->content_type;
            reply->correlation_id = *msg->correlation_id;
            reply->data = *resp;

            output_queue.push_back( reply );
         }
      }
   }
}

consumer::consumer() :
   _handlers( std::make_shared< handler_table >() ),
   _publisher_broker( std::make_shared< message_broker >() ),
   _consumer_broker( std::make_shared< message_broker >() ) {}

consumer::~consumer() = default;

void consumer::start()
{
   _consumer_thread = std::make_unique< std::thread >( [&]()
   {
      consume( _consumer_broker );
   } );

   _publisher_thread = std::make_unique< std::thread >( [&]()
   {
      publisher( _publisher_broker );
   } );

   std::size_t num_threads = boost::thread::hardware_concurrency()+1;
   for( std::size_t i = 0; i < num_threads; i++ )
   {
      _consumer_pool.emplace_back( [&]()
      {
         consumer_thread_main( _input_queue, _output_queue, _handler_map );
      } );
   }
}

void consumer::stop()
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

error_code consumer::connect( const std::string& amqp_url )
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

error_code consumer::prepare( prepare_func f )
{
   return f( *_publisher_broker );
}

error_code consumer::add_msg_handler( const std::string& exchange, const std::string& topic, bool exclusive, msg_handler_func handler )
{
   auto queue_name = exclusive ? topic : topic + rand_str(16);

   auto ec = _consumer_broker->declare_exchange( exchange );
   if ec != error_code::success
      return ec;

   _consumer_broker->declare_queue( queue_name );
   if ec != error_code::success
      return ec;

   _consumer_broker->bind_queue( queue_name, exchange, topic );
   if ec != error_code::success
      return ec;

   _handler_map.emplace( std::pair< std::string, std::string >( exchange, topic ), handler );

   return ec;
}

void consumer::publisher( std::shared_ptr< message_broker > broker )
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

void consumer::consume( std::shared_ptr< message_broker > broker )
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
