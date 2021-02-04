#include <koinos/mq/consumer.hpp>

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

consumer::consumer( const std::string& amqp_url ) :
   _amqp_url( amqp_url ),
   _handlers( std::make_shared< handler_table >() ) {}

consumer::~consumer() {}

void consumer::start()
{
   _connect_thread = std::make_unique< std::thread >( [&]()
   {
      connect_loop();
   } );
}

void consumer::add_rpc_handler( const std::string& content_type, const std::string& rpc_type, rpc_handler_func handler )
{
   _handlers->_rpc_handler_map.emplace( std::pair< std::string, std::string >( content_type, rpc_type ), handler );
}

std::pair< error_code, std::shared_ptr< std::thread > > consumer::connect()
{
   std::shared_ptr< message_broker > broker = std::make_shared< message_broker >();
   std::pair< error_code, std::shared_ptr< std::thread > > result;

   result.first = broker->connect( _amqp_url );
   if ( result.first != error_code::success )
      return result;

   KOINOS_TODO( "Does this work for n>1?" );
   for ( auto it = _handlers->_rpc_handler_map.begin(); it != _handlers->_rpc_handler_map.end(); ++it )
   {
      auto queue_name = std::string( "koinos_rpc_" ) + it->first.second;

      result.first = broker->declare_queue( queue_name );
      if ( result.first != error_code::success )
         return result;

      result.first = broker->bind_queue( queue_name, exchange::rpc, queue_name );
      if ( result.first != error_code::success )
         return result;
   }

   std::shared_ptr< consumer > sthis = shared_from_this();

   result.second = std::make_shared< std::thread >( [ sthis, broker ]() { sthis->consume( broker, sthis->_handlers ); } );
   return result;
}

void consumer::consume( std::shared_ptr< message_broker > broker, std::shared_ptr< handler_table > handlers )
{
   while ( true )
   {
      auto result = broker->consume();

      if ( result.first == error_code::time_out )
         continue;

      if ( result.first != error_code::success )
      {
         LOG(error) << "failed to consume message";
         continue;
      }

      if ( !result.second.has_value() )
      {
         LOG(error) << "consumption succeeded but resulted in an empty message";
         continue;
      }

      rpc_call call;
      call.req = *result.second;

      handlers->handle_rpc_call( call );

      broker->publish( call.resp );
   }
}


void consumer::connect_loop()
{
   const static int RETRY_MIN_DELAY_MS = 1000;
   const static int RETRY_MAX_DELAY_MS = 25000;
   const static int RETRY_DELAY_PER_RETRY_MS = 2000;

   while ( true )
   {
      int retry_count = 0;

      std::shared_ptr< std::thread > consumer_thread;

      while ( true )
      {
         std::unique_ptr< message_broker > broker;
         std::pair< error_code, std::shared_ptr< std::thread > > result = connect();
         consumer_thread = result.second;

         if ( result.first == error_code::success )
            break;

         int delay_ms = RETRY_MIN_DELAY_MS + RETRY_DELAY_PER_RETRY_MS * retry_count;
         if( delay_ms > RETRY_MAX_DELAY_MS )
            delay_ms = RETRY_MAX_DELAY_MS;

         // TODO:  Add quit notification for clean termination
         std::this_thread::sleep_for( std::chrono::milliseconds( delay_ms ) );
         retry_count++;
      }
      consumer_thread->join();
      // TODO:  Cleanup closed handlers
   }
}

} // koinos::mq
