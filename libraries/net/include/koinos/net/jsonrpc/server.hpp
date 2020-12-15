#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/generic/stream_protocol.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/strand.hpp>
#include <boost/make_unique.hpp>
#include <boost/optional.hpp>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <variant>
#include <vector>

#include <koinos/log.hpp>
#include <koinos/net/jsonrpc/constants.hpp>
#include <koinos/net/jsonrpc/request_handler.hpp>
#include <koinos/net/jsonrpc/types.hpp>

#include <nlohmann/json.hpp>

namespace koinos::net::jsonrpc {

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using stream_protocol = boost::asio::generic::stream_protocol;
using json = nlohmann::json;

// This function produces an HTTP response for the given
// request. The type of the response object depends on the
// contents of the request, so the interface requires the
// caller to pass a generic lambda for receiving the response.
template< class Body, class Allocator, class Send >
void handle_request( request_handler req_handler, http::request< Body, http::basic_fields< Allocator > >&& req, Send&& send )
{
   // Returns a bad request response
   auto const bad_request = [&req]( beast::string_view why )
   {
      http::response< http::string_body > res{ http::status::bad_request, req.version() };
      res.set( http::field::server, constants::version_string );
      res.set( http::field::content_type, constants::content_type::application_json );
      res.keep_alive( req.keep_alive() );
      json j = jsonrpc::response {
         .jsonrpc = "2.0",
         .id = nullptr,
         .error = jsonrpc::error_type {
            .code = error_code::invalid_request,
            .message = std::string( why )
         }
      };
      res.body() = j.dump();
      res.prepare_payload();
      return res;
   };

   // Returns a not found response
   auto const not_found = [&req]( beast::string_view target )
   {
      http::response< http::string_body > res{ http::status::not_found, req.version() };
      res.set( http::field::server, constants::version_string );
      res.set( http::field::content_type, constants::content_type::application_json );
      res.keep_alive( req.keep_alive() );
      json j = jsonrpc::response {
         .jsonrpc = "2.0",
         .id = nullptr,
         .error = jsonrpc::error_type {
            .code = error_code::method_not_found,
            .message = std::string( target )
         }
      };
      res.body() = j.dump();
      res.prepare_payload();
      return res;
   };

   // Returns a server error response
   auto const server_error = [&req]( beast::string_view what )
   {
      http::response< http::string_body > res{ http::status::internal_server_error, req.version() };
      res.set( http::field::server, constants::version_string );
      res.set( http::field::content_type, constants::content_type::application_json );
      res.keep_alive( req.keep_alive() );
      json j = jsonrpc::response {
         .jsonrpc = "2.0",
         .id = nullptr,
         .error = jsonrpc::error_type {
            .code = error_code::internal_error,
            .message = std::string( what )
         }
      };
      res.body() = j.dump();
      res.prepare_payload();
      return res;
   };

   switch ( req.method() )
   {
      case http::verb::get:
      case http::verb::put:
      case http::verb::post:
      case http::verb::head:
         break;
      default:
         return send( bad_request( "unsupported http method" ) );
   }

   if ( !req.target().empty() && req.target()[0] != '/' )
      return send( bad_request( "unsupported target" ) );


   http::string_body::value_type body;

   try
   {
      jsonrpc::request r = json::parse( req.body() );
      r.validate();

      auto h = req_handler.get_method_handler( r.method );

      if ( !h.has_value() )
         return send( not_found( r.method ) );

      json resp = jsonrpc::response {
         .jsonrpc = "2.0",
         .id = r.id,
         .error = {},
         .result = h.value()( r.params )
      };
      body = resp.dump();
   }
   catch ( const std::exception& e )
   {
      return send( server_error( e.what() ) );
   }

   // Cache the size since we need it after the move
   auto const size = body.size();

   // Respond to HEAD request
   if ( req.method() == http::verb::head )
   {
      http::response< http::empty_body > res{ http::status::ok, req.version() };
      res.set( http::field::server, constants::version_string );
      res.set( http::field::content_type, constants::content_type::application_json );
      res.content_length( size );
      res.keep_alive( req.keep_alive() );
      return send( std::move( res ) );
   }

   // Respond to GET/POST/PUT request
   http::response< http::string_body > res {
      std::piecewise_construct,
      std::make_tuple( std::move( body ) ),
      std::make_tuple( http::status::ok, req.version() )
   };

   res.set( http::field::server, constants::version_string );
   res.set( http::field::content_type, constants::content_type::application_json );
   res.content_length( size );
   res.keep_alive( req.keep_alive() );
   return send( std::move( res ) );
}

// Handles an HTTP server connection
class http_session : public std::enable_shared_from_this< http_session >
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

