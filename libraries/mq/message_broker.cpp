#include <koinos/mq/message_broker.hpp>

#include <cstdio>

#include <amqp_tcp_socket.h>
#include <amqp.h>
#include <amqp_framing.h>

#include <boost/lexical_cast.hpp>

#include <koinos/log.hpp>

namespace koinos::mq {

namespace detail {

class message_broker_impl final
{
private:
   amqp_connection_state_t connection = nullptr;
   const amqp_channel_t channel = 1;

   std::optional< std::string > error_info( amqp_rpc_reply_t r ) noexcept;

public:
   message_broker_impl() = default;
   ~message_broker_impl();

   error_code connect(
      const std::string& host,
      uint16_t port,
      const std::string& vhost,
      const std::string& user,
      const std::string& pass
   ) noexcept;

   void disconnect() noexcept;

   bool is_connected() noexcept;

   error_code publish(
      const std::string& routing_key,
      const std::string& data,
      const std::string& content_type,
      const std::string& exchange
   ) noexcept;

   std::pair< error_code, std::optional< message > > consume() noexcept;

   error_code queue_declare( const std::string& queue ) noexcept;
   error_code queue_bind(
      const std::string& queue,
      const std::string& exchange,
      const std::string& binding_key
   ) noexcept;
};

message_broker_impl::~message_broker_impl()
{
   disconnect();
}

void message_broker_impl::disconnect() noexcept
{
   if ( !connection )
      return;

   auto r = amqp_channel_close( connection, channel, AMQP_REPLY_SUCCESS );
   if ( r.reply_type != AMQP_RESPONSE_NORMAL )
   {
      LOG(error) << error_info( r ).value();
   }

   r = amqp_connection_close( connection, AMQP_REPLY_SUCCESS );
   if ( r.reply_type != AMQP_RESPONSE_NORMAL )
   {
      LOG(error) << error_info( r ).value();
   }

   int err = amqp_destroy_connection( connection );
   if ( err < AMQP_STATUS_OK )
   {
      LOG(error) << amqp_error_string2( err );
   }

   connection = nullptr;
}

bool message_broker_impl::is_connected() noexcept
{
   return connection != nullptr;
}

error_code message_broker_impl::publish(
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

error_code message_broker_impl::connect(
   const std::string& host,
   uint16_t port,
   const std::string& vhost,
   const std::string& user,
   const std::string& pass ) noexcept
{
   disconnect();

   connection = amqp_new_connection();
   amqp_socket_t *socket = amqp_tcp_socket_new( connection );

   if ( !socket )
   {
      LOG(error) << "failed to create socket";
      disconnect();
      return error_code::failure;
   }

   if ( amqp_socket_open( socket, host.c_str(), port ) )
   {
      LOG(error) << "failed to open socket";
      disconnect();
      return error_code::failure;
   }

   auto r = amqp_login(
      connection,
      vhost.c_str(),
      AMQP_DEFAULT_MAX_CHANNELS,
      AMQP_DEFAULT_FRAME_SIZE,
      0 /* seconds between heartbeat frames */,
      AMQP_SASL_METHOD_PLAIN,
      user.c_str(),
      pass.c_str()
   );
   if ( r.reply_type != AMQP_RESPONSE_NORMAL )
   {
      LOG(error) << error_info( r ).value();
      disconnect();
      return error_code::failure;
   }

   amqp_channel_open( connection, channel );
   r = amqp_get_rpc_reply( connection );
   if ( r.reply_type != AMQP_RESPONSE_NORMAL )
   {
      LOG(error) << error_info( r ).value();
      disconnect();
      return error_code::failure;
   }

   return error_code::success;
}

error_code message_broker_impl::queue_declare( const std::string& queue ) noexcept
{
   amqp_queue_declare( connection, channel, amqp_cstring_bytes( queue.c_str() ), 0, 0, 0, 1, amqp_empty_table );
   auto reply = amqp_get_rpc_reply( connection );
   if ( reply.reply_type != AMQP_RESPONSE_NORMAL )
   {
      LOG(error) << error_info( reply ).value();
      return error_code::failure;
   }

   return error_code::success;
}

error_code message_broker_impl::queue_bind(
   const std::string& queue,
   const std::string& exchange,
   const std::string& binding_key ) noexcept
{
   auto queue_bytes = amqp_cstring_bytes( queue.c_str() );
   amqp_queue_bind(
      connection,
      channel,
      queue_bytes,
      amqp_cstring_bytes( exchange.c_str() ),
      amqp_cstring_bytes( binding_key.c_str() ),
      amqp_empty_table
   );

   auto reply = amqp_get_rpc_reply( connection );
   if ( reply.reply_type != AMQP_RESPONSE_NORMAL )
   {
      LOG(error) << error_info( reply ).value();
      return error_code::failure;
   }

   amqp_basic_consume( connection, channel, queue_bytes, amqp_empty_bytes, 0, 1, 0, amqp_empty_table );
   reply = amqp_get_rpc_reply( connection );
   if ( reply.reply_type != AMQP_RESPONSE_NORMAL )
   {
      LOG(error) << error_info( reply ).value();
      return error_code::failure;
   }

   return error_code::success;
}

std::optional< std::string > message_broker_impl::error_info( amqp_rpc_reply_t r ) noexcept
{
   if ( r.reply_type == AMQP_RESPONSE_NONE )
   {
      return "missing rpc reply type";
   }
   else if ( r.reply_type == AMQP_RESPONSE_LIBRARY_EXCEPTION )
   {
      return amqp_error_string2( r.library_error );
   }
   else if ( r.reply_type == AMQP_RESPONSE_SERVER_EXCEPTION )
   {
      constexpr std::size_t bufsize = 256;
      char buf[ bufsize ];
      switch ( r.reply.id )
      {
         case AMQP_CONNECTION_CLOSE_METHOD:
         {
            amqp_connection_close_t *m = (amqp_connection_close_t *)r.reply.decoded;
            snprintf(
               buf,
               bufsize,
               "server connection error %u, message: %.*s",
               m->reply_code,
               (int)m->reply_text.len,
               (char*)m->reply_text.bytes
            );
            return buf;
         }
         case AMQP_CHANNEL_CLOSE_METHOD:
         {
            amqp_channel_close_t *m = (amqp_channel_close_t *)r.reply.decoded;
            snprintf(
               buf,
               bufsize,
               "server channel error %u, message: %.*s",
               m->reply_code,
               (int)m->reply_text.len,
               (char*)m->reply_text.bytes
            );
            return buf;
         }
         default:
            snprintf( buf, bufsize, "unknown server error, method id 0x%08X", r.reply.id );
            return buf;
      }
   }

   return {};
}

std::pair< error_code, std::optional< message > > message_broker_impl::consume() noexcept
{
   uint64_t delivery_tag;
   std::string exchange;
   std::string routing_key;
   std::string content_type;
   std::string data;

   constexpr std::size_t bufsize = 2048;
   char buf[ bufsize ] = { '\0' };

   amqp_envelope_t envelope;

   amqp_maybe_release_buffers( connection );

   timeval tv;
   tv.tv_sec = 1;
   tv.tv_usec = 0;
   auto reply = amqp_consume_message( connection, &envelope, &tv, 0 );

   if ( reply.reply_type == AMQP_RESPONSE_LIBRARY_EXCEPTION && reply.library_error == AMQP_STATUS_TIMEOUT )
   {
      return std::make_pair( error_code::time_out, std::optional< message >{} );
   }
   else if ( AMQP_RESPONSE_NORMAL != reply.reply_type )
   {
      LOG(error) << error_info( reply ).value();
      return std::make_pair( error_code::failure, std::optional< message >{} );
   }

   snprintf( buf, bufsize, "%u", (unsigned)envelope.delivery_tag );
   try
   {
      delivery_tag = boost::lexical_cast< uint64_t >( buf );
   }
   catch ( const boost::bad_lexical_cast& e )
   {
      LOG(error) << e.what();
      amqp_destroy_envelope( &envelope );
      return std::make_pair( error_code::failure, std::optional< message >{} );
   }

   snprintf( buf, bufsize, "%.*s", (int)envelope.exchange.len, (char *)envelope.exchange.bytes );
   exchange = buf;

   snprintf( buf, bufsize, "%.*s", (int)envelope.routing_key.len, (char *)envelope.routing_key.bytes );
   routing_key = buf;

   if ( envelope.message.properties._flags & AMQP_BASIC_CONTENT_TYPE_FLAG )
   {
      snprintf( buf, bufsize, "%.*s",
         (int)envelope.message.properties.content_type.len,
         (char *)envelope.message.properties.content_type.bytes
      );
      content_type = buf;
   }

   snprintf( buf, bufsize, "%.*s", (int)envelope.message.body.len, (char *)envelope.message.body.bytes );
   data = buf;

   amqp_destroy_envelope( &envelope );

   return std::make_pair(
      error_code::success,
      message {
         .delivery_tag = delivery_tag,
         .exchange = std::move( exchange ),
         .routing_key = std::move( routing_key ),
         .content_type = std::move( content_type ),
         .data = std::move( data )
      }
   );
}

} // detail

message_broker::message_broker()
{
   _message_broker_impl = std::make_unique< detail::message_broker_impl >();
}

message_broker::~message_broker() = default;

void message_broker::disconnect() noexcept
{
   _message_broker_impl->disconnect();
}

bool message_broker::is_connected() noexcept
{
   return _message_broker_impl->is_connected();
}

error_code message_broker::publish(
   const std::string& routing_key,
   const std::string& data,
   const std::string& content_type,
   const std::string& exchange ) noexcept
{
   return _message_broker_impl->publish( routing_key, data, content_type, exchange );
}

error_code message_broker::connect(
   const std::string& host,
   uint16_t port,
   const std::string& vhost,
   const std::string& user,
   const std::string& pass ) noexcept
{
   return _message_broker_impl->connect( host, port, vhost, user, pass );
}

error_code message_broker::queue_declare( const std::string& queue ) noexcept
{
   return _message_broker_impl->queue_declare( queue );
}

error_code message_broker::queue_bind(
   const std::string& queue,
   const std::string& exchange,
   const std::string& binding_key ) noexcept
{
   return _message_broker_impl->queue_bind( queue, exchange, binding_key );
}

std::pair< error_code, std::optional< message > > message_broker::consume() noexcept
{
   return _message_broker_impl->consume();
}

} // koinos::mq
