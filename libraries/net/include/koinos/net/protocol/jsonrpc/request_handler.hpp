#pragma once

#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>

#include <koinos/net/transport/http/abstract_request_handler.hpp>
#include <koinos/net/protocol/jsonrpc/types.hpp>

#include <nlohmann/json.hpp>

namespace koinos::net::protocol::jsonrpc {

using json = nlohmann::json;

class request_handler :
   public std::enable_shared_from_this< request_handler >,
   public koinos::net::transport::http::abstract_request_handler
{
public:
   using method_handler = std::function< json( const json::object_t& ) >;

   std::string handle( const std::string& body ) override
   {
      jsonrpc::response resp;

      try
      {
         jsonrpc::request req = json::parse( body );
         req.validate();

         auto h = get_method_handler( req.method );

         if ( !h.has_value() )
         {
            resp = jsonrpc::response {
               .jsonrpc = "2.0",
               .id = req.id,
               .error = jsonrpc::error_type {
                  .code = jsonrpc::error_code::method_not_found,
                  .message = "method not found: " + req.method
               }
            };
         }
         else
         {
            resp = jsonrpc::response {
               .jsonrpc = "2.0",
               .id = req.id,
               .error = {},
               .result = h.value()( req.params )
            };
         }
      }
      catch ( const std::exception& e )
      {
         resp = jsonrpc::response {
            .jsonrpc = "2.0",
            .id = nullptr,
            .error = jsonrpc::error_type {
               .code = jsonrpc::error_code::internal_error,
               .message = "an exception has ocurred: " + std::string( e.what() )
            }
         };
      }

      json j = resp;

      return j.dump();
   }

   void add_method_handler( const std::string& method_name, method_handler handler )
   {
      if ( method_handlers.find( method_name ) != method_handlers.end() )
         throw std::runtime_error( "unable to override method handler" );

      method_handlers.insert( { method_name, handler } );
   }

   std::optional< method_handler > get_method_handler( const std::string& method_name )
   {
      auto it = method_handlers.find( method_name );
      if ( it != method_handlers.end() )
         return it->second;
      return {};
   }

private:
   std::unordered_map< std::string, method_handler > method_handlers;
};

} // koinos::net::protocol::jsonrpc
