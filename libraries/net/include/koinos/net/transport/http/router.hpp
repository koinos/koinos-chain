#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include <koinos/net/transport/http/abstract_request_handler.hpp>

namespace koinos::net::transport::http {

namespace beast = boost::beast;
namespace http = beast::http;

constexpr const char* version_string = "Koinos/1.0";

struct router
{
   std::unordered_map< std::string, std::shared_ptr< abstract_request_handler > > handlers;

   template< class Body, class Allocator, class Send >
   void handle( http::request< Body, http::basic_fields< Allocator > >&& req, Send&& send ) const
   {
         // Returns a bad request response
      auto const bad_request = [ &req ]( std::string why )
      {
         http::response< http::string_body > res{ http::status::bad_request, req.version() };
         res.set( http::field::server, version_string );
         res.set( http::field::content_type, "text/html" );
         res.keep_alive( req.keep_alive() );
         res.body() = std::move( why );
         res.prepare_payload();
         return res;
      };

      // Returns a not found response
      auto const not_found = [ &req ]( std::string why )
      {
         http::response< http::string_body > res{ http::status::not_found, req.version() };
         res.set( http::field::server, version_string );
         res.set( http::field::content_type, "text/html" );
         res.keep_alive( req.keep_alive() );
         res.body() = std::move( why );
         res.prepare_payload();
         return res;
      };

      // Returns a server error response
      auto const server_error = [ &req ]( std::string why )
      {
         http::response< http::string_body > res{ http::status::internal_server_error, req.version() };
         res.set( http::field::server, version_string );
         res.set( http::field::content_type, "text/html" );
         res.keep_alive( req.keep_alive() );
         res.body() = std::move( why );
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

      if ( req.target().empty() || req.target() != "/" )
         return send( not_found( "unsupported target" ) );

      http::string_body::value_type body;

      try
      {
         auto iter = handlers.find( std::string( req[ http::field::content_type ] ) );

         if ( iter != handlers.end() )
            body = iter->second->handle( req.body() );
         else
            throw std::runtime_error( "unsupported content-type" );
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
         res.set( http::field::server, version_string );
         res.set( http::field::content_type, req[ http::field::content_type ] );
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

      res.set( http::field::server, version_string );
      res.set( http::field::content_type, req[ http::field::content_type ] );
      res.content_length( size );
      res.keep_alive( req.keep_alive() );
      return send( std::move( res ) );
   }
};

} // koinos::net::transport::http
