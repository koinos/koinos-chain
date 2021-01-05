#pragma once

#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include <koinos/net/protocol/jsonrpc/types.hpp>
#include <koinos/net/transport/http/abstract_request_handler.hpp>

namespace koinos::net::protocol::jsonrpc {

using method_handler = std::function< json( const json::object_t& ) >;

class request_handler : public koinos::net::transport::http::abstract_request_handler
{
public:
   request_handler();
   virtual ~request_handler() override;

   jsonrpc::request parse_request( const std::string& payload ) const;

   jsonrpc::response call_handler( const jsonrpc::id_type& id, const method_handler& h, const json::object_t& j ) const;

   std::string handle( const std::string& payload ) override;

   void add_method_handler( const std::string& method_name, method_handler handler );
   std::optional< method_handler > get_method_handler( const std::string& method_name );

private:
   std::unordered_map< std::string, method_handler > method_handlers;
};

} // koinos::net::protocol::jsonrpc
