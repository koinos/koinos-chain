#pragma once

#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>

#include <nlohmann/json.hpp>

namespace koinos::net::jsonrpc {

using json = nlohmann::json;

class request_handler : public std::enable_shared_from_this< request_handler > {
public:
   using method_handler = std::function< std::string( const json::object_t& ) >;

   void add_method_handler( const std::string& method_name, method_handler handler )
   {
      if ( method_handler_map.find( method_name ) == method_handler_map.end() )
         throw std::runtime_error( "unable to override method handler" );

      method_handler_map.insert( { method_name, handler } );
   }

   std::optional< method_handler > get_method_handler( const std::string& method_name )
   {
      auto it = method_handler_map.find( method_name );
      if ( it != method_handler_map.end() )
         return it->second;
      return {};
   }

private:
   std::unordered_map< std::string, method_handler > method_handler_map;
};

} // koinos::net::jsonrpc
