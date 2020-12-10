#pragma once

#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>

#include <koinos/net/jsonrpc/types.hpp>

#include <nlohmann/json.hpp>

namespace koinos::net::jsonrpc {

using json = nlohmann::json;

class request_handler : public std::enable_shared_from_this< request_handler > {
public:
   using method_handler = std::function< json( const json::object_t& ) >;

   void add_method_handler( const std::string& method_name, method_handler handler )
   {
      if ( method_handlers.find( method_name ) == method_handlers.end() )
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

} // koinos::net::jsonrpc
