
#include <koinos/net/protocol/jsonrpc/request_handler.hpp>

namespace koinos::net::transport::http {
abstract_request_handler::abstract_request_handler() {}
abstract_request_handler::~abstract_request_handler() {}
}

namespace koinos::net::protocol::jsonrpc {

request_handler::request_handler() {}
request_handler::~request_handler() {}

jsonrpc::request request_handler::parse_request( const std::string& payload ) const
{
   jsonrpc::request req;

   try
   {
      req = json::parse( payload );
   }
   catch( const jsonrpc::exception& e )
   {
      throw;
   }
   catch ( const std::exception& e )
   {
      throw jsonrpc::exception( jsonrpc::error_code::parse_error, "unable to parse request", e.what(), nullptr );
   }

   return req;
}

jsonrpc::response request_handler::call_handler( const jsonrpc::id_type& id, const method_handler& h, const json::object_t& j ) const
{
   jsonrpc::response resp;

   try
   {
      resp = jsonrpc::response {
         .jsonrpc = "2.0",
         .id = id,
         .error = {},
         .result = h( j )
      };
   }
   catch ( const jsonrpc::exception& e )
   {
      throw jsonrpc::exception( e.code, e.what(), e.data, id );
   }
   catch ( const std::exception& e )
   {
      throw jsonrpc::exception( jsonrpc::error_code::server_error, "a server error has occurred", e.what(), id );
   }

   return resp;
}

std::string request_handler::handle( const std::string& payload )
{
   jsonrpc::response resp;

   try
   {
      jsonrpc::request req = parse_request( payload );
      req.validate();

      auto h = get_method_handler( req.method );

      if ( !h.has_value() )
         throw jsonrpc::exception( jsonrpc::error_code::method_not_found, "method not found: " + req.method, {}, req.id );

      resp = call_handler( req.id, h.value(), req.params );
   }
   catch ( const jsonrpc::exception e )
   {
      resp = jsonrpc::response {
         .jsonrpc = "2.0",
         .id = e.id,
         .error = jsonrpc::error_type {
            .code = e.code,
            .message = e.what(),
            .data = e.data
         }
      };
   }
   catch ( const std::exception& e )
   {
      resp = jsonrpc::response {
         .jsonrpc = "2.0",
         .id = nullptr,
         .error = jsonrpc::error_type {
            .code = jsonrpc::error_code::internal_error,
            .message = "an internal error has ocurred",
            .data = e.what()
         }
      };
   }

   json j = resp;

   return j.dump();
}

void request_handler::add_method_handler( const std::string& method_name, method_handler handler )
{
   if ( method_handlers.find( method_name ) != method_handlers.end() )
      throw std::runtime_error( "unable to override method handler" );

   method_handlers.insert( { method_name, handler } );
}

std::optional< method_handler > request_handler::get_method_handler( const std::string& method_name )
{
   auto it = method_handlers.find( method_name );
   if ( it != method_handlers.end() )
      return it->second;
   return {};
}

} // koinos::net::protocol::jsonrpc
