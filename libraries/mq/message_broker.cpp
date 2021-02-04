#include <koinos/mq/message_broker.hpp>

#include <cstdio>

#include <amqp_tcp_socket.h>
#include <amqp.h>
#include <amqp_framing.h>

#include <boost/lexical_cast.hpp>

#include <koinos/log.hpp>
#include <koinos/util.hpp>

namespace koinos::mq {

namespace detail {

class message_broker_impl final
{
private:
   amqp_connection_state_t _connection = nullptr;
   const amqp_channel_t    _channel = 1;

   std::optional< std::string > error_info( amqp_rpc_reply_t r ) noexcept;

   error_code connect_internal(
      const std::string& host,
      uint16_t port,
      const std::string& vhost,
      const std::string& user,
      const std::string& pass
   ) noexcept;

public:
   message_broker_impl() = default;
   ~message_broker_impl();

   error_code connect( const std::string& url ) noexcept;

   void disconnect() noexcept;

   bool is_connected() noexcept;

   error_code publish( const message& msg ) noexcept;

   std::pair< error_code, std::optional< message > > consume() noexcept;

   error_code declare_exchange(
      const std::string& exchange,
      const std::string& exchange_type,
      bool passive,
      bool durable,
      bool auto_delete,
      bool internal
   ) noexcept;

   error_code declare_queue(
      const std::string& queue,
      bool passive,
      bool durable,
      bool exclusive,
      bool auto_delete
   ) noexcept;

