#pragma once

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/generic/stream_protocol.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <memory>

#include <koinos/net/transport/http/session.hpp>
#include <koinos/net/transport/http/router.hpp>
#include <koinos/log.hpp>

namespace koinos::net::transport::http {

namespace http = beast::http;
namespace net = boost::asio;
using stream_protocol = boost::asio::generic::stream_protocol;

// Accepts incoming connections and launches the sessions
class listener : public std::enable_shared_from_this< listener >
{
   net::io_context& ioc_;
   net::basic_socket_acceptor< stream_protocol > acceptor_;
   std::shared_ptr< const router > router_;

public:
   listener( net::io_context& ioc, stream_protocol::endpoint endpoint, std::shared_ptr< const router > const& r ) :
      ioc_( ioc ),
      acceptor_( net::make_strand( ioc ) ),
      router_( r )
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
         std::make_shared< session >( std::move( socket ), router_ )->run();
      }

      // Accept another connection
      do_accept();
   }
};

} // koinos::net::transport::http

