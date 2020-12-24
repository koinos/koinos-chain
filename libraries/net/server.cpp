#include <koinos/net/transport/http/server.hpp>

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/strand.hpp>

#include <koinos/log.hpp>

namespace koinos::net::transport::http {

server::server( boost::asio::io_context& ioc, boost::asio::generic::stream_protocol::endpoint endpoint, std::shared_ptr< const router > const& r ) :
   ioc_( ioc ),
   acceptor_( boost::asio::make_strand( ioc ) ),
   router_( r )
{
   // Open the acceptor
   acceptor_.open( endpoint.protocol() );

   // Allow address reuse
   acceptor_.set_option( boost::asio::socket_base::reuse_address( true ) );

   // Bind to the server address
   acceptor_.bind( endpoint );

   // Start listening for connections
   acceptor_.listen( boost::asio::socket_base::max_listen_connections );
}

// Start accepting incoming connections
void server::run()
{
   // We need to be executing within a strand to perform async operations
   // on the I/O objects in this session. Although not strictly necessary
   // for single-threaded contexts, this example code is written to be
   // thread-safe by default.
   boost::asio::dispatch( acceptor_.get_executor(), boost::beast::bind_front_handler( &server::do_accept, this->shared_from_this() ) );
}

void server::do_accept()
{
   // The new connection gets its own strand
   acceptor_.async_accept( boost::asio::make_strand( ioc_ ), boost::beast::bind_front_handler( &server::on_accept, this->shared_from_this() ) );
}

void server::on_accept( boost::beast::error_code ec, boost::asio::generic::stream_protocol::socket socket )
{
   if ( ec )
   {
      LOG(error) << "accept: " << ec.message();
   }
   else
   {
      // Create the session and run it
      std::make_shared< koinos::net::transport::http::session >( std::move( socket ), router_ )->run();
   }

   // Accept another connection
   do_accept();
}

} // koinos::net::transport::http