   error_code bind_queue(
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
   if ( !_connection )
      return;

   auto r = amqp_channel_close( _connection, _channel, AMQP_REPLY_SUCCESS );
   if ( r.reply_type != AMQP_RESPONSE_NORMAL )
   {
      LOG(error) << error_info( r ).value();
   }

   r = amqp_connection_close( _connection, AMQP_REPLY_SUCCESS );
   if ( r.reply_type != AMQP_RESPONSE_NORMAL )
   {
      LOG(error) << error_info( r ).value();
   }

   int err = amqp_destroy_connection( _connection );
   if ( err < AMQP_STATUS_OK )
   {
      LOG(error) << amqp_error_string2( err );
   }

   _connection = nullptr;
}

bool message_broker_impl::is_connected() noexcept
{
   return _connection != nullptr;
}

error_code message_broker_impl::publish( const message& msg ) noexcept
{
   amqp_basic_properties_t props;
   props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG;
   props.content_type = amqp_cstring_bytes( msg.content_type.c_str() );
   props.delivery_mode = 2; /* persistent delivery mode */
   if( msg.reply_to.has_value() )
   {
      props._flags |= AMQP_BASIC_REPLY_TO_FLAG;
      props.reply_to = amqp_cstring_bytes( msg.reply_to->c_str() );
   }

   if( msg.correlation_id.has_value() )
   {
      props._flags |= AMQP_BASIC_CORRELATION_ID_FLAG;
      props.correlation_id = amqp_cstring_bytes( msg.correlation_id->c_str() );
   }

   amqp_basic_publish(
      _connection,
      _channel,
      amqp_cstring_bytes( msg.exchange.c_str() ),
      amqp_cstring_bytes( msg.routing_key.c_str() ),
      0,
      0,
      &props,
      amqp_cstring_bytes( msg.data.c_str() )
   );

   return error_code::success;
}

error_code message_broker_impl::connect_internal(
   const std::string& host,
   uint16_t port,
   const std::string& vhost,
   const std::string& user,
   const std::string& pass ) noexcept
{
   disconnect();

   _connection = amqp_new_connection();
   amqp_socket_t *socket = amqp_tcp_socket_new( _connection );

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
      _connection,
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

   amqp_channel_open( _connection, _channel );
   r = amqp_get_rpc_reply( _connection );
   if ( r.reply_type != AMQP_RESPONSE_NORMAL )
   {
      LOG(error) << error_info( r ).value();
      disconnect();
      return error_code::failure;
   }

   return error_code::success;
}

error_code message_broker_impl::connect( const std::string& url ) noexcept
{
   std::vector< char > tmp_url( url.begin(), url.end() );
   tmp_url.push_back( '\0' );

   amqp_connection_info cinfo;
   auto result = amqp_parse_url( tmp_url.data(), &cinfo );

   if( result != AMQP_STATUS_OK )
   {
      LOG(error) << "unable to parse provided amqp url";
      return error_code::failure;
   }

   return connect_internal(
      std::string( cinfo.host ),
      uint16_t( cinfo.port ),
      std::string( "/" ) + cinfo.vhost,
      std::string( cinfo.user ),
      std::string( cinfo.password )
   );
}

error_code message_broker_impl::declare_exchange(
   const std::string& exchange,
   const std::string& exchange_type,
   bool passive,
   bool durable,
   bool auto_delete,
   bool internal ) noexcept
{
   amqp_exchange_declare(
      _connection,
      _channel,
      amqp_cstring_bytes( exchange.c_str() ),
      amqp_cstring_bytes( exchange_type.c_str() ),
      int( passive ),
      int( durable ),
      int( auto_delete ),
      int( internal ),
      amqp_empty_table
   );
   auto reply = amqp_get_rpc_reply( _connection );
   if ( reply.reply_type != AMQP_RESPONSE_NORMAL )
   {
      LOG(error) << error_info( reply ).value();
      return error_code::failure;
   }

   return error_code::success;
}

error_code message_broker_impl::declare_queue(
   const std::string& queue,
   bool passive,
   bool durable,
   bool exclusive,
   bool auto_delete ) noexcept
{
   KOINOS_TODO( "Allow server-assigned name to be returned" );
   amqp_queue_declare(
      _connection,
      _channel,
      amqp_cstring_bytes( queue.c_str() ),
      int( passive ),
      int( durable ),
      int( exclusive ),
      int( auto_delete ),
      amqp_empty_table
   );
   auto reply = amqp_get_rpc_reply( _connection );
   if ( reply.reply_type != AMQP_RESPONSE_NORMAL )
   {
      LOG(error) << error_info( reply ).value();
      return error_code::failure;
   }

   return error_code::success;
}

error_code message_broker_impl::bind_queue(
   const std::string& queue,
   const std::string& exchange,
   const std::string& binding_key ) noexcept
{
   auto queue_bytes = amqp_cstring_bytes( queue.c_str() );
   amqp_queue_bind(
      _connection,
      _channel,
      queue_bytes,
      amqp_cstring_bytes( exchange.c_str() ),
      amqp_cstring_bytes( binding_key.c_str() ),
      amqp_empty_table
   );

   auto reply = amqp_get_rpc_reply( _connection );
   if ( reply.reply_type != AMQP_RESPONSE_NORMAL )
   {
      LOG(error) << error_info( reply ).value();
      return error_code::failure;
   }

   amqp_basic_consume( _connection, _channel, queue_bytes, amqp_empty_bytes, 0, 1, 0, amqp_empty_table );
   reply = amqp_get_rpc_reply( _connection );
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
   std::pair< error_code, std::optional< message > > result;

   constexpr std::size_t bufsize = 2048;
   char buf[ bufsize ] = { '\0' };

   amqp_envelope_t envelope;

   amqp_maybe_release_buffers( _connection );

   timeval tv;
   tv.tv_sec = 1;
   tv.tv_usec = 0;
   auto reply = amqp_consume_message( _connection, &envelope, &tv, 0 );

   if ( reply.reply_type == AMQP_RESPONSE_LIBRARY_EXCEPTION && reply.library_error == AMQP_STATUS_TIMEOUT )
   {
      result.first = error_code::time_out;
      return result;
   }
   else if ( AMQP_RESPONSE_NORMAL != reply.reply_type )
   {
      LOG(error) << error_info( reply ).value();
      result.first = error_code::failure;
      return result;
   }

   uint64_t delivery_tag;

   snprintf( buf, bufsize, "%u", (unsigned)envelope.delivery_tag );
   try
   {
      delivery_tag = boost::lexical_cast< uint64_t >( buf );
   }
   catch ( const boost::bad_lexical_cast& e )
   {
      LOG(error) << e.what();
      amqp_destroy_envelope( &envelope );
      result.first = error_code::failure;
      return result;
   }

   result.second.emplace();

   message& msg = *result.second;

   msg.delivery_tag = delivery_tag;
   msg.exchange = std::string( (char*) envelope.exchange.bytes, (size_t) envelope.exchange.len );
   msg.routing_key = std::string( (char*) envelope.routing_key.bytes, (size_t) envelope.routing_key.len );

   if ( envelope.message.properties._flags & AMQP_BASIC_CONTENT_TYPE_FLAG )
   {
      msg.content_type = std::string(
         (char*) envelope.message.properties.content_type.bytes,
         (size_t) envelope.message.properties.content_type.len
      );
   }

   if ( envelope.message.properties._flags & AMQP_BASIC_REPLY_TO_FLAG )
   {
      msg.reply_to = std::string(
         (char*) envelope.message.properties.reply_to.bytes,
         (size_t) envelope.message.properties.reply_to.len
      );
   }

   if ( envelope.message.properties._flags & AMQP_BASIC_CORRELATION_ID_FLAG )
   {
      msg.correlation_id = std::string(
         (char*) envelope.message.properties.correlation_id.bytes,
         (size_t) envelope.message.properties.correlation_id.len
      );
   }

   msg.data = std::string(
      (char*) envelope.message.body.bytes,
      (size_t) envelope.message.body.len
   );

   amqp_destroy_envelope( &envelope );

   result.first = error_code::success;
   return result;
}

} // detail

message_broker::message_broker()
{
   _message_broker_impl = std::make_unique< detail::message_broker_impl >();
}

message_broker::~message_broker() = default;

error_code message_broker::connect( const std::string& url ) noexcept
{
   return _message_broker_impl->connect( url );
}

void message_broker::disconnect() noexcept
{
   _message_broker_impl->disconnect();
}

bool message_broker::is_connected() noexcept
{
   return _message_broker_impl->is_connected();
}

error_code message_broker::publish( const message& msg ) noexcept
{
   return _message_broker_impl->publish( msg );
}

error_code message_broker::declare_exchange(
      const std::string& exchange,
      const std::string& exchange_type,
      bool passive,
      bool durable,
      bool auto_delete,
      bool internal ) noexcept
{
   return _message_broker_impl->declare_exchange( exchange, exchange_type, passive, durable, auto_delete, internal );
}

error_code message_broker::declare_queue(
   const std::string& queue,
   bool passive,
   bool durable,
   bool exclusive,
   bool auto_delete ) noexcept
{
   return _message_broker_impl->declare_queue( queue, passive, durable, exclusive, auto_delete );
}

error_code message_broker::bind_queue(
   const std::string& queue,
   const std::string& exchange,
   const std::string& binding_key ) noexcept
{
   return _message_broker_impl->bind_queue( queue, exchange, binding_key );
}

std::pair< error_code, std::optional< message > > message_broker::consume() noexcept
{
   return _message_broker_impl->consume();
}

} // koinos::mq
