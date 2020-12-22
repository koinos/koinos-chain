#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/generic/stream_protocol.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/make_unique.hpp>
#include <boost/optional.hpp>

#include <koinos/log.hpp>
#include <koinos/net/transport/http/router.hpp>

namespace koinos::net::transport::http {

namespace {

namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;

}

// Handles an HTTP server connection
class session : public std::enable_shared_from_this< session >
{
   // This queue is used for HTTP pipelining.
   class queue
   {
      enum
      {
         // Maximum number of responses we will queue
         limit = 8
      };

      // The type-erased, saved work item
      struct work
      {
         virtual ~work() = default;
         virtual void operator()() = 0;
      };

      session& self_;
      std::vector< std::unique_ptr< work > > items_;

   public:
      explicit queue( session& self ) : self_( self )
      {
         static_assert( limit > 0, "queue limit must be positive" );
         items_.reserve( limit );
      }

      // Returns `true` if we have reached the queue limit
      bool is_full() const
      {
         return items_.size() >= limit;
      }

      // Called when a message finishes sending
      // Returns `true` if the caller should initiate a read
      bool on_write()
      {
         BOOST_ASSERT( !items_.empty() );
         auto const was_full = is_full();
         items_.erase( items_.begin() );
         if ( !items_.empty() )
            (*items_.front())();
         return was_full;
      }

      // Called by the HTTP handler to send a response.
      template< bool IsRequest, class Body, class Fields >
      void operator()( http::message< IsRequest, Body, Fields >&& msg )
      {
         // This holds a work item
         struct work_impl : work
         {
            session& self_;
            http::message< IsRequest, Body, Fields > msg_;

            work_impl( session& self, http::message< IsRequest, Body, Fields >&& msg) :
               self_( self ),
               msg_( std::move( msg ) ) {}

            void operator()()
            {
               http::async_write( self_.stream_, msg_, beast::bind_front_handler( &session::on_write, self_.shared_from_this(), msg_.need_eof() ) );
            }
         };

         // Allocate and store the work
         items_.push_back( boost::make_unique< work_impl >( self_, std::move( msg ) ) );

         // If there was no previous work, start this one
         if ( items_.size() == 1 )
            (*items_.front())();
      }
   };

   beast::basic_stream< asio::generic::stream_protocol > stream_;
   beast::flat_buffer buffer_;
   std::shared_ptr< const router > http_router_;
   queue queue_;

   // The parser is stored in an optional container so we can
   // construct it from scratch it at the beginning of each new message.
   boost::optional< http::request_parser< http::string_body > > parser_;

public:
   // Take ownership of the socket
   session( asio::generic::stream_protocol::socket&& socket, std::shared_ptr< const router > const& http_router ) :
      stream_( std::move( socket ) ),
      http_router_( http_router ),
      queue_( *this ) {}

   // Start the session
   void run()
   {
      // We need to be executing within a strand to perform async operations
      // on the I/O objects in this session. Although not strictly necessary
      // for single-threaded contexts, this example code is written to be
      // thread-safe by default.
      asio::dispatch( stream_.get_executor(), beast::bind_front_handler( &session::do_read, this->shared_from_this() ) );
   }


private:
   void do_read()
   {
      // Construct a new parser for each message
      parser_.emplace();

      // Apply a reasonable limit to the allowed size
      // of the body in bytes to prevent abuse.
      parser_->body_limit( 10000 );

      // Set the timeout.
      stream_.expires_after( std::chrono::seconds( 30 ) );

      // Read a request using the parser-oriented interface
      http::async_read( stream_, buffer_, *parser_, beast::bind_front_handler( &session::on_read, shared_from_this() ) );
   }

   void on_read( beast::error_code ec, std::size_t bytes_transferred )
   {
      boost::ignore_unused( bytes_transferred );

      // This means they closed the connection
      if ( ec == http::error::end_of_stream )
         return do_close();

      if ( ec )
      {
         LOG(error) << "read: " << ec.message();
         return;
      }

      // Send the response
      http_router_->handle( parser_->release(), queue_ );

      // If we aren't at the queue limit, try to pipeline another request
      if ( !queue_.is_full() )
         do_read();
   }

   void on_write( bool close, beast::error_code ec, std::size_t bytes_transferred )
   {
      boost::ignore_unused( bytes_transferred );

      if ( ec )
      {
         LOG(error) << "write: " << ec.message();
         return;
      }

      if ( close )
      {
         // This means we should close the connection, usually because
         // the response indicated the "Connection: close" semantic.
         return do_close();
      }

      // Inform the queue that a write completed
      if ( queue_.on_write() )
      {
         // Read another request
         do_read();
      }
   }

   void do_close()
   {
      // Send a TCP shutdown
      beast::error_code ec;
      stream_.socket().shutdown( asio::generic::stream_protocol::socket::shutdown_send, ec );

      // At this point the connection is closed gracefully
   }
};

} // koinos::net::transport::http
