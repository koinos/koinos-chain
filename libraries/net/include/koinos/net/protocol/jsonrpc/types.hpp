#pragma once

#include <cstddef>
#include <cstdint>
#include <exception>
#include <optional>
#include <string>
#include <variant>

#include <koinos/net/protocol/jsonrpc/fields.hpp>
#include <koinos/util.hpp>

#include <nlohmann/json.hpp>

namespace koinos::net::protocol::jsonrpc {

using id_type = std::variant< std::string, uint64_t, std::nullptr_t >;

enum class error_code : int32_t
{
   parse_error      = -32700, // Parse error Invalid JSON was received by the server.
   invalid_request  = -32600, // Invalid Request The JSON sent is not a valid Request object.
   method_not_found = -32601, // Method not found The method does not exist / is not available.
   invalid_params   = -32602, // Invalid params Invalid method parameter(s).
   internal_error   = -32603, // Internal error Internal JSON-RPC error.
   server_error     = -32000  // Server error (-32000 - -32099) Reserved for implementation-defined server-errors.
};

struct exception : virtual std::exception
{
   const std::string msg;
   const jsonrpc::id_type id;
   const jsonrpc::error_code code;
   const std::optional< std::string > data;

   exception( jsonrpc::error_code c, const std::string& m, std::optional< std::string > d = {}, jsonrpc::id_type i = nullptr ) :
      code( c ),
      msg( m ),
      data( d ),
      id( i )
   {}

   virtual ~exception() {}

   virtual const char* what() const noexcept override
   {
      return msg.c_str();
   }
};

} // koinos::net::protocol::jsonrpc

namespace nlohmann {

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
      using namespace koinos::net::protocol;
      if ( j.is_number() )
      {
         if ( j.get< double >() != j.get< uint64_t >() )
            throw jsonrpc::exception( jsonrpc::error_code::invalid_request, "id cannot be fractional" );
         id = j.get< uint64_t >();
      }
      else if ( j.is_string() )
      {
         id = j.get< std::string >();
      }
      else if ( j.is_null() )
      {
         id = nullptr;
      }
      else
      {
         throw jsonrpc::exception( jsonrpc::error_code::invalid_request, "id must be a non-fractional number, string or null" );
      }
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
         throw jsonrpc::exception( jsonrpc::error_code::invalid_request, "an invalid jsonrpc version was provided", {}, id );
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

struct error_type
{
   error_code                   code;
   std::string                  message;
   std::optional< std::string > data;
};

void to_json( json& j, const error_type& e )
{
   j = json {
      { field::code,    e.code },
      { field::message, e.message }
   };

   if ( e.data.has_value() )
      j[ field::data ] = e.data.value();
}

void from_json( const json& j, error_type& e )
{
   j.at( field::code ).get_to( e.code );
   j.at( field::message ).get_to( e.message );

   if ( j.contains( field::data ) )
      e.data = j[ field::data ];
}

struct response
{
   std::string                 jsonrpc = "2.0";
   id_type                     id = nullptr;
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
      throw jsonrpc::exception( jsonrpc::error_code::parse_error, "failed to jsonify due to an invalid response object" );
   }
}

void from_json( const json& j, response& r )
{
   j.at( field::id ).get_to( r.id );
   j.at( field::jsonrpc ).get_to( r.jsonrpc );

   if ( j.contains( field::result ) )
   {
      r.error.reset();
      r.result = j[ field::result ];
   }
   else if ( j.contains( field::error ) )
   {
      r.result.reset();
      r.error = j[ field::error ];
   }
   else
   {
      throw jsonrpc::exception( jsonrpc::error_code::parse_error, "failed to dejsonify due to an invalid response object" );
   }
}

} // koinos::net::jsonrpc
