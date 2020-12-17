#pragma once

#include <cstddef>
#include <cstdint>
#include <exception>
#include <optional>
#include <string>

#include <koinos/net/protocol/jsonrpc/fields.hpp>
#include <koinos/util.hpp>

#include <nlohmann/json.hpp>

namespace koinos::net::protocol::jsonrpc { using id_type = std::variant< std::string, uint64_t, std::nullptr_t >; }

namespace nlohmann
{

template <> struct adl_serializer< koinos::net::protocol::jsonrpc::id_type >
{
   static void to_json( nlohmann::json& j, const koinos::net::protocol::jsonrpc::id_type& id )
   {
      std::visit( koinos::overloaded {
         [&]( std::nullptr_t )     { j = nullptr; },
         [&]( uint64_t arg )       { j = arg; },
         [&]( std::string arg )    { j = arg; },
         [&]( auto )               { throw std::runtime_error( "unable to parse json rpc id" ); }
      }, id );
   }

   static void from_json( const nlohmann::json& j, koinos::net::protocol::jsonrpc::id_type& id )
   {
      if ( j.is_number() )
         id = j.get< uint64_t >();
      else if ( j.is_string() )
         id = j.get< std::string >();
      else if ( j.is_null() )
         id = nullptr;
      else
         throw std::runtime_error( "unable to parse json rpc id" );
   }
};

} // nlohmann

namespace koinos::net::protocol::jsonrpc {

using json = nlohmann::json;

struct request
{
   std::string    jsonrpc;
   id_type        id;
   std::string    method;
   json::object_t params;

   void validate()
   {
      if ( jsonrpc != "2.0" )
         throw std::runtime_error( "an invalid jsonrpc version was provided" );
   }
};

void to_json( json& j, const request& r )
{
   j = json {
      { field::jsonrpc, r.jsonrpc },
      { field::id,      r.id },
      { field::method,  r.method },
      { field::params,  r.params }
   };
}

void from_json( const json& j, request& r )
{
   j.at( field::jsonrpc ).get_to( r.jsonrpc );
   j.at( field::id ).get_to( r.id );
   j.at( field::method ).get_to( r.method );
   j.at( field::params ).get_to( r.params );
}

enum class error_code : int32_t
{
   parse_error      = -32700, // Parse error Invalid JSON was received by the server.
   invalid_request  = -32600, // Invalid Request The JSON sent is not a valid Request object.
   method_not_found = -32601, // Method not found The method does not exist / is not available.
   invalid_params   = -32602, // Invalid params Invalid method parameter(s).
   internal_error   = -32603, // Internal error Internal JSON-RPC error.
   server_error     = -32000  // Server error (-32000 - -32099) Reserved for implementation-defined server-errors.
};

struct error_type
{
   error_code  code;
   std::string message;
};

void to_json( json& j, const error_type& e )
{
   j = json {
      { field::code,    e.code },
      { field::message, e.message }
   };
}

void from_json( const json& j, error_type& e )
{
   j.at( field::code ).get_to( e.code );
   j.at( field::message ).get_to( e.message );
}

struct response
{
   std::string                 jsonrpc = "2.0";
   id_type                     id;
   std::optional< error_type > error;
   std::optional< json >       result;
};

void to_json( json& j, const response& r )
{
   if ( r.result.has_value() )
   {
      j = json {
         { field::jsonrpc, r.jsonrpc },
         { field::id, r.id },
         { field::result, r.result.value() }
      };
   }
   else if ( r.error.has_value() )
   {
      j = json {
         { field::jsonrpc, r.jsonrpc },
         { field::id, r.id },
         { field::error, r.error.value() }
      };
   }
   else
   {
      throw std::runtime_error( "failed to jsonify due to an invalid response object" );
   }
}

void from_json( const json& j, response& r )
{
   j.at( field::id ).get_to( r.id );
   j.at( field::jsonrpc ).get_to( r.jsonrpc );

   if ( j.contains( field::result ) )
   {
      r.result = j[ field::result ];
   }
   else if ( j.contains( field::error ) )
   {
      j.at( field::error ).get_to( *r.error );
   }
   else
   {
      throw std::runtime_error( "failed to dejsonify due to an invalid response object" );
   }
}

} // koinos::net::jsonrpc
