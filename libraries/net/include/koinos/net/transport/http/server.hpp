#pragma once

#include <memory>

#include <boost/asio/basic_socket_acceptor.hpp>
#include <boost/asio/generic/stream_protocol.hpp>
#include <boost/beast/core.hpp>

#include <koinos/net/transport/http/session.hpp>
#include <koinos/net/transport/http/router.hpp>

namespace koinos::net::transport::http {

// Accepts incoming connections and launches the sessions
class server : public std::enable_shared_from_this< server >
{
   boost::asio::io_context& ioc_;
   boost::asio::basic_socket_acceptor< boost::asio::generic::stream_protocol > acceptor_;
   std::shared_ptr< const router > router_;

public:
   server( boost::asio::io_context& ioc, boost::asio::generic::stream_protocol::endpoint endpoint, std::shared_ptr< const router > const& r );

   // Start accepting incoming connections
   void run();

private:
   void do_accept();
   void on_accept( boost::beast::error_code ec, boost::asio::generic::stream_protocol::socket socket );
};

} // koinos::net::transport::http
