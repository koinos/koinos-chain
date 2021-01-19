
#include <koinos/mq/message_broker.hpp>

#include <koinos/log.hpp>

namespace koinos::mq {

message_broker::~message_broker()
{
   reset();
}

void message_broker::reset() noexcept
{
   amqp_bytes_free( queue_name );
   amqp_channel_close( connection, channel, AMQP_REPLY_SUCCESS );
   amqp_connection_close( connection, AMQP_REPLY_SUCCESS );
   amqp_destroy_connection( connection );
}

error_code message_broker::publish(
   const std::string& routing_key,
   const std::string& data,
   const std::string& content_type,
   const std::string& exchange ) noexcept
{
   amqp_basic_properties_t props;
   props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG;
   props.content_type = amqp_cstring_bytes( content_type.c_str() );
   props.delivery_mode = 2; /* persistent delivery mode */
   amqp_basic_publish(
      connection,
      channel,
      amqp_cstring_bytes( exchange.c_str() ),
      amqp_cstring_bytes( routing_key.c_str() ),
      0,
      0,
      &props,
      amqp_cstring_bytes( data.c_str() )
   );

   return error_code::success;
}

error_code message_broker::connect( const std::string& hostname, uint16_t port ) noexcept
{
   connection = amqp_new_connection();
   socket = amqp_tcp_socket_new( connection );

   if ( !socket )
   {
      LOG(error) << "failed to create socket";
      return error_code::failure;
   }

   if ( amqp_socket_open( socket, hostname.c_str(), port ) )
   {
      LOG(error) << "failed to open socket";
      return error_code::failure;
   }

   auto resp = amqp_login( connection, "/", 0, 131072, 0, AMQP_SASL_METHOD_PLAIN, "guest", "guest" );
   if ( resp.reply_type != AMQP_RESPONSE_NORMAL )
   {
      LOG(error) << "failed to login to AMQP server";
      return error_code::failure;
   }

   if ( !amqp_channel_open( connection, channel ) )
   {
      LOG(error) << "failed to open channel";
      return error_code::failure;
   }

   return error_code::success;
}

} // koinos::mq