      http_session& self_;
      std::vector< std::unique_ptr< work > > items_;

   public:
      explicit queue( http_session& self ) : self_( self )
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
            http_session& self_;
            http::message< IsRequest, Body, Fields > msg_;

            work_impl( http_session& self, http::message< IsRequest, Body, Fields >&& msg) :
               self_( self ),
               msg_( std::move( msg ) ) {}

            void operator()()
            {
               http::async_write( self_.stream_, msg_, beast::bind_front_handler( &http_session::on_write, self_.shared_from_this(), msg_.need_eof() ) );
            }
         };

         // Allocate and store the work
         items_.push_back( boost::make_unique< work_impl >( self_, std::move( msg ) ) );

         // If there was no previous work, start this one
         if ( items_.size() == 1 )
            (*items_.front())();
      }
   };

   beast::basic_stream< stream_protocol > stream_;
   beast::flat_buffer buffer_;
   std::shared_ptr< request_handler const > req_handler_;
   queue queue_;

   // The parser is stored in an optional container so we can
   // construct it from scratch it at the beginning of each new message.
   boost::optional< http::request_parser< http::string_body > > parser_;

public:
   // Take ownership of the socket
   http_session( stream_protocol::socket&& socket, std::shared_ptr< request_handler const > const& req_handler ) :
      stream_( std::move( socket ) ),
      req_handler_( req_handler ),
      queue_( *this ) {}

   // Start the session
   void run()
   {
      // We need to be executing within a strand to perform async operations
      // on the I/O objects in this session. Although not strictly necessary
      // for single-threaded contexts, this example code is written to be
      // thread-safe by default.
      net::dispatch( stream_.get_executor(), beast::bind_front_handler( &http_session::do_read, this->shared_from_this() ) );
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
      http::async_read( stream_, buffer_, *parser_, beast::bind_front_handler( &http_session::on_read, shared_from_this() ) );
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
      handle_request( *req_handler_, parser_->release(), queue_ );

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
      stream_.socket().shutdown( stream_protocol::socket::shutdown_send, ec );

      // At this point the connection is closed gracefully
   }
};

//------------------------------------------------------------------------------

// Accepts incoming connections and launches the sessions
class listener : public std::enable_shared_from_this< listener >
{
   net::io_context& ioc_;
   net::basic_socket_acceptor< stream_protocol > acceptor_;
   std::shared_ptr< request_handler const > req_handler_;

public:
   listener( net::io_context& ioc, stream_protocol::endpoint endpoint, std::shared_ptr< request_handler const > const& req_handler ) :
      ioc_( ioc ),
      acceptor_( net::make_strand( ioc ) ),
      req_handler_( req_handler )
   {
      beast::error_code ec;

      // Open the acceptor
      acceptor_.open( endpoint.protocol(), ec );
      if ( ec )
      {
         LOG(error) << "open: " << ec.message();
         return;
      }

      // Allow address reuse
      acceptor_.set_option( net::socket_base::reuse_address( true ), ec );
      if ( ec )
      {
         LOG(error) << "set_option: " << ec.message();
         return;
      }

      // Bind to the server address
      acceptor_.bind( endpoint, ec );
      if ( ec )
      {
         LOG(error) << "bind: " << ec.message();
         return;
      }

      // Start listening for connections
      acceptor_.listen( net::socket_base::max_listen_connections, ec );
      if ( ec )
      {
         LOG(error) << "listen: " << ec.message();
         return;
      }
   }

   // Start accepting incoming connections
   void run()
   {
      // We need to be executing within a strand to perform async operations
      // on the I/O objects in this session. Although not strictly necessary
      // for single-threaded contexts, this example code is written to be
      // thread-safe by default.
      net::dispatch( acceptor_.get_executor(), beast::bind_front_handler( &listener::do_accept, this->shared_from_this() ) );
   }

private:
   void do_accept()
   {
      // The new connection gets its own strand
      acceptor_.async_accept( net::make_strand( ioc_ ), beast::bind_front_handler( &listener::on_accept, shared_from_this() ) );
   }

   void on_accept( beast::error_code ec, stream_protocol::socket socket )
   {
      if ( ec )
      {
         LOG(error) << "accept: " << ec.message();
      }
      else
      {
         // Create the http session and run it
         std::make_shared< http_session >( std::move( socket ), req_handler_ )->run();
      }

      // Accept another connection
      do_accept();
   }
};

} // koinos::net::jsonrpc
