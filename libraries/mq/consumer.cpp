#include <koinos/mq/consumer.hpp>

#include <chrono>

namespace koinos::mq {

void handler_table::handle_rpc_call( rpc_call& call )
{
   if ( !call.req.reply_to.has_value() )
   {
      LOG(error) << "Could not service RPC, reply_to not specified";
      return;
   }

   if ( !call.req.correlation_id.has_value() )
   {
      LOG(error) << "Could not service RPC, correlation_id not specified";
      return;
   }

   call.resp.exchange = "koinos_rpc_reply";
   call.resp.routing_key = *call.req.reply_to;
   call.resp.content_type = call.req.content_type;
   call.resp.correlation_id = call.req.correlation_id;

   const std::string prefix = "koinos_rpc_";

   if ( !boost::starts_with( call.req.routing_key, prefix ) )
   {
      LOG(error) << "Could not parse rpc_type";
      call.err = error_code::failure;
      call.resp.data = "{\"error\":\"Could not parse rpc_type\"}";
      return;
   }

   std::string rpc_type = call.req.routing_key.substr( prefix.size() );

   std::pair< std::string, std::string > k( call.req.content_type, rpc_type );

   auto it = _rpc_handler_map.find( k );
   if ( it == _rpc_handler_map.end() )
   {
      KOINOS_TODO( "Fallback to error handler indexed by content_type only, then fallback to default error handler" );
      LOG(error) << "Could not find RPC handler for requested content_type, routing_key";
      call.err = error_code::failure;
      KOINOS_TODO( "More meaningful error message" );
      call.resp.data = "{\"error\":\"Could not find RPC handler for requested content_type, routing_key\"}";
      return;
   }
   call.resp.data = (it->second)(call.req.data);
   call.err = error_code::success;
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
}

void consumer::stop()
{
   _input_queue.close();
   if ( _consumer_thread )
      _consumer_thread->join();

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

void consumer::add_rpc_handler( const std::string& content_type, const std::string& rpc_type, rpc_handler_func handler )
{
   _handlers->_rpc_handler_map.emplace( std::pair< std::string, std::string >( content_type, rpc_type ), handler );
}

void consumer::publisher( std::shared_ptr< message_broker > broker )
{
   while ( true )
   {
      std::shared_ptr< message > m;
      auto status = _output_queue.try_pull_front( m );

      if ( status == boost::queue_op_status::closed )
         break;

      if ( status == boost::queue_op_status::success )
      {
         auto r = broker->publish( *m );

         if ( r != error_code::success )
         {
            LOG(error) << "an error has occurred while publishing message";
         }
      }

      std::this_thread::sleep_for( std::chrono::milliseconds( 250 ) );
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

      auto status = _input_queue.try_push_back( result.second );

      if ( status == boost::queue_op_status::closed )
         break;
   }
}

} // koinos::mq
