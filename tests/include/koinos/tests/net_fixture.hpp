#pragma once

#include <koinos/net/jsonrpc/server.hpp>

struct net_fixture
{
   boost::asio::io_context ioc;

   net_fixture()
   {
      auto const address = boost::asio::ip::make_address( "127.0.0.1" );
      auto const port = static_cast< unsigned short >( 6770 );
      auto const request_handler = std::make_shared< koinos::net::jsonrpc::request_handler >();

      // Create and launch a listening port
      std::make_shared< koinos::net::jsonrpc::listener >(
         ioc,
         boost::asio::ip::tcp::endpoint{ address, port },
         request_handler )->run();

      // Capture SIGINT and SIGTERM to perform a clean shutdown
      boost::asio::signal_set signals( ioc, SIGINT, SIGTERM );
      signals.async_wait( [&]( boost::beast::error_code const&, int )
      {
         // Stop the `io_context`. This will cause `run()`
         // to return immediately, eventually destroying the
         // `io_context` and all of the sockets in it.
         ioc.stop();
      });

      ioc.run();
   }

   ~net_fixture()
   {
      ioc.stop();
   }
};
